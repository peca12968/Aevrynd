from __future__ import annotations

import hashlib
import http.client
import io
import sqlite3
import tempfile
import threading
import time
import unittest
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path
from types import SimpleNamespace
from contextlib import closing
from unittest.mock import patch

from tools.line_gateway.artifact_store import ArtifactError, ArtifactNotFound, ArtifactStore
from tools.line_gateway.server import GatewayRequestHandler, create_server


class ArtifactStoreTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.root = Path(self.temporary.name)
        self.store = ArtifactStore(self.root / "artifacts", allow_test_root=True)

    def tearDown(self):
        self.temporary.cleanup()

    def source(self, name="report.pptx", body=b"pptx-test"):
        path = self.root / name
        path.write_bytes(body)
        return path

    def test_publish_stores_only_hash_and_enforces_one_download(self):
        published = self.store.publish(self.source(), ttl_seconds=60, max_downloads=1, now=100)
        token = published.relative_url.rsplit("/", 1)[1]
        database = self.store.database.read_bytes()
        self.assertNotIn(token.encode(), database)
        result = self.store.consume(token, now=120)
        self.assertEqual(b"pptx-test", result.body)
        self.assertEqual("download.pptx", result.download_name)
        with self.assertRaises(ArtifactNotFound):
            self.store.consume(token, now=121)

    def test_extension_size_ttl_and_expiry_fail_closed(self):
        with self.assertRaisesRegex(ArtifactError, "extension_denied"):
            self.store.publish(self.source("bad.exe"), ttl_seconds=60)
        with self.assertRaisesRegex(ArtifactError, "ttl_denied"):
            self.store.publish(self.source(), ttl_seconds=59)
        published = self.store.publish(self.source(), ttl_seconds=60, now=100)
        with self.assertRaises(ArtifactNotFound):
            self.store.consume(published.relative_url.rsplit("/", 1)[1], now=161)

    def test_tampered_file_is_not_served(self):
        published = self.store.publish(self.source(), ttl_seconds=60, now=100)
        with closing(sqlite3.connect(self.store.database)) as connection:
            artifact_id, extension = connection.execute("SELECT artifact_id,extension FROM artifacts").fetchone()
        (self.store.files / f"{artifact_id}{extension}").write_bytes(b"changed")
        with self.assertRaises(ArtifactNotFound):
            self.store.inspect(published.relative_url.rsplit("/", 1)[1], now=120)
        with self.assertRaises(ArtifactNotFound):
            self.store.consume(published.relative_url.rsplit("/", 1)[1], now=120)

    def test_concurrent_download_limit_is_atomic(self):
        published = self.store.publish(self.source(), ttl_seconds=60, max_downloads=2, now=100)
        token = published.relative_url.rsplit("/", 1)[1]
        def attempt(_):
            try:
                self.store.consume(token, now=120)
                return True
            except ArtifactNotFound:
                return False
        with ThreadPoolExecutor(max_workers=6) as pool:
            results = list(pool.map(attempt, range(6)))
        self.assertEqual(2, sum(results))

    def test_gc_and_global_quota_bound_disk_growth(self):
        limited = ArtifactStore(self.root / "limited", allow_test_root=True,
                                max_total_bytes=20, max_artifacts=2)
        first = limited.publish(self.source("one.pdf", b"12345678"), ttl_seconds=60,
                                max_downloads=1, now=100)
        limited.publish(self.source("two.pdf", b"abcdefgh"), ttl_seconds=60, now=100)
        with self.assertRaisesRegex(ArtifactError, "quota_exceeded"):
            limited.publish(self.source("three.pdf", b"ABCDEFGH"), ttl_seconds=60, now=100)
        limited.consume(first.relative_url.rsplit("/", 1)[1], now=120)
        replacement = limited.publish(self.source("three.pdf", b"ABCDEFGH"), ttl_seconds=60, now=120)
        self.assertTrue(replacement.relative_url.startswith("/artifacts/"))
        self.assertLessEqual(len(list(limited.files.iterdir())), 2)

    def test_gc_preserves_unknown_orphan_and_counts_it_toward_physical_quota(self):
        limited = ArtifactStore(self.root / "orphan-limited", allow_test_root=True,
                                max_total_bytes=16, max_artifacts=2)
        orphan = limited.files / "unknown-orphan.pdf"
        orphan.write_bytes(b"orphan12")
        published = limited.publish(self.source("known.pdf", b"known123"), ttl_seconds=60, now=100)
        token = published.relative_url.rsplit("/", 1)[1]
        limited.garbage_collect(now=161)
        self.assertTrue(orphan.exists())
        with self.assertRaises(ArtifactNotFound):
            limited.inspect(token, now=161)
        with self.assertRaisesRegex(ArtifactError, "quota_exceeded"):
            limited.publish(self.source("new.pdf", b"newbytes9"), ttl_seconds=60, now=162)

    def test_reservation_persists_beyond_old_sixty_seconds_until_artifact_expiry(self):
        base = int(time.time())
        published = self.store.publish(self.source(), ttl_seconds=600, max_downloads=1, now=base)
        token = published.relative_url.rsplit("/", 1)[1]
        first = self.store.prepare_download(token, now=base + 20)
        with self.assertRaises(ArtifactNotFound):
            self.store.prepare_download(token, now=base + 21)
        self.store.release_download(first.reservation_id, now=base + 21)
        crashed = self.store.prepare_download(token, now=base + 22)
        with self.assertRaises(ArtifactNotFound):
            self.store.prepare_download(token, now=base + 200)
        with self.assertRaises(ArtifactNotFound):
            self.store.prepare_download(token, now=base + 599)
        restarted = ArtifactStore(self.store.root, allow_test_root=True,
                                  recover_startup_reservations=True)
        recovered = restarted.prepare_download(token, now=base + 599)
        self.assertNotEqual(crashed.reservation_id, recovered.reservation_id)
        restarted.finalize_download(recovered.reservation_id, now=base + 599)
        with self.assertRaises(ArtifactNotFound):
            restarted.prepare_download(token, now=base + 599)

    def test_parallel_reservations_enforce_exact_download_limit(self):
        published = self.store.publish(self.source(), ttl_seconds=600, max_downloads=2, now=100)
        token = published.relative_url.rsplit("/", 1)[1]

        def reserve(_):
            try:
                return self.store.prepare_download(token, now=120)
            except ArtifactNotFound:
                return None

        with ThreadPoolExecutor(max_workers=8) as pool:
            reservations = [item for item in pool.map(reserve, range(8)) if item is not None]
        self.assertEqual(2, len(reservations))
        self.store.release_download(reservations[0].reservation_id, now=121)
        replacement = self.store.prepare_download(token, now=121)
        self.store.finalize_download(reservations[1].reservation_id, now=121)
        self.store.finalize_download(replacement.reservation_id, now=121)


class ArtifactHttpTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        root = Path(self.temporary.name)
        source = root / "report.pdf"
        source.write_bytes(b"safe-pdf")
        self.store = ArtifactStore(root / "artifacts", allow_test_root=True)
        self.published = self.store.publish(source, ttl_seconds=3600, max_downloads=1)
        config = SimpleNamespace(bind_host="127.0.0.1", port=0, request_timeout_seconds=2.0)
        self.server = create_server(config, lambda _event: True, artifact_store=self.store)
        self.thread = threading.Thread(target=self.server.serve_forever, daemon=True)
        self.thread.start()

    def tearDown(self):
        self.server.shutdown(); self.server.server_close(); self.thread.join(timeout=2)
        self.temporary.cleanup()

    def request(self, path, headers=None):
        connection = http.client.HTTPConnection("127.0.0.1", self.server.server_port, timeout=2)
        connection.request("GET", path, headers=headers or {})
        response = connection.getresponse()
        result = response.status, dict(response.headers), response.read()
        connection.close()
        return result

    def test_preview_does_not_consume_and_download_is_one_time(self):
        status, headers, body = self.request(self.published.relative_url)
        self.assertEqual(200, status); self.assertIn(b"/download", body)
        self.assertEqual("private, no-store, max-age=0", headers["Cache-Control"])
        self.assertNotIn(str(Path(self.temporary.name)), str(headers))
        self.assertEqual(200, self.request(self.published.relative_url)[0])
        self.assertEqual((200, b"safe-pdf"), (lambda response: (response[0], response[2]))(
            self.request(self.published.relative_url + "/download")))
        self.assertEqual(404, self.request(self.published.relative_url + "/download")[0])
        self.assertEqual(404, self.request(self.published.relative_url)[0])

    def test_range_query_and_traversal_are_rejected_without_consuming(self):
        self.assertEqual(400, self.request(self.published.relative_url + "?x=1")[0])
        self.assertEqual(400, self.request(self.published.relative_url, {"Range": "bytes=0-1"})[0])
        self.assertEqual(400, self.request(self.published.relative_url + "/download?x=1")[0])
        self.assertEqual(400, self.request(self.published.relative_url + "/download", {"Range": "bytes=0-1"})[0])
        self.assertIn(self.request("/artifacts/..%2fsecret")[0], {400, 404})
        self.assertEqual(200, self.request(self.published.relative_url)[0])
        self.assertEqual(200, self.request(self.published.relative_url + "/download")[0])

    def test_broken_transport_releases_reservation_without_consuming(self):
        token = self.published.relative_url.rsplit("/", 1)[1]

        class BrokenWriter(io.BytesIO):
            def write(self, _body):
                raise BrokenPipeError()

        handler = object.__new__(GatewayRequestHandler)
        handler.path = self.published.relative_url + "/download"
        handler.headers = SimpleNamespace(get_all=lambda _name, default=None: default or [])
        handler.server = SimpleNamespace(
            artifact_store=self.store,
            gateway_config=SimpleNamespace(request_timeout_seconds=2.0),
        )
        handler.connection = SimpleNamespace(settimeout=lambda _seconds: None)
        handler.wfile = BrokenWriter()
        handler.send_response = lambda _status: None
        handler.send_header = lambda _name, _value: None
        handler.end_headers = lambda: None
        GatewayRequestHandler._handle_artifact(handler)
        available = self.store.prepare_download(token)
        self.store.release_download(available.reservation_id)

    def test_post_flush_finalize_failure_retries_and_keeps_limit_reserved(self):
        token = self.published.relative_url.rsplit("/", 1)[1]
        handler = object.__new__(GatewayRequestHandler)
        handler.path = self.published.relative_url + "/download"
        handler.headers = SimpleNamespace(get_all=lambda _name, default=None: default or [])
        handler.server = SimpleNamespace(
            artifact_store=self.store,
            gateway_config=SimpleNamespace(request_timeout_seconds=2.0),
        )
        handler.connection = SimpleNamespace(settimeout=lambda _seconds: None)
        handler.wfile = io.BytesIO()
        handler.send_response = lambda _status: None
        handler.send_header = lambda _name, _value: None
        handler.end_headers = lambda: None
        with patch.object(self.store, "finalize_download", side_effect=sqlite3.OperationalError("busy")) as finalize:
            GatewayRequestHandler._handle_artifact(handler)
        self.assertEqual(3, finalize.call_count)
        with self.assertRaises(ArtifactNotFound):
            self.store.prepare_download(token)


if __name__ == "__main__": unittest.main()
