"""Convert the supplied GLB pack to cached, orthographic 2.5D sprite pages.

Offline asset preparation only; this does not build or launch the game.
Requires numpy and Pillow. Original resources are never modified.
"""

import argparse
import hashlib
import io
import json
import math
from pathlib import Path
import struct
import time

import numpy as np
from PIL import Image


VERSION = b"arcana-software-bake-4-screen-facing-side-corpse"
PAGE = 2048
FRAMES = 6
DIRECTIONS = 8
CHARACTERS = {
    "protagonist": "characters/presets/protagonist.glb",
    "villager": "characters/presets/villager.glb",
    "guard": "characters/presets/guard.glb",
    "blacksmith": "characters/presets/blacksmith.glb",
    "boar": "animals/boar.glb",
    "wolf": "animals/wolf.glb",
    "deer": "animals/deer.glb",
    "goat": "animals/goat.glb",
}
ENVIRONMENT = [
    "prefab_cottage", "pine_tall", "oak_mature", "spruce_young", "oak_young",
    "tree_dead", "stump", "fallen_log", "rock_01", "rock_02", "rock_03",
    "well", "anvil", "table", "bench", "chair", "barrel", "crate", "crate_open",
    "sack", "firewood_stack", "lantern", "fence_2m", "chest_rigged",
    "fern", "shrub", "reeds", "grass_clump_01", "grass_clump_02",
    "grass_clump_03", "mushrooms", "wall_window_2m", "post_2p5m",
    "foundation_2x2", "beam_2m", "bucket", "clay_pot", "hammer", "axe",
]


def rotation(q):
    x, y, z, w = q / max(1e-12, np.linalg.norm(q))
    return np.array([
        [1 - 2*y*y - 2*z*z, 2*x*y - 2*z*w, 2*x*z + 2*y*w, 0],
        [2*x*y + 2*z*w, 1 - 2*x*x - 2*z*z, 2*y*z - 2*x*w, 0],
        [2*x*z - 2*y*w, 2*y*z + 2*x*w, 1 - 2*x*x - 2*y*y, 0],
        [0, 0, 0, 1],
    ], dtype=np.float32)


def slerp(a, b, t):
    dot = float(np.dot(a, b))
    if dot < 0:
        b, dot = -b, -dot
    if dot > .9995:
        q = a * (1 - t) + b * t
        return q / max(1e-12, np.linalg.norm(q))
    angle = math.acos(min(1, dot))
    return (a * math.sin((1-t)*angle) + b * math.sin(t*angle)) / math.sin(angle)


