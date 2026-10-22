import sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parent))
from common import *
from mathutils import Euler

MODULES={}
SOCKETS={}


def asset(name,category,pieces,snap=.25,sockets=None):
    o=join(name,pieces)
    o['category']=category
    o['snap_m']=snap
    o['pivot']='Ground origin; architecture lower-left module corner'
    if sockets:
        o['sockets_json']=json.dumps(sockets)
        SOCKETS[name]=sockets
    o.asset_mark()
    export(f'environment/{name}.glb',[o],False)
    e=register(name,category,[o],f'environment/{name}.glb')
    e['snap_m']=snap
    e['dimensions_m']=list(o.dimensions)
    MODULES[name]=o
    return o


def beam(name,a,b,width=.12,depth=.12,material='wood'):
    a,b=Vector(a),Vector(b)
    o=box(name,(a+b)/2,(width,depth,(b-a).length),material)
    # Geometry is baked at origin: rotate the vertices around beam center.
    q=Vector((0,0,1)).rotation_difference(b-a)
    center=(a+b)/2
    for v in o.data.vertices:
        v.co=center+q@(v.co-center)
    return o


def wall(name,opening=None):
    pieces=[]
    if opening=='door':
        for x in [.25,1.75]:
            pieces += [box('infill',(x,0,1.25),(.5,.17,2.5),'plaster')]
        pieces += [box('lintel infill',(1,0,2.32),(1,.17,.36),'plaster')]
    elif opening=='window':
        pieces += [box('under window',(1,0,.55),(2,.17,1.1),'plaster'),
                   box('over window',(1,0,2.16),(2,.17,.68),'plaster')]
        for x in [.25,1.75]:
            pieces += [box('window side',(x,0,1.46),(.5,.17,.72),'plaster')]
        for x in [.5,1.5]:
            pieces += [box('window jamb',(x,-.015,1.46),(.09,.25,.83),'wood')]
        for z in [1.10,1.82]:
            pieces += [box('sill',(1,-.015,z),(1.17,.28,.10),'wood')]
        pieces += [box('mullion',(1,0,1.46),(.065,.1,.70),'wood')]
    else:
        pieces += [box('infill',(1,0,1.25),(2,.17,2.5),'plaster')]
        pieces += [beam('diagonal brace',(.12,-.12,.2),(1.88,-.12,2.34),.105,.10)]
    for x in [.06,1.94]:
        pieces += [box('post',(x,0,1.25),(.12,.25,2.5),'wood')]
    for z in [.07,2.43]:
        pieces += [box('rail',(1,0,z),(2,.25,.14),'wood')]
    if opening=='door':
        for x in [.50,1.50]:
            pieces += [box('door jamb',(x,0,1.06),(.12,.28,2.12),'wood')]
        pieces += [box('door lintel',(1,0,2.08),(1.12,.28,.14),'wood')]
    return asset(name,'architecture',pieces,.25,{'left':[0,0,0],'right':[2,0,0],'top':[0,0,2.5]})


