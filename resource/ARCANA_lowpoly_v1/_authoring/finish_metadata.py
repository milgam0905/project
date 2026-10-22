import json
import struct
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]


def write(name,data):
    (ROOT/name).write_text(json.dumps(data,ensure_ascii=False,indent=2),encoding='utf-8')


def animations():
    families={}
    for family,prefix,duration,height in [('human','HUM',1,1.8),('deer','DEER',1.2,1.25),
                                          ('wolf','WOLF',1.2,.82),('boar','BOAR',1.2,.75),('goat','GOAT',1.2,.88)]:
        human=family=='human'
        run_dur=20/30 if human else (22/30 if family in ['deer','goat'] else .6)
        walk_speed=.45/.62 if human else height*.43/(.67*1.2)
        run_speed=.72/(.4*run_dur) if human else height*.85/(.43*run_dur)
        clips={
            'reference':{'clip':prefix+('_Reference_A' if human else '_Reference'),'frames':[1,2],'loop':False,'purpose':'Rest/bind reference; not locomotion idle'},
            'turn_base':{'clip':prefix+'_Turn_Base','frames':[1,2],'loop':False,'purpose':'Neutral standing pose for turn interpolation and local bone-space delta reference'},
            'idle':{'clip':prefix+'_Idle','frames':[1,61],'loop':True,'duration_s':2},
            'walk':{'clip':prefix+'_Walk','frames':[1,31 if human else 37],'loop':True,'duration_s':duration,'nominal_speed_mps':round(walk_speed,4),'stance_fraction':.62 if human else .67},
            'run':{'clip':prefix+'_Run','frames':[1,21 if human else (23 if family in ['deer','goat'] else 19)],'loop':True,'duration_s':run_dur,'nominal_speed_mps':round(run_speed,4),'stance_fraction':.4 if human else .43},
            'turn_left':{'clip':prefix+'_Turn_L90','frames':[1,25 if human else 31],'loop':False,'root_yaw_degrees':90},
            'turn_right':{'clip':prefix+'_Turn_R90','frames':[1,25 if human else 31],'loop':False,'root_yaw_degrees':-90},
            'attack':{'clip':prefix+'_Attack','frames':[1,31],'loop':False,'events':{'windup_end':12,'strike_peak':22,'recover_end':31}},
            'jump':{'clip':prefix+'_Jump','frames':[1,43],'loop':False,'events':{'takeoff':10,'apex':22,'landing':34 if human else 35,'recover_end':43},'vertical_root_motion':True},
        }
        if human:
            walk_contacts={'foot.L':0,'foot.R':.5}
            run_contacts=walk_contacts
        else:
            walk_contacts={'front_paw.L':0,'hind_paw.L':.25,'front_paw.R':.5,'hind_paw.R':.75}
            run_contacts=({'front_paw.L':0,'hind_paw.R':.35,'hind_paw.L':.48,'front_paw.R':.87}
                          if family in ['deer','goat'] else {'front_paw.L':0,'hind_paw.R':0,'front_paw.R':.5,'hind_paw.L':.5})
        families[family]={'fps':30,'clips':clips,'walk_contact_phase':walk_contacts,'run_contact_phase':run_contacts,
            'walk_run_blend':{'duration_s':.20 if human else .24,'phase_mode':'normalized_shared_phase',
                'formula':'weight_run=clamp((speed-walk.nominal_speed_mps)/(run.nominal_speed_mps-walk.nominal_speed_mps),0,1)',
                'playback_rate':'mixed_nominal_speed=(1-weight_run)*walk_speed+weight_run*run_speed; phase_rate=speed / ((1-weight_run)*walk_stride+weight_run*run_stride)',
                'walk_stride_m':walk_speed*duration,'run_stride_m':run_speed*run_dur,
                'gait_change_note':'Deer/goat walk-to-bound has different contact order; prefer a hind-contact handoff and foot locking for final game integration.'}}
    write('metadata/animation_blending.json',{'schema':'arcana.animation.v1','fps':30,'axes':{'blend':'Z up, -Y forward, meters','glb':'Y up, +Z forward, meters'},
        'families':families,'transitions':[
            {'from':'idle','to':'walk','crossfade_s':.18,'sync':'Start at left front/left foot strike'},
            {'from':'walk','to':'run','crossfade_s':.20,'sync':'Retain normalized gait phase; see species gait change note'},
            {'from':'run','to':'walk','crossfade_s':.24,'sync':'Retain phase and decelerate world displacement together'},
            {'from':'walk/run','to':'idle','crossfade_s':.20,'sync':'Prefer next planted foot; optional foot locking'},
            {'from':'idle','to':'turn_left/turn_right','crossfade_s':.12,'sync':'Consume root yaw exactly once'},
            {'from':'turn_left/turn_right','to':'idle','crossfade_s':.14,'sync':'Accumulate final yaw into controller; remove/reset clip root yaw before next idle'},
            {'from':'locomotion','to':'jump','crossfade_s':.10,'sync':'Preserve horizontal velocity externally; root Z provides vertical arc'},
            {'from':'jump','to':'locomotion','crossfade_s':.16,'sync':'Blend after landing; start target gait at a planted contact'},
            {'from':'locomotion','to':'attack','crossfade_s':.12,'sync':'Full body attack; stop or reduce movement'},
            {'from':'attack','to':'locomotion','crossfade_s':.18,'sync':'After recovery marker'}],
        'rotation_interpolation':{'rotation_format':'GLB quaternion channels','method':'shortest-arc slerp with quaternion sign correction',
            'neutral_clip':'Turn_Base','rules':['Turn_Base is a reference pose, not an additive animation by itself','For additive turning calculate local rotation delta against Turn_Base','Turn clips include real root yaw; choose animation root authority or controller authority once','For an arbitrary target yaw scale the root yaw delta; large angles should chain turns','Idle/walk/run can be directionally reoriented by a controller; no strafing clips are supplied']},
        'root_motion':{'walk_run':'In-place horizontal locomotion; controller supplies translation from nominal stride speed','turn':'Root yaw +90/-90, ends at new heading','jump':'Transient vertical root displacement returns to ground; horizontal velocity is external','animal_attack':'Transient local forward root excursion returns to origin; not a permanent dash'},
        'runtime_status':'Authoring data only. No mixer, IK foot locking, AI, collision or engine integration implemented.'})


def inventory():
    result={'schema':'arcana.asset_pack.v1','created':'2026-10-21','authoring':'Blender 5.2.2 LTS','units':'meters','assets':[]}
    for p in sorted(ROOT.rglob('*.glb')):
        b=p.read_bytes();n=struct.unpack_from('<I',b,12)[0];d=json.loads(b[20:20+n])
        tris=sum(d['accessors'][q['indices']]['count']//3 for m in d.get('meshes',[]) for q in m['primitives'])
        result['assets'].append({'file':p.relative_to(ROOT).as_posix(),'bytes':len(b),'triangles':tris,'meshes':len(d.get('meshes',[])),
                                 'skins':len(d.get('skins',[])),'animations':[a['name'] for a in d.get('animations',[])]})
    result['counts']={'glb':len(result['assets']),'blend':len(list(ROOT.rglob('*.blend'))),'texture_png':len(list((ROOT/'materials/textures').glob('*.png')))}
    write('metadata/asset_manifest.json',result)


if __name__=='__main__':
    animations();inventory()
