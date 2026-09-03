"""Authenticated command routing for LINE webhook events."""

from __future__ import annotations

from dataclasses import dataclass
from enum import Enum
import re
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
    STATUS = "status"
    ARTIFACT = "artifact"
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
    # @Qwen 是自動邊緣助理別名，不得繞過台中／桃園的一次式分類與回答。
    ("@Qwen", Route.AUTO_QWEN),
    ("@台中", Route.TAICHUNG),
    ("@桃園", Route.TAOYUAN),
    ("@Codex", Route.CODEX),
    ("@全部", Route.ALL),
)

_STATUS_COMMANDS = frozenset(
    {
        "狀態",
        "三台狀態",
        "主機狀態",
        "主電腦狀態",
        "主電腦進度",
        "Codex進度",
        "codex進度",
        "開發進度",
        "現在進度",
        "目前進度",
        "目前開發進度",
        "目前整體工作進度",
        "Aevrynd開發進度",
        "aevrynd開發進度",
        "遊戲引擎開發進度",
        "專案開發進度",
        "台中電腦工作進度",
        "桃園電腦工作進度",
        "台中工作進度",
        "桃園工作進度",
        "台中正在做什麼",
        "桃園正在做什麼",
        "三台電腦工作進度",
        "現階段是否可以透過LINE得知主電腦進度",
        "現在主電腦Codex開發進度如何",
    }
)
_STATUS_LEADING_WORDS = ("請回報", "請查詢", "請查看", "回報", "查詢", "查看")
_STATUS_TRAILING_WORDS = ("如何", "呢", "嗎", "？", "?")
_STATUS_SUBJECTS = {
    "三台": ("", "電腦", "主機"),
    "全部": ("", "電腦", "主機"),
    "台中": ("", "電腦", "主機"),
    "桃園": ("", "電腦", "主機"),
    "主電腦": ("",),
    "主機": ("",),
    "Codex": ("",),
    "codex": ("",),
    "Aevrynd": ("",),
    "aevrynd": ("",),
    "遊戲引擎": ("",),
    "專案": ("",),
}
_STATUS_PREDICATES = frozenset(
    {
        "狀態",
        "進度",
        "工作進度",
        "工作狀態",
        "任務",
        "任務進度",
        "任務狀態",
        "正在做什麼",
        "正在執行什麼",
        "開發進度",
        "有工作進度",
    }
)
_ARTIFACT_MARKERS = ("ppt", "pptx", "word", "docx", "pdf", "簡報", "文件", "檔案")
_ARTIFACT_ACTIONS = ("產生", "生成", "建立", "製作", "做一份", "做個", "幫我做", "輸出")


def _is_bounded_status_command(normalized: str, default_route: Route) -> bool:
    """只接受有限詞彙與固定語序，避免一般問答誤判成狀態查詢。"""

    if not normalized or len(normalized) > 64:
        return False
    if normalized in _STATUS_COMMANDS:
        return True
    candidate = normalized
    for word in _STATUS_LEADING_WORDS:
        if candidate.startswith(word):
            candidate = candidate[len(word) :]
            break
    for word in _STATUS_TRAILING_WORDS:
        if candidate.endswith(word):
            candidate = candidate[: -len(word)]
            break
    if default_route in {Route.TAICHUNG, Route.TAOYUAN} and candidate in _STATUS_PREDICATES | {
        "電腦工作進度",
        "主機工作進度",
        "電腦狀態",
        "主機狀態",
    }:
        return True
    for subject, machine_words in _STATUS_SUBJECTS.items():
        if not candidate.startswith(subject):
            continue
        tail = candidate[len(subject) :]
        return any(tail == machine + predicate for machine in machine_words for predicate in _STATUS_PREDICATES)
    return False


def _classify_command(command: str, default_route: Route) -> tuple[Route, str, str]:
    normalized = "".join(command.strip().split())
    if (
        _is_bounded_status_command(normalized, default_route)
        or normalized in {"主電腦Codex進度", "主電腦codex進度"}
    ):
        return Route.STATUS, command.strip(), "deterministic-status"
    if re.fullmatch(r"(?:工作|任務)(?:狀態)?[：:\s]+[0-9a-fA-F-]{36}", command.strip()):
        return Route.STATUS, command.strip(), "deterministic-job-status"
    lowered = command.casefold()
    if any(marker in lowered for marker in _ARTIFACT_MARKERS) and any(
        action in command for action in _ARTIFACT_ACTIONS
    ):
        return Route.ARTIFACT, command.strip(), "deterministic-artifact"
    return default_route, command, ""


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
        selected, command_text, reason = _classify_command(command_text, route)
        if selected is Route.STATUS and route in {Route.TAICHUNG, Route.TAOYUAN}:
            city = "台中" if route is Route.TAICHUNG else "桃園"
            if not command_text.startswith(city):
                command_text = city + command_text
        return RouteDecision(selected, True, text, command_text, reason or "explicit-prefix")
    if source_type == "user":
        selected, command_text, reason = _classify_command(stripped, Route.AUTO_QWEN)
        return RouteDecision(selected, True, text, command_text, reason or "direct-message-auto")
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
    original_command = command
    for selector, route in (("台中", Route.TAICHUNG), ("桃園", Route.TAOYUAN)):
        if not command.startswith(selector):
            continue
        tail = command[len(selector) :]
        command = tail.lstrip(" \t\r\n:：")
        # 城市選擇器只接受精確中文名稱；尾端分隔符可省略，但不可串接另一城市造成歧義。
        if command.startswith("台中") or command.startswith("桃園"):
            return _ignored("assistant-selector-invalid", text)
        selected = route
        break
    status_candidate = original_command if selected in {Route.TAICHUNG, Route.TAOYUAN} else command
    classified, classified_text, reason = _classify_command(status_candidate, selected)
    return RouteDecision(classified, True, text, classified_text if reason else command, reason or "self-mention")


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
