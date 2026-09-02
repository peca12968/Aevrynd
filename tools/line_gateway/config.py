"""Strict configuration loading for the Aevrynd LINE gateway."""

from __future__ import annotations

import ipaddress
import os
import re
from dataclasses import dataclass, field
from typing import Mapping
from urllib.parse import urlsplit

from .credential_store import CredentialReader, CredentialResolver, read_windows_credential


class GatewayConfigError(ValueError):
    """Configuration failure with secret-safe messages."""


_IDENTIFIER_PATTERN = re.compile(r"^[A-Za-z0-9_-]{2,128}$")
_SECRET_TARGETS = {
    "channel_secret": (
        "AEVRYND_LINE_CHANNEL_SECRET",
        "Aevrynd/LineGateway/ChannelSecret",
    ),
    "channel_access_token": (
        "AEVRYND_LINE_CHANNEL_ACCESS_TOKEN",
        "Aevrynd/LineGateway/ChannelAccessToken",
    ),
    "qwen_api_key": (
        "AEVRYND_QWEN_API_KEY",
        "Aevrynd/LineGateway/QwenApiKey",
    ),
    "secondary_qwen_api_key": (
        "AEVRYND_QWEN_SECONDARY_API_KEY",
        "Aevrynd/LineGateway/SecondaryQwenApiKey",
    ),
}


@dataclass(frozen=True, slots=True)
class GatewayConfig:
    channel_secret: str = field(repr=False)
    channel_access_token: str = field(repr=False)
    qwen_api_key: str | None = field(default=None, repr=False)
    secondary_qwen_api_key: str | None = field(default=None, repr=False)
    allowed_user_ids: frozenset[str] = field(default_factory=frozenset)
    allowed_group_ids: frozenset[str] = field(default_factory=frozenset)
    bind_host: str = "127.0.0.1"
    port: int = 8787
    max_body_bytes: int = 262_144
    request_timeout_seconds: float = 5.0
    max_events_per_webhook: int = 100
    qwen_base_url: str = "http://192.168.50.98:8080/v1"
    secondary_qwen_base_url: str = "http://100.89.42.36:8080/v1"
    taichung_qwen_role: str = "primary"
    taoyuan_qwen_role: str = "secondary"

    def __post_init__(self) -> None:
        _validate_secret(self.channel_secret, required=True)
        _validate_secret(self.channel_access_token, required=True)
        _validate_secret(self.qwen_api_key, required=False)
        _validate_secret(self.secondary_qwen_api_key, required=False)
        _validate_identifier_set(self.allowed_user_ids)
        _validate_identifier_set(self.allowed_group_ids)
        if not self.allowed_user_ids or not self.allowed_group_ids:
            raise GatewayConfigError("User and group allowlists must both be configured.")
        try:
            bind_address = ipaddress.ip_address(self.bind_host)
        except ValueError as error:
            raise GatewayConfigError("Bind host must be a loopback IP address.") from error
        if not bind_address.is_loopback:
            raise GatewayConfigError("Bind host must be a loopback IP address.")
        if not 1024 <= self.port <= 65_535:
            raise GatewayConfigError("Port is outside the permitted range.")
        if not 1_024 <= self.max_body_bytes <= 1_048_576:
            raise GatewayConfigError("Body size limit is outside the permitted range.")
        if not 0.5 <= self.request_timeout_seconds <= 30.0:
            raise GatewayConfigError("Request timeout is outside the permitted range.")
        if not 1 <= self.max_events_per_webhook <= 100:
            raise GatewayConfigError("Event count limit is outside the permitted range.")
        _validate_qwen_base_url(self.qwen_base_url)
        _validate_qwen_base_url(self.secondary_qwen_base_url)
        if self.qwen_base_url != "http://192.168.50.98:8080/v1":
            raise GatewayConfigError("Primary Qwen endpoint role is fixed.")
        if self.secondary_qwen_base_url != "http://100.89.42.36:8080/v1":
            raise GatewayConfigError("Secondary Qwen endpoint role is fixed.")
        if {self.taichung_qwen_role, self.taoyuan_qwen_role} != {"primary", "secondary"}:
            raise GatewayConfigError("Assistant role mapping must be an exact permutation.")


def _validate_secret(value: str | None, *, required: bool) -> None:
    if value is None:
        if required:
            raise GatewayConfigError("A required credential is unavailable.")
        return
    if not isinstance(value, str) or not 16 <= len(value) <= 16_384:
        raise GatewayConfigError("A credential has an invalid size.")
    if any(character in value for character in ("\x00", "\r", "\n")):
        raise GatewayConfigError("A credential contains invalid characters.")


