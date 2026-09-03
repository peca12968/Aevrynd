"""Bounded OpenAI-compatible client for the LAN Qwen service."""

from __future__ import annotations

import json
import socket
import threading
from contextlib import contextmanager
from dataclasses import dataclass
from typing import Any, Callable, Iterator
from urllib import error, parse, request


DEFAULT_ALLOWED_BASE_URL = "http://192.168.50.98:8080/v1"
SECONDARY_ALLOWED_BASE_URL = "http://100.89.42.36:8080/v1"
NO_THINK_DIRECTIVE = "/no_think\n"


class QwenClientError(RuntimeError):
    retryable = False


class RetryableQwenError(QwenClientError):
    retryable = True


class InvalidQwenResponse(QwenClientError):
    retryable = False


class _NoRedirectHandler(request.HTTPRedirectHandler):
    def redirect_request(self, *_args: Any, **_kwargs: Any) -> None:
        # Authorization 不得被 30x 帶往未列入 allowlist 的目的地。
        return None


def _open_without_redirects(api_request: request.Request, *, timeout: float):
    return request.build_opener(_NoRedirectHandler()).open(api_request, timeout=timeout)


@dataclass(frozen=True, slots=True)
class QwenReply:
    text: str
    model: str


def _normalize_base_url(value: str) -> str:
    parsed = parse.urlsplit(value)
    if (
        parsed.scheme != "http"
        or parsed.hostname is None
        or parsed.username is not None
        or parsed.password is not None
        or parsed.query
        or parsed.fragment
        or parsed.path.rstrip("/") != "/v1"
    ):
        raise ValueError("qwen base URL is not allowed")
    port = parsed.port
    if (parsed.hostname, port) not in {("192.168.50.98", 8080), ("100.89.42.36", 8080)}:
        raise ValueError("qwen base URL is not allowlisted")
    return f"http://{parsed.hostname}:{port}/v1"


class QwenClient:
    def __init__(
        self,
        base_url: str,
        api_key: str,
        model: str,
        *,
        timeout: float = 120.0,
        max_input_chars: int = 16_384,
        max_output_chars: int = 16_384,
        max_tokens: int = 1_024,
        max_response_bytes: int = 262_144,
        opener: Callable[..., Any] | None = None,
    ) -> None:
        self._base_url = _normalize_base_url(base_url)
        self._origin = self._base_url[:-3]
        if not api_key or len(api_key) > 4_096:
            raise ValueError("api_key is invalid")
        if not model or len(model) > 256:
            raise ValueError("model is invalid")
        if timeout <= 0 or timeout > 300:
            raise ValueError("timeout is outside the allowed range")
        if not 1 <= max_tokens <= 4_096:
            raise ValueError("max_tokens is outside the allowed range")
        if not 256 <= max_input_chars <= 65_536:
            raise ValueError("max_input_chars is outside the allowed range")
        if not 256 <= max_output_chars <= 65_536:
            raise ValueError("max_output_chars is outside the allowed range")
        if not 1_024 <= max_response_bytes <= 1_048_576:
            raise ValueError("max_response_bytes is outside the allowed range")
        self._api_key = api_key
        self._model = model
        self._timeout = float(timeout)
        self._max_input_chars = int(max_input_chars)
        self._max_output_chars = int(max_output_chars)
        self._max_tokens = int(max_tokens)
        self._max_response_bytes = int(max_response_bytes)
        self._opener = _open_without_redirects if opener is None else opener

    def list_models(self) -> tuple[str, ...]:
        payload = self._request("GET", "/models", None)
        data = payload.get("data")
        if not isinstance(data, list) or len(data) > 128:
            raise InvalidQwenResponse("Qwen model list has an invalid shape")
        models: list[str] = []
        for item in data:
            if not isinstance(item, dict) or not isinstance(item.get("id"), str):
                raise InvalidQwenResponse("Qwen model entry has an invalid shape")
            model_id = item["id"]
            if not model_id or len(model_id) > 256:
                raise InvalidQwenResponse("Qwen model identifier is invalid")
            models.append(model_id)
        return tuple(models)

    def chat(self, prompt: str, *, system_prompt: str = "") -> QwenReply:
        if not isinstance(prompt, str) or not prompt or len(prompt) > self._max_input_chars:
            raise ValueError("prompt is outside the allowed range")
        if not isinstance(system_prompt, str) or len(system_prompt) > self._max_input_chars:
            raise ValueError("system_prompt is outside the allowed range")
        messages: list[dict[str, str]] = []
        if system_prompt:
            messages.append({"role": "system", "content": system_prompt})
        # Qwen reasoning 模式可能耗盡輸出額度而留下空 content；簡單助理固定停用思考模式。
        messages.append({"role": "user", "content": NO_THINK_DIRECTIVE + prompt})
        payload = self._request(
            "POST",
            "/chat/completions",
            {
                "model": self._model,
                "messages": messages,
                "max_tokens": self._max_tokens,
                "temperature": 0.2,
            },
        )
        choices = payload.get("choices")
        if not isinstance(choices, list) or len(choices) != 1:
            raise InvalidQwenResponse("Qwen response must contain exactly one choice")
        choice = choices[0]
        if not isinstance(choice, dict) or not isinstance(choice.get("message"), dict):
            raise InvalidQwenResponse("Qwen response choice is invalid")
        content = choice["message"].get("content")
        if not isinstance(content, str) or not content or len(content) > self._max_output_chars:
            raise InvalidQwenResponse("Qwen response content is invalid or oversized")
        returned_model = payload.get("model", self._model)
        if not isinstance(returned_model, str) or len(returned_model) > 256:
            raise InvalidQwenResponse("Qwen response model is invalid")
        return QwenReply(content, returned_model)

    def slots_ready(self) -> bool:
        payload = self._request("GET", "/slots", None)
        slots: object = payload
        if isinstance(payload, dict):
            has_slots = "slots" in payload
            has_data = "data" in payload
            if has_slots == has_data:
                return False
            slots = payload["slots" if has_slots else "data"]
        if not isinstance(slots, list) or not 1 <= len(slots) <= 128:
            return False
        if any(
            not isinstance(item, dict)
            or not isinstance(item.get("is_processing"), bool)
            for item in slots
        ):
            return False
        return any(item["is_processing"] is False for item in slots)

    def _request(self, method: str, path: str, payload: dict[str, object] | None) -> Any:
        if path not in {"/models", "/chat/completions", "/slots"}:
            raise ValueError("Qwen endpoint is not allowed")
        body = None
        if payload is not None:
            body = json.dumps(payload, ensure_ascii=False, separators=(",", ":")).encode("utf-8")
        headers = {
            "Authorization": f"Bearer {self._api_key}",
            "Accept": "application/json",
            "User-Agent": "Aevrynd-Line-Gateway/1",
        }
        if body is not None:
            headers["Content-Type"] = "application/json"
        request_url = self._origin + path if path == "/slots" else self._base_url + path
        api_request = request.Request(request_url, data=body, headers=headers, method=method)
        try:
            with self._opener(api_request, timeout=self._timeout) as response:
                status = int(response.getcode())
                raw = response.read(self._max_response_bytes + 1)
        except error.HTTPError as exc:
            status = int(exc.code)
            if 500 <= status < 600 or status in {408, 429}:
                raise RetryableQwenError(f"Qwen service returned HTTP {status}") from exc
            raise QwenClientError(f"Qwen request was rejected with HTTP {status}") from exc
        except (error.URLError, TimeoutError, socket.timeout, OSError) as exc:
            # 不附帶 request 物件，避免 API key 出現在例外文字。
            raise RetryableQwenError("Qwen request failed before a response") from exc
        if not 200 <= status < 300:
            raise InvalidQwenResponse("Qwen response status is invalid")
        if len(raw) > self._max_response_bytes:
            raise InvalidQwenResponse("Qwen response exceeds max_response_bytes")
        try:
            decoded = json.loads(raw.decode("utf-8"))
        except (UnicodeDecodeError, json.JSONDecodeError) as exc:
            raise InvalidQwenResponse("Qwen response is not valid JSON") from exc
        if path == "/slots" and isinstance(decoded, list):
            return decoded
        if not isinstance(decoded, dict):
            raise InvalidQwenResponse("Qwen response root must be an object")
        return decoded


