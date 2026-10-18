"""Run the unchanged real C++ cases and retain their original raw-byte/readback gates."""
from pathlib import Path
import json,os,re,subprocess,sys,tempfile
from MistwatchPlayerDynamicContactReadback import ValidatePlayerMatrix,need,row
from MistwatchPlayerHeadContactSet import ValidateHeadContactSet

HERE=Path(__file__).resolve().parent
FIXTURES=HERE.parent/'fixtures/dynamic-contact'
TAPE_FIELDS=('recipe','tick','accepted','dashing','lowCeiling','steps','CC.StateHash',
    'Player.StateHash','Motor.StateHash','EvasionHash','PoseHash','stats.steps','stats.collisions',
    'stats.jumps','stats.landings','stats.terrainContacts','DirectLastTick','retainedCapacityBytes',
    'PendingEvadeTicks','RemainderBits')

def tape(path):
    raw=path.read_bytes();lines=raw.splitlines()
    need(len(lines)==1440,'EXACT1440_PARITY_ROWS:'+str(path))
    for index,line in enumerate(lines):
        values=line.split(b'\t')
        need(len(values)==20 and all(re.fullmatch(rb'[0-9]+',v)and int(v)<2**64 for v in values),
             'EXACT20_UNSIGNED_PARITY_FIELDS:'+str(index))
        need((int(values[0]),int(values[1]),int(values[2]))==(index//120,index%120,1),
             'EXACT_PARITY_RECIPE_TICK_ACCEPTANCE:'+str(index))
    return raw

def compare_tapes(left,right,work,label,expected_equal=True):
    a,b=tape(left),tape(right);equal=a==b
    report=dict(left=row(left),right=row(right),rows=1440,fields=20,raw_equal=equal,
        expected_equal=expected_equal,PoseHash_preserved=True)
    for index,(x,y)in enumerate(zip(a.splitlines(),b.splitlines())):
        if x!=y:
            report['first_difference']=dict(line=index+1,recipe=index//120,tick=index%120,
                left_raw=x.decode('ascii'),right_raw=y.decode('ascii'),
                fields=[dict(column=i,name=TAPE_FIELDS[i],left=u.decode('ascii'),right=v.decode('ascii'))
                    for i,(u,v)in enumerate(zip(x.split(b'\t'),y.split(b'\t')))if u!=v])
            break
    with(work/(label+'.readback.json')).open('x',encoding='utf-8')as stream:
        json.dump(report,stream,indent=2);stream.write('\n')
    need(equal==expected_equal,'FULL1440_RAW_PARITY_CONTRACT:'+label)

def ordinary(executable,work,name):
    need(executable.is_file(),'REAL_ORDINARY_EXECUTABLE_MISSING')
    actual,folder=run(executable,[],work,name)
    need(Path(actual['stdout']['path']).read_bytes().splitlines()==
         [b'ORIGINAL_F_GAMEPLAY_EMPTY_PARITY snapshots=1440'],'EXACT_ORDINARY1440_MARKER')
    tape(folder/'parity.tsv')
    return folder

def run(executable,args,work,name,accepted=(0,)):
    folder=work/name;folder.mkdir()
    output=folder/'stdout.txt';error=folder/'stderr.txt'
    command=[str(executable),*args]
    with output.open('xb')as stdout,error.open('xb')as stderr:
        completed=subprocess.run(command,cwd=folder,stdin=subprocess.DEVNULL,stdout=stdout,stderr=stderr,
            check=False,creationflags=subprocess.CREATE_NO_WINDOW if os.name=='nt'else 0)
    result=dict(command=command,exit_code=completed.returncode,stdout=row(output),stderr=row(error))
    need(completed.returncode in accepted and error.read_bytes()==b'','REAL_CPP_CASE_FAILED:'+name)
    return result,folder

def main():
    need(sys.dont_write_bytecode and not sys.flags.optimize,'MINUS_B_WITHOUT_OPT_REQUIRED')
    need(len(sys.argv)>=2 and sys.argv[1]in('player','pair','pair-history'),'EXPECTED_EXPLICIT_DYNAMIC_CONTACT_MODE')
    mode=sys.argv[1];need(len(sys.argv)=={'player':3,'pair':5,'pair-history':4}[mode],'EXACT_ARGUMENT_COUNT')
    executable=Path(sys.argv[2]).resolve();need(executable.is_file(),'REAL_CPP_EXECUTABLE_MISSING')
    # 只在 CTest 指定的 build working directory 建立新資料夾；保留失敗原物，不覆寫前輪。
    work=Path(tempfile.mkdtemp(prefix='MistwatchDynamicContact-'+mode+'-',dir=Path.cwd()))
    print('DYNAMIC_CONTACT_CASE_OUTPUT='+str(work),flush=True)
    if mode=='player':
        empty,_=run(executable,['--empty'],work,'empty36')
        raw=Path(empty['stdout']['path']).read_bytes();lines=raw.splitlines(keepends=True)
        tape=b''.join(line for line in lines if line.startswith(b'EMPTY_WORD_TAPE tick='))
        need(len(tape.splitlines())==36 and raw.count(b'EMPTY_DYNAMIC_COMPATIBILITY_TAPE_COMPLETE rows=36')==1,
             'EXACT_EMPTY_WORD_TAPE_MISSING')
        need(tape==(FIXTURES/'original-empty36.golden').read_bytes(),'ORIGINAL_EMPTY36_FULL_RAW_BYTE_PARITY_CHANGED')
        # 原讀回允許完整矩陣先以1回報 property RED，再由原 reader 保留第一個具體分歧。
        actual,folder=run(executable,['--matrix'],work,'head56',(0,1))
        fixed=json.loads((FIXTURES/'player-head-fixed-cases.json').read_bytes())
        readback=ValidatePlayerMatrix(actual,fixed,folder)
        # 本輪核可的額外回歸 gate；原 reader、位置、輸入及物理案例完全保留。
        expected=json.loads((FIXTURES/'player-head-contact-set.json').read_bytes())
        ValidateHeadContactSet(readback['case_actuals'],expected)
        print('FORMAL_PLAYER_DYNAMIC_CONTACT_CASES_COMPLETE cases=56 ticks=2128 empty36_raw_equal=1 main=0 native=0 GPU=0')
    elif mode=='pair':
        asset=Path(sys.argv[3]).resolve();need(asset.is_file(),'REAL_RAW_BOAR_ASSET_MISSING_NO_FALLBACK')
        baseline=Path(sys.argv[4]).resolve();need(baseline!=executable,'INDEPENDENT_CURRENT_ORDINARY_EXECUTABLE')
        ordinary_folder=ordinary(baseline,work,'ordinary-current')
        actual,folder=run(executable,[str(asset)],work,'pair')
        text=Path(actual['stdout']['path']).read_text(encoding='ascii')
        need('SEQUENTIAL_PAIR_MINIMAL_CPU_PASS legacy_ticks=660 npc_approaches=8 early_falls=2'in text,'REQUIRED_CPU_MARKER_MISSING')
        need('GAMEPLAY_DYNAMIC_CONTACT_CANDIDATE_CPU_PASS parity=1440 approaches=24 atomic=7 retry_edges=4 other=6'in text,
             'OPTIONAL_STEP_GAMEPLAY_FULL_REQUALIFICATION_MISSING')
        need('PAIR_SHARED_INPUT_GUARD_CPU_PASS profiles=10 negatives=270 positives=30'in text,'EXACT_INPUT_GUARD_COVERAGE_MISSING')
        compare_tapes(ordinary_folder/'parity.tsv',folder/'parity.tsv',work,'current-ordinary-vs-empty')
        need((folder/'pair-segments.tsv').is_file(),'ACTUAL_PAIR_SEGMENT_OUTPUT_MISSING')
        print('FORMAL_PAIR_CONTACT_CASES_COMPLETE current_ordinary1440_raw_equal=1 main=0 native=0 camera=0 save=0 GPU=0')
    else:
        historical=Path(sys.argv[3]).resolve();need(historical!=executable,'INDEPENDENT_HISTORICAL_EXECUTABLE')
        current_folder=ordinary(executable,work,'ordinary-current')
        historical_folder=ordinary(historical,work,'ordinary-no-braking')
        golden=FIXTURES/'original-pair1440.golden';expected=row(golden)
        need(expected['bytes']==175372 and expected['sha256']=='ECDBEF8BDCE405DD710D81DD175D634CF23E0D792EFF0842F63528CD109DEF76',
             'IMMUTABLE_HISTORICAL1440_GOLDEN')
        compare_tapes(historical_folder/'parity.tsv',golden,work,'historical-no-braking-vs-golden')
        compare_tapes(current_folder/'parity.tsv',historical_folder/'parity.tsv',work,'current-braking-effect',False)
        print('FORMAL_HISTORICAL_BRAKING_PARITY_COMPLETE original1440_raw_equal=1 current_differs=1 fields=20 main=0 native=0 GPU=0')

if __name__=='__main__':
    try:main()
    except Exception as error:
        print('DYNAMIC_CONTACT_FIRST_DIVERGENCE '+str(error),file=sys.stderr,flush=True)
        raise SystemExit(1)
