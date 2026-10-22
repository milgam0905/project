import sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parent))
from common import *


def build():
    entries=[]
    for kind in ['door','chest']:
        reset()
        if kind=='door':
            rig=armature('ARC_Door_v1',[('root',(0,0,0),(0,.12,0),None),('hinge',(0,0,0),(0,0,.3),'root')])
            pieces=[]
            for i in range(6):
                pieces += [box('door plank',(.073+i*.148,0,1.0),(.142,.075,2.0),'wood','hinge',.005)]
            for z in [.4,1.55]:
                pieces += [box('strap',(.44,-.05,z),(.86,.024,.055),'iron','hinge')]
                for x in [.05,.77]:
                    pieces += [ellipsoid('rivet',(x,-.069,z),(.013,.01,.013),'iron_edge','hinge',6,4)]
            pieces += [ellipsoid('handle',(.76,-.07,1.0),(.03,.028,.05),'iron','hinge',8,4)]
            o=join('door_leaf',pieces);bind(o,rig,'leaf')
            objects=[rig,o]
        else:
            rig=armature('ARC_Chest_v1',[('root',(0,0,0),(0,.12,0),None),('lid',(0,.29,.46),(.3,.29,.46),'root')])
            body=[box('bottom',(0,0,.045),(.88,.64,.09),'wood','root')]
            for x in [-.41,.41]:
                body += [box('side',(x,0,.26),(.06,.64,.43),'wood','root')]
            for y in [-.29,.29]:
                body += [box('side',(0,y,.26),(.88,.06,.43),'wood','root')]
            for x in [-.29,.29]:
                body += [box('strap',(x,-.325,.25),(.055,.025,.43),'iron','root')]
            lid=[box('lid',(0,0,.51),(.90,.66,.10),'wood_light','lid',.018)]
            for x in [-.29,.29]:
                lid += [box('lid strap',(x,0,.573),(.055,.65,.025),'iron','lid')]
            lid += [box('latch',(0,-.34,.49),(.08,.028,.16),'iron','lid')]
            o=join('chest_body',body);bind(o,rig,'body')
            lid=join('chest_lid',lid);bind(lid,rig,'lid')
            objects=[rig,o,lid]
        def pose(t,opening=True):
            u=t*t*(3-2*t)
            if not opening:
                u=1-u
            if kind=='door':
                # Vertical hinge's local Y is world Z.
                rig.pose.bones['hinge'].rotation_euler.y=-math.pi*.52*u
            else:
                # Lid bone is +X; local Y follows world X.
                rig.pose.bones['lid'].rotation_euler.y=-math.pi*.58*u
        action(rig,kind.upper()+'_Closed',2,lambda t,f:None)
        action(rig,kind.upper()+'_Open',31,lambda t,f:pose(t,True))
        action(rig,kind.upper()+'_Close',31,lambda t,f:pose(t,False))
        action(rig,kind.upper()+'_Hold_Open',2,lambda t,f:pose(1,True))
        export(f'environment/{kind}_rigged.glb',objects)
        save(f'environment/{kind}_rigged.blend')
        entries.append(register(kind+'_rigged','interactive',objects,f'environment/{kind}_rigged.glb'))
    json_out('metadata/interactive_inventory.json',entries)


if __name__=='__main__':
    build()