class QwenClientGate:
    """Serialize every operation targeting one physical single-slot Qwen server."""

    def __init__(self, client: QwenClient) -> None:
        self._client = client
        self._lock = threading.RLock()

    @contextmanager
    def session(
        self,
        *,
        owner_check: Callable[[], None] | None = None,
        acquire_timeout: float | None = None,
    ) -> Iterator["QwenClientGate"]:
        # 同一節點的 slots 檢查與 chat 必須保持原子，避免兩條 lane 同時占用單槽。
        if acquire_timeout is not None:
            if acquire_timeout <= 0 or acquire_timeout > 1:
                raise ValueError("acquire_timeout is outside the allowed range")
            if not self._lock.acquire(timeout=acquire_timeout):
                raise RetryableQwenError("Qwen node is already assigned")
        else:
            while not self._lock.acquire(timeout=0.1):
                if owner_check is not None:
                    owner_check()
        try:
            if owner_check is not None:
                owner_check()
            yield self
        finally:
            self._lock.release()

    def chat(self, prompt: str, *, system_prompt: str = "") -> QwenReply:
        with self._lock:
            return self._client.chat(prompt, system_prompt=system_prompt)

    def slots_ready(self) -> bool:
        with self._lock:
            return self._client.slots_ready()


class QwenClientPool:
    """Primary-first single-slot pool with bounded secondary failover."""

    def __init__(self, primary: QwenClientGate, secondary: QwenClientGate | None = None) -> None:
        self._primary = primary
        self._secondary = secondary

    def chat(self, prompt: str, *, system_prompt: str = "") -> QwenReply:
        try:
            return self._primary.chat(prompt, system_prompt=system_prompt)
        except RetryableQwenError:
            if self._secondary is None:
                raise
            if isinstance(self._secondary, QwenClientGate):
                with self._secondary.session():
                    if not self._secondary.slots_ready():
                        raise
                    return self._secondary.chat(prompt, system_prompt=system_prompt)
            if not self._secondary.slots_ready():
                raise
            return self._secondary.chat(prompt, system_prompt=system_prompt)
