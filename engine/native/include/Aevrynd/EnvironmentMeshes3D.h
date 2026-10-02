#pragma once
#include "Mesh3D.h"
#include <cstdint>
#include <span>

namespace Aevrynd {

enum class EnvironmentMeshDetail3D : uint8_t {Near,Far};
enum class GrassBladeStyle3D : uint8_t {Upright,Curved};

// 產生可由 GPU 大量實例化的低記憶體自然環境網格。
Mesh3D CreateGrassClumpMesh3D(unsigned bladeCount=9,unsigned segments=3,GrassBladeStyle3D style=GrassBladeStyle3D::Upright);
// 彎曲葉軸與成對小葉；近遠兩種幾何密度，固定種子可重現。
Mesh3D CreateFernMesh3D(EnvironmentMeshDetail3D detail=EnvironmentMeshDetail3D::Near,unsigned variationSeed=1);
Mesh3D CreateBroadleafCrownMesh3D(unsigned slices=10,unsigned stacks=6);
Mesh3D CreateTreeLodMesh3D(unsigned sides=6);
Mesh3D CreateBroadleafTreeMesh3D(EnvironmentMeshDetail3D detail=EnvironmentMeshDetail3D::Near,unsigned variationSeed=1);
Mesh3D CreateConiferTreeMesh3D(EnvironmentMeshDetail3D detail=EnvironmentMeshDetail3D::Near,unsigned variationSeed=1);
Mesh3D CreateSkyDomeMesh3D(unsigned slices=32,unsigned stacks=16);
Mesh3D CreateMountainMesh3D(unsigned radialSegments=24,unsigned rings=6,unsigned variationSeed=1);
Mesh3D CreateMountainRangeMesh3D(EnvironmentMeshDetail3D detail=EnvironmentMeshDetail3D::Near,unsigned variationSeed=1);
Mesh3D CreateRockMesh3D(EnvironmentMeshDetail3D detail=EnvironmentMeshDetail3D::Near,unsigned variationSeed=1);
Mesh3D CreateCurvedRibbonMesh3D(std::span<const Vec3> centers,std::span<const float> halfWidths,float heightOffset=0.f);

}
