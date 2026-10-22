import sys
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parent))
from common import *
import struct
import numpy as np


def glb_check(path):
    raw=path.read_bytes()
    assert raw[:4]==b'glTF'
    size,kind=struct.unpack_from('<II',raw,12)
    doc=json.loads(raw[20:20+size])
    offset=20+size
    bindata=raw[offset+8:]
    types={'SCALAR':1,'VEC2':2,'VEC3':3,'VEC4':4,'MAT4':16}
    dtypes={5120:'i1',5121:'u1',5122:'<i2',5123:'<u2',5125:'<u4',5126:'<f4'}
    def accessor(index):
        a=doc['accessors'][index]
        bv=doc['bufferViews'][a['bufferView']]
        dtype=np.dtype(dtypes[a['componentType']])
        n=types[a['type']]
        at=bv.get('byteOffset',0)+a.get('byteOffset',0)
        stride=bv.get('byteStride',dtype.itemsize*n)
        arr=np.ndarray((a['count'],n),dtype=dtype,buffer=bindata,offset=at,strides=(stride,dtype.itemsize))
        if a.get('normalized'):
            arr=arr.astype(float)/np.iinfo(dtype).max
        assert np.isfinite(arr).all(), f'nonfinite accessor {index}'
        return arr
    for i in range(len(doc.get('accessors',[]))):
        accessor(i)
    triangles=0
    for m in doc.get('meshes',[]):
        for prim in m['primitives']:
            attrs=prim['attributes']
            triangles+=doc['accessors'][prim['indices']]['count']//3
            assert 'NORMAL' in attrs and 'TEXCOORD_0' in attrs
            if 'WEIGHTS_0' in attrs:
                weights=accessor(attrs['WEIGHTS_0'])
                assert np.max(np.abs(weights.sum(axis=1)-1))<.002
                assert (weights>=0).all()
    clips=[]
    for anim in doc.get('animations',[]):
        duration=max(float(accessor(s['input']).max()-accessor(s['input']).min()) for s in anim['samplers'])
        clips.append({'name':anim['name'],'duration_seconds':round(duration,6),'channels':len(anim['channels'])})
    return {'file':path.relative_to(ROOT).as_posix(),'triangles':triangles,'skins':len(doc.get('skins',[])),
            'clips':clips,'size_bytes':len(raw),'status':'pass'}


def blend_check(path,preview=False):
    bpy.ops.wm.open_mainfile(filepath=str(path))
    rigs=[o for o in bpy.context.scene.objects if o.type=='ARMATURE']
    result={'file':path.relative_to(ROOT).as_posix(),'status':'pass','rigs':[]}
    for rig in rigs:
        errors=[]
        meshes=[o for o in bpy.context.scene.objects if o.type=='MESH' and any(m.type=='ARMATURE' and m.object==rig for m in o.modifiers)]
        for o in meshes:
            for v in o.data.vertices:
                weights=[g.weight for g in v.groups if o.vertex_groups[g.group].name in rig.data.bones]
                if not weights or abs(sum(weights)-1)>.001:
                    errors.append(f'{o.name}: invalid weights vertex {v.index}')
                    break
        loop_errors={}
        max_gap=0
        for a in bpy.data.actions:
            rig.animation_data_create()
            rig.animation_data.action=a
            if a.slots:
                rig.animation_data.action_slot=a.slots[0]
            start,end=[int(f) for f in a.frame_range]
            bpy.context.scene.frame_set(start)
            matrices={p.name:p.matrix.copy() for p in rig.pose.bones}
            if a.get('loop'):
                bpy.context.scene.frame_set(end)
                err=max(max(abs(x-y) for row1,row2 in zip(matrices[p.name],p.matrix) for x,y in zip(row1,row2)) for p in rig.pose.bones)
                loop_errors[a.name]=err
                if err>1e-4:
                    errors.append(f'{a.name}: loop mismatch {err}')
            if 'Walk' in a.name or 'Run' in a.name:
                for f in range(start,end+1):
                    bpy.context.scene.frame_set(f)
                    for bone in rig.pose.bones:
                        # Test connected joints, independent of intentional rest-space offsets.
                        if bone.parent and (bone.bone.head_local-bone.parent.bone.tail_local).length<1e-5:
                            gap=(bone.head-bone.parent.tail).length
                            max_gap=max(max_gap,gap)
                if max_gap>.008:
                    errors.append(f'{a.name}: connected joint gap {max_gap:.5f}m')
        result['rigs'].append({'name':rig.name,'bones':len(rig.data.bones),'meshes':len(meshes),
                              'loop_matrix_error':loop_errors,'max_locomotion_joint_gap_m':max_gap,'errors':errors})
        if errors:
            result['status']='fail'
    # Save predictable colored viewport framing without touching geometry or action data.
    target=(0,0,.95);distance=3.3
    rel=path.relative_to(ROOT).as_posix()
    if 'environment_library' in rel:
        target=(14.5,13,1.3);distance=38
    elif 'cottage_assembled' in rel:
        target=(0,0,1.8);distance=9
    elif 'arcana_pbr' in rel:
        target=(2.25,.75,0);distance=7
    elif '/deer.' in rel:
        target=(0,-.1,1.1);distance=4.2
    elif '/door_' in rel:
        target=(.44,0,1);distance=3.3
    elif '/chest_' in rel:
        target=(0,0,.3);distance=2.2
    for screen in bpy.data.screens:
        for area in screen.areas:
            if area.type=='VIEW_3D':
                space=area.spaces.active
                space.shading.color_type='MATERIAL'
                space.region_3d.view_location=target
                space.region_3d.view_distance=distance
                space.region_3d.view_rotation=Vector((4,-6,3)).to_track_quat('Z','Y')
    if rigs:
        rig=rigs[0]
        idle=next((a for a in bpy.data.actions if 'Idle' in a.name or 'Closed' in a.name),None)
        if idle:
            rig.animation_data.action=idle
            rig.animation_data.action_slot=idle.slots[0]
            bpy.context.scene.frame_start=1;bpy.context.scene.frame_end=int(idle.frame_range[1])
        bpy.context.scene.frame_set(1)
    bpy.context.preferences.filepaths.save_version=0
    bpy.context.preferences.filepaths.file_preview_type='NONE'
    bpy.ops.wm.save_as_mainfile(filepath=str(path))
    return result


if __name__=='__main__':
    report={'glb':[],'blend':[]}
    for p in sorted(ROOT.rglob('*.glb')):
        try:
            report['glb'].append(glb_check(p))
        except Exception as e:
            report['glb'].append({'file':p.relative_to(ROOT).as_posix(),'status':'fail','error':str(e)})
    for p in sorted(ROOT.rglob('*.blend')):
        report['blend'].append(blend_check(p))
    report['passed']=all(x['status']=='pass' for group in ['glb','blend'] for x in report[group])
    json_out('metadata/validation.json',report)
    print('ARCANA_VALIDATION',report['passed'])
