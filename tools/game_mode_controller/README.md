# Aevrynd Game Mode Controller — main-node deployable stage

This package controls Qwen game mode on exactly three Windows PCs. The current deployable stage runs the authenticated loopback hub and agent on `main`, safely adopts the existing lightweight Qwen service, exposes signed tray commands for all three fixed targets, and can register limited `AtLogOn` startup. It does **not** install a model, open a firewall port, or contact another PC until the corresponding remote agent stage is deployed.

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
- `process_guard.py`: pinned runtime-manifest loader and PID/owner/path/argv/hash identity gate.
- `windows_runtime.py`: Win32/CIM process inspector, exact launcher, legacy-light adoption, same-handle termination, and manifest capture.
- `credential_store.py`: fixed Windows Credential Manager readers for node HMAC and lightweight-Qwen API credentials.
- `acceptance.py`: bounded local Qwen acceptance suite.
- `security.py`: request/response signatures and replay ledger.
- `coordinator.py`, `api.py`: durable hub aggregation, latest-intent command queue, and strict transport-independent API.
- `line_notifications.py`: safe LINE gateway delivery adapter.
- `installer_transaction.py`: single-read pinned closed-source manifest verification, install-wide bounded cross-process locking, same-volume staging, post-copy verification, bounded Windows replace retry, atomic component swaps, rollback, deferred post-install commit, and durable crash recovery.
- `deployment_transaction.py`: one durable commit decision spanning core components, `node.json`, pinned runtime manifest, and desktop shortcut.
- `runner.py`: signed loopback HTTP hub, main agent reconciliation loop, and tray CLI.
- `scripts`: transactional installation, runtime capture, desktop launcher, actionable tray, and startup registration.

## Tests

From the `Games` repository root, with Python 3.11 or newer:

```powershell
& 'C:\Path\To\Python312\python.exe' -B -m unittest -v tools.game_mode_controller.tests.test_core tools.game_mode_controller.tests.test_runtime
```

The tests use temporary files and fakes only. They do not change services or networking.

To inspect the Windows staging plan without changing the machine:

```powershell
pwsh -NoProfile -File .\tools\game_mode_controller\scripts\Install.ps1 -NodeId main -WhatIf
```

`-WhatIf` returns before privilege, credential, runtime-manifest, filesystem, shortcut, process, or scheduled-task mutation.

## Main deployment sequence

Prerequisites are Python 3.11+, PowerShell 7, the already installed lightweight Qwen service on `127.0.0.1:18080`, its fixed `Aevrynd/QwenLight/ApiKey` credential, and a 32+ character generic credential named `Aevrynd/GameModeController/main`. The existing service should be running while its identity is captured.

```powershell
$python = 'C:\Users\USER\AppData\Local\Programs\Python\Python312\python.exe'
$manifest = Join-Path $env:TEMP 'aevrynd-main-runtime.json'
pwsh -NoProfile -File .\tools\game_mode_controller\scripts\Capture-MainRuntimeManifest.ps1 `
  -PythonExe $python -OutputPath $manifest `
  -ModelId 'C:\Users\USER\AppData\Local\AevryndQwenLight\models\Qwen3-4B-Q4_K_M.gguf'
$manifestSha256 = (Get-FileHash -Algorithm SHA256 -LiteralPath $manifest).Hash.ToLowerInvariant()
pwsh -NoProfile -File .\tools\game_mode_controller\scripts\Install.ps1 `
  -NodeId main -PythonExe $python -RuntimeManifestPath $manifest `
  -RuntimeManifestSha256 $manifestSha256 -RegisterStartup -WhatIf
```

Remove `-WhatIf` only after reviewing the exact target. The installer transaction verifies the closed source manifest, re-verifies the pinned runtime manifest and both large runtime files, stages and atomically swaps the application, stores only manifest metadata (never secrets), and optionally registers one limited current-user logon task. It does not stop Qwen during installation. One durable commit intent covers core, `node.json`, runtime manifest, and shortcut: pre-intent power loss rolls all four back; post-intent power loss completes all four forward. Before creating the leaf, elevated installation resolves the Windows CommonApplicationData known folder, rejects a reparse point at every existing `ProgramData\Aevrynd\GameModeController` ancestor, and rejects an existing `Aevrynd` or controller directory with an untrusted owner or writer. Missing ancestors are created one level at a time and immediately sealed before the next child is created. Installation replaces Aevrynd/root/app/scripts/config/manifests DACLs with SYSTEM/Administrators write plus current-user read/execute; only `state` grants current-user Modify. Startup registration independently rechecks ProgramData, Aevrynd, every executable/config component, owner, DACL, reparse attributes, installed-manifest digest, and launcher digest.

After an authorized real install, launch `Aevrynd Game Mode` from the desktop or start the scheduled task. The tray has an enable action (`game_mode=true`, validated Qwen stop) and a disable action (`game_mode=false`, exact manifest launch plus health/models/single-slot/chat acceptance) for each node. Until remote agents are installed, Taichung and Taoyuan commands remain durable pending intents and their status remains offline.

Read-only or reversible verification after installation:

```powershell
$root = "$env:ProgramData\Aevrynd\GameModeController"
$config = Get-Content -Raw -LiteralPath "$root\config\node.json" | ConvertFrom-Json
$env:PYTHONPATH = "$root\app"
& $config.pythonExecutable -B -m tools.game_mode_controller.runner status
Get-ScheduledTask -TaskName 'Aevrynd Game Mode Controller' | Select-Object TaskName,State
Test-NetConnection 127.0.0.1 -Port 8766
```

## Remaining remote-node inputs

The following must be collected on each PC before enabling controls or login startup:

1. Installed, approved full-profile runtimes and signed runtime manifests on Taichung and Taoyuan. This package performs no download.
2. A unique 32+ character `Aevrynd/GameModeController/<node>` credential on every node; the main hub additionally needs the two remote credentials before accepting their reports.
3. Remote authenticated polling runners and source/interface-scoped firewall rules. The current main listener intentionally remains loopback-only, so there is no remote control surface yet.
4. Confirmed routed connectivity for Taichung and Tailscale reachability for Taoyuan.
5. Remote installation access. Neither WinRM nor SSH is currently available on `.98`/`.36`.

The main agent never searches for or terminates a process by name. A stop opens one process handle first, verifies PID/liveness/owner/image/argv/executable SHA-256/command digest/model SHA-256 while retaining that handle, and terminates only that same handle. PID reuse or drift degrades the controller and leaves the replacement process running. Qwen, runner, and tray children receive explicit environment allowlists; controller, LINE, OpenAI, and unrelated Qwen secrets are not inherited.
