#pragma once
#include "MistwatchForestHabitatFloor.h"
#include "MistwatchSettlementSurface.h"
#include <string_view>

namespace Mistwatch::ForestLeafDomain {
struct Receipt {
    std::uint64_t sourceOriginal{},sourceLeaf{},beforeFingerprint{},afterFingerprint{},candidateFingerprint{};
    std::size_t referenced{},changed{},seams{},unusedPreserved{},bridgePreserved{},boundaryPreserved{};
};
struct Result {Aevrynd::Mesh3D mesh;Receipt receipt;};
inline bool Resolve(bool capture,bool replay,std::wstring_view profile) {
    if(profile.empty()||profile==L"original")return false;
    if(profile!=L"leaf-coverage-strength-v1")throw std::invalid_argument("LEAF_DOMAIN_PROFILE");
    if(capture==replay)throw std::invalid_argument("LEAF_DOMAIN_ISOLATED_CONTEXT");
    return true;
}
namespace Detail {
inline void Need(bool value,const char* reason){if(!value)throw std::invalid_argument(reason);}
inline bool Fixed(const Aevrynd::Vertex3D& a,const Aevrynd::Vertex3D& b) {
    return SettlementGround::SameFloat(a.x,b.x)&&SettlementGround::SameFloat(a.y,b.y)&&
        SettlementGround::SameFloat(a.z,b.z)&&a.normal==b.normal&&a.color==b.color&&SettlementGround::SameFloat(a.v,b.v);
}
inline bool Protected(const Aevrynd::Vertex3D& v) {
    return (v.z>32.f&&v.z<40.f)||std::abs(v.x)==50.f||v.z==-20.f||v.z==100.f;
}
inline void Validate(const Aevrynd::Mesh3D& mesh) {
    Need(mesh.Valid()&&mesh.vertices.size()<=100000&&mesh.indices.size()<=600000,"LEAF_DOMAIN_BUDGET");
    for(const auto& v:mesh.vertices){Aevrynd::Vec2 encoded;
        Need(Session::Finite({v.x,v.y,v.z})&&Aevrynd::TryEncodeTerrainLayerCoverage3D(v.u,v.v,encoded),"LEAF_DOMAIN_VERTEX");
    }
}
inline SettlementSurface::Result Canonical(const Aevrynd::Mesh3D& source) {
    const auto entrance=CreateEntranceRoadDomain(source);
    return SettlementSurface::Create(source,entrance,SettlementGround::Create(source,entrance));
}
inline bool Same(const SettlementSurface::Result& a,const SettlementSurface::Result& b) {
    return SettlementGround::SameMesh(a.forest,b.forest)&&SettlementGround::SameMesh(a.entrance,b.entrance)&&
        SettlementGround::SameMesh(a.gravel,b.gravel)&&a.sourceDomains==b.sourceDomains&&
        a.gravelSourceTriangles==b.gravelSourceTriangles&&a.gravelBoundaryVertices==b.gravelBoundaryVertices&&a.fingerprints==b.fingerprints;
}
}
// 兩條作者配方先各走同一正式分域器；只提取真正森林引用的葉層，絕不提交道路的未引用UV差異。
inline Result Create(const Aevrynd::Mesh3D& sourceOriginal,const Aevrynd::Mesh3D& sourceLeaf,
    const SettlementSurface::Result& original,const SettlementSurface::Result& leaf) {
    Detail::Validate(sourceOriginal);Detail::Validate(sourceLeaf);
    Detail::Need(sourceOriginal.indices==sourceLeaf.indices&&sourceOriginal.vertices.size()==sourceLeaf.vertices.size(),"LEAF_DOMAIN_SOURCE_SHAPE");
    for(std::size_t i=0;i<sourceOriginal.vertices.size();++i){const auto& a=sourceOriginal.vertices[i];const auto& b=sourceLeaf.vertices[i];
        Detail::Need(Detail::Fixed(a,b)&&b.u>=a.u,"LEAF_DOMAIN_SOURCE_FIXED");
        if(Detail::Protected(a))Detail::Need(SettlementGround::SameFloat(a.u,b.u),"LEAF_DOMAIN_PROTECTED_SOURCE");
    }
    Detail::Need(Detail::Same(original,Detail::Canonical(sourceOriginal)),"LEAF_DOMAIN_ORIGINAL_CANONICAL");
    Detail::Need(Detail::Same(leaf,Detail::Canonical(sourceLeaf)),"LEAF_DOMAIN_CANDIDATE_CANONICAL");
    Detail::Need(original.sourceDomains==leaf.sourceDomains&&original.forest.indices==leaf.forest.indices&&
        original.entrance.indices==leaf.entrance.indices&&original.gravel.indices==leaf.gravel.indices,"LEAF_DOMAIN_MEMBERSHIP");
    std::vector<std::uint8_t> membership(sourceOriginal.vertices.size());
    for(const auto index:original.forest.indices)membership[index]|=1u;
    for(const auto index:original.entrance.indices){membership[index]|=2u;
        Detail::Need(SettlementGround::SameVertex(original.entrance.vertices[index],leaf.entrance.vertices[index]),"LEAF_DOMAIN_ROAD_REFERENCED");}
    for(const auto index:original.gravel.indices){membership[index]|=4u;
        Detail::Need(SettlementGround::SameVertex(original.gravel.vertices[index],leaf.gravel.vertices[index]),"LEAF_DOMAIN_GRAVEL_REFERENCED");}
    Result result{original.forest,{}};auto& r=result.receipt;
    r.sourceOriginal=EntranceRoadDomainDetail::Fingerprint(sourceOriginal);r.sourceLeaf=EntranceRoadDomainDetail::Fingerprint(sourceLeaf);
    r.beforeFingerprint=EntranceRoadDomainDetail::Fingerprint(original.forest);r.candidateFingerprint=EntranceRoadDomainDetail::Fingerprint(leaf.forest);
    for(std::size_t i=0;i<membership.size();++i){const auto& a=original.forest.vertices[i];const auto& b=leaf.forest.vertices[i];
        Detail::Need(Detail::Fixed(a,b)&&b.u>=a.u,"LEAF_DOMAIN_FOREST_FIXED");
        r.bridgePreserved+=a.z>32.f&&a.z<40.f;
        r.boundaryPreserved+=std::abs(a.x)==50.f||a.z==-20.f||a.z==100.f;
        if(Detail::Protected(a))Detail::Need(SettlementGround::SameFloat(a.u,b.u),"LEAF_DOMAIN_PROTECTED_FOREST");
        if(!(membership[i]&1u)){++r.unusedPreserved;continue;}
        ++r.referenced;
        if(membership[i]&6u){++r.seams;
            Detail::Need(SettlementGround::SameVertex(a,b),"LEAF_DOMAIN_SEAM");continue;}
        if(!SettlementGround::SameFloat(a.u,b.u)){
            Detail::Need(b.u>a.u,"LEAF_DOMAIN_GAIN_BITS");result.mesh.vertices[i].u=b.u;++r.changed;
        }
    }
    Detail::Need(r.changed>0&&r.seams>0&&r.bridgePreserved>0&&r.boundaryPreserved>0,"LEAF_DOMAIN_NO_GAIN_OR_PROTECTION");
    r.afterFingerprint=EntranceRoadDomainDetail::Fingerprint(result.mesh);
    return result;
}
}