class Model:
    def __init__(self, path):
        data = path.read_bytes()
        magic, version, length = struct.unpack_from("<III", data)
        if magic != 0x46546c67 or version != 2 or length != len(data):
            raise ValueError(f"Invalid GLB: {path}")
        offset = 12
        self.binary = None
        while offset < length:
            size, kind = struct.unpack_from("<II", data, offset)
            offset += 8
            chunk = data[offset:offset+size]
            if kind == 0x4e4f534a:
                self.doc = json.loads(chunk)
            elif kind == 0x004e4942:
                self.binary = chunk
            offset += size
        self.arrays = {}
        self.images = []
        for image in self.doc.get("images", []):
            view = self.doc["bufferViews"][image["bufferView"]]
            start = view.get("byteOffset", 0)
            rgba = Image.open(io.BytesIO(self.binary[start:start+view["byteLength"]])).convert("RGBA")
            self.images.append(np.asarray(rgba, dtype=np.float32) / 255)
        self.primitives = []
        for ni, node in enumerate(self.doc["nodes"]):
            if "mesh" not in node:
                continue
            for primitive in self.doc["meshes"][node["mesh"]]["primitives"]:
                if primitive.get("mode", 4) != 4:
                    raise ValueError("Only triangle GLB primitives are supported")
                attrs = primitive["attributes"]
                positions = self.access(attrs["POSITION"])
                indices = self.access(primitive["indices"]).astype(np.int32).reshape(-1, 3)
                joints = self.access(attrs["JOINTS_0"]).astype(np.int32) if "JOINTS_0" in attrs else None
                weights = self.access(attrs["WEIGHTS_0"]) if "WEIGHTS_0" in attrs else None
                if weights is not None:
                    weights = weights / np.maximum(weights.sum(axis=1, keepdims=True), 1e-12)
                uv = self.access(attrs["TEXCOORD_0"]) if "TEXCOORD_0" in attrs else np.zeros((len(positions), 2))
                mat = self.doc.get("materials", [])[primitive["material"]]
                pbr = mat.get("pbrMetallicRoughness", {})
                color = np.array(pbr.get("baseColorFactor", [1, 1, 1, 1]), dtype=np.float32)
                texture = None
                if "baseColorTexture" in pbr:
                    ti = pbr["baseColorTexture"]["index"]
                    texture = self.images[self.doc["textures"][ti]["source"]]
                self.primitives.append((ni, positions, indices, joints, weights, uv, color, texture))

    def access(self, index):
        if index in self.arrays:
            return self.arrays[index]
        a = self.doc["accessors"][index]
        if "sparse" in a:
            raise ValueError("Sparse accessors are not supported")
        view = self.doc["bufferViews"][a["bufferView"]]
        dtype = {5120: "i1", 5121: "u1", 5122: "<i2", 5123: "<u2", 5125: "<u4", 5126: "<f4"}[a["componentType"]]
        columns = {"SCALAR": 1, "VEC2": 2, "VEC3": 3, "VEC4": 4, "MAT4": 16}[a["type"]]
        size = np.dtype(dtype).itemsize
        values = np.ndarray((a["count"], columns), dtype=dtype, buffer=self.binary,
                            offset=view.get("byteOffset", 0) + a.get("byteOffset", 0),
                            strides=(view.get("byteStride", size*columns), size)).copy()
        if a.get("normalized") and a["componentType"] != 5126:
            values = values.astype(np.float32) / np.iinfo(dtype).max
        self.arrays[index] = values
        return values

    def clip(self, name):
        return next((a for a in self.doc.get("animations", []) if a["name"].endswith("_" + name)), None)

    def pose(self, clip_name, phase):
        nodes = self.doc["nodes"]
        trs = [[np.array(n.get("translation", [0, 0, 0]), dtype=np.float32),
                np.array(n.get("rotation", [0, 0, 0, 1]), dtype=np.float32),
                np.array(n.get("scale", [1, 1, 1]), dtype=np.float32)] for n in nodes]
        animation = self.clip(clip_name)
        if animation:
            duration = max(float(self.access(s["input"])[-1, 0]) for s in animation["samplers"])
            moment = duration * phase
            for channel in animation["channels"]:
                sampler = animation["samplers"][channel["sampler"]]
                times = self.access(sampler["input"]).ravel()
                values = self.access(sampler["output"])
                i = min(max(int(np.searchsorted(times, moment, side="right")) - 1, 0), len(times)-1)
                j = min(i+1, len(times)-1)
                t = 0 if i == j else (moment-times[i]) / (times[j]-times[i])
                path = channel["target"]["path"]
                mode = sampler.get("interpolation", "LINEAR")
                if mode not in ("LINEAR", "STEP"):
                    raise ValueError(f"Unsupported interpolation: {mode}")
                value = values[i] if mode == "STEP" else (slerp(values[i], values[j], t) if path == "rotation" else values[i]*(1-t)+values[j]*t)
                trs[channel["target"]["node"]][{"translation": 0, "rotation": 1, "scale": 2}[path]] = value
        local = []
        for n, (translation, quaternion, scale) in zip(nodes, trs):
            matrix = np.array(n["matrix"], dtype=np.float32).reshape(4, 4).T if "matrix" in n else rotation(quaternion)
            if "matrix" not in n:
                matrix[:3, :3] *= scale
                matrix[:3, 3] = translation
            local.append(matrix)
        global_m = [None] * len(nodes)
        children = {child for n in nodes for child in n.get("children", [])}

        def visit(index, parent):
            global_m[index] = parent @ local[index]
            for child in nodes[index].get("children", []):
                visit(child, global_m[index])

        for index in range(len(nodes)):
            if index not in children:
                visit(index, np.eye(4, dtype=np.float32))
        triangles = []
        for ni, positions, indices, joints, weights, uv, color, texture in self.primitives:
            homogeneous = np.column_stack((positions, np.ones(len(positions))))
            skin = nodes[ni].get("skin")
            if skin is not None:
                skin = self.doc["skins"][skin]
                inverse = self.access(skin["inverseBindMatrices"]).reshape(-1, 4, 4).transpose(0, 2, 1)
                matrices = np.array([global_m[j] for j in skin["joints"]]) @ inverse
                blended = (matrices[joints] * weights[:, :, None, None]).sum(axis=1)
                transformed = np.einsum("nij,nj->ni", blended, homogeneous)[:, :3]
            else:
                transformed = (homogeneous @ global_m[ni].T)[:, :3]
            triangles.append((transformed[indices], uv[indices], color, texture))
        return triangles