def _validate_identifier_set(values: frozenset[str]) -> None:
    if not isinstance(values, frozenset) or len(values) > 256:
        raise GatewayConfigError("An allowlist is invalid.")
    if any(not _IDENTIFIER_PATTERN.fullmatch(value) for value in values):
        raise GatewayConfigError("An allowlist contains an invalid identifier.")


def _parse_identifier_set(raw_value: str | None) -> frozenset[str]:
    if raw_value is None:
        return frozenset()
    values = raw_value.split(",")
    if any(not value.strip() for value in values):
        raise GatewayConfigError("An allowlist contains an empty identifier.")
    result = frozenset(value.strip() for value in values)
    _validate_identifier_set(result)
    return result


def _read_integer(
    environment: Mapping[str, str], name: str, default: int
) -> int:
    raw_value = environment.get(name)
    if raw_value is None:
        return default
    if not raw_value.isascii() or not raw_value.isdecimal():
        raise GatewayConfigError("A numeric configuration value is invalid.")
    return int(raw_value)


def _read_float(
    environment: Mapping[str, str], name: str, default: float
) -> float:
    raw_value = environment.get(name)
    if raw_value is None:
        return default
    try:
        value = float(raw_value)
    except ValueError as error:
        raise GatewayConfigError("A numeric configuration value is invalid.") from error
    if value != value or value in (float("inf"), float("-inf")):
        raise GatewayConfigError("A numeric configuration value is invalid.")
    return value


def _validate_qwen_base_url(value: str) -> None:
    if not isinstance(value, str) or len(value) > 2_048:
        raise GatewayConfigError("Qwen base URL is invalid.")
    parsed = urlsplit(value)
    try:
        port = parsed.port
    except ValueError as error:
        raise GatewayConfigError("Qwen base URL is invalid.") from error
    if (
        parsed.scheme not in {"http", "https"}
        or not parsed.hostname
        or parsed.username is not None
        or parsed.password is not None
        or parsed.query
        or parsed.fragment
        or parsed.path.rstrip("/") != "/v1"
        or port is None
    ):
        raise GatewayConfigError("Qwen base URL is invalid.")
    if (parsed.scheme, parsed.hostname, port) not in {
        ("http", "192.168.50.98", 8080),
        ("http", "100.89.42.36", 8080),
    }:
        raise GatewayConfigError("Qwen base URL is not allowlisted.")


def load_config(
    environment: Mapping[str, str] | None = None,
    credential_reader: CredentialReader | None = None,
) -> GatewayConfig:
    """Load validated configuration without returning partial or permissive state."""

    source = os.environ if environment is None else environment
    resolver = CredentialResolver(
        source,
        read_windows_credential if credential_reader is None else credential_reader,
    )
    credentials = {
        name: resolver.resolve(environment_name, target_name)
        for name, (environment_name, target_name) in _SECRET_TARGETS.items()
    }
    if credentials["channel_secret"] is None or credentials["channel_access_token"] is None:
        raise GatewayConfigError("Required LINE credentials are unavailable.")

    return GatewayConfig(
        channel_secret=credentials["channel_secret"],
        channel_access_token=credentials["channel_access_token"],
        qwen_api_key=credentials["qwen_api_key"],
        secondary_qwen_api_key=credentials["secondary_qwen_api_key"],
        allowed_user_ids=_parse_identifier_set(source.get("AEVRYND_LINE_ALLOWED_USER_IDS")),
        allowed_group_ids=_parse_identifier_set(source.get("AEVRYND_LINE_ALLOWED_GROUP_IDS")),
        bind_host=source.get("AEVRYND_LINE_BIND_HOST", "127.0.0.1").strip(),
        port=_read_integer(source, "AEVRYND_LINE_PORT", 8787),
        max_body_bytes=_read_integer(source, "AEVRYND_LINE_MAX_BODY_BYTES", 262_144),
        request_timeout_seconds=_read_float(
            source, "AEVRYND_LINE_REQUEST_TIMEOUT_SECONDS", 5.0
        ),
        max_events_per_webhook=_read_integer(
            source, "AEVRYND_LINE_MAX_EVENTS_PER_WEBHOOK", 100
        ),
        qwen_base_url=source.get(
            "AEVRYND_QWEN_BASE_URL", "http://192.168.50.98:8080/v1"
        ).strip(),
        secondary_qwen_base_url=source.get(
            "AEVRYND_QWEN_SECONDARY_BASE_URL", "http://100.89.42.36:8080/v1"
        ).strip(),
        taichung_qwen_role=source.get("AEVRYND_ASSISTANT_TAICHUNG_QWEN_ROLE", "primary").strip(),
        taoyuan_qwen_role=source.get("AEVRYND_ASSISTANT_TAOYUAN_QWEN_ROLE", "secondary").strip(),
    )
