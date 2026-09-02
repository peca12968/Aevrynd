"""Durable, bounded work queue for the LINE gateway."""

from __future__ import annotations

import json
import sqlite3
import time
import uuid
from contextlib import contextmanager
from dataclasses import dataclass
from pathlib import Path
from typing import Any, Iterable


JOB_STATUSES = frozenset(
    {"queued", "running", "awaiting_codex", "completed", "failed", "dead_letter"}
)
DELIVERY_STATUSES = frozenset({"queued", "running", "completed", "dead_letter"})
MAX_REQUEST_CHARS = 16_384
MAX_RESULT_CHARS = 32_768
MAX_IDENTIFIER_CHARS = 256
MAX_ROUTE_CHARS = 32
JOB_LANES = frozenset({"taichung", "taoyuan", "auto", "control"})
FAILURE_DELIVERY_TEXT = "工作處理失敗，請稍後查詢工作狀態。"
_DELIVERY_ID_NAMESPACE = uuid.UUID("9af706cf-508c-4e82-bdb7-e631aa70a79e")
_RETRY_KEY_NAMESPACE = uuid.UUID("86a90880-ca2e-47cc-9dac-c7eb9d302811")
_PROGRESS_ID_NAMESPACE = uuid.UUID("c84740e5-f7ac-4b29-99c3-81400801128e")
_PROGRESS_RETRY_NAMESPACE = uuid.UUID("26ad047e-44cd-41ee-a92a-48ab964b11af")
_RECEPTION_ID_NAMESPACE = uuid.UUID("d9512daf-8fab-4d85-b046-2ab6dba7088a")
_RECEPTION_RETRY_NAMESPACE = uuid.UUID("57c14ecf-7ec4-4bbf-90cd-ea328b401b28")


class QueueStoreError(RuntimeError):
    """Raised when durable queue invariants are violated."""


@dataclass(frozen=True, slots=True)
class EnqueueResult:
    job_id: str
    inserted: bool


@dataclass(frozen=True, slots=True)
class ClaimedJob:
    job_id: str
    webhook_event_id: str
    route: str
    source_id: str
    requester_id: str
    request_text: str
    attempt_count: int
    max_attempts: int
    expires_at: float
    result_text: str
    handoff_hash: str


@dataclass(frozen=True, slots=True)
class ClaimedDelivery:
    delivery_id: str
    job_id: str
    destination_id: str
    message_text: str
    retry_key: str
    attempt_count: int
    max_attempts: int
    expires_at: float


@dataclass(frozen=True, slots=True)
class ClaimedNotification:
    notification_id: str
    job_id: str
    destination_id: str
    message_text: str
    retry_key: str
    attempt_count: int
    max_attempts: int
    expires_at: float


@dataclass(frozen=True, slots=True)
class AssistantProgress:
    delivery_id: str
    route: str
    destination_id: str
    message_text: str
    retry_key: str
    status: str


def _require_text(value: object, name: str, maximum: int, *, allow_empty: bool = False) -> str:
    if not isinstance(value, str):
        raise QueueStoreError(f"{name} must be text")
    if not allow_empty and not value:
        raise QueueStoreError(f"{name} must not be empty")
    if len(value) > maximum:
        raise QueueStoreError(f"{name} exceeds the configured limit")
    return value


def _canonical_uuid(value: str, name: str) -> str:
    try:
        parsed = uuid.UUID(value)
    except (ValueError, TypeError, AttributeError) as exc:
        raise QueueStoreError(f"{name} must be a UUID") from exc
    canonical = str(parsed)
    if canonical != value.lower():
        raise QueueStoreError(f"{name} must use canonical UUID form")
    return canonical


