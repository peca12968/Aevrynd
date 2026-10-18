#include <Aevrynd/Renderer.h>
#include <array>
#include <cmath>
#include <iostream>
#include <stdexcept>
namespace Reference {
#define float3 Aevrynd::Vec3
#include "../shaders/TerrainRockVariation.hlsli"
#undef float3
}
using namespace Aevrynd;
int main()try{
    unsigned checks=0;const auto need=[&](bool ok){++checks;if(!ok)throw std::runtime_error("terrain rock domain invariant");};
    TerrainDistanceMaterialSettings3D legacy,candidate;candidate.rockVariationEnabled=true;
    need(TerrainDistanceMaterialMode3D(legacy)==1&&TerrainDistanceMaterialMode3D(candidate)==2);
    need(PackTerrainDecorrelationSettings3D(legacy)==PackTerrainDecorrelationSettings3D(candidate));
    need(FingerprintTerrainDistanceMaterialSettings3D(legacy)!=FingerprintTerrainDistanceMaterialSettings3D(candidate));
    for(bool enabled:{false,true})for(bool decorrelation:{false,true}){
        auto a=legacy,b=candidate;a.enabled=b.enabled=enabled;a.decorrelationEnabled=b.decorrelationEnabled=decorrelation;
        if(!enabled||!decorrelation)need(TerrainDistanceMaterialMode3D(a)==TerrainDistanceMaterialMode3D(b)&&FingerprintTerrainDistanceMaterialSettings3D(a)==FingerprintTerrainDistanceMaterialSettings3D(b));
    }
    // 平坦法線在任意投影面都回到原世界方向；旋轉不改長度或內積。
    const std::array<Vec3,9> normals{{{1,0,0},{-1,0,0},{0,1,0},{0,-1,0},{0,0,1},{0,0,-1},{.6f,.8f,0},{0,.6f,.8f},{.36f,.48f,.8f}}};
    for(Vec3 a:normals){
        const auto r=Reference::TerrainRockDomainRotate(a),restored=Reference::TerrainRockDomainInverse(r);
        need(Length(restored-a)<.000002f&&std::abs(Length(r)-Length(a))<.000002f);
        for(Vec3 b:normals)need(std::abs(Dot(r,Reference::TerrainRockDomainRotate(b))-Dot(a,b))<.000002f);
    }
    // 平移抵銷且正的等向縮放保持所有方向、尺度；鄰接網格沒有分格跳接。
    for(Vec3 p:std::array<Vec3,4>{{{-49,12,56},{-44,1,60},{0,0,0},{31,17,-45}}})for(Vec3 d:normals){
        const auto actual=Reference::TerrainRockSecondaryPosition(p+d)-Reference::TerrainRockSecondaryPosition(p);
        need(Length(actual-Reference::TerrainRockDomainRotate(d)*1.173f)<.00003f);
    }
    std::cout<<"TERRAIN_ROCK_VARIATION_PASS checks="<<checks<<" gpu=0\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
