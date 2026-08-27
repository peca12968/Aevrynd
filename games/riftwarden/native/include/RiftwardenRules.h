#pragma once
#include <algorithm>
#include <cmath>
#include <string>
#include <string_view>

namespace RiftwardenRules {
enum class TowerKind { Bolt, Frost, Nova };
enum class EnemyKind { Grunt, Swift, Tank, Boss };
enum class UpgradeKind { Damage, Gold, Range };
struct WaveComposition { int grunt{}, swift{}, tank{}, boss{}; };
struct HitRule { float multiplier{1.f}; float slowDuration{}; bool shatters{}; };
struct UpgradeLevels { int damage{}, gold{}, range{}; };

inline std::wstring SaveFileName(std::wstring_view executableStem){return executableStem.find(L"-Demo-v")!=std::wstring_view::npos?std::wstring(executableStem)+L"-save.json":L"riftwarden-save.json";}

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

// 每局在第四波與第八波結束後提供一次永久強化選擇。
constexpr bool UpgradeDueAfterWave(int wave) {
    return wave == 4 || wave == 8;
}

// 單一強化最多兩級，確保兩次選擇不會讓倍率失控。
constexpr int UpgradeLevel(const UpgradeLevels& levels, UpgradeKind kind) {
    switch (kind) {
    case UpgradeKind::Damage: return levels.damage;
    case UpgradeKind::Gold: return levels.gold;
    default: return levels.range;
    }
}

constexpr UpgradeLevels ApplyUpgrade(UpgradeLevels levels, UpgradeKind kind) {
    int* level = kind == UpgradeKind::Damage ? &levels.damage : kind == UpgradeKind::Gold ? &levels.gold : &levels.range;
    *level = (std::min)(2, *level + 1);
    return levels;
}

constexpr float UpgradeMultiplier(const UpgradeLevels& levels, UpgradeKind kind) {
    const float step = kind == UpgradeKind::Damage ? .15f : kind == UpgradeKind::Gold ? .2f : .12f;
    return 1.f + float((std::max)(0, (std::min)(2, UpgradeLevel(levels, kind)))) * step;
}

// 強化選擇期間必須暫停波次推進，並讓永久與臨時倍率相乘。
constexpr bool CanBeginWave(bool waveActive, int wave, bool upgradePending) {
    return !waveActive && wave < 12 && !upgradePending;
}

constexpr float CombinedModifier(float temporaryModifier, const UpgradeLevels& levels, UpgradeKind kind) {
    return temporaryModifier * UpgradeMultiplier(levels, kind);
}

// 將卡片命中區與選擇循環留在純規則層，方便自動驗證所有輸入方式。
constexpr int UpgradeCardAt(float x, float y) {
    if (y < 250.f || y > 490.f) return -1;
    if (x >= 305.f && x <= 495.f) return 0;
    if (x >= 545.f && x <= 735.f) return 1;
    if (x >= 785.f && x <= 975.f) return 2;
    return -1;
}

constexpr int MoveUpgradeSelection(int current, int direction) {
    return (current + direction + 3) % 3;
}

// 鏡頭震動採線性衰減，結束時必須完全回到穩定畫面。
constexpr float ShakeAmplitude(float remaining, float duration, float strength) {
    if (remaining <= 0.f || duration <= 0.f || strength <= 0.f) return 0.f;
    const float ratio = (std::min)(1.f, remaining / duration);
    return strength * ratio;
}
}
