#pragma once
#include <cmath>
#include <cstdint>
#include <string_view>

namespace Mistwatch {
// 純 UI 操作結果，不是存檔通行證，也不推測目前世界是否可還原。
enum class ActionFeedbackEvent : std::uint8_t {
    None, Saved, SaveFailed, SavePositionUnavailable, Loaded, LoadUnavailable, RetryFromBridge, RetryFromStart
};
enum class ActionFeedbackClock : std::uint8_t { Active, Suspended, LoadTransaction };

class ActionFeedbackToast {
public:
    static constexpr double DurationSeconds=4.0;

    // 只由真正操作的結果分支呼叫；普通更新／繪圖不得重新發布。
    bool Publish(ActionFeedbackEvent event) noexcept {
        switch(event) {
        case ActionFeedbackEvent::Saved:
        case ActionFeedbackEvent::SaveFailed:
        case ActionFeedbackEvent::SavePositionUnavailable:
        case ActionFeedbackEvent::Loaded:
        case ActionFeedbackEvent::LoadUnavailable:
        case ActionFeedbackEvent::RetryFromBridge:
        case ActionFeedbackEvent::RetryFromStart:break;
        default:return false;
        }
        event_=event;remaining_=DurationSeconds;return true;
    }
    // 暫停／失焦凍結 UI 秒數；F9 原子步不倒數，也不推進遊戲狀態。
    bool Advance(double elapsedSeconds,ActionFeedbackClock clock) noexcept {
        if(!std::isfinite(elapsedSeconds)||elapsedSeconds<0)return false;
        switch(clock) {
        case ActionFeedbackClock::Suspended:
        case ActionFeedbackClock::LoadTransaction:return true;
        case ActionFeedbackClock::Active:break;
        default:return false;
        }
        if(!Visible())return true;
        if(elapsedSeconds>=remaining_)Clear();
        else remaining_-=elapsedSeconds;
        return true;
    }
    void Clear() noexcept {event_=ActionFeedbackEvent::None;remaining_=0;}
    bool Visible() const noexcept {return event_!=ActionFeedbackEvent::None&&remaining_>0;}
    ActionFeedbackEvent Event() const noexcept {return event_;}
    double RemainingSeconds() const noexcept {return remaining_;}
    std::string_view Text() const noexcept {
        if(!Visible())return {};
        switch(event_) {
        case ActionFeedbackEvent::Saved:return "Last action: Saved";
        case ActionFeedbackEvent::SaveFailed:return "Last action: Save failed";
        case ActionFeedbackEvent::SavePositionUnavailable:return "Last action: Save position unavailable";
        case ActionFeedbackEvent::Loaded:return "Last action: Loaded";
        case ActionFeedbackEvent::LoadUnavailable:return "Last action: Load unavailable";
        case ActionFeedbackEvent::RetryFromBridge:return "Last action: Retry from bridgehead";
        case ActionFeedbackEvent::RetryFromStart:return "Last action: Retry from starting point";
        default:return {};
        }
    }
private:
    ActionFeedbackEvent event_{ActionFeedbackEvent::None};
    double remaining_{};
};
}
