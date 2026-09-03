"use strict";

const authForm = document.querySelector("#auth-form");
const authResult = document.querySelector("#auth-result");
const authTitle = document.querySelector("#auth-result-title");
const authDetail = document.querySelector("#auth-result-detail");

function showAuthResult(title, detail) {
  authTitle.textContent = title;
  authDetail.textContent = detail;
  authResult.hidden = false;
}

document.querySelector("#request-otp").addEventListener("click", async () => {
  if (!authForm.reportValidity()) return;
  const button = document.querySelector("#request-otp");
  button.disabled = true;
  const label = button.querySelector("span");
  label.textContent = "處理中…";
  try {
    const result = await window.aevryndAuth.requestEmailOtp(document.querySelector("#email-input").value);
    if (!result.ok) throw new Error("OTP delivery failed");
    document.querySelector("#otp-stage").hidden = false;
    const previewCode = result.preview_code ? ` Preview 驗證碼：${result.preview_code}` : "";
    showAuthResult("請求已受理", `${result.message}${previewCode}`);
    document.querySelector("#otp-input").focus();
    let seconds = Number(result.retry_after || 60);
    label.textContent = `${seconds} 秒後可重寄`;
    const timer = window.setInterval(() => {
      seconds -= 1;
      label.textContent = seconds > 0 ? `${seconds} 秒後可重寄` : "重新請求驗證碼";
      if (seconds <= 0) {
        window.clearInterval(timer);
        button.disabled = false;
      }
    }, 1000);
  } catch {
    showAuthResult("暫時無法處理", "請確認本機 Preview Server 正在執行後再試一次。");
    button.disabled = false;
    label.textContent = "請求 8 位驗證碼";
  }
});

authForm.addEventListener("submit", async (event) => {
  event.preventDefault();
  const email = document.querySelector("#email-input").value;
  const otp = document.querySelector("#otp-input").value;
  const button = authForm.querySelector("button[type='submit']");
  button.disabled = true;
  document.querySelector("#auth-submit-label").textContent = "Preview 驗證中…";
  const result = await window.aevryndAuth.verifyEmailOtp(email, otp);
  if (result.authenticated) {
    showAuthResult("Preview 驗證完成", "已建立安全工作階段，正在開啟控制中心。");
    window.setTimeout(() => window.location.assign("/portal"), 350);
  } else {
    showAuthResult("無法驗證", result.message);
    button.disabled = false;
    document.querySelector("#auth-submit-label").textContent = "驗證並登入";
  }
});
