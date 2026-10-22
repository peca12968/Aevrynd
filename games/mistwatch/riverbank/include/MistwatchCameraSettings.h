#pragma once
#include <algorithm>
#include <charconv>
#include <locale>
#include <sstream>
#include <string>
#include <string_view>

namespace Mistwatch {
// 操作偏好不屬於任務存檔；讀檔、重試及鏡頭交易不應回復玩家的裝置設定。
struct CameraSettings {
    unsigned sensitivityPercent{100};
    bool invertVertical{};
    bool Valid() const noexcept {return sensitivityPercent>=25&&sensitivityPercent<=200;}
    float Sensitivity() const noexcept {return float(sensitivityPercent)/100.f;}
    bool Adjust(bool allowed,bool decrease,bool increase,bool invert,bool reset) noexcept {
        if(!allowed||!Valid())return false;
        auto next=*this;
        if(reset)next={};
        else {
            if(decrease!=increase)next.sensitivityPercent=unsigned(std::clamp(
                int(next.sensitivityPercent)+(increase?25:-25),25,200));
            if(invert)next.invertVertical=!next.invertVertical;
        }
        const bool changed=next.sensitivityPercent!=sensitivityPercent||next.invertVertical!=invertVertical;
        *this=next;return changed;
    }
    std::string MenuLine() const {
        return "鏡頭 "+std::to_string(sensitivityPercent)+"% · [ / ] 調整 · I 垂直反轉："+
            (invertVertical?"開":"關")+" · Backspace 重設";
    }
    std::string Serialize() const {
        if(!Valid())return {};
        return "MISTWATCH_CAMERA 1 "+std::to_string(sensitivityPercent)+" "+(invertVertical?"1":"0");
    }
    bool Restore(std::string_view text) {
        if(text.size()>128)return false;
        std::istringstream in{std::string(text)};in.imbue(std::locale::classic());
        std::string tag,version,percent,inverted,extra;
        if(!(in>>tag>>version>>percent>>inverted)||tag!="MISTWATCH_CAMERA"||version!="1"||
            (inverted!="0"&&inverted!="1")||percent.empty())return false;
        for(char ch:percent)if(ch<'0'||ch>'9')return false;
        CameraSettings next;
        const auto parsed=std::from_chars(percent.data(),percent.data()+percent.size(),next.sensitivityPercent);
        if(parsed.ec!=std::errc{}||parsed.ptr!=percent.data()+percent.size()||!next.Valid()||(in>>extra))return false;
        next.invertVertical=inverted=="1";*this=next;return true;
    }
};
inline bool CanEditCameraSettings(bool focused,bool wasStarted,bool wasPaused,
    bool started,bool paused,bool enterPressed,bool escapePressed) noexcept {
    // 只接受既有選單的獨立按鍵；開始、暫停及恢復當步都不修改操作偏好。
    return focused&&!enterPressed&&!escapePressed&&
        ((!wasStarted&&!started)||(wasStarted&&started&&wasPaused&&paused));
}
}
