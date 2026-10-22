import sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parent))
from common import *

SPECS={
    'deer':dict(h=1.25,body=(.25,.70,.32),neck=.48,head=(.135,.27,.15),coat='deer',
                temper='gentle',diet='herbivore',gait='four-beat walk / gathered gallop',speed=(1.35,5.8), hoof=True),
    'wolf':dict(h=.82,body=(.24,.65,.27),neck=.18,head=(.15,.25,.16),coat='wolf',
                temper='dangerous',diet='carnivore',gait='four-beat walk / diagonal trot',speed=(1.4,4.8), hoof=False),
    'boar':dict(h=.75,body=(.34,.62,.34),neck=.06,head=(.22,.32,.23),coat='boar',
                temper='dangerous_when_threatened',diet='omnivore',gait='four-beat walk / diagonal trot',speed=(.95,3.6), hoof=True),
    'goat':dict(h=.88,body=(.235,.52,.26),neck=.23,head=(.13,.20,.15),coat='goat',
                temper='neutral',diet='herbivore',gait='four-beat walk / paired bounding gallop',speed=(1.05,3.5), hoof=True),
}


def build_species(species):
    reset()
    s=SPECS[species]
    h=s['h']; bx,by,bz=s['body']; hx,hy,hz=s['head']
    fy=-by*.66; ry=by*.65
    neckbase=(0,-by*.72,h+.09)
    neckend=(0,-by-.13,h+.10+s['neck'])
    hc=(0,-by-.13-hy*.50,h+.13+s['neck'])
    bones=[('root',(0,0,0),(0,.18,0),None),
           ('pelvis',(0,ry,h),(0,ry-.25,h+.015),'root'),
           ('spine',(0,ry-.25,h+.015),(0,fy+.1,h+.035),'pelvis'),
           ('chest',(0,fy+.1,h+.035),neckbase,'spine'),
           ('neck',neckbase,neckend,'chest'),
           ('head',neckend,(0,hc[1]-hy*.8,hc[2]-.035),'neck'),
           ('jaw',(0,hc[1]-.01,hc[2]-.08),(0,hc[1]-hy,hc[2]-.10),'head')]
    limb={}
    for side,sign in [('L',1),('R',-1)]:
        x=sign*bx*.72
        shoulder=(x,fy,h)
        elbow=(x,fy+.065,h*.55)
        wrist=(x,fy-.025,.13)
        toe=(x,fy-.12,.055)
        hip=(x,ry,h)
        knee=(x,ry-.22,h*.61)
        hock=(x,ry+.09,h*.27)
        ankle=(x,ry-.035,.105)
        hindtoe=(x,ry-.14,.055)
        for prefix,pts,parent in [('front',[shoulder,elbow,wrist,toe],'chest'),('hind',[hip,knee,hock,ankle,hindtoe],'pelvis')]:
            names=[f'{prefix}_{k}.{side}' for k in (['upper','lower','paw'] if prefix=='front' else ['thigh','shin','pastern','paw'])]
            for i,name in enumerate(names):
                bones.append((name,pts[i],pts[i+1],parent if i==0 else names[i-1]))
            limb[(prefix,side)]=(pts,names)
    tailpts=[(0,by*.82,h+.04),(0,by+(.2 if species=='wolf' else .08),h-.10),
             (0,by+(.42 if species=='wolf' else .14),h-(.32 if species=='wolf' else .18))]
    bones += [('tail.01',tailpts[0],tailpts[1],'pelvis'),('tail.02',tailpts[1],tailpts[2],'tail.01')]
    rig=armature(f'ARC_{species.title()}_v1',bones)
    rig['rig_family']=f'ARC_{species.title()}_v1'
    rig['anatomy']='Hind thigh -> forward stifle -> rearward raised hock -> pastern -> split hoof/paw. Front elbow and carpus distinct.'
    parts={}

    def part(slot,pieces,variant='base'):
        o=join(f'{species}_{slot}_{variant}',pieces)
        bind(o,rig,slot)
        parts[o.name]=o
        return o

    # Torso rings follow the length of the spine, not a scaled human body.
    points=[(0,by*.85,h),(0,by*.52,h),(0,0,h),(0,-by*.56,h+.025),(0,-by*.83,h+.045)]
    torso=tube('ribcage',points,[(bx*.60,bz*.70),(bx*.95,bz*.92),(bx,bz),(bx*.92,bz*1.04),(bx*.52,bz*.67)],s['coat'],n=12,
               ring_weights=[{'pelvis':1},{'pelvis':.85,'spine':.15},{'spine':1},{'spine':.25,'chest':.75},{'chest':1}])
    pieces=[torso,ellipsoid('brisket',(0,fy-.005,h-.10),(bx*.8,bx*.85,bz*.85),s['coat'],'chest',10,6)]
    if species!='boar':
        pieces += [ellipsoid('belly patch',(0,-.03,h-bz*.74),(bx*.70,by*.66,bz*.28),'belly','spine',10,4)]
    body=part('body',pieces)
    legpieces=[]
    for (prefix,side),(pts,names) in limb.items():
        if prefix=='hind':
            r=[bx*.52,bx*.34,bx*.17,bx*.13]
            w=[{names[0]:.7,'pelvis':.3},{names[0]:.5,names[1]:.5},{names[1]:.5,names[2]:.5},{names[2]:1}]
            legpieces += [ellipsoid('haunch',pts[0],(bx*.57,by*.27,bz*.80),s['coat'],names[0],10,6)]
        else:
            r=[bx*.34,bx*.23,bx*.14]
            w=[{names[0]:.75,'chest':.25},{names[0]:.5,names[1]:.5},{names[1]:1}]
        if species=='boar':
            r=[v*1.25 for v in r]
        path=pts[:-1]+[(pts[-2][0],pts[-2][1],.07)]
        r=r+[r[-1]*.9]
        w=w+[{names[-2]:.3,names[-1]:.7}]
        legpieces += [tube('leg',path,r,s['coat'],n=8,ring_weights=w)]
        foot=pts[-2]
        if s['hoof']:
            # Separate cloven halves, visible gap, long rear pastern.
            for split in [-1,1]:
                legpieces += [box('cloven hoof',(foot[0]+split*.022,foot[1]-.022,.055),(.036,.115,.10),'hoof',names[-1],.009)]
        else:
            legpieces += [ellipsoid('paw',(foot[0],foot[1]-.028,.062),(.076,.12,.06),'wolf_dark',names[-1],8,4)]
            for dx in [-.04,0,.04]:
                legpieces += [ellipsoid('toe',(foot[0]+dx,foot[1]-.105,.047),(.024,.040,.032),'hoof',names[-1],6,4)]
    part('legs',legpieces)
    neck=tube('neck', [neckbase,tuple((Vector(neckbase)+Vector(neckend))/2),neckend],
              [bx*.62,bx*.49,hx*.82],s['coat'],n=10,
              ring_weights=[{'chest':.4,'neck':.6},{'neck':1},{'neck':.7,'head':.3}])
    headpieces=[neck,ellipsoid('cranium',hc,(hx,hy*.68,hz),s['coat'],'head',10,6)]
    muzzle=(0,hc[1]-hy*.75,hc[2]-.055)
    headpieces += [ellipsoid('muzzle',muzzle,(hx*.72,hy*.66,hz*.55),s['coat'],'head',8,5),
                   ellipsoid('lower jaw',(0,muzzle[1]+.025,muzzle[2]-.045),(hx*.63,hy*.59,hz*.28),s['coat'],'jaw',8,4)]
    nose=(0,hc[1]-hy*1.24,hc[2]-.04)
    headpieces += [ellipsoid('nose',nose,(hx*.6,.045,hz*.27),'hoof','head',8,4)]
    for sign in [-1,1]:
        eye=(sign*hx*.91,hc[1]-hy*.25,hc[2]+hz*.23)
        headpieces += [ellipsoid('eye',eye,(.018,.025,.019),'eye','head',8,4),
                       ellipsoid('eye glint',(eye[0]+sign*.006,eye[1]-.009,eye[2]+.006),(.005,.008,.005),'eye_white','head',6,4)]
        base=Vector((sign*hx*.78,hc[1]+.03,hc[2]+hz*.64))
        earlen=.22 if species=='deer' else .14
        tip=base+Vector((sign*earlen*.7,.035,earlen))
        verts=[tuple(base+Vector((-.055,0,0))),tuple(base+Vector((.055,0,0))),tuple(tip),tuple(base+Vector((0,.048,.01)))]
        headpieces += [mesh('ear',verts,[(0,1,2),(1,3,2),(3,0,2),(0,3,1)],s['coat'],[{'head':1}]*4),
                       mesh('inner ear',[tuple(base+Vector((-.035,-.005,.015))),tuple(base+Vector((.035,-.005,.015))),tuple(tip+Vector((0,-.003,-.03)))],[(0,1,2)],'belly',[{'head':1}]*3)]
    part('head',headpieces)
    if species=='wolf':
        tailrad=[.10,.105,.055]
    else:
        tailrad=[.046,.044,.014]
    part('tail',[tube('tail',tailpts,tailrad,s['coat'],n=8,
                      ring_weights=[{'tail.01':1},{'tail.01':.4,'tail.02':.6},{'tail.02':1}])])
    ornaments=[]
    if species=='deer':
        for sign in [-1,1]:
            start=Vector((sign*.078,hc[1]+.01,hc[2]+.13))
            main=[start,start+Vector((sign*.08,.02,.18)),start+Vector((sign*.17,.10,.38)),start+Vector((sign*.19,.23,.52))]
            ornaments += [tube('antler beam',main,[.031,.026,.019,.004],'horn','head',6)]
            for k in [1,2]:
                p=main[k]
                ornaments += [tube('antler tine',[p,p+Vector((sign*.07,-.16,.15)),p+Vector((sign*.09,-.21,.24))],[.02,.012,.002],'horn','head',5)]
        part('antlers',ornaments,'stag')
    elif species=='goat':
        for sign in [-1,1]:
            start=Vector((sign*.065,hc[1]+.025,hc[2]+.12))
            ornaments += [tube('horn',[start,start+Vector((sign*.018,.055,.15)),start+Vector((sign*.025,.16,.23)),start+Vector((sign*.03,.24,.20))],[.035,.026,.017,.003],'horn','head',7)]
        ornaments += [tube('beard',[(0,hc[1]-.10,hc[2]-.12),(0,hc[1]-.09,hc[2]-.28)],[.055,.005],'goat','jaw',6)]
        part('horns',ornaments)
    elif species=='boar':
        for sign in [-1,1]:
            start=Vector((sign*.15,muzzle[1]-.10,muzzle[2]-.06))
            ornaments += [tube('tusk',[start,start+Vector((sign*.04,-.04,.06)),start+Vector((sign*.025,-.035,.15))],[.029,.018,.002],'horn','jaw',6)]
        for i in range(9):
            y=-.35+i*.09
            z=h+bz*.91
            ornaments += [mesh('bristles',[(-.027,y-.04,z),(.027,y-.04,z),(0,y+.02,z+.14)],[(0,1,2)],'hair',[{'spine':1}]*3)]
        part('tusks_mane',ornaments)
    else:
        for sign in [-1,1]:
            ornaments += [mesh('ruff',[(sign*.13,-.38,h+.17),(sign*.27,-.39,h+.03),(sign*.36,-.20,h+.11),(sign*.18,-.15,h+.25)],
                                [(0,1,2),(0,2,3)],'wolf_dark',[{'chest':1}]*4)]
        part('ruff',ornaments)

    def pose(t,kind):
        p=rig.pose.bones
        if kind=='idle':
            p['spine'].rotation_euler.x=.008*math.sin(math.tau*t)
            p['tail.01'].rotation_euler.x=.08*math.sin(math.tau*t)
            p['head'].rotation_euler.y=.04*math.sin(math.tau*t)
            return
        if kind in ['walk','run']:
            run=kind=='run'
            wave=math.tau*t
            bob=(-h*.18 if run else -h*.06)+(.045 if run else .014)*(1-math.cos(wave*2))*.5
            p['root'].location.z=bob
            p['spine'].rotation_euler.x=(.03 if run else .012)*math.sin(wave)
            p['tail.01'].rotation_euler.x=.10*math.sin(wave)
            p['neck'].rotation_euler.x=-.03*math.sin(wave)
            bpy.context.view_layer.update()
            if run and species in ['deer','goat']:
                offsets={('front','L'):0,('front','R'):.13,('hind','L'):.52,('hind','R'):.65}
            elif run:
                offsets={('front','L'):0,('front','R'):.5,('hind','L'):.5,('hind','R'):0}
            else:
                offsets={('front','L'):0,('front','R'):.5,('hind','L'):.75,('hind','R'):.25}
            for key,(pts,names) in limb.items():
                cyc=(t+offsets[key])%1
                stance=.43 if run else .67
                stride=(h*.85 if run else h*.43)
                if cyc<stance:
                    yy=-stride/2+stride*cyc/stance
                    zz=0
                else:
                    u=(cyc-stance)/(1-stance)
                    yy=stride/2-stride*u
                    zz=(.18 if run else .075)*math.sin(math.pi*u)
                prefix,side=key
                start=Vector(pts[0])+Vector((0,0,bob))
                foot=Vector(pts[-2])+Vector((0,yy,zz))
                if prefix=='front':
                    l1=(Vector(pts[1])-Vector(pts[0])).length
                    l2=(Vector(pts[2])-Vector(pts[1])).length
                    joint=solve_knee(start,foot,l1,l2,(0,1,0))
                    aim_bone(rig,names[0],start,joint)
                    aim_bone(rig,names[1],joint,foot)
                else:
                    # Hock remains raised and behind the pastern through the stride.
                    raised=foot+(Vector(pts[2])-Vector(pts[3]))
                    l1=(Vector(pts[1])-Vector(pts[0])).length
                    l2=(Vector(pts[2])-Vector(pts[1])).length
                    stifle=solve_knee(start,raised,l1,l2,(0,-1,0))
                    aim_bone(rig,names[0],start,stifle)
                    aim_bone(rig,names[1],stifle,raised)
                    aim_bone(rig,names[2],raised,foot)
                aim_bone(rig,names[-1],foot,foot+(Vector(pts[-1])-Vector(pts[-2])))
        elif kind.startswith('turn'):
            sign=1 if kind.endswith('left') else -1
            p['root'].rotation_euler.z=sign*math.pi/2*(t*t*(3-2*t))
            p['neck'].rotation_euler.y=sign*.20*math.sin(math.pi*t)
            for key,(pts,names) in limb.items():
                side=1 if key[1]=='L' else -1
                p[names[0]].rotation_euler.x=side*.13*math.sin(math.tau*t)
                p[names[1]].rotation_euler.x=.14*max(0,math.sin(math.tau*t+side))
        elif kind=='attack':
            anticip=max(0,math.sin(math.pi*min(t/.4,1))) if t<.4 else 0
            strike=max(0,math.sin(math.pi*(t-.4)/.6)) if t>=.4 else 0
            p['neck'].rotation_euler.x=-.25*anticip+.45*strike
            p['head'].rotation_euler.x=-.12*anticip+.20*strike
            p['root'].location.y=-.18*strike
            p['jaw'].rotation_euler.x=.5*strike if species=='wolf' else .05*strike
            p['spine'].rotation_euler.x=-.08*strike
        elif kind=='jump':
            if t<.2:
                crouch=math.sin(math.pi*t/.4)
                lift=-.08*crouch
            elif t<.8:
                u=(t-.2)/.6
                crouch=.45*math.sin(math.pi*u)
                lift=(.6 if species in ['goat','deer'] else .35)*4*u*(1-u)
            else:
                crouch=.65*math.sin(math.pi*(t-.8)/.2)
                lift=-.08*crouch
            p['root'].location.z=lift
            for (prefix,side),(pts,names) in limb.items():
                p[names[0]].rotation_euler.x=(-.45 if prefix=='front' else .35)*crouch
                p[names[1]].rotation_euler.x=(.72 if prefix=='front' else -.55)*crouch
                if prefix=='hind':
                    p[names[2]].rotation_euler.x=.35*crouch

    prefix=species.upper()
    clips=[('Reference',2,'reference',False),('Idle',61,'idle',True),('Walk',37,'walk',True),
           ('Run',23 if species in ['deer','goat'] else 19,'run',True),
           ('Turn_L90',31,'turn_left',False),('Turn_R90',31,'turn_right',False),
           ('Turn_Base',2,'idle',False),('Attack',31,'attack',False),('Jump',43,'jump',False)]
    for title,frames,kind,loop in clips:
        action(rig,f'{prefix}_{title}',frames,lambda t,f,k=kind:pose(t,k),loop)
    main=[rig]+list(parts.values())
    export(f'animals/{species}.glb',main)
    entry=register(species,'animal',main,f'animals/{species}.glb')
    run_duration=(22 if species in ['deer','goat'] else 18)/30
    entry.update({'temperament':s['temper'],'diet':s['diet'],'gait':s['gait'],
                  'walk_speed_mps':round(h*.43/(.67*1.2),4),'run_speed_mps':round(h*.85/(.43*run_duration),4)})
    folder=ROOT/f'animals/{species}_parts'
    folder.mkdir(exist_ok=True)
    for name,o in parts.items():
        export(f'animals/{species}_parts/{name}.glb',[rig,o])
    # A second body color can be swapped within this species only.
    alt=body.copy(); alt.data=body.data.copy()
    bpy.context.collection.objects.link(alt)
    alt.name=f'{species}_body_dark'
    for i,m in enumerate(alt.data.materials):
        if m==mat(s['coat']):
            alt.data.materials[i]=mat('wolf_dark' if species=='wolf' else 'leather_dark')
    export(f'animals/{species}_parts/{alt.name}.glb',[rig,alt])
    alt.hide_set(True); alt.hide_render=True
    for o in parts.values():
        o.hide_set(False); o.hide_render=False
    rig.animation_data.action=bpy.data.actions[f'{prefix}_Idle']
    rig.animation_data.action_slot=rig.animation_data.action.slots[0]
    save(f'animals/{species}.blend')
    stage(main,f'previews/{species}.png',target=(0,-.1,h*.68),distance=3.7 if species=='deer' else 3.0)
    json_out(f'metadata/{species}_inventory.json',{'asset':entry,'parts':list(parts.keys()),'clips':[x[0] for x in clips],
             'compatibility':'Swap only within species and rig version. Antlers/horns/tusks/ruff optional; no cross-species leg swapping.',
             'temperament_note':'Visual design classification; game AI and combat behavior are not implemented by these assets.'})


if __name__=='__main__':
    for species in SPECS:
        build_species(species)
