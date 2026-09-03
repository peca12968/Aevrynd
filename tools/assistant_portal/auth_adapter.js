"use strict";

/**
 * 驗證介面的共同契約。正式版可換成 FirebaseAuthAdapter 或 SmtpAuthAdapter，
 * 畫面層不需要接觸供應商的憑證或實作細節。
 */
class PortalAuthAdapter {
  constructor() {
    this.csrfToken = "";
  }

  async initialize() {
    const response = await fetch("/api/auth/csrf", { credentials: "same-origin" });
    const data = await response.json();
    this.csrfToken = data.csrf_token;
  }

  async requestEmailOtp(email) {
    return this.#post("/api/auth/request-otp", { email });
  }

  async verifyEmailOtp(email, otp) {
    return this.#post("/api/auth/verify-otp", { email, otp });
  }

  async #post(path, body) {
    if (!this.csrfToken) await this.initialize();
    const response = await fetch(path, {
      method: "POST",
      credentials: "same-origin",
      headers: { "Content-Type": "application/json", "X-CSRF-Token": this.csrfToken },
      body: JSON.stringify(body)
    });
    const data = await response.json();
    return { ...data, ok: response.ok };
  }
}

window.aevryndAuth = new PortalAuthAdapter();
