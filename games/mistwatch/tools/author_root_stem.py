"""衍生獨立短幹根部：裁切最低0.10m、續接原UV並封口；保留原掃描資產。"""
import collections
import argparse
import hashlib
import json
import math
import struct
from pathlib import Path

EXPECTED='B964D1F7B16FB3602FF038AE111C79D0BFC029185896E95DA97CC1ACB4F59145'

def sub(a,b):return tuple(x-y for x,y in zip(a,b))
def dot(a,b):return sum(x*y for x,y in zip(a,b))
def cross(a,b):return (a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],a[0]*b[1]-a[1]*b[0])
def unit(p):
    length=math.sqrt(dot(p,p))
    if length<1e-10:raise ValueError('degenerate_normal')
    return tuple(x/length for x in p)
def unpack(p):return tuple(max(-1,struct.unpack('<b',bytes([(p>>(8*j))&255]))[0]/127) for j in range(3))
def pack(n,w):return sum((round(max(-1,min(1,x))*127)&255)<<(8*j) for j,x in enumerate(n)) | (w&0xff000000)
def key(p):return tuple(round(x*1000000) for x in p[:3])
def f32(x):return struct.unpack('<f',struct.pack('<f',x))[0]

def validate_simple_ring(points):
    def orientation(a,b,c):return (b[0]-a[0])*(c[1]-a[1])-(b[1]-a[1])*(c[0]-a[0])
    def on(a,b,p):return abs(orientation(a,b,p))<=1e-12 and all(min(a[k],b[k])-1e-12<=p[k]<=max(a[k],b[k])+1e-12 for k in (0,1))
    n=len(points)
    if n<3:raise ValueError('short_ring')
    for i in range(n):
        a,b=points[i],points[(i+1)%n]
        if math.dist(a,b)<1e-7:raise ValueError('zero_length_cut_edge')
        for j in range(i+1,n):
            if j==i+1 or (i==0 and j==n-1):continue
            c,d=points[j],points[(j+1)%n]
            s1,s2,s3,s4=orientation(a,b,c),orientation(a,b,d),orientation(c,d,a),orientation(c,d,b)
            if (s1*s2<0 and s3*s4<0) or on(a,b,c) or on(a,b,d) or on(c,d,a) or on(c,d,b):
                raise ValueError('self_intersecting_cut')

