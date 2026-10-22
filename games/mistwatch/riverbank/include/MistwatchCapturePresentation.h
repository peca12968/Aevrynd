#pragma once
#include <Aevrynd/DiagnosticWindow3D.h>
#include <stdexcept>
#include <string_view>

namespace Mistwatch {
inline bool CaptureBenchmarkRemainsStatic(bool capture,bool benchmark,bool fixedCamera,bool walk,bool series,bool audio,bool wildlife,bool focus)noexcept{
    return capture&&benchmark&&fixedCamera&&!walk&&!series&&!audio&&!wildlife&&!focus;
}
// 只允許獨立、量測中的靜態GPU基準；普通遊玩與原同步模式維持既有設定。
inline bool SelectCaptureBenchmarkPresentation(std::wstring_view option,bool capture,bool cost,bool benchmark,bool fixedCamera,
                                               bool replay,bool legacyWindow,bool legacyPresent){
    if(option.empty())return false;
    if(option!=L"offscreen-unsynchronized"||!capture||!cost||!benchmark||!fixedCamera||replay||legacyWindow||legacyPresent)
        throw std::runtime_error("Capture presentation requires isolated measured static GPU benchmark");
    return true;
}
}
