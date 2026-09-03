"""Durable read-only dispatcher for bounded remote Qwen tasks."""

from __future__ import annotations

import argparse
import json
import os
import sqlite3
import sys
import time
import uuid
from contextlib import closing
from dataclasses import dataclass
from pathlib import Path
from typing import Callable, Iterable


PROJECT_ROOT = Path(__file__).resolve().parents[2]
if str(PROJECT_ROOT) not in sys.path:
    sys.path.insert(0, str(PROJECT_ROOT))

from tools.line_gateway.credential_store import (  # noqa: E402
    CredentialResolver,
    CredentialStoreError,
)
from tools.line_gateway.qwen_client import (  # noqa: E402
    DEFAULT_ALLOWED_BASE_URL,
    SECONDARY_ALLOWED_BASE_URL,
    InvalidQwenResponse,
    QwenClient,
    QwenClientError,
    RetryableQwenError,
)


ALLOWED_TASK_TYPES = frozenset({"summary", "classification", "test-template", "inventory"})
MAX_EVIDENCE_BYTES = 8_192
MAX_RESULT_BYTES = 16_384
MAX_ATTEMPTS = 3
LEASE_SECONDS = 180
DEFAULT_DEADLINE_SECONDS = 45.0
MAX_DEADLINE_SECONDS = 60.0
NODE_LABELS = ("taichung", "taoyuan")
_CREDENTIAL_TARGETS = {
    "taichung": ("AEVRYND_QWEN_API_KEY", "Aevrynd/LineGateway/QwenApiKey"),
    "taoyuan": ("AEVRYND_QWEN_SECONDARY_API_KEY", "Aevrynd/LineGateway/SecondaryQwenApiKey"),
}
_BASE_URLS = {
    "taichung": DEFAULT_ALLOWED_BASE_URL,
    "taoyuan": SECONDARY_ALLOWED_BASE_URL,
}
_RESULT_KEYS = frozenset(
    {"schemaVersion", "taskId", "taskType", "answer", "items", "confidence", "needsCodex"}
)


class DispatcherError(RuntimeError):
    """A secret-safe dispatcher failure."""


@dataclass(frozen=True, slots=True)
class ClaimedTask:
    task_id: str
    task_type: str
    evidence: str
    attempts: int


def default_database_path() -> Path:
    local_app_data = os.environ.get("LOCALAPPDATA")
    if not local_app_data:
        raise DispatcherError("The local application data directory is unavailable.")
    return Path(local_app_data) / "Aevrynd" / "RemoteQwenDispatcher" / "tasks.sqlite3"