def architecture():
    asset('foundation_2x2','architecture',[box('foundation',(1,1,.09),(2,2,.18),'stone',bevel=.035)],.25)
    pieces=[]
    for i in range(8):
        pieces += [box('floor plank',(.125+i*.25,1,.045),(.242,2,.09),'wood_light' if i%3 else 'wood',bevel=.004)]
    asset('floor_wood_2x2','architecture',pieces,.25)
    wall('wall_solid_2m')
    wall('wall_door_2m','door')
    wall('wall_window_2m','window')
    asset('post_2p5m','architecture',[box('post',(0,0,1.25),(.19,.19,2.5),'wood',bevel=.012)],.25)
    asset('beam_2m','architecture',[box('beam',(1,0,0),(2,.18,.18),'wood',bevel=.01)],.25)
    for kind in ['thatch','shingle']:
        pieces=[mesh('roof substrate',[(-.08,-.12,0),(2.08,-.12,0),(2.08,2,1.25),(-.08,2,1.25),
                                      (-.08,-.12,-.10),(2.08,-.12,-.10),(2.08,2,1.15),(-.08,2,1.15)],
                     [(0,1,2,3),(7,6,5,4),(4,5,1,0),(1,5,6,2),(2,6,7,3),(3,7,4,0)],kind)]
        # Roof is built in short overlapping courses, with visible uneven eaves.
        for row in range(7):
            y=-.13+row*.31
            for col in range(8):
                x=col*.27-.03+(row%2)*.055
                z=(y+.17)*1.25/2+.035
                pieces += [box('roof course',(x,y+.16,z),(.275,.39,.075),kind,rot=(math.atan(1.25/2),0,0))]
        asset(f'roof_{kind}_2m','architecture',pieces,.25,{'eave':[0,0,0],'ridge':[0,2,1.25],'next':[2,0,0]})
    asset('gable_4m','architecture',[
        mesh('gable infill',[(-2,0,0),(2,0,0),(0,0,1.25),(-2,.16,0),(2,.16,0),(0,.16,1.25)],
             [(0,1,2),(5,4,3),(0,3,4,1),(1,4,5,2),(2,5,3,0)],'plaster'),
        beam('gable left',(-2,-.04,0),(0,-.04,1.25)),beam('gable right',(0,-.04,1.25),(2,-.04,0)),
        beam('king post',(0,-.04,0),(0,-.04,1.25))],.25)
    asset('ridge_2m','architecture',[tube('ridge',[(0,0,0),(2,0,0)],[.12,.12],'thatch',n=6)],.25)
    stones=[]
    for row in range(7):
        for col in range(2):
            stones += [box('chimney stone',((col-.5)*.28+(row%2)*.035,0,.14+row*.23),(.29,.52,.225),'stone_light' if (row+col)%3==0 else 'stone',bevel=.015)]
    asset('chimney_1p6m','architecture',stones,.25)
    pieces=[]
    for x in [0,2]:
        pieces += [box('fence post',(x,0,.58),(.14,.14,1.16),'wood',bevel=.01)]
    for z in [.39,.84]:
        pieces += [box('rail',(1,0,z),(2,.09,.12),'wood')]
    pieces += [beam('brace',(.08,.01,.31),(1.92,.01,.93),.075,.07)]
    asset('fence_2m','architecture',pieces,.25)
    asset('stairs_1m','architecture',[box('step',(0,-.28,.08),(1,.62,.16),'stone',bevel=.015),
                                         box('step',(0,.01,.19),(1,.34,.22),'stone',bevel=.015)],.25)
    asset('shutter_1m','architecture',[box('shutter',(.25,0,.36),(.48,.055,.70),'wood'),
                                           box('shutter',(.75,0,.36),(.48,.055,.70),'wood'),
                                           box('batten',(.5,-.04,.17),(.95,.04,.06),'iron'),
                                           box('batten',(.5,-.04,.55),(.95,.04,.06),'iron')],.25)


