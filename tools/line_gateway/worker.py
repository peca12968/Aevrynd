"""Deterministic queue worker and webhook event sink."""

from __future__ import annotations

import json
import uuid
from collections.abc import Callable, Mapping

from .codex_handoff import CodexHandoff, CodexHandoffError
from .host_tasks import HostTaskError, HostTasks
from .line_client import LineClient, LineClientError
from .queue_store import ClaimedJob, QueueStore, QueueStoreError
from .qwen_client import QwenClient, QwenClientError
from .routing import Route, RouteDecision


HELP_TEXT = "可用指令：@主機 health、@主機 queue、@主機 status <job-id>、@Qwen、@Codex、@全部。"
ACK_TEXT = "已排入處理佇列。"
MAX_LINE_TEXT_CHARS = 5_000
# QwenClient 強制 timeout 不超過 300 秒；保留 60 秒給狀態寫回與本機工作。
WORK_LEASE_SECONDS = 360


class _LeaseLost(RuntimeError):
    pass


class Worker:
    def __init__(
        self,
        queue_store: QueueStore,
        line_client: LineClient,
        host_tasks: HostTasks,
        codex_handoff: CodexHandoff,
        *,
        qwen_client: QwenClient | None,
        worker_id: str | None = None,
        acknowledgement: Callable[[str, str], None] | None = None,
    ) -> None:
        self._queue_store = queue_store
        self._line_client = line_client
        self._host_tasks = host_tasks
        self._codex_handoff = codex_handoff
        self._qwen_client = qwen_client
        self._worker_id = worker_id or f"line-worker-{uuid.uuid4()}"
        self._acknowledgement = acknowledgement

    def accept_event(self, event: Mapping[str, object], decision: RouteDecision) -> bool:
        """Queue an authorized routed event without persisting its raw payload or reply token."""
        if not isinstance(event, Mapping) or not isinstance(decision, RouteDecision):
            return False
        if not decision.authorized or decision.route is Route.IGNORE:
            return True
        event_id = event.get("webhookEventId")
        source = event.get("source")
        if not isinstance(event_id, str) or not isinstance(source, Mapping):
            return False
        source_type = source.get("type")
        requester_id = source.get("userId", "")
        if not isinstance(source_type, str) or not isinstance(requester_id, str):
            return False
        if source_type == "group":
            destination_id = source.get("groupId")
        elif source_type == "user":
            destination_id = requester_id
        else:
            return False
        if not isinstance(destination_id, str) or not destination_id:
            return False
        request_text = decision.command_text
        if decision.route is Route.HELP:
            request_text = "help"
        elif not request_text:
            request_text = "help" if decision.route is Route.HOST else "請說明要處理的工作。"
        try:
            queued = self._queue_store.enqueue(
                webhook_event_id=event_id,
                route=decision.route.value,
                source_id=destination_id,
                requester_id=requester_id,
                request_text=request_text,
            )
        except (QueueStoreError, OSError):
            return False

        # replyToken 僅在此同步 callback 的 process memory 中使用，絕不進 SQLite。
        reply_token = event.get("replyToken")
        if queued.inserted and self._acknowledgement is not None and isinstance(reply_token, str):
            try:
                self._acknowledgement(reply_token, ACK_TEXT)
            except Exception:
                # webhook 已耐久入列；ack 失敗不能造成 LINE redelivery 重複工作。
                pass
        return True

    def process_once(self) -> bool:
        did_work = self._reconcile_codex()
        did_work = self._poll_codex() or did_work
        job = self._queue_store.claim_next(
            self._worker_id, lease_seconds=WORK_LEASE_SECONDS
        )
        if job is not None:
            self._dispatch(job)
            did_work = True
        delivery = self._queue_store.claim_delivery(self._worker_id)
        if delivery is not None:
            self._deliver(delivery.delivery_id, delivery.destination_id, delivery.message_text, delivery.retry_key)
            did_work = True
        return did_work

    def _dispatch(self, job: ClaimedJob) -> None:
        try:
            if job.route == Route.HELP.value:
                self._finish(job, HELP_TEXT)
            elif job.route == Route.HOST.value:
                result = self._host_tasks.execute_text(job.request_text)
                self._finish(job, self._format_json("主機", result))
            elif job.route == Route.QWEN.value:
                self._finish(job, self._run_qwen(job.request_text))
            elif job.route == Route.CODEX.value:
                self._start_codex(job, partial_result="")
            elif job.route == Route.ALL.value:
                # @全部先收集可立即完成的只讀主機狀態與 Qwen，再等待 Codex 回覆。
                self._start_codex(job, partial_result=self._compose_all_partial(job.request_text))
            else:
                self._dead_letter_running(job, "unsupported_route")
        except _LeaseLost:
            # 租約已被其他 worker 接手；舊 owner 不得改寫其結果。
            return
        except (QwenClientError, LineClientError) as exc:
            delay = min(60, 2 ** min(job.attempt_count, 6)) if getattr(exc, "retryable", False) else None
            self._fail_running(job, "upstream_error", retry_delay_seconds=delay)
        except (HostTaskError, CodexHandoffError, ValueError):
            self._dead_letter_running(job, "invalid_job")
        except QueueStoreError:
            # 原子 owner 檢查拒絕過期 worker 時，不再嘗試第二次終態寫入。
            return
        except Exception:
            # 未知錯誤只留下固定代碼，避免 prompt 或 secret 進入資料庫。
            self._fail_running(job, "internal_error", retry_delay_seconds=10)

    def _run_qwen(self, prompt: str) -> str:
        if self._qwen_client is None:
            raise QwenClientError("Qwen is not configured")
        return "[Qwen]\n" + self._qwen_client.chat(prompt).text

    def _compose_all_partial(self, prompt: str) -> str:
        host_result = self._host_tasks.execute("health")
        qwen_result = self._run_qwen(prompt)
        return self._bounded_section(
            self._format_json("主機", host_result) + "\n\n" + qwen_result
        )

    def _start_codex(self, job: ClaimedJob, *, partial_result: str) -> None:
        self._renew_or_raise(job)
        handoff = self._codex_handoff.submit(job.job_id, job.request_text)
        self._renew_or_raise(job)
        self._queue_store.mark_awaiting_codex(
            job.job_id,
            handoff.request_hash,
            partial_result,
            worker_id=self._worker_id,
        )

    def _reconcile_codex(self) -> bool:
        did_work = False
        for job in self._queue_store.claim_reconcilable_codex(
            self._worker_id, lease_seconds=WORK_LEASE_SECONDS
        ):
            try:
                self._renew_or_raise(job)
                handoff = self._codex_handoff.submit(job.job_id, job.request_text)
                self._renew_or_raise(job)
                partial = (
                    self._compose_all_partial(job.request_text)
                    if job.route == Route.ALL.value
                    else job.result_text
                )
                self._renew_or_raise(job)
                self._queue_store.mark_awaiting_codex(
                    job.job_id,
                    handoff.request_hash,
                    partial,
                    worker_id=self._worker_id,
                )
            except _LeaseLost:
                pass
            except (QwenClientError, LineClientError) as exc:
                delay = 10 if getattr(exc, "retryable", False) else None
                self._fail_running(job, "upstream_error", retry_delay_seconds=delay)
            except (CodexHandoffError, HostTaskError, ValueError):
                self._dead_letter_running(job, "codex_handoff_invalid")
            except QueueStoreError:
                # 另一個 worker 已完成 reconcile 時，狀態競爭不應覆寫其結果。
                pass
            did_work = True
        return did_work

    def _poll_codex(self) -> bool:
        did_work = False
        for job in self._queue_store.list_awaiting_codex():
            try:
                response = self._codex_handoff.poll(
                    job.job_id, job.handoff_hash, recover_consumed=True
                )
            except CodexHandoffError:
                try:
                    self._queue_store.fail_awaiting_with_delivery(
                        job.job_id, "codex_handoff_invalid"
                    )
                except QueueStoreError:
                    pass
                did_work = True
                continue
            if response is None:
                continue
            try:
                if response.status == "failed":
                    self._queue_store.fail_awaiting_with_delivery(
                        job.job_id, "codex_reported_failure"
                    )
                else:
                    sections = []
                    if job.result_text:
                        sections.append(job.result_text)
                    sections.append("[Codex]\n" + response.result_text)
                    result = self._aggregate_sections(sections)
                    self._queue_store.complete_awaiting_with_delivery(job.job_id, result)
            except QueueStoreError:
                # 多 worker 同時觀察到同一冪等 response 時，只允許一個完成 transaction。
                pass
            did_work = True
        return did_work

    def _finish(self, job: ClaimedJob, result: str) -> None:
        bounded = self._bounded_line_text(result)
        self._renew_or_raise(job)
        self._queue_store.complete_with_delivery(
            job.job_id, bounded, expected_lease_owner=self._worker_id
        )

    def _renew_or_raise(self, job: ClaimedJob) -> None:
        if not self._queue_store.renew_lease(
            job.job_id, self._worker_id, lease_seconds=WORK_LEASE_SECONDS
        ):
            raise _LeaseLost("job lease ownership changed")

    def _fail_running(
        self, job: ClaimedJob, error_code: str, *, retry_delay_seconds: int | None
    ) -> None:
        try:
            self._queue_store.fail_running_with_delivery(
                job.job_id,
                error_code,
                expected_lease_owner=self._worker_id,
                retry_delay_seconds=retry_delay_seconds,
            )
        except QueueStoreError:
            pass

    def _dead_letter_running(self, job: ClaimedJob, error_code: str) -> None:
        try:
            self._queue_store.dead_letter_with_delivery(
                job.job_id,
                error_code,
                expected_lease_owner=self._worker_id,
            )
        except QueueStoreError:
            pass

    def _deliver(self, delivery_id: str, destination_id: str, message: str, retry_key: str) -> None:
        try:
            self._line_client.push(destination_id, message, retry_key)
        except LineClientError as exc:
            self._queue_store.finish_delivery(
                delivery_id,
                expected_lease_owner=self._worker_id,
                success=False,
                permanent=not exc.retryable,
                error_code="line_rejected" if not exc.retryable else "line_retry",
                retry_delay_seconds=10,
            )
        except Exception:
            self._queue_store.finish_delivery(
                delivery_id,
                expected_lease_owner=self._worker_id,
                success=False,
                error_code="line_internal",
                retry_delay_seconds=10,
            )
        else:
            self._queue_store.finish_delivery(
                delivery_id, expected_lease_owner=self._worker_id, success=True
            )

    @staticmethod
    def _format_json(label: str, value: Mapping[str, object]) -> str:
        return f"[{label}]\n" + json.dumps(value, ensure_ascii=False, sort_keys=True)

    @staticmethod
    def _bounded_line_text(value: str) -> str:
        if len(value) <= MAX_LINE_TEXT_CHARS:
            return value
        return value[: MAX_LINE_TEXT_CHARS - 12] + "\n…內容已截短"

    @staticmethod
    def _bounded_section(value: str, limit: int = 2_400) -> str:
        if len(value) <= limit:
            return value
        return value[: limit - 12] + "\n…內容已截短"

    @classmethod
    def _aggregate_sections(cls, sections: list[str]) -> str:
        # @全部固定保留 Codex 區段空間，避免前置結果把非同步答案完全截掉。
        bounded = [cls._bounded_section(section) for section in sections]
        return cls._bounded_line_text("\n\n".join(bounded))
