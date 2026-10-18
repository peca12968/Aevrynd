#include "MistwatchEntranceComposition.h"
#include "fallen/MistwatchAuthoredNativeExpected.h"
#include <iostream>
using namespace Aevrynd;
using namespace Mistwatch;
namespace Q=::Mistwatch::AuthoredQualification;
namespace W=::Mistwatch::FallenWorldSeat;
namespace N=::Mistwatch::AuthoredNativeExpected;
namespace {
void Need(bool value,const char* gate){if(!value)throw std::runtime_error(gate);}
std::vector<W::Placement> Mappings(){std::vector<W::Placement> out;
    for(const auto& p:FallenLogPlacements()){W::Placement q;q.propId=p.propId;q.anchor=p.authoredAnchor;q.pose=p.pose;q.collider=p.collider;out.push_back(q);}return out;}
}
int wmain(int argc,wchar_t** argv)try{
    Need(argc==3&&std::wstring_view(argv[1])==L"--runtime-assets","Absolute runtime usage");
    const auto root=std::filesystem::path(argv[2]);Need(root.is_absolute(),"Absolute runtime assets");
    std::array<Mesh3D,3> meshes;for(unsigned i=0;i<3;++i){meshes[i]=Q::Detail::LoadPinned(root/N::Detail::BroadleafPins[i].path,N::Detail::BroadleafPins[i]);RootBroadleafMesh(meshes[i]);}
    const auto terrain=CreateTerrain();W::Budget budget;const auto assets=Q::LoadAssets(root);
    const auto source=Q::CreateSourceContext(terrain,assets,budget);
    const auto original=Q::CreateContext(terrain,assets,source,Mappings(),budget);
    const auto originalSeal=Q::ContextFingerprint(original);const auto result=CreateRightForegroundComposition(original.author,terrain,meshes,assets.fernAssets,original.wood);
    for(unsigned i=0;i<12;++i){auto expected=original.author.broadleaf[i];
        if(i==9){expected.transform.position={9,terrain.SampleHeight(9,4)-.045f,4};const auto b=TransformBounds(BroadleafLowerTrunkBounds(),expected.transform);expected.trunk={b.center,b.extents};}
        Need(Q::Detail::Same(expected.transform,result.author.broadleaf[i].transform)&&Q::Detail::Same(expected.trunk,result.author.broadleaf[i].trunk),"Independent twelve tree mapping");}
    for(unsigned i=0;i<420;++i){auto expected=original.author.understory.ferns[i];
        if(i==188||i==332){expected.position.x+=2;expected.position.z+=i==188?-2.f:1.f;expected.position.y=terrain.SampleHeight(expected.position.x,expected.position.z);}
        Need(Q::Detail::Same(expected,result.author.understory.ferns[i]),"Independent420 fern mapping");}
    auto c=original;c.author=result.author;
    for(unsigned state=0;state<2;++state){const auto old=original.author.WorldSolids(state!=0),final=c.author.WorldSolids(state!=0);
        Need(old.size()==final.size(),"World count changed");unsigned differences{};
        for(unsigned i=0;i<old.size();++i)if(!Q::Detail::Same(old[i],final[i])){++differences;Need(i==old.size()-3,"Only broadleaf9 body may change");}
        Need(differences==1,"Both bridge states require one new body");
        unsigned replacements{};for(auto& solid:c.worlds[state])if(solid.role==W::Role::Broadleaf&&solid.id==9){solid.box=c.author.broadleaf[9].trunk;++replacements;}
        Need(replacements==1,"Typed source9 identity missing");c.vegetation[state]=c.author.VegetationSolids(state!=0);
        auto expectedWorld=c.vegetation[state];expectedWorld.insert(expectedWorld.end(),c.wood.begin(),c.wood.end());
        Q::Detail::SameBoxes(expectedWorld,W::Boxes(c.worlds[state]),"New author/render world ordering differs");}
    const auto originalSolids=W::Boxes(c.sourceMapped[0]);
    ForestCoverWorldView oldView{terrain,source.firs.roots,source.understory,originalSolids,c.sourceWood,4335,0};
    ForestCoverWorldView newView{terrain,c.author.firs.roots,c.author.understory,c.vegetation[0],c.wood,8617,0};
    oldView.expectedInputFingerprint=ForestCoverRetentionWorldFingerprint(oldView);newView.expectedInputFingerprint=ForestCoverRetentionWorldFingerprint(newView);
    c.retention=CreateForestCoverRetained(c.sourceCover,oldView,newView,assets.grassAssets,assets.grassIdentities);
    c.coverFingerprint=Q::Detail::CoverIdentity(c.retention.cover);c.retentionFingerprint=Q::Detail::RetentionIdentity(c.retention);
    Q::ValidateRetentionMappings(c,terrain,assets,budget);
    c.litter=CreateTrailLitter(terrain,W::Boxes(c.worlds[0]));
    c.colonies=CreateFernColonies(terrain,c.retention.cover,c.author.understory.ferns,c.vegetation[0],assets.fernAssets,{},c.wood,true);
    c.shrubs=CreateShrubCanopy(assets.shrubLeaf,terrain,c.vegetation[0],c.author.understory,c.retention.cover,c.colonies,assets.fernAssets,64,c.wood,true);
    Need(c.retention.cover.reachedBudget&&c.retention.cover.plants.size()==3600&&c.colonies.reachedBudget&&c.shrubs.reachedBudget&&c.shrubs.plants.size()==64,"New downstream quota missing");
    Q::ValidateDownstreamVolumes(c,terrain,assets,budget);
    // 新作者不使用原版 ValidateContext 的舊12樹／五蕨身分當成功；逐項輸出獨立CPU的衍生預期。
    N::Contract out;using N::Detail::Number;using N::Detail::Field;using N::Detail::Csv;
    using N::Detail::Colonies;using N::Detail::Poses;
    Number(out,"entrance_author_fingerprint",c.author.fingerprint);Number(out,"entrance_author_fern_relocations",7);
    Field(out,"entrance_composition_profile",std::string(RightForegroundProfile));Number(out,"entrance_composition_original_fingerprint",result.originalFingerprint);
    Number(out,"entrance_composition_pose_fingerprint",result.finalPoseFingerprint);Number(out,"entrance_composition_tree_moves",1);Number(out,"entrance_composition_fern_moves",2);
    const auto& cover=c.retention.cover;std::array<unsigned,5> gv{};std::array<unsigned,4> fv{};
    for(const auto& p:cover.plants)++gv[p.variant];for(const auto& p:c.colonies.plants)++fv[p.variant];
    Number(out,"forest_cover_patches",cover.patches.size());Number(out,"forest_cover_fingerprint",Q::Detail::NativeCoverIdentity(cover));
    Number(out,"forest_cover_clearance_colliders",c.worlds[0].size());Number(out,"forest_cover_clearance_collider_fingerprint",Q::Detail::BoxesIdentity(W::Boxes(c.worlds[0])));
    Field(out,"forest_cover_variants",Csv(gv));Number(out,"forest_cover_retained",c.retention.retained);Number(out,"forest_cover_rejected_original",c.retention.rejected);
    Number(out,"forest_cover_filled",c.retention.filled);Number(out,"forest_cover_provenance_fingerprint",c.retention.fingerprint);Number(out,"forest_cover_final_world_fingerprint",c.retention.finalInputFingerprint);
    Number(out,"fern_colony_population",c.colonies.plants.size());Number(out,"fern_colonies",c.colonies.colonies.size());
    Number(out,"fern_colony_fingerprint",Colonies(c.colonies.plants));Number(out,"fern_colony_original_fingerprint",Poses(c.author.understory.ferns));Field(out,"fern_colony_variants",Csv(fv));
    Number(out,"shrubs_fingerprint",c.shrubs.fingerprint);std::uint64_t authorTriangles{},renderTriangles{};
    for(const auto& p:c.shrubs.plants){const auto& v=c.shrubs.variants[p.variant];const auto mesh=CreateShrubOpenCrown(v,p.variant);
        authorTriangles+=(v.stems.indices.size()+v.leaves.indices.size())/3;renderTriangles+=(mesh.stems.indices.size()+mesh.leaves.indices.size())/3;}
    Number(out,"shrubs_author_instance_triangles",authorTriangles);Number(out,"shrubs_render_instance_triangles",renderTriangles);
    Number(out,"trail_litter_patches",c.litter.patches.size());Number(out,"trail_litter_branches",c.litter.branchCount);Number(out,"trail_litter_stones",c.litter.stoneCount);
    Number(out,"trail_litter_fingerprint",c.litter.fingerprint);Number(out,"trail_litter_triangles",(c.litter.branches.indices.size()+c.litter.stones.indices.size())/3);
    unsigned controls{};const auto reject=[&](const EntranceWorldAuthor& a,const std::array<Mesh3D,3>& m,std::span<const BoxCollider3D> w,const char* gate){
        bool rejected=false;try{(void)CreateRightForegroundComposition(a,terrain,m,assets.fernAssets,w);}catch(const std::invalid_argument& e){
            Need(std::string_view(e.what()).find(gate)!=std::string_view::npos,"Wrong negative reason");rejected=true;}
        Need(rejected&&Q::ContextFingerprint(original)==originalSeal,"Invalid input changed original or accepted");++controls;};
    auto bad=original.author;bad.broadleaf[9].transform.position.x+=1;reject(bad,meshes,c.wood,"source-tree-pose");
    bad=original.author;bad.understory.ferns[188].position.x+=1;reject(bad,meshes,c.wood,"canonical-author-identity");
    reject(original.author,meshes,std::span<const BoxCollider3D>(c.wood).first(8616),"complete-eight-canopy-wood");
    auto badMeshes=meshes;badMeshes[0].vertices[0].x+=.1f;reject(original.author,badMeshes,c.wood,"rooted-actual-mesh-identity");
    Need(Q::ContextFingerprint(original)==originalSeal,"Original context drift");
    for(const auto& [key,value]:out)std::cout<<"COMPOSITION_EXPECTED "<<key<<'='<<value<<'\n';
    std::cout<<"RIGHT_FOREGROUND_CPU_PASS tree_moves=1 fern_moves=2 negative_controls="<<controls<<" both_bridge_states=1 original_unchanged=1 downstream_volume=1 native=0 art=0\n";return 0;
}catch(const std::exception& e){std::cerr<<"RIGHT_FOREGROUND_FIRST_DIVERGENCE "<<e.what()<<'\n';return 1;}
