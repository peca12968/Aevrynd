#include "MistwatchForestCover.h"
#include <Aevrynd/MeshLoader.h>
#include <bit>
#include <iostream>
#include <numeric>
#include <filesystem>

namespace {
using namespace Aevrynd;
using namespace Mistwatch;
void Require(bool value,const char* message) {if(!value)throw std::runtime_error(message);}
std::uint64_t Hash(std::span<const ForestCoverPlant> plants) {
    std::uint64_t hash=1469598103934665603ull;
    const auto mix=[&](std::uint32_t value){for(unsigned byte=0;byte<4;++byte){hash^=(value>>(byte*8))&255u;hash*=1099511628211ull;}};
    for(const auto& plant:plants) {
        for(float value:{plant.transform.position.x,plant.transform.position.y,plant.transform.position.z,
            plant.transform.rotation.y,plant.transform.scale.x,plant.radius})mix(std::bit_cast<std::uint32_t>(value));
        mix(plant.variant);mix(plant.patch);
    }
    return hash;
}
struct Statistics {
    std::size_t count{},isolated{},clearanceRejected{},terrainRejected{};
    float nearestP50{},nearestP90{},nearestMean{},maximumTriangleRootError{};
    double cellVarianceToMean{};
    std::array<unsigned,5> variants{};
    std::array<unsigned,2> selected{},triangles{};
    unsigned initialShoulderRoots{},riverDryBankRoots{};
};
float MeshTriangleHeight(const HeightField3D& terrain,const Mesh3D& mesh,Vec3 point) {
    const auto& grid=terrain.Settings();
    const unsigned x=unsigned(std::floor((point.x-grid.origin.x)/grid.cellSize));
    const unsigned z=unsigned(std::floor((point.z-grid.origin.z)/grid.cellSize));
    const auto offset=(std::size_t(z)*(grid.width-1)+x)*6;
    for(unsigned triangle=0;triangle<2;++triangle) {
        const auto& a=mesh.vertices[mesh.indices[offset+triangle*3]];
        const auto& b=mesh.vertices[mesh.indices[offset+triangle*3+1]];
        const auto& c=mesh.vertices[mesh.indices[offset+triangle*3+2]];
        const float denominator=(b.z-c.z)*(a.x-c.x)+(c.x-b.x)*(a.z-c.z);
        const float u=((b.z-c.z)*(point.x-c.x)+(c.x-b.x)*(point.z-c.z))/denominator;
        const float v=((c.z-a.z)*(point.x-c.x)+(a.x-c.x)*(point.z-c.z))/denominator;
        if(u>=-.0001f&&v>=-.0001f&&u+v<=1.0001f)return u*a.y+v*b.y+(1.f-u-v)*c.y;
    }
    throw std::runtime_error("Root not contained in actual ground triangle");
}
// 獨立直接驗證原任務點與通路範圍，不只重跑原型接受函式。
void IndependentObjectives(Vec3 root,float radius) {
    const auto clearsCircle=[&](Vec3 point,float minimum) {
        const float dx=(std::max)(0.f,std::abs(point.x-root.x)-radius);
        const float dz=(std::max)(0.f,std::abs(point.z-root.z)-radius);
        Require(std::hypot(dx,dz)>=minimum-.0001f,"Objective footprint clearance failed");
    };
    const auto clearsBranch=[&](Vec3 point,float zMargin,float xMargin) {
        const bool hitsZ=root.z-radius<point.z+zMargin&&root.z+radius>point.z-zMargin;
        const bool hitsX=root.x-radius<(std::max)(0.f,point.x)+xMargin&&
            root.x+radius>(std::min)(0.f,point.x)-xMargin;
        Require(!(hitsZ&&hitsX),"Objective approach corridor obstructed");
    };
    Require(root.x+radius<=-4.5f||root.x-radius>=4.5f,"Main route footprint obstructed");
    Require(root.z+radius<=29.f||root.z-radius>=43.f,"Existing bridge strip narrowed");
    for(const auto p:Session::supplies){clearsCircle(p,3.f);clearsBranch(p,2.f,2.f);}
    for(const auto p:Session::repairSites){clearsCircle(p,4.f);clearsBranch(p,1.5f,1.f);}
    for(const auto p:Session::relaySites){clearsCircle(p,4.f);clearsBranch(p,1.8f,1.f);}
    clearsCircle(Session::arena,7.f);
    for(const auto p:{Session::fuse,Session::generator,Session::beacon,Session::rangerLog})clearsCircle(p,5.f);
    for(unsigned sample=0;sample<=24;++sample) {
        const float x=root.x-radius+2.f*radius*float(sample)/24.f;
        Require(std::abs(root.z-RiverCenterZ(x))-radius>=RiverOuterWidth(x)+2.f-.0001f,
            "Dry river margin footprint failed");
    }
}
Statistics Measure(std::span<const ForestCoverPlant> plants,const HeightField3D& terrain,
    const Mesh3D& terrainMesh,std::span<const BoxCollider3D> solids,
    const std::array<ForestCoverAsset,5>& assets) {
    Statistics stats;stats.count=plants.size();
    std::vector<float> nearest;nearest.reserve(plants.size());
    std::array<unsigned,32*49> cells{};
    const std::array<Vec3,2> feet{{{0,0,0},{0,0,30}}};
    for(std::size_t index=0;index<plants.size();++index) {
        const auto& plant=plants[index];const auto p=plant.transform.position;
        float distance=10000.f;
        for(std::size_t other=0;other<plants.size();++other)if(index!=other)
            distance=(std::min)(distance,std::hypot(p.x-plants[other].transform.position.x,p.z-plants[other].transform.position.z));
        nearest.push_back(distance);stats.isolated+=distance>1.f;
        const auto cellX=(std::min)(unsigned((p.x+32.f)/2.f),31u);
        const auto cellZ=(std::min)(unsigned((p.z+10.f)/2.f),48u);
        ++cells[cellZ*32+cellX];++stats.variants[plant.variant];
        stats.clearanceRejected+=!ForestCoverClearance(p,plant.radius,solids);
        stats.terrainRejected+=!ForestCoverFootprint(terrain,p,plant.radius,{});
        stats.maximumTriangleRootError=(std::max)(stats.maximumTriangleRootError,
            std::abs(p.y-MeshTriangleHeight(terrain,terrainMesh,p)));
        for(unsigned scene=0;scene<2;++scene)if(GrassRootSelected(p,feet[scene])) {
            ++stats.selected[scene];stats.triangles[scene]+=GrassTriangleCounts[plant.variant];
        }
        stats.initialShoulderRoots+=p.z>=0&&p.z<=26&&std::abs(p.x)>=4.5f&&std::abs(p.x)<=16.f;
        stats.riverDryBankRoots+=p.x<=-8.f&&p.x>=-24.f&&p.z>=20.f&&p.z<=52.f;
        Require(std::abs(plant.radius-assets[plant.variant].radius*plant.transform.scale.x)<.00001f,
            "Stored radius differs from actual normalized asset");
    }
    std::sort(nearest.begin(),nearest.end());
    if(!nearest.empty()) {
        stats.nearestP50=nearest[nearest.size()/2];stats.nearestP90=nearest[nearest.size()*9/10];
        stats.nearestMean=std::accumulate(nearest.begin(),nearest.end(),0.f)/float(nearest.size());
    }
    const double mean=double(plants.size())/double(cells.size());double variance{};
    for(const unsigned cell:cells)variance+=(double(cell)-mean)*(double(cell)-mean);
    stats.cellVarianceToMean=variance/double(cells.size())/mean;
    return stats;
}
}
int main(int argc,char** argv) {
    try {
        Require(argc==2,"Expected actual copied grass mesh directory");
        const auto meshRoot=std::filesystem::path(argv[1]);
        const auto terrain=CreateTerrain();const auto terrainMesh=CreateTerrainMesh(terrain);
        const auto trees=TreePositions();const auto props=ForestProps(terrain,trees);
        const auto understory=CreateUnderstory(terrain,trees,props);
        const auto broadleaf=CreateBroadleafTrees(terrain,trees,props,understory);
        const auto solids=WorldSolids(false),repairedSolids=WorldSolids(true);
        Require(trees.size()==100&&understory.saplings.size()==42&&understory.ferns.size()==420&&
            broadleaf.size()==12&&solids.size()==221,"Actual forest/collision fixture changed");
        const auto oldRoots=GroundCoverPositions(terrain);
        Require(oldRoots.size()==1363,"Actual scatter changed; refresh comparison before accepting prototype");
        for(const auto root:oldRoots) {
            Require(std::abs(root.x)>=1.8f&&SettlementTrailDistance(root.x,root.z)>=.7f&&
                RiverDistance(root.x,root.z)>=RiverOuterWidth(root.x)+1.5f,"Original root contract changed");
            for(const auto p:Session::repairSites)Require(std::hypot(root.x-p.x,root.z-p.z)>=3.f,"Original repair exclusion changed");
        }
        std::array<ForestCoverAsset,5> assets{};std::array<Mesh3D,5> meshes{};
        for(unsigned index=0;index<5;++index) {
            auto loaded=LoadAevryndMeshFile(meshRoot/GrassMeshName(index));
            Require(bool(loaded),"Actual grass mesh load failed");
            Require(loaded.mesh.vertices.size()==GrassVertexCounts[index]&&
                loaded.mesh.indices.size()==std::size_t(GrassTriangleCounts[index])*3,
                "Actual five-variant grass topology changed");
            NormalizeGrassAsset(loaded.mesh);
            assets[index]=ForestCoverAssetFromMesh(loaded.mesh);meshes[index]=std::move(loaded.mesh);
        }
        std::vector<ForestCoverPlant> baseline;
        for(std::size_t index=0;index<oldRoots.size();++index) {
            const auto root=oldRoots[index];const auto variant=unsigned(index%5);const auto scale=ForestCoverScale(root);
            baseline.push_back({{root,{0,root.x+root.z,0},{scale,scale,scale}},variant,
                (std::numeric_limits<unsigned>::max)(),assets[variant].radius*scale});
        }
        ForestCoverSettings settings;settings.targetPlants=oldRoots.size();
        const auto clustered=CreateForestCover(terrain,trees,understory,solids,assets,settings);
        const auto repeat=CreateForestCover(terrain,trees,understory,solids,assets,settings);
        auto changedSettings=settings;changedSettings.seed+=1;
        const auto changedSeed=CreateForestCover(terrain,trees,understory,solids,assets,changedSettings);
        const ForestCoverSettings authoredSettings;
        const auto authored=CreateForestCover(terrain,trees,understory,solids,assets,authoredSettings);
        const auto authoredRepeat=CreateForestCover(terrain,trees,understory,solids,assets,authoredSettings);
        Require(authoredSettings.targetPlants==3600&&authored.reachedBudget&&authored.plants.size()==3600,
            "Production forest population incomplete");
        Require(Hash(authored.plants)==Hash(authoredRepeat.plants),"Production transforms changed with same seed");
        Require(Hash(clustered.plants)==Hash(repeat.plants),"Same seed changed plant transforms");
        Require(clustered.plants.size()==repeat.plants.size()&&clustered.patches.size()==repeat.patches.size(),
            "Same seed changed plant or patch counts");
        for(std::size_t index=0;index<clustered.plants.size();++index) {
            const auto a=clustered.plants[index].transform.Matrix(),b=repeat.plants[index].transform.Matrix();
            for(unsigned row=0;row<4;++row)for(unsigned column=0;column<4;++column)
                Require(a[row][column]==b[row][column],"Same seed changed full transform");
        }
        for(std::size_t index=0;index<clustered.patches.size();++index) {
            const auto& a=clustered.patches[index];const auto& b=repeat.patches[index];
            Require(Length(a.centre-b.centre)==0&&a.radius==b.radius&&a.aspect==b.aspect&&
                a.yaw==b.yaw&&a.requested==b.requested&&a.accepted==b.accepted,
                "Same seed changed patch shape or accepted density");
        }
        Require(Hash(clustered.plants)!=Hash(changedSeed.plants),"Different seed did not change layout");
        Require(clustered.reachedBudget,"Same budget prototype could not find safe density");
        const auto baselineStats=Measure(baseline,terrain,terrainMesh,solids,assets);
        const auto clusterStats=Measure(clustered.plants,terrain,terrainMesh,solids,assets);
        Require(clusterStats.maximumTriangleRootError<.00001f,"Triangle root test failed");
        Require(!clusterStats.clearanceRejected&&!clusterStats.terrainRejected,"Generated plant footprint test failed");
        Require(clusterStats.nearestP50<baselineStats.nearestP50*.75f,"Clustering nearest-neighbour improvement absent");
        Require(clusterStats.cellVarianceToMean>baselineStats.cellVarianceToMean*1.4,"Patch distribution not distinct from scatter");
        Require(clusterStats.isolated<baselineStats.isolated/2,"Isolated grass fraction not improved");
        for(const unsigned count:clusterStats.variants)Require(count>clustered.plants.size()/10,"A grass variant disappeared");
        float maximumVertexHeightDelta{},minimumFootprintNormalY=1.f;
        for(const auto& plant:authored.plants) {
            const auto root=plant.transform.position;IndependentObjectives(root,plant.radius);
            Require(ForestCoverClearance(root,plant.radius,solids)&&
                ForestCoverFootprint(terrain,root,plant.radius,authoredSettings),"Production footprint contract failed");
            Require(ForestCoverClearance(root,plant.radius,repairedSolids),"Repaired world footprint collision");
            const auto matrix=plant.transform.Matrix();
            for(const auto& vertex:meshes[plant.variant].vertices) {
                const auto world=TransformPoint(matrix,{vertex.x,vertex.y,vertex.z});
                Require(terrain.Contains(world.x,world.z),"Real model vertex outside core terrain");
                const float delta=std::abs(terrain.SampleHeight(world.x,world.z)-root.y);
                maximumVertexHeightDelta=(std::max)(maximumVertexHeightDelta,delta);
                minimumFootprintNormalY=(std::min)(minimumFootprintNormalY,terrain.SampleNormal(world.x,world.z).y);
                Require(delta<=settings.maximumFootprintHeightDelta+.00001f,"Actual rotated mesh footprint height failed");
                Require(terrain.SampleNormal(world.x,world.z).y>=settings.minimumNormalY-.00001f,"Actual mesh footprint slope failed");
            }
        }
        Require(GrassRootSelected({},{35,0,0})&&!GrassRootSelected({},{35.01f,0,0})&&
            !GrassRootSelected({},{34.99f,1,0}),"Existing grass 3D selection boundary failed");
        Require(!ForestCoverClearance({},.5f,solids)&&
            !ForestCoverClearance(Session::supplies[0],.5f,solids)&&
            !ForestCoverClearance({0,-8,36},.5f,solids),"Hostile objective/root inputs accepted");
        Require(!ForestCoverFootprint(terrain,{1000,0,1000},.5f,{})&&
            !ForestCoverFootprint(terrain,{},-1.f,{}),"Hostile footprint inputs accepted");
        auto invalid=settings;invalid.minimumNormalY=.7f;bool rejectedUnsafe=false;
        try{(void)CreateForestCover(terrain,trees,understory,solids,assets,invalid);}catch(const std::invalid_argument&){rejectedUnsafe=true;}
        Require(rejectedUnsafe,"Unsafe slope settings accepted");
        const auto rejectedSettings=[&](ForestCoverSettings candidate) {
            try{(void)CreateForestCover(terrain,trees,understory,solids,assets,candidate);}
            catch(const std::invalid_argument&){return true;}return false;
        };
        invalid=settings;invalid.maximumFootprintHeightDelta=std::numeric_limits<float>::quiet_NaN();
        Require(rejectedSettings(invalid),"NaN footprint height setting accepted");
        invalid=settings;invalid.rootSeparationRatio=std::numeric_limits<float>::quiet_NaN();
        Require(rejectedSettings(invalid),"NaN separation setting accepted");
        invalid=settings;invalid.targetPlants=10001;
        Require(rejectedSettings(invalid),"Unbounded allocation request accepted");
        invalid=settings;invalid.independentAnchors=2001;
        Require(rejectedSettings(invalid),"Unbounded anchor request accepted");
        invalid=settings;invalid.minimumPlantsPerPatch=0;
        Require(rejectedSettings(invalid),"Zero patch population accepted");
        invalid=settings;invalid.maximumPlantsPerPatch=129;
        Require(rejectedSettings(invalid),"Unbounded patch population accepted");
        invalid=settings;invalid.maximumPlantsPerPatch=invalid.minimumPlantsPerPatch-1;
        Require(rejectedSettings(invalid),"Reversed patch population accepted");
        auto corruptAssets=assets;corruptAssets[2].radius=std::numeric_limits<float>::quiet_NaN();
        bool rejectedAsset=false;
        try{(void)CreateForestCover(terrain,trees,understory,solids,corruptAssets,settings);}
        catch(const std::invalid_argument&){rejectedAsset=true;}
        Require(rejectedAsset,"NaN actual asset radius accepted");
        corruptAssets=assets;corruptAssets[4].height=std::numeric_limits<float>::quiet_NaN();rejectedAsset=false;
        try{(void)CreateForestCover(terrain,trees,understory,solids,corruptAssets,settings);}
        catch(const std::invalid_argument&){rejectedAsset=true;}
        Require(rejectedAsset,"NaN actual asset height accepted");
        auto zeroSettings=settings;zeroSettings.targetPlants=0;
        const auto zero=CreateForestCover(terrain,trees,understory,solids,assets,zeroSettings);
        Require(zero.reachedBudget&&zero.plants.empty()&&zero.patches.empty(),"Zero budget contract failed");
        auto starvedSettings=settings;starvedSettings.maximumPatches=1;starvedSettings.attemptsPerPatch=1;
        const auto starved=CreateForestCover(terrain,trees,understory,solids,assets,starvedSettings);
        Require(!starved.reachedBudget&&starved.plants.size()<=1,"Budget shortfall was hidden by unsafe fallback");
        Require(!ForestCoverFootprint(terrain,{49.9f,terrain.SampleHeight(49.9f,0),0},.5f,{})&&
            ForestCoverFootprint(terrain,{0,0,-19.5f},.5f,{}),"Full footprint outside/touching core boundary counterexample failed");
        Require(!ForestCoverClearance({0,std::numeric_limits<float>::quiet_NaN(),0},.5f,solids)&&
            !ForestCoverFootprint(terrain,{},std::numeric_limits<float>::quiet_NaN(),{}),"NaN root or footprint radius accepted");
        // 採樣同一個實際世界的輸入在原型前後完全不變。
        const auto solidsAfter=WorldSolids(false);
        Require(solids.size()==solidsAfter.size(),"Prototype changed world collider count");
        for(std::size_t index=0;index<solids.size();++index)
            Require(Length(solids[index].center-solidsAfter[index].center)==0&&
                Length(solids[index].extents-solidsAfter[index].extents)==0,"Prototype moved a collidable object");
        Require(terrainMesh.indices==CreateTerrainMesh(terrain).indices,"Prototype changed ground topology");
        std::cout<<"MistwatchForestCover CPU PASS actual_scatter="<<baseline.size()<<" clustered="<<clustered.plants.size()<<
            " patches="<<clustered.patches.size()<<" nearest_p50="<<baselineStats.nearestP50<<"->"<<clusterStats.nearestP50<<
            " authored_population="<<authored.plants.size()<<" authored_patches="<<authored.patches.size()<<
            " isolated="<<baselineStats.isolated<<"->"<<clusterStats.isolated<<" cell_dispersion="<<baselineStats.cellVarianceToMean<<
            "->"<<clusterStats.cellVarianceToMean<<" unchanged_colliders="<<solids.size()<<
            " actual_footprint_height_delta="<<maximumVertexHeightDelta<<
            " minimum_footprint_normal_y="<<minimumFootprintNormalY<<" visual=UNVERIFIED GPU=NOT_RUN\n";
        return 0;
    }catch(const std::exception& error){std::cerr<<"FIRST_DIVERGENCE: "<<error.what()<<'\n';return 1;}
}
