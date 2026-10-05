#include "MistwatchMiddleLayer.h"
#include "MistwatchForestCover.h"
#include "MistwatchFernColonies.h"
#include "MistwatchLoadPreparation.h"
#include <Aevrynd/MeshLoader.h>
#include <bit>
#include <iostream>
#include <limits>

namespace {
using namespace Aevrynd;
using namespace Mistwatch;
void Require(bool condition,const char* message) {
    if(!condition)throw std::runtime_error(message);
}
template<class Function>void MustReject(Function function) {
    try {function();}catch(const std::invalid_argument&){return;}
    throw std::runtime_error("Invalid middle-layer input accepted");
}
bool SameTree(const BroadleafTree& a,const BroadleafTree& b) {
    const auto first=a.transform.Matrix(),second=b.transform.Matrix();
    for(unsigned row=0;row<4;++row)for(unsigned column=0;column<4;++column)
        if(first[row][column]!=second[row][column])return false;
    return Length(a.trunk.center-b.trunk.center)==0&&Length(a.trunk.extents-b.trunk.extents)==0;
}
}

int main() {
    try {
        Require(UseMiddleLayer(false,L"")&&UseMiddleLayer(false,L"0")&&UseMiddleLayer(false,L"invalid"),"Playable world can be disabled by diagnostic residue");
        Require(UseMiddleLayer(true,L"")&&UseMiddleLayer(true,L"1")&&!UseMiddleLayer(true,L"0"),"Explicit capture/default middle-layer policy changed");
        for(const auto flag:{L"2",L"01",L"true",L" "})MustReject([&]{(void)UseMiddleLayer(true,flag);});
        const auto source=std::filesystem::path(MISTWATCH_SOURCE_ROOT);
        const auto terrain=CreateTerrain();const auto conifers=TreePositions();
        const auto props=ForestProps(terrain,conifers);const auto understory=CreateUnderstory(terrain,conifers,props);
        const auto original=CreateBroadleafTrees(terrain,conifers,props,understory);
        const auto baseSolids=WorldSolids(false);
        std::array<FernColonyAsset,4> fernAssets;
        std::array<float,4> fernRadii{};
        for(unsigned i=0;i<4;++i) {
            auto loaded=LoadAevryndMeshFile(source/"engine/assets/mistwatch-fern-02/meshes"/FernMeshName(i));
            Require(bool(loaded),"Actual fern asset missing");NormalizeFernAsset(loaded.mesh);
            fernAssets[i]=FernColonyAssetFromMesh(loaded.mesh);fernRadii[i]=fernAssets[i].radius;
        }
        const auto generate=[&](unsigned target) {
            return CreateMiddleLayer(terrain,conifers,props,understory,original,baseSolids,target,fernRadii);
        };
        const auto result=generate(24),repeat=generate(24),prefix=generate(12),empty=generate(0),shortfall=generate(48);
        Require(original.size()==12&&conifers.size()==100&&understory.saplings.size()==42&&
            understory.ferns.size()==420&&baseSolids.size()==221,"Original world population changed");
        Require(result.reachedBudget&&result.trees.size()==24&&result.attempted==11395,"Authored budget changed");
        Require(empty.reachedBudget&&empty.trees.empty()&&empty.attempted==0,"Zero budget not empty");
        Require(!shortfall.reachedBudget&&shortfall.trees.size()<48&&shortfall.attempted==20252,
            "Bounded shortfall hidden or search unbounded");
        Require(prefix.reachedBudget&&prefix.trees.size()==12,"Prefix budget not reached");
        std::uint64_t fingerprint=1469598103934665603ull;
        auto solids=baseSolids;unsigned blocked{},fernChecks{},nearInitial{};
        auto trunkMesh=LoadAevryndMeshFile(source/"engine/assets/realistic-cc0-first-batch/cooked/meshes/tree-small-02"/BroadleafMeshName(2));
        Require(bool(trunkMesh),"Actual trunk asset missing");RootBroadleafMesh(trunkMesh.mesh);
        for(std::size_t i=0;i<result.trees.size();++i) {
            const auto& tree=result.trees[i];const auto matrix=tree.transform.Matrix();
            Require(SameTree(tree,repeat.trees[i]),"Repeated placement not bit exact");
            if(i<12)Require(SameTree(tree,prefix.trees[i]),"Budget changed accepted prefix");
            for(unsigned row=0;row<4;++row)for(unsigned column=0;column<4;++column) {
                fingerprint^=std::bit_cast<std::uint32_t>(matrix[row][column]);fingerprint*=1099511628211ull;
            }
            const auto crown=TransformBounds(BroadleafBounds(),tree.transform);
            Require(PropClearsObjectives({crown.center,crown.extents})&&BroadleafClearsSettlement(crown),
                "Full crown obstructs objective/settlement");
            Require(terrain.SampleNormal(tree.transform.position.x,tree.transform.position.z).y>=.97f,"Slope gate relaxed");
            for(const auto& vertex:trunkMesh.mesh.vertices)if(vertex.y<.02f) {
                const auto p=TransformPoint(matrix,{vertex.x,vertex.y,vertex.z});
                Require(std::abs(p.y-terrain.SampleHeight(p.x,p.z))<=.15f,"Actual trunk bottom floats/sinks");
            }
            // 舊蕨不重生：獨立使用真資產半徑對新樹幹測試圓形與方盒最短距離。
            for(std::size_t j=0;j<understory.ferns.size();++j) {
                const auto& fern=understory.ferns[j];
                const float dx=(std::max)(0.f,std::abs(tree.trunk.center.x-fern.position.x)-tree.trunk.extents.x);
                const float dz=(std::max)(0.f,std::abs(tree.trunk.center.z-fern.position.z)-tree.trunk.extents.z);
                Require(std::hypot(dx,dz)>=fernRadii[j%4]*fern.scale.x+.1f-1e-6f,"Original fern intersects new trunk");
                ++fernChecks;
            }
            for(std::size_t j=0;j<i;++j)Require(std::hypot(tree.transform.position.x-result.trees[j].transform.position.x,
                tree.transform.position.z-result.trees[j].transform.position.z)>=5.f,"Middle-layer spacing relaxed");
            nearInitial+=tree.transform.position.z<24.f&&std::abs(tree.transform.position.x)<20.f;
            solids.push_back(tree.trunk);
            const Vec3 start{tree.trunk.center.x+tree.trunk.extents.x+1.2f,0,tree.trunk.center.z};
            CharacterController3D actor;actor.Reset({start.x,terrain.SampleHeight(start.x,start.z),start.z});
            for(unsigned tick=0;tick<60;++tick)actor.Step({{-1,0,0},false},std::span(&tree.trunk,1),terrain,1.f/60.f);
            Require(actor.Stats().collisions&&actor.Position().x>=tree.trunk.center.x+tree.trunk.extents.x+actor.Settings().radius-.02f,
                "Controller crossed actual trunk");++blocked;
        }
        Require(fingerprint==9091870748330489572ull&&nearInitial==5,"Authored transform fingerprint changed");
        Require(MiddleLayerTransformFingerprint(result.trees)==fingerprint&&MiddleLayerTransformFingerprint(empty.trees)==0,
            "Runtime fingerprint differs from independent matrix hash");
        auto changed=result.trees;changed.front().transform.position.x+=.01f;
        Require(MiddleLayerTransformFingerprint(changed)!=fingerprint,"Position mutation hidden in fingerprint");
        std::array<ForestCoverAsset,5> grassAssets;
        for(unsigned i=0;i<5;++i) {
            auto loaded=LoadAevryndMeshFile(source/"engine/assets/mistwatch-grass-medium-02/meshes"/GrassMeshName(i));
            Require(bool(loaded),"Actual grass asset missing");NormalizeGrassAsset(loaded.mesh);
            grassAssets[i]=ForestCoverAssetFromMesh(loaded.mesh);
        }
        const auto grass=CreateForestCover(terrain,conifers,understory,solids,grassAssets);
        Require(grass.reachedBudget&&grass.plants.size()==3600&&grass.patches.size()==166,"Regenerated grass shortfall/layout drift");
        for(const auto& plant:grass.plants)Require(ForestCoverClearance(plant.transform.position,plant.radius,solids)&&
            ForestCoverFootprint(terrain,plant.transform.position,plant.radius,{}),"Regenerated grass clearance/footprint failed");
        const auto ferns=CreateFernColonies(terrain,grass,understory.ferns,solids,fernAssets);
        Require(ferns.reachedBudget&&ferns.plants.size()==300&&ferns.colonies.size()==86,"Regenerated fern shortfall/layout drift");
        for(const auto& plant:ferns.plants)Require(ForestCoverClearance(plant.transform.position,plant.radius,solids)&&
            ForestCoverFootprint(terrain,plant.transform.position,plant.radius,{})&&
            FernColonyClearsExisting(plant.transform.position,plant.radius,understory.ferns,fernAssets),"Regenerated fern footprint failed");
        CharacterController3D walker;walker.Reset({});unsigned routeTicks{};
        const auto walk=[&](Vec3 destination) {
            for(unsigned tick=0;tick<1800;++tick) {
                auto delta=destination-walker.Position();delta.y=0;if(Length(delta)<.15f)return true;
                walker.Step({Normalize(delta),false},solids,terrain,1.f/60.f);++routeTicks;
            }
            return false;
        };
        for(const auto p:Session::supplies)Require(walk({0,0,walker.Position().z})&&walk({0,0,p.z})&&walk(p),"Supply route blocked");
        for(unsigned i=0;i<Session::repairSites.size();++i) {
            const auto p=Session::repairSites[i];Require(walk({0,0,walker.Position().z})&&walk({0,0,p.z}),"Repair approach blocked");
            Require(walker.TrySetHeight(i==0?1.4f:1.8f,solids)&&walk(p+(i==0?Vec3{.65f,0,0}:Vec3{}))&&walk({0,0,p.z}),"Repair branch blocked");
            Require(walker.TrySetHeight(1.8f,solids),"Cannot leave crouch");
        }
        MustReject([&]{generate(97);});
        for(const float invalid:{0.f,-1.f,std::numeric_limits<float>::quiet_NaN(),std::numeric_limits<float>::infinity(),4.1f}) {
            auto radii=fernRadii;radii[0]=invalid;
            MustReject([&]{CreateMiddleLayer(terrain,conifers,props,understory,original,baseSolids,24,radii);});
        }
        std::cout<<"MIDDLE_LAYER_PASS; trees=24; solids="<<solids.size()<<"; fingerprint="<<fingerprint
            <<"; original_fern_checks="<<fernChecks<<"; trunk_collisions="<<blocked<<"; route_ticks="<<routeTicks
            <<"; grass=3600/166; ferns=300/86; shortfall_population="<<shortfall.trees.size()<<'\n';
        return 0;
    }catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
