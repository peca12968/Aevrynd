#pragma once

namespace Mistwatch {
struct RiverbankSettingsMenu {
    // 開啟與關閉設定的該一步也會攔截操作，避免同一批按鍵送入世界或其他面板。
    struct State { bool consumesInput{}, editable{}; };

    State Update(bool focused, bool toggle, bool escape, bool socialPanelOpen) noexcept {
        const bool previouslyOpen = open_;
        if (focused && !socialPanelOpen) {
            if (open_ && escape) open_ = false;
            else if (toggle) open_ = !open_;
        }
        return {previouslyOpen || open_, focused && previouslyOpen && open_ && !socialPanelOpen};
    }

    bool Open() const noexcept { return open_; }

private:
    bool open_{};
};
}
