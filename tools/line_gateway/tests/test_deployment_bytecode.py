"""Deployment invariants for the sealed LINE gateway Python runtime."""

from __future__ import annotations

import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest


PACKAGE_ROOT = Path(__file__).resolve().parents[1]
SCRIPTS_ROOT = PACKAGE_ROOT / "scripts"


class DeploymentBytecodeTests(unittest.TestCase):
    def test_runtime_import_with_fixed_launch_flags_creates_no_bytecode(self) -> None:
        with tempfile.TemporaryDirectory(prefix="aevrynd-bytecode-") as temporary:
            root = Path(temporary)
            package = root / "tools" / "line_gateway"
            package.mkdir(parents=True)
            for name in (
                "__init__.py",
                "config.py",
                "credential_store.py",
                "routing.py",
                "server.py",
                "signature.py",
            ):
                shutil.copy2(PACKAGE_ROOT / name, package / name)

            environment = os.environ.copy()
            environment["PYTHONPATH"] = str(root)
            environment["PYTHONDONTWRITEBYTECODE"] = "1"
            completed = subprocess.run(
                [sys.executable, "-B", "-c", "import tools.line_gateway.routing"],
                cwd=root,
                env=environment,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                timeout=10,
                check=False,
            )
            self.assertEqual(completed.returncode, 0, completed.stderr.decode(errors="replace"))
            self.assertEqual(list(root.rglob("*.pyc")), [])
            self.assertEqual(list(root.rglob("__pycache__")), [])

    def test_runner_guard_precedes_dynamic_application_imports(self) -> None:
        source = (SCRIPTS_ROOT / "gateway_runner.py").read_text(encoding="utf-8")
        self.assertLess(
            source.index("sys.dont_write_bytecode = True"),
            source.index("from tools.line_gateway.codex_handoff import CodexHandoff"),
        )


if __name__ == "__main__":
    unittest.main()
