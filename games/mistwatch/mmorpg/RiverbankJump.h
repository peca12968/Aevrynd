#pragma once

#include "RiverbankEncounter.h"
#include <optional>

namespace Mistwatch {
class RiverbankJump {
public:
    static constexpr unsigned FlightTicks=16;
    static constexpr float MaximumHeightMeters=.72f;

    bool Active() const noexcept { return age_!=0; }

    float Advance(std::optional<bool> acceptedJump, bool movementEnabled, bool canTakeOff) noexcept {
        const bool pressed=acceptedJump.value_or(false)&&!held_;
        // 缺少輸入的週期不算鬆開。保持按住或重送封包都不能連續起跳。
        if(acceptedJump)held_=*acceptedJump;
        if(!movementEnabled){
            age_=0;
            return 0.f;
        }
        if(Active()){
            if(++age_>=FlightTicks)age_=0;
        }else if(canTakeOff&&pressed){
            age_=1;
        }
        return Height(age_);
    }

    static constexpr float Height(unsigned age) noexcept {
        const float t=static_cast<float>(age)/FlightTicks;
        return 4.f*MaximumHeightMeters*t*(1.f-t);
    }

private:
    unsigned age_{};
    bool held_{};
};
}