def props():
    pieces=[]
    for i in range(5):
        pieces += [box('top board',((i-2)*.23,0,.77),(.222,.82,.08),'wood_light',bevel=.008)]
    for x in [-.43,.43]:
        for y in [-.29,.29]:
            pieces += [box('leg',(x,y,.36),(.10,.10,.72),'wood',bevel=.009)]
    pieces += [box('stretcher',(0,0,.24),(.93,.08,.09),'wood')]
    asset('table','household',pieces)
    pieces=[box('seat',(0,0,.45),(.44,.43,.07),'wood_light',bevel=.008)]
    for x in [-.16,.16]:
        for y in [-.15,.15]:
            pieces += [box('leg',(x,y,.23),(.065,.065,.46),'wood')]
        pieces += [box('back upright',(x,.16,.72),(.065,.07,.58),'wood')]
    pieces += [box('backrest',(0,.16,.88),(.40,.05,.13),'wood_light')]
    asset('chair','household',pieces)
    asset('stool','household',[box('seat',(0,0,.44),(.39,.36,.07),'wood_light',bevel=.012)]+
          [box('leg',(x,y,.22),(.055,.055,.44),'wood') for x,y in [(-.13,-.12),(.13,-.12),(0,.12)]])
    asset('bench','household',[box('seat',(0,0,.45),(1.65,.37,.08),'wood_light',bevel=.008)]+
          [box('support',(x,0,.22),(.12,.31,.44),'wood') for x in [-.65,.65]])
    bed=[box('bed frame',(0,0,.34),(.95,1.94,.18),'wood',bevel=.015),
         box('straw mattress',(0,0,.49),(.88,1.83,.17),'linen',bevel=.05),
         box('blanket',(0,-.18,.59),(.90,1.44,.06),'wool_moss',bevel=.02),
         ellipsoid('pillow',(0,.65,.62),(.33,.20,.085),'linen',segments=10,rings=4)]
    for x in [-.40,.40]:
        for y in [-.83,.83]:
            bed += [box('bed post',(x,y,.31),(.10,.10,.62),'wood')]
    asset('bed','household',bed)
    shelf=[box('upright',(x,0,.9),(.09,.37,1.8),'wood') for x in [-.55,.55]]
    for z in [.12,.66,1.20,1.75]:
        shelf += [box('shelf',(0,0,z),(1.12,.39,.065),'wood_light')]
    asset('shelf','household',shelf)
    for kind in ['crate','crate_open']:
        pieces=[box('base',(0,0,.04),(.72,.62,.08),'wood')]
        for z in [.15,.31,.47]:
            for y in [-.29,.29]:
                pieces += [box('slat',(0,y,z),(.72,.055,.145),'wood_light')]
            for x in [-.33,.33]:
                pieces += [box('slat',(x,0,z),(.055,.62,.145),'wood')]
        if kind=='crate':
            pieces += [box('lid',(0,0,.57),(.72,.62,.07),'wood_light'),beam('brace',(-.29,-.325,.08),(.29,-.325,.53),.06,.03)]
        asset(kind,'household',pieces)
    barrel=[tube('staves',[(0,0,.04),(0,0,.20),(0,0,.48),(0,0,.77),(0,0,.89)],
                  [.28,.32,.34,.32,.28],'wood',n=12)]
    for z,r in [(.15,.325),(.44,.348),(.77,.327)]:
        barrel += [tube('iron hoop',[(0,0,z-.024),(0,0,z+.024)],[r,r],'iron',n=12)]
    barrel += [tube('lid',[(0,0,.89),(0,0,.90)],[.276,.276],'wood_light',n=12)]
    asset('barrel','household',barrel)
    # Open vessels: hollow interior, no solid cap over the opening.
    for name,r,height,material in [('bucket',.19,.34,'wood'),('clay_pot',.20,.40,'clay'),('bowl',.16,.09,'clay')]:
        if name=='clay_pot':
            profile=[(.0,r*.55),(.08,r),(.25,r*1.15),(.35,r*.68),(.40,r*.63),(.40,r*.49),(.35,r*.52),(.24,r*.99),(.06,r*.80)]
        else:
            profile=[(.015,r*.70),(height,r),(height,r*.89),(.04,r*.61)]
        vs,fs=[],[];n=12
        for z,rad in profile:
            for j in range(n):
                a=math.tau*j/n
                vs.append((rad*math.cos(a),rad*math.sin(a),z))
        for k in range(len(profile)-1):
            for j in range(n):
                fs.append((k*n+j,k*n+(j+1)%n,(k+1)*n+(j+1)%n,(k+1)*n+j))
        fs += [tuple(reversed(range(n))),tuple((len(profile)-1)*n+j for j in range(n))]
        pieces=[mesh(name,vs,fs,material)]
        if name=='bucket':
            arc=[(.19*math.cos(math.pi*j/8),0,.31+.20*math.sin(math.pi*j/8)) for j in range(9)]
            pieces += [tube('handle',arc,[.012]*9,'iron',n=6)]
        asset(name,'household',pieces)
    asset('sack','household',[ellipsoid('sack',(0,0,.29),(.23,.20,.29),'linen'),tube('tied mouth',[(0,0,.49),(0,0,.60)],[.075,.045],'linen',n=8),
                                  tube('cord',[(0,0,.51),(0,0,.53)],[.078,.078],'leather',n=8)])
    asset('anvil','tools',[box('base',(0,0,.11),(.44,.37,.20),'iron',bevel=.03),
                                box('waist',(0,0,.27),(.20,.22,.22),'iron',bevel=.018),
                                box('face',(0,0,.44),(.62,.28,.14),'iron',bevel=.014),
                                tube('horn',[(.3,0,.43),(.62,0,.41)],[.12,.008],'iron_edge',n=8)])
    asset('hammer','tools',[box('handle',(0,0,.24),(.045,.045,.48),'wood',bevel=.004),box('head',(0,0,.48),(.25,.10,.10),'iron',bevel=.01)])
    asset('axe','tools',[box('haft',(0,0,.40),(.045,.05,.80),'wood',bevel=.006),
                              mesh('blade',[(0,-.04,.60),(.23,-.04,.52),(.25,-.04,.84),(0,-.04,.77),(0,.04,.60),(.23,.04,.52),(.25,.04,.84),(0,.04,.77)],
                                   [(0,1,2,3),(7,6,5,4),(0,4,5,1),(1,5,6,2),(2,6,7,3),(3,7,4,0)],'iron')])
    asset('shovel','tools',[box('shaft',(0,0,.65),(.04,.04,1.05),'wood'),box('grip',(0,0,1.20),(.21,.04,.04),'wood'),
                                 box('spade',(0,0,.17),(.23,.035,.30),'iron',bevel=.035)])
    asset('pitchfork','tools',[box('shaft',(0,0,.78),(.04,.04,1.25),'wood')]+
          [tube('tine',[(x,0,.16),(x,-.025,.37),(x,0,.49)],[.012,.015,.02],'iron',n=5) for x in [-.10,-.035,.035,.10]]+
          [box('crossbar',(0,0,.45),(.24,.04,.035),'iron')])
    asset('firewood_stack','household',[tube('split log',[(x,-.27,z),(x,.27,z)],[.09,.08],'wood',n=6)
             for x,z in [(-.20,.09),(0,.09),(.20,.09),(-.10,.25),(.10,.25),(0,.40)]])
    asset('lantern','household',[box('base',(0,0,.04),(.19,.19,.05),'iron',bevel=.012),
                                      box('roof',(0,0,.30),(.21,.21,.065),'iron',bevel=.025),
                                      box('candle',(0,0,.14),(.055,.055,.16),'linen')]+
          [box('corner',(x,y,.17),(.018,.018,.25),'iron') for x in [-.075,.075] for y in [-.075,.075]])
    asset('well','architecture',[tube('stone well',[(0,0,.04),(0,0,.72)],[.64,.64],'stone',n=12),
                                      tube('water dark recess',[(0,0,.73),(0,0,.735)],[.51,.51],'wolf_dark',n=12)]+
          [box('well post',(x,0,1.10),(.13,.13,2.2),'wood') for x in [-.82,.82]]+
          [box('well crossbeam',(0,0,2.14),(1.9,.16,.15),'wood'),tube('rope',[(0,0,.75),(0,0,2.12)],[.014,.014],'linen',n=5)])


