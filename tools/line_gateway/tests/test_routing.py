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
        self.assertEqual(Route.HELP, route_event(event, _config()).route)


if __name__ == "__main__":
    unittest.main()
