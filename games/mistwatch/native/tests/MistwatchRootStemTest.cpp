#include "MistwatchCanopyPopulation.h"
#include "MistwatchRootStemTerrain.h"
#include "MistwatchMiddleLayer.h"
#include <Aevrynd/MeshLoader.h>
#include <iostream>
using namespace Aevrynd;using namespace Mistwatch;
void Require(bool value,const char* label){if(!value)throw std::runtime_error(label);}
int main(){try{
    const auto assets=std::filesystem::path(MISTWATCH_SOURCE_ROOT)/"engine/assets";
    const auto terrain=CreateTerrain();const auto trees=TreePositions();const auto props=ForestProps(terrain,trees);
    const auto understory=CreateUnderstory(terrain,trees,props);const auto broadleaf=CreateBroadleafTrees(terrain,trees,props,understory);
    auto solids=WorldSolids(false);
    std::array<FernColonyAsset,4> fernAssets;
    for(unsigned i=0;i<4;++i){auto loaded=LoadAevryndMeshFile(assets/"mistwatch-fern-02/meshes"/FernMeshName(i));Require(bool(loaded),"Original fern missing");NormalizeFernAsset(loaded.mesh);fernAssets[i]=FernColonyAssetFromMesh(loaded.mesh);}
    const std::array<float,4> radii{fernAssets[0].radius,fernAssets[1].radius,fernAssets[2].radius,fernAssets[3].radius};
    const auto middle=CreateMiddleLayer(terrain,trees,props,understory,broadleaf,solids,24,radii);
    Require(middle.reachedBudget,"Original middle layer missing");for(const auto& p:middle.trees)solids.push_back(p.trunk);
    std::array<ForestCoverAsset,5> grassAssets;
    for(unsigned i=0;i<5;++i){auto loaded=LoadAevryndMeshFile(assets/"mistwatch-grass-medium-02/meshes"/GrassMeshName(i));Require(bool(loaded),"Original grass missing");NormalizeGrassAsset(loaded.mesh);grassAssets[i]=ForestCoverAssetFromMesh(loaded.mesh);}
    const auto grass=CreateForestCover(terrain,trees,understory,solids,grassAssets);Require(grass.reachedBudget,"Original grass incomplete");
    const auto colonies=CreateFernColonies(terrain,grass,understory.ferns,solids,fernAssets);Require(colonies.reachedBudget,"Original colonies incomplete");
    const auto leaf=LoadAevryndMeshFile(assets/"realistic-cc0-first-batch/cooked/meshes/tree-small-02"/BroadleafMeshName(1));Require(bool(leaf),"Shrub source missing");
    const auto shrubs=CreateShrubCanopy(leaf.mesh,terrain,solids,understory,grass,colonies,fernAssets);Require(shrubs.reachedBudget,"Original shrubs incomplete");
    const auto directory=assets/"realistic-cc0-first-batch/cooked/meshes/island-tree-01";
    constexpr const char* names[]{"island-tree-01-lod2-node0-mesh0-primitive0-material0-island-tree-01.aemesh",
        "island-tree-01-lod1-node0-mesh0-primitive1-material1-island-tree-01-leaves.aemesh",
        "island-tree-01-lod2-node0-mesh0-primitive2-material2-island-tree-01-branches.aemesh"};
    std::array<Mesh3D,3> meshes;for(unsigned i=0;i<3;++i){auto loaded=LoadAevryndMeshFile(directory/names[i]);Require(bool(loaded),"Canopy source missing");meshes[i]=std::move(loaded.mesh);}
    const auto geometry=MeasureCanopy(meshes);Require(geometry.triangles==606094,"True canopy triangles changed");
    std::cout<<"geometry_radius="<<geometry.radius<<" root_radius="<<geometry.rootRadius<<" lower_faces="<<geometry.lowerFaces<<" cross_edges="<<geometry.crossingEdges<<'\n';
    // 保留baseline原算法與原人口作對照；新構圖先配置有碰撞的完整樹，再依同門檻重新散布裝飾。
    const auto result=CreateCanopyPopulation(meshes,geometry,terrain,solids,understory,{}, {},fernAssets,{});
    std::cout<<"CANOPY_POPULATION plants="<<result.plants.size()<<" attempts="<<result.attempts<<" clearance_rejected="<<result.clearanceRejected<<" root_rejected="<<result.rootRejected<<" spacing_rejected="<<result.spacingRejected<<" mesh_rejected="<<result.meshRejected<<" ground_checks="<<result.groundChecks<<" fingerprint="<<result.fingerprint<<'\n';
    for(const auto& p:result.plants)std::cout<<"root="<<p.pose.position.x<<','<<p.pose.position.y<<','<<p.pose.position.z<<" yaw="<<p.pose.rotation.y<<" scale="<<p.pose.scale.x<<'\n';
    Require(result.reachedBudget,"Complete canopy population shortfall");
    unsigned startCount{};for(const auto& p:result.plants)startCount+=p.pose.position.z<26.f&&std::abs(p.pose.position.x)<26.f;
    std::cout<<"visible_start_candidates="<<startCount<<'\n';Require(startCount>=2,"Canopy population does not improve visible start region");
    for(std::size_t plantIndex=0;plantIndex<result.plants.size();++plantIndex){
        const auto& plant=result.plants[plantIndex];
        Require(PropClearsObjectives(plant.lower),"Complete physics envelope overlaps objective");
        for(const auto& existing:solids)
            Require(std::abs(existing.center.x-plant.lower.center.x)>=existing.extents.x+plant.lower.extents.x||
                std::abs(existing.center.z-plant.lower.center.z)>=existing.extents.z+plant.lower.extents.z,
                "Complete physics envelope overlaps original solid in XZ");
        for(std::size_t j=0;j<plantIndex;++j){const auto& b=result.plants[j].lower;
            Require(std::abs(b.center.x-plant.lower.center.x)>=b.extents.x+plant.lower.extents.x||
                std::abs(b.center.z-plant.lower.center.z)>=b.extents.z+plant.lower.extents.z,
                "Complete canopy physics envelopes overlap");
        }
        Require(plant.rootedTrunk.indices==meshes[0].indices,"Root adaptation changed source topology");
        unsigned changed{};
        for(std::size_t i=0;i<meshes[0].vertices.size();++i){const auto& a=meshes[0].vertices[i];const auto& b=plant.rootedTrunk.vertices[i];
            Require(a.x==b.x&&a.z==b.z&&a.u==b.u&&a.v==b.v&&a.color==b.color,"Root adaptation changed UV/color/horizontal footprint");
            if(a.y-geometry.rootMinimum>=.35f)Require(a.y==b.y&&a.normal==b.normal,"Upper trunk changed");
            changed+=a.y!=b.y;
        }
        Require(changed>0,"No actual root terrain adaptation");
        const auto matrix=plant.pose.Matrix();
        for(unsigned slot=0;slot<3;++slot){const auto& mesh=slot==0?plant.rootedTrunk:meshes[slot];
            for(const auto& v:mesh.vertices){const auto world=TransformPoint(matrix,{v.x,v.y,v.z});
                const auto delta=world-plant.lower.center;
                Require(std::abs(delta.x)<=plant.lower.extents.x+1.e-4f&&std::abs(delta.y)<=plant.lower.extents.y+1.e-4f&&
                    std::abs(delta.z)<=plant.lower.extents.z+1.e-4f,"Complete tree escapes collision envelope");
            }
        }
    }
    Require(grass.plants.size()==3600&&colonies.plants.size()==300&&understory.ferns.size()==420&&shrubs.plants.size()==64&&solids.size()==245,"Original population changed");
    auto nextSolids=solids;for(const auto& p:result.plants)nextSolids.push_back(p.lower);
    const auto nextGrass=CreateForestCover(terrain,trees,understory,nextSolids,grassAssets);Require(nextGrass.reachedBudget,"New grass population shortfall");
    const auto nextColonies=CreateFernColonies(terrain,nextGrass,understory.ferns,nextSolids,fernAssets);Require(nextColonies.reachedBudget,"New fern population shortfall");
    const auto nextShrubs=CreateShrubCanopy(leaf.mesh,terrain,nextSolids,understory,nextGrass,nextColonies,fernAssets);Require(nextShrubs.reachedBudget,"New shrub population shortfall");
    Require(nextGrass.plants.size()==3600&&nextColonies.plants.size()==300&&nextShrubs.plants.size()==64&&nextSolids.size()==249,"New authored world count mismatch");
    std::cout<<"NEW_WORLD_COUNTS grass="<<nextGrass.plants.size()<<" colonies="<<nextColonies.plants.size()<<" shrubs="<<nextShrubs.plants.size()<<" colliders="<<nextSolids.size()<<" shrub_fingerprint="<<nextShrubs.fingerprint<<'\n';
    // 另以CPU作者資料重算原生收據的全世界指紋，不從本次收據讀回期望值。
    const auto mix=[](std::uint64_t& hash,std::uint32_t bits){
        for(unsigned byte=0;byte<4;++byte){hash^=(bits>>(byte*8))&255u;hash*=1099511628211ull;}
    };
    std::uint64_t grassHash=14695981039346656037ull,solidHash=grassHash,fernHash=grassHash;
    std::array<unsigned,5> grassKinds{};std::array<unsigned,4> fernKinds{};
    for(const auto& p:nextGrass.plants){
        const auto root=p.transform.position;
        for(float value:{root.x,root.y,root.z,ForestCoverScale(root),root.x+root.z})mix(grassHash,std::bit_cast<std::uint32_t>(value));
        mix(grassHash,p.variant);++grassKinds[p.variant];
    }
    for(const auto& b:nextSolids)for(float value:{b.center.x,b.center.y,b.center.z,b.extents.x,b.extents.y,b.extents.z})mix(solidHash,std::bit_cast<std::uint32_t>(value));
    for(const auto& p:nextColonies.plants){
        const auto t=p.transform;
        for(float value:{t.position.x,t.position.y,t.position.z,t.rotation.x,t.rotation.y,t.rotation.z,t.scale.x,t.scale.y,t.scale.z})mix(fernHash,std::bit_cast<std::uint32_t>(value));
        mix(fernHash,p.variant);++fernKinds[p.variant];
    }
    Require(result.fingerprint==13093722598475582899ull&&nextShrubs.fingerprint==5849093981174083644ull,"Canopy/shrub profile identity mismatch");
    std::cout<<"grass_variants=";for(auto n:grassKinds)std::cout<<n<<',';std::cout<<" fern_variants=";for(auto n:fernKinds)std::cout<<n<<',';std::cout<<'\n';
    std::cout<<"INDEPENDENT_WORLD_IDENTITY grass="<<grassHash<<" solids="<<solidHash<<" fern="<<fernHash<<'\n';
    auto corrupt=geometry;corrupt.maximum.x+=1;
    bool rejected=false;try{(void)CreateCanopyPopulation(meshes,corrupt,terrain,solids,understory,{}, {},fernAssets,{},0);}catch(const std::invalid_argument&){rejected=true;}
    Require(rejected,"Stale complete envelope accepted");
    corrupt=geometry;corrupt.rootMinimum=std::numeric_limits<float>::quiet_NaN();
    rejected=false;try{(void)CreateCanopyPopulation(meshes,corrupt,terrain,solids,understory,{}, {},fernAssets,{},0);}catch(const std::invalid_argument&){rejected=true;}
    Require(rejected,"Nonfinite root envelope accepted");
    const auto repeat=CreateCanopyPopulation(meshes,geometry,terrain,solids,understory,{}, {},fernAssets,{});
    Require(repeat.fingerprint==result.fingerprint&&repeat.plants.size()==4,"Nondeterministic canopy author");
    const auto stem=LoadAevryndMeshFile(MISTWATCH_DERIVED_STEM);
    Require(bool(stem)&&stem.mesh.vertices.size()==3608&&stem.mesh.indices.size()/3==5330,"Derived stem engine load failed");
    Require(*std::min_element(stem.mesh.indices.begin(),stem.mesh.indices.end())<stem.mesh.vertices.size(),"Derived stem invalid index");
    unsigned capFaces{},exactIntersections{},exactPoints{},expandedColliders{};float capMin=10000,capMax=-10000,lowMin=10000;
    auto stemSolids=solids;
    for(const auto& plant:result.plants){
        const auto rooted=PlantDerivedRootStem(stem.mesh,geometry,plant.pose,terrain);
        const auto matrix=plant.pose.Matrix();
        const auto collider=ExtendRootCollision(plant.lower,rooted,plant.pose);
        const auto oldMin=plant.lower.center-plant.lower.extents,oldMax=plant.lower.center+plant.lower.extents;
        const auto newMin=collider.center-collider.extents,newMax=collider.center+collider.extents;
        Require(newMin.x<=oldMin.x+1.e-5f&&newMin.y<=oldMin.y+1.e-5f&&newMin.z<=oldMin.z+1.e-5f&&newMax.x>=oldMax.x-1.e-5f&&newMax.y>=oldMax.y-1.e-5f&&newMax.z>=oldMax.z-1.e-5f,"Root collision shrank original envelope");
        Require(collider.center.x==plant.lower.center.x&&collider.center.z==plant.lower.center.z&&collider.extents.x==plant.lower.extents.x&&collider.extents.z==plant.lower.extents.z,"Root extension changed horizontal world clearance");
        expandedColliders+=newMin.y<oldMin.y;
        Require(PropClearsObjectives(collider),"Derived expanded collision overlaps objective");
        stemSolids.push_back(collider);
        Require(rooted.indices==stem.mesh.indices,"Stem terrain changed derived topology");
        for(std::size_t i=0;i<rooted.vertices.size();++i){const auto& a=stem.mesh.vertices[i];const auto& b=rooted.vertices[i];
            Require(a.x==b.x&&a.z==b.z&&a.u==b.u&&a.v==b.v&&a.color==b.color,"Derived rooting changed horizontal/UV/color");
            if(a.y-geometry.rootMinimum>=.35f)Require(a.y==b.y&&a.normal==b.normal,"Derived upper stem changed");
            const auto world=TransformPoint(matrix,{b.x,b.y,b.z}),delta=world-collider.center;
            Require(std::abs(delta.x)<=collider.extents.x+1.e-4f&&std::abs(delta.y)<=collider.extents.y+1.e-4f&&std::abs(delta.z)<=collider.extents.z+1.e-4f,"Derived stem exceeds expanded complete tree envelope");
        }
        for(std::size_t face=0;face<rooted.indices.size();face+=3){
            bool low=false;for(unsigned j=0;j<3;++j)low|=stem.mesh.vertices[rooted.indices[face+j]].y-geometry.rootMinimum<.35f;
            if(!low)continue;
            std::array<Vec3,3> points;
            for(unsigned j=0;j<3;++j){const auto& v=rooted.vertices[rooted.indices[face+j]];points[j]=TransformPoint(matrix,{v.x,v.y,v.z});}
            const auto overlay=RootGroundOverlay(points,terrain);lowMin=(std::min)(lowMin,overlay.minimum);
            exactIntersections+=overlay.intersections;exactPoints+=overlay.points;
            Require(overlay.minimum>=-.04f,"Derived root violates original lower penetration gate");
            const bool cap=face/3>=5104+152;
            if(cap){++capFaces;capMin=(std::min)(capMin,overlay.minimum);capMax=(std::max)(capMax,overlay.maximum);
                Require(overlay.maximum<=-.005f,"Root cap not fully below actual terrain triangles");
                Require(Cross(points[1]-points[0],points[2]-points[0]).y<0,"Root cap not down facing");
            }
        }
    }
    Require(capFaces==4*74,"Complete cap face population mismatch");
    Require(expandedColliders>0,"Root expansion did not repair observed envelope divergence");
    const auto stemGrass=CreateForestCover(terrain,trees,understory,stemSolids,grassAssets);
    const auto stemColonies=CreateFernColonies(terrain,stemGrass,understory.ferns,stemSolids,fernAssets);
    const auto stemShrubs=CreateShrubCanopy(leaf.mesh,terrain,stemSolids,understory,stemGrass,stemColonies,fernAssets);
    const auto sameVector=[](Vec3 a,Vec3 b){return a.x==b.x&&a.y==b.y&&a.z==b.z;};
    const auto samePose=[&](const Transform3D& a,const Transform3D& b){return sameVector(a.position,b.position)&&sameVector(a.rotation,b.rotation)&&sameVector(a.scale,b.scale);};
    Require(stemGrass.plants.size()==nextGrass.plants.size()&&stemColonies.plants.size()==nextColonies.plants.size()&&stemShrubs.fingerprint==nextShrubs.fingerprint,"Expanded root collision changed decoration count");
    for(std::size_t i=0;i<stemGrass.plants.size();++i){const auto& a=stemGrass.plants[i];const auto& b=nextGrass.plants[i];Require(samePose(a.transform,b.transform)&&a.variant==b.variant&&a.radius==b.radius,"Expanded root collision changed grass placement");}
    for(std::size_t i=0;i<stemColonies.plants.size();++i){const auto& a=stemColonies.plants[i];const auto& b=nextColonies.plants[i];Require(samePose(a.transform,b.transform)&&a.variant==b.variant&&a.radius==b.radius,"Expanded root collision changed fern placement");}
    std::uint64_t stemSolidHash=14695981039346656037ull;
    for(const auto& b:stemSolids)for(float value:{b.center.x,b.center.y,b.center.z,b.extents.x,b.extents.y,b.extents.z})mix(stemSolidHash,std::bit_cast<std::uint32_t>(value));
    // 頂點全在地形上方仍可能中段穿地：真交集法必須抓到格點山峰。
    HeightField3D hostile({{0,0,0},3,3,1.f});hostile.Fill(0);hostile.SetHeight(1,1,1.f);
    const auto hostileRange=RootGroundOverlay({{{0,.1f,0},{2,.1f,0},{1,.1f,2}}},hostile);
    Require(hostileRange.minimum<-.8f,"Exact overlay missed interior terrain peak");
    std::cout<<"ROOT_STEM_CPU_PASS trees="<<result.plants.size()<<" unchanged_pose_fingerprint="<<result.fingerprint
        <<" source_trunk_triangles=5217 derived_trunk_triangles=5330 cap_faces="<<capFaces
        <<" exact_intersections="<<exactIntersections<<" exact_points="<<exactPoints<<" root_min_gap="<<lowMin
        <<" cap_min_gap="<<capMin<<" cap_max_gap="<<capMax<<'\n';
    std::cout<<"ROOT_COLLISION_EXPANSION count="<<expandedColliders<<" new_solids_fingerprint="<<stemSolidHash<<" canopy_total_triangles=606207 preserved_original_collision_not_shrunk\n";
    std::cout<<"CANOPY_CPU_PASS\n";return 0;
}catch(const std::exception& error){std::cerr<<"CANOPY_CPU_FIRST_DIVERGENCE "<<error.what()<<'\n';return 1;}}
