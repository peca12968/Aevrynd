"""獨立 append GPU witness；只消費 root 真 fresh qualification，不建置、不產生假資格。"""
import argparse
import datetime
import hashlib
import json
import os
from pathlib import Path
import subprocess
import time
import xml.etree.ElementTree as ET

def need(value,message):
    if not value:raise RuntimeError(message)
def read(path):return json.loads(Path(path).read_text(encoding='utf-8-sig'))
def sha(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest().upper()
def pin(path):return dict(path=str(path),bytes=Path(path).stat().st_size,sha256=sha(path))
def check(row):
    path=Path(row['path']);need(path.is_file() and path.stat().st_size==row['bytes'] and sha(path)==row['sha256'].upper(),'raw qualification differs: '+str(path))
def write(path,value):
    with path.open('x',encoding='utf-8') as stream:json.dump(value,stream,ensure_ascii=False,indent=2)
def utc():return datetime.datetime.now(datetime.timezone.utc).isoformat()
def junit(row,count):
    check(row);root=ET.parse(row['path']).getroot();cases=root.findall('.//testcase')
    need(len(cases)==count and all(c.get('status')=='run' for c in cases) and not root.findall('.//failure') and not root.findall('.//error') and not root.findall('.//skipped'),'fresh actual JUnit failed or count differs')
    return cases
def qualify(binding):
    need(binding['schema']=='uv-normal-core-root-binding-1','binding schema differs')
    check(binding['history_manifest'])
    need(binding['history_manifest']['sha256'].upper()=='9A0074BF6E1C400CF3EAA2459E2C3F45F1C12876C236E908BAFDD4DD9E0E22CD','original ordered gate reference differs')
    history=read(binding['history_manifest']['path'])
    for row in binding['pins']:check(row)
    for key in ['source_manifest','fresh_full_receipt','original_exe','candidate_exe','candidate_engine','fixture_source']:
        check(binding[key])
    source=Path(binding['source_root']);files=read(binding['source_manifest']['path'])['files']
    need(len({r['path'] for r in files})==len(files),'source path duplicate')
    for row in files:check(dict(row,path=str(source/row['path'])))
    need(Path(binding['fixture_source']['path'])==source/'engine/native/tests/UvNormalNativeRegression.cpp','candidate fixture source is not fresh core source')
    need(binding['fixture_source']['sha256'].upper()=='79EFA29CAD7CD0787C7A172BC482407E94D1A4CDEE46435A03017CC6296FEC38','proven fixture source changed')
    need(binding['original_exe']['sha256'].upper()=='F509FCD46808FC18C4057ACECA768820C788D84A1DFFEB2E5B3FC8021B14D0EF','actual qualified original fixture differs')
    need(Path(binding['candidate_exe']['path']).is_relative_to(Path(binding['build_root'])) and Path(binding['candidate_engine']['path']).is_relative_to(Path(binding['build_root'])),'candidate not in fresh build')
    receipt=read(binding['fresh_full_receipt']['path']);processes=receipt['processes']
    need(processes and all(p['status']=='terminal' and p['exit_code']==0 and p['pid']>0 for p in processes),'fresh actual process not passed')
    need(any('ALL_BUILD' in str(p.get('arguments',p.get('command',[]))) for p in processes),'actual fresh ALL_BUILD missing')
    for p in processes:
        if 'logs' in p:
            for row in p['logs']:check(row)
        else:check(p['stdout']);check(p['stderr'])
    cpu=junit(binding['cpu76_junit'],76)
    need([c.get('name') for c in cpu]==history['cpu_names'],'original ordered CPU76 differs')
    math=junit(binding['math_junit'],1)
    need(math[0].get('name')=='AevryndUvNormalSignedJacobianMathTest','new math case differs')
    need('UV_NORMAL_MATH_PASS checks=40334 legacy_negative_failures=2304 positive_bit_identical=3456 degenerate_fallbacks=7 provided_tangent_identical=13824' in ''.join(math[0].itertext()),'new actual math oracle differs')
    for mode,key,receipt_key in [('normal','normal32_junit','normal32_receipt'),('gbv','gbv17_junit','gbv17_receipt')]:
        entry=history['matrices'][mode];cases=junit(binding[key],1)
        need([c.get('name') for c in cases]==[entry['test']['name']],'original GPU CTest wrapper differs')
        check(binding[receipt_key]);gpu=read(binding[receipt_key]['path'])
        need(gpu['status']=='passed' and gpu['mode']==mode and gpu['cases']==len(entry['cases']) and gpu['case_names']==entry['cases'],'original ordered actual GPU case receipt differs')
    need(len(binding['assets'])==3 and {Path(r['path']).name for r in binding['assets']}=={'base.bmp','rough.bmp','normal.dds'},'constant runtime membership differs')
    expected={'base.bmp':(1078,'DAB3A66FE4E9F343B03A8DFD1BBAADB983C5E638B0A322951991BF9D9D4BE4D1'),'rough.bmp':(1078,'77571CD64F43BC92C988BEA9DA120D245F15855D1F5EDBB7BCAE9CF763F0D3EF'),'normal.dds':(516,'D7F2B0213380B91E5519FAF9A2A160AE243BE67B38DC3E9BD6F76AE5C7F760C0')}
    need(len({str(Path(row['path']).parent) for row in binding['assets']})==1,'runtime must share one directory')
    for row in binding['assets']:
        check(row);need((row['bytes'],row['sha256'].upper())==expected[Path(row['path']).name],'proven constant runtime changed')
    return next(row for row in binding['assets'] if Path(row['path']).name=='normal.dds')
def process(output,name,exe,assets,sign,mode,normal_sha):
    folder=output/'captures'/name;need(not folder.exists(),'case output already exists')
    env={k:v for k,v in os.environ.items() if not k.startswith('AEVRYND_')};env['AEVRYND_GPU_VALIDATION']='1' if mode=='gbv' else '0';env['LOCALAPPDATA']=str(output/'local-appdata')
    command=[str(exe),str(folder),str(assets),str(sign),mode,normal_sha.lower()]
    startup=subprocess.STARTUPINFO();startup.dwFlags|=subprocess.STARTF_USESHOWWINDOW;startup.wShowWindow=0
    with (output/(name+'.stdout')).open('xb') as stdout,(output/(name+'.stderr')).open('xb') as stderr:
        child=subprocess.Popen(command,cwd=output,env=env,stdout=stdout,stderr=stderr,startupinfo=startup,creationflags=subprocess.CREATE_NO_WINDOW)
        row=dict(name=name,pid=child.pid,command=command,cwd=str(output),started_utc=utc(),status='live');write(output/(name+'.started.json'),row)
        while child.poll() is None:print(json.dumps(dict(pid=child.pid,name=name,status='live')),flush=True);time.sleep(5)
        row.update(status='terminal',exit_code=child.returncode,finished_utc=utc())
    row['logs']=[pin(output/(name+'.stdout')),pin(output/(name+'.stderr'))];write(output/(name+'.process.json'),row)
    need(row['exit_code']==0,'actual native process failed: '+name)
    return folder,row
def run(binding_path,output):
    binding=read(binding_path);normal=qualify(binding)
    need(not output.exists(),'fresh once-only output already exists');output.mkdir(parents=True)
    write(output/'started.json',dict(pid=os.getpid(),utc=utc(),binding=pin(binding_path),finite_cases=8))
    actual=[];pairs=[]
    try:
        for mode in ['normal','gbv']:
            for sign in [1,-1]:
                receipts={};folders={}
                for variant in ['original','candidate']:
                    name=f'{mode}-{sign:+d}-{variant}';folder,row=process(output,name,Path(binding[variant+'_exe']['path']),Path(normal['path']).parent,sign,mode,normal['sha256']);actual.append(row);folders[variant]=folder;receipts[variant]=read(folder/'receipt.json')
                    r=receipts[variant];need(r['gpu_flags']==[mode=='gbv']*5 and r['debug_errors']==r['debug_warnings']==0,'actual debug/GBV differs');need(r['resident_before']==[2,1] and r['resident_after']==[0,0],'actual cleanup differs')
                original=receipts['original'];candidate=receipts['candidate'];need(original.keys()==candidate.keys(),'receipt membership differs')
                differences={key:[original[key],candidate[key]] for key in original if original[key]!=candidate[key]};need(set(differences)<= {'means'},'unexpected whole receipt difference')
                for name in ['object-id.u32','depth.f32','standard.roi.rgba']:need((folders['original']/name).read_bytes()==(folders['candidate']/name).read_bytes(),'public raw reference differs: '+name)
                standard,mirror=candidate['means'];old_standard,old_mirror=original['means'];correct_std,correct_mirror,legacy_mirror=candidate['cpu_lambert']
                need(sign*(correct_std-correct_mirror)>.6 and correct_std-legacy_mirror>.1,'CPU signed premise differs')
                need(old_standard-old_mirror>0 and abs(old_standard-old_mirror)<12,'original negative oracle differs; never retune')
                need(sign*(standard-mirror)>12 and sign*(old_mirror-mirror)>12,'candidate signed PNG response differs')
                pairs.append(dict(mode=mode,light_sign=sign,differences=differences,outputs={v:[pin(p) for p in sorted(folder.iterdir()) if p.is_file()] for v,folder in folders.items()}))
        write(output/'receipt.json',dict(status='fresh-core-appended-uv-normal-native-passed',binding=pin(binding_path),processes=actual,pairs=pairs,ground_grain_solved=False,hdr_hook_used=False))
    except Exception as error:write(output/'first-failure.json',dict(utc=utc(),error=str(error),processes=actual));raise

if __name__=='__main__':
    parser=argparse.ArgumentParser();parser.add_argument('--binding',type=Path,required=True);parser.add_argument('--output',type=Path,required=True);args=parser.parse_args();run(args.binding.resolve(),args.output.resolve())
