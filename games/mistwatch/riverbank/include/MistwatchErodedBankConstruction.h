#pragma once
#include "MistwatchRiverbankConstruction.h"
#include "MistwatchRiverProfile.h"
#include "MistwatchStormRiver.h"
#include <Aevrynd/HeightField3D.h>
#include <algorithm>
#include <bit>
#include <cmath>
#include <span>
#include <stdexcept>

namespace Mistwatch::ErodedBank {
inline constexpr float FootHeight=Mistwatch::StormRiver::Height;
struct Result {
    Aevrynd::HeightField3D field;
    unsigned changed{},protectedSamples{};
    float maximumLowering{};
};
inline float Domain(float x,float z) {
    using RiverbankConstruction::Smooth;
    return Smooth((std::abs(x)-3.5f)/2.5f)*Smooth((20.f-std::abs(x))/4.f)
        *Smooth((z-26.f)/2.f)*Smooth((47.f-z)/2.f);
}
Result Build(const Aevrynd::HeightField3D& original,const Aevrynd::HeightField3D& coherent,
                    std::span<const RiverbankConstruction::Guard> guards);
}