class QueueStore:
    """SQLite queue with atomic deduplication, leases and bounded persistence."""

    def __init__(self, db_path: str | Path) -> None:
        self._db_path = Path(db_path)
        self._db_path.parent.mkdir(parents=True, exist_ok=True)
        self._initialize()

    def _connect(self) -> sqlite3.Connection:
        connection = sqlite3.connect(self._db_path, timeout=10.0, isolation_level=None)
        connection.row_factory = sqlite3.Row
        connection.execute("PRAGMA foreign_keys = ON")
        connection.execute("PRAGMA busy_timeout = 10000")
        return connection

    @contextmanager
    def _connection(self) -> Iterable[sqlite3.Connection]:
        connection = self._connect()
        try:
            yield connection
        finally:
            connection.close()

    def _initialize(self) -> None:
        with self._connection() as connection:
            connection.execute("PRAGMA journal_mode = WAL")
            connection.execute("PRAGMA synchronous = FULL")
            connection.executescript(
                """
                CREATE TABLE IF NOT EXISTS events (
                    webhook_event_id TEXT PRIMARY KEY,
                    received_at REAL NOT NULL
                ) WITHOUT ROWID;

                CREATE TABLE IF NOT EXISTS jobs (
                    job_id TEXT PRIMARY KEY,
                    webhook_event_id TEXT NOT NULL UNIQUE REFERENCES events(webhook_event_id),
                    route TEXT NOT NULL,
                    source_id TEXT NOT NULL,
                    requester_id TEXT NOT NULL,
                    request_text TEXT NOT NULL,
                    status TEXT NOT NULL CHECK(status IN
                        ('queued','running','awaiting_codex','completed','failed','dead_letter')),
                    attempt_count INTEGER NOT NULL DEFAULT 0,
                    max_attempts INTEGER NOT NULL,
                    available_at REAL NOT NULL,
                    lease_until REAL,
                    lease_owner TEXT NOT NULL DEFAULT '',
                    created_at REAL NOT NULL,
                    updated_at REAL NOT NULL,
                    expires_at REAL NOT NULL,
                    result_text TEXT NOT NULL DEFAULT '',
                    error_code TEXT NOT NULL DEFAULT '',
                    handoff_hash TEXT NOT NULL DEFAULT '',
                    codex_request_prepared INTEGER NOT NULL DEFAULT 0
                );

                CREATE INDEX IF NOT EXISTS jobs_claim_idx
                    ON jobs(status, available_at, created_at);
                CREATE INDEX IF NOT EXISTS jobs_lease_idx
                    ON jobs(status, lease_until);

                CREATE TABLE IF NOT EXISTS deliveries (
                    delivery_id TEXT PRIMARY KEY,
                    job_id TEXT NOT NULL UNIQUE REFERENCES jobs(job_id),
                    destination_id TEXT NOT NULL,
                    message_text TEXT NOT NULL,
                    retry_key TEXT NOT NULL UNIQUE,
                    status TEXT NOT NULL CHECK(status IN
                        ('queued','running','completed','dead_letter')),
                    attempt_count INTEGER NOT NULL DEFAULT 0,
                    max_attempts INTEGER NOT NULL,
                    available_at REAL NOT NULL,
                    lease_until REAL,
                    lease_owner TEXT NOT NULL DEFAULT '',
                    created_at REAL NOT NULL,
                    updated_at REAL NOT NULL,
                    expires_at REAL NOT NULL,
                    error_code TEXT NOT NULL DEFAULT ''
                );

                CREATE INDEX IF NOT EXISTS deliveries_claim_idx
                    ON deliveries(status, available_at, created_at);

                CREATE TABLE IF NOT EXISTS reception_notifications (
                    notification_id TEXT PRIMARY KEY,
                    job_id TEXT NOT NULL UNIQUE REFERENCES jobs(job_id),
                    destination_id TEXT NOT NULL,
                    message_text TEXT NOT NULL,
                    retry_key TEXT NOT NULL UNIQUE,
                    status TEXT NOT NULL CHECK(status IN ('queued','running','completed','dead_letter')),
                    attempt_count INTEGER NOT NULL DEFAULT 0,
                    max_attempts INTEGER NOT NULL,
                    available_at REAL NOT NULL,
                    lease_until REAL,
                    lease_owner TEXT NOT NULL DEFAULT '',
                    created_at REAL NOT NULL,
                    updated_at REAL NOT NULL,
                    expires_at REAL NOT NULL,
                    error_code TEXT NOT NULL DEFAULT ''
                );
                CREATE INDEX IF NOT EXISTS reception_notifications_claim_idx
                    ON reception_notifications(status, available_at, created_at);

                CREATE TABLE IF NOT EXISTS assistant_progress (
                    delivery_id TEXT PRIMARY KEY,
                    job_id TEXT NOT NULL REFERENCES jobs(job_id),
                    route TEXT NOT NULL CHECK(route IN ('taichung','taoyuan')),
                    destination_id TEXT NOT NULL,
                    message_text TEXT NOT NULL,
                    retry_key TEXT NOT NULL UNIQUE,
                    identity_version INTEGER NOT NULL CHECK(identity_version IN (1,2)),
                    status TEXT NOT NULL CHECK(status IN ('pending','completed','dead_letter')),
                    created_at REAL NOT NULL,
                    updated_at REAL NOT NULL,
                    error_code TEXT NOT NULL DEFAULT '',
                    UNIQUE(job_id, route)
                );
                """
            )
            connection.execute("BEGIN IMMEDIATE")
            try:
                columns = {
                    str(row[1]) for row in connection.execute("PRAGMA table_info(jobs)")
                }
                if "codex_request_prepared" not in columns:
                    connection.execute(
                        "ALTER TABLE jobs ADD COLUMN codex_request_prepared INTEGER NOT NULL DEFAULT 0"
                    )
                progress_columns = connection.execute(
                    "PRAGMA table_info(assistant_progress)"
                ).fetchall()
                is_legacy = any(
                    str(row[1]) == "job_id" and int(row[5]) == 1 for row in progress_columns
                )
                residual = connection.execute(
                    "SELECT 1 FROM sqlite_master WHERE type = 'table' AND name = 'assistant_progress_legacy'"
                ).fetchone() is not None
                if is_legacy:
                    if residual:
                        raise QueueStoreError("ambiguous assistant progress migration state")
                    connection.execute(
                        "ALTER TABLE assistant_progress RENAME TO assistant_progress_legacy"
                    )
                    self._migration_checkpoint("renamed")
                    connection.execute(
                        """
                        CREATE TABLE assistant_progress (
                            delivery_id TEXT PRIMARY KEY,
                            job_id TEXT NOT NULL REFERENCES jobs(job_id),
                            route TEXT NOT NULL CHECK(route IN ('taichung','taoyuan')),
                            destination_id TEXT NOT NULL,
                            message_text TEXT NOT NULL,
                            retry_key TEXT NOT NULL UNIQUE,
                            identity_version INTEGER NOT NULL CHECK(identity_version IN (1,2)),
                            status TEXT NOT NULL CHECK(status IN ('pending','completed','dead_letter')),
                            created_at REAL NOT NULL,
                            updated_at REAL NOT NULL,
                            error_code TEXT NOT NULL DEFAULT '',
                            UNIQUE(job_id, route)
                        )
                        """
                    )
                    residual = True
                if residual:
                    legacy_rows = connection.execute(
                        "SELECT delivery_id, job_id, route, destination_id, message_text, retry_key, status, created_at, updated_at, error_code FROM assistant_progress_legacy"
                    ).fetchall()
                    for row in legacy_rows:
                        legacy_delivery, legacy_retry = self._stable_progress_identity(
                            str(row["job_id"]), identity_version=1
                        )
                        if str(row["delivery_id"]) != legacy_delivery or str(row["retry_key"]) != legacy_retry:
                            raise QueueStoreError("legacy assistant progress identity is invalid")
                        values = tuple(row[name] for name in (
                            "delivery_id", "job_id", "route", "destination_id", "message_text",
                            "retry_key", "status", "created_at", "updated_at", "error_code",
                        ))
                        connection.execute(
                            """
                            INSERT OR IGNORE INTO assistant_progress(
                                delivery_id, job_id, route, destination_id, message_text, retry_key,
                                identity_version, status, created_at, updated_at, error_code
                            ) VALUES (?, ?, ?, ?, ?, ?, 1, ?, ?, ?, ?)
                            """,
                            values,
                        )
                        copied = connection.execute(
                            "SELECT delivery_id, job_id, route, destination_id, message_text, retry_key, status, created_at, updated_at, error_code FROM assistant_progress WHERE job_id = ? AND route = ?",
                            (row["job_id"], row["route"]),
                        ).fetchone()
                        if copied is None or tuple(copied) != values:
                            raise QueueStoreError("assistant progress migration identity conflict")
                    self._migration_checkpoint("copied")
                    connection.execute("DROP TABLE assistant_progress_legacy")
                current_columns = {
                    str(row[1]) for row in connection.execute("PRAGMA table_info(assistant_progress)")
                }
                if "identity_version" not in current_columns:
                    connection.execute(
                        "ALTER TABLE assistant_progress ADD COLUMN identity_version INTEGER NOT NULL DEFAULT 2 CHECK(identity_version IN (1,2))"
                    )
                connection.commit()
            except Exception:
                connection.rollback()
                raise

    def _migration_checkpoint(self, _stage: str) -> None:
        """Test seam for proving that schema migration rolls back atomically."""

    @staticmethod
    def _stable_delivery_identity(job_id: str) -> tuple[str, str]:
        canonical = _canonical_uuid(job_id, "job_id")
        return (
            str(uuid.uuid5(_DELIVERY_ID_NAMESPACE, canonical)),
            str(uuid.uuid5(_RETRY_KEY_NAMESPACE, canonical)),
        )

    @staticmethod
    def _stable_progress_identity(
        job_id: str, route: str = "", *, identity_version: int
    ) -> tuple[str, str]:
        canonical = _canonical_uuid(job_id, "job_id")
        if identity_version == 1:
            identity = canonical
        elif identity_version == 2 and route in {"taichung", "taoyuan"}:
            identity = f"{canonical}/{route}"
        else:
            raise QueueStoreError("assistant progress identity version is invalid")
        return (
            str(uuid.uuid5(_PROGRESS_ID_NAMESPACE, identity)),
            str(uuid.uuid5(_PROGRESS_RETRY_NAMESPACE, identity)),
        )

    def _insert_delivery_in_transaction(
        self,
        connection: sqlite3.Connection,
        *,
        job_id: str,
        destination_id: str,
        message_text: str,
        timestamp: float,
        ttl_seconds: int = 86_400,
        max_attempts: int = 8,
    ) -> str:
        delivery_id, retry_key = self._stable_delivery_identity(job_id)
        connection.execute(
            """
            INSERT OR IGNORE INTO deliveries(
                delivery_id, job_id, destination_id, message_text, retry_key,
                status, max_attempts, available_at, created_at, updated_at, expires_at
            ) VALUES (?, ?, ?, ?, ?, 'queued', ?, ?, ?, ?, ?)
            """,
            (
                delivery_id,
                job_id,
                destination_id,
                message_text,
                retry_key,
                max_attempts,
                timestamp,
                timestamp,
                timestamp,
                timestamp + ttl_seconds,
            ),
        )
        existing = connection.execute(
            "SELECT delivery_id, destination_id, message_text, retry_key FROM deliveries WHERE job_id = ?",
            (job_id,),
        ).fetchone()
        if (
            existing is None
            or str(existing["delivery_id"]) != delivery_id
            or str(existing["destination_id"]) != destination_id
            or str(existing["message_text"]) != message_text
            or str(existing["retry_key"]) != retry_key
        ):
            raise QueueStoreError("delivery identity does not match the terminal job")
        return delivery_id

    @property
    def db_path(self) -> Path:
        return self._db_path

    def enqueue(
        self,
        *,
        webhook_event_id: str,
        route: str,
        source_id: str,
        requester_id: str,
        request_text: str,
        ttl_seconds: int = 3_600,
        max_attempts: int = 5,
        now: float | None = None,
        reception_notice: str = "",
    ) -> EnqueueResult:
        event_id = _require_text(webhook_event_id, "webhook_event_id", MAX_IDENTIFIER_CHARS)
        route_value = _require_text(route, "route", MAX_ROUTE_CHARS)
        source = _require_text(source_id, "source_id", MAX_IDENTIFIER_CHARS)
        requester = _require_text(
            requester_id, "requester_id", MAX_IDENTIFIER_CHARS, allow_empty=True
        )
        request = _require_text(request_text, "request_text", MAX_REQUEST_CHARS)
        notice = _require_text(reception_notice, "reception_notice", 5_000, allow_empty=True)
        if ttl_seconds < 1 or ttl_seconds > 604_800:
            raise QueueStoreError("ttl_seconds is outside the allowed range")
        if max_attempts < 1 or max_attempts > 20:
            raise QueueStoreError("max_attempts is outside the allowed range")
        timestamp = time.time() if now is None else float(now)
        job_id = str(uuid.uuid4())

        with self._connection() as connection:
            connection.execute("BEGIN IMMEDIATE")
            existing = connection.execute(
                "SELECT job_id FROM jobs WHERE webhook_event_id = ?", (event_id,)
            ).fetchone()
            if existing is not None:
                connection.commit()
                return EnqueueResult(str(existing["job_id"]), False)
            connection.execute(
                "INSERT INTO events(webhook_event_id, received_at) VALUES (?, ?)",
                (event_id, timestamp),
            )
            connection.execute(
                """
                INSERT INTO jobs(
                    job_id, webhook_event_id, route, source_id, requester_id,
                    request_text, status, max_attempts, available_at, created_at,
                    updated_at, expires_at
                ) VALUES (?, ?, ?, ?, ?, ?, 'queued', ?, ?, ?, ?, ?)
                """,
                (
                    job_id,
                    event_id,
                    route_value,
                    source,
                    requester,
                    request,
                    max_attempts,
                    timestamp,
                    timestamp,
                    timestamp,
                    timestamp + ttl_seconds,
                ),
            )
            if notice:
                identity = f"{job_id}/{route_value}"
                notification_id = str(uuid.uuid5(_RECEPTION_ID_NAMESPACE, identity))
                retry_key = str(uuid.uuid5(_RECEPTION_RETRY_NAMESPACE, identity))
                connection.execute(
                    "INSERT INTO reception_notifications(notification_id, job_id, destination_id, "
                    "message_text, retry_key, status, max_attempts, available_at, created_at, updated_at, expires_at) "
                    "VALUES (?, ?, ?, ?, ?, 'queued', 8, ?, ?, ?, ?)",
                    (notification_id, job_id, source, notice, retry_key, timestamp,
                     timestamp, timestamp, timestamp + ttl_seconds),
                )
            connection.commit()
        return EnqueueResult(job_id, True)

    def _recover_expired_jobs(self, connection: sqlite3.Connection, now: float) -> None:
        # 過期租約只會重新排隊或進入死信，不會同時被兩個 worker 領取。
        terminal_rows = connection.execute(
            """
            SELECT job_id, source_id FROM jobs
             WHERE (status = 'running' AND lease_until IS NOT NULL AND lease_until <= ?
                    AND (attempt_count >= max_attempts OR expires_at <= ?))
                OR (status = 'queued' AND expires_at <= ?)
            """,
            (now, now, now),
        ).fetchall()
        connection.execute(
            """
            UPDATE jobs
               SET status = CASE
                       WHEN attempt_count >= max_attempts OR expires_at <= ?
                           THEN 'dead_letter'
                       ELSE 'queued'
                   END,
                   available_at = ?, lease_until = NULL, lease_owner = '',
                   error_code = CASE
                       WHEN attempt_count >= max_attempts THEN 'attempt_cap'
                       WHEN expires_at <= ? THEN 'ttl_expired'
                       ELSE 'lease_expired'
                   END,
                   updated_at = ?
             WHERE status = 'running' AND lease_until IS NOT NULL AND lease_until <= ?
            """,
            (now, now, now, now, now),
        )
        connection.execute(
            """
            UPDATE jobs
               SET status = 'dead_letter', error_code = 'ttl_expired', updated_at = ?
             WHERE status = 'queued' AND expires_at <= ?
            """,
            (now, now),
        )
        for row in terminal_rows:
            self._insert_delivery_in_transaction(
                connection,
                job_id=str(row["job_id"]),
                destination_id=str(row["source_id"]),
                message_text=FAILURE_DELIVERY_TEXT,
                timestamp=now,
            )

    def claim_next(
        self, worker_id: str, *, lease_seconds: int = 60, now: float | None = None
    ) -> ClaimedJob | None:
        return self._claim_next(worker_id, lane=None, lease_seconds=lease_seconds, now=now)

    def claim_next_for_lane(
        self,
        worker_id: str,
        lane: str,
        *,
        lease_seconds: int = 60,
        now: float | None = None,
    ) -> ClaimedJob | None:
        """Atomically claim only work owned by one execution lane."""
        lane_value = _require_text(lane, "lane", MAX_ROUTE_CHARS)
        if lane_value not in JOB_LANES:
            raise QueueStoreError("lane is not supported")
        return self._claim_next(
            worker_id, lane=lane_value, lease_seconds=lease_seconds, now=now
        )

    def _claim_next(
        self,
        worker_id: str,
        *,
        lane: str | None,
        lease_seconds: int,
        now: float | None,
    ) -> ClaimedJob | None:
        owner = _require_text(worker_id, "worker_id", MAX_IDENTIFIER_CHARS)
        if lease_seconds < 1 or lease_seconds > 3_600:
            raise QueueStoreError("lease_seconds is outside the allowed range")
        timestamp = time.time() if now is None else float(now)
        with self._connection() as connection:
            connection.execute("BEGIN IMMEDIATE")
            self._recover_expired_jobs(connection, timestamp)
            lane_clause = {
                None: "",
                "taichung": "AND route = 'taichung'",
                "taoyuan": "AND route = 'taoyuan'",
                "auto": "AND route = 'auto_qwen'",
                "control": "AND route NOT IN ('taichung','taoyuan','auto_qwen')",
            }[lane]
            row = connection.execute(
                f"""
                SELECT * FROM jobs
                 WHERE status = 'queued' AND available_at <= ? AND expires_at > ?
                       AND attempt_count < max_attempts
                       AND NOT EXISTS (
                           SELECT 1 FROM reception_notifications n
                            WHERE n.job_id = jobs.job_id AND n.status != 'completed'
                       )
                       {lane_clause}
                 ORDER BY created_at, job_id
                 LIMIT 1
                """,
                (timestamp, timestamp),
            ).fetchone()
            if row is None:
                connection.commit()
                return None
            updated = connection.execute(
                """
                UPDATE jobs
                   SET status = 'running', attempt_count = attempt_count + 1,
                       lease_until = ?, lease_owner = ?, updated_at = ?
                 WHERE job_id = ? AND status = 'queued'
                   AND NOT EXISTS (
                       SELECT 1 FROM reception_notifications n
                        WHERE n.job_id = jobs.job_id AND n.status != 'completed'
                   )
                """,
                (timestamp + lease_seconds, owner, timestamp, row["job_id"]),
            ).rowcount
            if updated != 1:
                connection.rollback()
                return None
            claimed = connection.execute(
                "SELECT * FROM jobs WHERE job_id = ?", (row["job_id"],)
            ).fetchone()
            connection.commit()
        return self._to_claimed_job(claimed)

    @staticmethod
    def _to_claimed_job(row: sqlite3.Row) -> ClaimedJob:
        return ClaimedJob(
            job_id=str(row["job_id"]),
            webhook_event_id=str(row["webhook_event_id"]),
            route=str(row["route"]),
            source_id=str(row["source_id"]),
            requester_id=str(row["requester_id"]),
            request_text=str(row["request_text"]),
            attempt_count=int(row["attempt_count"]),
            max_attempts=int(row["max_attempts"]),
            expires_at=float(row["expires_at"]),
            result_text=str(row["result_text"]),
            handoff_hash=str(row["handoff_hash"]),
        )

    def renew_lease(
        self, job_id: str, worker_id: str, *, lease_seconds: int = 60, now: float | None = None
    ) -> bool:
        timestamp = time.time() if now is None else float(now)
        with self._connection() as connection:
            changed = connection.execute(
                """
                UPDATE jobs SET lease_until = ?, updated_at = ?
                 WHERE job_id = ? AND status = 'running' AND lease_owner = ?
                """,
                (timestamp + lease_seconds, timestamp, job_id, worker_id),
            ).rowcount
        return changed == 1

    def persist_canonical_codex_request(
        self,
        job_id: str,
        request_text: str,
        partial_result: str,
        *,
        expected_lease_owner: str,
        now: float | None = None,
    ) -> None:
        """Persist a validated handoff request before publishing its file."""
        request_value = _require_text(request_text, "request_text", MAX_REQUEST_CHARS)
        partial = _require_text(
            partial_result, "partial_result", MAX_RESULT_CHARS, allow_empty=True
        )
        owner = _require_text(
            expected_lease_owner, "expected_lease_owner", MAX_IDENTIFIER_CHARS
        )
        timestamp = time.time() if now is None else float(now)
        with self._connection() as connection:
            changed = connection.execute(
                """
                UPDATE jobs SET request_text = ?, result_text = ?,
                       codex_request_prepared = 1, updated_at = ?
                 WHERE job_id = ? AND status = 'running' AND lease_owner = ?
                       AND route IN ('taichung', 'taoyuan', 'auto_qwen')
                """,
                (request_value, partial, timestamp, job_id, owner),
            ).rowcount
        if changed != 1:
            raise QueueStoreError("Codex request persistence lost its job lease")

    def complete(
        self,
        job_id: str,
        result_text: str,
        *,
        expected_lease_owner: str,
        now: float | None = None,
    ) -> None:
        result = _require_text(result_text, "result_text", MAX_RESULT_CHARS, allow_empty=True)
        self._transition_running(
            job_id,
            "completed",
            result,
            "",
            expected_lease_owner=expected_lease_owner,
            now=now,
        )

    def complete_with_delivery(
        self,
        job_id: str,
        result_text: str,
        *,
        expected_lease_owner: str,
        now: float | None = None,
    ) -> str:
        return self._complete_with_delivery(
            job_id,
            result_text,
            expected_status="running",
            expected_lease_owner=expected_lease_owner,
            now=now,
        )

    def complete_awaiting_with_delivery(
        self, job_id: str, result_text: str, *, now: float | None = None
    ) -> str:
        return self._complete_with_delivery(
            job_id, result_text, expected_status="awaiting_codex", now=now
        )

    def _complete_with_delivery(
        self,
        job_id: str,
        result_text: str,
        *,
        expected_status: str,
        expected_lease_owner: str | None = None,
        now: float | None,
    ) -> str:
        result = _require_text(result_text, "result_text", MAX_RESULT_CHARS, allow_empty=True)
        owner = (
            None
            if expected_lease_owner is None
            else _require_text(expected_lease_owner, "expected_lease_owner", MAX_IDENTIFIER_CHARS)
        )
        if expected_status == "running" and owner is None:
            raise QueueStoreError("running completion requires its expected lease owner")
        timestamp = time.time() if now is None else float(now)
        with self._connection() as connection:
            connection.execute("BEGIN IMMEDIATE")
            owner_clause = "" if owner is None else " AND lease_owner = ?"
            lookup_parameters: tuple[object, ...] = (job_id, expected_status) + (
                () if owner is None else (owner,)
            )
            row = connection.execute(
                f"SELECT source_id FROM jobs WHERE job_id = ? AND status = ?{owner_clause}",
                lookup_parameters,
            ).fetchone()
            if row is None:
                connection.rollback()
                raise QueueStoreError("job is not in the expected completion state")
            update_parameters: tuple[object, ...] = (
                result,
                timestamp,
                job_id,
                expected_status,
            ) + (() if owner is None else (owner,))
            changed = connection.execute(
                f"""
                UPDATE jobs SET status = 'completed', result_text = ?, error_code = '',
                       lease_until = NULL, lease_owner = '', updated_at = ?
                 WHERE job_id = ? AND status = ?{owner_clause}
                """,
                update_parameters,
            ).rowcount
            if changed != 1:
                connection.rollback()
                raise QueueStoreError("job completion raced with another worker")
            delivery_id = self._insert_delivery_in_transaction(
                connection,
                job_id=job_id,
                destination_id=str(row["source_id"]),
                message_text=result,
                timestamp=timestamp,
            )
            connection.commit()
        return delivery_id

    def fail_running_with_delivery(
        self,
        job_id: str,
        error_code: str,
        *,
        expected_lease_owner: str,
        retry_delay_seconds: int | None = None,
        now: float | None = None,
    ) -> str:
        code = _require_text(error_code, "error_code", 128)
        owner = _require_text(
            expected_lease_owner, "expected_lease_owner", MAX_IDENTIFIER_CHARS
        )
        timestamp = time.time() if now is None else float(now)
        with self._connection() as connection:
            connection.execute("BEGIN IMMEDIATE")
            row = connection.execute(
                "SELECT source_id, attempt_count, max_attempts, expires_at FROM jobs WHERE job_id = ? AND status = 'running' AND lease_owner = ?",
                (job_id, owner),
            ).fetchone()
            if row is None:
                connection.rollback()
                raise QueueStoreError("job is not running")
            retry = (
                retry_delay_seconds is not None
                and retry_delay_seconds >= 0
                and int(row["attempt_count"]) < int(row["max_attempts"])
                and float(row["expires_at"]) > timestamp + retry_delay_seconds
            )
            status = "queued" if retry else ("dead_letter" if retry_delay_seconds is not None else "failed")
            changed = connection.execute(
                """
                UPDATE jobs SET status = ?, available_at = ?, lease_until = NULL,
                       lease_owner = '', error_code = ?, updated_at = ?
                 WHERE job_id = ? AND status = 'running' AND lease_owner = ?
                """,
                (status, timestamp + (retry_delay_seconds or 0), code, timestamp, job_id, owner),
            ).rowcount
            if changed != 1:
                connection.rollback()
                raise QueueStoreError("job lease owner changed before failure transition")
            if status != "queued":
                self._insert_delivery_in_transaction(
                    connection,
                    job_id=job_id,
                    destination_id=str(row["source_id"]),
                    message_text=FAILURE_DELIVERY_TEXT,
                    timestamp=timestamp,
                )
            connection.commit()
        return status

    def dead_letter_with_delivery(
        self,
        job_id: str,
        error_code: str,
        *,
        expected_lease_owner: str,
        now: float | None = None,
    ) -> None:
        code = _require_text(error_code, "error_code", 128)
        owner = _require_text(
            expected_lease_owner, "expected_lease_owner", MAX_IDENTIFIER_CHARS
        )
        timestamp = time.time() if now is None else float(now)
        with self._connection() as connection:
            connection.execute("BEGIN IMMEDIATE")
            row = connection.execute(
                "SELECT source_id FROM jobs WHERE job_id = ? AND status = 'running' AND lease_owner = ?",
                (job_id, owner),
            ).fetchone()
            if row is None:
                connection.rollback()
                raise QueueStoreError("job is not running")
            changed = connection.execute(
                """
                UPDATE jobs SET status = 'dead_letter', result_text = '', error_code = ?,
                       lease_until = NULL, lease_owner = '', updated_at = ?
                 WHERE job_id = ? AND status = 'running' AND lease_owner = ?
                """,
                (code, timestamp, job_id, owner),
            ).rowcount
            if changed != 1:
                connection.rollback()
                raise QueueStoreError("job lease owner changed before dead-letter transition")
            self._insert_delivery_in_transaction(
                connection,
                job_id=job_id,
                destination_id=str(row["source_id"]),
                message_text=FAILURE_DELIVERY_TEXT,
                timestamp=timestamp,
            )
            connection.commit()

    def fail(
        self,
        job_id: str,
        error_code: str,
        *,
        expected_lease_owner: str,
        retry_delay_seconds: int | None = None,
        now: float | None = None,
    ) -> str:
        code = _require_text(error_code, "error_code", 128)
        owner = _require_text(
            expected_lease_owner, "expected_lease_owner", MAX_IDENTIFIER_CHARS
        )
        timestamp = time.time() if now is None else float(now)
        with self._connection() as connection:
            connection.execute("BEGIN IMMEDIATE")
            row = connection.execute(
                "SELECT status, attempt_count, max_attempts, expires_at FROM jobs WHERE job_id = ? AND lease_owner = ?",
                (job_id, owner),
            ).fetchone()
            if row is None or row["status"] != "running":
                connection.rollback()
                raise QueueStoreError("job is not running")
            retry = (
                retry_delay_seconds is not None
                and retry_delay_seconds >= 0
                and int(row["attempt_count"]) < int(row["max_attempts"])
                and float(row["expires_at"]) > timestamp + retry_delay_seconds
            )
            if retry:
                status = "queued"
            elif retry_delay_seconds is not None:
                status = "dead_letter"
            else:
                status = "failed"
            changed = connection.execute(
                """
                UPDATE jobs SET status = ?, available_at = ?, lease_until = NULL,
                       lease_owner = '', error_code = ?, updated_at = ?
                 WHERE job_id = ? AND status = 'running' AND lease_owner = ?
                """,
                (status, timestamp + (retry_delay_seconds or 0), code, timestamp, job_id, owner),
            ).rowcount
            if changed != 1:
                connection.rollback()
                raise QueueStoreError("job lease owner changed before failure transition")
            connection.commit()
        return status

    def dead_letter(
        self,
        job_id: str,
        error_code: str,
        *,
        expected_lease_owner: str,
        now: float | None = None,
    ) -> None:
        self._transition_running(
            job_id,
            "dead_letter",
            "",
            error_code,
            expected_lease_owner=expected_lease_owner,
            now=now,
        )

    def mark_awaiting_codex(
        self,
        job_id: str,
        handoff_hash: str,
        partial_result: str = "",
        *,
        worker_id: str | None = None,
        now: float | None = None,
    ) -> None:
        digest = _require_text(handoff_hash, "handoff_hash", 64)
        if len(digest) != 64 or any(character not in "0123456789abcdef" for character in digest):
            raise QueueStoreError("handoff_hash must be a lowercase SHA-256 digest")
        partial = _require_text(
            partial_result, "partial_result", MAX_RESULT_CHARS, allow_empty=True
        )
        timestamp = time.time() if now is None else float(now)
        owner_clause = "" if worker_id is None else " AND lease_owner = ?"
        parameters: tuple[object, ...] = (
            partial,
            digest,
            timestamp,
            job_id,
        ) + (() if worker_id is None else (worker_id,))
        with self._connection() as connection:
            changed = connection.execute(
                f"""
                UPDATE jobs SET status = 'awaiting_codex', result_text = ?,
                       handoff_hash = ?, lease_until = NULL, lease_owner = '', updated_at = ?
                 WHERE job_id = ? AND status = 'running'{owner_clause}
                """,
                parameters,
            ).rowcount
        if changed != 1:
            raise QueueStoreError("job is not running")

    def complete_awaiting(
        self, job_id: str, result_text: str, *, now: float | None = None
    ) -> None:
        result = _require_text(result_text, "result_text", MAX_RESULT_CHARS, allow_empty=True)
        timestamp = time.time() if now is None else float(now)
        with self._connection() as connection:
            changed = connection.execute(
                """
                UPDATE jobs SET status = 'completed', result_text = ?, updated_at = ?
                 WHERE job_id = ? AND status = 'awaiting_codex'
                """,
                (result, timestamp, job_id),
            ).rowcount
        if changed != 1:
            raise QueueStoreError("job is not awaiting Codex")

    def fail_awaiting(
        self, job_id: str, error_code: str, *, now: float | None = None
    ) -> None:
        code = _require_text(error_code, "error_code", 128)
        timestamp = time.time() if now is None else float(now)
        with self._connection() as connection:
            changed = connection.execute(
                """
                UPDATE jobs SET status = 'failed', error_code = ?, updated_at = ?
                 WHERE job_id = ? AND status = 'awaiting_codex'
                """,
                (code, timestamp, job_id),
            ).rowcount
        if changed != 1:
            raise QueueStoreError("job is not awaiting Codex")

    def fail_awaiting_with_delivery(
        self, job_id: str, error_code: str, *, now: float | None = None
    ) -> None:
        code = _require_text(error_code, "error_code", 128)
        timestamp = time.time() if now is None else float(now)
        with self._connection() as connection:
            connection.execute("BEGIN IMMEDIATE")
            row = connection.execute(
                "SELECT source_id FROM jobs WHERE job_id = ? AND status = 'awaiting_codex'",
                (job_id,),
            ).fetchone()
            if row is None:
                connection.rollback()
                raise QueueStoreError("job is not awaiting Codex")
            connection.execute(
                """
                UPDATE jobs SET status = 'failed', error_code = ?, updated_at = ?
                 WHERE job_id = ? AND status = 'awaiting_codex'
                """,
                (code, timestamp, job_id),
            )
            self._insert_delivery_in_transaction(
                connection,
                job_id=job_id,
                destination_id=str(row["source_id"]),
                message_text=FAILURE_DELIVERY_TEXT,
                timestamp=timestamp,
            )
            connection.commit()

    def _transition_running(
        self,
        job_id: str,
        status: str,
        result_text: str,
        error_code: str,
        *,
        expected_lease_owner: str,
        now: float | None,
    ) -> None:
        if status not in JOB_STATUSES:
            raise QueueStoreError("invalid job status")
        owner = _require_text(
            expected_lease_owner, "expected_lease_owner", MAX_IDENTIFIER_CHARS
        )
        timestamp = time.time() if now is None else float(now)
        with self._connection() as connection:
            changed = connection.execute(
                """
                UPDATE jobs SET status = ?, result_text = ?, error_code = ?,
                       lease_until = NULL, lease_owner = '', updated_at = ?
                 WHERE job_id = ? AND status = 'running' AND lease_owner = ?
                """,
                (status, result_text, error_code, timestamp, job_id, owner),
            ).rowcount
        if changed != 1:
            raise QueueStoreError("job is not running")

    def claim_reconcilable_codex(
        self,
        worker_id: str,
        limit: int = 32,
        *,
        lease_seconds: int = 60,
        now: float | None = None,
    ) -> list[ClaimedJob]:
        owner = _require_text(worker_id, "worker_id", MAX_IDENTIFIER_CHARS)
        if limit < 1 or limit > 256:
            raise QueueStoreError("limit is outside the allowed range")
        if lease_seconds < 1 or lease_seconds > 3_600:
            raise QueueStoreError("lease_seconds is outside the allowed range")
        timestamp = time.time() if now is None else float(now)
        with self._connection() as connection:
            connection.execute("BEGIN IMMEDIATE")
            candidates = connection.execute(
                """
                SELECT job_id FROM jobs
                 WHERE status = 'running'
                       AND (route IN ('codex', 'all') OR codex_request_prepared = 1)
                       AND lease_until IS NOT NULL AND lease_until <= ?
                       AND expires_at > ?
                 ORDER BY updated_at LIMIT ?
                """,
                (timestamp, timestamp, limit),
            ).fetchall()
            job_ids = [str(row["job_id"]) for row in candidates]
            claimed: list[sqlite3.Row] = []
            for job_id in job_ids:
                changed = connection.execute(
                    """
                    UPDATE jobs SET lease_until = ?, lease_owner = ?, updated_at = ?
                     WHERE job_id = ? AND status = 'running'
                           AND lease_until IS NOT NULL AND lease_until <= ?
                    """,
                    (timestamp + lease_seconds, owner, timestamp, job_id, timestamp),
                ).rowcount
                if changed == 1:
                    row = connection.execute(
                        "SELECT * FROM jobs WHERE job_id = ?", (job_id,)
                    ).fetchone()
                    if row is not None:
                        claimed.append(row)
            connection.commit()
        return [self._to_claimed_job(row) for row in claimed]

    def list_awaiting_codex(
        self, limit: int = 32, *, now: float | None = None
    ) -> list[ClaimedJob]:
        if limit < 1 or limit > 256:
            raise QueueStoreError("limit is outside the allowed range")
        timestamp = time.time() if now is None else float(now)
        with self._connection() as connection:
            connection.execute("BEGIN IMMEDIATE")
            expired = connection.execute(
                """
                SELECT job_id, source_id FROM jobs
                 WHERE status = 'awaiting_codex'
                       AND (expires_at <= ? OR attempt_count > max_attempts)
                """,
                (timestamp,),
            ).fetchall()
            connection.execute(
                """
                UPDATE jobs SET status = 'dead_letter', error_code = CASE
                           WHEN expires_at <= ? THEN 'ttl_expired' ELSE 'attempt_cap' END,
                       updated_at = ?
                 WHERE status = 'awaiting_codex'
                       AND (expires_at <= ? OR attempt_count > max_attempts)
                """,
                (timestamp, timestamp, timestamp),
            )
            for row in expired:
                self._insert_delivery_in_transaction(
                    connection,
                    job_id=str(row["job_id"]),
                    destination_id=str(row["source_id"]),
                    message_text=FAILURE_DELIVERY_TEXT,
                    timestamp=timestamp,
                )
            rows = connection.execute(
                "SELECT * FROM jobs WHERE status = 'awaiting_codex' ORDER BY updated_at LIMIT ?",
                (limit,),
            ).fetchall()
            connection.commit()
        return [self._to_claimed_job(row) for row in rows]

    def enqueue_delivery(
        self,
        *,
        job_id: str,
        destination_id: str,
        message_text: str,
        retry_key: str | None = None,
        ttl_seconds: int = 86_400,
        max_attempts: int = 8,
        now: float | None = None,
    ) -> str:
        destination = _require_text(destination_id, "destination_id", MAX_IDENTIFIER_CHARS)
        message = _require_text(message_text, "message_text", MAX_RESULT_CHARS)
        if ttl_seconds < 1 or ttl_seconds > 604_800:
            raise QueueStoreError("ttl_seconds is outside the allowed range")
        if max_attempts < 1 or max_attempts > 20:
            raise QueueStoreError("max_attempts is outside the allowed range")
        delivery_id = str(uuid.uuid4())
        fixed_retry_key = str(uuid.uuid4()) if retry_key is None else _canonical_uuid(retry_key, "retry_key")
        timestamp = time.time() if now is None else float(now)
        with self._connection() as connection:
            connection.execute("BEGIN IMMEDIATE")
            existing = connection.execute(
                "SELECT delivery_id, retry_key FROM deliveries WHERE job_id = ?", (job_id,)
            ).fetchone()
            if existing is not None:
                connection.commit()
                return str(existing["delivery_id"])
            connection.execute(
                """
                INSERT INTO deliveries(
                    delivery_id, job_id, destination_id, message_text, retry_key,
                    status, max_attempts, available_at, created_at, updated_at, expires_at
                ) VALUES (?, ?, ?, ?, ?, 'queued', ?, ?, ?, ?, ?)
                """,
                (
                    delivery_id,
                    job_id,
                    destination,
                    message,
                    fixed_retry_key,
                    max_attempts,
                    timestamp,
                    timestamp,
                    timestamp,
                    timestamp + ttl_seconds,
                ),
            )
            connection.commit()
        return delivery_id

    def prepare_assistant_progress(
        self,
        job_id: str,
        route: str,
        *,
        expected_lease_owner: str,
        now: float | None = None,
    ) -> AssistantProgress:
        canonical_job = _canonical_uuid(job_id, "job_id")
        route_value = _require_text(route, "route", MAX_ROUTE_CHARS)
        if route_value not in {"taichung", "taoyuan"}:
            raise QueueStoreError("assistant progress route is invalid")
        owner = _require_text(expected_lease_owner, "expected_lease_owner", MAX_IDENTIFIER_CHARS)
        timestamp = time.time() if now is None else float(now)
        delivery_id, retry_key = self._stable_progress_identity(
            canonical_job, route_value, identity_version=2
        )
        label = "台中" if route_value == "taichung" else "桃園"
        message = f"[{label} Qwen]\n已接收，正在產生回覆…"
        with self._connection() as connection:
            connection.execute("BEGIN IMMEDIATE")
            job = connection.execute(
                "SELECT source_id FROM jobs WHERE job_id = ? AND status = 'running' AND lease_owner = ?",
                (canonical_job, owner),
            ).fetchone()
            if job is None:
                connection.rollback()
                raise QueueStoreError("job is not owned for assistant progress")
            connection.execute(
                """
                INSERT OR IGNORE INTO assistant_progress(
                    job_id, delivery_id, route, destination_id, message_text,
                    retry_key, identity_version, status, created_at, updated_at
                ) VALUES (?, ?, ?, ?, ?, ?, 2, 'pending', ?, ?)
                """,
                (canonical_job, delivery_id, route_value, str(job["source_id"]), message,
                 retry_key, timestamp, timestamp),
            )
            row = connection.execute(
                "SELECT * FROM assistant_progress WHERE job_id = ? AND route = ?",
                (canonical_job, route_value),
            ).fetchone()
            if row is None:
                connection.rollback()
                raise QueueStoreError("assistant progress insert was not persisted")
            version = int(row["identity_version"])
            expected_delivery, expected_retry = self._stable_progress_identity(
                canonical_job, route_value, identity_version=version
            )
            if (
                str(row["delivery_id"]) != expected_delivery
                or str(row["destination_id"]) != str(job["source_id"])
                or str(row["retry_key"]) != expected_retry
            ):
                connection.rollback()
                raise QueueStoreError("assistant progress identity does not match the job")
            connection.commit()
        return AssistantProgress(
            delivery_id=str(row["delivery_id"]),
            route=str(row["route"]),
            destination_id=str(row["destination_id"]),
            message_text=str(row["message_text"]),
            retry_key=str(row["retry_key"]),
            status=str(row["status"]),
        )

    def finish_assistant_progress(
        self,
        job_id: str,
        route: str,
        *,
        expected_lease_owner: str,
        success: bool,
        permanent: bool = False,
        error_code: str = "",
        now: float | None = None,
    ) -> str:
        canonical_job = _canonical_uuid(job_id, "job_id")
        route_value = _require_text(route, "route", MAX_ROUTE_CHARS)
        if route_value not in {"taichung", "taoyuan"}:
            raise QueueStoreError("assistant progress route is invalid")
        owner = _require_text(expected_lease_owner, "expected_lease_owner", MAX_IDENTIFIER_CHARS)
        code = _require_text(error_code, "error_code", 128, allow_empty=True)
        timestamp = time.time() if now is None else float(now)
        status = "completed" if success else ("dead_letter" if permanent else "pending")
        with self._connection() as connection:
            connection.execute("BEGIN IMMEDIATE")
            owned = connection.execute(
                "SELECT 1 FROM jobs WHERE job_id = ? AND status = 'running' AND lease_owner = ?",
                (canonical_job, owner),
            ).fetchone()
            if owned is None:
                connection.rollback()
                raise QueueStoreError("job lease owner changed before progress completion")
            row = connection.execute(
                "SELECT status FROM assistant_progress WHERE job_id = ? AND route = ?",
                (canonical_job, route_value),
            ).fetchone()
            if row is None or str(row["status"]) not in {"pending", "completed"}:
                connection.rollback()
                raise QueueStoreError("assistant progress is not pending")
            if str(row["status"]) == "completed" and not success:
                connection.rollback()
                raise QueueStoreError("completed assistant progress cannot regress")
            connection.execute(
                "UPDATE assistant_progress SET status = ?, error_code = ?, updated_at = ? WHERE job_id = ? AND route = ?",
                (status, code, timestamp, canonical_job, route_value),
            )
            connection.commit()
        return status


    def claim_reception_notification(
        self, worker_id: str, *, lease_seconds: int = 60, now: float | None = None
    ) -> ClaimedNotification | None:
        owner = _require_text(worker_id, "worker_id", MAX_IDENTIFIER_CHARS)
        timestamp = time.time() if now is None else float(now)
        with self._connection() as connection:
            connection.execute("BEGIN IMMEDIATE")
            connection.execute(
                "UPDATE reception_notifications SET status = CASE WHEN attempt_count >= max_attempts "
                "OR expires_at <= ? THEN 'dead_letter' ELSE 'queued' END, available_at = ?, "
                "lease_until = NULL, lease_owner = '', updated_at = ? WHERE status = 'running' "
                "AND lease_until IS NOT NULL AND lease_until <= ?",
                (timestamp, timestamp, timestamp, timestamp),
            )
            connection.execute(
                "UPDATE reception_notifications SET status = 'dead_letter', updated_at = ? "
                "WHERE status = 'queued' AND expires_at <= ?", (timestamp, timestamp),
            )
            connection.execute(
                "UPDATE jobs SET status = 'dead_letter', error_code = 'reception_notification_failed', "
                "updated_at = ? WHERE status = 'queued' AND job_id IN "
                "(SELECT job_id FROM reception_notifications WHERE status = 'dead_letter')",
                (timestamp,),
            )
            row = connection.execute(
                "SELECT * FROM reception_notifications WHERE status = 'queued' "
                "AND available_at <= ? AND expires_at > ? AND attempt_count < max_attempts "
                "ORDER BY created_at, notification_id LIMIT 1",
                (timestamp, timestamp),
            ).fetchone()
            if row is None:
                connection.commit()
                return None
            changed = connection.execute(
                "UPDATE reception_notifications SET status = 'running', attempt_count = attempt_count + 1, "
                "lease_until = ?, lease_owner = ?, updated_at = ? WHERE notification_id = ? AND status = 'queued'",
                (timestamp + lease_seconds, owner, timestamp, row["notification_id"]),
            ).rowcount
            if changed != 1:
                connection.rollback()
                return None
            claimed = connection.execute(
                "SELECT * FROM reception_notifications WHERE notification_id = ?",
                (row["notification_id"],),
            ).fetchone()
            connection.commit()
        return ClaimedNotification(
            notification_id=str(claimed["notification_id"]), job_id=str(claimed["job_id"]),
            destination_id=str(claimed["destination_id"]), message_text=str(claimed["message_text"]),
            retry_key=str(claimed["retry_key"]), attempt_count=int(claimed["attempt_count"]),
            max_attempts=int(claimed["max_attempts"]), expires_at=float(claimed["expires_at"]),
        )

    def finish_reception_notification(
        self, notification_id: str, *, expected_lease_owner: str, success: bool,
        permanent: bool = False, retry_delay_seconds: int = 5, now: float | None = None,
    ) -> str:
        owner = _require_text(expected_lease_owner, "expected_lease_owner", MAX_IDENTIFIER_CHARS)
        timestamp = time.time() if now is None else float(now)
        with self._connection() as connection:
            connection.execute("BEGIN IMMEDIATE")
            row = connection.execute(
                "SELECT * FROM reception_notifications WHERE notification_id = ? "
                "AND status = 'running' AND lease_owner = ?", (notification_id, owner),
            ).fetchone()
            if row is None:
                connection.rollback()
                raise QueueStoreError("reception notification is not running")
            terminal = (permanent or int(row["attempt_count"]) >= int(row["max_attempts"])
                        or float(row["expires_at"]) <= timestamp + retry_delay_seconds)
            status = "completed" if success else ("dead_letter" if terminal else "queued")
            connection.execute(
                "UPDATE reception_notifications SET status = ?, available_at = ?, lease_until = NULL, "
                "lease_owner = '', updated_at = ? WHERE notification_id = ? AND lease_owner = ?",
                (status, timestamp if status != "queued" else timestamp + retry_delay_seconds,
                 timestamp, notification_id, owner),
            )
            if status == "dead_letter":
                connection.execute(
                    "UPDATE jobs SET status = 'dead_letter', error_code = 'reception_notification_failed', "
                    "updated_at = ? WHERE job_id = ? AND status = 'queued'",
                    (timestamp, row["job_id"]),
                )
            connection.commit()
        return status

    def claim_delivery(
        self, worker_id: str, *, lease_seconds: int = 60, now: float | None = None
    ) -> ClaimedDelivery | None:
        owner = _require_text(worker_id, "worker_id", MAX_IDENTIFIER_CHARS)
        timestamp = time.time() if now is None else float(now)
        with self._connection() as connection:
            connection.execute("BEGIN IMMEDIATE")
            connection.execute(
                """
                UPDATE deliveries
                   SET status = CASE
                           WHEN attempt_count >= max_attempts OR expires_at <= ?
                               THEN 'dead_letter' ELSE 'queued' END,
                       available_at = ?, lease_until = NULL, lease_owner = '', updated_at = ?
                 WHERE status = 'running' AND lease_until IS NOT NULL AND lease_until <= ?
                """,
                (timestamp, timestamp, timestamp, timestamp),
            )
            connection.execute(
                """
                UPDATE deliveries SET status = 'dead_letter', error_code = 'ttl_expired', updated_at = ?
                 WHERE status = 'queued' AND expires_at <= ?
                """,
                (timestamp, timestamp),
            )
            row = connection.execute(
                """
                SELECT * FROM deliveries
                 WHERE status = 'queued' AND available_at <= ? AND expires_at > ?
                       AND attempt_count < max_attempts
                 ORDER BY created_at, delivery_id LIMIT 1
                """,
                (timestamp, timestamp),
            ).fetchone()
            if row is None:
                connection.commit()
                return None
            connection.execute(
                """
                UPDATE deliveries SET status = 'running', attempt_count = attempt_count + 1,
                       lease_until = ?, lease_owner = ?, updated_at = ?
                 WHERE delivery_id = ? AND status = 'queued'
                """,
                (timestamp + lease_seconds, owner, timestamp, row["delivery_id"]),
            )
            claimed = connection.execute(
                "SELECT * FROM deliveries WHERE delivery_id = ?", (row["delivery_id"],)
            ).fetchone()
            connection.commit()
        return ClaimedDelivery(
            delivery_id=str(claimed["delivery_id"]),
            job_id=str(claimed["job_id"]),
            destination_id=str(claimed["destination_id"]),
            message_text=str(claimed["message_text"]),
            retry_key=str(claimed["retry_key"]),
            attempt_count=int(claimed["attempt_count"]),
            max_attempts=int(claimed["max_attempts"]),
            expires_at=float(claimed["expires_at"]),
        )

    def finish_delivery(
        self,
        delivery_id: str,
        *,
        expected_lease_owner: str,
        success: bool,
        permanent: bool = False,
        error_code: str = "",
        retry_delay_seconds: int = 5,
        now: float | None = None,
    ) -> str:
        timestamp = time.time() if now is None else float(now)
        code = _require_text(error_code, "error_code", 128, allow_empty=True)
        owner = _require_text(
            expected_lease_owner, "expected_lease_owner", MAX_IDENTIFIER_CHARS
        )
        with self._connection() as connection:
            connection.execute("BEGIN IMMEDIATE")
            row = connection.execute(
                "SELECT * FROM deliveries WHERE delivery_id = ? AND status = 'running' AND lease_owner = ?",
                (delivery_id, owner),
            ).fetchone()
            if row is None:
                connection.rollback()
                raise QueueStoreError("delivery is not running")
            if success:
                status = "completed"
                available_at = timestamp
            elif (
                permanent
                or int(row["attempt_count"]) >= int(row["max_attempts"])
                or float(row["expires_at"]) <= timestamp + retry_delay_seconds
            ):
                status = "dead_letter"
                available_at = timestamp
            else:
                status = "queued"
                available_at = timestamp + max(0, retry_delay_seconds)
            changed = connection.execute(
                """
                UPDATE deliveries SET status = ?, available_at = ?, lease_until = NULL,
                       lease_owner = '', error_code = ?, updated_at = ?
                 WHERE delivery_id = ? AND status = 'running' AND lease_owner = ?
                """,
                (status, available_at, code, timestamp, delivery_id, owner),
            ).rowcount
            if changed != 1:
                connection.rollback()
                raise QueueStoreError("delivery lease owner changed before completion")
            connection.commit()
        return status

    def counts(self) -> dict[str, int]:
        with self._connection() as connection:
            rows = connection.execute(
                "SELECT status, COUNT(*) AS count FROM jobs GROUP BY status"
            ).fetchall()
        counts = {status: 0 for status in sorted(JOB_STATUSES)}
        counts.update({str(row["status"]): int(row["count"]) for row in rows})
        return counts

    def status(self, job_id: str) -> dict[str, Any] | None:
        _canonical_uuid(job_id, "job_id")
        with self._connection() as connection:
            row = connection.execute(
                """
                SELECT job_id, route, status, attempt_count, max_attempts,
                       created_at, updated_at, expires_at, error_code
                  FROM jobs WHERE job_id = ?
                """,
                (job_id,),
            ).fetchone()
        return None if row is None else dict(row)

    def recent(self, limit: int = 10) -> list[dict[str, Any]]:
        if limit < 1 or limit > 50:
            raise QueueStoreError("limit is outside the allowed range")
        with self._connection() as connection:
            rows = connection.execute(
                """
                SELECT job_id, route, status, attempt_count, created_at, updated_at, error_code
                  FROM jobs ORDER BY created_at DESC LIMIT ?
                """,
                (limit,),
            ).fetchall()
        return [dict(row) for row in rows]

    def export_safe_json(self) -> str:
        """Return aggregate diagnostics without prompts, tokens or destination IDs."""
        return json.dumps({"counts": self.counts()}, ensure_ascii=False, sort_keys=True)
