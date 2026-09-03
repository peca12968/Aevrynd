"""提供零相依、預設僅回環位址的 Aevrynd 助理入口伺服器。"""
from __future__ import annotations

import argparse
import contextlib
import email.message
import hashlib
import hmac
import http.cookies
import http.server
import ipaddress
import json
import os
import queue
import re
import secrets
import smtplib
import socketserver
import ssl
import threading
import time
import urllib.error
import urllib.parse
import urllib.request
from collections import defaultdict, deque
from dataclasses import dataclass
from pathlib import Path
from typing import Protocol

ALLOWED_EMAIL = "slime12968@gmail.com"
OTP_ALPHABET = "23456789ABCDEFGHJKMNPQRSTUVWXYZ"
OTP_LENGTH = 8
OTP_TTL_SECONDS = 600
RESEND_COOLDOWN_SECONDS = 60
SESSION_TTL_SECONDS = 8 * 60 * 60
CSRF_TTL_SECONDS = 15 * 60
MAX_OTP_ATTEMPTS = 5
RATE_WINDOW_SECONDS = 600
REQUEST_RATE_LIMIT = 5
VERIFY_ACCOUNT_RATE_LIMIT = 10
VERIFY_IP_RATE_LIMIT = 30
MAX_OTP_RECORDS = 64
MAX_PENDING_REQUESTS = 64
MAX_CSRF_TOKENS = 256
MAX_SESSIONS = 64
MAX_RATE_KEYS = 256
MAX_DELIVERY_QUEUE = 32
GENERIC_REQUEST_MESSAGE = "若此信箱已獲核准，系統將寄送登入驗證資訊。"
GENERIC_VERIFY_MESSAGE = "驗證失敗或驗證碼已失效。"
PUBLIC_ASSETS = frozenset(("/styles.css", "/auth_adapter.js", "/auth_login.js"))
PRIVATE_ASSETS = frozenset(("/app.js",))


class EmailAdapter(Protocol):
    is_preview: bool
    def send_otp(self, email: str, otp: str) -> None: ...


class PreviewEmailAdapter:
    """僅供明確的本機開發模式；不會宣稱已寄信。"""
    is_preview = True
    def send_otp(self, email: str, otp: str) -> None:
        del email, otp


class SmtpEmailAdapter:
    """以 SMTP SSL 寄送 OTP；密鑰僅從伺服器環境載入。"""
    is_preview = False
    def __init__(self) -> None:
        self.host = os.environ.get("AEVRYND_SMTP_HOST", "")
        self.port = int(os.environ.get("AEVRYND_SMTP_PORT", "465"))
        self.username = os.environ.get("AEVRYND_SMTP_USERNAME", "")
        self.password = os.environ.get("AEVRYND_SMTP_PASSWORD", "")
        self.sender = os.environ.get("AEVRYND_SMTP_SENDER", self.username)
        if not all((self.host, self.username, self.password, self.sender)):
            raise RuntimeError("SMTP adapter configuration is incomplete.")

    def send_otp(self, email: str, otp: str) -> None:
        message = email_message(self.sender, email, otp)
        with smtplib.SMTP_SSL(self.host, self.port, context=ssl.create_default_context(), timeout=15) as client:
            client.login(self.username, self.password)
            client.send_message(message)


class ResendEmailAdapter:
    """以 Resend API 寄送 OTP；不將 API key 寫入 URL 或日誌。"""
    is_preview = False
    def __init__(self) -> None:
        self.api_key = os.environ.get("AEVRYND_RESEND_API_KEY", "")
        self.sender = os.environ.get("AEVRYND_RESEND_SENDER", "")
        if not self.api_key or not self.sender:
            raise RuntimeError("Resend adapter configuration is incomplete.")

    def send_otp(self, email: str, otp: str) -> None:
        body = json.dumps({"from": self.sender, "to": [email], "subject": "Aevrynd 登入驗證碼", "text": f"您的 Aevrynd 驗證碼是：{otp}\n此驗證碼 10 分鐘內有效。"}).encode("utf-8")
        request = urllib.request.Request("https://api.resend.com/emails", data=body, method="POST", headers={"Authorization": f"Bearer {self.api_key}", "Content-Type": "application/json"})
        try:
            with urllib.request.urlopen(request, timeout=15) as response:
                if response.status not in (200, 201):
                    raise RuntimeError("Resend delivery failed.")
        except urllib.error.URLError as error:
            raise RuntimeError("Resend delivery failed.") from error


