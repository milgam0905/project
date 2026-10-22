import sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parent))
from common import *
import numpy as np


def build():
    reset()
    n=512
    yy,xx=np.mgrid[0:n,0:n]/n
    rng=np.random.default_rng(742)

    def noise(freqs):
        field=np.zeros((n,n),np.float32)
        for freq,amplitude in freqs:
            for j in range(6):
                kx=int(rng.integers(1,freq+1));ky=int(rng.integers(1,freq+1))
                phase=rng.uniform(0,math.tau)
                field += np.sin(math.tau*(kx*xx+ky*yy)+phase)*amplitude/6
        return field

    broad=noise([(4,.7),(13,.3),(47,.13)])
    fine=noise([(80,.13),(140,.07)])
    configs={
        'wood':('wood',.88,.18), 'bark':('bark',.98,.27), 'stone':('stone',.94,.32),
        'soil':('soil',.99,.23), 'grass':('grass',.97,.19), 'plaster':('plaster',.96,.10),
        'thatch':('thatch',.98,.19), 'moss':('leaf_dark',.99,.22),
    }
    inventory=[]
    for name,(base,rough,strength) in configs.items():
        if name in ['wood','bark','thatch']:
            strips=np.sin(math.tau*(xx*(24 if name=='wood' else 41)+.16*np.sin(math.tau*yy*3)))
            height=broad*.18+strips*.13+fine*.2
            if name=='bark':
                height += np.maximum(0,-strips-.45)*-.65
        elif name=='stone':
            height=broad*.65+fine*.45
        elif name in ['soil','grass','moss']:
            height=broad*.6+fine*.8
        else:
            height=broad*.12+fine*.25
        h=(height-height.min())/(height.max()-height.min())
        col=np.array(PALETTE[base][0])
        # Palette values are linear; color PNGs must be encoded as sRGB.
        color_linear=np.clip(col[None,None,:]*(.76+h[:,:,None]*.48),0,1)
        color=np.where(color_linear<=.0031308,color_linear*12.92,1.055*color_linear**(1/2.4)-.055)
        roughness=np.clip(rough+(h-.5)*.12,0,1)
        dx=(np.roll(h,-1,1)-np.roll(h,1,1))*strength*10
        dy=(np.roll(h,-1,0)-np.roll(h,1,0))*strength*10
        normal=np.stack([-dx,-dy,np.ones_like(dx)],axis=-1)
        normal/=np.linalg.norm(normal,axis=-1,keepdims=True)
        normal=normal*.5+.5
        imgs={}
        for suffix,pixels in [('basecolor',color),('roughness',np.repeat(roughness[:,:,None],3,axis=-1)),('normal',normal)]:
            img=bpy.data.images.new(f'{name}_{suffix}',width=n,height=n,alpha=True)
            img.colorspace_settings.name='sRGB' if suffix=='basecolor' else 'Non-Color'
            rgba=np.concatenate([pixels,np.ones((n,n,1))],axis=-1).astype(np.float32)
            img.pixels.foreach_set(rgba.ravel())
            img.filepath_raw=str(ROOT/f'materials/textures/{name}_{suffix}.png')
            img.file_format='PNG';img.save();img.pack()
            imgs[suffix]=img
        m=mat(base if name!='moss' else 'leaf_dark')
        m.name='ARC_PBR_'+name
        nodes=m.node_tree.nodes;links=m.node_tree.links
        p=nodes.get('Principled BSDF')
        for suffix in imgs:
            tex=nodes.new('ShaderNodeTexImage');tex.image=imgs[suffix];tex.extension='REPEAT'
            if suffix=='basecolor':
                links.new(tex.outputs['Color'],p.inputs['Base Color'])
            elif suffix=='roughness':
                links.new(tex.outputs['Color'],p.inputs['Roughness'])
            else:
                nm=nodes.new('ShaderNodeNormalMap');nm.inputs['Strength'].default_value=.45
                links.new(tex.outputs['Color'],nm.inputs['Color']);links.new(nm.outputs['Normal'],p.inputs['Normal'])
        m.asset_mark()
        m['tile_size_m']=1
        m['normal_convention']='OpenGL +Y tangent space'
        o=box(f'Material_{name}',((len(inventory)%4)*1.5,(len(inventory)//4)*1.5,.05),(1.2,1.2,.1),base)
        o.data.materials.clear();o.data.materials.append(m)
        join(o.name,[o])
        inventory.append({'name':name,'resolution':[512,512],'tileable':True,'maps':['basecolor_sRGB','roughness_linear','normal_OpenGL_linear']})
    save('materials/arcana_pbr_materials.blend')
    stage([], 'previews/materials.png',target=(2.25,.75,0),distance=7,resolution=(1400,800))
    json_out('metadata/materials.json',inventory)


if __name__=='__main__':
    build()
