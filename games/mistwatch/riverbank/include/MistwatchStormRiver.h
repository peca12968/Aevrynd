#pragma once
#include "Aevrynd/WaterShoreline3D.h"
namespace Mistwatch::StormRiver {
// Authored storm-stage water, not a change to the old vegetation/bed profile.
// It is deliberately not a support collider or a swimming simulation.
inline constexpr float Height=-1.4f;
inline constexpr float ShoreFade=.18f;
inline Aevrynd::Mesh3D Build(const Aevrynd::HeightField3D& finalTerrain){
    return Aevrynd::CreateShoreWeightedHeightFieldWaterMesh3D(finalTerrain,Height,ShoreFade);
}
}
