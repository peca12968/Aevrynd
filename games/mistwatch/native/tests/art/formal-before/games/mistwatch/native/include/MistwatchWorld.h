#pragma once
#include <Aevrynd/CollisionWorld3D.h>
#include <Aevrynd/HeightField3D.h>
#include <Aevrynd/TerrainLayerCoverage3D.h>
#include "MistwatchSession.h"
#include "MistwatchForestProps.h"
#include "MistwatchUnderstory.h"
#include "MistwatchBroadleaf.h"
#include "MistwatchRiverProfile.h"
#include "MistwatchRelay.h"
#include "MistwatchFirLayout.h"
#include "MistwatchForestFloorMask.h"
#include "MistwatchBankCoverage.h"
#include <array>
#include <stdexcept>
#include <vector>
namespace Mistwatch {
inline Aevrynd::HeightField3D CreateTerrain() {
    Aevrynd::HeightField3D terrain({{-50,0,-20},201,241,.5f});
    for(unsigned z=0;z<241;++z) for(unsigned x=0;x<201;++x) {
        const auto p=terrain.SamplePosition(x,z);
        terrain.SetHeight(x,z,GroundElevation(p.x,p.z));
    }
    terrain.RecalculateRange();
    return terrain;
}
inline Aevrynd::HeightFieldMeshSettings3D GroundMeshSettings() {
    Aevrynd::HeightFieldMeshSettings3D settings{{.65f,.75f,.55f,1},{.75f,.72f,.62f,1},{.45f,.43f,.39f,1}};
    settings.shoreHeight=RiverHeight;settings.shoreBlend=3.f;settings.shoreColor={.36f,.32f,.25f,1};
    return settings;
}
inline float SettlementTrailDistance(float x,float z) {
    float distance=10000.f;
    for(const auto p:{Session::fuse,Session::generator,Session::rangerLog}) {
        const float closestX=std::clamp(x,(std::min)(0.f,p.x),(std::max)(0.f,p.x));
        distance=(std::min)(distance,std::hypot(x-closestX,z-p.z));
    }
    for(const auto p:Session::relaySites) {
        const float closestX=std::clamp(x,(std::min)(0.f,p.x),(std::max)(0.f,p.x));
        distance=(std::min)(distance,std::hypot(x-closestX,z-p.z));
    }
    return distance;
}
inline std::uint32_t GroundLayerHeightColor(std::uint32_t color,float height,const Aevrynd::HeightField3D& core) {
    // Terrain 的 alpha 是海拔資料，不是透明度；兩個網格必須共用核心範圍與量化方式。
    const float range=core.MaximumHeight()-core.MinimumHeight();
    if(!std::isfinite(height)||!std::isfinite(range)||range<=0)
        throw std::runtime_error("Invalid ground layer height range");
    const float normalized=std::clamp((height-core.MinimumHeight())/range,0.f,1.f);
    return (color&0x00ffffffu)|(Aevrynd::PackColorRgba8({0,0,0,normalized})&0xff000000u);
}
inline Aevrynd::Mesh3D CreateGroundMesh(const Aevrynd::HeightField3D& terrain,bool layerHeight=true) {
    auto mesh=terrain.CreateMesh(GroundMeshSettings());
    if(layerHeight)for(auto& vertex:mesh.vertices)
        vertex.color=GroundLayerHeightColor(vertex.color,vertex.y,terrain);
    return mesh;
}
inline float RiverSedimentWeight(float x,float z) {
    const float mask=RiverSmooth((std::abs(x)-16.f)/8.f),distance=RiverDistance(x,z),width=RiverOuterWidth(x);
    // 水位 -4 穿過 -8～0 河床的半寬位置；可見下岸在半寬與全寬之間，不能誤當水下區域。
    if(mask<=0.f||distance<=width*.5f)return 0.f;
    const float outer=RiverSmooth((std::abs(x)-12.f)/12.f);
    const float bank=z<RiverCenterZ(x)?13.f+2.f*std::sin(x*.07f):13.f+2.f*std::cos(x*.08f);
    const float t=(distance-width)/((4.f+(bank-4.f)*outer)*(1.f+.5f*RiverErosionMask(x,z)));
    const float lower=RiverSmooth((distance/width-.5f)/.08f),upper=1.f-RiverSmooth((t-.45f)/.55f);
    // 中央橋樑域完全保留；外側真露出的河床岸坡與上岸採世界固定斑塊，不動水下／地形。
    const float signal=.5f+.5f*std::sin(x*.61f+z*.49f)*std::cos(x*.31f-z*.27f);
    const float breakup=RiverSmooth((signal-.30f)/.45f);
    return .95f*mask*lower*upper*(.65f+.35f*breakup);
}
inline Aevrynd::Mesh3D CreateTerrainMesh(const Aevrynd::HeightField3D& terrain,bool layerHeight=true) {
    auto mesh=CreateGroundMesh(terrain,layerHeight);
    // 通路直接混入地表頂點色，保留材質細節與原始碰撞高度，避免懸浮平板。
    for(auto& vertex:mesh.vertices) {
        if(vertex.z< -10.f || vertex.z>96.f || (vertex.z>32.f && vertex.z<40.f)) continue;
        const float center=.35f*std::sin(vertex.z*.19f);
        const float width=2.8f+.3f*std::sin(vertex.z*.37f);
        float weight=std::clamp(1.f-std::abs(vertex.x-center)/width,0.f,1.f);
        weight=(std::max)(weight,std::clamp(1.f-SettlementTrailDistance(vertex.x,vertex.z)/1.15f,0.f,1.f));
        weight=weight*weight*(3.f-2.f*weight);
        const auto original=Aevrynd::UnpackColorRgba8(vertex.color);
        const Aevrynd::Color soil{.46f,.34f,.22f,1};
        vertex.color=Aevrynd::PackColorRgba8({original.r+(soil.r-original.r)*weight,
            original.g+(soil.g-original.g)*weight,original.b+(soil.b-original.b)*weight,original.a});
    }
    if(!layerHeight)return mesh;
    for(auto& vertex:mesh.vertices){
        const float weight=RiverSedimentWeight(vertex.x,vertex.z);
        if(weight<=0.f)continue;
        // 第二槽是 Ground108 淺灰碎石，第三槽是褐色粗粒岩面；不改頂點色或曝光。
        Aevrynd::Vec2 encoded{};
        if(!Aevrynd::TryEncodeTerrainLayerCoverage3D(.15f*weight,.85f*weight,encoded))throw std::runtime_error("Invalid bank sediment coverage");
        vertex.u=encoded.x;vertex.v=encoded.y;
    }
    return mesh;
}
inline Aevrynd::Mesh3D CreateTerrainMesh(const Aevrynd::HeightField3D& terrain,bool layerHeight,
    ForestFloorMaskSelector selector) {
    if(selector==ForestFloorMaskSelector::Legacy)return CreateTerrainMesh(terrain,layerHeight);
    if(!ValidForestFloorMaskSelector(selector))throw std::invalid_argument("Invalid forest-floor selector");
    auto mesh=CreateGroundMesh(terrain,layerHeight);
    for(auto& vertex:mesh.vertices) {
        if(vertex.z< -10.f||vertex.z>96.f||(vertex.z>32.f&&vertex.z<40.f))continue;
        float weight=CentralTrailVisualWeight(vertex.x,vertex.z,selector);
        weight=(std::max)(weight,std::clamp(1.f-SettlementTrailDistance(vertex.x,vertex.z)/1.15f,0.f,1.f));
        const auto original=Aevrynd::UnpackColorRgba8(vertex.color);
        const Aevrynd::Color soil{.46f,.34f,.22f,1};
        vertex.color=Aevrynd::PackColorRgba8({original.r+(soil.r-original.r)*weight,
            original.g+(soil.g-original.g)*weight,original.b+(soil.b-original.b)*weight,original.a});
    }
    if(!layerHeight)return mesh;
    for(auto& vertex:mesh.vertices) {
        const auto coverage=ComposeForestFloorCoverage(vertex.x,vertex.z,terrain.SampleNormal(vertex.x,vertex.z),
            RiverSedimentWeight(vertex.x,vertex.z),selector);
        Aevrynd::Vec2 encoded{};
        if(!Aevrynd::TryEncodeTerrainLayerCoverage3D(coverage.soil,coverage.rock,encoded))
            throw std::runtime_error("Invalid forest-floor coverage");
        vertex.u=encoded.x;vertex.v=encoded.y;
    }
    return mesh;
}
inline Aevrynd::Mesh3D CreateTerrainMesh(const Aevrynd::HeightField3D& terrain,bool layerHeight,
    ForestFloorMaskSelector floor,BankCoverageSelector bank) {
    ValidateBankCoverageRoute(floor,bank);
    auto mesh=CreateTerrainMesh(terrain,layerHeight,floor);
    if(!layerHeight||bank==BankCoverageSelector::Legacy)return mesh;
    for(auto& vertex:mesh.vertices) {
        const auto coverage=ComposeBankFloorCoverage(vertex.x,vertex.z,terrain.SampleNormal(vertex.x,vertex.z),
            RiverSedimentWeight(vertex.x,vertex.z),floor,bank);
        Aevrynd::Vec2 encoded{};
        if(!Aevrynd::TryEncodeTerrainLayerCoverage3D(coverage.soil,coverage.rock,encoded))
            throw std::runtime_error("Invalid patchy bank coverage");
        vertex.u=encoded.x;vertex.v=encoded.y;
    }
    return mesh;
}
inline std::vector<Aevrynd::Vec3> TreePositions() {
    const auto terrain=CreateTerrain();
    std::vector<Aevrynd::Vec3> result{{-7,0,-5}};
    std::uint32_t seed=0x4d495354u;
    const auto random=[&](){seed=seed*1664525u+1013904223u;return float(seed>>8)/16777216.f;};
    for(int i=0;i<360 && result.size()<100;++i) {
        const float x=-38.f+76.f*random(),z=-12.f+100.f*random();
        if(std::abs(x)<4.f || RiverDistance(x,z)<RiverOuterWidth(x)+2.f) continue;
        bool occupied=false;
        for(const auto p:Session::supplies) occupied|=std::hypot(x-p.x,z-p.z)<3.f;
        for(const auto p:Session::repairSites) {
            occupied|=std::hypot(x-p.x,z-p.z)<4.f;
            occupied|=std::abs(z-p.z)<1.5f && x>(std::min)(0.f,p.x)-1.f && x<(std::max)(0.f,p.x)+1.f;
        }
        for(const auto p:{Session::fuse,Session::generator,Session::rangerLog}) occupied|=std::hypot(x-p.x,z-p.z)<5.f;
        for(const auto p:Session::relaySites) {
            occupied|=std::hypot(x-p.x,z-p.z)<4.f;
            occupied|=std::abs(z-p.z)<1.8f&&x>(std::min)(0.f,p.x)-1.f&&x<(std::max)(0.f,p.x)+1.f;
        }
        for(const auto p:result) occupied|=std::hypot(x-p.x,z-p.z)<3.5f;
        if(occupied) continue;
        result.push_back({x,terrain.SampleHeight(x,z),z});
    }
    return result;
}
inline MatureFirLayout CreateMatureFirLayout(const Aevrynd::HeightField3D& terrain,FirLayoutSelector selector) {
    return SelectMatureFirLayout(terrain,TreePositions(),selector);
}
inline std::vector<Aevrynd::Vec3> GroundCoverPositions(const Aevrynd::HeightField3D& terrain) {
    std::vector<Aevrynd::Vec3> result;
    std::uint32_t seed=0x47524153u;
    const auto random=[&](){seed=seed*1664525u+1013904223u;return float(seed>>8)/16777216.f;};
    for(unsigned i=0;i<1800;++i) {
        const float x=-32.f+64.f*random(),z=-10.f+98.f*random();
        if(std::abs(x)<1.8f || SettlementTrailDistance(x,z)<.7f || RiverDistance(x,z)<RiverOuterWidth(x)+1.5f) continue;
        bool shelter=false;
        for(const auto p:Session::repairSites)shelter|=std::hypot(x-p.x,z-p.z)<3.f;
        for(const auto p:{Session::fuse,Session::rangerLog})shelter|=std::abs(x-p.x)<3.f&&std::abs(z-p.z)<3.5f;
        if(shelter)continue;
        result.push_back({x,terrain.SampleHeight(x,z),z});
    }
    return result;
}
inline std::vector<Aevrynd::BoxCollider3D> WorkshopSolids() {
    const auto p=Session::fuse;
    return {{p+Aevrynd::Vec3{0,2.7f,0},{2.5f,.15f,2.f}},
            {p+Aevrynd::Vec3{-2.4f,1.3f,0},{.1f,1.3f,2.f}},
            {p+Aevrynd::Vec3{0,1.3f,-1.9f},{2.5f,1.3f,.1f}},
            {p+Aevrynd::Vec3{0,1.3f,1.9f},{2.5f,1.3f,.1f}}};
}
inline Aevrynd::BoxCollider3D GeneratorSolid() {
    return {Session::generator+Aevrynd::Vec3{0,.6f,0},{.85f,.6f,.6f}};
}
inline Aevrynd::BoxCollider3D FuseCaseSolid() {
    return {Session::fuse+Aevrynd::Vec3{-.9f,.45f,0},{.35f,.45f,.4f}};
}
inline std::vector<Aevrynd::BoxCollider3D> RangerHutSolids() {
    const auto p=Session::rangerLog;
    // 西側朝向主路，東側為實牆，南北窗戶由同一組板材／立柱碰撞體構成。
    std::vector<Aevrynd::BoxCollider3D> result{
        {p+Aevrynd::Vec3{0,2.7f,0},{2.5f,.15f,3.f}},
        {p+Aevrynd::Vec3{2.4f,1.3f,0},{.1f,1.3f,3.f}},
        {p+Aevrynd::Vec3{.9f,.5f,0},{.35f,.5f,.8f}}};
    for(float z:{-2.9f,2.9f}) {
        result.push_back({p+Aevrynd::Vec3{0,.45f,z},{2.5f,.45f,.1f}});
        result.push_back({p+Aevrynd::Vec3{0,2.4f,z},{2.5f,.2f,.1f}});
        for(float x:{-2.3f,0.f,2.3f})result.push_back({p+Aevrynd::Vec3{x,1.55f,z},{.15f,.65f,.1f}});
    }
    return result;
}
inline std::vector<Aevrynd::BoxCollider3D> RepairShelterSolids() {
    const auto p=Session::repairSites[0];
    // 東側入口開放；屋頂下緣 1.45 公尺，蹲姿 1.4 可進，站姿 1.8 被阻擋。
    return {{p+Aevrynd::Vec3{0,1.55f,0},{2.5f,.1f,2.f}},
        {p+Aevrynd::Vec3{-2.4f,.7f,0},{.1f,.7f,2.f}},
        {p+Aevrynd::Vec3{0,.7f,-1.9f},{2.5f,.7f,.1f}},
        {p+Aevrynd::Vec3{0,.7f,1.9f},{2.5f,.7f,.1f}},
        {p+Aevrynd::Vec3{0,.23f,0},{.3f,.23f,.2f}}};
}
inline const std::array<Aevrynd::BoxCollider3D,3>& CameraCeilingSolids() {
    // 只有結構屋頂約束仰角；設備與植被仍由完整碰撞清單處理避障。
    static const std::array roofs{WorkshopSolids()[0],RangerHutSolids()[0],RepairShelterSolids()[0]};
    return roofs;
}
// 渲染與碰撞共用設施尺寸，工棚東側保留入口。
inline std::vector<Aevrynd::BoxCollider3D> WorldSolids(bool repaired) {
    auto result=WorkshopSolids();
    result.push_back(FuseCaseSolid());
    const auto rangerHut=RangerHutSolids();
    result.insert(result.end(),rangerHut.begin(),rangerHut.end());
    const auto shelter=RepairShelterSolids();
    result.insert(result.end(),shelter.begin(),shelter.end());
    result.push_back(GeneratorSolid());
    for(unsigned index=0;index<3;++index) {
        const auto relay=RelaySolids(index);result.insert(result.end(),relay.begin(),relay.end());
    }
    if(repaired) result.push_back({{0,-.2f,36},{2,.2f,4}});
    const auto trees=TreePositions();
    for(const auto tree:trees) result.push_back({tree+Aevrynd::Vec3{0,2,0},{.25f,2,.25f}});
    const auto terrain=CreateTerrain();
    const auto props=ForestProps(terrain,trees);
    for(const auto& prop:props)result.push_back(prop.collider);
    const auto understory=CreateUnderstory(terrain,trees,props);
    for(const auto& tree:understory.saplings)result.push_back(SaplingCollider(tree));
    for(const auto& tree:CreateBroadleafTrees(terrain,trees,props,understory))result.push_back(tree.trunk);
    return result;
}
inline std::vector<Aevrynd::BoxCollider3D> WorldSolids(bool repaired,FirLayoutSelector selector) {
    switch(selector) {
    case FirLayoutSelector::Legacy:return WorldSolids(repaired);
    case FirLayoutSelector::ForestWood:break;
    default:throw std::invalid_argument("Invalid fir layout selector");
    }
    auto result=WorkshopSolids();
    result.push_back(FuseCaseSolid());
    const auto rangerHut=RangerHutSolids();
    result.insert(result.end(),rangerHut.begin(),rangerHut.end());
    const auto shelter=RepairShelterSolids();
    result.insert(result.end(),shelter.begin(),shelter.end());
    result.push_back(GeneratorSolid());
    for(unsigned index=0;index<3;++index) {
        const auto relay=RelaySolids(index);result.insert(result.end(),relay.begin(),relay.end());
    }
    if(repaired)result.push_back({{0,-.2f,36},{2,.2f,4}});
    const auto terrain=CreateTerrain();
    const auto layout=CreateMatureFirLayout(terrain,selector);
    for(const auto& tree:layout.firs)result.push_back(tree.collider);
    const auto props=ForestProps(terrain,layout.roots);
    for(const auto& prop:props)result.push_back(prop.collider);
    const auto understory=CreateUnderstory(terrain,layout.roots,props);
    for(const auto& tree:understory.saplings)result.push_back(SaplingCollider(tree));
    for(const auto& tree:CreateBroadleafTrees(terrain,layout.roots,props,understory))result.push_back(tree.trunk);
    return result;
}
}
