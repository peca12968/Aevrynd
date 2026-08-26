#pragma once
#include <algorithm>
#include <cmath>

namespace RiftwardenRules {
enum class TowerKind { Bolt, Frost, Nova };
enum class EnemyKind { Grunt, Swift, Tank, Boss };
struct WaveComposition { int grunt{}, swift{}, tank{}, boss{}; };
struct HitRule { float multiplier{1.f}; float slowDuration{}; bool shatters{}; };

inline EnemyKind SpawnKindFor(int wave, int remaining) {
    if (wave % 4 == 0 && remaining == 1) return EnemyKind::Boss;
    if (wave >= 3 && remaining % 7 == 0) return EnemyKind::Tank;
    if (wave >= 2 && remaining % 5 == 0) return EnemyKind::Swift;
    return EnemyKind::Grunt;
}

inline WaveComposition ComposeWave(int wave) {
    WaveComposition result;
    for (int remaining = 7 + wave * 2; remaining > 0; --remaining) {
        switch (SpawnKindFor(wave, remaining)) {
        case EnemyKind::Swift: ++result.swift; break;
        case EnemyKind::Tank: ++result.tank; break;
        case EnemyKind::Boss: ++result.boss; break;
        default: ++result.grunt; break;
        }
    }
    return result;
}

inline HitRule ResolveHit(TowerKind tower, EnemyKind enemy, bool chilled) {
    HitRule result;
    if (tower == TowerKind::Frost) {
        result.slowDuration = enemy == EnemyKind::Boss ? 1.2f : 2.2f;
        if (enemy == EnemyKind::Swift) result.multiplier *= 1.4f;
    } else {
        if (tower == TowerKind::Nova && enemy == EnemyKind::Tank) result.multiplier *= 1.35f;
        if (tower == TowerKind::Bolt && enemy == EnemyKind::Boss) result.multiplier *= 1.25f;
        if (chilled) {
            result.multiplier *= 1.3f;
            result.shatters = true;
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
