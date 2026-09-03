import pathlib
import sys
import unittest

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[2]))
raise SystemExit(0 if unittest.TextTestRunner(verbosity=2).run(
    unittest.defaultTestLoader.discover(str(pathlib.Path(__file__).with_name("tests")))
).wasSuccessful() else 1)
