import pathlib
import unittest


class PackageContractTests(unittest.TestCase):
    def test_installer_is_fail_closed_and_does_not_open_firewall(self):
        root = pathlib.Path(__file__).resolve().parents[1]
        text = (root / "scripts" / "Install-RemoteTestWorker.ps1").read_text(encoding="utf-8")
        self.assertIn("payload_hash_mismatch", text)
        self.assertIn("readiness_rejected", text)
        self.assertIn("payload_file_set_mismatch", text)
        self.assertIn("unsafe_staging_reparse", text)
        self.assertNotIn("New-NetFirewallRule", text)
        self.assertNotIn("Register-ScheduledTask", text)

    def test_package_excludes_secret_material(self):
        root = pathlib.Path(__file__).resolve().parents[1]
        text = (root / "scripts" / "Build-OfflinePackage.ps1").read_text(encoding="utf-8")
        self.assertIn("\\.(zip|db|key)$", text)
        self.assertIn("Phase1-reviewed.zip", text)
        self.assertNotIn("Copy-Item -LiteralPath (Join-Path $moduleRoot 'scripts') -Destination $payloadRoot -Recurse", text)

    def test_readiness_output_gets_restrictive_acl(self):
        root = pathlib.Path(__file__).resolve().parents[1]
        text = (root / "scripts" / "Collect-Readiness.ps1").read_text(encoding="utf-8")
        self.assertIn("/inheritance:r", text)
        self.assertIn("readiness_acl_failed", text)


if __name__ == "__main__": unittest.main()
