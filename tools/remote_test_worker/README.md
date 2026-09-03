# Aevrynd Remote Test Worker — Phase 1

This package defines a transport-independent, fail-closed protocol for two remote test nodes.
It does not create a listener, service, scheduled task, firewall rule, or credential. A later
phase must add an authenticated transport without widening the signed job schema.

Allowed jobs contain only a commit SHA, one fixed preset/label pair, an allowed timeout, node
identity, nonce, and short expiry. There is no command, path, environment, target, or extra-args
field. Receipts are HMAC authenticated and replay-protected and contain only bounded metadata.

Current repository readiness is expected to be false until the repository itself supplies the
`windows-release-gates` CMake configure/build presets and the exact `release-gates` CTest label.
The worker deliberately has no permissive fallback.

Run mock-only tests:

```powershell
python tools/remote_test_worker/run_tests.py
```

Collect a sanitized readiness manifest on either remote node:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\Collect-Readiness.ps1 -NodeId taichung
```

Build one credential-free ZIP suitable for either node:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\Build-OfflinePackage.ps1
```
