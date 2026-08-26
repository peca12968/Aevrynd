#pragma once
#include <algorithm>
#include <cmath>

namespace RiftwardenRules {
inline int ComboReward(int baseReward, int combo, float goldModifier) {
    const int clampedCombo = (std::max)(1, (std::min)(20, combo));
    const float comboBonus = 1.f + .05f * float((std::min)(10, clampedCombo - 1));
    return (std::max)(1, int(std::lround(baseReward * goldModifier * comboBonus)));
}
}