def author(data):
    magic,version,nv,ni=struct.unpack_from('<4I',data)
    assert magic==0x334d4541 and version==1 and len(data)==16+28*nv+4*ni
    old=list(struct.iter_unpack('<3fII2f',data[16:16+nv*28]))
    src=struct.unpack_from(f'<{ni}I',data,16+nv*28)
    base=min(v[1] for v in old);height=f32(base+.10)
    vertices=list(old);indices=[];intersections={};boundary=[];unchanged=[];removed=[];crossed=[]
    def clip(a,b):
        cache=tuple(sorted((a,b)))
        if cache in intersections:return intersections[cache]
        # 固定端點順序，避免不同三角面產生不一致的切面位置。
        a,b=cache;p,q=old[a],old[b];t=(height-p[1])/(q[1]-p[1])
        position=[f32(p[j]+(q[j]-p[j])*t) for j in range(3)];position[1]=height
        n=unit(tuple(x+(y-x)*t for x,y in zip(unpack(p[3]),unpack(q[3]))))
        colour=sum(round(((p[4]>>(8*j))&255)+(((q[4]>>(8*j))&255)-((p[4]>>(8*j))&255))*t)<<(8*j) for j in range(4))
        vertex=tuple(position)+(pack(n,p[3]),colour,f32(p[5]+(q[5]-p[5])*t),f32(p[6]+(q[6]-p[6])*t))
        i=len(vertices);vertices.append(vertex);intersections[cache]=i;return i
    for face in range(0,ni,3):
        ids=src[face:face+3];above=[old[i][1]>=height for i in ids]
        if all(above):indices.extend(ids);unchanged.append(face//3);continue
        if not any(above):removed.append(face//3);continue
        crossed.append(face//3);poly=[]
        for j in range(3):
            a,b=ids[j],ids[(j+1)%3]
            if above[j]:poly.append(a)
            if above[j]!=above[(j+1)%3]:poly.append(clip(a,b))
        assert len(poly) in (3,4)
        for j in range(1,len(poly)-1):indices.extend((poly[0],poly[j],poly[j+1]))
        cut_edges=[(a,b) for a,b in zip(poly,poly[1:]+poly[:1]) if vertices[a][1]==height and vertices[b][1]==height]
        assert len(cut_edges)==1
        a,b=cut_edges[0];boundary.append((a,b,ids))
    # 裁切必須形成單一閉環，否則拒絕，不猜測如何補洞。
    successors={};predecessors={}
    for a,b,_ in boundary:
        ka,kb=key(vertices[a]),key(vertices[b])
        if ka==kb or ka in successors or kb in predecessors:raise ValueError('nonmanifold_cut')
        successors[ka]=kb;predecessors[kb]=ka
    if successors.keys()!=predecessors.keys():raise ValueError('open_cut')
    start=min(successors);ring=[start];cursor=successors[start]
    while cursor!=start:
        if cursor in ring:raise ValueError('multiple_cut_loops')
        ring.append(cursor);cursor=successors[cursor]
    if len(ring)!=len(boundary):raise ValueError('disconnected_cut')
    validate_simple_ring([(k[0]/1e6,k[2]/1e6) for k in ring])
    retained_end=len(indices);uv_deltas=[];bottom_by_key={}
    for a,b,ids in boundary:
        va,vb=vertices[a],vertices[b];p,q,r=[old[i] for i in ids]
        e1,e2=sub(q[:3],p[:3]),sub(r[:3],p[:3]);g11,g12,g22=dot(e1,e1),dot(e1,e2),dot(e2,e2)
        det=g11*g22-g12*g12
        if det<1e-16:raise ValueError('degenerate_source_uv_basis')
        # 原三角面UV的最小二乘梯度；只外插新短幹段，不修改上部原UV。
        s=(e1[1]*g22-e2[1]*g12)/det;t=(e2[1]*g11-e1[1]*g12)/det
        gradient=[s*(q[j]-p[j])+t*(r[j]-p[j]) for j in (5,6)]
        if not all(math.isfinite(g) for g in gradient):raise ValueError('nonfinite_uv_gradient')
        delta=[(base-height)*g for g in gradient];uv_deltas.append(max(map(abs,delta)))
        n=unit(cross(sub(va[:3],vb[:3]),(0,base-height,0)))
        lower=[]
        for v in (va,vb):
            i=len(vertices);vertices.append((v[0],base,v[2],pack(n,v[3]),v[4],f32(v[5]+delta[0]),f32(v[6]+delta[1])));lower.append(i)
            bottom_by_key[key(v)]=i
        # 共享上環原頂點，邊的方向反轉，避免裙邊與原表面朝向衝突。
        la,lb=lower;indices.extend((b,a,la,b,la,lb))
    skirt_end=len(indices)
    # 下蓋用耳切而非中心扇形；非凸根截面也必須沒有外翻或交叉。
    cap_ring=[bottom_by_key[k] for k in reversed(ring)]
    def orient(a,b,c):
        p,q,r=(vertices[i] for i in (a,b,c));return (q[0]-p[0])*(r[2]-p[2])-(q[2]-p[2])*(r[0]-p[0])
    area=sum(vertices[a][0]*vertices[b][2]-vertices[b][0]*vertices[a][2] for a,b in zip(cap_ring,cap_ring[1:]+cap_ring[:1]))/2
    if area<=0:raise ValueError('cap_not_downward')
    remaining=cap_ring[:];cap=[]
    while len(remaining)>3:
        found=False
        for j,b in enumerate(remaining):
            a,c=remaining[j-1],remaining[(j+1)%len(remaining)]
            if orient(a,b,c)<=1e-12:continue
            if any(orient(a,b,p)>=-1e-12 and orient(b,c,p)>=-1e-12 and orient(c,a,p)>=-1e-12 for p in remaining if p not in (a,b,c)):continue
            cap.extend((a,b,c));remaining.pop(j);found=True;break
        if not found:raise ValueError('cap_triangulation_failed')
    cap.extend(remaining)
    # 封口與側幹共位置但分開法線，保留下向底蓋而不是沿用側幹法線。
    cap_ids={}
    for i in cap:
        if i not in cap_ids:
            v=vertices[i];cap_ids[i]=len(vertices);vertices.append(v[:3]+(pack((0,-1,0),v[3]),)+v[4:])
    indices.extend(cap_ids[i] for i in cap)
    # 只保留實際引用的頂點；不可把被裁掉的掃描平板留在測量包絡中。
    used=sorted(set(indices));mapping={old_index:i for i,old_index in enumerate(used)}
    compact=[vertices[i] for i in used];remapped=[mapping[i] for i in indices]
    result=struct.pack('<4I',magic,version,len(compact),len(remapped))+b''.join(struct.pack('<3fII2f',*v) for v in compact)+struct.pack(f'<{len(remapped)}I',*remapped)
    meta=dict(schema_version=1,operation='replace_lowest_0.10m_with_closed_bark_stem',base=base,cut_height=height,source_vertices=nv,source_triangles=ni//3,
        removed_complete_faces=len(removed),clipped_faces=len(crossed),unchanged_complete_faces=len(unchanged),cut_loop_vertices=len(ring),
        retained_triangles=retained_end//3,skirt_triangles=(skirt_end-retained_end)//3,cap_triangles=len(cap)//3,
        output_vertices=len(compact),output_triangles=len(remapped)//3,max_uv_extrapolation=max(uv_deltas),cap_area=area,
        original_vertex_map=[dict(source=i,target=mapping[i]) for i in used if i<nv],unchanged_source_faces=unchanged,
        limits='New derived root geometry, not classification that all removed faces are scan ground. Bottom cap is downward; no new texture/tint/exposure or collision shrink. Native terrain and visual acceptance still required.')
    return result,meta

if __name__=='__main__':
    parser=argparse.ArgumentParser(description='Cook independently derived CC0 canopy root into a fresh generated directory')
    parser.add_argument('--source',type=Path,required=True)
    parser.add_argument('--output-dir',type=Path,required=True)
    args=parser.parse_args();SOURCE=args.source.resolve();OUT=args.output_dir.resolve()
    data=SOURCE.read_bytes();assert hashlib.sha256(data).hexdigest().upper()==EXPECTED
    for bad in [[(0,0),(1,1),(0,1),(1,0)],[(0,0),(0,0),(1,1)]]:
        try:validate_simple_ring(bad)
        except ValueError:pass
        else:raise AssertionError('invalid ring accepted')
    result,meta=author(data);repeat,_=author(data);assert result==repeat
    # CMake會預先建立OUTPUT父目錄；只接受空生成目錄，仍不覆寫既有證據。
    OUT.mkdir(exist_ok=True)
    if any(OUT.iterdir()):raise ValueError('generated_root_directory_not_empty')
    target=OUT/'island-tree-01-lod2-trunk-root-stem.aemesh';target.write_bytes(result)
    meta.update(status='derived_asset_cpu_author_only_not_native_acceptance',source=str(SOURCE),source_sha256=EXPECTED,output=str(target),output_sha256=hashlib.sha256(result).hexdigest().upper(),author_sha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest().upper(),license='CC0; source batch LICENSE.md retained; derived root only')
    (OUT/'author-receipt.json').write_text(json.dumps(meta,indent=2),encoding='utf-8')
    print(json.dumps({k:v for k,v in meta.items() if k not in ('original_vertex_map','unchanged_source_faces')}))
