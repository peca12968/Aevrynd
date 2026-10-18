#pragma once
#include "MistwatchEntranceWorldAuthor.h"
#include "MistwatchWoodlandCanonical.h"
#include <Aevrynd/WorldDeltaPersistence3D.h>
#include <cstring>
#include <iomanip>
#include <sstream>

namespace Mistwatch::WoodlandCanopy {
inline constexpr const char* Profile="woodland-canopy-author-v2";
inline constexpr unsigned OriginalCount=8,TotalCount=12,MaximumFernMoves=32,FernProposals=64;
// 各株仍使用原來 15cm、10000 packed parts 的算法；此為十二株獨立有界集合。
inline constexpr unsigned MaximumWoodParts=TotalCount*10000;
inline constexpr std::array<EntranceCanopyAnchor,4> Anchors{{
    {-13.5f,47.5f,.7f,1.04f},{-16.5f,55.f,2.5f,1.07f},
    {13.f,53.f,4.1f,1.05f},{-16.f,72.f,1.6f,1.06f}}};
enum class WithdrawalReason : unsigned { FullEnvelopeOccupied };
struct Withdrawal {
    EntranceRole role{};unsigned sourceId{},anchor{},patch{};
    std::array<std::uint32_t,9> pose{};
    std::array<std::uint32_t,6> box{};
    WithdrawalReason reason{WithdrawalReason::FullEnvelopeOccupied};
    bool operator==(const Withdrawal&)const=default;
};
struct FernMove {
    unsigned sourceId{},anchor{},proposal{};
    std::array<std::uint32_t,9> before{},after{};
    bool operator==(const FernMove&)const=default;
};
struct Recipe {
    std::uint64_t inputIdentity{};
    std::vector<Withdrawal> withdrawals;
    std::vector<FernMove> fernMoves;
};
struct Result {
    EntranceWorldAuthor author;
    CanopyPopulation canopy;
    std::array<CanopyWoodProxy,TotalCount> proxies;
    std::vector<BoxCollider3D> wood;
    Recipe recipe;
    EntranceCanopyMetadata metadata;
    std::uint64_t inputIdentity{},outputIdentity{},woodFingerprint{};
    bool complete{};
};
namespace Detail {
inline void Need(bool ok,const char* gate,unsigned id=0){if(!ok)throw std::runtime_error(
    "WOODLAND_CANOPY_FIRST_DIVERGENCE id="+std::to_string(id)+" gate="+gate);}
using EntranceWorldDetail::PoseWords;
using EntranceWorldDetail::BoxWords;
struct Hash {
    std::uint64_t value{14695981039346656037ull};
    void Word(std::uint32_t w){for(unsigned i=0;i<4;++i){value^=(w>>(i*8))&255u;value*=1099511628211ull;}}
    void Wide(std::uint64_t w){Word(static_cast<std::uint32_t>(w));Word(static_cast<std::uint32_t>(w>>32));}
    template<std::size_t N>void Words(const std::array<std::uint32_t,N>& a){for(auto w:a)Word(w);}
    void Float(float f){Need(std::isfinite(f),"nonfinite");Word(std::bit_cast<std::uint32_t>(f));}
    void Mesh(const Mesh3D& m){Need(m.Valid()&&m.vertices.size()<=600000&&m.indices.size()<=1800000,"mesh-domain");
        Wide(m.vertices.size());Wide(m.indices.size());for(const auto& v:m.vertices){
            for(float f:{v.x,v.y,v.z,v.u,v.v})Float(f);Word(v.color);Word(v.normal);}
        for(auto i:m.indices){Need(i<m.vertices.size(),"mesh-index");Word(i);}}
};
inline std::uint64_t PlacementIdentity(const CanopyPopulation& c){std::uint64_t h=14695981039346656037ull;
    for(const auto& p:c.plants)for(float f:{p.pose.position.x,p.pose.position.y,p.pose.position.z,
        p.pose.rotation.y,p.pose.scale.x,p.crownRadius,p.rootRadius}){
        Need(std::isfinite(f),"nonfinite-canopy");h^=std::bit_cast<std::uint32_t>(f);h*=1099511628211ull;}return h;}
inline std::uint64_t Identity(const HeightField3D& terrain,const EntranceWorldAuthor& a,const CanopyPopulation& c,
    const std::array<FernColonyAsset,4>& ferns){
    Hash h;h.Word(0x57434132u);h.Word(OriginalCount);h.Word(TotalCount);h.Word(MaximumFernMoves);h.Word(FernProposals);
    // 固定落點是配方輸入的一部分；改位置後，舊配方不得沿用同一 identity。
    for(unsigned i=0;i<Anchors.size();++i){h.Word(i);const auto p=Anchors[i];h.Float(p.x);h.Float(p.z);h.Float(p.yaw);h.Float(p.scale);}
    h.Word(a.applied);h.Word(static_cast<unsigned>(a.fallenProfile));h.Wide(a.sourceFirFingerprint);
    h.Wide(a.sourceMiddleFingerprint);h.Wide(a.fingerprint);
    const auto& s=terrain.Settings();Need(s.width>=2&&s.depth>=2&&std::uint64_t(s.width)*s.depth<=4194304,"terrain-budget");
    h.Word(s.width);h.Word(s.depth);h.Float(s.cellSize);
    h.Float(s.origin.x);h.Float(s.origin.y);h.Float(s.origin.z);
    for(unsigned z=0;z<s.depth;++z)for(unsigned x=0;x<s.width;++x)h.Float(terrain.Height(x,z));
    for(const auto* ids:{&a.firSourceIds,&a.propSourceIds,&a.saplingSourceIds,&a.middleSourceIds}){
        h.Wide(ids->size());for(auto id:*ids)h.Word(id);}
    h.Wide(a.firs.fingerprint);h.Word(a.firs.reachedBudget);
    for(const auto& p:a.firs.firs){h.Words(PoseWords(p.render));h.Words(BoxWords(p.collider));h.Word(p.patch);
        h.Float(p.root.x);h.Float(p.root.y);h.Float(p.root.z);}
    for(auto r:a.firs.roots){h.Float(r.x);h.Float(r.y);h.Float(r.z);}
    for(const auto& p:a.props){h.Word(p.mesh);h.Words(PoseWords(p.transform));h.Words(BoxWords(p.collider));}
    for(const auto& p:a.understory.ferns)h.Words(PoseWords(p));
    for(const auto& p:a.understory.saplings)h.Words(PoseWords(p));
    for(const auto* trees:{&a.broadleaf,&a.middle.trees})for(const auto& p:*trees){h.Words(PoseWords(p.transform));h.Words(BoxWords(p.trunk));}
    for(const auto& b:a.mission)h.Words(BoxWords(b));
    h.Word(c.attempts);h.Word(c.clearanceRejected);h.Word(c.rootRejected);h.Word(c.spacingRejected);h.Word(c.meshRejected);
    h.Wide(c.fingerprint);h.Wide(c.groundChecks);h.Word(c.reachedBudget);h.Wide(c.plants.size());
    for(const auto& p:c.plants){h.Words(PoseWords(p.pose));h.Words(BoxWords(p.lower));h.Float(p.crownRadius);h.Float(p.rootRadius);h.Mesh(p.rootedTrunk);}
    h.Word(4);for(const auto& f:ferns){h.Float(f.radius);h.Float(f.height);h.Float(f.minimumY);h.Word(f.vertices);h.Word(f.triangles);}
    return h.value;
}
inline std::vector<std::uint64_t> CanonicalSourceWords(const EntranceWorldAuthor& a,const CanopyPopulation& c,
    const std::array<FernColonyAsset,4>& ferns){
    std::vector<std::uint64_t> out;out.reserve(Canonical::Fields.size());
    const auto put=[&](auto v){out.push_back(static_cast<std::uint64_t>(v));};
    const auto pose=[&](const Transform3D& p){for(auto v:PoseWords(p))put(v);};
    const auto box=[&](const BoxCollider3D& b){for(auto v:BoxWords(b))put(v);};
    put(0x57435332u);put(a.applied);put(static_cast<unsigned>(a.fallenProfile));put(a.sourceFirFingerprint);put(a.sourceMiddleFingerprint);
    put(a.fingerprint);put(a.firs.fingerprint);put(a.firs.reachedBudget);put(a.middle.reachedBudget);
    put(a.firs.firs.size());for(unsigned i=0;i<a.firs.firs.size();++i){const auto& p=a.firs.firs[i];put(a.firSourceIds[i]);pose(p.render);box(p.collider);
        for(float v:{p.root.x,p.root.y,p.root.z})put(std::bit_cast<std::uint32_t>(v));
        for(float v:{a.firs.roots[i].x,a.firs.roots[i].y,a.firs.roots[i].z})put(std::bit_cast<std::uint32_t>(v));put(p.patch);}
    put(a.props.size());for(unsigned i=0;i<a.props.size();++i){const auto& p=a.props[i];put(a.propSourceIds[i]);put(p.mesh);pose(p.transform);box(p.collider);}
    put(a.understory.saplings.size());for(unsigned i=0;i<a.understory.saplings.size();++i){put(a.saplingSourceIds[i]);pose(a.understory.saplings[i]);}
    put(a.middle.trees.size());for(unsigned i=0;i<a.middle.trees.size();++i){put(a.middleSourceIds[i]);pose(a.middle.trees[i].transform);box(a.middle.trees[i].trunk);}
    put(a.broadleaf.size());for(const auto& p:a.broadleaf){pose(p.transform);box(p.trunk);}
    put(a.understory.ferns.size());for(unsigned i=0;i<a.understory.ferns.size();++i){put(i);pose(a.understory.ferns[i]);}
    put(a.mission.size());for(const auto& b:a.mission)box(b);
    put(c.plants.size());put(c.attempts);put(c.clearanceRejected);put(c.rootRejected);put(c.spacingRejected);put(c.meshRejected);
    put(c.fingerprint);put(c.groundChecks);put(c.reachedBudget);
    for(unsigned i=0;i<c.plants.size();++i){const auto& p=c.plants[i];put(i);pose(p.pose);box(p.lower);
        put(std::bit_cast<std::uint32_t>(p.crownRadius));put(std::bit_cast<std::uint32_t>(p.rootRadius));
        const auto& m=p.rootedTrunk;Need(m.Valid()&&m.vertices.size()<=600000&&m.indices.size()<=1800000,"canonical-source-identity");
        put(m.vertices.size());put(m.indices.size());
        const std::array<std::uint32_t,4> h{0x334d4541u,1u,static_cast<unsigned>(m.vertices.size()),static_cast<unsigned>(m.indices.size())};
        std::vector<std::byte> raw(16+m.vertices.size()*sizeof(Vertex3D)+m.indices.size()*sizeof(std::uint32_t));
        std::memcpy(raw.data(),h.data(),16);std::memcpy(raw.data()+16,m.vertices.data(),m.vertices.size()*sizeof(Vertex3D));
        std::memcpy(raw.data()+16+m.vertices.size()*sizeof(Vertex3D),m.indices.data(),m.indices.size()*sizeof(std::uint32_t));
        std::array<std::byte,32> digest{};Need(WorldDeltaSha2563D(raw,digest),"canonical-source-identity");
        for(unsigned j=0;j<4;++j){std::uint64_t word{};std::memcpy(&word,digest.data()+j*8,8);put(word);}}
    put(4);for(unsigned i=0;i<4;++i){const auto& f=ferns[i];put(i);
        for(float v:{f.radius,f.height,f.minimumY})put(std::bit_cast<std::uint32_t>(v));put(f.vertices);put(f.triangles);}
    return out;
}
inline void ValidateCanonicalSnapshot(const HeightField3D& terrain,const EntranceWorldAuthor& a,const CanopyPopulation& c,
    const std::array<FernColonyAsset,4>& ferns){
    Need(a.firs.firs.size()==95&&a.firs.roots.size()==95&&a.firSourceIds.size()==95&&
        a.props.size()==33&&a.propSourceIds.size()==33&&a.understory.saplings.size()==40&&a.saplingSourceIds.size()==40&&
        a.middle.trees.size()==23&&a.middleSourceIds.size()==23&&a.broadleaf.size()==12&&a.understory.ferns.size()==420&&
        a.mission.size()==33&&c.plants.size()==8,"canonical-source-identity");
    // 只核固定地形取樣，不重新搜尋樹／蕨落點或重建植栽人口。
    const auto fixed=CreateTerrain();const auto& s=terrain.Settings();const auto& expected=fixed.Settings();
    Need(s.width==expected.width&&s.depth==expected.depth&&
        std::bit_cast<std::uint32_t>(s.cellSize)==std::bit_cast<std::uint32_t>(expected.cellSize)&&
        std::bit_cast<std::uint32_t>(s.origin.x)==std::bit_cast<std::uint32_t>(expected.origin.x)&&
        std::bit_cast<std::uint32_t>(s.origin.y)==std::bit_cast<std::uint32_t>(expected.origin.y)&&
        std::bit_cast<std::uint32_t>(s.origin.z)==std::bit_cast<std::uint32_t>(expected.origin.z),"canonical-source-identity");
    for(unsigned z=0;z<s.depth;++z)for(unsigned x=0;x<s.width;++x)
        Need(std::bit_cast<std::uint32_t>(terrain.Height(x,z))==std::bit_cast<std::uint32_t>(fixed.Height(x,z)),"canonical-source-identity");
    const auto actual=CanonicalSourceWords(a,c,ferns);
    Need(actual.size()==Canonical::Fields.size()&&std::equal(actual.begin(),actual.end(),Canonical::Fields.begin()),"canonical-source-identity");
}
inline void AssetSeal(const Mesh3D& m,const char* expected,unsigned slot){
    Hash validation;validation.Mesh(m);
    static_assert(sizeof(Vertex3D)==28&&std::endian::native==std::endian::little);
    const std::array<std::uint32_t,4> header{{0x334d4541u,1u,static_cast<unsigned>(m.vertices.size()),static_cast<unsigned>(m.indices.size())}};
    const auto vb=m.vertices.size()*sizeof(Vertex3D),ib=m.indices.size()*sizeof(std::uint32_t);
    std::vector<std::byte> bytes(16+vb+ib);std::memcpy(bytes.data(),header.data(),16);
    std::memcpy(bytes.data()+16,m.vertices.data(),vb);std::memcpy(bytes.data()+16+vb,m.indices.data(),ib);
    std::array<std::byte,32> digest{};Need(Aevrynd::WorldDeltaSha2563D(bytes,digest),"asset-sha",slot);
    std::ostringstream text;text<<std::hex<<std::uppercase<<std::setfill('0');
    for(auto b:digest)text<<std::setw(2)<<std::to_integer<unsigned>(b);
    Need(text.str()==expected,"raw-three-slot-root-stem-seal",slot);
}
inline bool Overlap(const BoxCollider3D& a,const BoxCollider3D& b){
    return std::abs(double(a.center.x)-b.center.x)<double(a.extents.x)+b.extents.x&&
        std::abs(double(a.center.y)-b.center.y)<double(a.extents.y)+b.extents.y&&
        std::abs(double(a.center.z)-b.center.z)<double(a.extents.z)+b.extents.z;
}
inline unsigned Occupied(const BoxCollider3D& box,const CanopyPopulation& c){
    for(unsigned i=OriginalCount;i<TotalCount;++i)if(Overlap(box,c.plants[i].lower))return i-OriginalCount;
    return 4;
}
inline void Refresh(EntranceWorldAuthor& a){
    a.firs.fingerprint=EntranceWorldDetail::FirFingerprint(a.firs);
    a.fingerprint=14695981039346656037ull;
    for(const auto& b:a.VegetationSolids(false))for(auto w:BoxWords(b))Mistwatch::Detail::Mix(a.fingerprint,w);
    for(unsigned i=0;i<420;++i){for(auto w:PoseWords(a.understory.ferns[i]))Mistwatch::Detail::Mix(a.fingerprint,w);
        Mistwatch::Detail::Mix(a.fingerprint,i%4);}
}
inline Result Prepare(const HeightField3D& terrain,const EntranceWorldAuthor& author,const CanopyPopulation& original,
    const std::array<Mesh3D,3>& meshes,const Mesh3D& derived,const std::array<FernColonyAsset,4>& ferns){
    Need(author.applied&&author.mission.size()==33&&author.firs.firs.size()==95&&author.firs.roots.size()==95&&
        author.props.size()==33&&author.understory.ferns.size()==420&&author.understory.saplings.size()==40&&author.middle.trees.size()==23&&
        author.broadleaf.size()==12,"canonical-eight-author-counts");
    Need(author.firSourceIds.size()==author.firs.firs.size()&&author.propSourceIds.size()==author.props.size()&&
        author.saplingSourceIds.size()==author.understory.saplings.size()&&author.middleSourceIds.size()==author.middle.trees.size(),"source-id-counts");
    for(const auto* ids:{&author.firSourceIds,&author.propSourceIds,&author.saplingSourceIds,&author.middleSourceIds})
        Need(std::adjacent_find(ids->begin(),ids->end(),[](unsigned a,unsigned b){return a>=b;})==ids->end(),"source-id-order");
    auto resealed=author;Refresh(resealed);
    Need(author.fallenProfile==FallenLogProfile::ScannedStraightV1&&author.fingerprint==7152938878276451026ull&&
        resealed.fingerprint==author.fingerprint&&resealed.firs.fingerprint==author.firs.fingerprint,"canonical-author-seal");
    Need(original.reachedBudget&&original.plants.size()==OriginalCount&&original.fingerprint==13527128492525768395ull&&
        PlacementIdentity(original)==original.fingerprint,"canonical-eight-placement");
    for(unsigned i=0;i<4;++i)Need(std::isfinite(ferns[i].radius)&&ferns[i].radius>0&&ferns[i].radius<=4&&
        std::isfinite(ferns[i].minimumY)&&std::isfinite(ferns[i].height)&&ferns[i].minimumY<ferns[i].height,"fern-asset",i);
    for(unsigned i=0;i<420;++i){const auto& p=author.understory.ferns[i];
        Need(Session::Finite(p.position)&&Session::Finite(p.rotation)&&Session::Finite(p.scale)&&p.rotation.x==0&&p.rotation.z==0&&
            p.scale.x>0&&p.scale.x==p.scale.y&&p.scale.x==p.scale.z&&terrain.Contains(p.position.x,p.position.z),"fern-pose",i);}
    ValidateCanonicalSnapshot(terrain,author,original,ferns);
    AssetSeal(meshes[0],"B964D1F7B16FB3602FF038AE111C79D0BFC029185896E95DA97CC1ACB4F59145",0);
    AssetSeal(meshes[1],"1773F7705A43DA3B3673D0D34C66C2C4FEFC47952970CB84665A774D6A54F871",1);
    AssetSeal(meshes[2],"5D32528C7604D041A0730755DDF765B111F82B43847C5F4AED0FE386E99D6188",2);
    AssetSeal(derived,"96EA5581E6A5D2A0C3260DEEBE614A4FA9BCCA0A77772E5EAE42E0C88730BC7C",3);
    Result r;r.author=author;r.canopy=original;r.inputIdentity=Identity(terrain,author,original,ferns);
    r.metadata.originalFingerprint=original.fingerprint;const auto geometry=MeasureCanopy(meshes);
    for(unsigned i=0;i<OriginalCount;++i){const auto& p=original.plants[i];
        const auto stem=PlantDerivedRootStem(derived,geometry,p.pose,terrain);
        Need(stem.indices==p.rootedTrunk.indices&&stem.vertices.size()==p.rootedTrunk.vertices.size()&&
            std::memcmp(stem.vertices.data(),p.rootedTrunk.vertices.data(),stem.vertices.size()*sizeof(Vertex3D))==0,"original-derived-root-bits",i);
        const auto rawRoot=RootCanopyToTerrain(meshes[0],geometry,p.pose,terrain);
        Vec3 low=geometry.minimum,high=geometry.maximum;for(const auto& v:rawRoot.vertices)Expand(low,high,{v.x,v.y,v.z});
        const auto ext=(high-low)*.5f;const auto bounds=TransformBounds(MeshBounds3D{(high+low)*.5f,Length(ext),ext},p.pose);
        Need(BoxWords(ExtendRootCollision({bounds.center,bounds.extents},stem,p.pose))==BoxWords(p.lower),"original-full-envelope-bits",i);
        r.proxies[i]=CreateCanopyWoodProxy(p.rootedTrunk,meshes[2],p.pose);}
    for(unsigned i=0;i<4;++i){const auto a=Anchors[i];
        Need(std::isfinite(a.x)&&std::isfinite(a.z)&&std::isfinite(a.yaw)&&std::isfinite(a.scale)&&a.scale>=1&&a.scale<=1.15f,"fixed-anchor",i);
        Need(terrain.Contains(a.x,a.z),"root-domain",i);
        const Vec3 root{a.x,terrain.SampleHeight(a.x,a.z),a.z};const float radius=geometry.radius*a.scale,base=geometry.rootRadius*a.scale;
        Need(terrain.Contains(root.x-radius,root.z-radius)&&terrain.Contains(root.x+radius,root.z+radius),"full-crown-domain",i);
        Need(ForestCoverFootprint(terrain,root,base,{}),"original-root-footprint",i);
        for(unsigned k=0;k<r.canopy.plants.size();++k){const auto& p=r.canopy.plants[k];
            if(!(std::hypot(root.x-p.pose.position.x,root.z-p.pose.position.z)>=radius+p.crownRadius+.5f))
                throw std::runtime_error("WOODLAND_CANOPY_FIRST_DIVERGENCE id="+std::to_string(i)+" gate=original-full-crown-spacing other="+std::to_string(k));}
        Transform3D pose{{root.x,root.y-geometry.rootMinimum*a.scale,root.z},{0,a.yaw,0},{a.scale,a.scale,a.scale}};
        const auto rooted=RootCanopyToTerrain(meshes[0],geometry,pose,terrain);
        Vec3 low=geometry.minimum,high=geometry.maximum;for(const auto& v:rooted.vertices)Expand(low,high,{v.x,v.y,v.z});
        const auto ext=(high-low)*.5f;const auto bounds=TransformBounds(MeshBounds3D{(high+low)*.5f,Length(ext),ext},pose);
        const BoxCollider3D box{bounds.center,bounds.extents};
        for(const auto& p:r.canopy.plants)Need(std::abs(p.lower.center.x-box.center.x)>=p.lower.extents.x+box.extents.x+.03f||
            std::abs(p.lower.center.z-box.center.z)>=p.lower.extents.z+box.extents.z+.03f,"original-final-envelope-spacing",i);
        auto stem=PlantDerivedRootStem(derived,geometry,pose,terrain);const auto finalBox=ExtendRootCollision(box,stem,pose);
        Need(PropClearsObjectives(finalBox),"final-root-objective-clearance",i);
        EntranceCanopyDetail::CheckGround(stem,pose,terrain,-.04f,i,r.metadata,true);
        EntranceCanopyDetail::CheckGround(meshes[1],pose,terrain,.04f,i,r.metadata);
        EntranceCanopyDetail::CheckGround(meshes[2],pose,terrain,-.04f,i,r.metadata);
        r.proxies[OriginalCount+i]=CreateCanopyWoodProxy(stem,meshes[2],pose);
        r.canopy.plants.push_back({pose,finalBox,radius,base,std::move(stem)});++r.canopy.attempts;++r.metadata.appended;
    }
    for(unsigned i=0;i<TotalCount;++i){const auto& proxy=r.proxies[i];
        Need(!proxy.parts.empty()&&proxy.parts.size()<=10000&&proxy.spacing==.15&&CanopyVolume::ValidWoodParts(proxy.parts),"complete-wood-proxy",i);
        r.wood.insert(r.wood.end(),proxy.parts.begin(),proxy.parts.end());}
    Need(r.wood.size()<=MaximumWoodParts,"profile-specific-wood-budget");
    r.metadata.woodParts=r.wood.size();r.canopy.groundChecks+=r.metadata.faceChecks;
    r.canopy.fingerprint=PlacementIdentity(r.canopy);r.canopy.reachedBudget=true;return r;
}
inline std::vector<Withdrawal> WithdrawOccupied(Result& r,const HeightField3D& terrain){
    std::vector<Withdrawal> out;
    const auto record=[&](EntranceRole role,unsigned id,const Transform3D& pose,const BoxCollider3D& box,unsigned patch){
        const unsigned at=Occupied(box,r.canopy);if(at==4)return false;
        out.push_back({role,id,at,patch,PoseWords(pose),BoxWords(box),WithdrawalReason::FullEnvelopeOccupied});return true;};
    auto original=r.author;r.author.firs.firs.clear();r.author.firs.roots.clear();r.author.firSourceIds.clear();
    for(unsigned i=0;i<original.firs.firs.size();++i){const auto& p=original.firs.firs[i];
        if(!record(EntranceRole::Fir,original.firSourceIds[i],p.render,p.collider,p.patch)){
            r.author.firs.firs.push_back(p);r.author.firs.roots.push_back(original.firs.roots[i]);r.author.firSourceIds.push_back(original.firSourceIds[i]);}}
    r.author.props.clear();r.author.propSourceIds.clear();
    for(unsigned i=0;i<original.props.size();++i){const auto& p=original.props[i];
        if(p.FallenLog()||!record(EntranceRole::Rock,original.propSourceIds[i],p.transform,p.collider,0)){
            r.author.props.push_back(p);r.author.propSourceIds.push_back(original.propSourceIds[i]);}}
    r.author.understory.saplings.clear();r.author.saplingSourceIds.clear();
    for(unsigned i=0;i<original.understory.saplings.size();++i){const auto& p=original.understory.saplings[i];
        if(!record(EntranceRole::Sapling,original.saplingSourceIds[i],p,SaplingCollider(p),0)){
            r.author.understory.saplings.push_back(p);r.author.saplingSourceIds.push_back(original.saplingSourceIds[i]);}}
    r.author.middle.trees.clear();r.author.middleSourceIds.clear();
    for(unsigned i=0;i<original.middle.trees.size();++i){const auto& p=original.middle.trees[i];
        if(!record(EntranceRole::Middle,original.middleSourceIds[i],p.transform,p.trunk,0)){
            r.author.middle.trees.push_back(p);r.author.middleSourceIds.push_back(original.middleSourceIds[i]);}}
    // 原兩個橋狀態的任務、倒木及保留角色皆驗；不用退讓表刪除硬設施。
    for(bool repaired:{false,true}){const auto solids=r.author.VegetationSolids(repaired);
        Need(CanopyVolume::ValidWoodParts(solids),"retained-world-valid");
        for(unsigned i=OriginalCount;i<TotalCount;++i){const auto& p=r.canopy.plants[i];
            Vec3 root=p.pose.position;root.y=terrain.SampleHeight(root.x,root.z);
            const auto clearance=[&](Vec3 center,float radius,const char* gate){
                if(ForestCoverClearance(center,radius,solids))return;
                std::size_t first=solids.size();for(std::size_t j=0;j<solids.size();++j)
                    if(std::abs(center.x-solids[j].center.x)<solids[j].extents.x+radius&&
                        std::abs(center.z-solids[j].center.z)<solids[j].extents.z+radius){first=j;break;}
                throw std::runtime_error("WOODLAND_CANOPY_FIRST_DIVERGENCE id="+std::to_string(i-OriginalCount)+
                    " gate="+gate+" bridge="+std::to_string(repaired)+" first_world_box="+std::to_string(first)+
                    " world_box_count="+std::to_string(solids.size()));};
            clearance(root,p.crownRadius,"original-crown-world-clearance");
            clearance(p.lower.center,(std::max)(p.lower.extents.x,p.lower.extents.z),"original-final-envelope-clearance");}}
    return out;
}
inline unsigned FernConflict(const Result& r,unsigned id,const Transform3D& pose,const std::array<FernColonyAsset,4>& assets){
    const auto& a=assets[id%4];const auto local=CanopyVolume::ConservativeLocalVolume(a.radius,a.minimumY,a.height);
    for(unsigned i=0;i<TotalCount;++i){const auto& p=r.canopy.plants[i];
        if(std::hypot(p.pose.position.x-pose.position.x,p.pose.position.z-pose.position.z)<p.rootRadius+a.radius*pose.scale.x+.03f||
            !CanopyVolume::ClearsCanopyWood(pose,local,r.proxies[i].parts))return i;}
    return TotalCount;
}
inline Transform3D Proposal(const Transform3D& before,unsigned proposal,const HeightField3D& terrain){
    Need(proposal<FernProposals,"fern-proposal-domain",proposal);
    constexpr std::array<Vec3,8> directions{{{1,0,0},{-1,0,0},{0,0,1},{0,0,-1},{.70710677f,0,.70710677f},
        {-.70710677f,0,.70710677f},{.70710677f,0,-.70710677f},{-.70710677f,0,-.70710677f}}};
    auto p=before;const float distance=.5f*float(proposal/8+1);const auto d=directions[proposal%8];
    p.position.x+=d.x*distance;p.position.z+=d.z*distance;
    if(terrain.Contains(p.position.x,p.position.z))p.position.y=terrain.SampleHeight(p.position.x,p.position.z);return p;
}
inline const char* FernGate(const Result& r,unsigned id,const Transform3D& p,const HeightField3D& terrain,
    const std::array<FernColonyAsset,4>& assets){
    const auto& a=assets[id%4];const float radius=a.radius*p.scale.x;
    if(!terrain.Contains(p.position.x,p.position.z)||terrain.SampleNormal(p.position.x,p.position.z).y<.88f||
        !PropClearsObjectives({p.position,{.9f,1,.9f}}))return "fern-original-domain-mission-slope";
    if(!ForestCoverFootprint(terrain,p.position,radius,{}))return "fern-original-footprint";
    for(auto root:r.author.firs.roots)if(std::hypot(root.x-p.position.x,root.z-p.position.z)<1)return "fern-original-fir-spacing";
    for(const auto& prop:r.author.props)if(std::abs(p.position.x-prop.collider.center.x)<prop.collider.extents.x+.9f&&
        std::abs(p.position.z-prop.collider.center.z)<prop.collider.extents.z+.9f)return "fern-original-prop-spacing";
    for(const auto& s:r.author.understory.saplings)if(std::hypot(s.position.x-p.position.x,s.position.z-p.position.z)<1)return "fern-original-sapling-spacing";
    for(unsigned j=0;j<420;++j)if(j!=id){const auto& q=r.author.understory.ferns[j];const float distance=std::hypot(q.position.x-p.position.x,q.position.z-p.position.z);
        if(distance<1||distance<.65f*(radius+assets[j%4].radius*q.scale.x))return "fern-original-neighbor-spacing";}
    const auto local=CanopyVolume::ConservativeLocalVolume(a.radius,a.minimumY,a.height);
    for(bool repaired:{false,true})if(!CanopyVolume::ClearsCanopyWood(p,local,r.author.VegetationSolids(repaired)))return "fern-full-volume-world";
    if(FernConflict(r,id,p,assets)!=TotalCount)return "fern-full-volume-twelve-wood-root-spacing";
    return nullptr;
}
inline void Finish(Result& r,const HeightField3D& terrain,const std::array<FernColonyAsset,4>& assets){
    for(unsigned id=0;id<420;++id)Need(FernConflict(r,id,r.author.understory.ferns[id],assets)==TotalCount,"all-420-full-wood-root",id);
    for(const auto& move:r.recipe.fernMoves){const auto gate=FernGate(r,move.sourceId,r.author.understory.ferns[move.sourceId],terrain,assets);
        Need(!gate,gate?gate:"fern-pass",move.sourceId);}
    Refresh(r.author);Hash h;for(const auto& box:r.wood)h.Words(BoxWords(box));r.woodFingerprint=h.value;
    r.outputIdentity=Identity(terrain,r.author,r.canopy,assets);r.complete=r.canopy.plants.size()==TotalCount&&r.metadata.appended==4;
    Need(r.complete,"complete-twelve-required");
}
}
// CPU 作者階段才搜尋固定的 64 個相鄰落點；遊戲一律使用 ApplyRecipe。
inline Recipe AuthorRecipe(const HeightField3D& terrain,const EntranceWorldAuthor& originalAuthor,const CanopyPopulation& original8,
    const std::array<Mesh3D,3>& meshes,const Mesh3D& derivedStem,const std::array<FernColonyAsset,4>& fernAssets){
    auto r=Detail::Prepare(terrain,originalAuthor,original8,meshes,derivedStem,fernAssets);
    r.recipe.inputIdentity=r.inputIdentity;r.recipe.withdrawals=Detail::WithdrawOccupied(r,terrain);
    for(unsigned id=0;id<420;++id){const auto before=r.author.understory.ferns[id];const unsigned conflict=Detail::FernConflict(r,id,before,fernAssets);
        if(conflict==TotalCount)continue;
        Detail::Need(conflict>=OriginalCount,"original-eight-fern-invalid",id);
        Detail::Need(r.recipe.fernMoves.size()<MaximumFernMoves,"fern-move-budget",id);
        bool placed{};const char* lastGate="no-proposal";
        for(unsigned p=0;p<FernProposals;++p){const auto candidate=Detail::Proposal(before,p,terrain);
            lastGate=Detail::FernGate(r,id,candidate,terrain,fernAssets);if(lastGate)continue;
            r.recipe.fernMoves.push_back({id,conflict-OriginalCount,p,Detail::PoseWords(before),Detail::PoseWords(candidate)});
            r.author.understory.ferns[id]=candidate;placed=true;break;}
        if(!placed)throw std::runtime_error("WOODLAND_CANOPY_FIRST_DIVERGENCE id="+std::to_string(id)+
            " gate=bounded-fern-relocation-exhausted proposals=64 last="+(lastGate?lastGate:"none"));
    }
    Detail::Finish(r,terrain,fernAssets);return r.recipe;
}
inline Result ApplyRecipe(const HeightField3D& terrain,const EntranceWorldAuthor& originalAuthor,const CanopyPopulation& original8,
    const std::array<Mesh3D,3>& meshes,const Mesh3D& derivedStem,const std::array<FernColonyAsset,4>& fernAssets,const Recipe& recipe){
    Detail::Need(recipe.fernMoves.size()<=MaximumFernMoves&&recipe.withdrawals.size()<=200,"recipe-budget");
    Detail::ValidateCanonicalSnapshot(terrain,originalAuthor,original8,fernAssets);
    Detail::Need(recipe.inputIdentity==Detail::Identity(terrain,originalAuthor,original8,fernAssets),"recipe-input-identity");
    auto r=Detail::Prepare(terrain,originalAuthor,original8,meshes,derivedStem,fernAssets);
    const auto expected=Detail::WithdrawOccupied(r,terrain);Detail::Need(recipe.withdrawals==expected,"exact-occupied-withdrawal-table");r.recipe=recipe;
    unsigned previous{};bool first=true;
    for(const auto& move:recipe.fernMoves){Detail::Need(move.sourceId<420&&(first||move.sourceId>previous),"fern-move-source-order",move.sourceId);
        first=false;previous=move.sourceId;const auto before=r.author.understory.ferns[move.sourceId];
        Detail::Need(Detail::PoseWords(before)==move.before,"fern-before-bits",move.sourceId);
        const auto conflict=Detail::FernConflict(r,move.sourceId,before,fernAssets);
        Detail::Need(conflict>=OriginalCount&&conflict<TotalCount&&conflict-OriginalCount==move.anchor,"fern-actual-new-conflict",move.sourceId);
        const auto candidate=Detail::Proposal(before,move.proposal,terrain);
        Detail::Need(Detail::PoseWords(candidate)==move.after,"fern-fixed-after-bits",move.sourceId);
        const auto gate=Detail::FernGate(r,move.sourceId,candidate,terrain,fernAssets);Detail::Need(!gate,gate?gate:"fern-pass",move.sourceId);
        r.author.understory.ferns[move.sourceId]=candidate;
    }
    Detail::Finish(r,terrain,fernAssets);return r;
}
}
