import tempfile
import unittest
from pathlib import Path

from tools.remote_test_worker.controller import create_job, verify_receipt
from tools.remote_test_worker.protocol import ProtocolError, ReplayLedger, verify_envelope
from tools.remote_test_worker.worker import process_job
from tools.remote_test_worker.worker import SingleJobLock

KEY = b"k" * 32
SHA = "a" * 40


class FakeExecutor:
    def run(self, commit_sha, preset, timeout_seconds):
        return {"status": "passed", "log": "token=hidden C:\\private\\file ok",
                "toolchain": {"git": "2.51", "cmake": "4.1", "ctest": "4.1"},
                "tests": {"total": 1, "passed": 1, "failed": 0, "skipped": 0},
                "durations_ms": {"fetch": 1, "configure": 2, "build": 3, "test": 4, "total": 10}}


class ProtocolTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.job = create_job("taichung", SHA, "windows-release-gates", 300, KEY, now=1000)

    def tearDown(self): self.tmp.cleanup()

    def ledger(self, name): return ReplayLedger(Path(self.tmp.name) / name)

    def test_signed_round_trip_and_receipt(self):
        receipt = process_job(self.job, "taichung", KEY, self.ledger("worker.db"), FakeExecutor(), now=1000)
        checked = verify_receipt(receipt, self.job, KEY, self.ledger("controller.db"))
        self.assertEqual("passed", checked["status"])
        serialized = str(checked)
        self.assertNotIn("hidden", serialized)
        self.assertNotIn("private", serialized)

    def test_tamper_is_rejected(self):
        self.job["payload"]["timeout_seconds"] = 301
        with self.assertRaisesRegex(ProtocolError, "bad_signature"):
            process_job(self.job, "taichung", KEY, self.ledger("worker.db"), FakeExecutor(), now=1000)

    def test_replay_is_rejected(self):
        ledger = self.ledger("worker.db")
        process_job(self.job, "taichung", KEY, ledger, FakeExecutor(), now=1000)
        with self.assertRaisesRegex(ProtocolError, "replay"):
            process_job(self.job, "taichung", KEY, ledger, FakeExecutor(), now=1000)

    def test_unknown_field_cannot_be_command_injection(self):
        self.job["payload"]["command"] = "powershell evil"
        from tools.remote_test_worker.protocol import sign
        hostile = sign(self.job["payload"], KEY)
        with self.assertRaisesRegex(ProtocolError, "job_schema"):
            process_job(hostile, "taichung", KEY, self.ledger("worker.db"), FakeExecutor(), now=1000)

    def test_signed_type_confusion_is_rejected(self):
        from tools.remote_test_worker.protocol import sign
        for field, value in (("schema_version", True), ("issued_at", "1000"),
                             ("expires_at", 1000.5), ("job_id", self.job["payload"]["job_id"].upper())):
            payload = dict(self.job["payload"])
            payload[field] = value
            with self.subTest(field=field), self.assertRaises(ProtocolError):
                process_job(sign(payload, KEY), "taichung", KEY,
                            self.ledger("worker-" + field + ".db"), FakeExecutor(), now=1000)

    def test_signature_must_be_canonical_hex_string(self):
        malformed = dict(self.job)
        malformed["signature"] = 1
        with self.assertRaisesRegex(ProtocolError, "invalid_envelope"):
            process_job(malformed, "taichung", KEY, self.ledger("worker.db"), FakeExecutor(), now=1000)

    def test_receipt_rejects_path_in_toolchain(self):
        class PathLeakingExecutor(FakeExecutor):
            def run(inner_self, *args):
                result = super().run(*args)
                result["toolchain"]["cmake"] = "C:\\Tools\\cmake.exe"
                return result
        with self.assertRaisesRegex(ProtocolError, "receipt_toolchain_value"):
            process_job(self.job, "taichung", KEY, self.ledger("worker.db"), PathLeakingExecutor(), now=1000)

    def test_qwen_is_bounded_and_cannot_change_execution(self):
        seen = []
        receipt = process_job(self.job, "taichung", KEY, self.ledger("worker.db"), FakeExecutor(),
                              lambda text: seen.append(text) or "run-shell", now=1000)
        payload = verify_envelope(receipt, KEY)
        self.assertLessEqual(len(seen[0].encode()), 4096)
        self.assertEqual("clean", payload["classification"])

    def test_executor_cannot_spoof_receipt_shape_or_counts(self):
        class SpoofingExecutor(FakeExecutor):
            def run(inner_self, *args):
                result = super().run(*args)
                result["extra"] = "spoof"
                return result
        with self.assertRaisesRegex(ProtocolError, "executor_result_schema"):
            process_job(self.job, "taichung", KEY, self.ledger("worker.db"), SpoofingExecutor(), now=1000)

    def test_redaction_covers_bearer_unc_and_temp_paths(self):
        from tools.remote_test_worker.worker import redact_log
        redacted = redact_log(r"Authorization: Bearer abc \\server\share\x C:\\Temp\\x /tmp/a")
        self.assertNotIn("abc", redacted)
        self.assertNotIn("server", redacted)
        self.assertNotIn("Temp", redacted)
        self.assertNotIn("/tmp", redacted)

    def test_single_job_lock(self):
        lock_path = Path(self.tmp.name) / "worker.lock"
        with SingleJobLock(lock_path):
            with self.assertRaisesRegex(RuntimeError, "worker_busy"):
                with SingleJobLock(lock_path):
                    pass
        self.assertFalse(lock_path.exists())


if __name__ == "__main__": unittest.main()