def foliage_blob(name,loc,scale,material='leaf'):
    bpy.ops.mesh.primitive_ico_sphere_add(subdivisions=1,radius=1,location=loc)
    o=bpy.context.object
    o.name=name
    o.scale=scale
    apply_transform(o)
    for v in o.data.vertices:
        v.co+=Vector((random.uniform(-.07,.07),random.uniform(-.07,.07),random.uniform(-.07,.07)))
    o.data.materials.append(mat(material))
    return o


def nature():
    for name,height,radius,layers in [('pine_tall',5.8,1.24,6),('spruce_young',3.1,.83,5)]:
        pieces=[tube('trunk',[(0,0,0),(.05,.02,height*.45),(-.04,0,height)],[.19,.12,.015],'bark',n=8)]
        for k in range(layers):
            base=height*(.22+.105*k)
            r=radius*(1-.14*k)
            n=11; vs=[]
            for j in range(n):
                a=math.tau*j/n+(k%2)*.2
                rr=r*random.uniform(.80,1.09)
                vs.append((rr*math.cos(a),rr*math.sin(a),base+random.uniform(-.12,.06)))
            vs += [(0,0,base+height*.27),(0,0,base+.09)]
            fs=[(j,(j+1)%n,n) for j in range(n)]+[(n+1,(j+1)%n,j) for j in range(n)]
            pieces += [mesh('needle whorl',vs,fs,'leaf_dark' if k%2==0 else 'leaf')]
        asset(name,'nature',pieces)
    for name,scale,dead in [('oak_mature',1,False),('oak_young',.65,False),('tree_dead',.85,True)]:
        pieces=[tube('trunk',[(0,0,0),(.12,0,1.6),(-.08,.08,3.0),(0,.1,4.2)],
                      [.31,.23,.13,.02],'bark',n=9)]
        for k in range(6):
            a=math.tau*k/6
            end=Vector((math.cos(a)*1.35,math.sin(a)*1.25,3.4+(k%3)*.45))
            start=Vector((.02,0,2.05+k*.16))
            pieces += [tube('bough',[start,(start+end)*.5+Vector((0,0,.2)),end],[.13,.075,.015],'bark',n=7)]
            if not dead:
                pieces += [foliage_blob('oak crown',end+Vector((0,0,.24)),(1.15,1.05,.85),'leaf' if k%2 else 'leaf_dark')]
        if not dead:
            pieces += [foliage_blob('top crown',(0,0,4.05),(1.1,1.1,.9))]
        for o in pieces:
            for v in o.data.vertices:
                v.co*=scale
        asset(name,'nature',pieces)
    asset('stump','nature',[tube('stump',[(0,0,0),(.03,0,.46)],[.34,.24],'bark',n=9),
                                 tube('cut face',[(.03,0,.46),(.03,0,.465)],[.235,.235],'wood_end',n=9)]+
          [tube('root',[(math.cos(a)*.15,math.sin(a)*.15,.15),(math.cos(a)*.64,math.sin(a)*.64,.015)],[.13,.025],'bark',n=6) for a in [0,1.5,3,4.7]])
    asset('fallen_log','nature',[tube('log',[(-1,0,.21),(.90,.03,.20)],[.22,.18],'bark',n=9),
                                      tube('cut end',[(-1.002,0,.21),(-1.005,0,.21)],[.213,.213],'wood_end',n=9),
                                      tube('branch',[(.25,0,.25),(.15,.37,.40)],[.08,.015],'bark',n=6)])
    for i,size in enumerate([(.46,.35,.30),(.85,.61,.65),(1.35,.9,1.1)]):
        o=foliage_blob('rock',(0,0,size[2]*.65),size,'stone')
        asset(f'rock_{i+1:02d}','nature',[o])
    for variant in range(3):
        pieces=[]
        for k in range(18+variant*8):
            a=random.random()*math.tau
            r=random.random()*.33
            x,y=math.cos(a)*r,math.sin(a)*r
            h=random.uniform(.15,.39)+variant*.025
            lean=Vector((math.cos(a)*.11,math.sin(a)*.11,h))
            side=Vector((-math.sin(a)*.022,math.cos(a)*.022,0))
            start=Vector((x,y,0))
            vs=[tuple(start-side),tuple(start+side),tuple(start+lean)]
            # Back-to-back triangles avoid dependence on alpha or culling flags.
            pieces += [mesh('blade',vs,[(0,1,2),(2,1,0)],'grass' if k%3 else 'leaf')]
        asset(f'grass_clump_{variant+1:02d}','nature',pieces)
    fern=[]
    for k in range(8):
        a=math.tau*k/8
        end=Vector((math.cos(a)*.52,math.sin(a)*.52,.26))
        fern += [tube('fern rachis',[(0,0,.03),end*.5+Vector((0,0,.19)),end],[.012,.009,.003],'leaf',n=5)]
        for j in range(1,7):
            t=j/7
            center=end*t+Vector((0,0,.23*math.sin(math.pi*t)))
            side=Vector((-math.sin(a),math.cos(a),0))*.14*(1-t*.7)
            forward=Vector((math.cos(a),math.sin(a),0))*.07
            for s in [-1,1]:
                fern += [mesh('pinna',[tuple(center-forward),tuple(center+side*s),tuple(center+forward)],[(0,1,2),(2,1,0)],'leaf')]
    asset('fern','nature',fern)
    asset('shrub','nature',[foliage_blob('shrub crown',(x,y,z),(.48,.45,.42),'leaf_dark' if i%2 else 'leaf')
                for i,(x,y,z) in enumerate([(-.31,0,.39),(.25,0,.36),(0,.21,.60),(0,-.19,.48)])])
    reeds=[]
    for k in range(9):
        x,y=random.uniform(-.28,.28),random.uniform(-.23,.23)
        height=random.uniform(.72,1.10)
        reeds += [tube('reed stem',[(x,y,0),(x+.06,y,height)],[.009,.006],'grass',n=5),
                  ellipsoid('cattail',(x+.06,y,height-.06),(.028,.028,.13),'wood',segments=6,rings=4)]
    asset('reeds','nature',reeds)
    mushrooms=[]
    for x,y,h in [(-.10,0,.17),(.12,.06,.25),(0,-.1,.12)]:
        mushrooms += [tube('stem',[(x,y,0),(x,y,h)],[.022,.018],'linen',n=6),
                      ellipsoid('cap',(x,y,h),(.10,.10,.045),'clay',segments=8,rings=4)]
    asset('mushrooms','nature',mushrooms)
    for name,material in [('ground_soil','soil'),('ground_grass','grass'),('ground_stone','stone'),('ground_path','soil')]:
        n=8;vs=[];fs=[]
        for y in range(n+1):
            for x in range(n+1):
                z=0 if x in [0,n] or y in [0,n] else random.uniform(-.018,.018)
                vs.append((x*2/n,y*2/n,z))
        for y in range(n):
            for x in range(n):
                a=y*(n+1)+x
                fs += [(a,a+1,a+n+2),(a,a+n+2,a+n+1)]
        pieces=[mesh(name,vs,fs,material)]
        if name=='ground_path':
            for k in range(11):
                pieces += [ellipsoid('path pebble',(random.uniform(.2,1.8),random.uniform(.2,1.8),.018),(.08,.055,.028),'stone',segments=6,rings=4)]
        asset(name,'terrain',pieces,.25)


