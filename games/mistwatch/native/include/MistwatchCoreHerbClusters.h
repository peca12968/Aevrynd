#pragma once
#include "MistwatchRoadsideCover.h"
#include "MistwatchNettleAsset.h"
#include <Aevrynd/GroundConform3D.h>
#include <functional>
#include <optional>
#include <string_view>

namespace Mistwatch::CoreHerbClusters {
using namespace Aevrynd;
inline constexpr std::string_view Profile="core-herb-clusters-v1";
inline constexpr unsigned ClusterCount=12,PerCluster=16,Target=192,AttemptsPerCluster=512;
inline constexpr unsigned MaximumProposals=ClusterCount*AttemptsPerCluster;
inline constexpr unsigned AngularStrata=32,RadialStrata=16;
inline constexpr std::string_view SamplingProfile="stratified-polar-512-v1";
static_assert(AngularStrata*RadialStrata==AttemptsPerCluster);
inline constexpr float WindRadius=.19f,WindUp=.011f;
inline constexpr std::array<std::uint64_t,2> SourceFingerprints{5775871692697253043ull,3304028173848676471ull};
using Sources=std::array<const Mesh3D*,2>;
using GroundSampler=std::function<std::optional<double>(float,float)>;
struct AuthorLayout {std::array<Vec3,ClusterCount> centres;std::array<float,ClusterCount> rotationOffsets{};};
// 依固定完整世界CPU作者配置選出的局部布局；仍須整面支撐及原生提交，不代表美術通過。
inline constexpr AuthorLayout DefaultLayout{{{{-10.5f,0,0.f},{-10.5f,0,6.f},{-19.f,0,23.f},
    {-8.f,0,49.f},{-16.5f,0,68.f},{-19.25f,0,77.75f},{17.5f,0,-.5f},{14.f,0,11.f},{18.4f,0,24.f},
    {17.f,0,54.f},{18.f,0,65.f},{19.2f,0,78.5f}}},
    {{.5235987755982988f,0,0,0,1.5707963267948966f,-.916297857297023f,0,0,0,0,0,-.5235987755982988f}}};
enum class Rejection : unsigned {Domain,Route,Terrain,World,Vegetation,Spacing,Conform,SupportSelection,Count};
enum class RootWind : unsigned {PinnedV1=1};
inline constexpr std::string_view RootWindProfile="root-pinned-v1";
inline constexpr float RootPinnedHeight=.035f,RootWindFadeHeight=.35f;
struct ConformCounters {
    unsigned sampledVertices{},movedVertices{},affectedFaces{},rebuiltNormals{};
    float sourceMinimumY{},sourceMaximumY{};
    double maximumGroundDelta{};
};
struct Plant {
    Transform3D pose;Vec3 root{};
    unsigned variant{},cluster{};float radius{},requestedHeight{};
    Mesh3D derivedMesh;
    std::uint64_t sourceFingerprint{},derivedFingerprint{};
    ConformCounters conformCounters;
    RootWind rootWind{RootWind::PinnedV1};
    unsigned pinnedVertices{},changedWindVertices{};
    bool conformResult{},wholeSupportProven{};
};
using SelectionFilter=std::function<bool(const Plant&)>;
struct Cluster {Vec3 centre{};float authorRotation{};unsigned first{},count{},proposals{};
    std::array<unsigned,static_cast<unsigned>(Rejection::Count)> rejected{};};
struct Plan {
    std::vector<Plant> plants;
    std::array<Cluster,ClusterCount> clusters{};
    std::array<unsigned,static_cast<unsigned>(Rejection::Count)> rejected{};
    std::array<std::uint64_t,2> sourceFingerprints{};
    std::array<unsigned,2> variants{},sides{};
    unsigned proposals{},failedClusters{};
    RootWind rootWind{RootWind::PinnedV1};
    unsigned pinnedVertices{},changedWindVertices{};
    bool complete{},groupedWorldVerified{},wholeSupportProven{};
    std::uint64_t fingerprint{},terrainFingerprint{},worldFingerprint{},existingFingerprint{},authorFingerprint{};
};
namespace Detail {
inline void Need(bool yes,const char* reason){if(!yes)throw std::invalid_argument(reason);}
inline void Word(std::uint64_t& h,std::uint32_t v){for(unsigned b=0;b<4;++b){h^=(v>>(8*b))&255u;h*=1099511628211ull;}}
inline void Wide(std::uint64_t& h,std::uint64_t v){Word(h,static_cast<std::uint32_t>(v));Word(h,static_cast<std::uint32_t>(v>>32));}
inline void Float(std::uint64_t& h,float v){Word(h,std::bit_cast<std::uint32_t>(v));}
inline std::uint64_t LayoutFingerprint(const AuthorLayout& layout){std::uint64_t h=14695981039346656037ull;Word(h,0x43484c31u);
    for(unsigned i=0;i<ClusterCount;++i){const auto p=layout.centres[i];const float offset=layout.rotationOffsets[i];
        Need(Session::Finite(p)&&p.y==0&&std::abs(p.x)>=4&&std::abs(p.x)<=24&&p.z>=-6&&p.z<=84&&
            ((i<6)==(p.x<0))&&std::isfinite(offset)&&std::abs(offset)<=3.14159265359f,"CORE_HERB_AUTHOR_LAYOUT");
        for(float f:{p.x,p.y,p.z,offset})Float(h,f);
    }return h;
}
inline std::uint64_t MeshFingerprint(const Mesh3D& mesh){std::uint64_t h=14695981039346656037ull;
    Word(h,static_cast<unsigned>(mesh.vertices.size()));Word(h,static_cast<unsigned>(mesh.indices.size()));
    for(const auto& v:mesh.vertices){for(float f:{v.x,v.y,v.z,v.u,v.v})Float(h,f);Word(h,v.normal);Word(h,v.color);}
    for(auto i:mesh.indices)Word(h,i);return h;
}
inline void Boxes(std::uint64_t& h,std::span<const BoxCollider3D> boxes){Word(h,static_cast<unsigned>(boxes.size()));
    for(const auto& b:boxes)for(float f:{b.center.x,b.center.y,b.center.z,b.extents.x,b.extents.y,b.extents.z})Float(h,f);}
struct Proposal {
    unsigned variantSlot{},angularCell{},radialCell{};
    float height{},yaw{};
    double angularUnit{},radialUnit{},angle{},distance{};
};
// 每筆／每維度獨立的固定整數混合；拒絕某筆不推進其他筆的亂數狀態。
inline float ProposalUnit(unsigned cluster,unsigned attempt,unsigned dimension){
    Need(cluster<ClusterCount&&attempt<AttemptsPerCluster&&dimension<5,"CORE_HERB_PROPOSAL_INDEX");
    std::uint32_t v=0x43485231u^((cluster+1)*0x9e3779b9u)^((attempt+1)*0x85ebca6bu)^((dimension+1)*0xc2b2ae35u);
    v^=v>>16;v*=0x7feb352du;v^=v>>15;v*=0x846ca68bu;v^=v>>16;
    return float(v>>8)/16777216.f;
}
inline Proposal Propose(unsigned cluster,unsigned attempt){
    Need(cluster<ClusterCount&&attempt<AttemptsPerCluster,"CORE_HERB_PROPOSAL_INDEX");
    Proposal p;
    const unsigned row=attempt/AngularStrata;
    const unsigned reversed=((row&1u)<<3)|((row&2u)<<1)|((row&4u)>>1)|((row&8u)>>3);
    p.angularCell=((attempt%AngularStrata)*(2u*(cluster%16u)+1u)+cluster*7u)%AngularStrata;
    p.radialCell=(reversed+cluster*3u)%RadialStrata;
    // 留在各格內部，避免浮點四捨五入將最後一格誤算成下一格。
    p.angularUnit=(double(p.angularCell)+.01+.98*ProposalUnit(cluster,attempt,0))/AngularStrata;
    p.radialUnit=(double(p.radialCell)+.01+.98*ProposalUnit(cluster,attempt,1))/RadialStrata;
    p.angle=6.2831853071795864769*p.angularUnit;p.distance=6.*std::sqrt(p.radialUnit);
    p.variantSlot=static_cast<unsigned>(ProposalUnit(cluster,attempt,2)>=.5f);
    p.height=p.variantSlot==0?.3f+.35f*ProposalUnit(cluster,attempt,3):.18f+.17f*ProposalUnit(cluster,attempt,3);
    p.yaw=6.28318530718f*ProposalUnit(cluster,attempt,4);
    return p;
}
inline bool KnownConformRejection(std::string_view reason){
    return reason=="GROUND_CONFORM_SURFACE_MISSING"||reason=="GROUND_CONFORM_ROOT_NOT_ATTACHED"||
        reason=="GROUND_CONFORM_GROUND_DELTA"||reason=="GROUND_CONFORM_FACE_ORIENTATION"||reason=="GROUND_CONFORM_DERIVED_NORMAL";
}
inline CanopyVolume::PlantVolume Volume(const Plant& p,const CanopyVolume::PlantLocalVolume& actual){
    return {p.pose.position,CanopyVolume::ConservativeLocalVolume(actual.radius+WindRadius/p.pose.scale.x,
        actual.minimumY,actual.maximumY+WindUp/p.pose.scale.x),p.pose.scale.x,p.pose.rotation};
}
// 用完整 yaw 半徑與世界風包絡檢查核心作者範圍，不因縮小 instance 而縮小風。
inline bool Domain(Vec3 root,float radius){const float x=std::abs(root.x);
    return std::isfinite(radius)&&radius>0&&x-radius>=4.f&&x+radius<=24.f&&root.z-radius>=-6.f&&root.z+radius<=84.f;}
// 僅作者化衍生資產的反向風權重；來源相對高度不因坡面 conform 而改變。
inline std::uint32_t RootWindBits(std::uint32_t original,double height){
    const auto bits=original&0xff000000u;const float weight=UnpackNormalSnorm8(original).w;
    if(weight<0||height>=double(RootWindFadeHeight))return bits;
    const double t=std::clamp((height-double(RootPinnedHeight))/double(RootWindFadeHeight-RootPinnedHeight),0.,1.);
    const double wind=(1.-double(std::clamp(weight,0.f,1.f)))*t*t*(3.-2.*t);
    return PackNormalSnorm8({},static_cast<float>(1.-wind))&0xff000000u;
}
inline void AuthorRootWind(Plant& plant,const Mesh3D& source){
    for(std::size_t i=0;i<source.vertices.size();++i){const auto& raw=source.vertices[i];auto& derived=plant.derivedMesh.vertices[i];
        const double height=(double(raw.y)-plant.conformCounters.sourceMinimumY)*plant.pose.scale.x;
        const auto bits=RootWindBits(raw.normal,height);
        plant.pinnedVertices+=UnpackNormalSnorm8(raw.normal).w>=0&&height<=double(RootPinnedHeight);
        plant.changedWindVertices+=bits!=(raw.normal&0xff000000u);
        derived.normal=(derived.normal&0x00ffffffu)|bits;
    }
}
inline std::uint64_t Fingerprint(const Plan& p){std::uint64_t h=14695981039346656037ull;
    Word(h,3);for(auto value:p.sourceFingerprints)Wide(h,value);
    Wide(h,p.terrainFingerprint);Wide(h,p.worldFingerprint);Wide(h,p.existingFingerprint);
    Wide(h,p.authorFingerprint);
    Word(h,p.groupedWorldVerified);Word(h,p.proposals);Word(h,p.failedClusters);Word(h,p.complete);
    Word(h,static_cast<unsigned>(p.rootWind));Word(h,p.pinnedVertices);Word(h,p.changedWindVertices);
    for(auto count:p.rejected)Word(h,count);
    for(const auto& c:p.clusters){for(float f:{c.centre.x,c.centre.y,c.centre.z,c.authorRotation})Float(h,f);Word(h,c.first);Word(h,c.count);Word(h,c.proposals);
        for(auto count:c.rejected)Word(h,count);}
    Word(h,static_cast<unsigned>(p.plants.size()));
    for(const auto& a:p.plants){for(float f:{a.pose.position.x,a.pose.position.y,a.pose.position.z,a.pose.rotation.x,a.pose.rotation.y,
        a.pose.rotation.z,a.pose.scale.x,a.pose.scale.y,a.pose.scale.z,a.root.x,a.root.y,a.root.z,a.radius,a.requestedHeight})Float(h,f);
        Word(h,a.variant);Word(h,a.cluster);Wide(h,a.sourceFingerprint);Wide(h,a.derivedFingerprint);
        Word(h,static_cast<unsigned>(a.rootWind));Word(h,a.pinnedVertices);Word(h,a.changedWindVertices);
        const auto& c=a.conformCounters;Word(h,c.sampledVertices);Word(h,c.movedVertices);Word(h,c.affectedFaces);Word(h,c.rebuiltNormals);
        Float(h,c.sourceMinimumY);Float(h,c.sourceMaximumY);Wide(h,std::bit_cast<std::uint64_t>(c.maximumGroundDelta));}
    return h;
}
}
inline bool Resolve(bool capture,bool replay,std::wstring_view value){
    if(value.empty()||value==L"original")return false;
    Detail::Need(value==L"core-herb-clusters-v1","CORE_HERB_PROFILE");
    Detail::Need(capture!=replay,"CORE_HERB_REQUIRED_CONTEXT");return true;
}
// 十二個固定且錯落的世界群落，不依相機、不刪舊草，也不把 partial 補成假足額。
// sample 非空時必須來自真正最終提交地表；空值只使用 supplied HeightField 的同對角線三角高度。
// complete 只表示配置及逐頂點 conform 完成，不是完整三角面或風擺根部支撐證明。
inline Plan Create(const Sources& sources,const HeightField3D& terrain,
    std::span<const BoxCollider3D> before,std::span<const BoxCollider3D> after,
    const WoodlandBridgeWorld::ValidatedWorld* groupedWorld=nullptr,
    std::span<const BoxCollider3D> existingFullVolumes={},GroundSampler sample={},SelectionFilter selection={},
    const AuthorLayout& author=DefaultLayout){
    namespace D=Detail;Plan plan;std::array<NettleAssetBounds,2> assets;
    plan.authorFingerprint=D::LayoutFingerprint(author);
    std::array<CanopyVolume::PlantLocalVolume,2> sourceVolumes;
    for(unsigned slot=0;slot<2;++slot){const auto* mesh=sources[slot];const unsigned variant=slot+2;
        D::Need(mesh&&mesh->Valid()&&mesh->vertices.size()==NettleVertexCounts[variant]&&
            mesh->indices.size()==std::size_t(NettleTriangleCounts[variant])*3,"CORE_HERB_SOURCE_TOPOLOGY");
        assets[slot]=MeasureNettleAssetBounds(*mesh);plan.sourceFingerprints[slot]=D::MeshFingerprint(*mesh);
        D::Need(plan.sourceFingerprints[slot]==SourceFingerprints[slot],"CORE_HERB_SOURCE_IDENTITY");
        D::Need(CanopyVolume::MeasureLocalVolume(std::span<const Vertex3D>(mesh->vertices),sourceVolumes[slot]),"CORE_HERB_SOURCE_VOLUME");
    }
    D::Need(CanopyVolume::ValidWoodParts(before)&&CanopyVolume::ValidWoodParts(after),"CORE_HERB_WORLD_INVALID");
    D::Need(CanopyVolume::ValidWoodParts(existingFullVolumes),"CORE_HERB_EXISTING_INVALID");
    D::Need(before.empty()==after.empty(),"CORE_HERB_WORLD_STATES");
    if(groupedWorld){groupedWorld->RequireValid();D::Need(before.empty()&&after.empty(),"CORE_HERB_MIXED_WORLD_INPUT");plan.groupedWorldVerified=true;}
    const auto& grid=terrain.Settings();
    D::Need(grid.width==201&&grid.depth==241&&grid.cellSize==.5f&&grid.origin.x==-50.f&&grid.origin.y==0&&grid.origin.z==-20.f,
        "CORE_HERB_TERRAIN_LAYOUT");
    plan.terrainFingerprint=14695981039346656037ull;
    for(unsigned z=0;z<grid.depth;++z)for(unsigned x=0;x<grid.width;++x){const float y=terrain.Height(x,z);
        D::Need(std::isfinite(y),"CORE_HERB_TERRAIN_FINITE");D::Float(plan.terrainFingerprint,y);}
    plan.worldFingerprint=plan.existingFingerprint=14695981039346656037ull;
    if(groupedWorld){D::Boxes(plan.worldFingerprint,groupedWorld->Bodies(false));D::Boxes(plan.worldFingerprint,groupedWorld->Bodies(true));
        D::Wide(plan.worldFingerprint,WoodlandBridgeWorld::WoodIdentity.fingerprint);
    }else{D::Boxes(plan.worldFingerprint,before);D::Boxes(plan.worldFingerprint,after);}
    D::Boxes(plan.existingFingerprint,existingFullVolumes);
    if(!sample)sample=[&terrain](float x,float z)->std::optional<double>{if(!terrain.Contains(x,z))return{};return double(terrain.SampleHeight(x,z));};
    // 西北群落避開 z=62 的 relay 橫向通路；東北群落位於 generator 任務包絡外。
    // 只改作者提案中心，原半徑、間距、通路／風包絡與每群 512 次上限完全保留。
    plan.plants.reserve(Target);
    for(unsigned cluster=0;cluster<ClusterCount;++cluster){auto& patch=plan.clusters[cluster];patch.centre=author.centres[cluster];
        patch.authorRotation=float(cluster)*1.137f+author.rotationOffsets[cluster];
        const auto reject=[&](Rejection reason){const auto index=static_cast<unsigned>(reason);++plan.rejected[index];++patch.rejected[index];};
        const auto centreHeight=sample(patch.centre.x,patch.centre.z);if(centreHeight&&std::isfinite(*centreHeight))patch.centre.y=static_cast<float>(*centreHeight);
        patch.first=static_cast<unsigned>(plan.plants.size());
        for(unsigned attempt=0;attempt<AttemptsPerCluster&&patch.count<PerCluster;++attempt){++patch.proposals;++plan.proposals;
            const auto proposal=D::Propose(cluster,attempt);const unsigned slot=proposal.variantSlot;const auto& asset=assets[slot];
            const float height=proposal.height,scale=height/(asset.maximumY-asset.minimumY),yaw=proposal.yaw;
            const double angle=proposal.angle,distance=proposal.distance;
            // 真世界灌木與蕨類佔據狹窄作者區；擴大同群落尋位範圍，不刪舊植被或縮小風／碰撞體。
            // 橢圓方向隨 patch 改變，避免雙側六條整齊籬笆；原16株及512提案上限不變。
            const float rotation=patch.authorRotation;const double dx=distance*std::cos(angle),dz=.8*distance*std::sin(angle);
            Vec3 root{static_cast<float>(patch.centre.x+dx*std::cos(rotation)-dz*std::sin(rotation)),0,
                static_cast<float>(patch.centre.z+dx*std::sin(rotation)+dz*std::cos(rotation))};
            const auto localWind=CanopyVolume::ConservativeLocalVolume(sourceVolumes[slot].radius+WindRadius/scale,
                sourceVolumes[slot].minimumY,sourceVolumes[slot].maximumY+WindUp/scale);
            const float radius=std::nextafter(localWind.radius*scale,std::numeric_limits<float>::infinity());
            if(!D::Domain(root,radius)){reject(Rejection::Domain);continue;}
            if(!RoadsideCoverRouteClearance(root,radius)){reject(Rejection::Route);continue;}
            const auto heightAtRoot=sample(root.x,root.z);
            if(!heightAtRoot||!std::isfinite(*heightAtRoot)){reject(Rejection::Terrain);continue;}
            root.y=static_cast<float>(*heightAtRoot);
            if(!Session::Finite(root)||!ForestCoverFootprint(terrain,root,radius,{})){reject(Rejection::Terrain);continue;}
            bool close{};for(const auto& old:plan.plants)
                if(std::hypot(double(root.x)-old.root.x,double(root.z)-old.root.z)<.65*(double(radius)+old.radius)){close=true;break;}
            if(close){reject(Rejection::Spacing);continue;}
            Plant p;p.root=root;p.variant=slot+2;p.cluster=cluster;p.radius=radius;p.requestedHeight=height;
            // 此項只把原始最低根高對齊地表 datum；接地仍由每個來源根頂點個別形變，非抬高整株補洞。
            p.pose={{root.x,static_cast<float>(*heightAtRoot-double(asset.minimumY)*scale),root.z},{0,yaw,0},{scale,scale,scale}};
            GroundConformResult3D conform;
            try{conform=CreateGroundConformedMesh3D(*sources[slot],p.pose,sample);}
            catch(const std::invalid_argument& error){if(!D::KnownConformRejection(error.what()))throw;reject(Rejection::Conform);continue;}
            p.derivedMesh=std::move(conform.mesh);p.conformResult=true;
            p.conformCounters={conform.sampledVertices,conform.movedVertices,conform.affectedFaces,conform.rebuiltNormals,
                conform.sourceMinimumY,conform.sourceMaximumY,conform.maximumGroundDelta};
            D::AuthorRootWind(p,*sources[slot]);
            CanopyVolume::PlantLocalVolume local;D::Need(CanopyVolume::MeasureLocalVolume(std::span<const Vertex3D>(p.derivedMesh.vertices),local),
                "CORE_HERB_DERIVED_VOLUME");
            const auto volume=D::Volume(p,local);
            // 既有硬植被先檢查；明確撞上者不再消耗完整樹木精確求交。每株接受者仍須兩態世界皆清空。
            if(!CanopyVolume::ClearsCanopyWood(volume,existingFullVolumes)){reject(Rejection::Vegetation);continue;}
            const bool clearsBefore=groupedWorld?groupedWorld->Clears(volume,false):CanopyVolume::ClearsCanopyWood(volume,before);
            const bool clearsAfter=groupedWorld?groupedWorld->Clears(volume,true):CanopyVolume::ClearsCanopyWood(volume,after);
            if(!clearsBefore||!clearsAfter){reject(Rejection::World);continue;}
            p.sourceFingerprint=plan.sourceFingerprints[slot];p.derivedFingerprint=D::MeshFingerprint(p.derivedMesh);
            // 外部精確支撐驗證可以拒絕候選；拒絕仍耗同一有限提案額度，不冒充完整支撐證明。
            if(selection&&!selection(p)){reject(Rejection::SupportSelection);continue;}
            plan.pinnedVertices+=p.pinnedVertices;plan.changedWindVertices+=p.changedWindVertices;
            plan.plants.push_back(std::move(p));++patch.count;++plan.variants[slot];++plan.sides[root.x<0?0:1];
        }
        if(patch.count!=PerCluster)++plan.failedClusters;
    }
    plan.complete=plan.failedClusters==0&&plan.plants.size()==Target&&plan.sides[0]==Target/2&&plan.sides[1]==Target/2;
    plan.fingerprint=D::Fingerprint(plan);return plan;
}
}
