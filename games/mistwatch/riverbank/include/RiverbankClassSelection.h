#pragma once

#include "RiverbankEncounter.h"
#include <Aevrynd/Input.h>
#include <Aevrynd/Renderer.h>
#include <array>
#include <cmath>
#include <string>
#include <string_view>

namespace Mistwatch::RiverbankClassSelection {
struct Option {
    RiverbankClass playerClass;
    unsigned key;
    std::string_view role, skill, description;
    Aevrynd::Color accent;
};

inline constexpr std::array options{
    Option{RiverbankClass::Guardian, '1', "近身防護", "挑釁護衛", "吸引首領注意並減輕受傷", {.95f,.73f,.45f,1}},
    Option{RiverbankClass::Ranger, '2', "遠程輸出", "連射", "從遠處攻擊，保留走位空間", {.60f,.87f,.68f,1}},
    Option{RiverbankClass::Priest, '3', "治療支援", "治療／救援", "治療隊友或救起倒地隊友", {.69f,.80f,1,1}},
};

inline constexpr float CardWidth = 282.f, CardHeight = 206.f;

constexpr Aevrynd::Vec2 CardCenter(std::size_t index) {
    return {640.f + (float(index) - 1.f) * 306.f, 374.f};
}

inline RiverbankClass HoveredClass(const Aevrynd::Input& input) {
    if (!input.Focused() || input.FocusLost() || !input.MouseInsideCanvas()) return RiverbankClass::None;
    const auto mouse = input.Mouse();
    for (std::size_t i = 0; i < options.size(); ++i) {
        const auto center = CardCenter(i);
        if (std::abs(mouse.x - center.x) < CardWidth * .5f &&
            std::abs(mouse.y - center.y) < CardHeight * .5f) return options[i].playerClass;
    }
    return RiverbankClass::None;
}

inline RiverbankClass ReadSelection(const Aevrynd::Input& input) {
    if (!input.Focused() || input.FocusLost()) return RiverbankClass::None;
    for (const auto& option : options)
        if (input.Pressed(option.key)) return option.playerClass;
    return input.MousePressed() ? HoveredClass(input) : RiverbankClass::None;
}

inline void Draw(Aevrynd::DrawList& draw, RiverbankClass hovered) {
    using Aevrynd::Color;
    constexpr Color text{.93f,.96f,.94f,1};
    constexpr Color secondary{.76f,.84f,.81f,1};
    draw.Quad({640,334},{1008,358},{.027f,.046f,.044f,.98f});
    draw.SystemText({178,184},"選擇職業",32.f,text);
    draw.SystemText({180,233},"點選卡片或按 1–3",18.f,secondary);
    for (std::size_t i = 0; i < options.size(); ++i) {
        const auto& option = options[i];
        const auto center = CardCenter(i);
        const auto stats = CombatStatsFor(option.playerClass);
        const bool highlight = hovered == option.playerClass;
        draw.Quad(center,{CardWidth,CardHeight},
                  highlight ? Color{.12f,.21f,.18f,1} : Color{.061f,.098f,.089f,1});
        draw.Quad({center.x,center.y-CardHeight*.5f+1.5f},{CardWidth,3.f},option.accent);
        const float x = center.x - CardWidth*.5f + 20.f;
        const auto title = std::string(1, char(option.key)) + "　" + std::string(RiverbankClassName(option.playerClass));
        draw.SystemText({x,290},title,25.f,text);
        draw.SystemText({x,327},option.role,17.f,option.accent);
        draw.SystemText({x,360},"Q " + std::string(option.skill),19.f,text);
        draw.SystemText({x,389},option.description,16.f,secondary);
        draw.SystemText({x,438},"生命 " + std::to_string(stats.health) + "　技能消耗 " +
                        std::to_string(stats.skillCost) + " 資源",15.f,secondary);
    }
}
}
