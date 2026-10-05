#include "MistwatchShrubCanopy.h"
#include "MistwatchMiddleLayer.h"
#include <Aevrynd/MeshLoader.h>
#include <iostream>
#include <limits>
using namespace Aevrynd;using namespace Mistwatch;
void Require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
float TriangleHeight(const HeightField3D& terrain,float x,float z){
    const auto& s=terrain.Settings();const float gx=(x-s.origin.x)/s.cellSize,gz=(z-s.origin.z)/s.cellSize;
    const auto ix=(std::min)(unsigned(std::floor(gx)),s.width-2),iz=(std::min)(unsigned(std::floor(gz)),s.depth-2);
    const float tx=gx-float(ix),tz=gz-float(iz),a=terrain.Height(ix,iz),b=terrain.Height(ix+1,iz),c=terrain.Height(ix,iz+1),d=terrain.Height(ix+1,iz+1);
    return tx+tz<=1?a+(b-a)*tx+(c-a)*tz:d+(c-d)*(1-tx)+(b-d)*(1-tz);
}
int main(){try{
    const auto assets=std::filesystem::path(MISTWATCH_SOURCE_ROOT)/"engine/assets";
    const auto terrain=CreateTerrain(),treesTerrain=terrain;const auto trees=TreePositions();const auto props=ForestProps(terrain,trees);
    const auto understory=CreateUnderstory(terrain,trees,props);const auto broadleaf=CreateBroadleafTrees(terrain,trees,props,understory);
    auto solids=WorldSolids(false);const auto middle=CreateMiddleLayer(terrain,trees,props,understory,broadleaf,solids);
    Require(middle.reachedBudget,"Original middle layer failed");for(const auto& tree:middle.trees)solids.push_back(tree.trunk);
    std::array<ForestCoverAsset,5> grassAssets;
    for(unsigned i=0;i<5;++i){auto loaded=LoadAevryndMeshFile(assets/"mistwatch-grass-medium-02/meshes"/GrassMeshName(i));Require(bool(loaded),"Grass asset missing");NormalizeGrassAsset(loaded.mesh);grassAssets[i]=ForestCoverAssetFromMesh(loaded.mesh);}
    std::array<FernColonyAsset,4> fernAssets;
    for(unsigned i=0;i<4;++i){auto loaded=LoadAevryndMeshFile(assets/"mistwatch-fern-02/meshes"/FernMeshName(i));Require(bool(loaded),"Fern asset missing");NormalizeFernAsset(loaded.mesh);fernAssets[i]=FernColonyAssetFromMesh(loaded.mesh);}
    const auto grass=CreateForestCover(terrain,trees,understory,solids,grassAssets);Require(grass.reachedBudget,"Original grass failed");
    const auto colonies=CreateFernColonies(terrain,grass,understory.ferns,solids,fernAssets);Require(colonies.reachedBudget,"Original colonies failed");
    auto leaf=LoadAevryndMeshFile(assets/"realistic-cc0-first-batch/cooked/meshes/tree-small-02"/BroadleafMeshName(1));Require(bool(leaf),"Leaf asset missing");
    unsigned rejectedInputs{};
    const auto mustReject=[&](const auto& badUnderstory,const auto& badGrass,const auto& badColonies,const char* label){
        bool refused=false;
        try{(void)CreateShrubCanopy(leaf.mesh,terrain,solids,badUnderstory,badGrass,badColonies,fernAssets);}
        catch(const std::invalid_argument&){refused=true;}
        if(!refused)throw std::runtime_error(std::string("Invalid existing footprint accepted: ")+label);
        ++rejectedInputs;
    };
    for(float bad:{(std::numeric_limits<float>::quiet_NaN)(),(std::numeric_limits<float>::infinity)(),-1.f,0.f,5.f}){
        auto invalidGrass=grass;invalidGrass.plants.front().radius=bad;mustReject(understory,invalidGrass,colonies,"grass radius");
        auto invalidColonies=colonies;invalidColonies.plants.front().radius=bad;mustReject(understory,grass,invalidColonies,"colony radius");
        auto invalidUnderstory=understory;invalidUnderstory.ferns.front().scale={bad,bad,bad};mustReject(invalidUnderstory,grass,colonies,"existing fern scale");
    }
    for(unsigned kind=0;kind<5;++kind){
        auto invalidGrass=grass;auto invalidColonies=colonies;auto invalidUnderstory=understory;
        const auto damage=[&](Transform3D& transform){
            if(kind==0)transform.position.x=(std::numeric_limits<float>::quiet_NaN)();
            if(kind==1)transform.rotation.y=(std::numeric_limits<float>::infinity)();
            if(kind==2)transform.scale.z=transform.scale.x+.1f;
            if(kind==3)transform.position.x=10000.f;
            if(kind==4)transform.rotation.x=.1f;
        };
        damage(invalidGrass.plants.front().transform);mustReject(understory,invalidGrass,colonies,"grass transform");
        damage(invalidColonies.plants.front().transform);mustReject(understory,grass,invalidColonies,"colony transform");
        damage(invalidUnderstory.ferns.front());mustReject(invalidUnderstory,grass,colonies,"existing fern transform");
    }
    {auto bad=grass;bad.plants.front().variant=5;mustReject(understory,bad,colonies,"grass variant");}
    {auto bad=colonies;bad.plants.front().variant=4;mustReject(understory,grass,bad,"colony variant");}
    const auto result=CreateShrubCanopy(leaf.mesh,terrain,solids,understory,grass,colonies,fernAssets);
    const auto repeat=CreateShrubCanopy(leaf.mesh,terrain,solids,understory,grass,colonies,fernAssets);
    Require(result.reachedBudget&&result.plants.size()==64,"Shrub population missing");Require(result.fingerprint==repeat.fingerprint,"Nondeterministic shrub placement");
    Require(result.fingerprint==18032575485754629455ull&&result.attempted==6284,"Authored shrub profile drift");
    unsigned initial{},left{},right{};std::size_t checked{};float minimumLeafClearance=100.f;
    for(unsigned i=0;i<4;++i){const auto& shrub=result.variants[i];Require(shrub.stemCount==7&&shrub.leafCount==504,"Missing shrub structure");
        Require(shrub.attachments.size()==504,"Missing complete leaf attachment records");
        for(const auto& attachment:shrub.attachments){
            Require(attachment.vertex<shrub.leaves.vertices.size(),"Attachment index missing");
            const auto& vertex=shrub.leaves.vertices[attachment.vertex];
            Require(std::abs(Length(Vec3{vertex.x,vertex.y,vertex.z}-attachment.centre)-attachment.branchRadius)<1.e-6f,
                "Actual copied leaf anchor does not touch author branch surface");
            Require(attachment.branchRadius>=.001499f&&attachment.branchRadius<=.003801f,"Attachment radius not branch taper");
        }
        Require(shrub.stems.Valid()&&shrub.leaves.Valid()&&shrub.height<1.2f&&shrub.radius<.75f,"Invalid shrub topology/envelope");
        Require(shrub.leaves.indices.size()<36000&&shrub.stems.indices.size()<12000,"Unbounded shrub triangles");
        for(std::size_t n=0;n<shrub.leaves.vertices.size();++n){const auto& a=shrub.leaves.vertices[n];const auto& b=repeat.variants[i].leaves.vertices[n];
            Require(a.x==b.x&&a.y==b.y&&a.z==b.z&&a.u==b.u&&a.v==b.v&&a.normal==b.normal,"Nondeterministic complete leaf geometry");}
        std::cout<<"variant="<<i<<" radius="<<shrub.radius<<" height="<<shrub.height<<" leaves="<<shrub.leafCount<<" triangles="<<(shrub.leaves.indices.size()+shrub.stems.indices.size())/3<<'\n';
    }
    for(const auto& p:result.plants){const auto root=p.pose.position;left+=root.x<0;right+=root.x>0;initial+=root.z<26&&std::abs(root.x)<18;
        Require(std::abs(root.y-TriangleHeight(terrain,root.x,root.z))<2.e-5f,"Shrub root lacks real triangle support");
        Require(root.x-p.radius>=4.5f||root.x+p.radius<=-4.5f,"Shrub obstructs main route footprint");
        Require(root.z-p.radius>=43||root.z+p.radius<=29,"Shrub obstructs bridge strip");
        for(const auto& s:solids)Require(std::abs(root.x-s.center.x)>=s.extents.x+p.radius||std::abs(root.z-s.center.z)>=s.extents.z+p.radius,"Shrub enters solid");
        for(const auto& g:grass.plants)Require(std::hypot(root.x-g.transform.position.x,root.z-g.transform.position.z)>=p.radius+g.radius+.03f,"Shrub violates complete grass clearance");
        for(const auto& f:colonies.plants)Require(std::hypot(root.x-f.transform.position.x,root.z-f.transform.position.z)>=p.radius+f.radius+.03f,"Shrub violates complete fern clearance");
        const auto matrix=p.pose.Matrix();const auto& shrub=result.variants[p.variant];
        for(const auto* mesh:{&shrub.stems,&shrub.leaves})for(const auto& v:mesh->vertices){const auto world=TransformPoint(matrix,{v.x,v.y,v.z});
            Require(std::hypot(world.x-root.x,world.z-root.z)<=p.radius+1.e-5f,"Actual shrub escapes complete footprint");
            Require(terrain.Contains(world.x,world.z),"Shrub outside ground");
            const auto h=TriangleHeight(terrain,world.x,world.z);Require(std::abs(h-root.y)<=.24002f,"Shrub footprint hidden slope exceeds gate");
            Require(world.y-h>=-.04f,"Shrub branch penetrates ground beyond embedded root");
            if(mesh==&shrub.leaves){minimumLeafClearance=(std::min)(minimumLeafClearance,world.y-h);Require(world.y-h>.04f,"Shrub leaves buried in ground");}
            ++checked;
        }
        for(const auto* mesh:{&shrub.stems,&shrub.leaves})for(std::size_t face=0;face<mesh->indices.size();face+=3){
            const auto& a=mesh->vertices[mesh->indices[face]],b=mesh->vertices[mesh->indices[face+1]],c=mesh->vertices[mesh->indices[face+2]];
            for(const auto weights:std::array<Vec3,4>{{{1.f/3,1.f/3,1.f/3},{.8f,.1f,.1f},{.1f,.8f,.1f},{.1f,.1f,.8f}}}){
                const auto local=Vec3{a.x,a.y,a.z}*weights.x+Vec3{b.x,b.y,b.z}*weights.y+Vec3{c.x,c.y,c.z}*weights.z;
                const auto world=TransformPoint(matrix,local);const auto h=TriangleHeight(terrain,world.x,world.z);
                Require(world.y-h>=-.04f,"Shrub triangle interior crosses ground beyond embedded root");++checked;
            }
        }
    }
    Require(initial>=8&&left>=16&&right>=16,"Insufficient visible route-shoulder population");
    auto invalid=leaf.mesh;invalid.vertices[0].x=(std::numeric_limits<float>::quiet_NaN)();bool rejected=false;
    try{(void)CreateShrubCanopy(invalid,terrain,solids,understory,grass,colonies,fernAssets);}catch(const std::invalid_argument&){rejected=true;}
    Require(rejected,"Nonfinite mesh accepted");
    auto blocked=solids;blocked.push_back({{0,0,0},{100,100,100}});
    const auto empty=CreateShrubCanopy(leaf.mesh,terrain,blocked,understory,grass,colonies,fernAssets);
    Require(!empty.reachedBudget&&empty.plants.empty(),"Blocked world generated shrubs");
    Require(grass.plants.size()==3600&&colonies.plants.size()==300&&understory.ferns.size()==420&&solids.size()==245,"Existing world changed");
    Require(treesTerrain.Settings().width==terrain.Settings().width,"Terrain unexpectedly changed");
    std::cout<<"SHRUB_CPU_PASS plants="<<result.plants.size()<<" attempts="<<result.attempted<<" initial="<<initial<<" left="<<left<<" right="<<right<<" checked="<<checked<<" leaf_clearance="<<minimumLeafClearance<<" fingerprint="<<result.fingerprint<<" rejected_inputs="<<rejectedInputs<<'\n';return 0;
}catch(const std::exception& error){std::cerr<<"SHRUB_CPU_FIRST_DIVERGENCE "<<error.what()<<'\n';return 1;}}
