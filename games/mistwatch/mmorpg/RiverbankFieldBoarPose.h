#pragma once

#include "MistwatchBoar.h"

namespace Mistwatch {
inline bool PlaceRiverbankFieldBoar(Boar& boar, Aevrynd::Vec3 feet, const Aevrynd::HeightField3D& terrain,
                                   Aevrynd::Vec3 movement, float dt, EnemyPhase phase,
                                   float remaining, Aevrynd::Vec3 look) {
    if (!boar.Update(movement, dt, phase, remaining, look)) return false;
    if (boar.PlaceFeet(feet, terrain)) return true;

    // 斜坡換步時，舊的四個支撐點可能無法同時維持接地。改以當前位置重新求四蹄支撐。
    boar.initialized = false;
    return boar.Update({}, dt, phase, remaining, look) && boar.PlaceFeet(feet, terrain);
}
}
