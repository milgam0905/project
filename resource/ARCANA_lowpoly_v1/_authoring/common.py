"""Blender-only asset authoring utilities. No game source dependencies."""
import bpy
import math
import json
import random
import tempfile
from pathlib import Path
from mathutils import Vector, Matrix

ROOT = Path(__file__).resolve().parents[1]
TEMP = ROOT/'_authoring'/'tmp'
TEMP.mkdir(exist_ok=True)
tempfile.tempdir=str(TEMP)
bpy.context.preferences.filepaths.temporary_directory=str(TEMP)
random.seed(742)
PALETTE = {
    'skin': ((0.48, 0.30, 0.20), 0, .86),
    'skin_pale': ((0.65, 0.47, 0.34), 0, .87),
    'skin_dark': ((0.25, 0.13, 0.08), 0, .89),
    'wool_moss': ((.16, .21, .12), 0, .97),
    'wool_slate': ((.13, .20, .24), 0, .96),
    'wool_ochre': ((.35, .25, .12), 0, .95),
    'linen': ((.49, .43, .31), 0, .98),
    'leather': ((.19, .095, .043), 0, .83),
    'leather_dark': ((.075, .048, .031), 0, .88),
    'iron': ((.18, .20, .20), .72, .63),
    'iron_edge': ((.34, .36, .34), .8, .45),
    'brass': ((.38, .27, .10), .65, .58),
    'hair': ((.058, .032, .018), 0, .95),
    'hair_ash': ((.25, .24, .20), 0, .95),
    'eye': ((.022, .028, .021), 0, .36),
    'eye_white': ((.61, .57, .43), 0, .65),
    'mouth': ((.20, .085, .055), 0, .94),
    'wood': ((.25, .145, .067), 0, .94),
    'wood_light': ((.40, .27, .13), 0, .94),
    'wood_end': ((.30, .20, .10), 0, .95),
    'stone': ((.27, .285, .26), 0, .98),
    'stone_light': ((.40, .405, .34), 0, .98),
    'plaster': ((.50, .46, .34), 0, 1),
    'thatch': ((.39, .31, .145), 0, 1),
    'shingle': ((.16, .19, .18), 0, .98),
    'clay': ((.36, .19, .10), 0, .95),
    'bark': ((.13, .095, .061), 0, 1),
    'leaf_dark': ((.065, .12, .055), 0, 1),
    'leaf': ((.13, .21, .075), 0, 1),
    'grass': ((.24, .28, .10), 0, 1),
    'soil': ((.15, .105, .062), 0, 1),
    'deer': ((.34, .20, .10), 0, 1),
    'belly': ((.51, .45, .32), 0, 1),
    'wolf': ((.22, .245, .23), 0, 1),
    'wolf_dark': ((.105, .13, .125), 0, 1),
    'boar': ((.15, .095, .06), 0, 1),
    'goat': ((.40, .36, .27), 0, 1),
    'horn': ((.46, .40, .27), 0, .86),
    'hoof': ((.052, .045, .032), 0, .88),
}
MATS = {}
ASSETS = []


def reset():
    bpy.ops.object.select_all(action='SELECT')
    bpy.ops.object.delete(use_global=False)
    for a in list(bpy.data.actions):
        bpy.data.actions.remove(a)
    bpy.context.scene.unit_settings.system = 'METRIC'
    bpy.context.scene.unit_settings.scale_length = 1
    bpy.context.scene.render.fps = 30
    bpy.context.scene['arcana_asset_version'] = '1.0'
    bpy.context.scene['axes'] = 'Blender Z up, -Y forward; GLB Y up, +Z forward'


def mat(name):
    try:
        if name in MATS and MATS[name].name in bpy.data.materials:
            return MATS[name]
    except ReferenceError:
        MATS.pop(name,None)
    color, metal, rough = PALETTE[name]
    m = bpy.data.materials.get('ARC_' + name) or bpy.data.materials.new('ARC_' + name)
    m.use_nodes = True
    p = m.node_tree.nodes.get('Principled BSDF')
    p.inputs['Base Color'].default_value = (*color, 1)
    p.inputs['Metallic'].default_value = metal
    p.inputs['Roughness'].default_value = rough
    m.diffuse_color = (*color, 1)
    MATS[name] = m
    return m


