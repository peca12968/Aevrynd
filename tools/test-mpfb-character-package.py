"""對真實 candidate7 清單做獨立負向測試，不改輸入資產。"""
import argparse
import copy
import importlib.util
from pathlib import Path
import subprocess
import sys
import tempfile
from unittest.mock import patch

sys.dont_write_bytecode = True
spec = importlib.util.spec_from_file_location("package_tool", Path(__file__).with_name("build-mpfb-character-package.py"))
package = importlib.util.module_from_spec(spec)
spec.loader.exec_module(package)
parser = argparse.ArgumentParser()
parser.add_argument("--artifact-root", required=True, type=Path)
parser.add_argument("--scratch", required=True, type=Path)
args = parser.parse_args()
result = package.build(args.artifact_root)
assert result["jointCount"] == 53 and len(result["components"]) == 4 and not result["gameplayReady"]
original = package.skeleton
for field in ("name", "parent", "translation", "inverseBind"):
    def damaged(path):
        rig, doc = original(path)
        if "bounded-shell" in str(path):
            rig = copy.deepcopy(rig)
            if field == "name":
                rig["nodes"][0]["name"] = "foreign-root"
            elif field == "parent":
                rig["nodes"][1]["parent"] = -1
            elif field == "translation":
                rig["nodes"][0]["translation"][0] += .001
            else:
                rig["inverseBinds"][0] = list(rig["inverseBinds"][0])
                rig["inverseBinds"][0][0] += .001
        return rig, doc
    with patch.object(package, "skeleton", damaged):
        try:
            package.build(args.artifact_root)
        except ValueError as error:
            assert "skeleton/bind mismatch" in str(error)
        else:
            raise AssertionError(field + " accepted")
original_identity = package.identity
def corrupt_texture(path):
    item = original_identity(path)
    if path.name == "source-0.png":
        item["sha256"] = "0" * 64
    return item
with patch.object(package, "identity", corrupt_texture):
    try:
        package.build(args.artifact_root)
    except ValueError as error:
        assert "texture identity mismatch" in str(error)
    else:
        raise AssertionError("texture corruption accepted")
args.scratch.mkdir(parents=True, exist_ok=True)
with tempfile.TemporaryDirectory(prefix="package-negative-", dir=args.scratch) as temporary:
    output = Path(temporary) / "rejected.json"
    run = subprocess.run([sys.executable, str(Path(__file__).with_name("build-mpfb-character-package.py")),
                          "--artifact-root", str(Path(temporary) / "absent"), "--output", str(output)], capture_output=True)
    assert run.returncode != 0 and not output.exists()
    assert not list(Path(temporary).glob("mpfb-package-*"))
print("MPFB_PACKAGE_TEST=PASS source-bind-negative=4 texture-negative=1 missing-input-atomic=1")