def orient(triangles, direction, corpse=False):
    # Match WorldGeometry::FacingDirection and the 2:1 world-to-screen projection.
    screen_angle = direction * math.pi / 4
    sx, sy = math.sin(screen_angle), math.cos(screen_angle)
    angle = math.atan2(sx + 2*sy, -sx + 2*sy)
    c, s = math.cos(angle), math.sin(angle)
    matrix = np.array([[c, 0, s], [0, 1, 0], [-s, 0, c]])
    points = np.concatenate([p.reshape(-1, 3) for p, *_ in triangles]) if corpse else None
    center = (points.min(axis=0) + points.max(axis=0))*.5 if corpse else None
    result = []
    for positions, uv, color, texture in triangles:
        p = positions.copy()
        if corpse:
            # Roll onto the side around forward +Z, not pitch head-first around X.
            roll = np.array([[0, -1, 0], [1, 0, 0], [0, 0, 1]])
            p = (p - center) @ roll.T
        p = p @ matrix.T
        result.append((p, uv, color, texture))
    if corpse:
        floor = min(p[:, :, 1].min() for p, *_ in result)
        for p, *_ in result:
            p[:, :, 1] -= floor
    return result


PROJECTION = np.array([[math.sqrt(.5), 0, -math.sqrt(.5)],
                       [math.sqrt(.125), -math.sqrt(.75), math.sqrt(.125)],
                       [math.sqrt(.375), .5, math.sqrt(.375)]])


def raster(triangles, width, height, scale, anchor):
    pixels = np.zeros((height, width, 4), dtype=np.uint8)
    depth = np.full((height, width), -np.inf, dtype=np.float32)
    light = np.array([-.35, .85, .40])
    light /= np.linalg.norm(light)
    for positions, uvs, color, texture in triangles:
        projected = positions @ PROJECTION.T
        projected[:, :, :2] *= scale
        projected[:, :, :2] += anchor
        normals = np.cross(positions[:, 1]-positions[:, 0], positions[:, 2]-positions[:, 0])
        normals /= np.maximum(np.linalg.norm(normals, axis=1, keepdims=True), 1e-12)
        illumination = .43 + .57*np.maximum(normals @ light, 0)
        for tri, uv, shade in zip(projected, uvs, illumination):
            lo = np.maximum(np.floor(tri[:, :2].min(axis=0)).astype(int), [0, 0])
            hi = np.minimum(np.ceil(tri[:, :2].max(axis=0)).astype(int), [width-1, height-1])
            if (hi < lo).any():
                continue
            a, b, c = tri
            den = (b[1]-c[1])*(a[0]-c[0]) + (c[0]-b[0])*(a[1]-c[1])
            if abs(den) < 1e-8:
                continue
            yy, xx = np.mgrid[lo[1]:hi[1]+1, lo[0]:hi[0]+1]
            x, y = xx+.5, yy+.5
            wa = ((b[1]-c[1])*(x-c[0]) + (c[0]-b[0])*(y-c[1])) / den
            wb = ((c[1]-a[1])*(x-c[0]) + (a[0]-c[0])*(y-c[1])) / den
            wc = 1-wa-wb
            z = wa*a[2] + wb*b[2] + wc*c[2]
            mask = (wa >= -1e-5) & (wb >= -1e-5) & (wc >= -1e-5) & (z > depth[yy, xx])
            if not mask.any():
                continue
            rgb = np.broadcast_to(color, (*wa.shape, 4)).copy()
            if texture is not None:
                coords = wa[:, :, None]*uv[0] + wb[:, :, None]*uv[1] + wc[:, :, None]*uv[2]
                tx = ((coords[:, :, 0] % 1)*texture.shape[1]).astype(int) % texture.shape[1]
                ty = ((coords[:, :, 1] % 1)*texture.shape[0]).astype(int) % texture.shape[0]
                sample = texture[ty, tx].copy()
                sample[:, :, :3] **= 2.2
                rgb *= sample
            rgb[:, :, :3] = np.clip(rgb[:, :, :3]*shade, 0, 1) ** (1/2.2)
            mask &= rgb[:, :, 3] >= .5
            pixels[yy[mask], xx[mask]] = np.clip(rgb[mask]*255, 0, 255).astype(np.uint8)
            depth[yy[mask], xx[mask]] = z[mask]
    return Image.fromarray(pixels)


