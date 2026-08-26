#include "RiftwardenRules.h"
#include <cmath>
#include <iostream>

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
