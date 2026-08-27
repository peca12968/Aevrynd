#pragma once
#include "Mesh3D.h"
#include <span>

namespace Aevrynd {

// 產生可由 GPU 大量實例化的低記憶體自然環境網格。
Mesh3D CreateGrassClumpMesh3D(unsigned bladeCount=9,unsigned segments=3);
Mesh3D CreateBroadleafCrownMesh3D(unsigned slices=10,unsigned stacks=6);
Mesh3D CreateTreeLodMesh3D(unsigned sides=6);
Mesh3D CreateCurvedRibbonMesh3D(std::span<const Vec3> centers,std::span<const float> halfWidths,float heightOffset=0.f);

}
