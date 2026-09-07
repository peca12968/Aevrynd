"""將角色 GLB 的稀疏存取器展開，保存節點、材質及變形目標。"""
import argparse
import base64
import copy
import hashlib
import importlib.util
import json
from pathlib import Path
import struct

SPEC = importlib.util.spec_from_file_location('aevrynd_glb_decoder', Path(__file__).with_name('convert-glb-to-aemesh.py'))
DECODER = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(DECODER)

def normalize_document(document, binary):
    declared = document['buffers'][0]['byteLength']
    if not isinstance(declared,int) or isinstance(declared,bool) or declared < 0 or declared > len(binary):
        raise ValueError('Invalid declared buffer length')
    # 解碼只允許存取宣告長度，不把 GLB 對齊填充當成合法資料。
    binary = binary[:declared]
    output = copy.deepcopy(document)
    packed = bytearray(binary)
    for index, accessor in enumerate(document.get('accessors', [])):
        kind, component = accessor['type'], accessor['componentType']
        # 既有解碼器不處理短整數矩陣的欄對齊，此情況明確拒絕。
        if kind in ('MAT2','MAT3') and DECODER.FORMATS[component][1] < 4:
            raise ValueError('Padded integer matrix accessor unsupported')
        semantic, _, _ = DECODER.decode_accessor(document, binary, index)
        raw_document = dict(document)
        raw_document['accessors'] = list(document['accessors'])
        raw_document['accessors'][index] = dict(accessor, normalized=False)
        raw, _, _ = DECODER.decode_accessor(raw_document, binary, index)
        while len(packed) % 4:
            packed.append(0)
        start = len(packed)
        code = '<'+DECODER.FORMATS[component][0]*DECODER.COMPONENTS[kind]
        for row in raw:
            packed.extend(struct.pack(code,*row))
        view = {'buffer':0,'byteOffset':start,'byteLength':len(packed)-start}
        output.setdefault('bufferViews',[]).append(view)
        dense = output['accessors'][index]
        dense.pop('sparse',None)
        dense['bufferView']=len(output['bufferViews'])-1
        dense['byteOffset']=0
        actual, _, _ = DECODER.decode_accessor(output,packed,index)
        if actual != semantic:
            raise ValueError(f'Accessor semantic mismatch: {index}')
    output['buffers'][0]['byteLength']=len(packed)
    return output,bytes(packed)

def normalize(source, destination):
    document,binary=DECODER.read_glb(source)
    output,packed=normalize_document(document,binary)
    # 轉換目錄後仍保留外部圖片的內容；嵌入圖片 bufferView 不變。
    for image in output.get('images',[]):
        uri=image.get('uri','')
        if uri and not uri.startswith('data:'):
            image_bytes=(source.parent/uri).read_bytes()
            mime=image.get('mimeType') or {'.png':'image/png','.jpg':'image/jpeg','.jpeg':'image/jpeg'}.get(Path(uri).suffix.lower())
            if not mime:
                raise ValueError('External image MIME type unknown')
            image['uri']='data:'+mime+';base64,'+base64.b64encode(image_bytes).decode('ascii')
    destination.mkdir(parents=True,exist_ok=False)
    output['buffers'][0]['uri']='character-dense.bin'
    (destination/'character-dense.bin').write_bytes(packed)
    (destination/'character-dense.gltf').write_text(json.dumps(output,separators=(',',':')),encoding='utf-8')
    receipt={'sourceSha256':hashlib.sha256(source.read_bytes()).hexdigest(),
             'accessorsVerified':len(document.get('accessors',[])),
             'sparseAccessorsExpanded':sum('sparse' in a for a in document.get('accessors',[])),
             'status':'ALL_ACCESSORS_NUMERICALLY_EQUAL','runtimeIntegrationVerified':False}
    (destination/'receipt.json').write_text(json.dumps(receipt,indent=2),encoding='utf-8')
    print(json.dumps(receipt))

def self_test():
    document={'buffers':[{'byteLength':8}], 'bufferViews':[{'buffer':0,'byteOffset':0,'byteLength':1},{'buffer':0,'byteOffset':4,'byteLength':4}],
              'accessors':[{'componentType':5126,'count':3,'type':'SCALAR','sparse':{'count':1,'indices':{'bufferView':0,'componentType':5121},'values':{'bufferView':1}}}]}
    binary=bytes([1,0,0,0])+struct.pack('<f',7)
    out,data=normalize_document(document,binary)
    assert DECODER.decode_accessor(out,data,0)[0]==[[0],[7],[0]]
    for broken,blob in [(document,bytes([3])+binary[1:]),(document,binary[:-1])]:
        try:
            normalize_document(broken,blob)
        except (ValueError,DECODER.ConversionError):
            continue
        raise AssertionError('Invalid sparse or length accepted')
    print('INVALID_SPARSE_AND_LENGTH_REJECTED')

if __name__=='__main__':
    parser=argparse.ArgumentParser()
    parser.add_argument('--source',type=Path)
    parser.add_argument('--output',type=Path)
    parser.add_argument('--self-test',action='store_true')
    args=parser.parse_args()
    if args.self_test:
        self_test()
    if args.source and args.output:
        normalize(args.source,args.output)
    elif not args.self_test:
        parser.error('--source and --output required')
