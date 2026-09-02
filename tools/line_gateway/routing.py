"""Authenticated command routing for LINE webhook events."""

from __future__ import annotations

from dataclasses import dataclass
from enum import Enum
from typing import Mapping

from .config import GatewayConfig


class Route(str, Enum):
    HOST = "host"
    QWEN = "qwen"
    AUTO_QWEN = "auto_qwen"
    TAICHUNG = "taichung"
    TAOYUAN = "taoyuan"
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
    ("@台中", Route.TAICHUNG),
    ("@桃園", Route.TAOYUAN),
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
        return RouteDecision(Route.AUTO_QWEN, True, text, stripped, "direct-message-auto")
    return RouteDecision(Route.IGNORE, True, text, "", "group-prefix-required")


def _utf16_boundaries(text: str) -> dict[int, int]:
    result = {0: 0}
    units = 0
    for index, character in enumerate(text, 1):
        units += len(character.encode("utf-16-le")) // 2
        result[units] = index
    return result


def _route_self_mention(text: str, mention: object) -> RouteDecision | None:
    if not isinstance(mention, Mapping) or not isinstance(mention.get("mentionees"), list):
        return None
    boundaries = _utf16_boundaries(text)
    total_units = max(boundaries)
    ranges: list[tuple[int, int, bool]] = []
    for item in mention["mentionees"]:
        if not isinstance(item, Mapping):
            return _ignored("mention-invalid", text)
        index, length = item.get("index"), item.get("length")
        if isinstance(index, bool) or isinstance(length, bool) or not isinstance(index, int) or not isinstance(length, int):
            return _ignored("mention-invalid", text)
        end = index + length
        if index < 0 or length < 1 or end > total_units or index not in boundaries or end not in boundaries:
            return _ignored("mention-range-invalid", text)
        ranges.append((index, end, item.get("type") == "user" and item.get("isSelf") is True))
    ordered = sorted(ranges)
    if any(ordered[i][0] < ordered[i - 1][1] for i in range(1, len(ordered))):
        return _ignored("mention-overlap", text)
    self_ranges = [(start, end) for start, end, is_self in ranges if is_self]
    if not self_ranges:
        return None
    characters = list(text)
    for start, end in sorted(self_ranges, reverse=True):
        del characters[boundaries[start] : boundaries[end]]
    command = "".join(characters).strip()
    if not command:
        return _ignored("assistant-question-required", text)
    selected = Route.AUTO_QWEN
    for selector, route in (("台中", Route.TAICHUNG), ("桃園", Route.TAOYUAN)):
        if not command.startswith(selector):
            continue
        tail = command[len(selector) :]
        if not tail or (not tail[0].isspace() and tail[0] not in {":", "："}):
            continue
        command = tail.lstrip(" \t\r\n:：")
        if not command or command.startswith("台中 ") or command.startswith("台中：") or command.startswith("桃園 ") or command.startswith("桃園："):
            return _ignored("assistant-selector-invalid", text)
        selected = route
        break
    return RouteDecision(selected, True, text, command, "self-mention")


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
    mentioned = _route_self_mention(text, message.get("mention"))
    if mentioned is not None:
        return mentioned
    return _route_text(text, source_type=source_type)
