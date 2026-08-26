# 本地執行環境

此目錄供本機放置 `llama.cpp` 執行檔與 GGUF 模型，不會提交到 Git。

預期結構：

```text
引擎/runtime/
├─ llama.cpp/
│  └─ llama-server.exe
└─ models/
   └─ Qwen3.8-27B-UD-Q4_K_M.gguf
```

模型與執行檔必須由使用者在自己的電腦下載，遊戲只會呼叫 `127.0.0.1:8081`。
