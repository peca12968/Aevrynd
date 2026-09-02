"""Bounded Qwen health/models/slots/chat acceptance checks."""

from __future__ import annotations

import json
import socket
from dataclasses import dataclass
from typing import Any, Callable
from urllib import error, request


class AcceptanceError(RuntimeError):
    pass


class _NoRedirectHandler(request.HTTPRedirectHandler):
    # 驗收請求攜帶憑證，任何 redirect 都必須永久拒絕，不能讓 Authorization 離開 allowlist。
    def redirect_request(self, req, fp, code, msg, headers, newurl):  # type: ignore[no-untyped-def]
        return None


@dataclass(frozen=True, slots=True)
class ProbeResponse:
    status: int
    value: object


class QwenAcceptanceProbe:
    def __init__(self, base_url: str, expected_model: str, api_key: str | None, *, timeout: float = 5.0, opener: Callable[..., Any] | None = None) -> None:
        if base_url not in {"http://127.0.0.1:8080", "http://127.0.0.1:8081"}:
            raise ValueError("local Qwen probe URL is not allowlisted")
        if not expected_model or len(expected_model) > 512:
            raise ValueError("expected model is invalid")
        if api_key is not None and (len(api_key) < 16 or any(c in api_key for c in "\r\n\x00")):
            raise ValueError("Qwen API key is invalid")
        if not 0.5 <= timeout <= 30:
            raise ValueError("timeout is outside the allowed range")
        self.base_url = base_url
        self.expected_model = expected_model
        self.api_key = api_key
        self.timeout = timeout
        self._opener = request.build_opener(_NoRedirectHandler()).open if opener is None else opener

    def run(self) -> None:
        health = self._call("GET", "/health")
        if health.status != 200 or not isinstance(health.value, dict) or health.value.get("status") not in {"ok", "ready"}:
            raise AcceptanceError("Qwen health acceptance failed")
        models = self._call("GET", "/v1/models")
        data = models.value.get("data") if isinstance(models.value, dict) else None
        if models.status != 200 or not isinstance(data, list) or self.expected_model not in {str(item.get("id")) for item in data if isinstance(item, dict)}:
            raise AcceptanceError("Qwen models acceptance failed")
        slots = self._call("GET", "/slots")
        if slots.status != 200 or not isinstance(slots.value, list) or len(slots.value) != 1:
            raise AcceptanceError("Qwen single-slot acceptance failed")
        chat = self._call(
            "POST", "/v1/chat/completions",
            {"model": self.expected_model, "messages": [{"role": "user", "content": "Reply exactly READY"}], "max_tokens": 8, "temperature": 0},
        )
        choices = chat.value.get("choices") if isinstance(chat.value, dict) else None
        if chat.status != 200 or not isinstance(choices, list) or not choices or not isinstance(choices[0], dict):
            raise AcceptanceError("Qwen chat acceptance failed")
        message = choices[0].get("message")
        if not isinstance(message, dict) or not isinstance(message.get("content"), str) or not message["content"].strip():
            raise AcceptanceError("Qwen chat response is invalid")

    def _call(self, method: str, path: str, payload: object | None = None) -> ProbeResponse:
        body = None if payload is None else json.dumps(payload, separators=(",", ":")).encode("utf-8")
        headers = {"Accept": "application/json", "User-Agent": "Aevrynd-GameModeController/1"}
        if body is not None:
            headers["Content-Type"] = "application/json"
        if self.api_key:
            headers["Authorization"] = f"Bearer {self.api_key}"
        req = request.Request(self.base_url + path, data=body, headers=headers, method=method)
        try:
            with self._opener(req, timeout=self.timeout) as response:
                status = int(response.getcode())
                raw = response.read(262_145)
        except error.HTTPError as exc:
            exc.close()
            if 300 <= int(exc.code) < 400:
                raise AcceptanceError("Qwen redirect is permanently forbidden") from exc
            raise AcceptanceError(f"Qwen endpoint rejected acceptance check with HTTP {int(exc.code)}") from exc
        except (error.URLError, TimeoutError, socket.timeout, OSError) as exc:
            raise AcceptanceError("Qwen endpoint was unavailable during acceptance") from exc
        if len(raw) > 262_144:
            raise AcceptanceError("Qwen acceptance response is too large")
        try:
            value = json.loads(raw.decode("utf-8"))
        except (UnicodeError, json.JSONDecodeError) as exc:
            raise AcceptanceError("Qwen acceptance response is invalid JSON") from exc
        return ProbeResponse(status, value)
