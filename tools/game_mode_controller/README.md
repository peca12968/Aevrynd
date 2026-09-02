# Aevrynd Game Mode Controller — safe core work package

This package is the first deployable-design slice for controlling Qwen game mode on exactly three Windows PCs. It is intentionally dependency-free Python plus PowerShell and does **not** install a model, stop a process, open a firewall port, register startup, or contact another PC by itself.

## Fixed topology

| Node | Fixed address | Hub path | Restore profile |
| --- | --- | --- | --- |
| `main` | `100.115.174.48`, `192.168.50.18` | loopback | `light` |
| `taichung` | `192.168.50.98` | outbound to `192.168.50.18:8766` | `full` |
| `taoyuan` | `100.89.42.36` | outbound to `100.115.174.48:8766` | `full` |

There is no peer-to-peer control mesh. Taichung and Taoyuan agents poll the main PC hub outbound. The agent listener is fixed to `127.0.0.1:8765`; a hub listener may only use `127.0.0.1`, `192.168.50.18`, or `100.115.174.48` on port `8766`. Wildcard binds and configuration-selected hosts are rejected.

When the hub is unreachable, a node keeps its atomic local `game_mode` intent and reconciles it at the next agent start. `game_mode=true` means Qwen must remain stopped. `game_mode=false` means Qwen is restored using the node's fixed profile and is only reported running after `health`, `models`, single `slots`, and `chat/completions` checks pass. The controller never owns or names Codex and exposes no generic process or shell operation.

## Security model

- Node IDs, IPs, ports, routes, commands (`set game mode` only), and profiles are closed allowlists.
- Each node uses its own HMAC-SHA256 credential. Secrets are injected through the existing `line_gateway.credential_store` pattern and are never accepted in JSON, CLI arguments, state, logs, or this repository.
- Signed requests cover protocol version, actor node, method, path, timestamp, random nonce, and exact body hash. A SQLite unique nonce ledger gives durable replay rejection; accepted responses are signed too.
- Atomic JSON uses a per-path in-process lock plus a cross-process file lock with a bounded timeout, same-directory exclusive temporary files, `fsync`, and `os.replace`; duplicate JSON keys and oversized state are rejected.
- A Qwen process can only be stopped through the single `validate_stop_eligibility` gate. Its persisted PID is mandatory, and Windows owner, executable path, exact argument vector, command digest, live executable SHA-256, and live model SHA-256 must match the closed runtime manifest. PID reuse or any drift fails closed.
- A hub status report/ack may only describe the authenticated node. Any authenticated node may request game mode for one of the three fixed targets, which is the intended cross-PC control policy.
- The main hub writes one durable notification for each observed Qwen transition to `running` or `stopped`. The message contains all three nodes. Pending and dead-letter audit records are never capacity-evicted; only delivered records may be compacted, and a full record/byte-bounded outbox rejects the new transition atomically. Permanent LINE 3xx/4xx becomes an explicit retained dead letter, while pending records older than 24 hours become `line_delivery_expired` without attempting the old retry key. `NoRedirectLineClient` uses a redirect-disabled opener so Authorization can never follow a redirect.

## Modules

- `inventory.py`: immutable node/topology/profile allowlist.
- `models.py`, `atomic_store.py`, `state_machine.py`: validated local state and boot reconciliation.
- `process_guard.py`: PID/owner/path/argv/hash identity gate.
- `acceptance.py`: bounded local Qwen acceptance suite.
- `security.py`: request/response signatures and replay ledger.
- `coordinator.py`, `api.py`: durable hub aggregation, latest-intent command queue, and strict transport-independent API.
- `line_notifications.py`: safe LINE gateway delivery adapter.
- `installer_transaction.py`: single-read pinned closed-source manifest verification, install-wide bounded cross-process locking, same-volume staging, post-copy verification, bounded Windows replace retry, atomic component swaps, rollback, deferred post-install commit, and durable crash recovery.
- `scripts`: installation, desktop launcher, and system-tray UI skeletons.

## Tests

From the `Games` repository root, with Python 3.11 or newer:

```powershell
python -m unittest -v tools.game_mode_controller.tests.test_core
```

The tests use temporary files and fakes only. They do not change services or networking.

To inspect the Windows staging plan without changing the machine:

```powershell
pwsh -NoProfile -File .\tools\game_mode_controller\scripts\Install.ps1 -NodeId main -WhatIf
```

## Deliberately unresolved deployment inputs

The following must be collected on each PC before enabling controls or login startup:

1. An installed, approved `llama-server.exe` and chosen light/full GGUF files. This work package performs no download.
2. Exact SHA-256 hashes, exact argv, expected Windows account, model ID, and confirmed local port for each node's signed runtime manifest. The main profile is always light; the other two are currently fixed full.
3. A per-node 32+ character random secret stored in Windows Credential Manager. The main hub needs a credential for each node; a remote node only needs its own. Credential target naming and ACL rollout must be agreed before deployment.
4. A standard-library Windows process inspector/launcher adapter that can authoritatively obtain PID owner and exact argv, then call the existing process guard. Until this exists, the tray deliberately shows status only and disables controls.
5. The hub HTTP host and outbound polling agent runners, Windows Firewall rules limited to the exact source/interface, and a decision for Taichung connectivity if `192.168.50.98` is not actually on the same routed LAN as `192.168.50.18`. Taichung currently has no confirmed Tailscale route; Taoyuan depends on Tailscale reaching `100.115.174.48`.
6. The LINE destination ID and the already-secured main Gateway access token. Only the main hub may drain notifications.
7. Remote installation access. Neither WinRM nor SSH is available on `.98`/`.36`, so the package cannot yet be copied or installed there automatically.

`Install.ps1` stages the closed core and creates the desktop shortcut only when explicitly run without `-WhatIf`. It intentionally does not register an `AtLogOn` task while the authenticated agent/runtime adapter is absent; this prevents a partially installed controller from claiming that Qwen is managed.
