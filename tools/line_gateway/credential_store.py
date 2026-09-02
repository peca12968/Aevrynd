"""Secret resolution through environment variables or Windows Credential Manager."""

from __future__ import annotations

import ctypes
import os
from ctypes import wintypes
from dataclasses import dataclass
from typing import Callable, Mapping


class CredentialStoreError(RuntimeError):
    """Raised without including credential names or values in the exception text."""


CredentialReader = Callable[[str], str | None]


class _CredentialAttributeW(ctypes.Structure):
    _fields_ = [
        ("keyword", wintypes.LPWSTR),
        ("flags", wintypes.DWORD),
        ("value_size", wintypes.DWORD),
        ("value", ctypes.POINTER(ctypes.c_ubyte)),
    ]


class _CredentialW(ctypes.Structure):
    _fields_ = [
        ("flags", wintypes.DWORD),
        ("credential_type", wintypes.DWORD),
        ("target_name", wintypes.LPWSTR),
        ("comment", wintypes.LPWSTR),
        ("last_written", wintypes.FILETIME),
        ("credential_blob_size", wintypes.DWORD),
        ("credential_blob", ctypes.POINTER(ctypes.c_ubyte)),
        ("persist", wintypes.DWORD),
        ("attribute_count", wintypes.DWORD),
        ("attributes", ctypes.POINTER(_CredentialAttributeW)),
        ("target_alias", wintypes.LPWSTR),
        ("user_name", wintypes.LPWSTR),
    ]


def _decode_credential_blob(blob: bytes) -> str:
    if not blob:
        return ""
    # 先接受不含 NUL 的 UTF-8；UTF-16LE 的 ASCII 內容會含 NUL，不會被誤判。
    try:
        decoded = blob.decode("utf-8")
        if "\x00" not in decoded:
            return decoded
    except UnicodeDecodeError:
        pass
    # Windows 泛用認證通常以 UTF-16LE 儲存。
    if len(blob) % 2 == 0:
        try:
            decoded = blob.decode("utf-16-le")
            if "\x00" not in decoded:
                return decoded.rstrip("\x00")
        except UnicodeDecodeError:
            pass
    raise CredentialStoreError("Credential data is not valid text.")


def read_windows_credential(target_name: str) -> str | None:
    """Read a generic credential without exposing its target or value on failure."""

    if os.name != "nt":
        return None
    if not target_name or len(target_name) > 256:
        raise CredentialStoreError("Credential lookup parameters are invalid.")

    credential_pointer = ctypes.POINTER(_CredentialW)()
    try:
        advapi32 = ctypes.WinDLL("Advapi32.dll", use_last_error=True)
        cred_read = advapi32.CredReadW
        cred_read.argtypes = [
            wintypes.LPCWSTR,
            wintypes.DWORD,
            wintypes.DWORD,
            ctypes.POINTER(ctypes.POINTER(_CredentialW)),
        ]
        cred_read.restype = wintypes.BOOL
        cred_free = advapi32.CredFree
        cred_free.argtypes = [ctypes.c_void_p]
        cred_free.restype = None
        if not cred_read(target_name, 1, 0, ctypes.byref(credential_pointer)):
            error_code = ctypes.get_last_error()
            if error_code == 1168:  # ERROR_NOT_FOUND
                return None
            raise CredentialStoreError("Windows Credential Manager lookup failed.")
        credential = credential_pointer.contents
        size = int(credential.credential_blob_size)
        if size < 1 or size > 16_384:
            raise CredentialStoreError("Credential data size is invalid.")
        blob = ctypes.string_at(credential.credential_blob, size)
        return _decode_credential_blob(blob)
    finally:
        if credential_pointer:
            try:
                cred_free(credential_pointer)
            except (NameError, OSError):
                pass


@dataclass(frozen=True, slots=True)
class CredentialResolver:
    """Resolve secrets from an injected environment and credential reader."""

    environment: Mapping[str, str]
    credential_reader: CredentialReader = read_windows_credential

    def resolve(self, environment_name: str, target_name: str) -> str | None:
        value = self.environment.get(environment_name)
        if value is not None:
            stripped = value.strip()
            return stripped or None
        try:
            value = self.credential_reader(target_name)
        except CredentialStoreError:
            raise
        except Exception as error:
            raise CredentialStoreError("Credential lookup failed.") from error
        if value is None:
            return None
        stripped = value.strip()
        return stripped or None
