#pragma once
#include <algorithm>
#include <cmath>

namespace RiftwardenRules {
enum class SpawnKind { Grunt, Swift, Tank, Boss };
struct WaveComposition { int grunt{}, swift{}, tank{}, boss{}; };

inline SpawnKind SpawnKindFor(int wave, int remaining) {
    if (wave % 4 == 0 && remaining == 1) return SpawnKind::Boss;
    if (wave >= 3 && remaining % 7 == 0) return SpawnKind::Tank;
    if (wave >= 2 && remaining % 5 == 0) return SpawnKind::Swift;
    return SpawnKind::Grunt;
}

inline WaveComposition ComposeWave(int wave) {
    WaveComposition result;
    for (int remaining = 7 + wave * 2; remaining > 0; --remaining) {
        switch (SpawnKindFor(wave, remaining)) {
        case SpawnKind::Swift: ++result.swift; break;
        case SpawnKind::Tank: ++result.tank; break;
        case SpawnKind::Boss: ++result.boss; break;
        default: ++result.grunt; break;
        }
    }
    return result;
}

inline int ComboReward(int baseReward, int combo, float goldModifier) {
    const int clampedCombo = (std::max)(1, (std::min)(20, combo));
    const float comboBonus = 1.f + .05f * float((std::min)(10, clampedCombo - 1));
    return (std::max)(1, int(std::lround(baseReward * goldModifier * comboBonus)));
}
}
