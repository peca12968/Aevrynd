#pragma once
#include "MistwatchRouteGrassCook.h"
#include <Aevrynd/BoundedBinaryReader3D.h>

namespace Mistwatch::RouteGrassCookLoad {
using namespace Aevrynd;
namespace D=CoreHerbClusters::Detail;
namespace K=RouteGrassCook;
inline constexpr std::string_view Profile="complete-route-cooked-source-foot-v1";
struct Approval {std::array<std::byte,32> bundle{},build{};};
inline bool Nonzero(const std::array<std::byte,32>& value){for(auto b:value)if(b!=std::byte{})return true;return false;}
inline void RequireApproval(const Approval& approval){D::Need(Nonzero(approval.bundle)&&Nonzero(approval.build),"ROUTE_GRASS_LOAD_NO_APPROVAL");}
inline float Float(BoundedBinaryReader3D& in){const float value=in.Float();D::Need(std::isfinite(value),"ROUTE_GRASS_LOAD_NONFINITE");return value;}
inline double Double(BoundedBinaryReader3D& in){const double value=in.Double();D::Need(std::isfinite(value),"ROUTE_GRASS_LOAD_NONFINITE");return value;}
inline bool Boolean(BoundedBinaryReader3D& in){const auto value=in.Word();D::Need(value<=1,"ROUTE_GRASS_LOAD_BOOLEAN");return value!=0;}
inline Vec3 Vector(BoundedBinaryReader3D& in){const float x=Float(in),y=Float(in),z=Float(in);return{x,y,z};}
inline std::uint64_t Fingerprint(const RouteGrassPlan::Plan& plan){
    std::uint64_t h=14695981039346656037ull;D::Word(h,2);D::Wide(h,RouteGrassContext::Fingerprint(plan.context));D::Wide(h,plan.surfaceFingerprint);
    for(const auto& a:plan.instances){const auto& p=a.plant;D::Word(h,a.ordinal);D::Word(h,a.bin);D::Wide(h,p.sourceFingerprint);D::Wide(h,p.derivedFingerprint);
        for(float f:{p.pose.position.x,p.pose.position.y,p.pose.position.z,p.pose.rotation.y,p.pose.scale.x,p.root.x,p.root.y,p.root.z,p.radius})D::Float(h,f);}
    return h;
}
inline void ValidateRecord(const RouteGrassPlan::Instance& a,unsigned slot,const RouteGrassContext::Seal& context,std::uint64_t surface){
    const auto& p=a.plant;const auto& q=a.proof;const auto& c=q.contact;const auto& stats=p.conformCounters;
    D::Need(slot<128&&p.variant<5,"ROUTE_GRASS_LOAD_VARIANT_WIND");
    LandscapeSpeciesDetail::ValidateMesh(p.derivedMesh,false);const auto nv=p.derivedMesh.vertices.size(),ni=p.derivedMesh.indices.size();
    const unsigned bin=(a.ordinal/96)*8/168+(a.ordinal%2?8u:0u);
    D::Need(a.ordinal==RouteGrassAuthor::Ordinals[slot]&&a.bin==bin&&nv==GrassVertexCounts[p.variant]&&ni==3u*GrassTriangleCounts[p.variant]&&
        p.sourceFingerprint==RouteGrassSupport::SourceFingerprints[p.variant]&&p.sourceFingerprint==context.words[p.variant]&&
        p.sourceFingerprint==q.sourceFingerprint&&p.derivedFingerprint==q.derivedFingerprint&&q.surfaceFingerprint==surface&&
        D::MeshFingerprint(p.derivedMesh)==p.derivedFingerprint,"ROUTE_GRASS_LOAD_PLANT_IDENTITY");
    D::Need(Session::Finite(p.pose.position)&&Session::Finite(p.pose.rotation)&&Session::Finite(p.pose.scale)&&Session::Finite(p.root)&&
        p.pose.rotation.x==0&&p.pose.rotation.z==0&&p.pose.scale.x>0&&p.pose.scale.x==p.pose.scale.y&&p.pose.scale.x==p.pose.scale.z&&
        std::isfinite(p.radius)&&p.radius>0&&p.radius<=4&&p.requestedHeight==0&&p.conformResult&&!p.wholeSupportProven&&
        p.rootWind==CoreHerbClusters::RootWind::PinnedV1,"ROUTE_GRASS_LOAD_POSE");
    D::Need(c.safe&&q.allFaces==ni/3&&std::uint64_t(c.evaluatedFaces)+c.skippedFaces==q.allFaces&&q.sourceFootPieces>0&&
        q.sourceFootPieces==c.footPieces&&q.windPinnedVertices==p.pinnedVertices&&q.windPinnedVertices>0&&q.windPinnedVertices<=nv&&
        c.footprintFaces>0&&std::isfinite(c.minimumNormalY)&&c.minimumNormalY>=OuterUnderstory::SlopeLimit&&
        std::isfinite(c.maximumHeightDelta)&&c.maximumHeightDelta>=0&&c.maximumHeightDelta<=OuterUnderstory::HeightLimit&&
        std::isfinite(c.minimumGapBound)&&c.minimumGapBound>=-OuterUnderstory::ContactLimit&&
        std::isfinite(c.maximumFootGapBound)&&c.maximumFootGapBound<=OuterUnderstory::ContactLimit&&
        std::isfinite(stats.sourceMinimumY)&&std::isfinite(stats.sourceMaximumY)&&stats.sourceMaximumY>stats.sourceMinimumY&&
        std::isfinite(stats.maximumGroundDelta)&&stats.maximumGroundDelta>=0&&stats.maximumGroundDelta<=.24f&&
        stats.sampledVertices<=nv&&stats.movedVertices<=stats.sampledVertices&&stats.affectedFaces<=ni/3&&stats.rebuiltNormals<=nv&&p.changedWindVertices<=nv,
        "ROUTE_GRASS_LOAD_PROOF");
}
inline RouteGrassPlan::Plan Decode(std::span<const std::byte> bytes,const Approval& approval){
    RequireApproval(approval);D::Need(bytes.size()>=192&&bytes.size()<=K::MaximumBytes,"ROUTE_GRASS_LOAD_SIZE");
    // approved SHA來自編譯端，不從套件內的自述expected hash取得。
    D::Need(K::Hash(bytes)==approval.bundle,"ROUTE_GRASS_LOAD_BUNDLE_IDENTITY");BoundedBinaryReader3D in(bytes);
    D::Need(in.Word()==K::Magic&&in.Word()==K::Schema&&in.Word()==128&&in.Word()==RouteGrassContext::Schema,"ROUTE_GRASS_LOAD_SCHEMA");
    const auto build=in.Take(32);D::Need(std::equal(build.begin(),build.end(),approval.build.begin()),"ROUTE_GRASS_LOAD_BUILD_IDENTITY");
    RouteGrassPlan::Plan result;for(auto& word:result.context.words)word=in.Wide();(void)RouteGrassContext::Fingerprint(result.context);
    result.fingerprint=in.Wide();result.surfaceFingerprint=in.Wide();D::Need(result.surfaceFingerprint==result.context.words[5],"ROUTE_GRASS_LOAD_SURFACE_IDENTITY");
    result.instances.reserve(128);
    for(unsigned i=0;i<128;++i){RouteGrassPlan::Instance a;a.ordinal=in.Word();a.bin=in.Word();auto& p=a.plant;auto& q=a.proof;auto& c=q.contact;
        p.variant=in.Word();const auto wind=in.Word();D::Need(p.variant<5&&wind==1,"ROUTE_GRASS_LOAD_VARIANT_WIND");p.rootWind=CoreHerbClusters::RootWind::PinnedV1;
        q.sourceFingerprint=in.Wide();q.derivedFingerprint=in.Wide();q.surfaceFingerprint=in.Wide();
        p.sourceFingerprint=q.sourceFingerprint;p.derivedFingerprint=q.derivedFingerprint;
        p.pose.position=Vector(in);p.pose.rotation=Vector(in);p.pose.scale=Vector(in);p.root=Vector(in);p.radius=Float(in);p.requestedHeight=Float(in);
        auto& stats=p.conformCounters;stats.sampledVertices=in.Word();stats.movedVertices=in.Word();stats.affectedFaces=in.Word();stats.rebuiltNormals=in.Word();
        stats.sourceMinimumY=Float(in);stats.sourceMaximumY=Float(in);stats.maximumGroundDelta=Double(in);
        p.pinnedVertices=in.Word();p.changedWindVertices=in.Word();p.conformResult=Boolean(in);p.wholeSupportProven=Boolean(in);
        q.allFaces=in.Word();q.sourceFootPieces=in.Word();q.windPinnedVertices=in.Word();c.safe=Boolean(in);const auto rejection=in.Word();
        D::Need(rejection<static_cast<unsigned>(OuterUnderstory::Reject::Count),"ROUTE_GRASS_LOAD_REJECTION");c.rejection=static_cast<OuterUnderstory::Reject>(rejection);
        c.restingFootObserved=Boolean(in);c.restingFootAboveLimit=Boolean(in);c.maximumRestingFootGap=Float(in);c.minimumNormalY=Float(in);
        c.maximumHeightDelta=Float(in);c.minimumGapBound=Float(in);c.maximumFootGapBound=Float(in);
        c.footprintFaces=in.Word();c.evaluatedFaces=in.Word();c.skippedFaces=in.Word();c.footPieces=in.Word();
        c.cachedGroundFaces=in.Wide();c.aabbRejected=in.Wide();c.exactOverlayFaces=in.Wide();c.groundHeightPlanes=in.Wide();c.sourceHeightPlanes=in.Wide();c.cachedHeightEvaluations=in.Wide();
        D::Need(in.Word()==0x334d4541u&&in.Word()==1,"ROUTE_GRASS_LOAD_MESH_SCHEMA");const auto nv=in.Word(),ni=in.Word();
        D::Need(nv==GrassVertexCounts[p.variant]&&ni==3u*GrassTriangleCounts[p.variant]&&std::uint64_t(nv)*28+std::uint64_t(ni)*4<=in.Remaining(),"ROUTE_GRASS_LOAD_MESH_BUDGET");
        p.derivedMesh.vertices.reserve(nv);p.derivedMesh.indices.reserve(ni);
        for(unsigned j=0;j<nv;++j){const float x=Float(in),y=Float(in),z=Float(in);const auto normal=in.Word(),color=in.Word();const float u=Float(in),v=Float(in);
            p.derivedMesh.vertices.push_back({x,y,z,normal,color,u,v});}
        for(unsigned j=0;j<ni;++j){const auto index=in.Word();D::Need(index<nv,"ROUTE_GRASS_LOAD_MESH_INDEX");p.derivedMesh.indices.push_back(index);}
        LandscapeSpeciesDetail::ValidateMesh(p.derivedMesh,false);
        const unsigned expectedBin=(a.ordinal/96)*8/168+(a.ordinal%2?8u:0u);
        D::Need(a.ordinal==RouteGrassAuthor::Ordinals[i]&&a.bin==expectedBin&&p.sourceFingerprint==RouteGrassSupport::SourceFingerprints[p.variant]&&
            p.sourceFingerprint==result.context.words[p.variant]&&q.surfaceFingerprint==result.surfaceFingerprint&&
            D::MeshFingerprint(p.derivedMesh)==p.derivedFingerprint,"ROUTE_GRASS_LOAD_PLANT_IDENTITY");
        D::Need(p.pose.rotation.x==0&&p.pose.rotation.z==0&&p.pose.scale.x>0&&p.pose.scale.x==p.pose.scale.y&&p.pose.scale.x==p.pose.scale.z&&
            p.radius>0&&p.radius<=4&&p.requestedHeight==0&&p.conformResult&&!p.wholeSupportProven,"ROUTE_GRASS_LOAD_POSE");
        D::Need(c.safe&&q.allFaces==ni/3&&std::uint64_t(c.evaluatedFaces)+c.skippedFaces==q.allFaces&&q.sourceFootPieces>0&&
            q.sourceFootPieces==c.footPieces&&q.windPinnedVertices==p.pinnedVertices&&q.windPinnedVertices>0&&q.windPinnedVertices<=nv&&
            c.footprintFaces>0&&c.minimumNormalY>=OuterUnderstory::SlopeLimit&&c.maximumHeightDelta>=0&&c.maximumHeightDelta<=OuterUnderstory::HeightLimit&&
            c.minimumGapBound>=-OuterUnderstory::ContactLimit&&c.maximumFootGapBound<=OuterUnderstory::ContactLimit&&
            stats.sourceMaximumY>stats.sourceMinimumY&&stats.maximumGroundDelta>=0&&stats.maximumGroundDelta<=.24f&&
            stats.sampledVertices<=nv&&stats.movedVertices<=stats.sampledVertices&&stats.affectedFaces<=ni/3&&stats.rebuiltNormals<=nv&&
            p.changedWindVertices<=nv,"ROUTE_GRASS_LOAD_PROOF");
        ValidateRecord(a,i,result.context,result.surfaceFingerprint);result.instances.push_back(std::move(a));
    }
    D::Need(in.Remaining()==0,"ROUTE_GRASS_LOAD_TRAILING");D::Need(Fingerprint(result)==result.fingerprint,"ROUTE_GRASS_LOAD_PLAN_IDENTITY");return result;
}
inline bool Same(float a,float b){return std::bit_cast<std::uint32_t>(a)==std::bit_cast<std::uint32_t>(b);}
inline void ValidatePlant(const RouteGrassPlan::Instance& a,const Mesh3D& source,const RouteGrassAuthor::Proposal& author,float groundY){
    const auto& p=a.plant;const auto& q=a.proof;namespace GC=GroundConformDetail3D;
    D::Need(source.Valid()&&p.derivedMesh.Valid()&&source.indices==p.derivedMesh.indices&&source.vertices.size()==p.derivedMesh.vertices.size(),
        "ROUTE_GRASS_ADMIT_SOURCE_GEOMETRY");
    LandscapeSpeciesDetail::ValidateMesh(source,false);LandscapeSpeciesDetail::ValidateMesh(p.derivedMesh,false);
    D::Need(a.ordinal==author.ordinal&&a.bin==author.bin&&p.variant==author.variant&&p.sourceFingerprint==D::MeshFingerprint(source),"ROUTE_GRASS_ADMIT_AUTHOR");
    float minimum=source.vertices.front().y,maximum=minimum;
    for(const auto& v:source.vertices){minimum=(std::min)(minimum,v.y);maximum=(std::max)(maximum,v.y);}
    const auto bounds=ForestCoverAssetFromMesh(source);const float radius=std::nextafter(bounds.radius*author.scale+CoreHerbClusters::WindRadius,std::numeric_limits<float>::infinity());
    const Transform3D expected{{author.root.x,groundY-minimum*author.scale,author.root.z},{0,author.root.x+author.root.z,0},{author.scale,author.scale,author.scale}};
    const auto root=TransformPoint(expected.Matrix(),{0,minimum,0});
    D::Need(Session::Finite(root)&&std::abs(double(groundY)-author.root.y)<.000002&&
        Same(p.pose.position.x,expected.position.x)&&Same(p.pose.position.y,expected.position.y)&&Same(p.pose.position.z,expected.position.z)&&
        Same(p.pose.rotation.y,expected.rotation.y)&&Same(p.pose.scale.x,author.scale)&&Same(p.root.x,root.x)&&Same(p.root.y,root.y)&&Same(p.root.z,root.z)&&Same(p.radius,radius),
        "ROUTE_GRASS_ADMIT_POSE");
    const auto& stats=p.conformCounters;
    D::Need(Same(stats.sourceMinimumY,minimum)&&Same(stats.sourceMaximumY,maximum)&&source.indices==p.derivedMesh.indices&&source.vertices.size()==p.derivedMesh.vertices.size(),"ROUTE_GRASS_ADMIT_SOURCE_GEOMETRY");
    std::vector<bool> moved(source.vertices.size()),affected(source.vertices.size());std::vector<GC::Normal> normals(source.vertices.size());
    std::map<std::array<std::uint32_t,3>,std::uint32_t> welded;unsigned sampled{},movedCount{},pinned{},changed{},faces{},rebuilt{};
    for(unsigned i=0;i<source.vertices.size();++i){const auto& s=source.vertices[i];const auto& v=p.derivedMesh.vertices[i];const double height=(double(s.y)-minimum)*p.pose.scale.x;
        D::Need(Same(s.x,v.x)&&Same(s.z,v.z)&&Same(s.u,v.u)&&Same(s.v,v.v)&&s.color==v.color,"ROUTE_GRASS_ADMIT_MATERIAL_XZ");
        const auto expectedW=D::RootWindBits(s.normal,height);D::Need((v.normal&0xff000000u)==expectedW,"ROUTE_GRASS_ADMIT_WIND");
        pinned+=UnpackNormalSnorm8(s.normal).w>=0&&height<=double(CoreHerbClusters::RootPinnedHeight);changed+=expectedW!=(s.normal&0xff000000u);
        sampled+=height<double(.35f);moved[i]=!Same(s.y,v.y);movedCount+=moved[i];
        D::Need(height<double(.35f)||!moved[i],"ROUTE_GRASS_ADMIT_UPPER_VERTEX");
        const std::array<std::uint32_t,3> key{GC::Word(s.x),GC::Word(s.y),GC::Word(s.z)};
        const auto [it,inserted]=welded.emplace(key,GC::Word(v.y));D::Need(inserted||it->second==GC::Word(v.y),"ROUTE_GRASS_ADMIT_SPLIT_WELD");
    }
    for(unsigned f=0;f<source.indices.size();f+=3){const auto i=source.indices[f],j=source.indices[f+1],k=source.indices[f+2];
        const auto old=GC::Cross(source.vertices[i],source.vertices[j],source.vertices[k]);const auto fresh=GC::Cross(p.derivedMesh.vertices[i],p.derivedMesh.vertices[j],p.derivedMesh.vertices[k]);
        D::Need(std::isfinite(GC::Dot(fresh,fresh))&&GC::Dot(fresh,fresh)>0&&GC::Dot(old,fresh)>0,"ROUTE_GRASS_ADMIT_FACE_ORIENTATION");
        for(auto id:{i,j,k}){auto& n=normals[id];n.x+=fresh.x;n.y+=fresh.y;n.z+=fresh.z;}
        if(moved[i]||moved[j]||moved[k]){++faces;affected[i]=affected[j]=affected[k]=true;}
    }
    for(unsigned i=0;i<source.vertices.size();++i){auto bits=source.vertices[i].normal&0x00ffffffu;
        if(affected[i]){const auto n=normals[i];const double length=std::sqrt(GC::Dot(n,n));D::Need(std::isfinite(length)&&length>0,"ROUTE_GRASS_ADMIT_NORMAL");
            bits=PackNormalSnorm8({float(n.x/length),float(n.y/length),float(n.z/length)})&0x00ffffffu;++rebuilt;}
        D::Need((p.derivedMesh.vertices[i].normal&0x00ffffffu)==bits,"ROUTE_GRASS_ADMIT_NORMAL_IDENTITY");
    }
    D::Need(sampled==stats.sampledVertices&&movedCount==stats.movedVertices&&faces==stats.affectedFaces&&rebuilt==stats.rebuiltNormals&&
        pinned==p.pinnedVertices&&pinned==q.windPinnedVertices&&changed==p.changedWindVertices,"ROUTE_GRASS_ADMIT_COUNTERS");
}
inline void RequireApprovedPlan(const RouteGrassPlan::Plan& plan,const Approval& approval){
    RequireApproval(approval);
    // Plan為可變值型別；重新編碼全部欄位可拒絕Decode之後的proof/geometry改寫，而不重跑exact overlay。
    const auto encoded=K::Encode(plan,approval.build);D::Need(K::Hash(encoded.Data())==approval.bundle,"ROUTE_GRASS_ADMIT_APPROVED_BYTES");
}
inline void Admit(const RouteGrassPlan::Plan& plan,const std::array<const Mesh3D*,5>& sources,const Mesh3D& actualGround,
    const HeightField3D& terrain,const WoodlandBridgeWorld::ValidatedWorld& world,std::span<const BoxCollider3D> retained,std::span<const UnderstoryRetention::Row> soft,
    const Approval& approval){
    RequireApprovedPlan(plan,approval);
    RouteGrassContext::RequireSame(plan.context,RouteGrassContext::Create(sources,actualGround,terrain,world,retained,soft));
    D::Need(plan.instances.size()==128&&Fingerprint(plan)==plan.fingerprint,"ROUTE_GRASS_ADMIT_PLAN");
    const RouteGrassSupport::Ground ground(actualGround);const auto authors=RouteGrassAuthor::Create(terrain);
    for(unsigned i=0;i<128;++i){const auto& a=plan.instances[i];const auto& p=a.plant;
        ValidateRecord(a,i,plan.context,plan.surfaceFingerprint);
        const auto y=ground.Surface().At({authors[i].root.x,0,authors[i].root.z}).Approximate();
        ValidatePlant(a,*sources[authors[i].variant],authors[i],static_cast<float>(y));
        CanopyVolume::PlantLocalVolume local;D::Need(CanopyVolume::MeasureLocalVolume(std::span<const Vertex3D>(p.derivedMesh.vertices),local),"ROUTE_GRASS_ADMIT_VOLUME");
        const auto volume=D::Volume(p,local);
        D::Need(RoadsideCoverRouteClearance(p.root,p.radius)&&ForestCoverFootprint(terrain,p.root,p.radius,{})&&world.Clears(volume,false)&&world.Clears(volume,true)&&
            CanopyVolume::ClearsCanopyWood(volume,retained),"ROUTE_GRASS_ADMIT_WORLD");
        for(const auto& old:soft)D::Need(std::hypot(double(p.root.x)-old.anchor.x,double(p.root.z)-old.anchor.z)>=.65*(double(p.radius)+old.footprintRadius),"ROUTE_GRASS_ADMIT_EXISTING_SPACING");
        for(unsigned j=0;j<i;++j){const auto& old=plan.instances[j].plant;D::Need(std::hypot(double(p.root.x)-old.root.x,double(p.root.z)-old.root.z)>=.65*(double(p.radius)+old.radius),"ROUTE_GRASS_ADMIT_NEW_SPACING");}
    }
}
inline std::vector<std::byte> Read(const std::filesystem::path& path){
    D::Need(path.is_absolute(),"ROUTE_GRASS_LOAD_PATH");std::ifstream file(path,std::ios::binary|std::ios::ate);D::Need(bool(file),"ROUTE_GRASS_LOAD_OPEN");
    const auto length=file.tellg();D::Need(length>=192&&length<=static_cast<std::streamoff>(K::MaximumBytes),"ROUTE_GRASS_LOAD_SIZE");file.seekg(0);
    std::vector<std::byte> bytes(static_cast<std::size_t>(length));file.read(reinterpret_cast<char*>(bytes.data()),length);
    D::Need(bool(file)&&file.peek()==std::char_traits<char>::eof(),"ROUTE_GRASS_LOAD_READ");return bytes;
}
}
