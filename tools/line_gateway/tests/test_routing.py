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
            "@Qwen hello": Route.QWEN,
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
        self.assertEqual(Route.AUTO_QWEN, route_event(mentioned("@Bot 台中桃園"), _config()).route)
        for text in ("@Bot 台中：桃園：問題", "@Bot 桃園："):
            with self.subTest(text=text):
                self.assertEqual(Route.IGNORE, route_event(mentioned(text), _config()).route)

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
