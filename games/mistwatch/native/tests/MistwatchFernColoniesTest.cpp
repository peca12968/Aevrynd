#include "MistwatchFernColonies.h"
#include "MistwatchMiddleLayer.h"
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
    throw std::runtime_error("Invalid input was accepted");
}
std::uint64_t HashTransform(std::uint64_t hash,const Transform3D& transform) {
    const auto matrix=transform.Matrix();
    for(unsigned row=0;row<4;++row)for(unsigned column=0;column<4;++column) {
        hash^=std::bit_cast<std::uint32_t>(matrix[row][column]);hash*=1099511628211ull;
    }
    return hash;
}
std::uint64_t WorldFingerprint(std::span<const Vec3> trees,const ForestUnderstory& understory,
    std::span<const BroadleafTree> broadleaf,std::span<const BoxCollider3D> solids,const ForestCoverResult& grass) {
    std::uint64_t hash=14695981039346656037ull;
    for(const auto root:trees)hash=HashTransform(hash,{root,{},{1,1,1}});
    for(const auto& fern:understory.ferns)hash=HashTransform(hash,fern);
    for(const auto& sapling:understory.saplings)hash=HashTransform(hash,sapling);
    for(const auto& tree:broadleaf) {
        hash=HashTransform(hash,tree.transform);hash=HashTransform(hash,{tree.trunk.center,{},tree.trunk.extents});
    }
    for(const auto& solid:solids)hash=HashTransform(hash,{solid.center,{},solid.extents});
    for(const auto& plant:grass.plants) {
        hash=HashTransform(hash,plant.transform);hash^=plant.variant;hash*=1099511628211ull;
    }
    return hash;
}
bool SamePlant(const FernColonyPlant& a,const FernColonyPlant& b) {
    if(a.variant!=b.variant||a.colony!=b.colony||a.grassPatch!=b.grassPatch||a.radius!=b.radius)return false;
    const auto first=a.transform.Matrix(),second=b.transform.Matrix();
    for(unsigned row=0;row<4;++row)for(unsigned column=0;column<4;++column)
        if(first[row][column]!=second[row][column])return false;
    return true;
}
bool SamePopulation(const FernColonyResult& first,const FernColonyResult& second) {
    if(first.plants.size()!=second.plants.size()||first.colonies.size()!=second.colonies.size()||
       first.reachedBudget!=second.reachedBudget)return false;
    for(std::size_t i=0;i<first.plants.size();++i)if(!SamePlant(first.plants[i],second.plants[i]))return false;
    for(std::size_t i=0;i<first.colonies.size();++i) {
        const auto& a=first.colonies[i];const auto& b=second.colonies[i];
        if(a.centre.x!=b.centre.x||a.centre.y!=b.centre.y||a.centre.z!=b.centre.z||a.radius!=b.radius||
           a.grassPatch!=b.grassPatch||a.requested!=b.requested||a.accepted!=b.accepted)return false;
    }
    return true;
}
float TriangleHeight(const HeightField3D& terrain,float x,float z) {
    // 獨立以真正格點三角面重建高度，不以解析 HillElevation 或平滑曲面替代。
    const auto& grid=terrain.Settings();
    const float gx=(x-grid.origin.x)/grid.cellSize,gz=(z-grid.origin.z)/grid.cellSize;
    const unsigned ix=(std::min)(unsigned(std::floor(gx)),grid.width-2);
    const unsigned iz=(std::min)(unsigned(std::floor(gz)),grid.depth-2);
    const float tx=gx-float(ix),tz=gz-float(iz);
    const float a=terrain.Height(ix,iz),b=terrain.Height(ix+1,iz),c=terrain.Height(ix,iz+1),d=terrain.Height(ix+1,iz+1);
    return tx+tz<=1.f?a+(b-a)*tx+(c-a)*tz:d+(c-d)*(1.f-tx)+(b-d)*(1.f-tz);
}
struct Ratios {float original{100},grass{100},added{100};};
Ratios CheckPopulation(const FernColonyResult& population,const ForestCoverResult& grass,
    const ForestUnderstory& original,std::span<const BoxCollider3D> solids,const HeightField3D& terrain,
    const std::array<FernColonyAsset,4>& assets,const std::array<Mesh3D,4>& meshes) {
    Ratios ratios;const ForestCoverSettings footprintSettings{};
    std::array<unsigned,4> variants{};std::vector<unsigned> accepted(population.colonies.size());
    for(std::size_t index=0;index<population.plants.size();++index) {
        const auto& plant=population.plants[index];const auto root=plant.transform.position;
        Require(plant.variant<4&&plant.colony<population.colonies.size()&&plant.grassPatch<grass.patches.size(),"Invalid identity");
        ++variants[plant.variant];++accepted[plant.colony];
        Require(Session::Finite(root)&&Session::Finite(plant.transform.rotation)&&Session::Finite(plant.transform.scale),"Nonfinite generated transform");
        Require(plant.transform.rotation.x==0&&plant.transform.rotation.z==0&&plant.transform.scale.x>=.6f&&
            plant.transform.scale.x<=.78f&&plant.transform.scale.x==plant.transform.scale.y&&
            plant.transform.scale.x==plant.transform.scale.z,"Fern scale or rotation altered");
        Require(plant.radius==assets[plant.variant].radius*plant.transform.scale.x,"Measured footprint mismatch");
        Require(terrain.Contains(root.x,root.z)&&std::abs(root.y-TriangleHeight(terrain,root.x,root.z))<2e-5f,"Triangle groundroot mismatch");
        Require(ForestCoverClearance(root,plant.radius,solids)&&ForestCoverFootprint(terrain,root,plant.radius,footprintSettings),"Clearance or terrain gate failed");
        const auto& colony=population.colonies[plant.colony];const auto& anchor=grass.patches[plant.grassPatch];
        Require(colony.grassPatch==plant.grassPatch&&std::hypot(root.x-colony.centre.x,root.z-colony.centre.z)<=colony.radius+1e-5f,"Colony identity mismatch");
        const float offset=std::hypot(colony.centre.x-anchor.centre.x,colony.centre.z-anchor.centre.z);
        Require(offset>=anchor.radius*.9f-1e-5f&&offset<=anchor.radius*1.3f+1e-5f,"Not anchored to actual grass patch");
        for(const auto& solid:solids)Require(std::abs(root.x-solid.center.x)>=solid.extents.x+plant.radius||
            std::abs(root.z-solid.center.z)>=solid.extents.z+plant.radius,"Solid full radius violated");
        for(std::size_t i=0;i<original.ferns.size();++i) {
            const auto& fern=original.ferns[i];const float sum=plant.radius+assets[i%4].radius*fern.scale.x;
            const float distance=std::hypot(root.x-fern.position.x,root.z-fern.position.z);
            Require(distance>=.65f*sum,"Original fern root spacing violated");ratios.original=(std::min)(ratios.original,distance/sum);
        }
        for(const auto& blade:grass.plants) {
            const float sum=plant.radius+blade.radius,distance=std::hypot(root.x-blade.transform.position.x,root.z-blade.transform.position.z);
            Require(distance>=.65f*sum,"Grass root spacing violated");ratios.grass=(std::min)(ratios.grass,distance/sum);
        }
        for(std::size_t i=0;i<index;++i) {
            const auto& fern=population.plants[i];const float sum=plant.radius+fern.radius;
            const float distance=std::hypot(root.x-fern.transform.position.x,root.z-fern.transform.position.z);
            Require(distance>=.65f*sum,"New fern root spacing violated");ratios.added=(std::min)(ratios.added,distance/sum);
        }
        // 全部真頂點都包含於保守水平腳印；只驗證地形高差，不宣稱葉片不埋地。
        const auto matrix=plant.transform.Matrix();
        for(const auto& vertex:meshes[plant.variant].vertices) {
            const auto world=TransformPoint(matrix,{vertex.x,vertex.y,vertex.z});
            Require(std::hypot(world.x-root.x,world.z-root.z)<=plant.radius+1e-4f&&terrain.Contains(world.x,world.z),"True vertex escaped footprint");
            Require(std::abs(TriangleHeight(terrain,world.x,world.z)-root.y)<=.24002f&&
                terrain.SampleNormal(world.x,world.z).y>=.87999f,"True vertex terrain footprint failed");
        }
    }
    for(unsigned variant=0;variant<4;++variant)Require(variants[variant]>0,"Fern variant absent");
    for(std::size_t i=0;i<population.colonies.size();++i)Require(accepted[i]==population.colonies[i].accepted&&
        accepted[i]>0&&accepted[i]<=population.colonies[i].requested,"Colony accounting failed");
    return ratios;
}
}
int main(int argc,char** argv) {
    try {
        const bool useMiddle=argc==4&&std::string_view(argv[3])=="--middle-layer";
        Require(argc==3||useMiddle,"Expected grass and fern directories, optional --middle-layer");
        const std::filesystem::path grassDirectory(argv[1]),fernDirectory(argv[2]);
        const auto terrain=CreateTerrain();const auto trees=TreePositions();const auto props=ForestProps(terrain,trees);
        const auto original=CreateUnderstory(terrain,trees,props);const auto broadleaf=CreateBroadleafTrees(terrain,trees,props,original);
        auto solids=WorldSolids(false);
        Require(trees.size()==100&&original.ferns.size()==420&&original.saplings.size()==42&&broadleaf.size()==12&&solids.size()==221,"Original world population changed");
        if(useMiddle) {
            const auto middle=CreateMiddleLayer(terrain,trees,props,original,broadleaf,solids);
            Require(middle.reachedBudget&&middle.trees.size()==24,"Production middle-layer population incomplete");
            for(const auto& tree:middle.trees)solids.push_back(tree.trunk);
            Require(solids.size()==245,"Production collision world incomplete");
        }
        std::array<ForestCoverAsset,5> grassAssets;
        for(unsigned variant=0;variant<5;++variant) {
            auto loaded=LoadAevryndMeshFile(grassDirectory/GrassMeshName(variant));
            Require(bool(loaded)&&loaded.mesh.vertices.size()==GrassVertexCounts[variant]&&
                loaded.mesh.indices.size()==3u*GrassTriangleCounts[variant],"Real grass mesh unavailable");
            NormalizeGrassAsset(loaded.mesh);grassAssets[variant]=ForestCoverAssetFromMesh(loaded.mesh);
        }
        const auto grass=CreateForestCover(terrain,trees,original,solids,grassAssets);
        Require(grass.reachedBudget&&grass.plants.size()==3600&&grass.patches.size()==(useMiddle?154u:165u),"River-bank true grass author profile changed");
        const auto before=WorldFingerprint(trees,original,broadleaf,solids,grass);
        Require(before==(useMiddle?14570490119226306139ull:14319149161271836151ull),"River-bank true world author fingerprint changed");
        std::array<Mesh3D,4> meshes;std::array<FernColonyAsset,4> assets;
        constexpr std::array<float,4> expectedRadius{.981686473f,.821101248f,.549620509f,.635080874f};
        constexpr std::array<float,4> expectedHeight{.635495126f,.512394726f,.412848055f,.319213778f};
        constexpr std::array<float,4> expectedMinimumY{-.0488164499f,-.0460378788f,-.0460376292f,-.0217263121f};
        for(unsigned variant=0;variant<4;++variant) {
            auto loaded=LoadAevryndMeshFile(fernDirectory/FernMeshName(variant));
            Require(bool(loaded)&&loaded.mesh.vertices.size()==FernVertexCounts[variant]&&
                loaded.mesh.indices.size()==3u*FernTriangleCounts[variant],"Real fern mesh unavailable");
            const auto source=loaded.mesh;NormalizeFernAsset(loaded.mesh);meshes[variant]=std::move(loaded.mesh);
            assets[variant]=FernColonyAssetFromMesh(meshes[variant]);
            Require(std::abs(assets[variant].radius-expectedRadius[variant])<1e-6f&&
                std::abs(assets[variant].height-expectedHeight[variant])<1e-6f&&
                std::abs(assets[variant].minimumY-expectedMinimumY[variant])<1e-6f,"Real fern bounds changed");
            Require(source.indices==meshes[variant].indices,"Fern topology modified");
            for(std::size_t i=0;i<source.vertices.size();++i) {
                const auto& a=source.vertices[i];const auto& b=meshes[variant].vertices[i];
                Require(b.x==a.x*FernAssetScale&&b.y==a.y*FernAssetScale&&b.z==a.z*FernAssetScale&&
                    b.normal==a.normal&&b.color==a.color&&b.u==a.u&&b.v==a.v,"Fern pivot, UV or material data modified");
            }
        }
        const FernColonySettings settings{};
        Require(settings.targetPlants==300&&settings.seed==0x43464e32u&&settings.maximumColonies==256&&
            settings.attemptsPerColony==240&&settings.rootSeparationRatio==.65f,"Authored colony defaults changed");
        const auto population=CreateFernColonies(terrain,grass,original.ferns,solids,assets,settings);
        Require(population.reachedBudget&&population.plants.size()==300&&population.colonies.size()==(useMiddle?86u:84u),"300 authored fern colonies incomplete");
        const auto repeat=CreateFernColonies(terrain,grass,original.ferns,solids,assets,settings);
        Require(SamePopulation(population,repeat),"Same seed full transform determinism failed");
        auto alternate=settings;alternate.seed^=0x11111111u;
        Require(!SamePopulation(population,CreateFernColonies(terrain,grass,original.ferns,solids,assets,alternate)),"Different seed did not change population");
        const auto ratios=CheckPopulation(population,grass,original,solids,terrain,assets,meshes);
        auto larger=settings;larger.targetPlants=500;
        const auto shortfall=CreateFernColonies(terrain,grass,original.ferns,solids,assets,larger);
        Require(!shortfall.reachedBudget&&shortfall.plants.size()==(useMiddle?459u:498u)&&
            shortfall.colonies.size()==(useMiddle?129u:140u),"River-bank 500 shortfall was hidden or changed");
        (void)CheckPopulation(shortfall,grass,original,solids,terrain,assets,meshes);
        for(std::size_t i=0;i<population.plants.size();++i)Require(SamePlant(population.plants[i],shortfall.plants[i]),"Budget candidate prefix changed");
        auto zero=settings;zero.targetPlants=0;
        const auto empty=CreateFernColonies(terrain,grass,original.ferns,solids,assets,zero);
        Require(empty.reachedBudget&&empty.plants.empty()&&empty.colonies.empty(),"Zero target failed");
        const ForestCoverResult noAnchors{};
        const auto noPopulation=CreateFernColonies(terrain,noAnchors,original.ferns,solids,assets,settings);
        Require(!noPopulation.reachedBudget&&noPopulation.plants.empty(),"Empty anchors caused unsafe fallback");
        auto starved=settings;starved.maximumColonies=1;starved.attemptsPerColony=1;
        const auto scarce=CreateFernColonies(terrain,grass,original.ferns,solids,assets,starved);
        Require(!scarce.reachedBudget&&scarce.plants.size()<=1,"Starved budget caused unsafe fallback");
        const float nan=std::numeric_limits<float>::quiet_NaN(),infinity=std::numeric_limits<float>::infinity();
        for(const float value:{-.1f,0.f,.64f,2.1f,nan,infinity}) {
            auto invalid=settings;invalid.rootSeparationRatio=value;
            MustReject([&]{(void)CreateFernColonies(terrain,grass,original.ferns,solids,assets,invalid);});
        }
        for(const unsigned value:{0u,1001u}) {
            auto invalid=settings;invalid.maximumColonies=value;
            MustReject([&]{(void)CreateFernColonies(terrain,grass,original.ferns,solids,assets,invalid);});
        }
        for(const unsigned value:{0u,10001u}) {
            auto invalid=settings;invalid.attemptsPerColony=value;
            MustReject([&]{(void)CreateFernColonies(terrain,grass,original.ferns,solids,assets,invalid);});
        }
        auto tooMany=settings;tooMany.targetPlants=1001;
        MustReject([&]{(void)CreateFernColonies(terrain,grass,original.ferns,solids,assets,tooMany);});
        for(unsigned variant=0;variant<4;++variant)for(const float value:{-.1f,0.f,4.1f,nan,infinity}) {
            auto invalid=assets;invalid[variant].radius=value;
            MustReject([&]{(void)CreateFernColonies(terrain,grass,original.ferns,solids,invalid);});
            invalid=assets;invalid[variant].height=value;
            MustReject([&]{(void)CreateFernColonies(terrain,grass,original.ferns,solids,invalid);});
        }
        for(const float value:{-4.1f,1.f,nan,infinity}) {
            auto invalid=assets;invalid[0].minimumY=value;
            MustReject([&]{(void)CreateFernColonies(terrain,grass,original.ferns,solids,invalid);});
        }
        auto invalidMesh=meshes[0];invalidMesh.vertices[0].y=nan;
        MustReject([&]{(void)FernColonyAssetFromMesh(invalidMesh);});
        invalidMesh=meshes[0];invalidMesh.indices[0]=unsigned(invalidMesh.vertices.size());
        MustReject([&]{(void)FernColonyAssetFromMesh(invalidMesh);});
        invalidMesh={};MustReject([&]{(void)FernColonyAssetFromMesh(invalidMesh);});
        auto invalidOriginal=original.ferns;invalidOriginal[0].position.x=nan;
        MustReject([&]{(void)CreateFernColonies(terrain,grass,invalidOriginal,solids,assets);});
        invalidOriginal=original.ferns;invalidOriginal[0].scale.x=-1;
        MustReject([&]{(void)CreateFernColonies(terrain,grass,invalidOriginal,solids,assets);});
        invalidOriginal=original.ferns;invalidOriginal[0].rotation.x=.1f;
        MustReject([&]{(void)CreateFernColonies(terrain,grass,invalidOriginal,solids,assets);});
        invalidOriginal=original.ferns;invalidOriginal[0].position.x=-1000;
        MustReject([&]{(void)CreateFernColonies(terrain,grass,invalidOriginal,solids,assets);});
        invalidOriginal=original.ferns;invalidOriginal.resize(10001,original.ferns[0]);
        MustReject([&]{(void)CreateFernColonies(terrain,grass,invalidOriginal,solids,assets);});
        auto invalidGrass=grass;invalidGrass.patches[0].radius=nan;
        MustReject([&]{(void)CreateFernColonies(terrain,invalidGrass,original.ferns,solids,assets);});
        invalidGrass=grass;invalidGrass.plants[0].radius=-1;
        MustReject([&]{(void)CreateFernColonies(terrain,invalidGrass,original.ferns,solids,assets);});
        invalidGrass=grass;invalidGrass.plants[0].transform.scale.x=nan;
        MustReject([&]{(void)CreateFernColonies(terrain,invalidGrass,original.ferns,solids,assets);});
        invalidGrass=grass;invalidGrass.plants[0].radius=4.1f;
        MustReject([&]{(void)CreateFernColonies(terrain,invalidGrass,original.ferns,solids,assets);});
        invalidGrass=grass;invalidGrass.plants[0].transform.position.y=10001;
        MustReject([&]{(void)CreateFernColonies(terrain,invalidGrass,original.ferns,solids,assets);});
        invalidGrass=grass;invalidGrass.patches[0].centre.z=1000;
        MustReject([&]{(void)CreateFernColonies(terrain,invalidGrass,original.ferns,solids,assets);});
        invalidGrass=grass;invalidGrass.patches.resize(1001,grass.patches[0]);
        MustReject([&]{(void)CreateFernColonies(terrain,invalidGrass,original.ferns,solids,assets);});
        invalidGrass=grass;invalidGrass.plants.resize(10001,grass.plants[0]);
        MustReject([&]{(void)CreateFernColonies(terrain,invalidGrass,original.ferns,solids,assets);});
        auto invalidSolids=solids;invalidSolids[0].extents.x=nan;
        MustReject([&]{(void)CreateFernColonies(terrain,grass,original.ferns,invalidSolids,assets);});
        invalidSolids=solids;invalidSolids[0].extents.z=-1;
        MustReject([&]{(void)CreateFernColonies(terrain,grass,original.ferns,invalidSolids,assets);});
        invalidSolids=solids;invalidSolids.resize(10001,solids[0]);
        MustReject([&]{(void)CreateFernColonies(terrain,grass,original.ferns,invalidSolids,assets);});
        Require(!FernColonyClearsExisting({nan,0,0},.5f,original.ferns,assets)&&
            !FernColonyClearsExisting({},-1,original.ferns,assets)&&
            !FernColonyClearsExisting({},4.1f,original.ferns,assets),"Invalid helper root or radius accepted");
        const ForestCoverSettings footprintSettings{};
        const Vec3 edge{-49.99f,terrain.SampleHeight(-49.99f,0),0};
        Require(!ForestCoverFootprint(terrain,edge,assets[0].radius*.6f,footprintSettings),"Boundary footprint accepted");
        Require(before==WorldFingerprint(trees,original,broadleaf,solids,grass),"Original world mutated");
        std::cout<<"MistwatchFernColoniesTest pass; middle="<<useMiddle<<" solids="<<solids.size()
            <<"; added300=300; target500="<<shortfall.plants.size()<<"; root ratios="<<
            ratios.original<<'/'<<ratios.grass<<'/'<<ratios.added<<"; original world unchanged; GPU visual unverified\n";
        return 0;
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
