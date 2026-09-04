"""Bounded local worker for GPT-5.6 Luna.

The worker only accepts text work and never executes model-produced commands.
The API key is read from the process environment and is never persisted.
"""

from __future__ import annotations

import argparse
import json
import os
import threading
import time
import urllib.error
import urllib.request
import uuid
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from typing import Any

NODE_IDS = {"taichung", "taoyuan"}
DEFAULT_MODEL = "gpt-5.6-luna"
DEFAULT_API_ROOT = "https://api.openai.com/v1"
MAX_PROMPT_CHARS = 16_000
MAX_OUTPUT_TOKENS = 4_096


def _json_bytes(value: Any) -> bytes:
    return json.dumps(value, ensure_ascii=False, separators=(",", ":")).encode("utf-8")


class LunaError(RuntimeError):
    """A bounded, user-safe worker error."""


class LunaClient:
    def __init__(self, api_key: str, api_root: str, model: str, timeout: float = 120.0) -> None:
        if not api_key or any(ch.isspace() for ch in api_key):
            raise LunaError("api_key_missing_or_invalid")
        self.api_key = api_key
        self.api_root = api_root.rstrip("/")
        self.model = model
        self.timeout = timeout

    def health(self) -> dict[str, Any]:
        request = urllib.request.Request(
            f"{self.api_root}/models",
            headers={"Authorization": f"Bearer {self.api_key}", "Accept": "application/json"},
            method="GET",
        )
        try:
            with urllib.request.urlopen(request, timeout=min(self.timeout, 15)) as response:
                if response.status != 200:
                    raise LunaError("api_health_failed")
                return {"reachable": True, "status": response.status}
        except (urllib.error.HTTPError, urllib.error.URLError, TimeoutError) as exc:
            raise LunaError("api_health_failed") from exc

    def complete(self, prompt: str) -> str:
        body = {
            "model": self.model,
            "input": prompt,
            "max_output_tokens": MAX_OUTPUT_TOKENS,
        }
        request = urllib.request.Request(
            f"{self.api_root}/responses",
            data=_json_bytes(body),
            headers={
                "Authorization": f"Bearer {self.api_key}",
                "Content-Type": "application/json",
                "Accept": "application/json",
            },
            method="POST",
        )
        try:
            with urllib.request.urlopen(request, timeout=self.timeout) as response:
                payload = json.loads(response.read(2_000_000).decode("utf-8"))
        except (urllib.error.HTTPError, urllib.error.URLError, TimeoutError, ValueError) as exc:
            raise LunaError("luna_request_failed") from exc
        text = payload.get("output_text")
        if not isinstance(text, str) or not text.strip():
            raise LunaError("luna_empty_response")
        return text[:MAX_PROMPT_CHARS]


class WorkStore:
    def __init__(self, root: Path, node_id: str, model: str) -> None:
        self.root = root
        self.inbox = root / "inbox"
        self.records = root / "records"
        self.node_id = node_id
        self.model = model
        self.lock = threading.Lock()
        self.active: str | None = None
        for directory in (self.inbox, self.records):
            directory.mkdir(parents=True, exist_ok=True)

    def register(self, prompt: str) -> dict[str, Any]:
        if not isinstance(prompt, str) or not prompt.strip() or len(prompt) > MAX_PROMPT_CHARS:
            raise LunaError("prompt_invalid")
        work_id = str(uuid.uuid4())
        record = {
            "schemaVersion": 1,
            "workId": work_id,
            "nodeId": self.node_id,
            "model": self.model,
            "status": "queued",
            "createdUtc": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()),
            "prompt": prompt,
        }
        self._write(self.inbox / f"{work_id}.json", record)
        return {key: value for key, value in record.items() if key != "prompt"}

    def _write(self, path: Path, value: dict[str, Any]) -> None:
        temporary = path.with_suffix(".tmp")
        temporary.write_bytes(_json_bytes(value))
        temporary.replace(path)

    def status(self) -> dict[str, Any]:
        with self.lock:
            active = self.active
        return {"nodeId": self.node_id, "model": self.model, "activeWorkId": active,
                "queued": len(list(self.inbox.glob("*.json")))}

    def process_one(self, client: LunaClient) -> bool:
        candidates = sorted(self.inbox.glob("*.json"))
        if not candidates:
            return False
        path = candidates[0]
        with self.lock:
            self.active = path.stem
        try:
            record = json.loads(path.read_text(encoding="utf-8"))
            record["status"] = "running"
            record["startedUtc"] = time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime())
            prompt = record.get("prompt")
            if not isinstance(prompt, str):
                raise LunaError("record_invalid")
            result = client.complete(prompt)
            record["status"] = "completed"
            record["result"] = result
        except (OSError, ValueError, LunaError) as exc:
            record = locals().get("record", {"schemaVersion": 1, "workId": path.stem})
            record["status"] = "failed"
            record["errorCode"] = str(exc)[:64]
        record["finishedUtc"] = time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime())
        self._write(self.records / path.name, record)
        path.unlink(missing_ok=True)
        with self.lock:
            self.active = None
        return True


