"""檢查正式入口部署的封閉檔案、密鑰與傳輸契約。"""
from __future__ import annotations

import re
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SCRIPTS = ROOT / "scripts"


class DeploymentContractTests(unittest.TestCase):
    def test_required_scripts_exist(self) -> None:
        names = {
            "Portal.Common.ps1", "Install.ps1", "Set-GmailCredential.ps1",
            "Start-Portal.ps1", "Stop-Portal.ps1", "Start-Ngrok.ps1",
            "Stop-Ngrok.ps1", "Switch-ToPortal.ps1", "Restore-LineGateway.ps1",
            "Register-Startup.ps1", "Activate.ps1", "Preflight.ps1",
            "Set-NgrokApiCredential.ps1", "Admin-Broker.ps1", "Limited-Task.ps1",
        }
        self.assertEqual(names, {path.name for path in SCRIPTS.glob("*.ps1")})

    def test_production_tuple_is_fixed_and_preview_is_absent(self) -> None:
        start = (SCRIPTS / "Start-Portal.ps1").read_text(encoding="utf-8-sig")
        common = (SCRIPTS / "Portal.Common.ps1").read_text(encoding="utf-8-sig")
        self.assertIn("'--production'", start)
        self.assertNotIn("--dev-show-otp", start)
        self.assertIn("sample-cupid-overdrawn.ngrok-free.dev", common)
        self.assertIn("127.0.0.1", start)
        self.assertIn("8792", common)

    def test_secrets_are_credential_only_and_not_cli_arguments(self) -> None:
        source = "\n".join(path.read_text(encoding="utf-8-sig") for path in SCRIPTS.glob("*.ps1"))
        self.assertIn("Read-Host", source)
        self.assertIn("-AsSecureString", source)
        self.assertIn("CredReadW", source)
        self.assertIn("CredWriteW", source)
        self.assertNotRegex(source, r"(?i)(app.?password|authtoken)\s*=\s*['\"][A-Za-z0-9_-]{16,}")
        self.assertNotIn("ArgumentList.Add(secret)", source)

    def test_switch_has_durable_automatic_rollback_and_explicit_restore(self) -> None:
        switch = (SCRIPTS / "Switch-ToPortal.ps1").read_text(encoding="utf-8-sig")
        rollback = (SCRIPTS / "Restore-LineGateway.ps1").read_text(encoding="utf-8-sig")
        self.assertIn("Write-PortalRuntimeIntent", switch)
        broker = (SCRIPTS / "Admin-Broker.ps1").read_text(encoding="utf-8-sig")
        self.assertNotIn("Set-LineBaseline", switch)
        self.assertIn("Admin-Broker.ps1", rollback)
        self.assertIn("Set-LineBaseline", broker)
        self.assertLess(broker.index("$journal=Read-PortalSwitchState"), broker.index("Get-LineServiceTruth"))
        self.assertIn("Disable-PortalStartupTask", broker)
        self.assertIn("Protect-PrivilegedStateFile", (SCRIPTS / "Portal.Common.ps1").read_text(encoding="utf-8-sig"))
        self.assertNotIn("Read-PortalSwitchState", switch)

    def test_launch_failures_kill_children_and_startup_is_not_elevated(self) -> None:
        common = (SCRIPTS / "Portal.Common.ps1").read_text(encoding="utf-8-sig")
        for name in ("Start-Portal.ps1", "Start-Ngrok.ps1"):
            source = (SCRIPTS / name).read_text(encoding="utf-8-sig")
            self.assertIn("Protect-PortalStateFile", source)
            self.assertIn("Invoke-ManagedProcessLaunchGuard", source)
            self.assertIn("LaunchIntent", source)
            self.assertIn("Stop-ProtectedPortalLaunchTree", source)
        self.assertIn("$process.Kill($true)", common)
        self.assertIn("Get-NetTCPConnection -State Listen -LocalPort $script:PortalPort", common)
        self.assertIn("Name='ngrok.exe'", common)
        self.assertIn("safe stage", (SCRIPTS / "Start-Portal.ps1").read_text(encoding="utf-8-sig"))
        startup = (SCRIPTS / "Register-Startup.ps1").read_text(encoding="utf-8-sig")
        self.assertIn("-RunLevel Limited", startup)
        self.assertIn("-RunLevel Highest", startup)
        self.assertIn("Admin-Broker.ps1", startup)
        self.assertIn("Aevrynd Assistant Portal'", startup)
        self.assertIn("Unregister-ScheduledTask", startup)
        for name in ("Start-Portal.ps1", "Start-Ngrok.ps1", "Switch-ToPortal.ps1"):
            self.assertIn("Assert-StandardUserProcess", (SCRIPTS / name).read_text(encoding="utf-8-sig"))
        self.assertIn("Assert-Administrator", (SCRIPTS / "Restore-LineGateway.ps1").read_text(encoding="utf-8-sig"))

    def test_admin_and_limited_roles_are_separated(self) -> None:
        activate = (SCRIPTS / "Activate.ps1").read_text(encoding="utf-8-sig")
        broker = (SCRIPTS / "Admin-Broker.ps1").read_text(encoding="utf-8-sig")
        limited = (SCRIPTS / "Limited-Task.ps1").read_text(encoding="utf-8-sig")
        self.assertNotIn("StartPortal", activate)
        self.assertNotIn("pythonExe", broker)
        self.assertIn("Assert-Administrator", broker)
        self.assertIn("Assert-StandardUserProcess", limited)
        self.assertIn("Invoke-LimitedPortalTask -Operation Preflight", activate)
        self.assertLess(activate.index("-Operation Recover"), activate.index("Register-Startup.ps1"))

    def test_zero_publish_domain_authorization_and_persistent_diagnostics(self) -> None:
        common = (SCRIPTS / "Portal.Common.ps1").read_text(encoding="utf-8-sig")
        server = (ROOT / "server.py").read_text(encoding="utf-8-sig")
        self.assertIn("api.ngrok.com/reserved_domains", common)
        self.assertIn("ConvertFrom-Json", common)
        self.assertIn("Assert-NgrokPaginationUri", common)
        self.assertIn("AEVRYND_SMTP_DIAGNOSTIC_PATH", common)
        self.assertIn("/api/health/email", server)
        self.assertIn("SmtpReadinessJournal", server)
        gmail = (SCRIPTS / "Set-GmailCredential.ps1").read_text(encoding="utf-8-sig")
        ngrok_api = (SCRIPTS / "Set-NgrokApiCredential.ps1").read_text(encoding="utf-8-sig")
        for source in (gmail, ngrok_api):
            self.assertIn("-AsSecureString", source)
            self.assertIn("Available", source)
            self.assertIn("Existing", source)

    def test_timeout_termination_and_privileged_journal_split(self) -> None:
        common = (SCRIPTS / "Portal.Common.ps1").read_text(encoding="utf-8-sig")
        install = (SCRIPTS / "Install.ps1").read_text(encoding="utf-8-sig")
        self.assertIn("Stop-ScheduledTask", common)
        self.assertIn("could not be terminated", common)
        self.assertIn("privileged\\switch.json", common)
        self.assertIn("state\\intent.json", common)
        self.assertIn("Set-CanonicalPrivilegedAcl", install)
        self.assertIn("Stop-ProtectedPortalLaunchTree", common)

    def test_gmail_password_and_privileged_acl_are_canonical(self) -> None:
        common = (SCRIPTS / "Portal.Common.ps1").read_text(encoding="utf-8-sig")
        gmail = (SCRIPTS / "Set-GmailCredential.ps1").read_text(encoding="utf-8-sig")
        self.assertIn("NormalizeGmail", common)
        self.assertIn("ValidGmail", common)
        self.assertIn("StoreGmail", gmail)
        self.assertIn("-Replace", gmail)
        self.assertIn("exactly 16 ASCII letters or digits", gmail)
        self.assertIn("AreAccessRulesProtected", common)
        self.assertIn("InheritanceFlags]::None", common)
        self.assertIn("PropagationFlags]::None", common)

    def test_grouped_gmail_credentials_are_not_reused(self) -> None:
        common = (SCRIPTS / "Portal.Common.ps1").read_text(encoding="utf-8-sig")
        gmail = (SCRIPTS / "Set-GmailCredential.ps1").read_text(encoding="utf-8-sig")
        self.assertIn("if(bytes!=32)return false", common)
        self.assertIn("value.Length!=16", common)
        self.assertIn("ValidGmail($script:GmailCredentialTarget)", gmail)
        self.assertIn("Existing Gmail credential is invalid", gmail)

    def test_privileged_state_file_acl_rejects_extra_rules(self) -> None:
        common = (SCRIPTS / "Portal.Common.ps1").read_text(encoding="utf-8-sig")
        self.assertIn("$rules.Count-ne2", common)
        self.assertIn("AccessControlType]::Allow", common)
        self.assertIn("$_.IdentityReference.Value-notin$allowed", common)

    def test_ngrok_requires_local_api_and_public_instance_marker(self) -> None:
        common = (SCRIPTS / "Portal.Common.ps1").read_text(encoding="utf-8-sig")
        self.assertIn("127.0.0.1:4040/api/tunnels", common)
        self.assertIn("public_url", common)
        self.assertIn("config.addr", common)
        self.assertIn("aevrynd-instance", common)

    def test_installer_closed_allowlists_include_every_runtime_file(self) -> None:
        install = (SCRIPTS / "Install.ps1").read_text(encoding="utf-8-sig")
        for name in ("server.py", "index.html", "styles.css", "app.js", "Activate.ps1", "Admin-Broker.ps1", "Limited-Task.ps1", "Set-NgrokApiCredential.ps1", "Restore-LineGateway.ps1", "ngrok-package-identity.json"):
            self.assertIn(f"'{name}'", install)
        self.assertIn("Get-VerifiedFileHash", install)


if __name__ == "__main__":
    unittest.main()
