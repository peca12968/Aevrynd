"""Root：取得官方蕁麻原檔，保留六型真幾何，轉成可供原生引擎讀取的資產。"""
from pathlib import Path
from datetime import datetime, timezone
from types import SimpleNamespace
import hashlib, importlib.util, json, math, struct, sys, urllib.request

F=Path('D:/Download/Aevrynd/Worktrees/mistwatch-clean-20261002')
H=Path(__file__).resolve().parent
TC=Path('D:/Download/Aevrynd/ToolCache/DirectXTex/may2026/texconv.exe')
OFFICIAL='https://polyhaven.com/a/nettle_plant'
PINS={
 'nettle_plant_2k.gltf':('b684bcbe68aa98aa7988e3fa3b40ffd1',10450),
 'nettle_plant.bin':('975cb59d9f42871bb448de70aeecbc01',956560),
 'nettle_plant_diff_2k.jpg':('bd08db8a634d02c12a0cf4f4daac8937',1197929),
 'nettle_plant_nor_gl_2k.jpg':('f9c7a2f400685159583a75221e06bfa8',2356297),
 'nettle_plant_arm_2k.jpg':('dee323f552417bf65614c1f9bbb73214',1566589),
 'nettle_plant_alpha_2k.png':('e41081a23ef3480364649e152b0a09b3',421402)}
TOOL_PINS={'convert-glb-to-aemesh.py':'33bc28f796f30ca5ccbd51b5022579fb994b16e60799a6d7ed9fa5fd66aa0c61',
 'bake-photoreal-materials.py':'90e648ece087da2a8daaadc12756492f5f3f9e3abcdd4c08337b0cf03d2366bc'}

def need(ok,why):
    if not ok:raise ValueError(why)

def row(p):
    b=p.read_bytes();return dict(path=p.as_posix(),bytes=len(b),sha256=hashlib.sha256(b).hexdigest())

