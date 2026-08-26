#include "RiftwardenRules.h"
#include <cmath>
#include <iostream>

// 這些斷言由編譯器執行，即使企業政策阻擋新測試程式也能驗證核心規則。
constexpr auto FirstUpgrade = RiftwardenRules::ApplyUpgrade({}, RiftwardenRules::UpgradeKind::Damage);
constexpr auto SecondUpgrade = RiftwardenRules::ApplyUpgrade(FirstUpgrade, RiftwardenRules::UpgradeKind::Damage);
constexpr auto ClampedUpgrade = RiftwardenRules::ApplyUpgrade(SecondUpgrade, RiftwardenRules::UpgradeKind::Damage);
constexpr auto GoldUpgrade = RiftwardenRules::ApplyUpgrade({}, RiftwardenRules::UpgradeKind::Gold);
constexpr auto RangeUpgrade = RiftwardenRules::ApplyUpgrade({}, RiftwardenRules::UpgradeKind::Range);
static_assert(RiftwardenRules::UpgradeDueAfterWave(4));
static_assert(RiftwardenRules::UpgradeDueAfterWave(8));
static_assert(!RiftwardenRules::UpgradeDueAfterWave(6));
static_assert(ClampedUpgrade.damage == 2);
static_assert(RiftwardenRules::UpgradeMultiplier(SecondUpgrade, RiftwardenRules::UpgradeKind::Damage) > 1.299f);
static_assert(RiftwardenRules::UpgradeMultiplier(SecondUpgrade, RiftwardenRules::UpgradeKind::Damage) < 1.301f);
static_assert(RiftwardenRules::UpgradeMultiplier(GoldUpgrade, RiftwardenRules::UpgradeKind::Gold) > 1.199f);
static_assert(RiftwardenRules::UpgradeMultiplier(GoldUpgrade, RiftwardenRules::UpgradeKind::Gold) < 1.201f);
static_assert(RiftwardenRules::UpgradeMultiplier(RangeUpgrade, RiftwardenRules::UpgradeKind::Range) > 1.119f);
static_assert(RiftwardenRules::UpgradeMultiplier(RangeUpgrade, RiftwardenRules::UpgradeKind::Range) < 1.121f);
static_assert(RiftwardenRules::CanBeginWave(false, 4, false));
static_assert(!RiftwardenRules::CanBeginWave(false, 4, true));
static_assert(!RiftwardenRules::CanBeginWave(true, 4, false));
static_assert(!RiftwardenRules::CanBeginWave(false, 12, false));
static_assert(RiftwardenRules::CombinedModifier(1.2f, SecondUpgrade, RiftwardenRules::UpgradeKind::Damage) > 1.559f);
static_assert(RiftwardenRules::CombinedModifier(1.2f, SecondUpgrade, RiftwardenRules::UpgradeKind::Damage) < 1.561f);
static_assert(RiftwardenRules::UpgradeCardAt(400.f, 370.f) == 0);
static_assert(RiftwardenRules::UpgradeCardAt(640.f, 370.f) == 1);
static_assert(RiftwardenRules::UpgradeCardAt(880.f, 370.f) == 2);
static_assert(RiftwardenRules::UpgradeCardAt(520.f, 370.f) == -1);
static_assert(RiftwardenRules::UpgradeCardAt(640.f, 500.f) == -1);
static_assert(RiftwardenRules::MoveUpgradeSelection(0, -1) == 2);
static_assert(RiftwardenRules::MoveUpgradeSelection(2, 1) == 0);
static_assert(RiftwardenRules::ShakeAmplitude(.2f, .4f, 8.f) == 4.f);
static_assert(RiftwardenRules::ShakeAmplitude(.6f, .4f, 8.f) == 8.f);
static_assert(RiftwardenRules::ShakeAmplitude(0.f, .4f, 8.f) == 0.f);
static_assert(RiftwardenRules::ShakeAmplitude(.2f, 0.f, 8.f) == 0.f);

int main() {
    if (RiftwardenRules::ComboReward(17, 1, 1.f) != 17) {
        std::cerr << "The first kill must retain its base reward\n";
        return 1;
    }
    if (RiftwardenRules::ComboReward(17, 5, 1.f) != 20) {
        std::cerr << "A five-hit combo must grant a 20 percent bonus\n";
        return 2;
    }
    if (RiftwardenRules::ComboReward(17, 20, 1.25f) != 32) {
        std::cerr << "Combo and event gold modifiers must stack predictably\n";
        return 3;
    }
    const auto first = RiftwardenRules::ComposeWave(1);
    if (first.grunt != 9 || first.swift != 0 || first.tank != 0 || first.boss != 0) {
        std::cerr << "The first wave composition is incorrect\n";
        return 4;
    }
    const auto fourth = RiftwardenRules::ComposeWave(4);
    if (fourth.grunt != 9 || fourth.swift != 3 || fourth.tank != 2 || fourth.boss != 1) {
        std::cerr << "The fourth wave preview must match its spawn rules\n";
        return 5;
    }
    const auto frostSwift = RiftwardenRules::ResolveHit(RiftwardenRules::TowerKind::Frost, RiftwardenRules::EnemyKind::Swift, false);
    if (std::abs(frostSwift.multiplier - 1.4f) > .001f || std::abs(frostSwift.slowDuration - 2.2f) > .001f) {
        std::cerr << "Frost must counter and chill swift enemies\n";
        return 6;
    }
    const auto novaShatter = RiftwardenRules::ResolveHit(RiftwardenRules::TowerKind::Nova, RiftwardenRules::EnemyKind::Tank, true);
    if (std::abs(novaShatter.multiplier - 1.755f) > .001f || !novaShatter.shatters) {
        std::cerr << "Nova must counter tanks and shatter chilled targets\n";
        return 7;
    }
    const auto bossSlow = RiftwardenRules::ResolveHit(RiftwardenRules::TowerKind::Frost, RiftwardenRules::EnemyKind::Boss, false);
    if (std::abs(bossSlow.slowDuration - 1.2f) > .001f) {
        std::cerr << "Boss chill resistance is incorrect\n";
        return 8;
    }
    return 0;
}
