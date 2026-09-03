"use strict";

const { chromium } = require("playwright");

async function main() {
  const browser = await chromium.launch({
    headless: true,
    executablePath: "C:\\Program Files\\Google\\Chrome\\Application\\chrome.exe"
  });
  try {
    for (const viewport of [{ width: 1440, height: 1000 }, { width: 390, height: 844 }]) {
      const page = await browser.newPage({ viewport });
      await page.goto(process.env.AEVRYND_PORTAL_URL || "http://127.0.0.1:8790/", { waitUntil: "networkidle" });
      const dimensions = await page.evaluate(() => ({ width: innerWidth, scrollWidth: document.documentElement.scrollWidth }));
      if (dimensions.scrollWidth > dimensions.width) throw new Error(`Horizontal overflow at ${viewport.width}px: ${JSON.stringify(dimensions)}`);
      await page.getByRole("heading", { name: "登入助理控制中心" }).waitFor();
      await page.close();
    }

    const page = await browser.newPage({ viewport: { width: 1280, height: 900 } });
    await page.goto(process.env.AEVRYND_PORTAL_URL || "http://127.0.0.1:8790/", { waitUntil: "networkidle" });
    await page.getByLabel("Email").fill("SLIME12968@GMAIL.COM");
    await page.getByRole("button", { name: "請求 8 位驗證碼" }).click();
    await page.locator("#auth-result-detail").filter({ hasText: "Preview 驗證碼" }).waitFor();
    const detail = await page.locator("#auth-result-detail").textContent();
    const match = detail && detail.match(/Preview 驗證碼：([A-Z2-9]{8})/);
    if (!match) throw new Error("Preview OTP was not displayed explicitly.");
    await page.getByLabel("8 位英數驗證碼").fill("AAAAAAAA");
    await page.getByRole("button", { name: "驗證並登入" }).click();
    await page.getByText("驗證失敗或驗證碼已失效。").waitFor();
    await page.getByLabel("8 位英數驗證碼").fill(match[1]);
    await page.getByRole("button", { name: "驗證並登入" }).click();
    await page.getByRole("heading", { name: "AI 助理控制中心" }).waitFor();
    console.log("Browser checks passed: desktop, mobile, OTP rejection, OTP success.");
  } finally {
    await browser.close();
  }
}

main().catch((error) => {
  console.error(error);
  process.exitCode = 1;
});
