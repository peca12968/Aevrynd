"""Allowlist and exact route selection tests."""

from __future__ import annotations

import unittest

from tools.line_gateway.config import GatewayConfig
from tools.line_gateway.routing import Route, route_event


def _config() -> GatewayConfig:
    return GatewayConfig(
        channel_secret="test-channel-secret-value",
        channel_access_token="test-access-token-value",
        allowed_user_ids=frozenset({"allowed-user"}),
        allowed_group_ids=frozenset({"allowed-group"}),
    )


def _event(text: str, *, user: str = "allowed-user", group: str = "allowed-group") -> dict:
    return {
        "source": {"type": "group", "userId": user, "groupId": group},
        "message": {"type": "text", "text": text},
    }


class RoutingTests(unittest.TestCase):
    def test_both_user_and_group_must_be_allowlisted(self) -> None:
        self.assertFalse(route_event(_event("@Qwen hi", user="attacker"), _config()).authorized)
        self.assertFalse(route_event(_event("@Qwen hi", group="other-group"), _config()).authorized)

    def test_exact_group_tags_route_and_strip_command(self) -> None:
        cases = {
            "@主機 health": Route.HOST,
            "@Qwen hello": Route.AUTO_QWEN,
            "@台中 hello": Route.TAICHUNG,
            "@桃園 hello": Route.TAOYUAN,
            "@Codex task": Route.CODEX,
            "@全部 task": Route.ALL,
        }
        for text, expected in cases.items():
            with self.subTest(text=text):
                decision = route_event(_event(text), _config())
                self.assertTrue(decision.authorized)
                self.assertEqual(expected, decision.route)
                self.assertEqual(text.split(maxsplit=1)[1], decision.command_text)

    def test_prefix_confusables_and_suffixes_do_not_route(self) -> None:
        hostile = (
            "x@Qwen hello",
            "＠Qwen hello",
            "@qwen hello",
            "@QwenAttack hello",
            "@Codex/health",
            "@全部!",
            "Qwen hello",
        )
        for text in hostile:
            with self.subTest(text=text):
                self.assertEqual(Route.IGNORE, route_event(_event(text), _config()).route)

    def test_direct_message_defaults_to_help(self) -> None:
        event = {
            "source": {"type": "user", "userId": "allowed-user"},
            "message": {"type": "text", "text": "hello"},
        }
        self.assertEqual(Route.AUTO_QWEN, route_event(event, _config()).route)

    def test_self_mention_routes_auto_or_exact_city_selector(self) -> None:
        def mentioned(text: str, index: int = 0, length: int = 4) -> dict:
            event = _event(text)
            event["message"]["mention"] = {"mentionees": [{"index": index, "length": length, "type": "user", "isSelf": True}]}
            return event

        self.assertEqual(Route.AUTO_QWEN, route_event(mentioned("@Bot 比較台中和桃園"), _config()).route)
        taichung = route_event(mentioned("@Bot 台中：請回答"), _config())
        self.assertEqual((Route.TAICHUNG, "請回答"), (taichung.route, taichung.command_text))
        taoyuan = route_event(mentioned("@Bot 桃園 問題"), _config())
        self.assertEqual((Route.TAOYUAN, "問題"), (taoyuan.route, taoyuan.command_text))
        compact_taoyuan = route_event(mentioned("@Bot 桃園測試"), _config())
        self.assertEqual((Route.TAOYUAN, "測試"), (compact_taoyuan.route, compact_taoyuan.command_text))
        compact_taichung = route_event(mentioned("@Bot 台中現在幾點"), _config())
        self.assertEqual((Route.TAICHUNG, "現在幾點"), (compact_taichung.route, compact_taichung.command_text))
        city_only = route_event(mentioned("@Bot 桃園："), _config())
        self.assertEqual((Route.TAOYUAN, ""), (city_only.route, city_only.command_text))
        for text in ("@Bot 台中桃園", "@Bot 台中：桃園：問題"):
            with self.subTest(text=text):
                self.assertEqual(Route.IGNORE, route_event(mentioned(text), _config()).route)

        for text in (
            "@Bot 請桃園回答",
            "@Bot x台中測試",
            "@Bot 桃圜測試",
            "@Bot 臺中測試",
            "@Bot Ｔ桃園測試",
        ):
            with self.subTest(text=text):
                decision = route_event(mentioned(text), _config())
                self.assertEqual(Route.AUTO_QWEN, decision.route)

    def test_status_and_artifact_requests_bypass_qwen(self) -> None:
        def mentioned(command: str) -> dict:
            event = _event("@Bot " + command)
            event["message"]["mention"] = {
                "mentionees": [{"index": 0, "length": 4, "type": "user", "isSelf": True}]
            }
            return event

        for command in (
            "三台狀態",
            "主電腦進度",
            "Codex進度",
            "桃園狀態",
            "台中狀態",
            "現階段是否可以透過LINE得知主電腦進度",
            "現在主電腦Codex開發進度如何",
            "桃園電腦工作進度",
            "台中電腦工作進度",
            "桃園工作進度",
            "台中工作進度",
            "桃園正在做什麼",
            "台中正在做什麼",
            "三台電腦工作進度",
            "回報三台電腦正在做什麼",
            "查詢全部主機任務狀態",
            "請查看桃園主機正在執行什麼？",
            "回報台中電腦任務",
            "查詢主機工作狀態",
            "請回報Codex開發進度",
            "桃園有工作進度嗎",
            "Aevrynd開發進度",
            "遊戲引擎開發進度",
            "目前整體工作進度",
            "桃園電腦目前的工作？",
            "台中電腦目前的工作？",
        ):
            with self.subTest(command=command):
                self.assertEqual(Route.STATUS, route_event(mentioned(command), _config()).route)
        for text, city in (("@桃園 工作進度", "桃園"), ("@台中 電腦工作進度", "台中")):
            with self.subTest(text=text):
                decision = route_event(_event(text), _config())
                self.assertEqual(Route.STATUS, decision.route)
                self.assertTrue(decision.command_text.startswith(city))
        for command in (
            "回報三台電腦溫度",
            "查詢桃園天氣狀態",
            "台中有哪些工作機會",
            "全部遊戲任務怎麼做",
            "請說明主機是什麼",
            "回報三台電腦正在做什麼以及明天天氣",
            "桃園天氣有工作進度嗎",
            "Aevrynd官網開發進度如何",
        ):
            with self.subTest(command=command):
                self.assertNotEqual(Route.STATUS, route_event(mentioned(command), _config()).route)
        for command in ("幫我做一份ppt", "產生 PDF 檔案", "建立word文件"):
            with self.subTest(command=command):
                self.assertEqual(Route.ARTIFACT, route_event(mentioned(command), _config()).route)

    def test_self_mention_rejects_forgery_and_malformed_ranges(self) -> None:
        plain = _event("@Official hello")
        self.assertEqual(Route.IGNORE, route_event(plain, _config()).route)
        for mentionees in (
            [{"index": 999, "length": 1, "type": "user", "isSelf": True}],
            [{"index": 0, "length": 4, "type": "user", "isSelf": True}, {"index": 2, "length": 2, "type": "user"}],
            [{"index": 0, "length": 0, "type": "user", "isSelf": True}],
        ):
            event = _event("@Bot hello")
            event["message"]["mention"] = {"mentionees": mentionees}
            with self.subTest(mentionees=mentionees):
                self.assertEqual(Route.IGNORE, route_event(event, _config()).route)


if __name__ == "__main__":
    unittest.main()
