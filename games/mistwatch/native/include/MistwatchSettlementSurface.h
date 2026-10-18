#pragma once
#include "MistwatchSettlementGround.h"
#include "MistwatchEntranceRoadMaterial.h"
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <tuple>
#include <vector>

namespace Mistwatch::SettlementSurface {
inline constexpr const char* Profile="workshop-gravel-surface-candidate-v1";
enum class Domain : std::uint8_t { Forest,Entrance,Gravel };
struct Result {
    Aevrynd::Mesh3D forest,entrance,gravel;
    std::vector<Domain> sourceDomains;
    std::vector<std::uint32_t> gravelSourceTriangles,gravelBoundaryVertices;
    std::array<std::uint64_t,3> fingerprints{};
};
namespace Detail {
inline bool SameReceipt(const EntranceRoadDomainReceipt& a,const EntranceRoadDomainReceipt& b) {
    const auto fields=[](const EntranceRoadDomainReceipt& r){return std::tie(r.sourceFingerprint,r.forestFingerprint,
        r.roadFingerprint,r.sourceTriangles,r.forestTriangles,r.roadTriangles,r.sharedVertices,
        r.forestChangedVertices,r.roadChangedVertices);};
    return fields(a)==fields(b);
}
inline bool SameDomain(const EntranceRoadDomainResult& a,const EntranceRoadDomainResult& b) {
    return SettlementGround::SameMesh(a.forest,b.forest)&&SettlementGround::SameMesh(a.road,b.road)&&
        a.sharedBoundaryVertices==b.sharedBoundaryVertices&&a.triangleIsRoad==b.triangleIsRoad&&SameReceipt(a.receipt,b.receipt);
}
inline Aevrynd::MaterialTextureLayerDefinition3D BaseLayer(const Aevrynd::MaterialTextureSetDefinition3D& t) {
    return {t.baseColorPath,t.roughnessPath,t.normalPath,t.surfaceEncoding,t.integrity};
}
inline bool SameLayer(const Aevrynd::MaterialTextureLayerDefinition3D& a,const Aevrynd::MaterialTextureLayerDefinition3D& b) {
    if(a.baseColorPath!=b.baseColorPath||a.roughnessPath!=b.roughnessPath||a.normalPath!=b.normalPath||
        a.surfaceEncoding!=b.surfaceEncoding)return false;
    for(unsigned i=0;i<3;++i)if(a.integrity[i].expectedBytes!=b.integrity[i].expectedBytes||
        a.integrity[i].expectedSha256!=b.integrity[i].expectedSha256)return false;
    return true;
}
}
// 分域只拆既有索引；不移動地形、不加入覆蓋平面，也不修改既有地坪作者器。
inline Result Create(const Aevrynd::Mesh3D& source,const EntranceRoadDomainResult& entrance,
    const SettlementGround::Result& settlement) {
    const auto canonicalEntrance=CreateEntranceRoadDomain(source);
    if(!Detail::SameDomain(entrance,canonicalEntrance))
        throw std::invalid_argument("SETTLEMENT_SURFACE_ENTRANCE_MISMATCH");
    const auto canonical=SettlementGround::Create(source,canonicalEntrance);
    if(!Detail::SameDomain(settlement.partition,canonical.partition)||
        settlement.movedSourceTriangles!=canonical.movedSourceTriangles||
        settlement.newBoundaryVertices!=canonical.newBoundaryVertices||
        settlement.changedForestVertices!=canonical.changedForestVertices||
        settlement.changedRoadVertices!=canonical.changedRoadVertices)
        throw std::invalid_argument("SETTLEMENT_SURFACE_SETTLEMENT_MISMATCH");
    Result out;
    out.forest=canonical.partition.forest;
    out.entrance=canonicalEntrance.road;
    out.gravel=canonical.partition.road;
    const auto prefix=canonicalEntrance.road.indices.size();
    if(prefix>=out.gravel.indices.size()||
        !std::equal(canonicalEntrance.road.indices.begin(),canonicalEntrance.road.indices.end(),out.gravel.indices.begin()))
        throw std::invalid_argument("SETTLEMENT_SURFACE_ROAD_PREFIX");
    out.gravel.indices.erase(out.gravel.indices.begin(),out.gravel.indices.begin()+static_cast<std::ptrdiff_t>(prefix));
    out.gravelSourceTriangles=canonical.movedSourceTriangles;
    out.gravelBoundaryVertices=canonical.newBoundaryVertices;
    out.sourceDomains.resize(source.indices.size()/3,Domain::Forest);
    for(std::size_t t=0;t<out.sourceDomains.size();++t)
        if(canonicalEntrance.triangleIsRoad[t])out.sourceDomains[t]=Domain::Entrance;
    for(const auto t:out.gravelSourceTriangles)out.sourceDomains[t]=Domain::Gravel;
    if(out.forest.indices.size()+out.entrance.indices.size()+out.gravel.indices.size()!=source.indices.size()||
        out.gravel.indices.size()!=out.gravelSourceTriangles.size()*3)
        throw std::invalid_argument("SETTLEMENT_SURFACE_MEMBERSHIP");
    for(auto* mesh:{&out.forest,&out.entrance,&out.gravel}){
        mesh->InvalidateRaycastIndex();
        if(!mesh->Valid())throw std::invalid_argument("SETTLEMENT_SURFACE_MESH_INVALID");
    }
    out.fingerprints={EntranceRoadDomainDetail::Fingerprint(out.forest),
        EntranceRoadDomainDetail::Fingerprint(out.entrance),EntranceRoadDomainDetail::Fingerprint(out.gravel)};
    return out;
}
// 原始 ground108 三通道與封印完整轉為基層；共用落葉、岩層、取樣比例及材質參數不變。
inline Aevrynd::MaterialAssetDefinition3D MaterialDefinition(const Aevrynd::MaterialAssetDefinition3D& forest) {
    const auto expected=HabitatGroundDefinition(true,true);
    if(forest.material.materialClass!=Aevrynd::MaterialClass3D::Terrain||
        !Detail::SameLayer(Detail::BaseLayer(forest.textures),Detail::BaseLayer(expected.textures))||
        !Detail::SameLayer(forest.textures.additionalLayers[0],expected.textures.additionalLayers[0])||
        !Detail::SameLayer(forest.textures.additionalLayers[1],expected.textures.additionalLayers[1])||
        forest.textures.maximumResolution!=expected.textures.maximumResolution||
        forest.textures.alphaMode!=expected.textures.alphaMode||
        !SettlementGround::SameFloat(forest.textures.alphaCutoff,expected.textures.alphaCutoff))
        throw std::invalid_argument("SETTLEMENT_SURFACE_MATERIAL_MISMATCH");
    auto result=forest;
    result.name="settlement-gravel";
    const auto ground=GroundMaterialDefinition();
    const auto& gravel=ground.textures.additionalLayers[0];
    result.textures.baseColorPath=gravel.baseColorPath;
    result.textures.roughnessPath=gravel.roughnessPath;
    result.textures.normalPath=gravel.normalPath;
    result.textures.surfaceEncoding=gravel.surfaceEncoding;
    result.textures.integrity=gravel.integrity;
    return result;
}
}
