import json
import tempfile
import unittest
from pathlib import Path
from unittest import mock

from tools.luna_worker.worker import LunaClient, LunaError, WorkStore


class WorkerTests(unittest.TestCase):
    def test_registration_and_status_are_node_bound(self):
        with tempfile.TemporaryDirectory() as directory:
            store = WorkStore(Path(directory), "taoyuan", "gpt-5.6-luna")
            result = store.register("只做分析")
            self.assertEqual("taoyuan", result["nodeId"])
            self.assertEqual(1, store.status()["queued"])

    def test_invalid_prompt_is_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            store = WorkStore(Path(directory), "taichung", "gpt-5.6-luna")
            with self.assertRaises(LunaError):
                store.register("x" * 16_001)

    def test_client_rejects_missing_key(self):
        with self.assertRaises(LunaError):
            LunaClient("", "https://api.openai.com/v1", "gpt-5.6-luna")

    def test_process_writes_bounded_result(self):
        with tempfile.TemporaryDirectory() as directory:
            store = WorkStore(Path(directory), "taichung", "gpt-5.6-luna")
            store.register("hello")
            client = mock.Mock()
            client.complete.return_value = "完成"
            self.assertTrue(store.process_one(client))
            records = list((Path(directory) / "records").glob("*.json"))
            payload = json.loads(records[0].read_text(encoding="utf-8"))
            self.assertEqual("completed", payload["status"])
            self.assertEqual("完成", payload["result"])


if __name__ == "__main__":
    unittest.main()

