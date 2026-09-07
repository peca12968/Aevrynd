"""匯出已逐頂點核對 AESK 基底的暫態角色差值資料。"""
import argparse
import hashlib
import importlib.util
import json
import math
import os
from pathlib import Path
import struct
import tempfile

SPEC=importlib.util.spec_from_file_location('character_normalizer',Path(__file__).with_name('normalize-character-gltf.py'))
NORMALIZER=importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(NORMALIZER)
D=NORMALIZER.DECODER

def export(source, asset, destination, scale=1., binary_output=None):
    if not math.isfinite(scale) or scale<=0:
        raise ValueError('Invalid scale')
    doc=json.loads(source.read_text(encoding='utf-8'),object_pairs_hook=D.unique_object)
    if len(doc['buffers'])!=1 or len(doc['meshes'])!=1:
        raise ValueError('One buffer and mesh required')
    binary_path=(source.parent/doc['buffers'][0]['uri']).resolve()
    binary_path.relative_to(source.parent.resolve())
    binary=binary_path.read_bytes()
    if len(binary)!=doc['buffers'][0]['byteLength']:
        raise ValueError('Buffer length mismatch')
    data=asset.read_bytes()
    if len(data)<64 or len(data)>64*1024*1024:
        raise ValueError('AESK length invalid')
    magic,version,header,flags,size,table,chunk_count,vertex_count,index_count=struct.unpack_from('<IIIIQQIII',data)
    if (magic,version,header)!=(0x4B534541,2,64) or size!=len(data) or table!=64 or chunk_count>128 or vertex_count>100000 or index_count>300000:
        raise ValueError('AESK header invalid')
    D.checked_range(table,chunk_count*32,len(data),'chunk table')
    chunks={}
    for i in range(chunk_count):
        kind,revision,required,offset,length,count,stride=struct.unpack_from('<4sHHQQII',data,table+i*32)
        D.checked_range(offset,length,len(data),'chunk')
        if kind in chunks or length!=count*stride or offset<table+chunk_count*32:
            raise ValueError('AESK chunk invalid')
        chunks[kind]=(offset,length,count,stride)
    vo,_,vc,vs=chunks[b'VERT'];io,_,ic,its=chunks[b'INDX']
    if (vc,vs,ic,its)!=(vertex_count,28,index_count,4):
        raise ValueError('AESK geometry declaration invalid')
    mesh=doc['meshes'][0]
    names=mesh['extras']['targetNames']
    if not names or len(names)>128 or len(set(names))!=len(names) or any(not isinstance(n,str) or not n or len(n)>128 for n in names):
        raise ValueError('Target names invalid')
    offsets=[[] for _ in names];baseline=[];source_indices=[]
    for primitive in mesh['primitives']:
        if primitive.get('mode',4)!=4:
            raise ValueError('Only triangle primitives are supported')
        positions,_,kind=D.decode_accessor(doc,binary,primitive['attributes']['POSITION'])
        if kind!='VEC3' or len(primitive['targets'])!=len(names):
            raise ValueError('Primitive targets invalid')
        vertex_base=len(baseline)
        if 'indices' in primitive:
            local_indices,component,index_kind=D.decode_accessor(doc,binary,primitive['indices'])
            if index_kind!='SCALAR' or component not in (5121,5123,5125):
                raise ValueError('Primitive index declaration invalid')
            local_indices=[row[0] for row in local_indices]
        else:
            local_indices=list(range(len(positions)))
        if len(local_indices)%3 or any(not isinstance(index,int) or isinstance(index,bool) or index<0 or index>=len(positions) for index in local_indices):
            raise ValueError('Primitive topology invalid')
        # 與正式 AESK 轉換器完全相同：轉為左手座標繞向、循環旋轉至
        # 字典序最小表示，再於每個 primitive 內排序三角形。
        canonical_triangles=[]
        for start in range(0,len(local_indices),3):
            a,b,c=local_indices[start:start+3]
            oriented=(a,c,b)
            rotations=(oriented,oriented[1:]+oriented[:1],oriented[2:]+oriented[:2])
            canonical_triangles.append(min(rotations))
        for triangle in sorted(canonical_triangles):
            source_indices.extend(vertex_base+index for index in triangle)
        baseline.extend((p[0]*scale,p[1]*scale,-p[2]*scale) for p in positions)
        for target_index,target in enumerate(primitive['targets']):
            values,component,kind=D.decode_accessor(doc,binary,target['POSITION'])
            if kind!='VEC3' or component!=5126 or len(values)!=len(positions):
                raise ValueError('Morph position declaration invalid')
            offsets[target_index].extend((v[0]*scale,v[1]*scale,-v[2]*scale) for v in values)
    if len(baseline)!=vertex_count:
        raise ValueError('Baseline vertex count mismatch')
    for index,position in enumerate(baseline):
        expected=struct.pack('<3f',*position)
        if expected!=data[vo+index*28:vo+index*28+12]:
            raise ValueError(f'Baseline position mismatch at vertex {index}')
    indices=list(struct.unpack_from('<'+'I'*index_count,data,io))
    if index_count%3 or any(i>=vertex_count for i in indices):
        raise ValueError('AESK topology invalid')
    if source_indices!=indices:
        raise ValueError('Source and AESK topology mismatch')
    for target in offsets:
        if any(not all(math.isfinite(v) for v in delta) for delta in target):
            raise ValueError('Morph overflow')
    encoded_names=[]
    for name in names:
        try:
            encoded=name.encode('utf-8')
        except UnicodeEncodeError as error:
            raise ValueError('Target name is not valid UTF-8') from error
        if not encoded or len(encoded)>128 or b'\0' in encoded:
            raise ValueError('UTF8 target name exceeds format limits')
        encoded_names.append(encoded)
    result={'schemaVersion':1,'format':'AEVRYND_CHARACTER_MORPH_JSON_WIP',
            'assetSha256':hashlib.sha256(data).hexdigest(),'vertexCount':vertex_count,'indexCount':index_count,
            'baselineVerticesVerified':vertex_count,'scale':scale,'indices':indices,
            'requiresTangentRebuild':True,'targets':[{'name':name,'minimumWeight':0,'maximumWeight':1,'offsets':delta} for name,delta in zip(names,offsets)]}
    if destination.exists() or (binary_output is not None and binary_output.exists()):
        raise ValueError('Output already exists')
    if binary_output is not None and destination.resolve()==binary_output.resolve():
        raise ValueError('JSON and binary outputs must differ')
    # 在建立任何輸出前先完成 JSON 與二進位序列化，避免輸入錯誤留下半套資料。
    json_payload=json.dumps(result,separators=(',',':'),allow_nan=False).encode('utf-8')
    payload=None
    if binary_output is not None:
        payload=bytearray(b'ACMS'+struct.pack('<I',1)+hashlib.sha256(data).digest()+struct.pack('<III',vertex_count,index_count,len(names)))
        payload.extend(struct.pack('<'+'I'*index_count,*indices))
        for encoded,delta in zip(encoded_names,offsets):
            payload.extend(struct.pack('<H',len(encoded))+encoded+struct.pack('<ff',0.,1.))
            for value in delta:
                try:
                    payload.extend(struct.pack('<3f',*value))
                except (OverflowError,struct.error) as error:
                    raise ValueError('Morph value is not representable as float32') from error
    destination.parent.mkdir(parents=True,exist_ok=True)
    if binary_output is not None:
        binary_output.parent.mkdir(parents=True,exist_ok=True)
    temporary=[];committed_binary=False
    try:
        def stage(parent,contents):
            handle=tempfile.NamedTemporaryFile(prefix='.aevrynd-morph-',dir=parent,delete=False)
            temporary.append(Path(handle.name))
            with handle:
                handle.write(contents)
                handle.flush();os.fsync(handle.fileno())
            return Path(handle.name)
        json_temporary=stage(destination.parent,json_payload)
        binary_temporary=stage(binary_output.parent,payload) if binary_output is not None else None
        if binary_temporary is not None:
            os.replace(binary_temporary,binary_output);temporary.remove(binary_temporary);committed_binary=True
        os.replace(json_temporary,destination);temporary.remove(json_temporary)
    except Exception:
        if committed_binary and binary_output is not None and binary_output.exists():
            binary_output.unlink()
        raise
    finally:
        for path in temporary:
            try:path.unlink()
            except FileNotFoundError:pass
    print(json.dumps({k:result[k] for k in ('schemaVersion','assetSha256','vertexCount','indexCount','baselineVerticesVerified')}))

if __name__=='__main__':
    p=argparse.ArgumentParser()
    p.add_argument('--source',type=Path,required=True)
    p.add_argument('--asset',type=Path,required=True)
    p.add_argument('--output',type=Path,required=True)
    p.add_argument('--scale',type=float,default=1.)
    p.add_argument('--binary-output',type=Path)
    a=p.parse_args()
    export(a.source,a.asset,a.output,a.scale,a.binary_output)
