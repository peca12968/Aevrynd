#pragma once
#include <stdexcept>
#include <string_view>

namespace Mistwatch {
// 呼叫端須先確認路徑／CLI 與字串讀取完整；本函式不做 I/O 或推測被截斷前的內容。
// 錯誤優先序固定：字串、隔離模式衝突、手動模式衝突、缺少隔離上下文。
[[nodiscard]] inline bool ResolveWoodlandCandidate(bool validCapture,bool validReplay,
    bool manualCandidate,std::wstring_view requested) {
    const bool candidate=requested==L"woodland-canopy-author-v2";
    if(!requested.empty()&&requested!=L"original"&&!candidate)
        throw std::invalid_argument("WOODLAND_PROFILE_INVALID");
    if(validCapture&&validReplay)
        throw std::invalid_argument("WOODLAND_CONTEXT_CONFLICT");
    if(manualCandidate&&(validCapture||validReplay))
        throw std::invalid_argument("WOODLAND_MANUAL_CONTEXT_CONFLICT");
    // 明確 CLI 是唯一非隔離啟用方式；已識別的環境字串不否決這項手動請求。
    if(manualCandidate)return true;
    if(!candidate)return false;
    if(!validCapture&&!validReplay)
        throw std::invalid_argument("WOODLAND_ISOLATION_REQUIRED");
    return true;
}
}
