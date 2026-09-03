import unittest
from pathlib import Path

from tools.remote_test_worker.executor import WINDOWS_CREATION_FLAGS, build_command_plan
from tools.remote_test_worker.policy import FIXED_ORIGIN
from tools.remote_test_worker.protocol import ProtocolError


class ExecutorTests(unittest.TestCase):
    def test_exact_allowlisted_commands(self):
        plan = build_command_plan("a" * 40, "windows-release-gates", Path("C:/ProgramData/Aevrynd/Repo"),
                                  Path("C:/ProgramData/Aevrynd/Jobs"), FIXED_ORIGIN)
        self.assertEqual(("cmake", "--preset", "windows-release-gates"), plan.configure)
        self.assertEqual(("git", "-C", "C:\\ProgramData\\Aevrynd\\Repo", "rev-parse", "--verify",
                          "a" * 40 + "^{commit}"), plan.verify)
        self.assertIn("AevryndStreamlinePackageFixture", plan.build)
        self.assertEqual("^release-gates$", plan.test[-2])
        self.assertEqual(0x08004000, WINDOWS_CREATION_FLAGS)

    def test_wrong_origin_fails_closed(self):
        with self.assertRaisesRegex(ProtocolError, "origin_mismatch"):
            build_command_plan("a" * 40, "windows-release-gates", Path("C:/fixed"), Path("C:/jobs"), "https://evil")

    def test_commit_cannot_be_a_path_or_ref(self):
        with self.assertRaisesRegex(ProtocolError, "commit_denied"):
            build_command_plan("../main", "windows-release-gates", Path("C:/fixed"), Path("C:/jobs"), FIXED_ORIGIN)

    def test_unc_work_root_fails_closed(self):
        with self.assertRaisesRegex(ProtocolError, "unsafe_fixed_root"):
            build_command_plan("a" * 40, "windows-release-gates", Path("C:/fixed"),
                               Path(r"\\server\jobs"), FIXED_ORIGIN)


if __name__ == "__main__": unittest.main()
