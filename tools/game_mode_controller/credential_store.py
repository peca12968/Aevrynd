"""Bounded Windows Credential Manager access for controller node secrets."""

from __future__ import annotations

import ctypes
import os
from ctypes import wintypes

from .inventory import get_node


class CredentialError(RuntimeError):
    pass


class _CredentialW(ctypes.Structure):
    _fields_ = [
        ("flags", wintypes.DWORD), ("credential_type", wintypes.DWORD),
        ("target_name", wintypes.LPWSTR), ("comment", wintypes.LPWSTR),
        ("last_written", wintypes.FILETIME), ("credential_blob_size", wintypes.DWORD),
        ("credential_blob", ctypes.POINTER(ctypes.c_ubyte)), ("persist", wintypes.DWORD),
        ("attribute_count", wintypes.DWORD), ("attributes", ctypes.c_void_p),
        ("target_alias", wintypes.LPWSTR), ("user_name", wintypes.LPWSTR),
    ]


def credential_target(node_id: str) -> str:
    get_node(node_id)
    return f"Aevrynd/GameModeController/{node_id}"


def read_node_secret(node_id: str) -> str | None:
    """Resolve only a fixed per-node target; never include its name or value in errors."""

    get_node(node_id)
    environment_name = f"AEVRYND_GAME_MODE_SECRET_{node_id.upper()}"
    environment_value = os.environ.get(environment_name)
    if environment_value is not None:
        return _validate_secret(environment_value.strip())
    if os.name != "nt":
        return None
    return _read_credential(credential_target(node_id))


def read_qwen_api_key(node_id: str = "main") -> str | None:
    """Read the fixed per-node Qwen API credential without accepting a target name."""

    get_node(node_id)
    environment_name = "AEVRYND_QWEN_LIGHT_API_KEY" if node_id == "main" else f"AEVRYND_GAME_MODE_QWEN_API_KEY_{node_id.upper()}"
    environment_value = os.environ.get(environment_name)
    if environment_value is not None:
        return _validate_secret(environment_value.strip())
    if os.name != "nt":
        return None
    target = "Aevrynd/QwenLight/ApiKey" if node_id == "main" else f"Aevrynd/GameModeController/{node_id}/QwenApiKey"
    return _read_credential(target)


def _read_credential(target: str) -> str | None:
    pointer = ctypes.POINTER(_CredentialW)()
    try:
        advapi32 = ctypes.WinDLL("Advapi32.dll", use_last_error=True)
        read = advapi32.CredReadW
        read.argtypes = [wintypes.LPCWSTR, wintypes.DWORD, wintypes.DWORD, ctypes.POINTER(ctypes.POINTER(_CredentialW))]
        read.restype = wintypes.BOOL
        free = advapi32.CredFree
        free.argtypes = [ctypes.c_void_p]
        free.restype = None
        if not read(target, 1, 0, ctypes.byref(pointer)):
            if ctypes.get_last_error() == 1168:
                return None
            raise CredentialError("node credential lookup failed")
        item = pointer.contents
        size = int(item.credential_blob_size)
        if not 1 <= size <= 16_384:
            raise CredentialError("node credential size is invalid")
        raw = ctypes.string_at(item.credential_blob, size)
        value = _decode(raw)
        return _validate_secret(value.strip())
    finally:
        if pointer:
            try:
                free(pointer)
            except (NameError, OSError):
                pass


def _decode(raw: bytes) -> str:
    for encoding in ("utf-8", "utf-16-le"):
        try:
            value = raw.decode(encoding).rstrip("\x00")
            if "\x00" not in value:
                return value
        except UnicodeDecodeError:
            continue
    raise CredentialError("node credential text is invalid")


def _validate_secret(value: str) -> str:
    if not 32 <= len(value) <= 16_384 or any(character in value for character in "\r\n\x00"):
        raise CredentialError("node credential is invalid")
    return value
