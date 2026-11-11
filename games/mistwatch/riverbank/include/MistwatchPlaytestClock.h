#pragma once
#include <array>
#include <chrono>
#include <optional>
#include <string>

namespace Mistwatch {
// 僅記錄從本次程式的新巡守開始的時間。讀檔不補算未觀測的前段。
class PlaytestClock {
public:
    using Clock=std::chrono::steady_clock;
    enum class Leg { BridgePreparation, Encounter, SettlementPower, RescueSignal, Count };
    using LegSeconds=std::array<std::chrono::seconds,std::size_t(Leg::Count)>;

    void Start(Clock::time_point now) noexcept {
        elapsed_={};
        legs_={};
        leg_=Leg::BridgePreparation;
        activeSince_=now;
        phase_=Phase::Active;
    }

    void Clear() noexcept { *this={}; }

    void SetActive(Clock::time_point now,bool active) noexcept {
        if(phase_==Phase::Active&&!active){
            AccumulateUntil(now);
            phase_=Phase::Suspended;
        }else if(phase_==Phase::Suspended&&active){
            activeSince_=now;
            phase_=Phase::Active;
        }
    }

    void EnterLeg(Clock::time_point now,Leg next) noexcept {
        if(phase_!=Phase::Active||next==leg_||next>=Leg::Count)return;
        AccumulateUntil(now);
        leg_=next;
    }

    void Finish(Clock::time_point now) noexcept {
        SetActive(now,false);
        if(phase_==Phase::Suspended)phase_=Phase::Complete;
    }

    std::optional<std::chrono::seconds> CompletedTime() const noexcept {
        if(phase_!=Phase::Complete)return std::nullopt;
        return std::chrono::duration_cast<std::chrono::seconds>(elapsed_);
    }

    std::optional<LegSeconds> CompletedLegs() const noexcept {
        if(phase_!=Phase::Complete)return std::nullopt;
        LegSeconds result{};
        Clock::duration elapsed{};
        std::chrono::seconds previous{};
        // 各段採用累計邊界的整秒差，畫面上的各段合計會等於總計。
        for(std::size_t index=0;index<result.size();++index){
            elapsed+=legs_[index];
            const auto boundary=std::chrono::duration_cast<std::chrono::seconds>(elapsed);
            result[index]=boundary-previous;
            previous=boundary;
        }
        return result;
    }

    std::string CompletedSummary() const {
        const auto total=CompletedTime();
        const auto legs=CompletedLegs();
        if(!total||!legs)return {};
        return "MISTWATCH_PLAYTEST 1\nactive_s="+std::to_string(total->count())+
            "\nbridge_s="+std::to_string((*legs)[0].count())+
            "\nencounter_s="+std::to_string((*legs)[1].count())+
            "\npower_s="+std::to_string((*legs)[2].count())+
            "\nsignal_s="+std::to_string((*legs)[3].count())+"\n";
    }

private:
    enum class Phase {Unmeasured,Active,Suspended,Complete};
    void AccumulateUntil(Clock::time_point now) noexcept {
        const auto delta=now-activeSince_;
        elapsed_+=delta;
        legs_[std::size_t(leg_)]+=delta;
        activeSince_=now;
    }
    Phase phase_{Phase::Unmeasured};
    Leg leg_{Leg::BridgePreparation};
    Clock::time_point activeSince_{};
    Clock::duration elapsed_{};
    std::array<Clock::duration,std::size_t(Leg::Count)> legs_{};
};
}
