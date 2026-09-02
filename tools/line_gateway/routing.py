"""Authenticated command routing for LINE webhook events."""

from __future__ import annotations

from dataclasses import dataclass
from enum import Enum
from typing import Mapping

from .config import GatewayConfig


class Route(str, Enum):
    HOST = "host"
    QWEN = "qwen"
    CODEX = "codex"
    ALL = "all"
    HELP = "help"
    IGNORE = "ignore"


@dataclass(frozen=True, slots=True)
class RouteDecision:
    route: Route
    authorized: bool
    original_text: str
    command_text: str
    reason: str


_PREFIXES = (
    ("@主機", Route.HOST),
    ("@Qwen", Route.QWEN),
    ("@Codex", Route.CODEX),
    ("@全部", Route.ALL),
)


def _ignored(reason: str, original_text: str = "") -> RouteDecision:
    return RouteDecision(Route.IGNORE, False, original_text, "", reason)


def _authorize_source(
    source: Mapping[str, object], config: GatewayConfig
) -> tuple[bool, str]:
    source_type = source.get("type")
    user_id = source.get("userId")
    if not isinstance(user_id, str) or user_id not in config.allowed_user_ids:
        return False, "user-not-allowed"
    if source_type == "user":
        return True, "authorized-user"
    if source_type == "group":
        group_id = source.get("groupId")
        if not isinstance(group_id, str) or group_id not in config.allowed_group_ids:
            return False, "group-not-allowed"
        return True, "authorized-group"
    return False, "source-type-not-supported"


def _route_text(text: str, *, source_type: str) -> RouteDecision:
    # str.strip/isspace 涵蓋 Unicode 空白；original_text 仍保留未修改內容。
    stripped = text.strip()
    for prefix, route in _PREFIXES:
        if not stripped.startswith(prefix):
            continue
        if len(stripped) != len(prefix) and not stripped[len(prefix)].isspace():
            continue
        command_text = stripped[len(prefix) :].lstrip()
        return RouteDecision(route, True, text, command_text, "explicit-prefix")
    if source_type == "user":
        return RouteDecision(Route.HELP, True, text, "", "direct-message-default")
    return RouteDecision(Route.IGNORE, True, text, "", "group-prefix-required")


def route_event(event: Mapping[str, object], config: GatewayConfig) -> RouteDecision:
    """Authorize before inspecting message content, then route exact prefixes."""

    source = event.get("source")
    if not isinstance(source, Mapping):
        return _ignored("source-missing")
    authorized, reason = _authorize_source(source, config)
    if not authorized:
        return _ignored(reason)

    message = event.get("message")
    if not isinstance(message, Mapping) or message.get("type") != "text":
        return RouteDecision(Route.IGNORE, True, "", "", "text-message-required")
    text = message.get("text")
    if not isinstance(text, str):
        return RouteDecision(Route.IGNORE, True, "", "", "text-message-required")
    source_type = source.get("type")
    if not isinstance(source_type, str):
        return RouteDecision(Route.IGNORE, True, text, "", "source-type-not-supported")
    return _route_text(text, source_type=source_type)
