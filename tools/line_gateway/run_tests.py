"""Run the stdlib-only LINE gateway test suite."""

from __future__ import annotations

import unittest
from pathlib import Path
import sys


def main() -> int:
    gateway_root = Path(__file__).resolve().parent
    repository_root = gateway_root.parents[1]
    if str(repository_root) not in sys.path:
        sys.path.insert(0, str(repository_root))
    tests_root = gateway_root / "tests"
    suite = unittest.defaultTestLoader.discover(str(tests_root), pattern="test_*.py")
    result = unittest.TextTestRunner(verbosity=2).run(suite)
    return 0 if result.wasSuccessful() else 1


if __name__ == "__main__":
    raise SystemExit(main())
