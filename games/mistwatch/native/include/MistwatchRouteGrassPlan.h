#pragma once
#include "MistwatchRouteGrassAuthor.h"
#include "MistwatchRouteGrassSupport.h"
#include "MistwatchUnderstoryRetention.h"
#include "MistwatchRouteGrassContext.h"

namespace Mistwatch::RouteGrassPlan {
using namespace Aevrynd;
namespace C=CoreHerbClusters;
namespace S=RouteGrassSupport;
inline constexpr std::string_view Profile="complete-route-source-foot-v1";
struct Instance {C::Plant plant;S::Proof proof;unsigned ordinal{},bin{};};
struct Plan {std::vector<Instance> instances;std::uint64_t fingerprint{},surfaceFingerprint{};
    RouteGrassContext::Seal context;};
inline bool Resolve(bool capture,bool replay,bool coreHerbs,std::wstring_view value){
    if(value.empty()||value==L"original")return false;
    C::Detail::Need((value==L"complete-route-source-foot-v1"||value==L"complete-route-cooked-source-foot-v1")&&capture!=replay&&coreHerbs,
        "ROUTE_GRASS_ISOLATED_CONTEXT");return true;
}
inline Plan Create(const std::array<const Mesh3D*,5>& sources,const Mesh3D& actualGround,
    const HeightField3D& terrain,const WoodlandBridgeWorld::ValidatedWorld& world,
    std::span<const BoxCollider3D> retained,std::span<const UnderstoryRetention::Row> soft){
    const auto summary=UnderstoryRetention::Validate(soft);
    C::Detail::Need(summary.counts==UnderstoryRetention::Counts&&retained.size()==1024,"ROUTE_GRASS_COMPLETE_RETENTION");
    world.RequireValid();const S::Ground ground(actualGround);Plan plan;plan.surfaceFingerprint=ground.Fingerprint();
    plan.context=RouteGrassContext::Create(sources,actualGround,terrain,world,retained,soft);
    const auto sample=[&](float x,float z)->std::optional<double>{return ground.Surface().At({x,0,z}).Approximate();};
    std::array<ForestCoverAsset,5> bounds;
    for(unsigned slot=0;slot<5;++slot){C::Detail::Need(sources[slot]&&sources[slot]->Valid()&&
        C::Detail::MeshFingerprint(*sources[slot])==S::SourceFingerprints[slot],"ROUTE_GRASS_SOURCE_IDENTITY");
        bounds[slot]=ForestCoverAssetFromMesh(*sources[slot]);}
    plan.instances.reserve(RouteGrassAuthor::Target);
    std::uint64_t fingerprint=14695981039346656037ull;C::Detail::Word(fingerprint,2);
    C::Detail::Wide(fingerprint,RouteGrassContext::Fingerprint(plan.context));C::Detail::Wide(fingerprint,ground.Fingerprint());
    for(const auto& candidate:RouteGrassAuthor::Create(terrain)){
        const auto& source=*sources[candidate.variant];const auto& bound=bounds[candidate.variant];
        const float rootY=static_cast<float>(*sample(candidate.root.x,candidate.root.z));
        C::Detail::Need(std::abs(double(rootY)-candidate.root.y)<.000002,"ROUTE_GRASS_GROUND_COLLISION_DISAGREEMENT");
        const Vec3 root{candidate.root.x,rootY,candidate.root.z};
        C::Plant plant;plant.variant=candidate.variant;plant.root=root;
        plant.pose={{root.x,root.y-bound.minimumY*candidate.scale,root.z},{0,root.x+root.z,0},
            {candidate.scale,candidate.scale,candidate.scale}};
        auto conform=CreateGroundConformedMesh3D(source,plant.pose,sample);plant.root=conform.root;
        plant.derivedMesh=std::move(conform.mesh);plant.conformResult=true;
        plant.conformCounters={conform.sampledVertices,conform.movedVertices,conform.affectedFaces,conform.rebuiltNormals,
            conform.sourceMinimumY,conform.sourceMaximumY,conform.maximumGroundDelta};
        C::Detail::AuthorRootWind(plant,source);
        plant.radius=std::nextafter(bound.radius*candidate.scale+C::WindRadius,std::numeric_limits<float>::infinity());
        CanopyVolume::PlantLocalVolume local;C::Detail::Need(CanopyVolume::MeasureLocalVolume(
            std::span<const Vertex3D>(plant.derivedMesh.vertices),local),"ROUTE_GRASS_DERIVED_VOLUME");
        const auto volume=C::Detail::Volume(plant,local);
        C::Detail::Need(RoadsideCoverRouteClearance(root,plant.radius)&&ForestCoverFootprint(terrain,root,plant.radius,{})&&
            world.Clears(volume,false)&&world.Clears(volume,true)&&CanopyVolume::ClearsCanopyWood(volume,retained),
            "ROUTE_GRASS_COMPLETE_WORLD_REJECTED");
        for(const auto& old:soft)C::Detail::Need(std::hypot(double(root.x)-old.anchor.x,double(root.z)-old.anchor.z)>=
            .65*(double(plant.radius)+old.footprintRadius),"ROUTE_GRASS_EXISTING_SPACING");
        for(const auto& old:plan.instances)C::Detail::Need(std::hypot(double(root.x)-old.plant.root.x,double(root.z)-old.plant.root.z)>=
            .65*(double(plant.radius)+old.plant.radius),"ROUTE_GRASS_NEW_SPACING");
        const auto proof=S::Check(static_cast<S::Source>(candidate.variant),ground,source,plant.derivedMesh,
            plant.pose,plant.root,plant.radius);
        C::Detail::Need(proof.contact.safe&&proof.allFaces==source.indices.size()/3&&proof.sourceFootPieces&&proof.windPinnedVertices,
            "ROUTE_GRASS_SOURCE_FOOT_REJECTED");
        plant.sourceFingerprint=proof.sourceFingerprint;plant.derivedFingerprint=proof.derivedFingerprint;
        C::Detail::Word(fingerprint,candidate.ordinal);C::Detail::Word(fingerprint,candidate.bin);
        C::Detail::Wide(fingerprint,proof.sourceFingerprint);C::Detail::Wide(fingerprint,proof.derivedFingerprint);
        for(float f:{plant.pose.position.x,plant.pose.position.y,plant.pose.position.z,plant.pose.rotation.y,
            plant.pose.scale.x,plant.root.x,plant.root.y,plant.root.z,plant.radius})C::Detail::Float(fingerprint,f);
        plan.instances.push_back({std::move(plant),proof,candidate.ordinal,candidate.bin});
    }
    C::Detail::Need(plan.instances.size()==RouteGrassAuthor::Target,"ROUTE_GRASS_POPULATION_SHORTFALL");
    plan.fingerprint=fingerprint;return plan;
}
}
