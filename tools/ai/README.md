# Remote Qwen Dispatcher

`remote_qwen_dispatcher.py` is an explicit, read-only helper for small evidence-based tasks. It does not run continuously and cannot read files, execute commands, call Git, or write the repository.

Allowed task types are `summary`, `classification`, `test-template`, and `inventory`. Input is literal text capped at 8 KiB. Jobs, leases, receipts, and validated output are stored only in a dedicated SQLite database under the current user's local application data directory.

```powershell
python tools/ai/remote_qwen_dispatcher.py submit --task-type summary --evidence "Measured facts to summarize"
python tools/ai/remote_qwen_dispatcher.py run-one --deadline-seconds 45
python tools/ai/remote_qwen_dispatcher.py status --task-id "<UUID>"
```

`run-one` alternates between the configured Taichung and Taoyuan nodes and fails over when the selected node is busy or temporarily unavailable. Its total deadline defaults to 45 seconds and cannot exceed 60 seconds; the first node cannot consume the entire deadline. Ctrl+C immediately returns an in-progress task to the durable queue. Output is intentionally capped at 64 tokens because these 27B workers are not suitable for long generation through this lane. Credentials are read through the existing Windows Credential Manager targets. CLI output contains only stable node labels and never prints credentials or endpoint addresses.

All model output is advisory and must pass the fixed JSON schema before it is stored. Main Codex remains responsible for repository reads, writes, tests, review, and Git operations.
