import sys
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parent))
from common import *


def build():
    folder=ROOT/'previews'/'poses'
    folder.mkdir(exist_ok=True)
    for family,prefix,file in [('human','HUM','characters/human_modular.blend'),('wolf','WOLF','animals/wolf.blend')]:
        if '--wolf-only' in sys.argv and family!='wolf':
            continue
        bpy.ops.wm.open_mainfile(filepath=str(ROOT/file))
        MATS.clear()
        rig=next(o for o in bpy.context.scene.objects if o.type=='ARMATURE')
        for index,(clip,frame) in enumerate([('Idle',1),('Walk',7),('Run',6),('Turn_L90',13),('Attack',22),('Jump',22)]):
            a=bpy.data.actions[f'{prefix}_{clip}']
            rig.animation_data.action=a
            rig.animation_data.action_slot=a.slots[0]
            bpy.context.scene.frame_set(frame)
            if index==0:
                stage([],f'previews/poses/{family}_{clip}.png',target=(0,-.05,1 if family=='human' else .6),
                      distance=2.8 if family=='human' else 2.65,resolution=(600,700),samples=8)
            else:
                bpy.context.scene.render.filepath=str(folder/f'{family}_{clip}.png')
                bpy.ops.render.render(write_still=True)
        # Actual frame animation previews are rendered from Blender skin deformation.
        a=bpy.data.actions[f'{prefix}_Walk']
        rig.animation_data.action=a
        rig.animation_data.action_slot=a.slots[0]
        bpy.context.scene.render.resolution_x=400
        bpy.context.scene.render.resolution_y=460
        bpy.context.scene.cycles.samples=4
        frames=ROOT/'previews'/'walk_frames'/family
        frames.mkdir(parents=True,exist_ok=True)
        end=30 if family=='human' else 36
        for f in range(1,end+1,2):
            bpy.context.scene.frame_set(f)
            bpy.context.scene.render.filepath=str(frames/f'{f:03d}.png')
            bpy.ops.render.render(write_still=True)


if __name__=='__main__':
    build()
