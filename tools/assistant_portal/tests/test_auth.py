"""驗證 Email OTP 安全契約、並發行為與 HTTP gate。"""
from __future__ import annotations

import importlib.util
import json
import os
import subprocess
import sys
import threading
import tempfile
import time
import unittest
import urllib.error
import urllib.request
from unittest import mock
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location("assistant_portal_server", ROOT / "server.py")
assert SPEC and SPEC.loader
server_module = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = server_module
SPEC.loader.exec_module(server_module)


class Clock:
    def __init__(self) -> None:
        self.value = 1000.0
    def __call__(self) -> float:
        return self.value


class PersistentDiagnosticTests(unittest.TestCase):
    def test_readiness_is_persistent_bounded_and_non_secret(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "smtp.json"
            journal = server_module.SmtpReadinessJournal(str(path))
            journal.write(ready=True, safe_code="ready", successes=3, failures=1)
            self.assertEqual({"ready": True, "status": "ready"}, server_module.SmtpReadinessJournal(str(path)).public())
            content = path.read_text(encoding="utf-8")
            self.assertNotIn(server_module.ALLOWED_EMAIL, content)
            self.assertLessEqual(len(content), 1024)

    def test_corrupt_diagnostic_fails_closed_without_detail(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "smtp.json"
            path.write_text("secret-content", encoding="utf-8")
            self.assertEqual({"ready": False, "status": "unknown"}, server_module.SmtpReadinessJournal(str(path)).public())


class RecordingAdapter:
    is_preview = False
    def __init__(self, delay: float = 0) -> None:
        self.delay = delay
        self.sent: list[tuple[str, str]] = []
    def send_otp(self, email: str, otp: str) -> None:
        if self.delay:
            time.sleep(self.delay)
        self.sent.append((email, otp))


class FailingAdapter(RecordingAdapter):
    def send_otp(self, email: str, otp: str) -> None:
        del email, otp
        raise RuntimeError("simulated delivery failure")


class UnknownEmailReadinessTests(unittest.TestCase):
    def test_unknown_email_does_not_change_persistent_readiness(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "smtp.json"
            diagnostic = server_module.SmtpReadinessJournal(str(path))
            diagnostic.write(ready=False, safe_code="smtp_timeout", failures=1)
            before = path.read_bytes()
            auth = server_module.AuthManager(RecordingAdapter(), diagnostic=diagnostic)
            auth.request_otp("unknown@example.com", "192.0.2.1")
            deadline = time.monotonic() + 1
            while auth._pending and time.monotonic() < deadline:
                time.sleep(0.01)
            auth.close()
            self.assertEqual(before, path.read_bytes())


class AuthManagerTests(unittest.TestCase):
    def setUp(self) -> None:
        self.clock = Clock()
        self.auth = server_module.AuthManager(server_module.PreviewEmailAdapter(), dev_show_otp=True, clock=self.clock)

    def request(self, email: str = server_module.ALLOWED_EMAIL, ip: str = "127.0.0.1") -> tuple[str | None, int, bool]:
        return self.auth.request_otp(email, ip)

    def test_allowlist_normalizes_and_exact_otp_is_single_use(self) -> None:
        code, _, failed = self.request("  SLIME12968@GMAIL.COM ")
        self.assertFalse(failed)
        self.assertRegex(code or "", r"^[A-Z2-9]{8}$")
        record = self.auth._otp_records[server_module.ALLOWED_EMAIL]
        self.assertNotIn(code.encode("ascii"), (record.salt, record.digest))
        self.assertEqual((False, None), self.auth.verify_otp(server_module.ALLOWED_EMAIL, code.lower(), "127.0.0.1"))
        success, token = self.auth.verify_otp(server_module.ALLOWED_EMAIL, code, "127.0.0.1")
        self.assertTrue(success)
        self.assertTrue(token)
        self.assertEqual((False, None), self.auth.verify_otp(server_module.ALLOWED_EMAIL, code, "127.0.0.1"))

    def test_otp_generation_excludes_ambiguous_symbols(self) -> None:
        for _ in range(100):
            code = self.auth.generate_otp()
            self.assertEqual(8, len(code))
            self.assertTrue(set(code) <= set(server_module.OTP_ALPHABET))
            self.assertFalse(set(code) & set("01ILO"))

    def test_expiry_and_five_failures_invalidate(self) -> None:
        code, _, _ = self.request()
        for invalid in ("", "short", "!!!!!!!!", "TOO-LONG-CODE", "00000000"):
            self.auth.verify_otp(server_module.ALLOWED_EMAIL, invalid, "127.0.0.1")
        self.assertEqual((False, None), self.auth.verify_otp(server_module.ALLOWED_EMAIL, code, "127.0.0.1"))
        self.clock.value += 61
        next_code, _, _ = self.request()
        self.clock.value += 601
        self.assertEqual((False, None), self.auth.verify_otp(server_module.ALLOWED_EMAIL, next_code, "127.0.0.1"))

    def test_unknown_email_never_gets_code_or_record(self) -> None:
        code, retry, failed = self.request("UNKNOWN@EXAMPLE.COM")
        self.assertIsNone(code)
        self.assertEqual(60, retry)
        self.assertFalse(failed)
        self.assertNotIn("unknown@example.com", self.auth._otp_records)

    def test_account_and_ip_limiters_are_independent(self) -> None:
        self.request("one@example.com", "10.0.0.1")
        self.assertIn("one@example.com", self.auth._account_requests)
        self.assertIn("10.0.0.1", self.auth._ip_requests)
        self.assertNotIn(("one@example.com", "10.0.0.1"), self.auth._account_requests)
        self.clock.value += 61
        for index in range(server_module.REQUEST_RATE_LIMIT):
            self.request(f"rotate{index}@example.com", "10.0.0.2")
        blocked, _, _ = self.request("fresh@example.com", "10.0.0.2")
        self.assertIsNone(blocked)
        self.clock.value += 601
        for index in range(server_module.REQUEST_RATE_LIMIT):
            self.request("fixed@example.com", f"10.0.1.{index}")
            self.clock.value += 61
        blocked, _, _ = self.request("fixed@example.com", "10.0.2.1")
        self.assertIsNone(blocked)

    def test_all_state_maps_are_bounded_and_ttl_cleaned(self) -> None:
        for index in range(server_module.MAX_RATE_KEYS + 20):
            self.request(f"user{index}@example.com", f"10.1.{index // 250}.{index % 250}")
            self.auth.issue_csrf()
        self.assertLessEqual(len(self.auth._account_requests), server_module.MAX_RATE_KEYS)
        self.assertLessEqual(len(self.auth._ip_requests), server_module.MAX_RATE_KEYS)
        self.assertLessEqual(len(self.auth._csrf_tokens), server_module.MAX_CSRF_TOKENS)
        self.clock.value += server_module.RATE_WINDOW_SECONDS + server_module.CSRF_TTL_SECONDS + 1
        self.auth.issue_csrf()
        self.assertLessEqual(len(self.auth._account_requests), 1)
        self.assertLessEqual(len(self.auth._ip_requests), 1)

    def test_csrf_capacity_never_evicts_a_live_token(self) -> None:
        first = self.auth.issue_csrf("10.10.0.1")
        self.assertIsNotNone(first)
        for index in range(1, server_module.MAX_CSRF_TOKENS):
            token = self.auth.issue_csrf(f"10.10.{index // 29}.{index % 29 + 1}")
            self.assertIsNotNone(token)
        self.assertIsNone(self.auth.issue_csrf("10.20.0.1"))
        self.assertTrue(self.auth.validate_csrf(first or "", first or ""))

    def test_csrf_ip_limit_is_independent_from_global_limit(self) -> None:
        for _ in range(server_module.CSRF_IP_RATE_LIMIT):
            self.assertIsNotNone(self.auth.issue_csrf("10.30.0.1"))
        self.assertIsNone(self.auth.issue_csrf("10.30.0.1"))
        self.assertIsNotNone(self.auth.issue_csrf("10.30.0.2"))

    def test_session_store_is_bounded_and_expired_entries_are_collected(self) -> None:
        for index in range(server_module.MAX_SESSIONS + 25):
            self.auth._sessions[index.to_bytes(32, "little")] = self.clock.value + 10
        self.auth.issue_csrf()
        self.assertLessEqual(len(self.auth._sessions), server_module.MAX_SESSIONS)
        self.clock.value += 11
        self.auth.issue_csrf()
        self.assertEqual(0, len(self.auth._sessions))

    def test_dev_otp_display_rejects_real_adapter(self) -> None:
        with self.assertRaises(RuntimeError):
            server_module.AuthManager(RecordingAdapter(), dev_show_otp=True)

    def test_gmail_smtp_adapter_requires_exact_allowlisted_identity(self) -> None:
        base = {
            "AEVRYND_SMTP_HOST": "smtp.gmail.com",
            "AEVRYND_SMTP_PORT": "465",
            "AEVRYND_SMTP_PASSWORD": "not-a-real-app-password",
        }
        with mock.patch.dict(os.environ, {**base, "AEVRYND_SMTP_USERNAME": "other@gmail.com", "AEVRYND_SMTP_SENDER": "other@gmail.com"}, clear=False):
            with self.assertRaises(RuntimeError):
                server_module.SmtpEmailAdapter()
        with mock.patch.dict(os.environ, {**base, "AEVRYND_SMTP_USERNAME": server_module.ALLOWED_EMAIL, "AEVRYND_SMTP_SENDER": "other@gmail.com"}, clear=False):
            with self.assertRaises(RuntimeError):
                server_module.SmtpEmailAdapter()
        with mock.patch.dict(os.environ, {**base, "AEVRYND_SMTP_USERNAME": server_module.ALLOWED_EMAIL, "AEVRYND_SMTP_SENDER": server_module.ALLOWED_EMAIL}, clear=False):
            self.assertFalse(server_module.SmtpEmailAdapter().is_preview)

    def test_smtp_preflight_logs_in_without_sending_mail(self) -> None:
        environment = {
            "AEVRYND_SMTP_HOST": "smtp.gmail.com",
            "AEVRYND_SMTP_PORT": "465",
            "AEVRYND_SMTP_USERNAME": server_module.ALLOWED_EMAIL,
            "AEVRYND_SMTP_SENDER": server_module.ALLOWED_EMAIL,
            "AEVRYND_SMTP_PASSWORD": "not-a-real-app-password",
        }
        client = mock.MagicMock()
        client.__enter__.return_value = client
        with mock.patch.dict(os.environ, environment, clear=False), mock.patch.object(server_module.smtplib, "SMTP_SSL", return_value=client):
            server_module.SmtpEmailAdapter().preflight()
        client.login.assert_called_once_with(server_module.ALLOWED_EMAIL, "not-a-real-app-password")
        client.send_message.assert_not_called()

    def test_slow_adapter_does_not_hold_global_lock_and_concurrent_send_is_reserved(self) -> None:
        adapter = RecordingAdapter(delay=0.2)
        auth = server_module.AuthManager(adapter, clock=self.clock)
        result: list[tuple[str | None, int, bool]] = []
        worker = threading.Thread(target=lambda: result.append(auth.request_otp(server_module.ALLOWED_EMAIL, "10.0.0.1")))
        worker.start()
        time.sleep(0.05)
        started = time.monotonic()
        auth.issue_csrf()
        self.assertLess(time.monotonic() - started, 0.08)
        duplicate = auth.request_otp(server_module.ALLOWED_EMAIL, "10.0.0.2")
        worker.join(timeout=1)
        deadline = time.monotonic() + 1
        while len(adapter.sent) < 1 and time.monotonic() < deadline:
            time.sleep(0.01)
        self.assertEqual(1, len(adapter.sent))
        self.assertIsNone(duplicate[0])
        self.assertEqual(1, len(result))
        auth.close()

    def test_delivery_health_exposes_only_bounded_safe_codes(self) -> None:
        auth = server_module.AuthManager(FailingAdapter(), clock=self.clock)
        auth.request_otp(server_module.ALLOWED_EMAIL, "10.0.0.5")
        deadline = time.monotonic() + 1
        while auth.delivery_health()["failures"] == 0 and time.monotonic() < deadline:
            time.sleep(0.01)
        self.assertEqual({"successes": 0, "failures": 1, "last_safe_code": "delivery_internal_error"}, auth.delivery_health())
        auth.close()


class HttpAuthTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        auth = server_module.AuthManager(server_module.PreviewEmailAdapter(), dev_show_otp=True)
        cls.server = server_module.PortalServer(("127.0.0.1", 0), ROOT, auth, False)
        cls.thread = threading.Thread(target=cls.server.serve_forever, daemon=True)
        cls.thread.start()
        cls.base = f"http://127.0.0.1:{cls.server.server_port}"

    @classmethod
    def tearDownClass(cls) -> None:
        cls.server.shutdown()
        cls.server.server_close()
        cls.thread.join(timeout=2)

    def fetch_csrf(self) -> tuple[str, str]:
        response = urllib.request.urlopen(self.base + "/api/auth/csrf")
        return json.load(response)["csrf_token"], response.headers["Set-Cookie"]

    def post(self, path: str, body: dict[str, object], csrf: str = "", cookie: str = ""):
        return urllib.request.urlopen(urllib.request.Request(self.base + path, data=json.dumps(body).encode(), headers={"Content-Type": "application/json", "Cookie": cookie, "X-CSRF-Token": csrf, "Origin": self.base}))

    def test_unauthenticated_page_and_assets_are_server_gated(self) -> None:
        landing = urllib.request.urlopen(self.base + "/").read().decode()
        self.assertIn("登入助理控制中心", landing)
        self.assertNotIn("AI 助理控制中心", landing)
        self.assertNotIn("三台電腦預覽狀態", landing)
        for path in ("/portal", "/app.js", "/api/private/status"):
            with self.assertRaises(urllib.error.HTTPError) as captured:
                urllib.request.urlopen(self.base + path)
            self.assertEqual(401, captured.exception.code)
        health = json.load(urllib.request.urlopen(self.base + "/api/health/email"))
        self.assertEqual({"ready": False, "status": "unknown"}, health)
        self.assertEqual({"ready", "status"}, set(health))

    def test_static_allowlist_blocks_source_tests_dotfiles_and_traversal(self) -> None:
        self.assertEqual(200, urllib.request.urlopen(self.base + "/styles.css").status)
        for path in ("/server.py", "/README.md", "/tests/test_auth.py", "/.git/config", "/%2e%2e/server.py", "/styles.css?x=1"):
            with self.assertRaises(urllib.error.HTTPError) as captured:
                urllib.request.urlopen(self.base + path)
            self.assertEqual(404, captured.exception.code)

    def test_csrf_unknown_email_and_session_cookie_contract(self) -> None:
        with self.assertRaises(urllib.error.HTTPError) as captured:
            self.post("/api/auth/request-otp", {"email": server_module.ALLOWED_EMAIL})
        self.assertEqual(403, captured.exception.code)
        csrf, cookie = self.fetch_csrf()
        unknown = json.load(self.post("/api/auth/request-otp", {"email": "unknown@example.com"}, csrf, cookie))
        self.assertEqual(server_module.GENERIC_REQUEST_MESSAGE, unknown["message"])
        self.assertNotIn("preview_code", unknown)
        allowed = json.load(self.post("/api/auth/request-otp", {"email": server_module.ALLOWED_EMAIL}, csrf, cookie))
        self.assertRegex(allowed["preview_code"], r"^[A-Z2-9]{8}$")
        verified = self.post("/api/auth/verify-otp", {"email": server_module.ALLOWED_EMAIL, "otp": allowed["preview_code"]}, csrf, cookie)
        self.assertTrue(json.load(verified)["authenticated"])
        session_cookie = verified.headers["Set-Cookie"]
        self.assertIn("HttpOnly", session_cookie)
        self.assertIn("SameSite=Strict", session_cookie)
        self.assertIn("Max-Age=28800", session_cookie)
        self.assertNotIn("Secure", session_cookie)
        portal = urllib.request.urlopen(urllib.request.Request(self.base + "/portal", headers={"Cookie": session_cookie})).read().decode()
        self.assertIn("AI 助理控制中心", portal)
        self.assertNotIn("登入助理控制中心", portal)
        session_pair = session_cookie.split(";", 1)[0]
        csrf_pair = cookie.split(";", 1)[0]
        logout = self.post("/api/auth/logout", {}, csrf, f"{csrf_pair}; {session_pair}")
        self.assertIn("Max-Age=0", logout.headers["Set-Cookie"])
        with self.assertRaises(urllib.error.HTTPError) as captured:
            urllib.request.urlopen(urllib.request.Request(self.base + "/portal", headers={"Cookie": session_pair}))
        self.assertEqual(401, captured.exception.code)

    def test_verify_unknown_and_wrong_code_have_identical_response(self) -> None:
        csrf, cookie = self.fetch_csrf()
        unknown = json.load(self.post("/api/auth/verify-otp", {"email": "unknown@example.com", "otp": "AAAAAAAA"}, csrf, cookie))
        wrong = json.load(self.post("/api/auth/verify-otp", {"email": server_module.ALLOWED_EMAIL, "otp": "AAAAAAAA"}, csrf, cookie))
        self.assertEqual(unknown, wrong)

    def test_bad_host_and_origin_are_rejected(self) -> None:
        request = urllib.request.Request(self.base + "/", headers={"Host": "evil.example"})
        with self.assertRaises(urllib.error.HTTPError) as captured:
            urllib.request.urlopen(request)
        self.assertEqual(421, captured.exception.code)
        csrf, cookie = self.fetch_csrf()
        request = urllib.request.Request(self.base + "/api/auth/request-otp", data=b'{"email":"a@example.com"}', headers={"Content-Type": "application/json", "Cookie": cookie, "X-CSRF-Token": csrf, "Origin": "https://evil.example"})
        with self.assertRaises(urllib.error.HTTPError) as captured:
            urllib.request.urlopen(request)
        self.assertEqual(403, captured.exception.code)


class ProductionStartupTests(unittest.TestCase):
    def test_production_fails_closed_with_preview_adapter(self) -> None:
        environment = os.environ.copy()
        environment["AEVRYND_AUTH_ADAPTER"] = "preview"
        result = subprocess.run(
            [sys.executable, str(ROOT / "server.py"), "--production", "--port", "18991"],
            capture_output=True,
            text=True,
            timeout=5,
            env=environment,
        )
        self.assertNotEqual(0, result.returncode)
        self.assertIn("Production requires", result.stderr + result.stdout)

    def test_production_session_cookie_is_secure_and_no_otp_is_returned(self) -> None:
        adapter = RecordingAdapter()
        auth = server_module.AuthManager(adapter)
        server = server_module.PortalServer(("127.0.0.1", 0), ROOT, auth, True, public_host="portal.example.com", trusted_proxy_sources=frozenset(("127.0.0.1",)), instance_nonce="a" * 32)
        thread = threading.Thread(target=server.serve_forever, daemon=True)
        thread.start()
        base = f"http://127.0.0.1:{server.server_port}"
        try:
            csrf_response = urllib.request.urlopen(urllib.request.Request(base + "/api/auth/csrf", headers={"Host": "portal.example.com", "X-Forwarded-For": "203.0.113.10"}))
            csrf = json.load(csrf_response)["csrf_token"]
            csrf_cookie = csrf_response.headers["Set-Cookie"]
            self.assertIn("Secure", csrf_cookie)
            self.assertEqual("max-age=31536000; includeSubDomains", csrf_response.headers["Strict-Transport-Security"])
            landing = urllib.request.urlopen(urllib.request.Request(base + "/", headers={"Host": "portal.example.com"})).read().decode()
            self.assertIn('<meta name="aevrynd-instance" content="' + "a" * 32 + '">', landing)
            request = urllib.request.Request(base + "/api/auth/request-otp", data=json.dumps({"email": server_module.ALLOWED_EMAIL}).encode(), headers={"Host": "portal.example.com", "Origin": "https://portal.example.com", "X-Forwarded-For": "203.0.113.10", "Content-Type": "application/json", "Cookie": csrf_cookie, "X-CSRF-Token": csrf})
            payload = json.load(urllib.request.urlopen(request))
            self.assertNotIn("preview_code", payload)
            deadline = time.monotonic() + 1
            while len(adapter.sent) < 1 and time.monotonic() < deadline:
                time.sleep(0.01)
            code = adapter.sent[0][1]
            deadline = time.monotonic() + 1
            while server_module.ALLOWED_EMAIL not in auth._otp_records and time.monotonic() < deadline:
                time.sleep(0.01)
            verify = urllib.request.Request(base + "/api/auth/verify-otp", data=json.dumps({"email": server_module.ALLOWED_EMAIL, "otp": code}).encode(), headers={"Host": "portal.example.com", "Origin": "https://portal.example.com", "X-Forwarded-For": "203.0.113.10", "Content-Type": "application/json", "Cookie": csrf_cookie, "X-CSRF-Token": csrf})
            response = urllib.request.urlopen(verify)
            self.assertTrue(json.load(response)["authenticated"])
            self.assertIn("Secure", response.headers["Set-Cookie"])
            session_cookie = response.headers["Set-Cookie"].split(";", 1)[0]
            status = urllib.request.urlopen(urllib.request.Request(base + "/api/private/status", headers={"Host": "portal.example.com", "Cookie": session_cookie}))
            self.assertEqual({"successes", "failures", "last_safe_code"}, set(json.load(status)["email_delivery"]))
        finally:
            server.shutdown()
            server.server_close()
            thread.join(timeout=2)

    def test_delivery_failure_does_not_change_generic_http_response(self) -> None:
        auth = server_module.AuthManager(FailingAdapter())
        server = server_module.PortalServer(("127.0.0.1", 0), ROOT, auth, True, public_host="portal.example.com", trusted_proxy_sources=frozenset(("127.0.0.1",)), instance_nonce="b" * 32)
        thread = threading.Thread(target=server.serve_forever, daemon=True)
        thread.start()
        base = f"http://127.0.0.1:{server.server_port}"
        try:
            csrf_response = urllib.request.urlopen(urllib.request.Request(base + "/api/auth/csrf", headers={"Host": "portal.example.com", "X-Forwarded-For": "203.0.113.10"}))
            csrf = json.load(csrf_response)["csrf_token"]
            cookie = csrf_response.headers["Set-Cookie"]
            def request(email: str) -> tuple[int, dict[str, object]]:
                value = urllib.request.Request(base + "/api/auth/request-otp", data=json.dumps({"email": email}).encode(), headers={"Host": "portal.example.com", "Origin": "https://portal.example.com", "X-Forwarded-For": "203.0.113.10", "Content-Type": "application/json", "Cookie": cookie, "X-CSRF-Token": csrf})
                response = urllib.request.urlopen(value)
                return response.status, json.load(response)
            known = request(server_module.ALLOWED_EMAIL)
            unknown = request("unknown@example.com")
            self.assertEqual(known[0], unknown[0])
            self.assertEqual(known[1]["message"], unknown[1]["message"])
            self.assertNotIn("preview_code", known[1])
            self.assertNotIn("preview_code", unknown[1])
        finally:
            server.shutdown()
            server.server_close()
            thread.join(timeout=2)

    def test_trusted_proxy_requires_one_strict_forwarding_value(self) -> None:
        auth = server_module.AuthManager(server_module.PreviewEmailAdapter())
        server = server_module.PortalServer(("127.0.0.1", 0), ROOT, auth, True, public_host="portal.example.com", trusted_proxy_sources=frozenset(("127.0.0.1",)), instance_nonce="c" * 32)
        thread = threading.Thread(target=server.serve_forever, daemon=True)
        thread.start()
        base = f"http://127.0.0.1:{server.server_port}"
        try:
            csrf_response = urllib.request.urlopen(urllib.request.Request(base + "/api/auth/csrf", headers={"Host": "portal.example.com", "X-Forwarded-For": "203.0.113.10"}))
            csrf = json.load(csrf_response)["csrf_token"]
            cookie = csrf_response.headers["Set-Cookie"]
            base_headers = {"Host": "portal.example.com", "Origin": "https://portal.example.com", "Content-Type": "application/json", "Cookie": cookie, "X-CSRF-Token": csrf}
            invalid_headers = (
                {},
                {"X-Forwarded-For": "203.0.113.1, 203.0.113.2"},
                {"X-Forwarded-For": "203.0.113.1", "Forwarded": "for=203.0.113.1"},
                {"Forwarded": "for=unknown"},
            )
            for extra in invalid_headers:
                headers = {**base_headers, **extra}
                request = urllib.request.Request(base + "/api/auth/request-otp", data=b'{"email":"unknown@example.com"}', headers=headers)
                with self.assertRaises(urllib.error.HTTPError) as captured:
                    urllib.request.urlopen(request)
                self.assertEqual(400, captured.exception.code)
            valid = urllib.request.Request(base + "/api/auth/request-otp", data=b'{"email":"unknown@example.com"}', headers={**base_headers, "Forwarded": 'for="203.0.113.77"'})
            self.assertEqual(200, urllib.request.urlopen(valid).status)
        finally:
            server.shutdown()
            server.server_close()
            thread.join(timeout=2)

    def test_untrusted_source_cannot_spoof_forwarded_ip(self) -> None:
        auth = server_module.AuthManager(server_module.PreviewEmailAdapter())
        server = server_module.PortalServer(("127.0.0.1", 0), ROOT, auth, True, public_host="portal.example.com", trusted_proxy_sources=frozenset(("203.0.113.200",)), instance_nonce="d" * 32)
        thread = threading.Thread(target=server.serve_forever, daemon=True)
        thread.start()
        base = f"http://127.0.0.1:{server.server_port}"
        try:
            csrf_response = urllib.request.urlopen(urllib.request.Request(base + "/api/auth/csrf", headers={"Host": "portal.example.com"}))
            csrf = json.load(csrf_response)["csrf_token"]
            cookie = csrf_response.headers["Set-Cookie"]
            request = urllib.request.Request(base + "/api/auth/request-otp", data=b'{"email":"unknown@example.com"}', headers={"Host": "portal.example.com", "Origin": "https://portal.example.com", "X-Forwarded-For": "not-an-ip", "Content-Type": "application/json", "Cookie": cookie, "X-CSRF-Token": csrf})
            self.assertEqual(200, urllib.request.urlopen(request).status)
            self.assertIn("127.0.0.1", auth._ip_requests)
            self.assertNotIn("not-an-ip", auth._ip_requests)
        finally:
            server.shutdown()
            server.server_close()
            thread.join(timeout=2)


if __name__ == "__main__":
    unittest.main()
