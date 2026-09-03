"""Immutable policy for the restricted worker.

這些值不是 job 輸入；變更必須隨受信任套件更新。
"""

from dataclasses import dataclass

SCHEMA_VERSION = 1
FIXED_ORIGIN = "https://github.com/peca12968/Aevrynd.git"
MAX_TTL_SECONDS = 300
MAX_CLOCK_SKEW_SECONDS = 30
MAX_LOG_BYTES = 1_048_576
SUMMARY_LOG_BYTES = 4096
ALLOWED_TIMEOUTS = frozenset({300, 900, 1800})
ALLOWED_NODES = frozenset({"taichung", "taoyuan"})


@dataclass(frozen=True)
class TestPreset:
    configure_preset: str
    build_preset: str
    targets: tuple[str, ...]
    test_label: str


# Phase 1 僅開放明列組合。repo 尚未提供對應 preset/label 時 readiness 必須失敗關閉。
PRESETS = {
    "windows-release-gates": TestPreset(
        configure_preset="windows-release-gates",
        build_preset="windows-release-gates",
        targets=("AevryndStreamlinePackageFixture",),
        test_label="release-gates",
    )
}

RECEIPT_STATUSES = frozenset(
    {"passed", "test_failed", "build_failed", "timeout", "infrastructure_error"}
)
CLASSIFICATIONS = frozenset(
    {"clean", "test_failure", "build_failure", "timeout", "infrastructure_error", "unknown"}
)