def mesh(name, vertices, faces, material, weights=None):
    d = bpy.data.meshes.new(name)
    d.from_pydata(vertices, [], faces)
    d.update()
    o = bpy.data.objects.new(name, d)
    bpy.context.collection.objects.link(o)
    o.data.materials.append(mat(material))
    if weights:
        for i, ws in enumerate(weights):
            for bone, w in ws.items():
                if w > 0:
                    g = o.vertex_groups.get(bone) or o.vertex_groups.new(name=bone)
                    g.add([i], w, 'REPLACE')
    return o


def apply_transform(o):
    bpy.ops.object.select_all(action='DESELECT')
    o.select_set(True)
    bpy.context.view_layer.objects.active = o
    bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)


def rigid(o, bone):
    if bone:
        o.vertex_groups.new(name=bone).add(list(range(len(o.data.vertices))), 1, 'REPLACE')
    return o


def box(name, loc, scale, material, bone=None, bevel=0, rot=None):
    bpy.ops.mesh.primitive_cube_add(size=1, location=loc)
    o = bpy.context.object
    o.name = name
    o.dimensions = scale
    if rot:
        o.rotation_euler = rot
    apply_transform(o)
    o.data.materials.append(mat(material))
    if bevel:
        m = o.modifiers.new('Small worn edges', 'BEVEL')
        m.width = bevel
        m.segments = 1
        bpy.ops.object.modifier_apply(modifier=m.name)
    return rigid(o, bone)


def ellipsoid(name, loc, scale, material, bone=None, segments=10, rings=6):
    bpy.ops.mesh.primitive_uv_sphere_add(segments=segments, ring_count=rings, radius=1, location=loc)
    o = bpy.context.object
    o.name = name
    o.scale = scale
    apply_transform(o)
    o.data.materials.append(mat(material))
    return rigid(o, bone)


def tube(name, points, radii, material, bone=None, n=8, ring_weights=None):
    vs, fs, ws = [], [], []
    for i, p in enumerate(points):
        tangent = Vector(points[min(i + 1, len(points)-1)]) - Vector(points[max(0, i-1)])
        tangent.normalize()
        side = tangent.cross(Vector((0, 1, 0)))
        if side.length < .05:
            side = tangent.cross(Vector((1, 0, 0)))
        side.normalize()
        other = tangent.cross(side).normalized()
        r = radii[i]
        rx, ry = (r, r) if isinstance(r, (float, int)) else r
        for j in range(n):
            a = math.tau * j/n
            v = Vector(p) + side * (rx*math.cos(a)) + other*(ry*math.sin(a))
            vs.append(tuple(v))
            ws.append(ring_weights[i] if ring_weights else ({bone: 1} if bone else {}))
        if i:
            for j in range(n):
                a = (i-1)*n+j
                b = (i-1)*n+(j+1)%n
                fs.append((a, b, i*n+(j+1)%n, i*n+j))
    fs.extend([tuple(reversed(range(n))), tuple((len(points)-1)*n+j for j in range(n))])
    return mesh(name, vs, fs, material, ws)


def rings_z(name, rings, material, n=12):
    # (z, x radius, y radius, y center, weight dictionary)
    vs, fs, weights = [], [], []
    for k, (z, rx, ry, cy, w) in enumerate(rings):
        for j in range(n):
            a = math.tau*j/n
            vs.append((rx*math.cos(a), cy+ry*math.sin(a), z))
            weights.append(w)
        if k:
            for j in range(n):
                fs.append(((k-1)*n+j, (k-1)*n+(j+1)%n, k*n+(j+1)%n, k*n+j))
    fs.extend([tuple(reversed(range(n))), tuple((len(rings)-1)*n+j for j in range(n))])
    return mesh(name, vs, fs, material, weights)


