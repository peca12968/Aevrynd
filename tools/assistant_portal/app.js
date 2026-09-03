"use strict";

const navItems = [...document.querySelectorAll("[data-view]")];
const panels = [...document.querySelectorAll(".view-panel")];
const pageTitle = document.querySelector("#page-title");
const viewLabel = document.querySelector("#view-label");
const chatForm = document.querySelector("#chat-form");
const messageInput = document.querySelector("#message-input");
const messages = document.querySelector("#messages");

const viewCopy = {
  chat: ["AI 助理控制中心", "CHAT"],
  tasks: ["任務與節點調度", "TASKS"],
  files: ["檔案工作區", "FILES"]
};

function openView(view) {
  navItems.forEach((item) => {
    const isActive = item.dataset.view === view;
    item.classList.toggle("active", isActive);
    isActive ? item.setAttribute("aria-current", "page") : item.removeAttribute("aria-current");
  });
  panels.forEach((panel) => panel.classList.toggle("active", panel.id === `${view}-view`));
  pageTitle.textContent = viewCopy[view][0];
  viewLabel.textContent = viewCopy[view][1];
}

navItems.forEach((item) => item.addEventListener("click", () => openView(item.dataset.view)));
document.querySelectorAll("[data-open-view]").forEach((item) => item.addEventListener("click", () => openView(item.dataset.openView)));

function addMessage(text, role, name) {
  const article = document.createElement("article");
  article.className = `message ${role}`;
  const avatar = document.createElement("span");
  avatar.className = "message-avatar";
  avatar.textContent = role === "user" ? "AX" : "A";
  const body = document.createElement("div");
  const meta = document.createElement("div");
  meta.className = "message-meta";
  const strong = document.createElement("strong");
  strong.textContent = name;
  const time = document.createElement("span");
  time.textContent = "剛剛";
  const paragraph = document.createElement("p");
  paragraph.textContent = text;
  meta.append(strong, time);
  body.append(meta, paragraph);
  article.append(avatar, body);
  messages.append(article);
  messages.scrollTop = messages.scrollHeight;
}

chatForm.addEventListener("submit", (event) => {
  event.preventDefault();
  const text = messageInput.value.trim();
  if (!text) return;
  const assistant = document.querySelector('input[name="assistant"]:checked').value;
  const names = { auto: "自動分流", taichung: "台中助理", taoyuan: "桃園助理", main: "主電腦" };
  addMessage(text, "user", "您");
  messageInput.value = "";
  messageInput.style.height = "auto";
  window.setTimeout(() => addMessage(`這是介面預覽，尚未連接 ${names[assistant]} 的實際服務。`, "assistant", "Aevrynd Assistant"), 280);
});

messageInput.addEventListener("keydown", (event) => {
  if (event.key === "Enter" && !event.shiftKey) {
    event.preventDefault();
    chatForm.requestSubmit();
  }
});
messageInput.addEventListener("input", () => {
  messageInput.style.height = "auto";
  messageInput.style.height = `${Math.min(messageInput.scrollHeight, 120)}px`;
});

document.querySelectorAll(".suggestions button").forEach((button) => {
  button.addEventListener("click", () => {
    messageInput.value = button.textContent;
    messageInput.focus();
  });
});

document.querySelector("#clear-chat").addEventListener("click", () => {
  messages.querySelectorAll(".message.user").forEach((message) => message.remove());
});

const gameModeDialog = document.querySelector("#game-mode-dialog");
document.querySelector("#game-mode-button").addEventListener("click", () => gameModeDialog.showModal());

document.querySelector("#logout-button").addEventListener("click", async () => {
  const csrfResponse = await fetch("/api/auth/csrf", { credentials: "same-origin" });
  const csrf = await csrfResponse.json();
  await fetch("/api/auth/logout", {
    method: "POST",
    credentials: "same-origin",
    headers: { "X-CSRF-Token": csrf.csrf_token }
  });
  window.location.assign("/");
});
