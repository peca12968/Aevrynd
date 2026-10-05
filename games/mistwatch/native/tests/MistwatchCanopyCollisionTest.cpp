#include "MistwatchCanopyCollision.h"
#include "MistwatchCanopyPopulation.h"
#include "MistwatchRootStemTerrain.h"
#include "MistwatchMiddleLayer.h"
#include "MistwatchShrubCanopy.h"
#include <Aevrynd/MeshLoader.h>
#include <Aevrynd/CharacterController3D.h>
#include <iostream>

using namespace Aevrynd;
using namespace Mistwatch;
void Require(bool value,const char* text){if(!value)throw std::runtime_error(text);}
void Mix(std::uint64_t& hash,std::uint32_t bits){
    for(unsigned byte=0;byte<4;++byte){hash^=(bits>>(byte*8))&255u;hash*=1099511628211ull;}
}
std::uint64_t ColliderIdentity(std::span<const BoxCollider3D> boxes){
    std::uint64_t hash=14695981039346656037ull;
    for(const auto& b:boxes)for(float v:{b.center.x,b.center.y,b.center.z,b.extents.x,b.extents.y,b.extents.z})Mix(hash,std::bit_cast<std::uint32_t>(v));
    return hash;
}
std::uint64_t GrassIdentity(const ForestCoverResult& grass){
    std::uint64_t hash=14695981039346656037ull;
    for(const auto& p:grass.plants){const auto r=p.transform.position;
        for(float v:{r.x,r.y,r.z,ForestCoverScale(r),r.x+r.z})Mix(hash,std::bit_cast<std::uint32_t>(v));Mix(hash,p.variant);}
    return hash;
}
std::uint64_t FernIdentity(const FernColonyResult& fern){
    std::uint64_t hash=14695981039346656037ull;
    for(const auto& p:fern.plants){const auto& t=p.transform;
        for(float v:{t.position.x,t.position.y,t.position.z,t.rotation.x,t.rotation.y,t.rotation.z,t.scale.x,t.scale.y,t.scale.z})Mix(hash,std::bit_cast<std::uint32_t>(v));Mix(hash,p.variant);}
    return hash;
}
// 此處獨立驗所有實際頂點的世界 AABB；不以配置器 helper 自我驗證。
void VerifyMeshClearance(const Mesh3D& mesh,const Transform3D& pose,std::span<const BoxCollider3D> wood){
    const auto matrix=pose.Matrix();
    Vec3 low{INFINITY,INFINITY,INFINITY},high{-INFINITY,-INFINITY,-INFINITY};
    for(const auto& v:mesh.vertices){const auto p=TransformPoint(matrix,{v.x,v.y,v.z});
        low.x=(std::min)(low.x,p.x);low.y=(std::min)(low.y,p.y);low.z=(std::min)(low.z,p.z);
        high.x=(std::max)(high.x,p.x);high.y=(std::max)(high.y,p.y);high.z=(std::max)(high.z,p.z);}
    for(const auto& box:wood){const auto a=box.center-box.extents,b=box.center+box.extents;
        Require(high.x<a.x||low.x>b.x||high.y<a.y||low.y>b.y||high.z<a.z||low.z>b.z,"actual static plant mesh intersects canopy wood");}
}
bool BodyOverlap(Vec3 feet,const BoxCollider3D& box){
    const auto d=feet+Vec3{0,.9f,0}-box.center;
    return std::abs(d.x)<.45f+box.extents.x-.0001f&&std::abs(d.y)<.9f+box.extents.y-.0001f&&std::abs(d.z)<.45f+box.extents.z-.0001f;
}
unsigned Walk(CharacterController3D& actor,Vec3 endpoint,const HeightField3D& terrain,std::span<const BoxCollider3D> colliders){
    unsigned ticks=0;
    while(std::abs(actor.Position().z-endpoint.z)>.003f&&ticks<512){
        const float delta=endpoint.z-actor.Position().z;
        actor.Step({{0,0,std::clamp(delta*20.f,-.3f,.3f)},false,1},colliders,terrain,1.f/120.f);
        for(const auto& box:colliders)Require(!BodyOverlap(actor.Position(),box),"route body intersects collider");
        ++ticks;
    }
    Require(ticks<512&&std::abs(actor.Position().x-endpoint.x)<.0001f,"body route failed to reach endpoint");
    Require(actor.Grounded()&&std::abs(actor.Position().y-terrain.SampleHeight(actor.Position().x,actor.Position().z))<.0001f,"route ground support failed");
    return ticks;
}
int main(){try{
    const std::filesystem::path repo=MISTWATCH_SOURCE_ROOT;
    const auto terrain=CreateTerrain();const auto trees=TreePositions();const auto props=ForestProps(terrain,trees);
    const auto understory=CreateUnderstory(terrain,trees,props);const auto broadleaf=CreateBroadleafTrees(terrain,trees,props,understory);
    auto original=WorldSolids(false);std::array<FernColonyAsset,4> fernAssets;
    for(unsigned i=0;i<4;++i){auto loaded=LoadAevryndMeshFile(repo/"engine/assets/mistwatch-fern-02/meshes"/FernMeshName(i));Require(bool(loaded),"fern source unavailable");NormalizeFernAsset(loaded.mesh);fernAssets[i]=FernColonyAssetFromMesh(loaded.mesh);}
    const std::array<float,4> radii{fernAssets[0].radius,fernAssets[1].radius,fernAssets[2].radius,fernAssets[3].radius};
    const auto middle=CreateMiddleLayer(terrain,trees,props,understory,broadleaf,original,24,radii);
    Require(middle.reachedBudget,"original middle layer incomplete");for(const auto& p:middle.trees)original.push_back(p.trunk);
    Require(original.size()==245,"original collision world changed");
    const auto meshFolder=repo/"engine/assets/realistic-cc0-first-batch/cooked/meshes/island-tree-01";
    constexpr const char* names[]{"island-tree-01-lod2-node0-mesh0-primitive0-material0-island-tree-01.aemesh","island-tree-01-lod1-node0-mesh0-primitive1-material1-island-tree-01-leaves.aemesh","island-tree-01-lod2-node0-mesh0-primitive2-material2-island-tree-01-branches.aemesh"};
    std::array<Mesh3D,3> meshes;
    for(unsigned i=0;i<3;++i){auto loaded=LoadAevryndMeshFile(meshFolder/names[i]);Require(bool(loaded),"actual canopy source unavailable");meshes[i]=std::move(loaded.mesh);}
    const auto geometry=MeasureCanopy(meshes);
    auto population=CreateCanopyPopulation(meshes,geometry,terrain,original,understory,{},{},fernAssets,{});
    Require(population.reachedBudget&&population.fingerprint==13093722598475582899ull,"true canopy pose identity changed");
    auto stem=LoadAevryndMeshFile(MISTWATCH_DERIVED_STEM);Require(bool(stem),"derived stem unavailable");
    std::vector<BoxCollider3D> wood;std::uint64_t triangles=0,surfaceCells=0,enclosedCells=0;
    for(auto& plant:population.plants){
        const auto root=PlantDerivedRootStem(stem.mesh,geometry,plant.pose,terrain);
        plant.lower=ExtendRootCollision(plant.lower,root,plant.pose);
        const auto proxy=CreateCanopyWoodProxy(root,meshes[2],plant.pose);
        Require(!proxy.parts.empty()&&proxy.enclosedGridCells>0,"root proxy occupancy incomplete");
        wood.insert(wood.end(),proxy.parts.begin(),proxy.parts.end());triangles+=proxy.sourceTriangles;
        surfaceCells+=proxy.surfaceCells;enclosedCells+=proxy.enclosedGridCells;
    }
    Require(wood.size()==4267&&triangles==304768,"current geometry proxy identity changed");
    auto combined=original;combined.insert(combined.end(),wood.begin(),wood.end());
    auto oldCombined=original;for(const auto& plant:population.plants)oldCombined.push_back(plant.lower);
    Require(surfaceCells==17911&&enclosedCells==207&&ColliderIdentity(combined)==230929787695590120ull,"wood author occupancy or world collider identity drift");
    Require(ColliderIdentity(oldCombined)==16643660641049883501ull,"legacy forest collision identity drift");
    std::array<Mesh3D,5> grassMeshes;std::array<ForestCoverAsset,5> grassAssets;
    for(unsigned i=0;i<5;++i){auto loaded=LoadAevryndMeshFile(repo/"engine/assets/mistwatch-grass-medium-02/meshes"/GrassMeshName(i));Require(bool(loaded),"grass source unavailable");
        NormalizeGrassAsset(loaded.mesh);grassMeshes[i]=std::move(loaded.mesh);grassAssets[i]=ForestCoverAssetFromMesh(grassMeshes[i]);}
    std::array<Mesh3D,4> fernMeshes;
    for(unsigned i=0;i<4;++i){auto loaded=LoadAevryndMeshFile(repo/"engine/assets/mistwatch-fern-02/meshes"/FernMeshName(i));Require(bool(loaded),"fern clearance source unavailable");NormalizeFernAsset(loaded.mesh);fernMeshes[i]=std::move(loaded.mesh);}
    auto shrubSource=LoadAevryndMeshFile(repo/"engine/assets/realistic-cc0-first-batch/cooked/meshes/tree-small-02"/BroadleafMeshName(1));Require(bool(shrubSource),"shrub source unavailable");
    const auto grass=CreateForestCover(terrain,trees,understory,original,grassAssets,{},wood);
    const auto fern=CreateFernColonies(terrain,grass,understory.ferns,original,fernAssets,{},wood);
    const auto shrub=CreateShrubCanopy(shrubSource.mesh,terrain,original,understory,grass,fern,fernAssets,64,wood);
    Require(grass.reachedBudget&&grass.plants.size()==3600&&grass.patches.size()==168&&GrassIdentity(grass)==6471420486779392182ull,"wood grass author identity drift");
    Require(fern.reachedBudget&&fern.plants.size()==300&&fern.colonies.size()==78&&FernIdentity(fern)==9573416696332517026ull,"wood fern author identity drift");
    Require(shrub.reachedBudget&&shrub.plants.size()==64&&shrub.fingerprint==6244520233766686875ull,"wood shrub author identity drift");
    std::array<unsigned,5> grassCounts{};std::array<unsigned,4> fernCounts{};unsigned restoredGrass=0,restoredFern=0,restoredShrub=0;
    const auto insideOldCrown=[&](Vec3 p){return std::any_of(population.plants.begin(),population.plants.end(),[&](const auto& c){return std::abs(p.x-c.lower.center.x)<c.lower.extents.x&&std::abs(p.z-c.lower.center.z)<c.lower.extents.z;});};
    for(const auto& p:grass.plants){++grassCounts[p.variant];restoredGrass+=insideOldCrown(p.transform.position);VerifyMeshClearance(grassMeshes[p.variant],p.transform,wood);}
    for(const auto& p:fern.plants){++fernCounts[p.variant];restoredFern+=insideOldCrown(p.transform.position);VerifyMeshClearance(fernMeshes[p.variant],p.transform,wood);}
    for(const auto& p:shrub.plants){restoredShrub+=insideOldCrown(p.pose.position);VerifyMeshClearance(shrub.variants[p.variant].stems,p.pose,wood);VerifyMeshClearance(shrub.variants[p.variant].leaves,p.pose,wood);}
    Require(grassCounts==std::array<unsigned,5>{929,749,614,528,780}&&fernCounts==std::array<unsigned,4>{11,22,165,102},"wood vegetation variant identities drift");
    Require(restoredGrass>0&&restoredFern>0&&restoredShrub>0,"wood collision failed to restore ground vegetation under old crown air walls");
    const auto legacyGrass=CreateForestCover(terrain,trees,understory,oldCombined,grassAssets);
    const auto legacyFern=CreateFernColonies(terrain,legacyGrass,understory.ferns,oldCombined,fernAssets);
    const auto legacyShrub=CreateShrubCanopy(shrubSource.mesh,terrain,oldCombined,understory,legacyGrass,legacyFern,fernAssets);
    Require(legacyGrass.reachedBudget&&legacyGrass.patches.size()==168&&GrassIdentity(legacyGrass)==4989138190905410642ull,"legacy canopy grass identity drift");
    Require(legacyFern.reachedBudget&&legacyFern.colonies.size()==85&&FernIdentity(legacyFern)==5579852658837856591ull,"legacy canopy fern identity drift");
    Require(legacyShrub.reachedBudget&&legacyShrub.fingerprint==5849093981174083644ull,"legacy canopy shrub identity drift");
    std::cout<<"WOOD_WORLD_AUTHOR_PASS grass_fp="<<GrassIdentity(grass)<<" fern_fp="<<FernIdentity(fern)<<" shrub_fp="<<shrub.fingerprint
        <<" collider_fp="<<ColliderIdentity(combined)<<" under_crown_grass="<<restoredGrass<<" under_crown_fern="<<restoredFern<<" under_crown_shrub="<<restoredShrub
        <<" full_static_mesh_clearance_checked=4028 legacy_canopy_identities_preserved=1 dynamic_wind_unverified=1\n";
    const Vec3 witnesses[]{
        {-18.3179742552f,0,7.2568771204f},{15.7204334376f,0,4.331083912f},
        {-19.4306881992f,0,-.3230881732f},{-29.8309511696f,0,80.7272427048f}};
    unsigned routeTicks=0;
    for(unsigned i=0;i<4;++i){
        const auto& plant=population.plants[i];auto target=witnesses[i];target.y=terrain.SampleHeight(target.x,target.z);
        Require(BodyOverlap(target,plant.lower),"original full canopy obstacle not reproduced");
        auto start=target;start.z=plant.lower.center.z-plant.lower.extents.z-.65f;start.y=terrain.SampleHeight(start.x,start.z);
        Require(!BodyOverlap(start,plant.lower),"route starts inside old canopy envelope");
        CharacterController3D actor;Require(actor.Settings().radius==.45f&&actor.Settings().height==1.8f,"game body dimensions changed");
        actor.Reset(start);actor.Step({},combined,terrain,1.f/120.f);
        routeTicks+=Walk(actor,target,terrain,combined);routeTicks+=Walk(actor,start,terrain,combined);
        CharacterController3D oldActor;oldActor.Reset(start);
        for(unsigned tick=0;tick<192;++tick){const float delta=target.z-oldActor.Position().z;
            oldActor.Step({{0,0,std::clamp(delta*20.f,-.3f,.3f)},false,1},oldCombined,terrain,1.f/120.f);}
        Require(std::abs(oldActor.Position().z-target.z)>.2f&&oldActor.Stats().collisions>0,"old crown box no longer reproduces blocked walk");
        CollisionWorld3D world;for(const auto& box:wood)Require(bool(world.CreateBox({box})),"wood query setup failed");
        const auto root=plant.pose.position;const float ground=terrain.SampleHeight(root.x,root.z);
        const auto hit=world.SweepSphere({root.x-1.5f,ground+.4f,root.z},.15f,{3,0,0});
        Require(bool(hit)&&hit.fraction<1,"wood surface interception failed");
    }
    std::cout<<"CANOPY_COLLISION_PASS trees=4 source_triangles="<<triangles<<" wood_boxes="<<wood.size()
        <<" original_boxes=245 body_radius=.45 body_height=1.8 round_trip_routes=4 terrain_ticks="<<routeTicks
        <<" wood_sweep_interceptions=4 old_crown_blocked_routes=4 camera_game_loop_validated=0\n";return 0;
}catch(const std::exception& e){std::cerr<<"CANOPY_COLLISION_FIRST_DIVERGENCE "<<e.what()<<'\n';return 1;}}
