#pragma once
#include <chrono>
#include <optional>

namespace Mistwatch {
// 僅記錄從本次程式的新巡守開始的時間。讀檔不補算未觀測的前段。
class PlaytestClock {
public:
    using Clock=std::chrono::steady_clock;

    void Start(Clock::time_point now) noexcept {
        elapsed_={};
        activeSince_=now;
        phase_=Phase::Active;
    }

    void Clear() noexcept { *this={}; }

    void SetActive(Clock::time_point now,bool active) noexcept {
        if(phase_==Phase::Active&&!active){
            elapsed_+=now-activeSince_;
            phase_=Phase::Suspended;
        }else if(phase_==Phase::Suspended&&active){
            activeSince_=now;
            phase_=Phase::Active;
        }
    }

    void Finish(Clock::time_point now) noexcept {
        SetActive(now,false);
        if(phase_==Phase::Suspended)phase_=Phase::Complete;
    }

    std::optional<std::chrono::seconds> CompletedTime() const noexcept {
        if(phase_!=Phase::Complete)return std::nullopt;
        return std::chrono::duration_cast<std::chrono::seconds>(elapsed_);
    }

private:
    enum class Phase {Unmeasured,Active,Suspended,Complete};
    Phase phase_{Phase::Unmeasured};
    Clock::time_point activeSince_{};
    Clock::duration elapsed_{};
};
}
