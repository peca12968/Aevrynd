"""分離凍結歷史對照與當前單一足部介入；不修改正式來源或歷史 expected-data。"""
from pathlib import Path
import argparse,hashlib,sys

HERE=Path(__file__).resolve().parent
SOURCE=HERE.parent.parent/'include'
CURRENT=(b'if(!explorer.PlaceFeet(controller.Position(),controller.Grounded(),crouched,terrain,boxes,hulls,&failureLine,&footPlacementDiagnostic,\n'
         b'                        ExplorerFootPlacementMode::Advance,input.movement.x==0&&input.movement.z==0&&!motion.dashing)){')
HISTORICAL=b'if(!explorer.PlaceFeet(controller.Position(),controller.Grounded(),crouched,terrain,boxes,hulls,&failureLine,&footPlacementDiagnostic)){'
MOTOR_REGIONS=(
    (b'    static constexpr float FixedStep=1.f/120.f,Acceleration=12.f,Deceleration=18.f,CrouchDeceleration=30.f;\n',
     b'    static constexpr float FixedStep=1.f/120.f,Acceleration=12.f,Deceleration=18.f;\n'),
    ('            // 接地蹲行較快收速；空中及一般行走沿用原有慣性，不瞬移或硬夾速度。\n'
     '            const float rate=Length(desired)<Length(next.velocity_)?\n'
     '                (input.crouched&&candidate.Grounded()?CrouchDeceleration:Deceleration):Acceleration;\n'.encode('utf-8'),
     b'            const float rate=Length(desired)<Length(next.velocity_)?Deceleration:Acceleration;\n'))
HISTORICAL_MOTOR_LF_SHA256='72942C100BE5A384E589B8888275AEF158C62A399C91DCE46C1DDAC405EB1825'

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

def inverse_motor(original):
    inverse=original;edits=[]
    for index,(current,historical)in enumerate(MOTOR_REGIONS):
        forms=(current,current.replace(b'\n',b'\r\n'))
        counts=[inverse.count(form)for form in forms]
        need(sum(counts)==1,'ONE_REVIEWED_MOTOR_REGION_LF_OR_CRLF:'+str(index))
        variant=counts.index(1);current=forms[variant]
        if variant:historical=historical.replace(b'\n',b'\r\n')
        need(inverse.count(historical)==0,'HISTORICAL_MOTOR_REGION_ALREADY_PRESENT:'+str(index))
        inverse=inverse.replace(current,historical,1)
        need(inverse.count(historical)==1,'UNIQUE_INVERSE_MOTOR_REGION:'+str(index))
        edits.append((historical,current))
    forward=inverse
    for historical,current in reversed(edits):
        need(forward.count(historical)==1,'UNIQUE_FORWARD_MOTOR_REGION')
        forward=forward.replace(historical,current,1)
    need(forward==original,'ENTIRE_MOTOR_RAW_FORWARD_INVERSE')
    # 只接受已封存的舊18馬達；LF/CRLF選擇不容許其他程式變動混入歷史golden。
    need(hashlib.sha256(inverse.replace(b'\r\n',b'\n')).hexdigest().upper()==HISTORICAL_MOTOR_LF_SHA256,
         'FROZEN_HISTORICAL_MOTOR_FULL_CONTENT')
    return inverse

def source_headers(source):
    for path in(source,*source.rglob('*')):
        need(not path.is_symlink()and not(path.stat().st_file_attributes&1024),
             'NO_REPARSE_SOURCE_CLOSURE:'+str(path))
    headers={p.relative_to(source).as_posix():ordinary_file(p)for p in source.rglob('*.h')}
    need(headers and len({k.casefold()for k in headers})==len(headers),'UNIQUE_COMPLETE_QUOTE_CLOSURE')
    return headers

def copy_headers(output,binary,headers):
    for directory in(output,*output.parents):
        if directory.exists():
            need(directory.is_dir()and not directory.is_symlink()and
                 not(directory.stat().st_file_attributes&1024),'NO_REPARSE_PRIVATE_DIRECTORY')
        if directory==binary:break
    if output.exists():
        for path in output.rglob('*'):
            need(not path.is_symlink()and not(path.stat().st_file_attributes&1024),
                 'NO_REPARSE_EXISTING_PRIVATE_CLOSURE')
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

def main():
    parser=argparse.ArgumentParser()
    parser.add_argument('--binary-root',required=True);parser.add_argument('--output',required=True)
    parser.add_argument('--current-output',required=True)
    args=parser.parse_args()
    need(sys.dont_write_bytecode and not sys.flags.optimize,'PYTHON_MINUS_B_WITHOUT_OPT_REQUIRED')
    binary=Path(args.binary_root).resolve();output=Path(args.output).resolve()
    current_output=Path(args.current_output).resolve();source=SOURCE.resolve()
    need(all(p.is_relative_to(binary)and p!=binary for p in(output,current_output))and
         not source.is_relative_to(binary)and not binary.is_relative_to(source)and
         not output.is_relative_to(current_output)and not current_output.is_relative_to(output),
         'DISTINCT_PRIVATE_BINARY_TREES_ONLY')
    headers=source_headers(source)
    original=headers['MistwatchPlayerLocomotion.h']
    inverse=inverse_player(original)
    current_headers=dict(headers);current_headers['MistwatchPlayerLocomotion.h']=inverse
    historical_headers=dict(current_headers)
    motor=headers['MistwatchLocomotionMotor.h'];historical_motor=inverse_motor(motor)
    historical_headers['MistwatchLocomotionMotor.h']=historical_motor
    need({k for k in headers if headers[k]!=current_headers[k]}=={'MistwatchPlayerLocomotion.h'}and
         {k for k in headers if headers[k]!=historical_headers[k]}==
         {'MistwatchPlayerLocomotion.h','MistwatchLocomotionMotor.h'},'EXACT_PRIVATE_INTERVENTIONS')
    copy_headers(output,binary,historical_headers)
    copy_headers(current_output,binary,current_headers)
    need(source_headers(source)==headers,'SOURCE_QUOTE_CLOSURE_UNCHANGED')
    print('HISTORICAL_BRAKING_HEADERS_PREPARED headers='+str(len(headers))+
          ' single_player_region=1 motor_regions=2 source_player_sha256='+hashlib.sha256(original).hexdigest().upper()+
          ' private_player_sha256='+hashlib.sha256(inverse).hexdigest().upper()+
          ' source_motor_sha256='+hashlib.sha256(motor).hexdigest().upper()+
          ' historical_motor_sha256='+hashlib.sha256(historical_motor).hexdigest().upper(),flush=True)
    print('CURRENT_BRAKING_CONTROL_HEADERS_PREPARED headers='+str(len(headers))+
          ' single_player_region=1 motor_raw_equal=1',flush=True)

if __name__=='__main__':
    try:main()
    except Exception as error:
        print('HISTORICAL_BRAKING_HEADERS_FIRST_DIVERGENCE '+str(error),file=sys.stderr,flush=True)
        raise SystemExit(1)