class TaskStore:
    def __init__(self, database_path: Path) -> None:
        self.database_path = database_path.resolve()
        self.database_path.parent.mkdir(parents=True, exist_ok=True)
        self._initialize()

    def _connect(self) -> sqlite3.Connection:
        connection = sqlite3.connect(self.database_path, timeout=5.0, isolation_level=None)
        connection.row_factory = sqlite3.Row
        connection.execute("PRAGMA journal_mode=WAL")
        connection.execute("PRAGMA synchronous=FULL")
        connection.execute("PRAGMA busy_timeout=5000")
        return connection

    def _initialize(self) -> None:
        with closing(self._connect()) as connection:
            connection.executescript(
                """
                CREATE TABLE IF NOT EXISTS tasks (
                    task_id TEXT PRIMARY KEY,
                    task_type TEXT NOT NULL,
                    evidence TEXT NOT NULL,
                    status TEXT NOT NULL,
                    assigned_node TEXT,
                    attempts INTEGER NOT NULL DEFAULT 0,
                    lease_until REAL,
                    result_json TEXT,
                    error_code TEXT,
                    created_at REAL NOT NULL,
                    updated_at REAL NOT NULL
                );
                CREATE INDEX IF NOT EXISTS tasks_claim_index
                    ON tasks(status, created_at);
                CREATE TABLE IF NOT EXISTS metadata (
                    key TEXT PRIMARY KEY,
                    value TEXT NOT NULL
                );
                INSERT OR IGNORE INTO metadata(key, value) VALUES ('next_node', 'taichung');
                """
            )

    def submit(self, task_type: str, evidence: str, *, now: float | None = None) -> str:
        if task_type not in ALLOWED_TASK_TYPES:
            raise DispatcherError("Task type is not allowed.")
        if not isinstance(evidence, str) or not evidence.strip():
            raise DispatcherError("Evidence must be non-empty text.")
        if "\x00" in evidence or len(evidence.encode("utf-8")) > MAX_EVIDENCE_BYTES:
            raise DispatcherError("Evidence is outside the bounded size.")
        task_id = str(uuid.uuid4())
        timestamp = time.time() if now is None else now
        with closing(self._connect()) as connection:
            connection.execute(
                "INSERT INTO tasks(task_id,task_type,evidence,status,created_at,updated_at) "
                "VALUES(?,?,?,'queued',?,?)",
                (task_id, task_type, evidence, timestamp, timestamp),
            )
        return task_id

    def claim(self, *, now: float | None = None) -> tuple[ClaimedTask, tuple[str, str]] | None:
        timestamp = time.time() if now is None else now
        with closing(self._connect()) as connection:
            connection.execute("BEGIN IMMEDIATE")
            row = connection.execute(
                "SELECT task_id,task_type,evidence,attempts FROM tasks "
                "WHERE status='queued' OR (status='processing' AND lease_until<?) "
                "ORDER BY created_at LIMIT 1",
                (timestamp,),
            ).fetchone()
            if row is None:
                connection.execute("COMMIT")
                return None
            cursor = connection.execute(
                "SELECT value FROM metadata WHERE key='next_node'"
            ).fetchone()["value"]
            first = cursor if cursor in NODE_LABELS else NODE_LABELS[0]
            second = NODE_LABELS[1] if first == NODE_LABELS[0] else NODE_LABELS[0]
            connection.execute(
                "UPDATE metadata SET value=? WHERE key='next_node'", (second,)
            )
            attempts = int(row["attempts"]) + 1
            connection.execute(
                "UPDATE tasks SET status='processing',assigned_node=?,attempts=?,lease_until=?,"
                "error_code=NULL,updated_at=? WHERE task_id=?",
                (first, attempts, timestamp + LEASE_SECONDS, timestamp, row["task_id"]),
            )
            connection.execute("COMMIT")
            return (
                ClaimedTask(row["task_id"], row["task_type"], row["evidence"], attempts),
                (first, second),
            )

    def complete(self, task_id: str, node: str, result: dict[str, object]) -> None:
        encoded = json.dumps(result, ensure_ascii=False, separators=(",", ":"))
        if len(encoded.encode("utf-8")) > MAX_RESULT_BYTES:
            raise DispatcherError("Validated result exceeds the bounded size.")
        with closing(self._connect()) as connection:
            changed = connection.execute(
                "UPDATE tasks SET status='completed',assigned_node=?,lease_until=NULL,"
                "result_json=?,error_code=NULL,updated_at=? "
                "WHERE task_id=? AND status='processing'",
                (node, encoded, time.time(), task_id),
            ).rowcount
        if changed != 1:
            raise DispatcherError("Task ownership was lost before completion.")

    def fail(self, task_id: str, error_code: str, *, retry: bool) -> None:
        if not error_code.isascii() or len(error_code) > 64:
            error_code = "DispatcherFailure"
        with closing(self._connect()) as connection:
            row = connection.execute(
                "SELECT attempts FROM tasks WHERE task_id=? AND status='processing'", (task_id,)
            ).fetchone()
            if row is None:
                raise DispatcherError("Task ownership was lost before failure handling.")
            requeue = retry and int(row["attempts"]) < MAX_ATTEMPTS
            connection.execute(
                "UPDATE tasks SET status=?,assigned_node=NULL,lease_until=NULL,error_code=?,updated_at=? "
                "WHERE task_id=?",
                ("queued" if requeue else "failed", error_code, time.time(), task_id),
            )

    def status(self, task_id: str) -> dict[str, object] | None:
        try:
            normalized = str(uuid.UUID(task_id))
        except ValueError as error:
            raise DispatcherError("Task identifier is invalid.") from error
        with closing(self._connect()) as connection:
            row = connection.execute(
                "SELECT task_id,task_type,status,assigned_node,attempts,result_json,error_code,"
                "created_at,updated_at FROM tasks WHERE task_id=?", (normalized,)
            ).fetchone()
        if row is None:
            return None
        result = json.loads(row["result_json"]) if row["result_json"] else None
        return {
            "taskId": row["task_id"],
            "taskType": row["task_type"],
            "status": row["status"],
            "node": row["assigned_node"],
            "attempts": row["attempts"],
            "result": result,
            "errorCode": row["error_code"],
            "createdAt": row["created_at"],
            "updatedAt": row["updated_at"],
        }


