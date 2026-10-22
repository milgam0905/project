"""Apply final reviewed attack poses without rebuilding any geometry."""
import sys
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parent))
from common import *


def build():
    for family in ['human','deer','wolf','boar','goat']:
        source='characters/human_modular.blend' if family=='human' else f'animals/{family}.blend'
        bpy.ops.wm.open_mainfile(filepath=str(ROOT/source))
        MATS.clear()
        rig=next(o for o in bpy.context.scene.objects if o.type=='ARMATURE')
        prefix='HUM' if family=='human' else family.upper()
        a=bpy.data.actions[prefix+'_Attack']
        rig.animation_data.action=a
        rig.animation_data.action_slot=a.slots[0]
        for f in range(1,32):
            bpy.context.scene.frame_set(f)
            t=(f-1)/30
            if family=='human':
                forearm=rig.pose.bones['forearm.R']
                wrist=forearm.tail.copy()
                aim_bone(rig,'hand.R',wrist,wrist+(forearm.tail-forearm.head).normalized()*rig.data.bones['hand.R'].length)
                p=rig.pose.bones['hand.R']
                p.keyframe_insert(data_path='location',frame=f,group=p.name)
                p.keyframe_insert(data_path='rotation_euler',frame=f,group=p.name)
            else:
                anticip=math.sin(math.pi*min(t/.4,1)) if t<.4 else 0
                strike=math.sin(math.pi*(t-.4)/.6) if t>=.4 else 0
                for name,angle in [('neck',-.25*anticip+.45*strike),('head',-.12*anticip+.20*strike)]:
                    p=rig.pose.bones[name]
                    p.rotation_euler.x=angle
                    p.keyframe_insert(data_path='rotation_euler',frame=f,group=name)
        rig.animation_data.action=None
        reset_pose(rig)
        meshes=[o for o in bpy.context.scene.objects if o.type=='MESH' and o.parent==rig]
        visible={o.name:(o.hide_get(),o.hide_render) for o in meshes}
        if family=='human':
            for o in meshes:
                export(f'characters/parts/{o.name}.glb',[rig,o])
            meta=json.loads((ROOT/'metadata/human_parts.json').read_text(encoding='utf-8'))
            for name,ids in meta['presets'].items():
                export(f'characters/presets/{name}.glb',[rig]+[bpy.data.objects['HUM_'+i] for i in ids])
        else:
            primary=[o for o in meshes if not visible[o.name][1]]
            export(f'animals/{family}.glb',[rig]+primary)
            for o in meshes:
                export(f'animals/{family}_parts/{o.name}.glb',[rig,o])
        for o in meshes:
            o.hide_set(visible[o.name][0]);o.hide_render=visible[o.name][1]
        rig.animation_data.action=bpy.data.actions[prefix+'_Idle']
        rig.animation_data.action_slot=rig.animation_data.action.slots[0]
        save(source)
        if family in ['human','wolf']:
            rig.animation_data.action=a
            rig.animation_data.action_slot=a.slots[0]
            bpy.context.scene.frame_set(22)
            stage([],f'previews/poses/{family}_Attack.png',target=(0,-.05,1 if family=='human' else .6),
                  distance=2.8 if family=='human' else 2.65,resolution=(600,700),samples=8)


if __name__=='__main__':
    build()