def email_message(sender: str, recipient: str, otp: str) -> email.message.EmailMessage:
    message = email.message.EmailMessage()
    message["From"] = sender
    message["To"] = recipient
    message["Subject"] = "Aevrynd 登入驗證碼"
    message.set_content(f"您的 Aevrynd 驗證碼是：{otp}\n此驗證碼 10 分鐘內有效，且只能使用一次。")
    return message


@dataclass
class OtpRecord:
    salt: bytes
    digest: bytes
    expires_at: float
    attempts: int = 0


@dataclass
class PendingRequest:
    reservation_id: str
    expires_at: float


@dataclass
class DeliveryJob:
    reservation_id: str
    email: str
    otp: str
    record: OtpRecord
    allowed: bool


class AuthManager:
    """管理 bounded、具 TTL 的驗證狀態，且不保存明文 OTP。"""
    def __init__(self, adapter: EmailAdapter, *, dev_show_otp: bool = False, clock=time.monotonic) -> None:
        if dev_show_otp and not adapter.is_preview:
            raise RuntimeError("OTP display is only available with the preview adapter.")
        self.adapter = adapter
        self.dev_show_otp = dev_show_otp
        self.clock = clock
        self._otp_records: dict[str, OtpRecord] = {}
        self._pending: dict[str, PendingRequest] = {}
        self._cooldowns: dict[str, float] = {}
        self._account_requests: dict[str, deque[float]] = defaultdict(deque)
        self._ip_requests: dict[str, deque[float]] = defaultdict(deque)
        self._verify_accounts: dict[str, deque[float]] = defaultdict(deque)
        self._verify_ips: dict[str, deque[float]] = defaultdict(deque)
        self._csrf_tokens: dict[str, float] = {}
        self._sessions: dict[bytes, float] = {}
        self._lock = threading.RLock()
        self._delivery_queue: queue.Queue[DeliveryJob | None] | None = None
        self._delivery_thread: threading.Thread | None = None
        if not adapter.is_preview:
            self._delivery_queue = queue.Queue(MAX_DELIVERY_QUEUE)
            self._delivery_thread = threading.Thread(target=self._delivery_loop, name="AevryndEmailDelivery", daemon=True)
            self._delivery_thread.start()

    @staticmethod
    def normalize_email(value: str) -> str:
        return value.strip().lower()

    @staticmethod
    def generate_otp() -> str:
        return "".join(secrets.choice(OTP_ALPHABET) for _ in range(OTP_LENGTH))

    @staticmethod
    def _hash_otp(salt: bytes, otp: str) -> bytes:
        return hashlib.pbkdf2_hmac("sha256", otp.encode("ascii"), salt, 120_000)

    @staticmethod
    def _trim_map(mapping: dict, capacity: int) -> None:
        while len(mapping) > capacity:
            mapping.pop(next(iter(mapping)))

    def _clean_locked(self, now: float) -> None:
        self._otp_records = {key: value for key, value in self._otp_records.items() if value.expires_at > now}
        self._pending = {key: value for key, value in self._pending.items() if value.expires_at > now}
        self._cooldowns = {key: value for key, value in self._cooldowns.items() if value > now}
        self._csrf_tokens = {key: value for key, value in self._csrf_tokens.items() if value > now}
        self._sessions = {key: value for key, value in self._sessions.items() if value > now}
        for mapping in (self._account_requests, self._ip_requests, self._verify_accounts, self._verify_ips):
            for key in list(mapping):
                history = mapping[key]
                while history and history[0] <= now - RATE_WINDOW_SECONDS:
                    history.popleft()
                if not history:
                    del mapping[key]
            self._trim_map(mapping, MAX_RATE_KEYS)
        for mapping, capacity in ((self._otp_records, MAX_OTP_RECORDS), (self._pending, MAX_PENDING_REQUESTS), (self._csrf_tokens, MAX_CSRF_TOKENS), (self._sessions, MAX_SESSIONS)):
            self._trim_map(mapping, capacity)

    @staticmethod
    def _limited(mapping: dict[str, deque[float]], key: str, limit: int, now: float) -> bool:
        history = mapping[key]
        if len(history) >= limit:
            return True
        history.append(now)
        return False

    def issue_csrf(self) -> str:
        token = secrets.token_urlsafe(32)
        with self._lock:
            now = self.clock()
            self._clean_locked(now)
            self._csrf_tokens[token] = now + CSRF_TTL_SECONDS
            self._trim_map(self._csrf_tokens, MAX_CSRF_TOKENS)
        return token

    def validate_csrf(self, cookie_token: str, header_token: str) -> bool:
        with self._lock:
            now = self.clock()
            self._clean_locked(now)
            return self._csrf_tokens.get(cookie_token, 0) > now and bool(header_token) and secrets.compare_digest(cookie_token, header_token)

    def request_otp(self, email: str, client_ip: str) -> tuple[str | None, int, bool]:
        normalized = self.normalize_email(email)
        now = self.clock()
        reservation_id = secrets.token_urlsafe(18)
        with self._lock:
            self._clean_locked(now)
            account_limited = self._limited(self._account_requests, normalized, REQUEST_RATE_LIMIT, now)
            ip_limited = self._limited(self._ip_requests, client_ip, REQUEST_RATE_LIMIT, now)
            cooldown_until = self._cooldowns.get(normalized, 0)
            if account_limited or ip_limited or cooldown_until > now or normalized in self._pending:
                return None, max(1, int(max(cooldown_until - now, RESEND_COOLDOWN_SECONDS))), False
            self._cooldowns[normalized] = now + RESEND_COOLDOWN_SECONDS
            self._pending[normalized] = PendingRequest(reservation_id, now + RESEND_COOLDOWN_SECONDS)
            self._trim_map(self._pending, MAX_PENDING_REQUESTS)
            self._trim_map(self._cooldowns, MAX_RATE_KEYS)
            self._trim_map(self._account_requests, MAX_RATE_KEYS)
            self._trim_map(self._ip_requests, MAX_RATE_KEYS)
        otp = self.generate_otp()
        salt = secrets.token_bytes(16)
        record = OtpRecord(salt, self._hash_otp(salt, otp), now + OTP_TTL_SECONDS)
        allowed = hmac.compare_digest(normalized, ALLOWED_EMAIL)
        job = DeliveryJob(reservation_id, normalized, otp, record, allowed)
        if self.adapter.is_preview:
            self._finish_delivery(job, succeeded=True)
            shown = otp if allowed and self.dev_show_otp else None
            return shown, RESEND_COOLDOWN_SECONDS, False
        assert self._delivery_queue is not None
        try:
            self._delivery_queue.put_nowait(job)
        except queue.Full:
            self._finish_delivery(job, succeeded=False)
            return None, RESEND_COOLDOWN_SECONDS, True
        return None, RESEND_COOLDOWN_SECONDS, False

    def _delivery_loop(self) -> None:
        assert self._delivery_queue is not None
        while True:
            job = self._delivery_queue.get()
            if job is None:
                self._delivery_queue.task_done()
                return
            succeeded = True
            try:
                if job.allowed:
                    self.adapter.send_otp(job.email, job.otp)
            except Exception:
                succeeded = False
            self._finish_delivery(job, succeeded)
            self._delivery_queue.task_done()

    def _finish_delivery(self, job: DeliveryJob, succeeded: bool) -> None:
        with self._lock:
            current = self._pending.get(job.email)
            if not current or current.reservation_id != job.reservation_id:
                return
            self._pending.pop(job.email, None)
            if job.allowed and succeeded:
                self._otp_records[job.email] = job.record
                self._trim_map(self._otp_records, MAX_OTP_RECORDS)
            elif not succeeded:
                self._cooldowns.pop(job.email, None)

    def close(self) -> None:
        if self._delivery_queue is None or self._delivery_thread is None:
            return
        with contextlib.suppress(queue.Full):
            self._delivery_queue.put_nowait(None)
        self._delivery_thread.join(timeout=2)

    def verify_otp(self, email: str, otp: str, client_ip: str) -> tuple[bool, str | None]:
        normalized = self.normalize_email(email)
        now = self.clock()
        with self._lock:
            self._clean_locked(now)
            account_limited = self._limited(self._verify_accounts, normalized, VERIFY_ACCOUNT_RATE_LIMIT, now)
            ip_limited = self._limited(self._verify_ips, client_ip, VERIFY_IP_RATE_LIMIT, now)
            self._trim_map(self._verify_accounts, MAX_RATE_KEYS)
            self._trim_map(self._verify_ips, MAX_RATE_KEYS)
            if account_limited or ip_limited:
                return False, None
            record = self._otp_records.get(normalized)
            if not record:
                return False, None
            record.attempts += 1
            valid_shape = len(otp) == OTP_LENGTH and all(character in OTP_ALPHABET for character in otp)
            matches = valid_shape and hmac.compare_digest(record.digest, self._hash_otp(record.salt, otp))
            if not matches:
                if record.attempts >= MAX_OTP_ATTEMPTS:
                    self._otp_records.pop(normalized, None)
                return False, None
            self._otp_records.pop(normalized, None)
            token = secrets.token_urlsafe(32)
            self._sessions[hashlib.sha256(token.encode("ascii")).digest()] = now + SESSION_TTL_SECONDS
            self._trim_map(self._sessions, MAX_SESSIONS)
            return True, token

    def validate_session(self, token: str) -> bool:
        if not token:
            return False
        digest = hashlib.sha256(token.encode("ascii")).digest()
        with self._lock:
            now = self.clock()
            self._clean_locked(now)
            return self._sessions.get(digest, 0) > now

    def logout(self, token: str) -> None:
        if token:
            with self._lock:
                self._sessions.pop(hashlib.sha256(token.encode("ascii")).digest(), None)


