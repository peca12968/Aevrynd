"""檢查入口網站靜態契約，不依賴第三方測試套件。"""

from __future__ import annotations

import re
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


class PortalContractTests(unittest.TestCase):
    def test_required_files_exist(self) -> None:
        for name in ("index.html", "styles.css", "app.js", "auth_adapter.js", "auth_login.js", "server.py", "README.md"):
            self.assertTrue((ROOT / name).is_file(), name)

    def test_no_external_assets(self) -> None:
        html = (ROOT / "index.html").read_text(encoding="utf-8")
        self.assertNotRegex(html, r"(?:src|href)=[\"']https?://")
        self.assertNotIn("cdn", html.lower())

    def test_preview_data_is_disclosed(self) -> None:
        html = (ROOT / "index.html").read_text(encoding="utf-8")
        self.assertGreaterEqual(len(re.findall(r"preview", html, re.IGNORECASE)), 8)
        self.assertIn("尚未連接即時資料來源", html)

    def test_email_auth_is_primary_and_phone_is_disabled(self) -> None:
        html = (ROOT / "index.html").read_text(encoding="utf-8")
        self.assertIn("Email OTP", html)
        self.assertIn("8 位英數驗證碼", html)
        self.assertRegex(html, r'<button[^>]+disabled[^>]*>電話 OTP')
        self.assertIn("NO EMAIL SENT", html)

    def test_browser_storage_is_not_used_for_credentials(self) -> None:
        scripts = "\n".join(path.read_text(encoding="utf-8") for path in (ROOT / "app.js", ROOT / "auth_adapter.js", ROOT / "auth_login.js"))
        self.assertNotIn("localStorage", scripts)
        self.assertNotIn("sessionStorage", scripts)

    def test_allowlist_account_is_not_exposed_in_html_or_javascript(self) -> None:
        client_source = "\n".join(path.read_text(encoding="utf-8") for path in (ROOT / "index.html", ROOT / "app.js", ROOT / "auth_adapter.js", ROOT / "auth_login.js"))
        self.assertNotIn("slime12968@gmail.com", client_source.lower())

    def test_landmarks_and_accessible_controls(self) -> None:
        html = (ROOT / "index.html").read_text(encoding="utf-8")
        for marker in ("<main", "<nav", "<h1", "aria-live=", "aria-label=", "skip-link"):
            self.assertIn(marker, html)
        self.assertNotRegex(html, r"<img(?![^>]+alt=)")

    def test_responsive_and_reduced_motion_rules(self) -> None:
        css = (ROOT / "styles.css").read_text(encoding="utf-8")
        self.assertIn("@media (max-width: 720px)", css)
        self.assertIn("prefers-reduced-motion", css)
        self.assertIn(":focus-visible", css)


if __name__ == "__main__":
    unittest.main()
