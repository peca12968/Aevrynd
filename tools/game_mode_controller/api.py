"""Strict transport-independent hub API dispatcher."""

from __future__ import annotations

import json
from dataclasses import dataclass
from typing import Callable, Mapping

from .coordinator import HubCoordinator
from .models import GameModeState
from .security import AuthenticationError, RequestAuthenticator, sign_response


@dataclass(frozen=True, slots=True)
class ApiResponse:
    status: int
    headers: dict[str, str]
    body: bytes


class HubApi:
    def __init__(self, coordinator: HubCoordinator, authenticator: RequestAuthenticator, secret_reader: Callable[[str], str | None]) -> None:
        self.coordinator = coordinator
        self.authenticator = authenticator
        self.secret_reader = secret_reader

    def dispatch(self, method: str, path: str, headers: Mapping[str, str], body: bytes) -> ApiResponse:
        if method not in {"GET", "POST"} or path not in {"/v1/status", "/v1/command", "/v1/poll", "/v1/ack"}:
            return ApiResponse(404, {"Content-Type": "application/json"}, b'{"status":"not-found"}')
        if len(body) > 262_144:
            return ApiResponse(413, {"Content-Type": "application/json"}, b'{"status":"too-large"}')
        try:
            auth = self.authenticator.verify(method, path, headers, body)
            payload = self._parse_body(body)
            result = self._route(auth.node_id, method, path, payload)
            return self._signed(200, auth.node_id, auth.nonce, result)
        except AuthenticationError:
            return ApiResponse(401, {"Content-Type": "application/json"}, b'{"status":"unauthorized"}')
        except (ValueError, KeyError, TypeError):
            return ApiResponse(400, {"Content-Type": "application/json"}, b'{"status":"invalid-request"}')

    @staticmethod
    def _parse_body(body: bytes) -> dict[str, object]:
        if not body:
            return {}
        try:
            value = json.loads(body.decode("utf-8"), object_pairs_hook=_unique_object)
        except (UnicodeError, json.JSONDecodeError, ValueError) as error:
            raise ValueError("request JSON is invalid") from error
        if not isinstance(value, dict):
            raise ValueError("request JSON must be an object")
        return value

    def _route(self, actor: str, method: str, path: str, payload: dict[str, object]) -> object:
        if (method, path) == ("GET", "/v1/status"):
            if payload:
                raise ValueError("status body must be empty")
            return {key: value.to_dict() for key, value in self.coordinator.snapshot().items()}
        if (method, path) == ("POST", "/v1/status"):
            if set(payload) != {"state"} or not isinstance(payload["state"], dict):
                raise ValueError("status schema is invalid")
            self.coordinator.report(actor, GameModeState.from_dict(payload["state"]))
            return {"accepted": True}
        if (method, path) == ("POST", "/v1/command"):
            if set(payload) != {"target_node_id", "game_mode"}:
                raise ValueError("command schema is invalid")
            command = self.coordinator.issue(actor, str(payload["target_node_id"]), payload["game_mode"])  # type: ignore[arg-type]
            return command.to_dict()
        if (method, path) == ("GET", "/v1/poll"):
            if payload:
                raise ValueError("poll body must be empty")
            command = self.coordinator.poll(actor)
            return None if command is None else command.to_dict()
        if (method, path) == ("POST", "/v1/ack"):
            if set(payload) != {"command_id", "state"} or not isinstance(payload["state"], dict):
                raise ValueError("ack schema is invalid")
            self.coordinator.acknowledge(actor, str(payload["command_id"]), GameModeState.from_dict(payload["state"]))
            return {"accepted": True}
        raise ValueError("route is invalid")

    def _signed(self, status: int, node_id: str, request_nonce: str, payload: object) -> ApiResponse:
        body = json.dumps({"schema_version": 1, "payload": payload}, ensure_ascii=False, sort_keys=True, separators=(",", ":")).encode("utf-8")
        secret = self.secret_reader(node_id)
        if secret is None:
            raise AuthenticationError("node credential is unavailable")
        signature = sign_response(secret, node_id, request_nonce, body)
        return ApiResponse(status, {"Content-Type": "application/json", "X-Aevrynd-Response-Signature": signature}, body)


def _unique_object(pairs: list[tuple[str, object]]) -> dict[str, object]:
    result: dict[str, object] = {}
    for key, value in pairs:
        if key in result:
            raise ValueError("duplicate JSON key")
        result[key] = value
    return result