class PortalServer(socketserver.ThreadingMixIn, http.server.HTTPServer):
    daemon_threads = True
    allow_reuse_address = True
    def __init__(self, address: tuple[str, int], root: Path, auth: AuthManager, production: bool, *, public_host: str = "", trusted_proxy_sources: frozenset[str] = frozenset()) -> None:
        self.portal_root = root
        self.auth = auth
        self.production = production
        self.public_host = public_host.lower()
        self.trusted_proxy_sources = trusted_proxy_sources
        super().__init__(address, PortalHandler)
        if production:
            self.allowed_hosts = frozenset((self.public_host,))
            self.allowed_origins = frozenset((f"https://{self.public_host}",))
        else:
            self.allowed_hosts = frozenset((f"127.0.0.1:{self.server_port}", f"localhost:{self.server_port}"))
            self.allowed_origins = frozenset((f"http://127.0.0.1:{self.server_port}", f"http://localhost:{self.server_port}"))

    def server_close(self) -> None:
        self.auth.close()
        super().server_close()


class PortalHandler(http.server.BaseHTTPRequestHandler):
    server: PortalServer
    def log_message(self, format: str, *args: object) -> None:
        # 僅記錄完成事件，不記錄路徑、內容、OTP、token、Cookie 或密鑰。
        del format, args
        print(f"[portal] request from {self.client_address[0]} completed")

    def do_GET(self) -> None:
        path = self._safe_path()
        if path is None:
            return
        if path == "/":
            self._send_html(self._render_template(authenticated=False))
        elif path == "/portal":
            if not self._require_session():
                return
            self._send_html(self._render_template(authenticated=True))
        elif path in PUBLIC_ASSETS:
            self._send_asset(path)
        elif path in PRIVATE_ASSETS:
            if self._require_session():
                self._send_asset(path)
        elif path == "/api/auth/csrf":
            token = self.server.auth.issue_csrf()
            self._send_json(200, {"csrf_token": token}, [self._cookie("aevrynd_csrf", token, http_only=False)])
        elif path.startswith("/api/"):
            if self._require_session():
                self._send_json(404, {"message": "Not found."})
        else:
            self._send_json(404, {"message": "Not found."})

    def do_POST(self) -> None:
        path = self._safe_path()
        if path is None:
            return
        if not self._require_origin():
            return
        if path == "/api/auth/logout":
            if not self._require_csrf() or not self._require_session():
                return
            self.server.auth.logout(self._read_cookie("aevrynd_session"))
            self._send_json(200, {"message": "Signed out."}, [self._cookie("aevrynd_session", "", True, max_age=0)])
            return
        if path not in ("/api/auth/request-otp", "/api/auth/verify-otp"):
            if path.startswith("/api/") and not self._require_session():
                return
            self._send_json(404, {"message": "Not found."})
            return
        if not self._require_csrf():
            return
        client_ip = self._client_ip()
        if client_ip is None:
            return
        body = self._read_json()
        if body is None:
            return
        email_value = body.get("email")
        if not isinstance(email_value, str) or not 3 <= len(email_value) <= 254:
            self._send_json(200, {"message": GENERIC_REQUEST_MESSAGE if path.endswith("request-otp") else GENERIC_VERIFY_MESSAGE, "authenticated": False})
            return
        if path.endswith("request-otp"):
            preview_code, retry_after, _delivery_failed = self.server.auth.request_otp(email_value, client_ip)
            payload: dict[str, object] = {"message": GENERIC_REQUEST_MESSAGE, "retry_after": retry_after, "preview": self.server.auth.adapter.is_preview}
            if preview_code is not None:
                payload["preview_code"] = preview_code
            # 寄送失敗仍使用相同通用回應，避免藉狀態碼枚舉核准帳號。
            self._send_json(200, payload)
            return
        otp_value = body.get("otp")
        otp = otp_value if isinstance(otp_value, str) else ""
        authenticated, session_token = self.server.auth.verify_otp(email_value, otp, client_ip)
        cookies = [self._cookie("aevrynd_session", session_token, True, max_age=SESSION_TTL_SECONDS)] if session_token else []
        self._send_json(200, {"message": "驗證成功。" if authenticated else GENERIC_VERIFY_MESSAGE, "authenticated": authenticated}, cookies)

    def _safe_path(self) -> str | None:
        host_values = self.headers.get_all("Host", failobj=[])
        if len(host_values) != 1 or host_values[0].strip().lower() not in self.server.allowed_hosts or host_values[0] != host_values[0].strip():
            self._send_json(421, {"message": "Invalid host."})
            return None
        parsed = urllib.parse.urlsplit(self.path)
        decoded = urllib.parse.unquote(parsed.path)
        if parsed.query or parsed.fragment or ".." in decoded or "\\" in decoded or any(part.startswith(".") for part in decoded.split("/") if part):
            self._send_json(404, {"message": "Not found."})
            return None
        return decoded

    def _require_origin(self) -> bool:
        origin_values = self.headers.get_all("Origin", failobj=[])
        if len(origin_values) == 1 and origin_values[0] in self.server.allowed_origins:
            return True
        self._send_json(403, {"message": "Invalid request origin."})
        return False

    def _client_ip(self) -> str | None:
        try:
            remote = str(ipaddress.ip_address(self.client_address[0]))
        except ValueError:
            self._send_json(400, {"message": "Invalid client address."})
            return None
        forwarded_values = self.headers.get_all("Forwarded", failobj=[])
        xff_values = self.headers.get_all("X-Forwarded-For", failobj=[])
        if remote not in self.server.trusted_proxy_sources:
            return remote
        if len(forwarded_values) + len(xff_values) != 1:
            self._send_json(400, {"message": "Invalid forwarding headers."})
            return None
        raw = forwarded_values[0] if forwarded_values else xff_values[0]
        if raw != raw.strip() or "," in raw or "\r" in raw or "\n" in raw:
            self._send_json(400, {"message": "Invalid forwarding headers."})
            return None
        candidate = raw
        if forwarded_values:
            match = re.fullmatch(r'for=(?:"\[([^\]]+)\]"|"([^"]+)"|([^;\s]+))', raw, re.IGNORECASE)
            if not match:
                self._send_json(400, {"message": "Invalid forwarding headers."})
                return None
            candidate = next(value for value in match.groups() if value is not None)
        try:
            return str(ipaddress.ip_address(candidate))
        except ValueError:
            self._send_json(400, {"message": "Invalid forwarding headers."})
            return None

    def _render_template(self, authenticated: bool) -> bytes:
        source = (self.server.portal_root / "index.html").read_text(encoding="utf-8")
        if authenticated:
            source = remove_marked(source, "登入")
            source = remove_marked(source, "登入腳本")
            source = source.replace('href="#auth-screen"', 'href="#main-content"').replace('id="portal-shell" hidden', 'id="portal-shell"')
        else:
            source = remove_marked(source, "控制台")
            source = remove_marked(source, "控制台腳本")
        return source.encode("utf-8")

    def _send_asset(self, path: str) -> None:
        asset_name = path.removeprefix("/")
        content = (self.server.portal_root / asset_name).read_bytes()
        media_type = "text/css; charset=utf-8" if path.endswith(".css") else "text/javascript; charset=utf-8"
        self.send_response(200)
        self.send_header("Content-Type", media_type)
        self.send_header("Content-Length", str(len(content)))
        self._security_headers()
        self.end_headers()
        self.wfile.write(content)

    def _require_session(self) -> bool:
        if self.server.auth.validate_session(self._read_cookie("aevrynd_session")):
            return True
        self._send_json(401, {"message": "Authentication required."})
        return False

    def _require_csrf(self) -> bool:
        if self.server.auth.validate_csrf(self._read_cookie("aevrynd_csrf"), self.headers.get("X-CSRF-Token", "")):
            return True
        self._send_json(403, {"message": "Invalid request."})
        return False

    def _read_json(self) -> dict[str, object] | None:
        try:
            length = int(self.headers.get("Content-Length", "0"))
        except ValueError:
            length = 0
        if not 1 <= length <= 4096 or self.headers.get_content_type() != "application/json":
            self._send_json(400, {"message": "Invalid request."})
            return None
        try:
            value = json.loads(self.rfile.read(length))
        except (json.JSONDecodeError, UnicodeDecodeError):
            self._send_json(400, {"message": "Invalid request."})
            return None
        if not isinstance(value, dict):
            self._send_json(400, {"message": "Invalid request."})
            return None
        return value

    def _read_cookie(self, name: str) -> str:
        jar = http.cookies.SimpleCookie()
        with contextlib.suppress(http.cookies.CookieError):
            jar.load(self.headers.get("Cookie", ""))
        return jar[name].value if name in jar else ""

    def _cookie(self, name: str, value: str | None, http_only: bool, max_age: int | None = None) -> str:
        parts = [f"{name}={value or ''}", "Path=/", "SameSite=Strict"]
        if http_only:
            parts.append("HttpOnly")
        if max_age is not None:
            parts.append(f"Max-Age={max_age}")
        if self.server.production:
            parts.append("Secure")
        return "; ".join(parts)

    def _security_headers(self) -> None:
        self.send_header("X-Content-Type-Options", "nosniff")
        self.send_header("Referrer-Policy", "no-referrer")
        self.send_header("Content-Security-Policy", "default-src 'self'; style-src 'self' 'unsafe-inline'; script-src 'self'; connect-src 'self'; img-src 'self' data:; object-src 'none'; base-uri 'none'; frame-ancestors 'none'; form-action 'self'")
        self.send_header("Cache-Control", "no-store")

    def _send_html(self, content: bytes) -> None:
        self.send_response(200)
        self.send_header("Content-Type", "text/html; charset=utf-8")
        self.send_header("Content-Length", str(len(content)))
        self._security_headers()
        self.end_headers()
        self.wfile.write(content)

    def _send_json(self, status: int, payload: dict[str, object], cookies: list[str] | None = None) -> None:
        content = json.dumps(payload, ensure_ascii=False, separators=(",", ":")).encode("utf-8")
        self.send_response(status)
        self.send_header("Content-Type", "application/json; charset=utf-8")
        self.send_header("Content-Length", str(len(content)))
        for cookie in cookies or []:
            self.send_header("Set-Cookie", cookie)
        self._security_headers()
        self.end_headers()
        self.wfile.write(content)