def join(name, objects):
    bpy.ops.object.select_all(action='DESELECT')
    for o in objects:
        o.hide_set(False)
        o.select_set(True)
    bpy.context.view_layer.objects.active = objects[0]
    bpy.ops.object.join()
    o = bpy.context.object
    o.name = name
    # Explicit planar/box UVs for every polygon; reusable tiling textures.
    uv = o.data.uv_layers.new(name='UV0') if not o.data.uv_layers else o.data.uv_layers[0]
    for p in o.data.polygons:
        norm = p.normal
        ax = max(range(3), key=lambda i: abs(norm[i]))
        axes = [i for i in range(3) if i != ax]
        for li in p.loop_indices:
            v = o.data.vertices[o.data.loops[li].vertex_index].co
            uv.data[li].uv = (v[axes[0]], v[axes[1]])
    o['uv_note'] = 'UV0 box projection, 1 UV unit per meter; overlaps intentional for tiling'
    return o


def armature(name, bones):
    d = bpy.data.armatures.new(name)
    o = bpy.data.objects.new(name, d)
    bpy.context.collection.objects.link(o)
    bpy.context.view_layer.objects.active = o
    o.select_set(True)
    bpy.ops.object.mode_set(mode='EDIT')
    for name, head, tail, parent in bones:
        b = d.edit_bones.new(name)
        b.head, b.tail = head, tail
        if parent:
            b.parent = d.edit_bones[parent]
        b.use_deform = not name.startswith('socket_')
    bpy.ops.object.mode_set(mode='OBJECT')
    o.show_in_front = True
    o.data.display_type = 'OCTAHEDRAL'
    return o


def bind(o, rig, slot):
    o.parent = rig
    mod = o.modifiers.new('Shared skeleton', 'ARMATURE')
    mod.object = rig
    o['slot'] = slot
    o['rig_id'] = rig.name
    o['origin_contract'] = 'Same rest-space origin; swap with identity local transform'


def reset_pose(rig):
    for p in rig.pose.bones:
        p.rotation_mode = 'XYZ'
        p.location = (0, 0, 0)
        p.rotation_euler = (0, 0, 0)
        p.scale = (1, 1, 1)


def aim_bone(rig, name, head, tail):
    p = rig.pose.bones[name]
    head, tail = Vector(head), Vector(tail)
    rest = p.bone.tail_local-p.bone.head_local
    q = rest.rotation_difference(tail-head) @ p.bone.matrix_local.to_quaternion()
    desired = Matrix.Translation(head) @ q.to_matrix().to_4x4()
    if p.parent:
        basis = (p.bone.matrix_local.inverted() @ p.parent.bone.matrix_local @
                 p.parent.matrix.inverted() @ desired)
    else:
        basis = p.bone.matrix_local.inverted() @ desired
    loc, rot, scale = basis.decompose()
    p.location = loc
    p.rotation_euler = rot.to_euler('XYZ')
    p.scale = scale
    bpy.context.view_layer.update()


def solve_knee(hip, foot, l1, l2, pole):
    h, f = Vector(hip), Vector(foot)
    delta = f-h
    d = min(delta.length, l1+l2-.0001)
    axis = delta.normalized()
    projection = (l1*l1-l2*l2+d*d)/(2*d)
    side = Vector(pole)-axis*Vector(pole).dot(axis)
    side.normalize()
    return h+axis*projection+side*math.sqrt(max(0,l1*l1-projection*projection))


def key_pose(rig, frame):
    for p in rig.pose.bones:
        p.keyframe_insert(data_path='location', frame=frame, group=p.name)
        p.keyframe_insert(data_path='rotation_euler', frame=frame, group=p.name)


def action(rig, name, frames, animate, loop=False):
    rig.animation_data_create()
    a = bpy.data.actions.new(name)
    a.use_fake_user = True
    rig.animation_data.action = a
    a['loop'] = loop
    a['fps'] = 30
    for f in range(1, frames+1):
        reset_pose(rig)
        animate((f-1)/(frames-1), f)
        key_pose(rig, f)
    a.use_frame_range = True
    a.frame_start, a.frame_end = 1, frames
    rig.animation_data.action = None
    reset_pose(rig)
    return a