def _system_prompt(task: ClaimedTask) -> str:
    return (
        "You are a read-only Aevrynd assistant. Use only the supplied evidence. "
        "Never request or claim file access, shell execution, Git operations, network access, "
        "or repository changes. Return one JSON object only with exactly these keys: "
        "schemaVersion, taskId, taskType, answer, items, confidence, needsCodex. "
        "schemaVersion must be 1; taskId and taskType must exactly match the request; "
        "answer is a concise string; items is an array of at most 12 concise strings; "
        "confidence is from 0 to 1; needsCodex is a boolean."
    )


def _user_prompt(task: ClaimedTask) -> str:
    instructions = {
        "summary": "Summarize the evidence without adding facts.",
        "classification": "Classify the evidence and explain the label briefly.",
        "test-template": "Draft a non-executable test-case template; do not write code or commands.",
        "inventory": "Inventory only the systems or facts explicitly present in the evidence.",
    }
    return (
        f"taskId: {task.task_id}\ntaskType: {task.task_type}\n"
        f"instruction: {instructions[task.task_type]}\nevidence:\n{task.evidence}"
    )


def validate_result(text: str, task: ClaimedTask) -> dict[str, object]:
    if not text or len(text.encode("utf-8")) > MAX_RESULT_BYTES:
        raise InvalidQwenResponse("Qwen task result is empty or oversized")
    stripped = text.strip()
    if stripped.startswith("```"):
        lines = stripped.splitlines()
        if len(lines) < 3 or not lines[-1].strip().startswith("```"):
            raise InvalidQwenResponse("Qwen task result fence is invalid")
        stripped = "\n".join(lines[1:-1])
        if stripped.lstrip().startswith("json"):
            stripped = stripped.lstrip()[4:].lstrip()
    try:
        result = json.loads(stripped)
    except json.JSONDecodeError as error:
        raise InvalidQwenResponse("Qwen task result is not JSON") from error
    if not isinstance(result, dict) or frozenset(result) != _RESULT_KEYS:
        raise InvalidQwenResponse("Qwen task result schema is invalid")
    if result["schemaVersion"] != 1 or result["taskId"] != task.task_id or result["taskType"] != task.task_type:
        raise InvalidQwenResponse("Qwen task result identity is invalid")
    answer = result["answer"]
    items = result["items"]
    confidence = result["confidence"]
    if not isinstance(answer, str) or not answer or len(answer) > 4_000:
        raise InvalidQwenResponse("Qwen task answer is invalid")
    if not isinstance(items, list) or len(items) > 12 or any(
        not isinstance(item, str) or not item or len(item) > 1_000 for item in items
    ):
        raise InvalidQwenResponse("Qwen task items are invalid")
    if isinstance(confidence, bool) or not isinstance(confidence, (int, float)) or not 0 <= confidence <= 1:
        raise InvalidQwenResponse("Qwen task confidence is invalid")
    if not isinstance(result["needsCodex"], bool):
        raise InvalidQwenResponse("Qwen task escalation flag is invalid")
    return result


ClientFactory = Callable[[str, float], QwenClient]