def place(name,loc,angle=0):
    src=MODULES[name]
    o=src.copy();o.data=src.data
    bpy.context.collection.objects.link(o)
    o.name='COTTAGE_'+name
    o.hide_set(False);o.hide_render=False
    o.location=loc;o.rotation_euler.z=angle
    return o


def cottage():
    pieces=[]
    for x in [-2,0]:
        for y in [-2,0]:
            pieces += [place('foundation_2x2',(x,y,0)),place('floor_wood_2x2',(x,y,.18))]
    # Four sides, 2 modules each. Every opening is real geometry.
    pieces += [place('wall_door_2m',(-2,-2,.27)),place('wall_window_2m',(0,-2,.27))]
    for x in [-2,0]:
        pieces += [place('wall_solid_2m',(x,2,.27))]
    for y in [-2,0]:
        pieces += [place('wall_window_2m',(-2,y,.27),math.pi/2),place('wall_solid_2m',(2,y,.27),math.pi/2)]
    for x in [-2,0]:
        pieces += [place('roof_thatch_2m',(x,-2,2.77)),place('roof_thatch_2m',(x+2,2,2.77),math.pi)]
    pieces += [place('gable_4m',(-2,0,2.77),math.pi/2),place('gable_4m',(2,0,2.77),math.pi/2),
               place('ridge_2m',(-2,0,4.02)),place('ridge_2m',(0,0,4.02)),
               place('chimney_1p6m',(1.1,.65,3.05)),place('stairs_1m',(-1,-2.43,0)),
               place('barrel',(2.53,-1.4,0)),place('crate',(2.45,-.45,0)),place('firewood_stack',(2.45,.55,0)),
               place('bench',(.55,-2.48,0)),place('bed',(-1.25,.85,.27)),place('table',(.7,.45,.27)),
               place('chair',(.8,-.3,.27)),place('shelf',(1.65,1.55,.27),math.pi/2)]
    door=[box('door board',(-1,-2.04,1.28),(.88,.07,1.97),'wood',bevel=.015),
          box('door strap',(-1,-2.09,.71),(.84,.025,.05),'iron'),box('door strap',(-1,-2.09,1.80),(.84,.025,.05),'iron'),
          ellipsoid('door handle',(-.71,-2.10,1.26),(.035,.025,.045),'iron',segments=8,rings=4)]
    pieces += [join('COTTAGE_STATIC_DOOR',door)]
    export('environment/prefab_cottage.glb',pieces,False)
    register('prefab_cottage','prefab',pieces,'environment/prefab_cottage.glb')
    for o in MODULES.values():
        o.hide_render=True;o.hide_set(True)
    save('environment/cottage_assembled.blend')
    # Add a small forest edge to make material and scale readable in the preview.
    for name,loc in [('pine_tall',(-4,2,0)),('spruce_young',(3.9,3.5,0)),('oak_young',(-4,-1,0)),
                     ('rock_02',(3,-3,0)),('grass_clump_03',(-2.7,-2.5,0)),('shrub',(2.6,1.8,0))]:
        place(name,loc)
    stage(pieces,'previews/cottage.png',target=(0,0,1.45),distance=11,resolution=(1600,1200))


