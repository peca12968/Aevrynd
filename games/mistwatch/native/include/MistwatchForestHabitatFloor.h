#pragma once
#include "MistwatchLandscape.h"
#include <bit>
#include <span>

namespace Mistwatch {
enum class ForestHabitatFloorProfile : std::uint8_t { Original, LeafCoverageStrengthV1 };
struct ForestHabitatRoots {
    std::span<const Aevrynd::Vec3> firs;
    std::span<const Aevrynd::Vec3> canopy;
};
struct ForestHabitatFloorReceipt {
    std::uint64_t fingerprint{14695981039346656037ull};
    std::size_t vertices{},changed{},soilDominant{},mossDominant{},bridgePreserved{};
    double soilSum{},rockSum{};
};
namespace ForestHabitatFloorDetail {
inline void ValidateProfile(ForestHabitatFloorProfile profile) {
    switch(profile){
    case ForestHabitatFloorProfile::Original:
    case ForestHabitatFloorProfile::LeafCoverageStrengthV1:return;
    }
    throw std::invalid_argument("Invalid forest habitat floor profile");
}
inline void Validate(ForestHabitatRoots roots) {
    if(roots.firs.size()>150||roots.canopy.size()>12)
        throw std::invalid_argument("Unbounded forest habitat roots");
    for(const auto cohort:{roots.firs,roots.canopy})for(const auto root:cohort)
        if(!Session::Finite(root)||std::abs(root.x)>50.f||root.z< -20.f||root.z>100.f)
            throw std::invalid_argument("Invalid forest habitat root");
}
inline float Noise(float x,float z,float size,std::uint32_t salt) {
    const float gx=x/size,gz=z/size;
    const int ix=int(std::floor(gx)),iz=int(std::floor(gz));
    const float tx=ForestFloorSmooth(gx-float(ix)),tz=ForestFloorSmooth(gz-float(iz));
    const auto sample=[&](int a,int b) {
        std::uint32_t word=std::uint32_t(a)*0x9e3779b9u^std::uint32_t(b)*0x85ebca6bu^salt;
        word^=word>>16;word*=0x7feb352du;word^=word>>15;word*=0x846ca68bu;word^=word>>16;
        return float(word>>8)/16777216.f;
    };
    const float a=sample(ix,iz),b=sample(ix+1,iz),c=sample(ix,iz+1),d=sample(ix+1,iz+1);
    return (a+(b-a)*tx)*(1.f-tz)+(c+(d-c)*tx)*tz;
}
inline float SpurWeight(float x,float z) {
    float distance=10000.f;
    const auto segment=[&](Aevrynd::Vec3 p) {
        const float closest=std::clamp(x,(std::min)(0.f,p.x),(std::max)(0.f,p.x));
        distance=(std::min)(distance,std::hypot(x-closest,z-p.z));
    };
    for(const auto p:Session::supplies)segment(p);
    for(const auto p:Session::repairSites)segment(p);
    for(const auto p:{Session::fuse,Session::generator,Session::rangerLog})segment(p);
    for(const auto p:Session::relaySites)segment(p);
    return ForestFloorSmooth(1.f-distance/1.15f);
}
inline ForestFloorCoverage Coverage(float x,float z,Aevrynd::Vec3 normal,float river,
    ForestHabitatRoots roots,ForestHabitatFloorProfile profile=ForestHabitatFloorProfile::Original) {
    ValidateProfile(profile);
    const auto base=ComposeBankFloorCoverage(x,z,normal,river,
        ForestFloorMaskSelector::ForestWood,BankCoverageSelector::Patchy);
    // 水下、橋域及核心邊界保留原土岩分層；地形、碰撞和頂點色都不變。
    if((z>32.f&&z<40.f)||std::abs(x)>=50.f||z<=-20.f||z>=100.f)return base;
    const float edge=ForestFloorSmooth((std::min)({50.f-std::abs(x),z+20.f,100.f-z})/5.f);
    const float signal=.62f*Noise(x,z,5.5f,0x464c4f52u)+.38f*Noise(x,z,13.f,0x48554d55u);
    // 候選只增加原落葉土層的覆蓋強度；根部、路徑與地理遮罩皆沿用原配方。
    const float strength=profile==ForestHabitatFloorProfile::LeafCoverageStrengthV1?.80f:.56f;
    float habitat=.10f+strength*ForestFloorSmooth((signal-.24f)/.53f);
    const auto rootSoil=[&](std::span<const Aevrynd::Vec3> cohort,float radius) {
        for(const auto root:cohort){const float distance=std::hypot(x-root.x,z-root.z);
            habitat=(std::max)(habitat,.85f*ForestFloorSmooth(1.f-(distance-.30f)/(radius-.30f)));}
    };
    rootSoil(roots.firs,1.8f);rootSoil(roots.canopy,4.2f);
    habitat=(std::max)(habitat,.78f*SpurWeight(x,z));
    const float bank=ForestWoodBankSedimentWeight(x,z,normal,river);
    const float soil=(std::max)(base.soil,habitat*edge*(1.f-bank)*(1.f-base.rock));
    const ForestFloorCoverage result{soil,base.rock};
    if(!std::isfinite(soil)||soil<0.f||soil>1.f||soil+base.rock>1.000001f)
        throw std::runtime_error("Habitat soil escaped normalized layers");
    return result;
}
inline void Mix(std::uint64_t& hash,std::uint32_t word) {
    for(unsigned byte=0;byte<4;++byte){hash^=(word>>(byte*8))&255u;hash*=1099511628211ull;}
}
}
inline ForestFloorCoverage ForestHabitatFloorCoverage(float x,float z,Aevrynd::Vec3 normal,
    float river,ForestHabitatRoots roots,ForestHabitatFloorProfile profile=ForestHabitatFloorProfile::Original) {
    ForestHabitatFloorDetail::ValidateProfile(profile);
    ForestHabitatFloorDetail::Validate(roots);
    if(!std::isfinite(x)||!std::isfinite(z)||std::abs(x)>1000.f||std::abs(z)>1000.f)
        throw std::invalid_argument("Invalid forest habitat coverage domain");
    return ForestHabitatFloorDetail::Coverage(x,z,normal,river,roots,profile);
}
// 僅編碼已存在三層 Terrain 的土／岩權重，不新增覆蓋平面或畫面貼片。
inline ForestHabitatFloorReceipt ApplyForestHabitatFloor(Aevrynd::Mesh3D& mesh,
    const Aevrynd::HeightField3D& terrain,ForestHabitatRoots roots,bool landscape=false,
    ForestHabitatFloorProfile profile=ForestHabitatFloorProfile::Original) {
    // 在任何頂點寫入之前拒絕未知配方，避免留下部分繪製的地表。
    ForestHabitatFloorDetail::ValidateProfile(profile);
    ForestHabitatFloorDetail::Validate(roots);
    if(!mesh.Valid())throw std::invalid_argument("Invalid forest habitat terrain mesh");
    for(const auto& vertex:mesh.vertices)
        if(!Session::Finite({vertex.x,vertex.y,vertex.z})||std::abs(vertex.x)>1000.f||std::abs(vertex.z)>1000.f)
            throw std::invalid_argument("Invalid forest habitat mesh domain");
    ForestHabitatFloorReceipt receipt;
    for(auto& vertex:mesh.vertices){
        const auto normal=landscape?LandscapeNormal(terrain,vertex.x,vertex.z):terrain.SampleNormal(vertex.x,vertex.z);
        const auto coverage=ForestHabitatFloorDetail::Coverage(vertex.x,vertex.z,normal,
            RiverSedimentWeight(vertex.x,vertex.z),roots,profile);
        Aevrynd::Vec2 encoded{};
        if(!Aevrynd::TryEncodeTerrainLayerCoverage3D(coverage.soil,coverage.rock,encoded))
            throw std::runtime_error("Invalid forest habitat mesh coverage");
        ++receipt.vertices;
        receipt.changed+=std::bit_cast<std::uint32_t>(vertex.u)!=std::bit_cast<std::uint32_t>(encoded.x)||
            std::bit_cast<std::uint32_t>(vertex.v)!=std::bit_cast<std::uint32_t>(encoded.y);
        receipt.bridgePreserved+=vertex.z>32.f&&vertex.z<40.f;
        receipt.soilDominant+=coverage.soil>.5f;receipt.mossDominant+=coverage.soil+coverage.rock<.5f;
        receipt.soilSum+=coverage.soil;receipt.rockSum+=coverage.rock;
        vertex.u=encoded.x;vertex.v=encoded.y;
        for(float value:{vertex.x,vertex.y,vertex.z,vertex.u,vertex.v})
            ForestHabitatFloorDetail::Mix(receipt.fingerprint,std::bit_cast<std::uint32_t>(value));
        ForestHabitatFloorDetail::Mix(receipt.fingerprint,vertex.color);
        ForestHabitatFloorDetail::Mix(receipt.fingerprint,vertex.normal);
    }
    for(auto index:mesh.indices)ForestHabitatFloorDetail::Mix(receipt.fingerprint,index);
    return receipt;
}
}
