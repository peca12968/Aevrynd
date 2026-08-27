#include <Aevrynd/EnvironmentMeshes3D.h>
#include <array>
#include <iostream>

using namespace Aevrynd;
int main(){
    const Mesh3D grassHigh=CreateGrassClumpMesh3D(11,3),grassLow=CreateGrassClumpMesh3D(4,2),crownHigh=CreateBroadleafCrownMesh3D(10,6),crownLow=CreateBroadleafCrownMesh3D(6,4);
    if(!grassHigh.Valid()||!grassLow.Valid()||grassLow.indices.size()>=grassHigh.indices.size()){std::cerr<<"Grass LOD generation failed\n";return 1;}
    if(!crownHigh.Valid()||!crownLow.Valid()||crownLow.indices.size()>=crownHigh.indices.size()){std::cerr<<"Crown LOD generation failed\n";return 2;}
    const std::array<Vec3,3> centers={Vec3{-2,0,0},Vec3{0,.1f,2},Vec3{3,0,4}};const std::array<float,3> widths={1.f,1.5f,2.f};const Mesh3D ribbon=CreateCurvedRibbonMesh3D(centers,widths,.05f);
    if(!ribbon.Valid()||ribbon.vertices.size()!=6||ribbon.indices.size()!=12){std::cerr<<"Curved ribbon generation failed\n";return 3;}
    if(CreateCurvedRibbonMesh3D(std::span<const Vec3>{centers}.first(2),widths).Valid()){std::cerr<<"Mismatched ribbon inputs must fail\n";return 4;}
    std::cout<<"grassHighTriangles="<<grassHigh.indices.size()/3<<" grassLowTriangles="<<grassLow.indices.size()/3<<" crownHighTriangles="<<crownHigh.indices.size()/3<<" crownLowTriangles="<<crownLow.indices.size()/3<<'\n';
}