class Handler(BaseHTTPRequestHandler):
    server_version = "AevryndLunaWorker/1"

    def do_GET(self) -> None:
        if self.path == "/health":
            self._send(200, {"status": "ok", **self.server.store.status()})
        elif self.path == "/status":
            self._send(200, self.server.store.status())
        else:
            self._send(404, {"error": "not_found"})

    def do_POST(self) -> None:
        if self.path != "/work":
            self._send(404, {"error": "not_found"})
            return
        try:
            length = int(self.headers.get("Content-Length", "-1"))
            if length < 2 or length > MAX_PROMPT_CHARS + 128:
                raise LunaError("body_invalid")
            body = json.loads(self.rfile.read(length).decode("utf-8"))
            if set(body) != {"prompt"}:
                raise LunaError("body_invalid")
            self._send(202, self.server.store.register(body["prompt"]))
        except (ValueError, UnicodeDecodeError, json.JSONDecodeError, LunaError):
            self._send(400, {"error": "work_rejected"})

    def _send(self, status: int, value: dict[str, Any]) -> None:
        data = _json_bytes(value)
        self.send_response(status)
        self.send_header("Content-Type", "application/json; charset=utf-8")
        self.send_header("Content-Length", str(len(data)))
        self.end_headers()
        self.wfile.write(data)

    def log_message(self, _format: str, *_args: object) -> None:
        return


def run(args: argparse.Namespace) -> int:
    node_id = args.node_id
    root = Path(args.work_dir)
    key = os.environ.get(args.api_key_env, "")
    client = LunaClient(key, args.api_root, args.model, args.timeout)
    store = WorkStore(root, node_id, args.model)
    server = ThreadingHTTPServer(("127.0.0.1", args.port), Handler)
    server.store = store
    server.client = client
    server.timeout = 0.5

    def process_loop() -> None:
        while getattr(server, "keep_running", True):
            try:
                store.process_one(client)
            except Exception:
                time.sleep(1)
            time.sleep(0.2)

    server.keep_running = True
    threading.Thread(target=process_loop, name="luna-work-loop", daemon=True).start()
    server.serve_forever()
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description="Aevrynd GPT-5.6 Luna worker")
    parser.add_argument("--node-id", required=True, choices=sorted(NODE_IDS))
    parser.add_argument("--work-dir", default=r"C:\ProgramData\Aevrynd\LunaWorker")
    parser.add_argument("--port", type=int, default=8792)
    parser.add_argument("--model", default=os.environ.get("AEVRYND_LUNA_MODEL", DEFAULT_MODEL))
    parser.add_argument("--api-root", default=os.environ.get("AEVRYND_OPENAI_API_ROOT", DEFAULT_API_ROOT))
    parser.add_argument("--api-key-env", default="AEVRYND_OPENAI_API_KEY")
    parser.add_argument("--timeout", type=float, default=120.0)
    args = parser.parse_args()
    try:
        return run(args)
    except LunaError as exc:
        print(str(exc))
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
