#pragma once
#include "MistwatchFernColonies.h"
#include "MistwatchImmutableVolumeBounds.h"
#include <numeric>
#include <bit>
#include <map>

namespace Mistwatch {
struct ShrubMesh {
    Aevrynd::Mesh3D stems,leaves;
    float radius{},height{};
    float minimumY{},maximumY{};
    unsigned leafCount{},stemCount{};
    struct Attachment {unsigned vertex{};Aevrynd::Vec3 centre;float branchRadius{};};
    std::vector<Attachment> attachments;
};
namespace ShrubDetail {
using namespace Aevrynd;
inline std::vector<Mesh3D> LeafComponents(const Mesh3D& source) {
    if(!source.Valid()||source.vertices.size()>200000||source.indices.size()>600000)
        throw std::invalid_argument("Invalid bounded shrub leaf source");
    for(const auto& v:source.vertices)if(!Session::Finite({v.x,v.y,v.z})||!std::isfinite(v.u)||!std::isfinite(v.v))
        throw std::invalid_argument("Nonfinite shrub leaf source");
    std::vector<unsigned> parent(source.vertices.size());std::iota(parent.begin(),parent.end(),0u);
    const auto root=[&](unsigned i){while(parent[i]!=i){parent[i]=parent[parent[i]];i=parent[i];}return i;};
    for(std::size_t i=0;i<source.indices.size();i+=3){
        const auto a=root(source.indices[i]);parent[root(source.indices[i+1])]=a;parent[root(source.indices[i+2])]=a;
    }
    std::map<unsigned,std::vector<unsigned>> faces;
    for(unsigned i=0;i<source.indices.size();i+=3)faces[root(source.indices[i])].push_back(i);
    std::vector<Mesh3D> result;
    // 原DDS/UV三角面積加權四點採樣排序的完整寬葉細枝；不是精確alpha積分。
    constexpr std::array<unsigned,16> selected{84719,58399,61097,75161,53291,81364,61487,80932,66675,55694,61573,51488,55464,96331,45365,77603};
    for(const auto key:selected){
        const auto found=faces.find(key);if(found==faces.end())throw std::runtime_error("Curated original leaf sample missing");
        const auto& group=found->second;
        if(group.size()<8||group.size()>32)continue;
        Mesh3D leaf;std::map<unsigned,unsigned> remap;
        for(const auto face:group)for(unsigned corner=0;corner<3;++corner){
            const auto index=source.indices[face+corner];
            if(!remap.contains(index)){remap[index]=unsigned(leaf.vertices.size());leaf.vertices.push_back(source.vertices[index]);}
            leaf.indices.push_back(remap[index]);
        }
        const auto bounds=leaf.Bounds();
        if(bounds.radius<.025f||bounds.radius>.09f)continue;
        // 作者附著點取真幾何最低頂點；保留完整面片與UV，不以AABB中心懸掛。
        const auto attachment=*std::min_element(leaf.vertices.begin(),leaf.vertices.end(),[](const auto& a,const auto& b){return a.y<b.y;});
        for(auto& v:leaf.vertices){v.x-=attachment.x;v.y-=attachment.y;v.z-=attachment.z;}
        result.push_back(std::move(leaf));
        if(result.size()==16)break;
    }
    if(result.size()!=16)throw std::runtime_error("Insufficient complete measured leaf components");
    return result;
}
inline void Append(Mesh3D& destination,const Mesh3D& source,const Transform3D& pose={}) {
    const auto base=unsigned(destination.vertices.size());const auto matrix=pose.Matrix(),normalMatrix=CreateNormalMatrix(matrix);
    for(const auto& original:source.vertices){
        auto v=original;const auto p=TransformPoint(matrix,{v.x,v.y,v.z});
        const auto n=UnpackNormalSnorm8(v.normal);const auto transformed=TransformNormal(normalMatrix,{n.x,n.y,n.z});
        v.x=p.x;v.y=p.y;v.z=p.z;v.normal=PackNormalSnorm8(transformed,n.w);destination.vertices.push_back(v);
    }
    for(const auto index:source.indices)destination.indices.push_back(base+index);
}
inline void Branch(Mesh3D& mesh,Vec3 a,Vec3 b,float r0,float r1) {
    const auto axis=Normalize(b-a);const auto side=Normalize(Cross(axis,std::abs(axis.y)>.9f?Vec3{1,0,0}:Vec3{0,1,0}));
    const auto up=Normalize(Cross(axis,side));const auto base=unsigned(mesh.vertices.size());
    for(unsigned ring=0;ring<2;++ring)for(unsigned i=0;i<6;++i){
        const float phi=float(i)*6.2831853f/6.f;const auto n=side*std::cos(phi)+up*std::sin(phi);
        const auto p=(ring?b:a)+n*(ring?r1:r0);
        mesh.vertices.push_back({p.x,p.y,p.z,PackNormalSnorm8(n),PackColorRgba8({1,1,1,1}),float(i)/6.f,float(ring)});
    }
    for(unsigned i=0;i<6;++i){const auto next=(i+1)%6;mesh.indices.insert(mesh.indices.end(),{base+i,base+6+i,base+next,base+next,base+6+i,base+6+next});}
}
inline ShrubMesh Build(const std::vector<Mesh3D>& cards,unsigned variant) {
    ShrubMesh result;std::uint32_t seed=0x42555348u+variant*977u;
    const auto random=[&](){seed=seed*1664525u+1013904223u;return float(seed>>8)/16777216.f;};
    // 基生多莖、分層側枝與完整真葉片；沒有保留大樹的高單幹或縮放整棵樹。
    for(unsigned stem=0;stem<7;++stem){
        const float phi=6.2831853f*(float(stem)/7.f)+(random()-.5f)*.3f;
        const Vec3 radial{std::cos(phi),0,std::sin(phi)};
        const float height=.62f+.34f*random(),spread=.18f+.18f*random();
        Vec3 previous{0,-.018f,0};
        for(unsigned level=1;level<=4;++level){
            const float t=float(level)/4.f;const auto joint=radial*(spread*t*t)+Vec3{0,height*t,0};
            Branch(result.stems,previous,joint,.014f*(1.f-.75f*(t-.25f)),.014f*(1.f-.75f*t));
            for(float sign:{-1.f,1.f}){
                const float angle=phi+sign*(.7f+.3f*random());const Vec3 direction{std::cos(angle),0,std::sin(angle)};
                const auto tip=joint+direction*(.12f+.10f*random())+Vec3{0,.025f+.035f*random(),0};
                Branch(result.stems,joint,tip,.0038f,.0015f);
                for(unsigned leaf=0;leaf<9;++leaf){
                    const float along=.2f+.1f*float(leaf);
                    const auto centre=joint+(tip-joint)*along;
                    const float branchRadius=.0038f+along*(.0015f-.0038f);
                    // 取細枝表面而非原25mm懸空偏移；完整原面片的作者錨點仍不宣稱真植物葉柄。
                    const auto position=centre+Vec3{-direction.z,0,direction.x}*(leaf%2?branchRadius:-branchRadius);
                    const auto index=(std::min)(15u,unsigned(random()*16.f));
                    const auto& card=cards[index];
                    const auto anchor=std::min_element(card.vertices.begin(),card.vertices.end(),[](const auto& a,const auto& b){return a.y<b.y;});
                    const auto vertex=unsigned(result.leaves.vertices.size()+std::size_t(anchor-card.vertices.begin()));
                    Append(result.leaves,cards[index],{position,{(random()-.5f)*.6f,random()*6.2831853f,(random()-.5f)*.6f},{1,1,1}});
                    result.attachments.push_back({vertex,centre,branchRadius});
                    ++result.leafCount;
                }
            }
            previous=joint;
        }
        ++result.stemCount;
    }
    for(const auto* mesh:{&result.stems,&result.leaves})for(const auto& v:mesh->vertices){
        result.radius=(std::max)(result.radius,std::hypot(v.x,v.z));result.height=(std::max)(result.height,v.y);
    }
    CanopyVolume::PlantLocalVolume volume;
    if(!CanopyVolume::MeasureLocalVolume(std::span<const Vertex3D>(result.stems.vertices),
        std::span<const Vertex3D>(result.leaves.vertices),volume))throw std::runtime_error("Invalid complete shrub full volume");
    result.minimumY=volume.minimumY;result.maximumY=volume.maximumY;
    if(!result.stems.Valid()||!result.leaves.Valid()||result.radius>.75f||result.height>1.2f||result.leafCount!=504)
        throw std::runtime_error("Shrub geometry outside bounded author envelope: radius="+std::to_string(result.radius)+", height="+std::to_string(result.height));
    return result;
}
}
struct ShrubPlant {Aevrynd::Transform3D pose;unsigned variant{};float radius{};};
// 有限資料容器，不執行外部回呼；各值只旁記原搜尋，不能改動接受門或亂數。
enum class ShrubSearchStage {Clearance,SolidVolume,CanopyWood,Terrain,Vegetation,Accepted};
enum ShrubVegetationMask : unsigned {ShrubOriginalFern=1u,ShrubColonyFern=2u,ShrubGrass=4u,ShrubSelf=8u};
struct ShrubSearchAttempt {
    unsigned attempt{};
    Aevrynd::Vec3 root;
    unsigned variant{};
    float scale{},radius{};
    ShrubSearchStage stage{ShrubSearchStage::Clearance};
    unsigned vegetationMask{};
};
struct ShrubSearchTrace {
    static constexpr unsigned MaximumRecords=24000;
    std::vector<ShrubSearchAttempt> records;
};
struct ShrubCanopyResult {
    std::array<ShrubMesh,4> variants;
    std::vector<ShrubPlant> plants;
    unsigned attempted{},rejectedClearance{},rejectedTerrain{},rejectedVegetation{};
    std::uint64_t fingerprint{14695981039346656037ull};
    bool reachedBudget{};
};
inline ShrubCanopyResult CreateShrubCanopy(const Aevrynd::Mesh3D& leaves,const Aevrynd::HeightField3D& terrain,
    std::span<const Aevrynd::BoxCollider3D> solids,const ForestUnderstory& understory,const ForestCoverResult& grass,
    const FernColonyResult& colonies,const std::array<FernColonyAsset,4>& fernAssets,unsigned target=64,
    std::span<const Aevrynd::BoxCollider3D> canopyWood={},bool fullSolidVolumes=false,ShrubSearchTrace* trace=nullptr,
    const GroupedCanopyVolume::ValidatedWood* groupedWood=nullptr,const EntryLayout& layout=ActiveEntryLayout) {
    using namespace Aevrynd;
    if(trace&&!trace->records.empty())throw std::invalid_argument("SHRUB_TRACE_NOT_EMPTY");
    // 舊 flat 分支不改；群組必須明確啟用完整實體體積，失效物件在生成前拒絕。
    if(groupedWood&&(!fullSolidVolumes||!canopyWood.empty()||groupedWood->Identity().groupCount==0||
        groupedWood->Identity().groupCount>GroupedCanopyVolume::MaximumGroups||
        groupedWood->Groups().size()!=groupedWood->Identity().groupCount||groupedWood->Identity().totalParts==0||
        groupedWood->Identity().totalParts>GroupedCanopyVolume::MaximumTotalParts))
        throw std::invalid_argument("Invalid grouped shrub canopy wood");
    if(!CanopyVolume::ValidWoodParts(canopyWood))throw std::invalid_argument("Invalid canopy wood parts for shrubs");
    if(target>128||solids.size()>10000||understory.ferns.size()>10000||grass.plants.size()>10000||colonies.plants.size()>1000)
        throw std::invalid_argument("Invalid shrub bounded population");
    // 完整腳印比較之前先驗證資料；NaN不得讓距離比較靜默變成未占用。
    const auto validPose=[&](const Transform3D& pose){
        return Session::Finite(pose.position)&&Session::Finite(pose.rotation)&&Session::Finite(pose.scale)&&
            pose.scale.x>0&&pose.scale.x<=4&&pose.scale.x==pose.scale.y&&pose.scale.x==pose.scale.z&&
            pose.rotation.x==0&&pose.rotation.z==0&&terrain.Contains(pose.position.x,pose.position.z)&&
            std::abs(pose.position.y)<=10000;
    };
    const auto validRadius=[](float radius){return std::isfinite(radius)&&radius>0&&radius<=4;};
    for(const auto& fern:understory.ferns)if(!validPose(fern))
        throw std::invalid_argument("Invalid shrub existing fern transform");
    for(const auto& blade:grass.plants)if(!validPose(blade.transform)||!validRadius(blade.radius)||blade.variant>=5)
        throw std::invalid_argument("Invalid shrub existing grass footprint");
    for(const auto& fern:colonies.plants)if(!validPose(fern.transform)||!validRadius(fern.radius)||fern.variant>=4)
        throw std::invalid_argument("Invalid shrub existing colony footprint");
    for(const auto& solid:solids)if(!Session::Finite(solid.center)||!Session::Finite(solid.extents)||solid.extents.x<0||solid.extents.y<0||solid.extents.z<0)
        throw std::invalid_argument("Invalid shrub solid");
    for(const auto& asset:fernAssets)if(!std::isfinite(asset.radius)||asset.radius<=0||asset.radius>4)
        throw std::invalid_argument("Invalid shrub existing fern radius");
    ShrubCanopyResult result;const auto cards=ShrubDetail::LeafComponents(leaves);
    for(unsigned i=0;i<4;++i)result.variants[i]=ShrubDetail::Build(cards,i);
    if(trace)trace->records.reserve(ShrubSearchTrace::MaximumRecords);
    std::uint32_t seed=0x53485242u;
    const auto random=[&](){seed=seed*1664525u+1013904223u;return float(seed>>8)/16777216.f;};
    const ForestCoverSettings footprint{};
    CanopyVolume::ImmutableVolumeBoundsCache solidBounds(solids);
    CanopyVolume::ImmutableVolumeBoundsCache canopyWoodBounds(canopyWood);
    for(unsigned attempt=0;attempt<24000&&result.plants.size()<target;++attempt){
        ++result.attempted;
        const unsigned variant=unsigned(random()*4.f);const float scale=.85f+.25f*random();
        const float x=(attempt%2?1.f:-1.f)*(6.f+22.f*random()),z=-8.f+92.f*random();
        const Vec3 root{x,terrain.SampleHeight(x,z),z};const float radius=result.variants[variant].radius*scale;
        ShrubSearchAttempt* record=nullptr;
        if(trace){
            if(trace->records.size()>=ShrubSearchTrace::MaximumRecords)throw std::runtime_error("SHRUB_TRACE_LIMIT");
            trace->records.push_back({attempt,root,variant,scale,radius,ShrubSearchStage::Clearance,0});
            record=&trace->records.back();
        }
        if(!ForestCoverClearance(root,radius,solids,layout)){++result.rejectedClearance;continue;}
        if(record)record->stage=ShrubSearchStage::SolidVolume;
        // 開始完整實體體積檢查
if(fullSolidVolumes&&!solids.empty()&&!solidBounds.Clears(
            CanopyVolume::PlantVolume{root,CanopyVolume::ConservativeLocalVolume(result.variants[variant].radius,
                result.variants[variant].minimumY,result.variants[variant].maximumY),scale,{}})){
            ++result.rejectedClearance;continue;
        }
// 結束完整實體體積檢查
if(record)record->stage=ShrubSearchStage::CanopyWood;
if((groupedWood||!canopyWood.empty())&&!(groupedWood?groupedWood->Clears(
            CanopyVolume::PlantVolume{root,CanopyVolume::ConservativeLocalVolume(result.variants[variant].radius,
                result.variants[variant].minimumY,result.variants[variant].maximumY),scale,{}}):canopyWoodBounds.Clears(
            CanopyVolume::PlantVolume{root,CanopyVolume::ConservativeLocalVolume(result.variants[variant].radius,
                result.variants[variant].minimumY,result.variants[variant].maximumY),scale,{}}))){
            ++result.rejectedClearance;continue;
        }
        if(record)record->stage=ShrubSearchStage::Terrain;
        if(!ForestCoverFootprint(terrain,root,radius,footprint)){++result.rejectedTerrain;continue;}
        if(record)record->stage=ShrubSearchStage::Vegetation;
        bool occupied=false;
        // 新灌木保留完整半徑和間隙，舊草蕨族群不重生；不能用放寬混合距離填滿配額。
        for(std::size_t i=0;i<understory.ferns.size();++i){const auto& f=understory.ferns[i];
            const bool blocked=std::hypot(x-f.position.x,z-f.position.z)<radius+fernAssets[i%4].radius*f.scale.x+.03f;
            occupied|=blocked;if(record&&blocked)record->vegetationMask|=ShrubOriginalFern;}
        for(const auto& f:colonies.plants){const bool blocked=std::hypot(x-f.transform.position.x,z-f.transform.position.z)<radius+f.radius+.03f;
            occupied|=blocked;if(record&&blocked)record->vegetationMask|=ShrubColonyFern;}
        for(const auto& g:grass.plants){const bool blocked=std::hypot(x-g.transform.position.x,z-g.transform.position.z)<radius+g.radius+.03f;
            occupied|=blocked;if(record&&blocked)record->vegetationMask|=ShrubGrass;}
        for(const auto& p:result.plants){const bool blocked=std::hypot(x-p.pose.position.x,z-p.pose.position.z)<radius+p.radius+.3f;
            occupied|=blocked;if(record&&blocked)record->vegetationMask|=ShrubSelf;}
        if(occupied){++result.rejectedVegetation;continue;}
        if(record)record->stage=ShrubSearchStage::Accepted;
        const Transform3D pose{root,{0,random()*6.2831853f,0},{scale,scale,scale}};
        result.plants.push_back({pose,variant,radius});
        for(float value:{x,root.y,z,pose.rotation.y,scale,radius}){result.fingerprint^=std::bit_cast<std::uint32_t>(value);result.fingerprint*=1099511628211ull;}
        result.fingerprint^=variant;result.fingerprint*=1099511628211ull;
    }
    result.reachedBudget=result.plants.size()==target;
    return result;
}

// V494純渲染子集；原作者、物理全足跡及族群生成器完全不改。
struct ShrubOpenCrown {
    Aevrynd::Mesh3D stems,leaves;
    float authorRadius{},authorHeight{},authorMinimumY{},authorMaximumY{};
    unsigned authorLeafCount{},renderLeafCount{},authorStemCount{};
    Aevrynd::MeshBounds3D actualStemBounds{},actualLeafBounds{};
    CanopyVolume::PlantLocalVolume actualRenderVolume{};
    std::vector<unsigned> sourceLeafIds,sourceVertexIds,sourceFaceOffsets;
    std::vector<ShrubMesh::Attachment> attachments;
};
namespace OpenCrownDetail {
using namespace Aevrynd;
inline void ValidateMesh(const Mesh3D& mesh) {
    if(!mesh.Valid()||mesh.vertices.size()>200000||mesh.indices.size()>600000)
        throw std::invalid_argument("Invalid bounded open-crown mesh");
    for(const auto& v:mesh.vertices) {
        const auto n=UnpackNormalSnorm8(v.normal);
        if(!Session::Finite({v.x,v.y,v.z})||!std::isfinite(v.u)||!std::isfinite(v.v)||
            !Session::Finite({n.x,n.y,n.z})||Length(Vec3{n.x,n.y,n.z})==0)
            throw std::invalid_argument("Invalid open-crown vertex");
    }
    for(std::size_t face=0;face<mesh.indices.size();face+=3) {
        const auto& a=mesh.vertices[mesh.indices[face]];
        const auto& b=mesh.vertices[mesh.indices[face+1]];
        const auto& c=mesh.vertices[mesh.indices[face+2]];
        const float area=Length(Cross(Vec3{b.x-a.x,b.y-a.y,b.z-a.z},Vec3{c.x-a.x,c.y-a.y,c.z-a.z}));
        if(!std::isfinite(area)||area==0)throw std::invalid_argument("Degenerate open-crown source triangle");
    }
}
}
inline ShrubOpenCrown CreateShrubOpenCrown(const ShrubMesh& source,unsigned variant) {
    using namespace Aevrynd;
    if(variant>=4||source.leafCount!=504||source.stemCount!=7||source.attachments.size()!=504)
        throw std::invalid_argument("Invalid original open-crown author contract");
    OpenCrownDetail::ValidateMesh(source.stems);OpenCrownDetail::ValidateMesh(source.leaves);
    CanopyVolume::PlantLocalVolume volume;
    float originalRadius{},originalHeight{};
    for(const auto* mesh:{&source.stems,&source.leaves})for(const auto& v:mesh->vertices) {
        originalRadius=(std::max)(originalRadius,std::hypot(v.x,v.z));originalHeight=(std::max)(originalHeight,v.y);
    }
    if(!CanopyVolume::MeasureLocalVolume(std::span<const Vertex3D>(source.stems.vertices),
        std::span<const Vertex3D>(source.leaves.vertices),volume)||source.radius!=originalRadius||
        source.minimumY!=volume.minimumY||source.maximumY!=volume.maximumY||source.height!=originalHeight||
        originalRadius>.75f||originalHeight>1.2f)
        throw std::invalid_argument("Original complete author footprint changed");
    // 真indices連通分量決定整葉所有權；不能用頂點數或面數固定步長猜葉片。
    const unsigned count=unsigned(source.leaves.vertices.size()),missing=~0u;
    std::vector<unsigned> parent(count),owner(count,missing);std::vector<bool> used(count);
    std::iota(parent.begin(),parent.end(),0u);
    const auto root=[&](unsigned i){while(parent[i]!=i){parent[i]=parent[parent[i]];i=parent[i];}return i;};
    for(std::size_t face=0;face<source.leaves.indices.size();face+=3) {
        const auto a=root(source.leaves.indices[face]);
        for(unsigned corner=0;corner<3;++corner) {
            const auto index=source.leaves.indices[face+corner];used[index]=true;parent[root(index)]=a;
        }
    }
    for(unsigned index=0;index<count;++index) {
        if(!used[index])throw std::invalid_argument("Orphan source leaf vertex");
        parent[index]=root(index);
    }
    unsigned components{};
    for(unsigned index=0;index<count;++index)components+=parent[index]==index;
    if(components!=504)throw std::invalid_argument("Source leaf membership is not 504 complete components");
    for(unsigned leaf=0;leaf<504;++leaf) {
        const auto& a=source.attachments[leaf];
        if(a.vertex>=count||!Session::Finite(a.centre)||!std::isfinite(a.branchRadius)||
            a.branchRadius<.001499f||a.branchRadius>.003801f||owner[parent[a.vertex]]!=missing)
            throw std::invalid_argument("Invalid or duplicate source leaf attachment");
        const auto& v=source.leaves.vertices[a.vertex];
        if(std::abs(Length(Vec3{v.x,v.y,v.z}-a.centre)-a.branchRadius)>1.e-6f)
            throw std::invalid_argument("Source whole leaf detached from its original twig");
        owner[parent[a.vertex]]=leaf;
    }
    for(unsigned index=0;index<count;++index)if(owner[parent[index]]==missing)
        throw std::invalid_argument("Complete leaf component has no unique attachment");
    // 九葉分組是原Build的側枝作者語義；用真錨點中心線／半徑驗證順序，不依賴mesh步長。
    std::array<bool,504> selected{};
    constexpr std::array<std::array<unsigned,4>,3> stagger{{{{0,2,5,8}},{{1,3,6,8}},{{0,3,5,7}}}};
    for(unsigned twig=0;twig<56;++twig) {
        const unsigned begin=twig*9;
        const auto first=source.attachments[begin].centre,last=source.attachments[begin+8].centre;
        if(Length(last-first)==0)throw std::invalid_argument("Collapsed original twig attachment line");
        for(unsigned leaf=0;leaf<9;++leaf) {
            const auto& a=source.attachments[begin+leaf];
            const float along=.2f+.1f*float(leaf);
            const float expected=.0038f+along*(.0015f-.0038f);
            if(Length(a.centre-(first+(last-first)*(float(leaf)/8.f)))>1.e-6f||
                a.branchRadius!=expected)
                throw std::invalid_argument("Original nine-leaf twig phase/order changed");
        }
        std::uint32_t hash=0x4f50454eu^(twig*0x9e3779b9u)^(variant*977u);
        hash^=hash>>16;hash*=0x7feb352du;hash^=hash>>15;
        for(const auto leaf:stagger[hash%3])selected[begin+leaf]=true;
    }
    ShrubOpenCrown result;
    result.stems=source.stems;
    result.authorRadius=source.radius;result.authorHeight=source.height;
    result.authorMinimumY=source.minimumY;result.authorMaximumY=source.maximumY;
    result.authorLeafCount=source.leafCount;result.authorStemCount=source.stemCount;
    std::vector<unsigned> remap(count,missing);
    // 所保留頂點／三角形依原順序複製，packed normal、UV、color及完整拓撲皆逐值原樣。
    for(unsigned index=0;index<count;++index)if(selected[owner[parent[index]]]) {
        remap[index]=unsigned(result.leaves.vertices.size());
        result.leaves.vertices.push_back(source.leaves.vertices[index]);result.sourceVertexIds.push_back(index);
    }
    for(unsigned face=0;face<source.leaves.indices.size();face+=3)if(selected[owner[parent[source.leaves.indices[face]]]]) {
        result.sourceFaceOffsets.push_back(face);
        for(unsigned corner=0;corner<3;++corner)result.leaves.indices.push_back(remap[source.leaves.indices[face+corner]]);
    }
    for(unsigned leaf=0;leaf<504;++leaf)if(selected[leaf]) {
        auto attachment=source.attachments[leaf];attachment.vertex=remap[attachment.vertex];
        result.attachments.push_back(attachment);result.sourceLeafIds.push_back(leaf);++result.renderLeafCount;
    }
    if(result.renderLeafCount!=224||!result.leaves.Valid())throw std::runtime_error("Open-crown whole-leaf selection failed");
    result.actualStemBounds=result.stems.Bounds();result.actualLeafBounds=result.leaves.Bounds();
    if(!CanopyVolume::MeasureLocalVolume(std::span<const Vertex3D>(result.stems.vertices),
        std::span<const Vertex3D>(result.leaves.vertices),result.actualRenderVolume))
        throw std::runtime_error("Invalid actual open-crown render volume");
    return result;
}
}