def build():
    reset()
    # Append native image-based PBR materials; all images are packed in the library.
    library=ROOT/'materials/arcana_pbr_materials.blend'
    if library.exists():
        with bpy.data.libraries.load(str(library),link=False) as (source,target):
            target.materials=[n for n in source.materials if n.startswith('ARC_PBR_')]
        for m in target.materials:
            key=m.name.replace('ARC_PBR_','').split('.')[0]
            MATS['leaf_dark' if key=='moss' else key]=m
    architecture();props();nature()
    # Asset Browser library is laid out for inspection; glb files stay at authoring origin.
    for idx,(name,o) in enumerate(MODULES.items()):
        o.location=((idx%9)*3.6,(idx//9)*4.4,0)
    save('environment/environment_library.blend')
    for o in MODULES.values():
        o.location=(0,0,0)
        o.hide_render=True
    cottage()
    json_out('metadata/environment_inventory.json',ASSETS)
    json_out('metadata/building_sockets.json',{'grid_m':.25,'wall_width_m':2,'wall_height_m':2.5,'floor_module_m':[2,2],
            'roof_half_span_m':2,'roof_rise_m':1.25,'sockets':SOCKETS,
            'rules':['Wall pivot at lower left; +X width, local Z height','Floor pivot lower-left XY','Roof eave -> ridge is +Y, +Z','Cottage finished floor z=.27; walls start z=.27; eaves z=2.77','For opposite roof use Z rotation 180 degrees and translate x+2,y+4','1-meter door opening; dynamic door origin at left hinge, place local x=.56,z=0']})


if __name__=='__main__':
    build()
