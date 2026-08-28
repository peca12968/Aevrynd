#pragma once
#include "Mesh3D.h"
#include "SkinnedModelAsset.h"
#include <span>

namespace Aevrynd {

// 將同一骨架姿勢的所有子網格視為單一交易，避免容量不足時只畫出部分角色。
bool SubmitSkinnedRenderable3D(DrawList& draw,SkinnedMeshHandle3D mesh,
    SkinPaletteRange3D palette,Matrix4 transform,std::uint32_t totalIndexCount,
    std::span<const SkinnedSubmesh3D> submeshes,
    std::span<const Material3D> materialBindings,SkinPaletteRange3D previousPalette={},
    Matrix4 previousTransform=Matrix4::Identity(),bool motionValid=false);

}