class Atlas:
    def __init__(self, output):
        self.output = output
        self.pages = [Image.new("RGBA", (PAGE, PAGE))]
        self.entries = []
        self.x = self.y = self.row = 0

    def add(self, key, image, anchor, scale):
        w, h = image.size
        if self.x+w > PAGE:
            self.x = 0
            self.y += self.row
            self.row = 0
        if self.y+h > PAGE:
            self.pages.append(Image.new("RGBA", (PAGE, PAGE)))
            self.x = self.y = self.row = 0
        self.pages[-1].paste(image, (self.x, self.y))
        self.entries.append(f"{key} {len(self.pages)-1} {self.x} {self.y} {w} {h} {anchor[0]:.5f} {anchor[1]:.5f} {scale:.5f}")
        self.x += w
        self.row = max(self.row, h)

    def save(self):
        for i, page in enumerate(self.pages):
            page.save(self.output/f"page_{i}.png")
        (self.output/"sprites.txt").write_text(
            f"ARCANA_SPRITES_3\n{len(self.pages)} {len(self.entries)}\n" + "\n".join(self.entries) + "\n", encoding="ascii")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, default=Path(__file__).resolve().parents[1]/"resource/ARCANA_lowpoly_v1")
    parser.add_argument("--output", type=Path, default=Path(__file__).resolve().parents[1]/"resource/ARCANA_runtime")
    args = parser.parse_args()
    inputs = list(CHARACTERS.values()) + [f"environment/{name}.glb" for name in ENVIRONMENT]
    inputs += [f"materials/textures/{name}_basecolor.png" for name in ("grass", "dirt", "stone")]
    # The material pack names soil rather than dirt in some exports.
    inputs = [name.replace("dirt_basecolor", "soil_basecolor") if not (args.source/name).exists() else name for name in inputs]
    digest = hashlib.sha256(VERSION + Path(__file__).read_bytes())
    for name in inputs:
        digest.update((args.source/name).read_bytes())
    signature = digest.hexdigest()
    args.output.mkdir(parents=True, exist_ok=True)
    stamp = args.output/"source.sha256"
    if stamp.exists() and stamp.read_text().strip() == signature:
        manifest = args.output/"sprites.txt"
        if manifest.exists():
            page_count = int(manifest.read_text().splitlines()[1].split()[0])
            if all((args.output/f"page_{i}.png").exists() for i in range(page_count)):
                print("ARCANA sprite cache is current.", flush=True)
                return
    atlas = Atlas(args.output)
    started = time.monotonic()
    for name, file in CHARACTERS.items():
        print(f"[bake] {name}", flush=True)
        model = Model(args.source/file)
        poses = {(clip, frame): model.pose(clip, frame/FRAMES) for clip in ("Idle", "Walk", "Attack") for frame in range(FRAMES)}
        all_points = np.concatenate([p.reshape(-1, 3) for pose in poses.values() for p, *_ in pose])
        radius = np.max(np.linalg.norm(all_points[:, [0, 2]], axis=1))
        scale = min(48, 84/max(2*radius, .1), 108/max(float(np.ptp(all_points[:, 1]))+radius, .1))
        anchor = (48, 108)
        for clip in ("Idle", "Walk", "Attack"):
            for direction in range(DIRECTIONS):
                for frame in range(FRAMES):
                    image = raster(orient(poses[clip, frame], direction), 96, 128, scale, anchor)
                    atlas.add(f"{name}.{clip.lower()}.{direction}.{frame}", image, anchor, scale)
        for direction in range(DIRECTIONS):
            image = raster(orient(poses["Idle", 0], direction, True), 96, 128, scale, anchor)
            atlas.add(f"{name}.dead.{direction}.0", image, anchor, scale)
    for name in ENVIRONMENT:
        print(f"[bake] {name}", flush=True)
        model = Model(args.source/f"environment/{name}.glb")
        pose = model.pose("Closed", 0)
        projected = np.concatenate([p.reshape(-1, 3) @ PROJECTION.T for p, *_ in pose])
        low, high = projected[:, :2].min(axis=0), projected[:, :2].max(axis=0)
        scale = min(80, 240/max(float((high-low).max()), .1))
        anchor = (128-(low[0]+high[0])*.5*scale, 128-(low[1]+high[1])*.5*scale)
        image = raster(pose, 256, 256, scale, anchor)
        atlas.add(name, image, anchor, scale)
    for name in ("grass", "dirt", "stone"):
        path = args.source/f"materials/textures/{name}_basecolor.png"
        if not path.exists():
            path = args.source/"materials/textures/soil_basecolor.png"
        Image.open(path).convert("RGBA").save(args.output/f"{name}.png")
    atlas.save()
    stamp.write_text(signature + "\n", encoding="ascii")
    print(f"[bake] {len(atlas.entries)} sprites, {len(atlas.pages)} pages, {time.monotonic()-started:.1f} seconds", flush=True)


if __name__ == "__main__":
    main()