def remove_marked(source: str, label: str) -> str:
    start_marker = f"<!-- {label}開始 -->"
    end_marker = f"<!-- {label}結束 -->"
    start = source.find(start_marker)
    end = source.find(end_marker)
    if start < 0 or end < start:
        raise RuntimeError(f"Template marker {label} is invalid.")
    return source[:start] + source[end + len(end_marker):]


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Aevrynd Assistant Portal server")
    parser.add_argument("--host", default="127.0.0.1", choices=("127.0.0.1", "localhost"))
    parser.add_argument("--port", default=8787, type=int)
    parser.add_argument("--production", action="store_true")
    parser.add_argument("--public-host", default="", help="Exact public hostname accepted in production")
    parser.add_argument("--trusted-proxy-source", action="append", default=[], help="Exact proxy source IP allowed to supply one Forwarded or X-Forwarded-For value")
    parser.add_argument("--dev-show-otp", action="store_true", help="Expose OTP only for the allowlisted account in loopback preview")
    return parser.parse_args()


def create_email_adapter() -> EmailAdapter:
    name = os.environ.get("AEVRYND_AUTH_ADAPTER", "preview").strip().lower()
    adapters = {"preview": PreviewEmailAdapter, "smtp": SmtpEmailAdapter, "resend": ResendEmailAdapter}
    if name not in adapters:
        raise RuntimeError("Unknown auth adapter.")
    return adapters[name]()


