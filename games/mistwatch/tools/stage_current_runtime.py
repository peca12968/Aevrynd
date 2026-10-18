"""Stage exact reviewed418 runtime copies; never overwrite, delete or claim world/native acceptance."""
from __future__ import annotations
import argparse,json,runpy
from pathlib import Path

HERE=Path(__file__).resolve().parent
def stage(source:Path,runtime:Path)->dict:
    p=runpy.run_path(str(HERE/'package_forest_surface.py'))
    source=p['input_dir'](source);runtime=p['input_dir'](runtime);assets=runtime/'assets'
    require=p['require'];inventory=p['inventory'];pin=p['pin'];fixed=p['fixed_pin']
    require(HERE==source/'games/mistwatch/tools','CURRENT_STAGE_SOURCE_ROOT')
    base=source/p['MAPPING_RELATIVE'];leaf=source/p['LEAF_RELATIVE']
    fixed(base,103994,p['MAPPING_SHA']);fixed(leaf,656,p['LEAF_SHA'])
    surface=source/'games/mistwatch/native/tests/qualification/runtime-forest-surface-full-v4.json'
    current=surface.with_name('runtime-current-forest-full-v5.json')
    fixed(surface,816,'467BE56D0F3C5F1408E014DB50728DD1BCC9D28AB0366CE6B74957C74505F291')
    fixed(current,298,'8A2B65C6F27D9E7AC7CED21E69327D41514A4D72B22773B582D7D9CD6A57816B')
    wanted={row[0]:row for row in json.loads(base.read_bytes())['entries'] if row[2]&16}
    require(len(wanted)==414,'CURRENT_STAGE_PARENT414')
    for row in json.loads(leaf.read_bytes())['additions']:
        require(row[0] not in wanted,'CURRENT_STAGE_LEAF_ALIAS');wanted[row[0]]=row
    require(len(wanted)==416,'CURRENT_STAGE_PARENT416');parent_names=set(wanted)
    for row in json.loads(surface.read_bytes())['additions']:
        require(row[0] not in wanted,'CURRENT_STAGE_SURFACE_ALIAS');wanted[row[0]]=row
    require(len(wanted)==418,'CURRENT_STAGE_EXACT418')
    before=inventory(assets);require(set(before)<=set(wanted),'CURRENT_STAGE_UNEXPECTED_MEMBER')
    copied=[]
    for name,row in wanted.items():
        p['relative_member'](name);target=assets/name
        if name in before:continue
        require(row[3]=='copy','CURRENT_STAGE_MISSING_GENERATED_INPUT')
        p['relative_member'](row[1]);origin=source/row[1];expected=fixed(origin,row[4],row[5])
        p['copy_exact'](origin,target,expected,assets);copied.append(name)
    after=inventory(assets);require(set(after)==set(wanted),'CURRENT_STAGE_EXACT_MEMBERSHIP')
    require(all(after[name]==row for name,row in before.items()),'CURRENT_STAGE_EXISTING_BYTES_CHANGED')
    # 完整舊416派生物、source copy與root canonical先照原規格驗，再獨立核對兩張高度。
    cohort,inputs=p['reviewed_runtime'](source,runtime,{name:after[name] for name in parent_names})
    require(cohort['baseline_runtime_count']==416,'CURRENT_STAGE_REVIEWED_PARENT')
    for name in set(wanted)-parent_names:
        row=wanted[name];require(row[3]=='copy','CURRENT_STAGE_HEIGHT_COPY_ONLY')
        fixed(source/row[1],row[4],row[5]);fixed(assets/name,row[4],row[5])
    return dict(status='CURRENT_RUNTIME_READY',profile='authored-current-forest-full-v5',runtime_files=418,
        copied_members=copied,runtime_identity_only=True,current_world_qualified=False,native49_qualified=False)
def main()->int:
    parser=argparse.ArgumentParser();parser.add_argument('--source-root',type=Path,required=True)
    parser.add_argument('--runtime-dir',type=Path,required=True);args=parser.parse_args()
    result=stage(args.source_root,args.runtime_dir)
    print(json.dumps(result,separators=(',',':')));return 0
if __name__=='__main__':raise SystemExit(main())