def dump(p,v):p.write_text(json.dumps(v,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')

def download(url,limit):
    need(url.startswith('https://api.polyhaven.com/') or url.startswith('https://dl.polyhaven.org/file/ph-assets/'), 'Only official public asset endpoint')
    req=urllib.request.Request(url,headers={'User-Agent':'Aevrynd-local-asset-import/1.0'})
    with urllib.request.urlopen(req,timeout=30) as r:
        need(r.status==200 and r.geturl().split('/')[2] in ('api.polyhaven.com','dl.polyhaven.org'),'Official endpoint drift')
        b=r.read(limit+1);need(len(b)<=limit,'Download byte limit exceeded');return b

def module(filename):
    p=F/'tools'/filename;need(row(p)['sha256']==TOOL_PINS[filename],'Cook tool drift')
    spec=importlib.util.spec_from_file_location(filename.replace('-','_').replace('.py',''),p)
    m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m);return m

def main():
    need(sys.dont_write_bytecode and not sys.flags.optimize and not (H/'prepare-receipt.json').exists(),'One non-optimized -B Root run')
    source=H/'source';prepared=H/'prepared';out=H/'cooked'
    need(not source.exists() and not prepared.exists() and not out.exists(),'Preserve previous artifacts')
    source.mkdir();prepared.mkdir();out.mkdir()
    r=dict(schema='mistwatch.nettle-official-cook.1',status='running',started_utc=datetime.now(timezone.utc).isoformat(),
           official_page=OFFICIAL,license_id='CC0-1.0',license_page='https://polyhaven.com/license',
           art_accepted=False,native_rendered=False,world_integrated=False,published=False,remote_jobs=[],files=[])
    try:
        metadata=download('https://api.polyhaven.com/files/nettle_plant',1024*1024)
        (H/'official-files.json').write_bytes(metadata);m=json.loads(metadata)
        d=m['gltf']['2k']['gltf'];entries=[(Path(d['url']).name,d)]
        entries.extend((Path(name).name,v) for name,v in d['include'].items())
        entries.append(('nettle_plant_alpha_2k.png',m['Alpha']['2k']['png']))
        need(len(entries)==6 and {n for n,_ in entries}==set(PINS),'Exact six official files required')
        for name,v in entries:
            need((v['md5'],v['size'])==PINS[name],'Official metadata identity changed')
            print('DOWNLOAD='+name,flush=True);b=download(v['url'],v['size'])
            need(len(b)==v['size'] and hashlib.md5(b).hexdigest()==v['md5'],'Official source identity mismatch')
            p=source/name;p.write_bytes(b);r['files'].append(dict(file=name,url=v['url'],official_md5=v['md5'],**row(p)))
        document=json.loads((source/'nettle_plant_2k.gltf').read_bytes())
        need(document['asset']['version']=='2.0' and len(document['nodes'])==len(document['meshes'])==6 and len(document['materials'])==1,'Six variant source layout')
        material=document['materials'][0]
        need(material['name']=='nettle_plant' and material['alphaMode']=='MASK' and material['alphaCutoff']==.5 and material['doubleSided'],'Original mask contract required')
        names=['nettle_plant_medium_a_LOD0','nettle_plant_medium_b_LOD0','nettle_plant_small_a_LOD0','nettle_plant_small_b_LOD0','nettle_plant_tall_a_LOD0','nettle_plant_tall_b_LOD0']
        need([n['name'] for n in document['nodes']]==names and document['scenes'][0]['nodes']==list(range(6)),'Variant correspondence')
        r['display_translations_removed']=[]
        for i,n in enumerate(document['nodes']):
            t=n.pop('translation',[0,0,0]);need(n['mesh']==i and set(n)=={'name','mesh'} and len(t)==3 and t[1:]==[0,0],'Only source display X translation may be removed')
            r['display_translations_removed'].append(t)
        need(len(document['buffers'])==1 and document['buffers'][0].pop('uri')=='nettle_plant.bin','One official binary')
        for image in document['images']:
            name=Path(image['uri']).name;need(name in PINS and (source/name).is_file(),'Official image reference')
            image['uri']=name
        for name in PINS:
            if name.endswith(('.jpg','.png')):(prepared/name).write_bytes((source/name).read_bytes())
        raw=json.dumps(document,separators=(',',':')).encode();raw+=b' '*((-len(raw))%4)
        binary=(source/'nettle_plant.bin').read_bytes();binary+=b'\0'*((-len(binary))%4)
        glb=prepared/'nettle-plant-rooted.glb'
        glb.write_bytes(struct.pack('<III',0x46546c67,2,28+len(raw)+len(binary))+struct.pack('<II',len(raw),0x4e4f534a)+raw+struct.pack('<II',len(binary),0x004e4942)+binary)
        converter=module('convert-glb-to-aemesh.py');baker=module('bake-photoreal-materials.py')
        need(row(TC)['sha256']==baker.DIRECTXTEX_SHA256,'Pinned DirectXTex required')
        print('COOK=exact-six-meshes',flush=True)
        mesh=converter.convert(glb,out/'meshes',out/'mesh-receipt.json','nettle-plant',0)
        need(mesh['outputCount']==6 and mesh['materialPrimitiveBijection'] and mesh['memoryGate'],'Six real mesh outputs')
        # 以原始 accessor 獨立核對每個打包頂點與三角形，不只接受轉換器自己的 pass。
        audit=[]
        for i,entry in enumerate(mesh['outputs']):
            p=document['meshes'][i]['primitives'];need(len(p)==1 and entry['nodeIndex']==entry['meshIndex']==i,'Single primitive per variant')
            p=p[0];a=p['attributes'];xyz,_,_=converter.decode_accessor(document,binary,a['POSITION'])
            normal,_,_=converter.decode_accessor(document,binary,a['NORMAL']);uv,_,_=converter.decode_accessor(document,binary,a['TEXCOORD_0'])
            ids,_,_=converter.decode_accessor(document,binary,p['indices']);ids=[v[0] for v in ids]
            b=(out/'meshes'/entry['file']).read_bytes();need(struct.unpack_from('<IIII',b)==(0x334D4541,1,len(xyz),len(ids)),'Exact native header')
            for k,(x,n,t) in enumerate(zip(xyz,normal,uv)):
                need(all(math.isfinite(v) for v in x+n+t) and 0<=t[0]<=1 and 0<=t[1]<=1,'Finite original mesh/UV')
                length=math.sqrt(sum(v*v for v in n));need(length>.1,'Original normal')
                packed=struct.pack('<fffIIff',x[0],x[1],-x[2],converter.pack_normal([v/length for v in n]),0xffffffff,t[0],t[1])
                need(b[16+28*k:16+28*(k+1)]==packed,'Original vertex/UV/normal not preserved')
            base=16+28*len(xyz)
            for k in range(0,len(ids),3):need(struct.unpack_from('<III',b,base+4*k)==(ids[k],ids[k+2],ids[k+1]),'Exact handedness winding')
            audit.append(dict(variant=i,source_name=names[i],vertices=len(xyz),triangles=len(ids)//3,
                              bounds_min=entry['boundsMinimumGltf'],bounds_max=entry['boundsMaximumGltf'],mesh=row(out/'meshes'/entry['file'])))
        sidecar=dict(schemaVersion=1,binding='runtime-sidecar',normalConvention='OpenGL',outputResolution=2048,expectedMipCount=12,
            materials=[dict(name='nettle',slot=0,baseColor='nettle_plant_diff_2k.jpg',normalGl='nettle_plant_nor_gl_2k.jpg',
                packedArm='nettle_plant_arm_2k.jpg',alphaMode='mask',alphaSource='separate',alphaMask='nettle_plant_alpha_2k.png',
                alphaCutoff=.5,alphaDilationPixels=8,doubleSided=True,materialClass='foliage')])
        dump(prepared/'nettle.materials.json',sidecar)
        files=[dict(path='prepared/'+p.name,bytes=p.stat().st_size,sha256=row(p)['sha256']) for p in prepared.iterdir() if p.suffix in ('.png','.jpg') or p.name.endswith('.materials.json')]
        manifest=dict(schemaVersion=1,memoryGate='pass',memoryGateMethod='Current process preflight and measured full baker tree',
            preflightPeakWorkingSetBytes=baker.peak_working_set(),assetCount=1,assets=[dict(assetName='nettle-plant',id='nettle_plant',
                officialPage=OFFICIAL,license='CC0-1.0',commercialUse=True,memoryGate='pass',files=files)])
        need(0<manifest['preflightPeakWorkingSetBytes']<baker.PEAK_WORKING_SET_LIMIT_BYTES,'Preflight memory budget')
        dump(H/'runtime-material-manifest.json',manifest);print('COOK=three-2K-full-mip-materials',flush=True)
        cooked=baker.cook(SimpleNamespace(asset_root=str(H),runtime_manifest=str(H/'runtime-material-manifest.json'),output=str(out/'materials'),texconv=str(TC),asset=['nettle-plant']))
        need(cooked['status']=='pass' and cooked['memoryGate']=='pass','Actual material bake failed')
        mats=cooked['assets'][0]['materials'];need(len(mats)==1 and mats[0]['maskCoverage']['source']==mats[0]['maskCoverage']['prepared'],'Exact alpha coverage preserved')
        need(mats[0]['normalChannelAudit']['maximumInversionError']==mats[0]['surfaceChannelAudit']['maximumPreservationError']==0,'Actual material channel mapping')
        r.update(status='official-six-variant-mesh-and-three-2K-DDS-cook-passed-native-integration-pending',mesh_audit=audit,material=cooked,
                 tool_inputs=[row(F/'tools'/name) for name in TOOL_PINS]+[row(TC),row(Path(__file__))],metadata=row(H/'official-files.json'))
    except Exception as e:r.update(status='first-divergence-preserved',first_divergence=repr(e))
    r['ended_utc']=datetime.now(timezone.utc).isoformat();dump(H/'prepare-receipt.json',r);print(r['status'],flush=True)
    return 0 if r['status'].startswith('official-six-variant') else 1

if __name__=='__main__':raise SystemExit(main())