def main() -> int:
    args = parse_args()
    if not 1024 <= args.port <= 65535:
        raise SystemExit("Port must be between 1024 and 65535.")
    adapter = create_email_adapter()
    if args.production and adapter.is_preview:
        raise SystemExit("Production requires a configured SMTP or Resend adapter.")
    if args.production and args.dev_show_otp:
        raise SystemExit("Production cannot expose OTP values.")
    if args.production and (not re.fullmatch(r"[a-z0-9](?:[a-z0-9.-]{0,251}[a-z0-9])?", args.public_host.lower()) or not args.trusted_proxy_source):
        raise SystemExit("Production requires an exact public host and at least one trusted proxy source.")
    try:
        trusted_proxies = frozenset(str(ipaddress.ip_address(value)) for value in args.trusted_proxy_source)
    except ValueError as error:
        raise SystemExit("Trusted proxy sources must be exact IP addresses.") from error
    root = Path(__file__).resolve().parent
    auth = AuthManager(adapter, dev_show_otp=args.dev_show_otp)
    with PortalServer((args.host, args.port), root, auth, args.production, public_host=args.public_host, trusted_proxy_sources=trusted_proxies) as server:
        print(f"Aevrynd Assistant Portal: http://{args.host}:{args.port}")
        with contextlib.suppress(KeyboardInterrupt):
            server.serve_forever()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
