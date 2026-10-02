"""Reproduce Fern02 cooking from hashed original CC0 files; never overwrite a cook."""
import argparse
import hashlib
import importlib.util
import json
import os
import pathlib
import struct
from types import SimpleNamespace


def load_tool(name, filename):
    spec=importlib.util.spec_from_file_location(name,pathlib.Path(__file__).with_name(filename))
    module=importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def cook(asset_root, output, texconv):
    if output.exists():
        raise ValueError('Preserve existing cook directory')
    source=asset_root/'source'
    download=json.loads((asset_root/'download-receipt.json').read_text(encoding='utf-8-sig'))
    if download['source']!='https://polyhaven.com/a/fern_02' or download['license_id']!='CC0-1.0' or len(download['files'])!=6:
        raise ValueError('Fern provenance mismatch')
    for entry in download['files']:
        if pathlib.PurePosixPath(entry['file']).name!=entry['file']:
            raise ValueError('Unsafe source path')
        path=source/entry['file']
        if path.stat().st_size!=entry['size'] or sha(path).upper()!=entry['sha256']:
            raise ValueError('Changed official source: '+entry['file'])
    document=json.loads((source/'fern_02_2k.gltf').read_text())
    if len(document['nodes'])!=4 or len(document['meshes'])!=4 or len(document['materials'])!=1:
        raise ValueError('Fern variant table changed')
    material=document['materials'][0]
    if material['alphaMode']!='MASK' or material['alphaCutoff']!=.5 or not material['doubleSided']:
        raise ValueError('Fern alpha contract changed')
    document['buffers'][0].pop('uri')
    offsets=[]
    for node in document['nodes']:
        offsets.append({'name':node['name'],'displayTranslationRemoved':node.pop('translation',[0,0,0])})
    prepared=output/'prepared'
    prepared.mkdir(parents=True)
    for image in document['images']:
        name=pathlib.PurePosixPath(image['uri']).stem+'.png'
        if not (source/name).is_file(): raise ValueError('Unresolved material image')
        image['uri']=name
    for name in ('fern_02_diff_2k.png','fern_02_nor_gl_2k.png','fern_02_arm_2k.png','fern_02_alpha_2k.png'):
        (prepared/name).write_bytes((source/name).read_bytes())
    encoded=json.dumps(document,separators=(',',':')).encode()
    encoded+=b' '*((-len(encoded))%4)
    binary=(source/'fern_02.bin').read_bytes()
    binary+=b'\0'*((-len(binary))%4)
    glb=prepared/'fern-02-rooted.glb'
    glb.write_bytes(struct.pack('<III',0x46546c67,2,28+len(encoded)+len(binary))+
                   struct.pack('<II',len(encoded),0x4e4f534a)+encoded+
                   struct.pack('<II',len(binary),0x004e4942)+binary)
    converter=load_tool('fern_mesh_converter','convert-glb-to-aemesh.py')
    mesh=converter.convert(glb,output/'meshes',output/'mesh-receipt.json','fern-02',0)
    if [entry['triangles'] for entry in mesh['outputs']]!=[2384,2248,784,816]:
        raise ValueError('Fern topology changed')
    sidecar={'schemaVersion':1,'binding':'runtime-sidecar','normalConvention':'OpenGL',
             'outputResolution':2048,'expectedMipCount':12,'materials':[
                 {'name':'fern','slot':0,'baseColor':'fern_02_diff_2k.png','normalGl':'fern_02_nor_gl_2k.png',
                  'packedArm':'fern_02_arm_2k.png','alphaMode':'mask','alphaSource':'separate',
                  'alphaMask':'fern_02_alpha_2k.png','alphaCutoff':.5,'alphaDilationPixels':8,
                  'doubleSided':True,'materialClass':'foliage'}]}
    (prepared/'fern.materials.json').write_text(json.dumps(sidecar,indent=2)+'\n',encoding='utf-8')
    files=[{'path':'prepared/'+path.name,'bytes':path.stat().st_size,'sha256':sha(path)} for path in prepared.iterdir()
           if path.suffix=='.png' or path.name.endswith('.materials.json')]
    baker=load_tool('fern_material_baker','bake-photoreal-materials.py')
    preflight_peak=baker.peak_working_set()
    if os.name=='nt' and (not preflight_peak or preflight_peak>=baker.PEAK_WORKING_SET_LIMIT_BYTES):
        raise ValueError('Fern preflight working set gate failed')
    manifest={'schemaVersion':1,'memoryGate':'pass',
        'memoryGateMethod':'Preflight current Python peak; complete baker and texconv peak checked after cooking',
        'preflightPeakWorkingSetBytes':preflight_peak,'assetCount':1,'assets':[
        {'assetName':'fern-02','id':'fern_02','officialPage':download['source'],'license':'CC0-1.0',
         'commercialUse':True,'memoryGate':'pass','files':files}]}
    manifest_path=output/'runtime-material-manifest.json'
    manifest_path.write_text(json.dumps(manifest,indent=2)+'\n',encoding='utf-8')
    cooked=baker.cook(SimpleNamespace(asset_root=str(output),runtime_manifest=str(manifest_path),
        output=str(output/'materials'),texconv=str(texconv),asset=['fern-02']))
    dds_bytes=sum(path.stat().st_size for path in (output/'materials').rglob('*.dds'))
    if dds_bytes!=3*5592580:
        raise ValueError('Fern complete DDS payload budget changed')
    receipt={'source':'https://polyhaven.com/a/fern_02','license':'CC0-1.0',
             'nodes':offsets,'scriptSha256':sha(pathlib.Path(__file__)),
             'meshConverterSha256':sha(pathlib.Path(converter.__file__)),
             'materialBakerSha256':sha(pathlib.Path(baker.__file__)),
             'ddsBytes':dds_bytes,'mesh':mesh,'material':cooked}
    (output/'fern-cook-receipt.json').write_text(json.dumps(receipt,indent=2)+'\n',encoding='utf-8')
    return receipt


if __name__=='__main__':
    parser=argparse.ArgumentParser()
    parser.add_argument('--asset-root',required=True,type=pathlib.Path)
    parser.add_argument('--output',required=True,type=pathlib.Path)
    parser.add_argument('--texconv',required=True,type=pathlib.Path)
    args=parser.parse_args()
    result=cook(args.asset_root.resolve(),args.output.resolve(),args.texconv.resolve())
    print('FERN_COOK_PASS; VARIANTS=4; TRIANGLES='+str(result['mesh']['totalTriangles']))