def default_client_factory(node: str, operation_budget: float) -> QwenClient:
    if node not in NODE_LABELS:
        raise DispatcherError("Node label is invalid.")
    if operation_budget < 2 or operation_budget > MAX_DEADLINE_SECONDS:
        raise DispatcherError("Node time budget is invalid.")
    environment_name, target_name = _CREDENTIAL_TARGETS[node]
    try:
        api_key = CredentialResolver(os.environ).resolve(environment_name, target_name)
    except CredentialStoreError as error:
        raise DispatcherError("A Qwen credential could not be read.") from error
    if api_key is None:
        raise DispatcherError("A Qwen credential is unavailable.")
    # 模型探測最多使用三秒，把大部分總時限保留給實際推論。
    probe_timeout = min(3.0, operation_budget / 3.0)
    probe = QwenClient(_BASE_URLS[node], api_key, "probe", timeout=probe_timeout, max_tokens=64)
    models = probe.list_models()
    if not models:
        raise DispatcherError("A Qwen node reported no model.")
    inference_timeout = max(1.0, operation_budget - probe_timeout)
    return QwenClient(_BASE_URLS[node], api_key, models[0], timeout=inference_timeout, max_tokens=64)


def run_one(
    store: TaskStore,
    client_factory: ClientFactory = default_client_factory,
    *,
    deadline_seconds: float = DEFAULT_DEADLINE_SECONDS,
    clock: Callable[[], float] = time.monotonic,
) -> dict[str, object]:
    if deadline_seconds < 5 or deadline_seconds > MAX_DEADLINE_SECONDS:
        raise DispatcherError("Run deadline is outside the allowed range.")
    claimed = store.claim()
    if claimed is None:
        return {"ok": True, "status": "idle"}
    task, node_order = claimed
    deadline = clock() + deadline_seconds
    retryable_failure = False
    try:
        for index, node in enumerate(node_order):
            remaining = deadline - clock()
            if remaining < 2:
                retryable_failure = True
                break
            # 第一節點最多取得三十秒，確保第二節點仍有快速 failover 時間。
            operation_budget = min(30.0 if index == 0 else remaining, remaining)
            try:
                client = client_factory(node, operation_budget)
                reply = client.chat(_user_prompt(task), system_prompt=_system_prompt(task))
                result = validate_result(reply.text, task)
                store.complete(task.task_id, node, result)
                return {"ok": True, "status": "completed", "taskId": task.task_id, "node": node}
            except RetryableQwenError:
                retryable_failure = True
                continue
            except (InvalidQwenResponse, QwenClientError, DispatcherError, ValueError):
                continue
    except KeyboardInterrupt:
        # 使用者中斷時立即歸還工作，不等待 lease 自然逾時。
        store.fail(task.task_id, "Interrupted", retry=True)
        raise
    except BaseException:
        store.fail(task.task_id, "DispatcherInterrupted", retry=True)
        raise
    store.fail(task.task_id, "RemoteNodesUnavailable", retry=retryable_failure)
    state = store.status(task.task_id)
    return {"ok": False, "status": state["status"], "taskId": task.task_id, "errorCode": "RemoteNodesUnavailable"}


def _emit(payload: dict[str, object]) -> None:
    print(json.dumps(payload, ensure_ascii=False, separators=(",", ":")))


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="Dispatch bounded read-only work to remote Qwen nodes.")
    commands = parser.add_subparsers(dest="command", required=True)
    submit = commands.add_parser("submit")
    submit.add_argument("--task-type", required=True, choices=sorted(ALLOWED_TASK_TYPES))
    submit.add_argument("--evidence", required=True)
    run = commands.add_parser("run-one")
    run.add_argument(
        "--deadline-seconds",
        type=float,
        default=DEFAULT_DEADLINE_SECONDS,
        help="Total deadline from 5 through 60 seconds.",
    )
    status = commands.add_parser("status")
    status.add_argument("--task-id", required=True)
    return parser


def main(arguments: Iterable[str] | None = None) -> int:
    parser = build_parser()
    options = parser.parse_args(arguments)
    try:
        store = TaskStore(default_database_path())
        if options.command == "submit":
            task_id = store.submit(options.task_type, options.evidence)
            _emit({"ok": True, "status": "queued", "taskId": task_id})
        elif options.command == "run-one":
            _emit(run_one(store, deadline_seconds=options.deadline_seconds))
        else:
            state = store.status(options.task_id)
            _emit({"ok": state is not None, "task": state})
        return 0
    except KeyboardInterrupt:
        _emit({"ok": False, "status": "interrupted", "errorCode": "Interrupted"})
        return 130
    except DispatcherError as error:
        _emit({"ok": False, "errorCode": type(error).__name__, "message": str(error)})
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
