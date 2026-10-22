#pragma once
namespace Mistwatch {
// 暫停選單可保存，但開始、焦點及有效路徑門檻仍完整保留。
inline bool CanSavePausedCheckpoint(bool started,bool paused,bool resumedFromPause,
    bool focused,bool focusLost,bool savePressed,bool hasSavePath)noexcept {
    return started&&paused&&!resumedFromPause&&focused&&!focusLost&&savePressed&&hasSavePath;
}
}
