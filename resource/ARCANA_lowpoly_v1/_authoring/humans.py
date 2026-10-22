import sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parent))
from common import *


def build():
    reset()
    bones = [
        ('root',(0,0,0),(0,.18,0),None),
        ('pelvis',(0,0,.96),(0,0,1.08),'root'),
        ('spine',(0,0,1.08),(0,0,1.29),'pelvis'),
        ('chest',(0,0,1.29),(0,0,1.49),'spine'),
        ('neck',(0,0,1.49),(0,0,1.565),'chest'),
        ('head',(0,0,1.565),(0,0,1.80),'neck'),
    ]
    for s, sign in [('L',1),('R',-1)]:
        bones += [
            (f'clavicle.{s}',(sign*.035,0,1.46),(sign*.225,0,1.445),'chest'),
            (f'upper_arm.{s}',(sign*.225,0,1.445),(sign*.395,0,1.205),f'clavicle.{s}'),
            (f'forearm.{s}',(sign*.395,0,1.205),(sign*.535,-.012,.997),f'upper_arm.{s}'),
            (f'hand.{s}',(sign*.535,-.012,.997),(sign*.585,-.018,.89),f'forearm.{s}'),
            (f'thigh.{s}',(sign*.112,0,.96),(sign*.112,-.025,.515),'pelvis'),
            (f'shin.{s}',(sign*.112,-.025,.515),(sign*.112,0,.115),f'thigh.{s}'),
            (f'foot.{s}',(sign*.112,0,.115),(sign*.112,-.16,.055),f'shin.{s}'),
            (f'toe.{s}',(sign*.112,-.16,.055),(sign*.112,-.23,.045),f'foot.{s}'),
        ]
    bones += [('socket_hand.R',(-.565,-.025,.95),(-.565,-.1,.95),'hand.R'),
              ('socket_back',(0,.12,1.35),(0,.12,1.48),'chest'),
              ('socket_belt',(0,-.12,1.005),(0,-.12,1.07),'pelvis')]
    rig = armature('ARC_Human_v1', bones)
    rig['proportions'] = '1.80m / 7 head units (head crown to chin = 0.257m)'
    rig['rig_family'] = 'ARC_Human_v1'
    parts = {}

    def part(slot, variant, pieces):
        o = join(f'HUM_{slot}_{variant}', pieces)
        bind(o, rig, slot)
        o['variant'] = variant
        o['seam_contract'] = 'human_seams_v1; head z1.543, neck z1.49, waist z1.03, wrist xyz per skeleton'
        parts[o.name] = o
        return o

    # Same seam coordinates for all interchangeable skin and face variants.
    for skin in ['skin', 'skin_pale', 'skin_dark']:
        head = rings_z('head', [
            (1.543,.065,.059,-.008,{'head':1}),
            (1.565,.081,.071,-.008,{'head':1}),
            (1.605,.105,.082,0,{'head':1}),
            (1.672,.116,.094,0,{'head':1}),
            (1.74,.101,.087,.008,{'head':1}),
            (1.785,.064,.061,.008,{'head':1}),
            (1.8,.009,.01,.008,{'head':1}),
        ], skin, 12)
        pieces = [head,
            ellipsoid('ear.L',(.113,.004,1.66),(.019,.028,.038),skin,'head',8,4),
            ellipsoid('ear.R',(-.113,.004,1.66),(.019,.028,.038),skin,'head',8,4)]
        part('head',skin,pieces)
        pieces = [tube('neck',[(0,0,1.475),(0,0,1.565)],[.057,.054],skin,
                            ring_weights=[{'neck':1},{'head':.35,'neck':.65}])]
        for side,sgn in [('L',1),('R',-1)]:
            h = f'hand.{side}'
            pieces += [tube('palm',[(sgn*.527,-.012,1.01),(sgn*.57,-.018,.915),(sgn*.585,-.018,.89)],
                           [(.037,.03),(.039,.027),(.023,.018)],skin,h),
                       ellipsoid('thumb',(sgn*.532,-.041,.942),(.022,.026,.041),skin,h,8,4)]
        # Neck and hands split into their own slots to support gloves.
        part('neck',skin,[pieces[0]])
        part('hands',skin,pieces[1:])

    for style in ['stern','soft','weathered']:
        pieces=[]
        for x in [-.046,.046]:
            pieces += [ellipsoid('eye socket',(x,-.082,1.676),(.032,.013,.017),'skin','head',8,4),
                       ellipsoid('eye',(x,-.093,1.677),(.023,.007,.009),'eye_white','head',8,4),
                       ellipsoid('iris',(x,-.099,1.677),(.008,.004,.008),'eye','head',8,4),
                       box('brow',(x,-.094,1.699),(.051,.014,.009),'hair','head',rot=(0,-.12 if x>0 else .12,0))]
        pieces += [mesh('nose',[(-.014,-.085,1.68),(.014,-.085,1.68),(-.018,-.092,1.627),(.018,-.092,1.627),(0,-.126,1.631)],
                        [(0,1,4),(1,3,4),(3,2,4),(2,0,4)],'skin',[{'head':1}]*5),
                   box('mouth',(0,-.079,1.600),(.049 if style!='soft' else .041,.009,.008),'mouth','head')]
        if style=='weathered':
            pieces += [box('scar',(-.061,-.094,1.649),(.007,.008,.042),'mouth','head',rot=(0,.3,0))]
        if style=='stern':
            pieces += [tube('mustache',[(-.031,-.083,1.616),(0,-.092,1.621),(.031,-.083,1.616)],[.009,.011,.009],'hair','head',6)]
        base_face=part('face',style,pieces)
        for skin in ['skin_pale','skin_dark']:
            copy=base_face.copy()
            copy.data=base_face.data.copy()
            bpy.context.collection.objects.link(copy)
            copy.name=f'HUM_face_{style}_{skin}'
            copy['variant']=style+'_'+skin
            for i,m in enumerate(copy.data.materials):
                if m==mat('skin'):
                    copy.data.materials[i]=mat(skin)
            parts[copy.name]=copy

    for style,material in [('cropped','hair'),('swept','hair'),('grey','hair_ash')]:
        vs,fs=[],[]
        n=12
        for layer in range(3):
            for j in range(n):
                a=math.tau*j/n
                if layer==0:
                    z=1.705+(.032 if math.sin(a)<-.45 else -.025)
                    r=1
                elif layer==1:
                    z=1.78+(0.018*math.cos(a) if style=='swept' else 0)
                    r=.76
                else:
                    z=1.813
                    r=.12
                vs.append((.121*r*math.cos(a),.009+.102*r*math.sin(a),z))
        for k in range(2):
            for j in range(n):
                fs.append((k*n+j,k*n+(j+1)%n,(k+1)*n+(j+1)%n,(k+1)*n+j))
        fs.append(tuple(24+j for j in range(n)))
        part('hair',style,[mesh('haircap',vs,fs,material,[{'head':1}]*len(vs))])

    for variant,cloth in [('traveler','wool_moss'),('villager','linen'),('guard','wool_slate'),('smith','wool_ochre')]:
        torso = rings_z('tunic',[
            (.905,.223,.138,0,{'pelvis':1}),
            (1.03,.176,.114,0,{'pelvis':.75,'spine':.25}),
            (1.12,.175,.116,0,{'spine':.9,'pelvis':.1}),
            (1.30,.224,.136,0,{'spine':.4,'chest':.6}),
            (1.42,.235,.124,0,{'chest':1}),
            (1.47,.199,.105,0,{'chest':1}),
            (1.49,.061,.06,0,{'chest':.75,'neck':.25}),
        ],cloth)
        pieces=[torso]
        for side,sgn in [('L',1),('R',-1)]:
            ua,fa=f'upper_arm.{side}',f'forearm.{side}'
            pieces += [tube('sleeve',[(sgn*.201,0,1.445),(sgn*.29,0,1.35),(sgn*.395,0,1.205),
                                      (sgn*.465,-.006,1.10),(sgn*.53,-.012,1.01)],
                           [.088,.079,.063,.055,.041],cloth,
                           ring_weights=[{ua:.75,'chest':.25},{ua:1},{ua:.5,fa:.5},{fa:1},{fa:1}])]
        # Visible seams and front lacing add readable detail at quarter view.
        pieces += [box('placket',(0,-.128,1.347),(.035,.014,.18),'leather','chest')]
        for z in [1.29,1.33,1.37,1.41]:
            pieces += [box('stitch',(0,-.139,z),(.045,.008,.005),'linen','chest')]
        part('torso',variant,pieces)

    for variant,material in [('wool','wool_slate'),('leather','leather_dark')]:
        pieces=[rings_z('seat',[(.86,.178,.115,0,{'pelvis':1}),(1.055,.174,.113,0,{'pelvis':1})],material)]
        for side,s in [('L',1),('R',-1)]:
            th,sh=f'thigh.{side}',f'shin.{side}'
            pieces += [tube('trouser',[(s*.112,0,.956),(s*.112,-.01,.77),(s*.112,-.025,.515),(s*.112,-.005,.34)],
                           [(.103,.108),(.09,.087),(.067,.065),(.06,.06)],material,
                           ring_weights=[{th:.7,'pelvis':.3},{th:1},{th:.5,sh:.5},{sh:1}])]
        part('legs',variant,pieces)

    for variant in ['travel','wrapped','guard']:
        pieces=[]
        for side,s in [('L',1),('R',-1)]:
            sh,ft=f'shin.{side}',f'foot.{side}'
            pieces += [tube('bootshaft',[(s*.112,0,.10),(s*.112,0,.22),(s*.112,-.003,.39)],
                           [.066,.069,.073],'leather',ring_weights=[{ft:.6,sh:.4},{sh:1},{sh:1}]),
                       box('boot sole',(s*.112,-.067,.043),(.145,.29,.067),'leather_dark',ft,.018),
                       ellipsoid('boot upper',(s*.112,-.063,.101),(.078,.152,.079),'leather',ft,10,5)]
            if variant=='wrapped':
                for z in [.23,.275,.32,.365]:
                    pieces += [tube('leg wrap',[(s*.112,0,z-.012),(s*.112,0,z+.012)],[.075,.075],'linen',sh)]
            if variant=='guard':
                pieces += [box('shin plate',(s*.112,-.062,.295),(.104,.036,.19),'iron',sh,.012)]
        part('boots',variant,pieces)

    belt=[rings_z('belt',[(1.01,.187,.128,0,{'pelvis':1}),(1.065,.18,.124,0,{'pelvis':1})],'leather_dark'),
          box('buckle',(0,-.13,1.038),(.071,.023,.049),'brass','pelvis',.006),
          box('pouch',(.19,-.02,.963),(.12,.09,.135),'leather','pelvis',.017),
          box('pouch flap',(.19,-.069,.996),(.126,.013,.052),'leather_dark','pelvis',.005)]
    part('belt','pouch',belt)
    part('outer','smith_apron',[
        mesh('apron',[(-.145,-.139,1.37),(.145,-.139,1.37),(.182,-.167,.77),(-.182,-.167,.77)],[(0,1,2,3)],
             'leather',[{'chest':1},{'chest':1},{'pelvis':1},{'pelvis':1}]),
        tube('apron neck strap',[(-.11,-.12,1.37),(-.09,-.08,1.47),(.09,-.08,1.47),(.11,-.12,1.37)],
             [.012]*4,'leather_dark','chest',6)])
    guard=[]
    for s,side in [(1,'L'),(-1,'R')]:
        guard += [ellipsoid('pauldron',(s*.23,0,1.449),(.12,.155,.065),'iron',f'upper_arm.{side}',8,4)]
    guard += [box('breastplate',(0,-.126,1.30),(.33,.065,.25),'iron','chest',.035),
              box('crest',(0,-.165,1.33),(.043,.012,.083),'brass','chest',.005)]
    part('outer','guard_armor',guard)
    # Hood is open at the front and does not conceal the face.
    hood=[]
    for i in range(9):
        a=math.radians(-12+i*25.5)
        hood += [ellipsoid('hood panel',(.131*math.cos(a),.017+.12*math.sin(a),1.714),(.038,.042,.106),'wool_moss','head',6,4)]
    part('hair','hood',hood)
    part('weapon','sword',[
        box('sword grip',(-.564,-.027,.94),(.036,.04,.17),'leather_dark','hand.R',.003),
        box('crossguard',(-.564,-.027,1.03),(.22,.032,.035),'iron','hand.R',.006),
        mesh('blade',[(-.598,-.027,1.05),(-.53,-.027,1.05),(-.564,-.045,1.05),(-.564,-.009,1.05),(-.564,-.027,1.68)],
             [(0,2,4),(2,1,4),(1,3,4),(3,0,4),(0,3,1,2)],'iron_edge',[{'hand.R':1}]*5)])
    part('weapon','hammer',[
        box('handle',(-.564,-.027,1.06),(.036,.036,.42),'wood','hand.R',.005),
        box('hammerhead',(-.564,-.027,1.285),(.22,.10,.11),'iron','hand.R',.014)])

    def hanging_arm(side,sgn,swing=0,bend=.16):
        bpy.context.view_layer.update()
        names=[f'upper_arm.{side}',f'forearm.{side}',f'hand.{side}']
        shoulder=rig.pose.bones[names[0]].head.copy()
        direction=rig.pose.bones['root'].matrix.to_quaternion()
        length=[rig.data.bones[n].length for n in names]
        elbow=shoulder+direction@Vector((sgn*.065,-math.sin(swing)*.28,-math.cos(swing)*.28)).normalized()*length[0]
        wrist=elbow+direction@Vector((sgn*.04,-math.sin(swing+bend)*.245,-math.cos(swing+bend)*.245)).normalized()*length[1]
        fingertip=wrist+direction@Vector((sgn*.018,-math.sin(swing+bend)*.11,-math.cos(swing+bend)*.11)).normalized()*length[2]
        aim_bone(rig,names[0],shoulder,elbow)
        aim_bone(rig,names[1],elbow,wrist)
        aim_bone(rig,names[2],wrist,fingertip)

    def pose(t, kind):
        p=rig.pose.bones
        if kind=='idle':
            p['chest'].rotation_euler.x=.012*math.sin(math.tau*t)
            p['head'].rotation_euler.y=.018*math.sin(math.tau*t)
            for side,sign in [('L',1),('R',-1)]:
                hanging_arm(side,sign)
            return
        if kind in ['walk','run']:
            run=kind=='run'
            phase=math.tau*t
            bob=(-.105 if run else -.06)+(.025 if run else .012)*(1-math.cos(2*phase))
            p['pelvis'].location.z=bob # pelvis local Y is Z; use world rest conversion below
            p['pelvis'].location=(0,bob,0)
            p['chest'].rotation_euler.x=.10 if run else .035
            p['pelvis'].rotation_euler.y=.045*math.sin(phase)
            bpy.context.view_layer.update()
            for side,sgn,offset in [('L',1,0),('R',-1,.5)]:
                cyc=(t+offset)%1
                stride=.72 if run else .45
                # stance travels front to back, swing returns with clearance.
                stance=.40 if run else .62
                if cyc<stance:
                    y=-stride/2+stride*cyc/stance
                    z=.115
                else:
                    u=(cyc-stance)/(1-stance)
                    y=stride/2-stride*u
                    z=.115+(.18 if run else .085)*math.sin(math.pi*u)
                h=Vector((sgn*.112,0,.96+bob))
                foot=Vector((sgn*.112,y,z))
                l1=(rig.data.bones[f'thigh.{side}'].tail_local-rig.data.bones[f'thigh.{side}'].head_local).length
                l2=(rig.data.bones[f'shin.{side}'].tail_local-rig.data.bones[f'shin.{side}'].head_local).length
                knee=solve_knee(h,foot,l1,l2,(0,-1,0))
                aim_bone(rig,f'thigh.{side}',h,knee)
                aim_bone(rig,f'shin.{side}',knee,foot)
                aim_bone(rig,f'foot.{side}',foot,foot+Vector((0,-.16,-.06)))
                hanging_arm(side,sgn,(.65 if run else .33)*math.sin(phase+offset*math.tau),.90 if run else .16)
        elif kind.startswith('turn'):
            sign=1 if kind.endswith('left') else -1
            ease=t*t*(3-2*t)
            p['root'].rotation_euler.z=sign*math.pi/2*ease
            p['chest'].rotation_euler.y=sign*.12*math.sin(math.pi*t)
            p['head'].rotation_euler.y=sign*.24*math.sin(math.pi*t)
            for side,sgn in [('L',1),('R',-1)]:
                hanging_arm(side,sgn)
                p[f'thigh.{side}'].rotation_euler.x=sgn*.14*math.sin(math.tau*t)
                p[f'shin.{side}'].rotation_euler.x=.17*max(0,math.sin(math.tau*t+sgn*.7))
        elif kind=='attack':
            wind=math.sin(math.pi*min(t/.38,1)) if t<.38 else 0
            strike=math.sin(math.pi*(t-.38)/.62) if t>=.38 else 0
            p['chest'].rotation_euler.y=-.38*wind+.38*strike
            p['pelvis'].rotation_euler.x=.08*strike
            hanging_arm('L',1)
            hanging_arm('R',-1)
            shoulder=p['upper_arm.R'].head.copy()
            upper=Vector((-.065,-.015,-.28))*(1-max(wind,strike))+Vector((-.12,.12,.24))*wind+Vector((-.08,-.26,-.10))*strike
            lower=Vector((-.04,-.04,-.245))*(1-max(wind,strike))+Vector((-.04,-.1,.24))*wind+Vector((-.02,-.22,-.12))*strike
            hand=lower.copy()
            elbow=shoulder+upper.normalized()*rig.data.bones['upper_arm.R'].length
            wrist=elbow+lower.normalized()*rig.data.bones['forearm.R'].length
            aim_bone(rig,'upper_arm.R',shoulder,elbow)
            aim_bone(rig,'forearm.R',elbow,wrist)
            aim_bone(rig,'hand.R',wrist,wrist+hand.normalized()*rig.data.bones['hand.R'].length)
        elif kind=='jump':
            # Crouch, impulse, airtime, landing compression, recover.
            lift=0
            crouch=0
            if t<.2:
                crouch=.42*math.sin(math.pi*t/.4)
            elif t<.78:
                u=(t-.2)/.58
                lift=.48*4*u*(1-u)
                crouch=.32*math.sin(math.pi*u)
            else:
                crouch=.34*math.sin(math.pi*(t-.78)/.22)
            p['root'].location.z=lift-.065*crouch/.42
            for side,sgn in [('L',1),('R',-1)]:
                p[f'thigh.{side}'].rotation_euler.x=-crouch
                p[f'shin.{side}'].rotation_euler.x=crouch*1.85
                p[f'foot.{side}'].rotation_euler.x=-crouch*.85
                hanging_arm(side,sgn,.45*math.sin(math.pi*t),.20)
            p['chest'].rotation_euler.x=.15*crouch

    action(rig,'HUM_Reference_A',2,lambda t,f:None)
    action(rig,'HUM_Idle',61,lambda t,f:pose(t,'idle'),True)
    action(rig,'HUM_Walk',31,lambda t,f:pose(t,'walk'),True)
    action(rig,'HUM_Run',21,lambda t,f:pose(t,'run'),True)
    action(rig,'HUM_Turn_L90',25,lambda t,f:pose(t,'turn_left'))
    action(rig,'HUM_Turn_R90',25,lambda t,f:pose(t,'turn_right'))
    action(rig,'HUM_Turn_Base',2,lambda t,f:pose(0,'idle'))
    action(rig,'HUM_Attack',31,lambda t,f:pose(t,'attack'))
    action(rig,'HUM_Jump',43,lambda t,f:pose(t,'jump'))

    presets={
        'protagonist':['head_skin_pale','neck_skin_pale','hands_skin_pale','face_weathered_skin_pale','hair_swept','torso_traveler','legs_wool','boots_travel','belt_pouch'],
        'villager':['head_skin','neck_skin','hands_skin','face_soft','hair_cropped','torso_villager','legs_wool','boots_wrapped','belt_pouch'],
        'guard':['head_skin_dark','neck_skin_dark','hands_skin_dark','face_stern_skin_dark','hair_cropped','torso_guard','legs_leather','boots_guard','outer_guard_armor','belt_pouch','weapon_sword'],
        'blacksmith':['head_skin','neck_skin','hands_skin','face_stern','hair_grey','torso_smith','legs_leather','boots_travel','outer_smith_apron','belt_pouch','weapon_hammer'],
    }
    for name,o in parts.items():
        export(f'characters/parts/{name}.glb',[rig,o])
        register(name,'human_part',[o],f'characters/parts/{name}.glb')
    for name,ids in presets.items():
        selected=[parts['HUM_'+i] for i in ids]
        export(f'characters/presets/{name}.glb',[rig]+selected)
        register(name,'human_preset',selected,f'characters/presets/{name}.glb')
    visible=set('HUM_'+i for i in presets['protagonist'])
    for name,o in parts.items():
        o.hide_set(name not in visible)
        o.hide_render=name not in visible
    bpy.ops.object.select_all(action='DESELECT')
    rig.select_set(True)
    bpy.context.view_layer.objects.active=rig
    rig.animation_data.action=bpy.data.actions['HUM_Idle']
    rig.animation_data.action_slot=rig.animation_data.action.slots[0]
    bpy.context.scene.frame_set(1)
    save('characters/human_modular.blend')
    json_out('metadata/human_parts.json',{'rig':'ARC_Human_v1','height_m':1.8,'head_units':7,
             'presets':presets,'slots':{k:{'slot':o['slot'],'file':f'characters/parts/{k}.glb'} for k,o in parts.items()},
             'compatibility':'All v1 human parts share identical rest matrices, bind space and seam positions. Select one per slot. Outer/weapon optional.',
             'skin_contract':'Match head, neck, hands and face skin suffix; face without suffix uses skin medium.',
             'overlap_rules':['Tunic covers upper trouser seam','Apron and armor use outer slot; do not enable together','Hood replaces hair','Face is decorative rigid head geometry; no facial animation']})
    # A four-character lineup without changing the modular source file.
    rig.animation_data.action=None
    reset_pose(rig)
    for o in parts.values():
        o.hide_render=True
    lineup=[]
    for idx,(name,ids) in enumerate(presets.items()):
        for ident in ids:
            src=parts['HUM_'+ident]
            o=src.copy()
            o.data=src.data.copy()
            bpy.context.collection.objects.link(o)
            o.parent=None
            o.modifiers.clear()
            o.hide_render=False
            o.hide_set(False)
            o.location.x=(idx-1.5)*.95
            lineup.append(o)
    stage(lineup,'previews/human_lineup.png',target=(0,0,.9),distance=4.9,resolution=(1600,1000))
    json_out('metadata/human_inventory.json',ASSETS)


if __name__=='__main__':
    build()