def save(path):
    bpy.context.scene.frame_set(1)
    bpy.context.preferences.filepaths.save_version = 0
    bpy.context.preferences.filepaths.file_preview_type = 'NONE'
    bpy.ops.wm.save_as_mainfile(filepath=str(ROOT / path))


def export(path, objects, animated=True):
    bpy.ops.object.select_all(action='DESELECT')
    for o in objects:
        o.hide_set(False)
        o.hide_viewport = False
        o.hide_render = False
        o.select_set(True)
    bpy.context.view_layer.objects.active = objects[0]
    bpy.ops.export_scene.gltf(
        filepath=str(ROOT/path), export_format='GLB', use_selection=True,
        export_animations=animated, export_animation_mode='ACTIONS',
        export_skins=animated, export_yup=True, export_extras=True,
        export_force_sampling=True, export_frame_step=1,
        export_def_bones=False, export_materials='EXPORT',
    )


def json_out(path, obj):
    (ROOT/path).write_text(json.dumps(obj, indent=2, ensure_ascii=False), encoding='utf-8')


def stage(objects, path, target=(0, 0, 1), distance=5, resolution=(1400, 1100),samples=32):
    for o in list(bpy.data.objects):
        if o.type in {'CAMERA', 'LIGHT'} or o.name.startswith('PREVIEW_'):
            bpy.data.objects.remove(o, do_unlink=True)
    bpy.context.scene.render.engine = 'CYCLES'
    bpy.context.scene.cycles.samples = samples
    bpy.context.scene.cycles.use_denoising = True
    bpy.context.scene.render.threads_mode = 'FIXED'
    bpy.context.scene.render.threads = 4
    bpy.context.scene.render.resolution_x, bpy.context.scene.render.resolution_y = resolution
    bpy.context.scene.render.resolution_percentage = 100
    bpy.context.scene.world.color = (.25, .25, .25)
    world = bpy.context.scene.world
    world.use_nodes = True
    world.node_tree.nodes['Background'].inputs[0].default_value = (.15, .18, .20, 1)
    world.node_tree.nodes['Background'].inputs[1].default_value = .45
    floor = box('PREVIEW_floor', (target[0], target[1], -.05), (200, 200, .1), 'stone')
    floor.data.materials.clear()
    m = bpy.data.materials.new('Preview dark ground')
    m.diffuse_color = (.045, .054, .053, 1)
    m.use_nodes = True
    m.node_tree.nodes['Principled BSDF'].inputs['Base Color'].default_value = m.diffuse_color
    floor.data.materials.append(m)
    for loc, power, size, color in [((3,-4,7), 950, 5, (1,.80,.61)), ((-4,-1,4),600,4,(.61,.76,1)), ((1,4,6),1100,3,(1,.9,.70))]:
        bpy.ops.object.light_add(type='AREA', location=Vector(target)+Vector(loc)*distance/5)
        l = bpy.context.object
        l.data.energy = power*(distance/5)**2
        l.data.shape = 'DISK'
        l.data.size = size*distance/5
        l.data.color = color
        l.rotation_euler = (Vector(target)-l.location).to_track_quat('-Z', 'Y').to_euler()
    bpy.ops.object.camera_add(location=Vector(target)+Vector((distance*.8, -distance*1.3, distance*.65)))
    cam = bpy.context.object
    cam.rotation_euler = (Vector(target)-cam.location).to_track_quat('-Z','Y').to_euler()
    cam.data.type = 'ORTHO'
    cam.data.ortho_scale = distance
    bpy.context.scene.camera = cam
    bpy.context.scene.view_settings.view_transform = 'AgX'
    bpy.context.scene.render.image_settings.file_format = 'PNG'
    bpy.context.scene.render.filepath = str(ROOT/path)
    bpy.ops.render.render(write_still=True)


def register(name, category, objects, path):
    entry = {'id': name, 'category': category, 'path': path,
             'triangles': sum(len(p.vertices)-2 for o in objects if o.type=='MESH' for p in o.data.polygons),
             'meshes': len([o for o in objects if o.type=='MESH'])}
    ASSETS.append(entry)
    return entry
