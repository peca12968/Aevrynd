"""Strict, portable deployment of one fixed development runtime cohort, not visual qualification."""
from pathlib import Path,PurePosixPath
import argparse,hashlib,json,os,shutil,tempfile
def need(v,m):
    if not v:raise RuntimeError(m)
def digest(path):return hashlib.sha256(path.read_bytes()).hexdigest().upper()
def inventory(root):
    need(root.is_dir() and not root.is_symlink(),'RUNTIME_ROOT')
    result={}
    for path in sorted(root.rglob('*')):
        need(not path.is_symlink(),'NO_RUNTIME_LINKS')
        if path.is_file():result[path.relative_to(root).as_posix()]={'bytes':path.stat().st_size,'sha256':digest(path)}
    return result
def main():
    ap=argparse.ArgumentParser();ap.add_argument('--manifest',type=Path,required=True);ap.add_argument('--source',type=Path,required=True);ap.add_argument('--output',type=Path,required=True);ap.add_argument('--verify-only',action='store_true');a=ap.parse_args()
    document=json.loads(a.manifest.read_bytes());expected=document['entries']
    need(document['count']==len(expected)==448 and document['status']=='FIXED_DEVELOPMENT_RUNTIME_IDENTITY_NOT_RELEASE_ACCEPTANCE','EXACT448_AUTHORITY')
    for name in expected:
        p=PurePosixPath(name);need(not p.is_absolute() and name==p.as_posix() and all(s not in ('.','..') and ':' not in s for s in p.parts),'SAFE_ASSET_MEMBER')
    need(inventory(a.source)==expected,'SOURCE_RUNTIME_IDENTITY')
    if a.output.exists():need(inventory(a.output)==expected,'EXISTING_OUTPUT_DIFFERS_DO_NOT_OVERWRITE')
    else:
        need(not a.verify_only,'RUNTIME_OUTPUT_MISSING');a.output.parent.mkdir(parents=True,exist_ok=True)
        stage=Path(tempfile.mkdtemp(prefix='riverbank-runtime-',dir=a.output.parent))
        for name,row in expected.items():
            source=a.source/name;target=stage/name;target.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(source,target)
            need(target.stat().st_size==row['bytes'] and digest(target)==row['sha256'],'DEPLOYED_ASSET_MISMATCH')
        need(inventory(stage)==expected and not a.output.exists(),'ATOMIC_COMPLETE_OUTPUT');os.rename(stage,a.output)
    print('RIVERBANK_FIXED_RUNTIME_IDENTITY_PASS files=448 scope=identity-only')
if __name__=='__main__':main()
