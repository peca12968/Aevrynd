# Remote Qwen Dispatcher

`remote_qwen_dispatcher.py` is an explicit, read-only helper for small evidence-based tasks. It does not run continuously and cannot read files, execute commands, call Git, or write the repository.

Allowed task types are `summary`, `classification`, `test-template`, and `inventory`. Input is literal text capped at 8 KiB. Jobs, leases, receipts, and validated output are stored only in a dedicated SQLite database under the current user's local application data directory.

```powershell
python tools/ai/remote_qwen_dispatcher.py submit --task-type summary --evidence "Measured facts to summarize"
python tools/ai/remote_qwen_dispatcher.py run-one --deadline-seconds 60
python tools/ai/remote_qwen_dispatcher.py status --task-id "<UUID>"
```

`run-one` alternates between the configured Taichung and Taoyuan nodes and fails over when the selected node is busy or temporarily unavailable. Its total deadline defaults to and cannot exceed 60 seconds; the first node cannot consume more than 58 seconds. Ctrl+C immediately returns an in-progress task to the durable queue. Thinking is disabled through the llama.cpp chat-template contract, output is constrained by a fixed JSON schema plus task-specific concise limits, and generation is capped at 160 tokens. Credentials are read through the existing Windows Credential Manager targets. CLI output contains only stable node labels and never prints credentials or endpoint addresses. Failure receipts distinguish busy nodes, transport timeout/unavailability, output truncation, invalid structured output, and local configuration rejection without exposing response text.

All model output is advisory and must pass the fixed JSON schema before it is stored. Main Codex remains responsible for repository reads, writes, tests, review, and Git operations.
