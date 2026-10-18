"""建立測試專用的單一煞停反向介入；不修改正式來源或歷史 expected-data。"""
from pathlib import Path
import argparse,hashlib,sys

HERE=Path(__file__).resolve().parent
SOURCE=HERE.parent.parent/'include'
CURRENT=(b'if(!explorer.PlaceFeet(controller.Position(),controller.Grounded(),crouched,terrain,boxes,hulls,&failureLine,&footPlacementDiagnostic,\n'
         b'                        ExplorerFootPlacementMode::Advance,input.movement.x==0&&input.movement.z==0&&!motion.dashing)){')
HISTORICAL=b'if(!explorer.PlaceFeet(controller.Position(),controller.Grounded(),crouched,terrain,boxes,hulls,&failureLine,&footPlacementDiagnostic)){'

def need(value,reason):
    if not value:raise RuntimeError(reason)

def ordinary_file(path):
    need(path.is_file()and not path.is_symlink()and not(path.stat().st_file_attributes&1024),
         'PHYSICAL_HEADER_REQUIRED:'+str(path))
    return path.read_bytes()

def inverse_player(original):
    forms=(CURRENT,CURRENT.replace(b'\n',b'\r\n'))
    counts=[original.count(form)for form in forms]
    need(sum(counts)==1,'ONE_REVIEWED_CURRENT_BRAKING_CALL_LF_OR_CRLF')
    current=forms[counts.index(1)]
    inverse=original.replace(current,HISTORICAL,1)
    need(inverse.count(HISTORICAL)==1 and inverse.replace(HISTORICAL,current,1)==original,
         'ENTIRE_PLAYER_RAW_FORWARD_INVERSE')
    return inverse

def main():
    parser=argparse.ArgumentParser()
    parser.add_argument('--binary-root',required=True);parser.add_argument('--output',required=True)
    args=parser.parse_args()
    need(sys.dont_write_bytecode and not sys.flags.optimize,'PYTHON_MINUS_B_WITHOUT_OPT_REQUIRED')
    binary=Path(args.binary_root).resolve();output=Path(args.output).resolve();source=SOURCE.resolve()
    need(output.is_relative_to(binary)and output!=binary and not source.is_relative_to(binary)and
         not binary.is_relative_to(source),'PRIVATE_BINARY_TREE_ONLY')
    headers={p.relative_to(source).as_posix():ordinary_file(p)for p in source.rglob('*.h')}
    need(headers and len({k.casefold()for k in headers})==len(headers),'UNIQUE_COMPLETE_QUOTE_CLOSURE')
    original=headers['MistwatchPlayerLocomotion.h']
    inverse=inverse_player(original)
    headers['MistwatchPlayerLocomotion.h']=inverse
    for directory in(output,*output.parents):
        if directory.exists():
            need(directory.is_dir()and not directory.is_symlink()and
                 not(directory.stat().st_file_attributes&1024),'NO_REPARSE_PRIVATE_DIRECTORY')
        if directory==binary:break
    if output.exists():
        need({p.relative_to(output).as_posix()for p in output.rglob('*')if p.is_file()}<=set(headers),
             'UNEXPECTED_PRIVATE_FILES_DO_NOT_DELETE')
    for relative,raw in headers.items():
        destination=output/relative;destination.parent.mkdir(parents=True,exist_ok=True)
        for directory in(destination.parent,*destination.parent.parents):
            need(not directory.is_symlink()and not(directory.stat().st_file_attributes&1024),
                 'NO_REPARSE_HEADER_PARENT')
            if directory==output:break
        if destination.exists():ordinary_file(destination)
        destination.write_bytes(raw)
        need(ordinary_file(destination)==raw,'PRIVATE_HEADER_RAW_COPY')
    print('HISTORICAL_BRAKING_HEADERS_PREPARED headers='+str(len(headers))+
          ' single_player_region=1 source_player_sha256='+hashlib.sha256(original).hexdigest().upper()+
          ' private_player_sha256='+hashlib.sha256(inverse).hexdigest().upper(),flush=True)

if __name__=='__main__':
    try:main()
    except Exception as error:
        print('HISTORICAL_BRAKING_HEADERS_FIRST_DIVERGENCE '+str(error),file=sys.stderr,flush=True)
        raise SystemExit(1)
