#include "fallen/MistwatchAuthoredWorldQualification.h"
#include "MistwatchWoodlandCanopy.h"
#include "MistwatchWoodlandRecipe.h"
#include "MistwatchGroupedCanopyVolume.h"
#include <iostream>
#include <numeric>
#include <functional>

namespace Q=::Mistwatch::AuthoredQualification;
namespace W=::Mistwatch::FallenWorldSeat;
namespace C=::Mistwatch::WoodlandCanopy;
using namespace Mistwatch;
namespace {
unsigned negatives{};
void Need(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
template<class Action>void Reject(Action&& action,std::string_view gate){
    bool rejected{};try{action();}catch(const std::exception& e){
        const std::string expected="WOODLAND_CANOPY_FIRST_DIVERGENCE id=0 gate="+std::string(gate);
        Need(e.what()==expected,"NEGATIVE_WRONG_FIRST_DIVERGENCE");rejected=true;}
    Need(rejected,"NEGATIVE_ACCEPTED");++negatives;
    std::cout<<"WOODLAND_CONTROL gate="<<gate<<" rejected=true"<<std::endl;
}
template<std::size_t N>void Words(const std::array<std::uint32_t,N>& a){
    std::cout<<'{';for(std::size_t i=0;i<N;++i){if(i)std::cout<<',';std::cout<<a[i]<<'u';}std::cout<<'}';
}
void EmitCanonicalSnapshot(const Q::Context& context,const Q::Assets& assets){
    // 來源是獨立 Q::ValidateContext/LoadAssets 的原八株，不從 native 畫面反填。
    std::vector<std::uint64_t> words;const auto put=[&](auto x){words.push_back(static_cast<std::uint64_t>(x));};
    const auto pose=[&](const Transform3D& p){for(float f:{p.position.x,p.position.y,p.position.z,p.rotation.x,p.rotation.y,p.rotation.z,p.scale.x,p.scale.y,p.scale.z})put(std::bit_cast<std::uint32_t>(f));};
    const auto box=[&](const BoxCollider3D& b){for(float f:{b.center.x,b.center.y,b.center.z,b.extents.x,b.extents.y,b.extents.z})put(std::bit_cast<std::uint32_t>(f));};
    const auto& a=context.author;const auto& c=context.canopy;
    put(0x57435332u);put(a.applied);put(static_cast<unsigned>(a.fallenProfile));put(a.sourceFirFingerprint);put(a.sourceMiddleFingerprint);
    put(a.fingerprint);put(a.firs.fingerprint);put(a.firs.reachedBudget);put(a.middle.reachedBudget);
    put(a.firs.firs.size());
    for(unsigned i=0;i<a.firs.firs.size();++i){const auto& p=a.firs.firs[i];put(a.firSourceIds[i]);pose(p.render);box(p.collider);
        for(auto v:{p.root.x,p.root.y,p.root.z})put(std::bit_cast<std::uint32_t>(v));
        for(auto v:{a.firs.roots[i].x,a.firs.roots[i].y,a.firs.roots[i].z})put(std::bit_cast<std::uint32_t>(v));put(p.patch);}
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
        const auto& m=p.rootedTrunk;put(m.vertices.size());put(m.indices.size());
        std::vector<std::byte> raw(16+m.vertices.size()*sizeof(Vertex3D)+m.indices.size()*sizeof(std::uint32_t));
        const std::array<std::uint32_t,4> h{0x334d4541u,1u,static_cast<unsigned>(m.vertices.size()),static_cast<unsigned>(m.indices.size())};
        std::memcpy(raw.data(),h.data(),16);std::memcpy(raw.data()+16,m.vertices.data(),m.vertices.size()*sizeof(Vertex3D));
        std::memcpy(raw.data()+16+m.vertices.size()*sizeof(Vertex3D),m.indices.data(),m.indices.size()*sizeof(std::uint32_t));
        std::array<std::byte,32> digest;Need(WorldDeltaSha2563D(raw,digest),"SNAPSHOT_MESH_SHA");
        for(unsigned j=0;j<4;++j){std::uint64_t word{};std::memcpy(&word,digest.data()+j*8,8);put(word);}}
    put(assets.fernAssets.size());for(unsigned i=0;i<4;++i){const auto& f=assets.fernAssets[i];put(i);
        for(float v:{f.radius,f.height,f.minimumY})put(std::bit_cast<std::uint32_t>(v));put(f.vertices);put(f.triangles);}
    std::cout<<"CANONICAL_WORDS=";for(std::size_t i=0;i<words.size();++i){if(i)std::cout<<',';std::cout<<words[i];}std::cout<<std::endl;
    std::cout<<"CANONICAL_SNAPSHOT_SOURCE_VALIDATED count="<<words.size()<<" native=false"<<std::endl;
}
void Emit(const C::Recipe& r){
    // 十進位 uint32 字面值精確保留 float bits；可直接轉成已審核的固定 C++ 表。
    std::cout<<"RECIPE_INPUT "<<r.inputIdentity<<"ull"<<std::endl;
    for(const auto& w:r.withdrawals){std::cout<<"RECIPE_WITHDRAW {EntranceRole::";
        switch(w.role){case EntranceRole::Fir:std::cout<<"Fir";break;case EntranceRole::Rock:std::cout<<"Rock";break;
        case EntranceRole::Sapling:std::cout<<"Sapling";break;case EntranceRole::Middle:std::cout<<"Middle";break;}
        std::cout<<','<<w.sourceId<<"u,"<<w.anchor<<"u,"<<w.patch<<"u,";Words(w.pose);std::cout<<',';Words(w.box);
        std::cout<<",WithdrawalReason::FullEnvelopeOccupied}"<<std::endl;}
    for(const auto& m:r.fernMoves){std::cout<<"RECIPE_FERN {"<<m.sourceId<<"u,"<<m.anchor<<"u,"<<m.proposal<<"u,";
        Words(m.before);std::cout<<',';Words(m.after);std::cout<<'}'<<std::endl;}
}
void PreselectAnchors(const Q::Context& context,const Q::Assets& assets,const HeightField3D& terrain){
    // 僅 CPU 作者預選：四個可見世界區域、每區固定 169 格，不改正式配置或降低任何门。
    // 預選通過仍須完整 AuthorRecipe 的全三角面、十二木件、蕨及下游資格。
    constexpr std::array<Vec3,4> centers{{{-13,0,48},{-18,0,55},{15,0,50},{-16,0,72}}};
    struct Offset {int x,z,distance;};std::vector<Offset> offsets;
    for(int z=-6;z<=6;++z)for(int x=-6;x<=6;++x)offsets.push_back({x,z,x*x+z*z});
    std::stable_sort(offsets.begin(),offsets.end(),[](const auto& a,const auto& b){return a.distance<b.distance;});
    const auto geometry=MeasureCanopy(assets.canopy);
    struct Candidate {unsigned ordinal;Transform3D pose;BoxCollider3D box;float radius;};
    std::array<std::vector<Candidate>,4> candidates;
    std::cout<<"PRESELECT_GEOMETRY crown_radius="<<geometry.radius<<" root_radius="<<geometry.rootRadius<<std::endl;
    for(unsigned region=0;region<4;++region){std::array<unsigned,7> rejects{};unsigned accepted{},ordinal{};
        for(const auto offset:offsets){const auto at=ordinal++;const auto a=C::Anchors[region];
            const float x=centers[region].x+float(offset.x)*.5f,z=centers[region].z+float(offset.z)*.5f;
            const Vec3 root{x,terrain.SampleHeight(x,z),z};const float radius=geometry.radius*a.scale,base=geometry.rootRadius*a.scale;
            if(!terrain.Contains(x-radius,z-radius)||!terrain.Contains(x+radius,z+radius)){++rejects[0];continue;}
            if(!ForestCoverFootprint(terrain,root,base,{})){++rejects[1];continue;}
            if(std::any_of(context.canopy.plants.begin(),context.canopy.plants.end(),[&](const auto& p){
                return std::hypot(x-p.pose.position.x,z-p.pose.position.z)<radius+p.crownRadius+.5f;})){++rejects[2];continue;}
            const Transform3D pose{{x,root.y-geometry.rootMinimum*a.scale,z},{0,a.yaw,0},{a.scale,a.scale,a.scale}};
            const auto rooted=RootCanopyToTerrain(assets.canopy[0],geometry,pose,terrain);
            Vec3 low=geometry.minimum,high=geometry.maximum;for(const auto& v:rooted.vertices)Expand(low,high,{v.x,v.y,v.z});
            const auto ext=(high-low)*.5f;const auto bounds=TransformBounds(MeshBounds3D{(high+low)*.5f,Length(ext),ext},pose);
            auto stem=PlantDerivedRootStem(assets.derivedStem,geometry,pose,terrain);
            const auto box=ExtendRootCollision({bounds.center,bounds.extents},stem,pose);
            if(std::any_of(context.canopy.plants.begin(),context.canopy.plants.end(),[&](const auto& p){
                return std::abs(p.lower.center.x-box.center.x)<p.lower.extents.x+box.extents.x+.03f&&
                    std::abs(p.lower.center.z-box.center.z)<p.lower.extents.z+box.extents.z+.03f;})){++rejects[3];continue;}
            if(!PropClearsObjectives(box)){++rejects[4];continue;}
            bool clear=true;
            for(bool repaired:{false,true}){auto solids=context.author.mission;
                if(repaired)solids.push_back({{0,-.2f,36},{2,.2f,4}});
                // 與 v2 退讓語意相同：只排除有真 3D envelope 交集的可退讓角色。
                const auto keep=[&](const BoxCollider3D& b){if(!C::Detail::Overlap(b,box))solids.push_back(b);};
                for(const auto& p:context.author.firs.firs)keep(p.collider);
                for(const auto& p:context.author.props){if(p.FallenLog())solids.push_back(p.collider);else keep(p.collider);}
                for(const auto& p:context.author.understory.saplings)keep(SaplingCollider(p));
                for(const auto& p:context.author.middle.trees)keep(p.trunk);
                for(const auto& p:context.author.broadleaf)solids.push_back(p.trunk);
                if(!ForestCoverClearance(root,radius,solids)){++rejects[5];clear=false;break;}
                if(!ForestCoverClearance(box.center,(std::max)(box.extents.x,box.extents.z),solids)){++rejects[6];clear=false;break;}}
            if(!clear)continue;
            if(accepted<5){std::cout<<"PRESELECT_POSSIBLE region="<<region<<" ordinal="<<at<<" x="<<x<<" z="<<z
                <<" yaw="<<a.yaw<<" scale="<<a.scale<<" pose=";Words(EntranceWorldDetail::PoseWords(pose));std::cout<<" full_faces=false"<<std::endl;}
            candidates[region].push_back({at,pose,box,radius});++accepted;
        }
        Need(ordinal==169&&ordinal==accepted+std::accumulate(rejects.begin(),rejects.end(),0u),"PRESELECT_COMPLETE_ACCOUNTING");
        std::cout<<"PRESELECT_REGION region="<<region<<" proposals="<<ordinal<<" possible="<<accepted
            <<" domain="<<rejects[0]<<" footprint="<<rejects[1]<<" crown_spacing="<<rejects[2]<<" envelope_spacing="<<rejects[3]
            <<" objective="<<rejects[4]<<" retained_crown="<<rejects[5]<<" retained_envelope="<<rejects[6]<<" full_qualified=false"<<std::endl;
    }
    // 聯合粗篩只接受同時保有原冠距與完整包絡間距的四株；仍不做正式配置。
    std::array<const Candidate*,4> chosen{};unsigned visits{};bool budgetReached{};
    const std::function<bool(unsigned)> select=[&](unsigned region){
        if(region==4)return true;
        for(const auto& p:candidates[region]){
            if(visits==65536){budgetReached=true;return false;}++visits;bool okay=true;
            for(unsigned i=0;i<region;++i){const auto& q=*chosen[i];
                if(std::hypot(p.pose.position.x-q.pose.position.x,p.pose.position.z-q.pose.position.z)<p.radius+q.radius+.5f||
                    (std::abs(p.box.center.x-q.box.center.x)<p.box.extents.x+q.box.extents.x+.03f&&
                     std::abs(p.box.center.z-q.box.center.z)<p.box.extents.z+q.box.extents.z+.03f)){okay=false;break;}}
            if(!okay)continue;chosen[region]=&p;if(select(region+1))return true;
            if(budgetReached)return false;
        }
        return false;
    };
    const bool complete=select(0);
    std::cout<<"PRESELECT_JOINT complete="<<complete<<" visits="<<visits<<" budget=65536 budget_reached="<<budgetReached<<" full_faces=false"<<std::endl;
    if(complete)for(unsigned region=0;region<4;++region){const auto& p=*chosen[region];
        std::cout<<"PRESELECT_JOINT_POSSIBLE region="<<region<<" ordinal="<<p.ordinal<<" x="<<p.pose.position.x<<" z="<<p.pose.position.z
            <<" yaw="<<p.pose.rotation.y<<" scale="<<p.pose.scale.x<<" pose=";Words(EntranceWorldDetail::PoseWords(p.pose));
        std::cout<<" box=";Words(EntranceWorldDetail::BoxWords(p.box));std::cout<<std::endl;}
}
void Independent(const C::Result& r,const Q::Context& old,const HeightField3D& terrain,const Q::Assets& assets){
    Need(r.complete&&r.canopy.reachedBudget&&r.canopy.plants.size()==12&&r.metadata.appended==4,"COMPLETE_TWELVE");
    Need(r.author.understory.ferns.size()==420&&r.author.broadleaf.size()==12,"FERN_BROADLEAF_RETENTION");
    for(unsigned i=0;i<8;++i){const auto& a=old.canopy.plants[i];const auto& b=r.canopy.plants[i];
        Need(Q::Detail::Same(a.pose,b.pose)&&Q::Detail::Same(a.lower,b.lower)&&Q::Detail::Same(a.rootedTrunk,b.rootedTrunk)&&
            Q::Detail::Same(a.crownRadius,b.crownRadius)&&Q::Detail::Same(a.rootRadius,b.rootRadius),"ORIGINAL_EIGHT_ALL_BITS");}
    Q::Detail::SameBoxes(std::span<const BoxCollider3D>(r.wood).first(old.wood.size()),old.wood,"ORIGINAL_EIGHT_WOOD_PREFIX");
    for(unsigned i=0;i<33;++i)Need(Q::Detail::Same(r.author.mission[i],old.author.mission[i]),"MISSION_UNCHANGED");
    for(unsigned i=0;i<12;++i)Need(Q::Detail::Same(r.author.broadleaf[i].transform,old.author.broadleaf[i].transform)&&
        Q::Detail::Same(r.author.broadleaf[i].trunk,old.author.broadleaf[i].trunk),"OLD_BROADLEAF_UNCHANGED");
    const auto removed=[&](EntranceRole role,unsigned id){return std::find_if(r.recipe.withdrawals.begin(),r.recipe.withdrawals.end(),
        [&](const auto& w){return w.role==role&&w.sourceId==id;})!=r.recipe.withdrawals.end();};
    const auto preserved=[&](const auto& oldIds,const auto& newIds,EntranceRole role,const auto& compare){
        std::size_t next{};for(std::size_t i=0;i<oldIds.size();++i)if(!removed(role,oldIds[i])){
            Need(next<newIds.size()&&newIds[next]==oldIds[i],"RETAINED_SOURCE_ID_ORDER");compare(i,next);++next;}
        Need(next==newIds.size(),"EXACT_WITHDRAWAL_COUNT");};
    preserved(old.author.firSourceIds,r.author.firSourceIds,EntranceRole::Fir,[&](std::size_t a,std::size_t b){
        Need(Q::Detail::Same(old.author.firs.firs[a].render,r.author.firs.firs[b].render)&&
            Q::Detail::Same(old.author.firs.firs[a].collider,r.author.firs.firs[b].collider)&&
            Q::Detail::Same(old.author.firs.roots[a],r.author.firs.roots[b]),"RETAINED_FIR_BITS");});
    preserved(old.author.propSourceIds,r.author.propSourceIds,EntranceRole::Rock,[&](std::size_t a,std::size_t b){
        Need(Q::Detail::Same(old.author.props[a].transform,r.author.props[b].transform)&&
            Q::Detail::Same(old.author.props[a].collider,r.author.props[b].collider)&&old.author.props[a].mesh==r.author.props[b].mesh,"RETAINED_PROP_BITS");});
    preserved(old.author.saplingSourceIds,r.author.saplingSourceIds,EntranceRole::Sapling,[&](std::size_t a,std::size_t b){
        Need(Q::Detail::Same(old.author.understory.saplings[a],r.author.understory.saplings[b]),"RETAINED_SAPLING_BITS");});
    preserved(old.author.middleSourceIds,r.author.middleSourceIds,EntranceRole::Middle,[&](std::size_t a,std::size_t b){
        Need(Q::Detail::Same(old.author.middle.trees[a].transform,r.author.middle.trees[b].transform)&&
            Q::Detail::Same(old.author.middle.trees[a].trunk,r.author.middle.trees[b].trunk),"RETAINED_MIDDLE_BITS");});
    for(const auto& w:r.recipe.withdrawals){Need(w.anchor<4&&w.reason==C::WithdrawalReason::FullEnvelopeOccupied,"WITHDRAWAL_REASON");
        BoxCollider3D box;box.center={std::bit_cast<float>(w.box[0]),std::bit_cast<float>(w.box[1]),std::bit_cast<float>(w.box[2])};
        box.extents={std::bit_cast<float>(w.box[3]),std::bit_cast<float>(w.box[4]),std::bit_cast<float>(w.box[5])};
        const auto& crown=r.canopy.plants[8+w.anchor].lower;
        Need(double(box.center.x)-box.extents.x<double(crown.center.x)+crown.extents.x&&
            double(box.center.x)+box.extents.x>double(crown.center.x)-crown.extents.x&&
            double(box.center.y)-box.extents.y<double(crown.center.y)+crown.extents.y&&
            double(box.center.y)+box.extents.y>double(crown.center.y)-crown.extents.y&&
            double(box.center.z)-box.extents.z<double(crown.center.z)+crown.extents.z&&
            double(box.center.z)+box.extents.z>double(crown.center.z)-crown.extents.z,"WITHDRAWAL_ACTUALLY_OCCUPIED");}
    for(unsigned i=8;i<12;++i){const auto& p=r.canopy.plants[i];const auto& a=C::Anchors[i-8];
        Need(p.pose.position.x==a.x&&p.pose.position.z==a.z&&p.pose.rotation.y==a.yaw&&p.pose.scale.x==a.scale,"FIXED_WORLD_NOT_CAMERA");
        for(unsigned j=0;j<i;++j){const auto& q=r.canopy.plants[j];
            Need(std::hypot(p.pose.position.x-q.pose.position.x,p.pose.position.z-q.pose.position.z)>=p.crownRadius+q.crownRadius+.5f,"CROWN_SPACING");
            Need(std::abs(p.lower.center.x-q.lower.center.x)>=p.lower.extents.x+q.lower.extents.x+.03f||
                std::abs(p.lower.center.z-q.lower.center.z)>=p.lower.extents.z+q.lower.extents.z+.03f,"ENVELOPE_SPACING");}
        Vec3 root=p.pose.position;root.y=terrain.SampleHeight(root.x,root.z);
        Need(ForestCoverFootprint(terrain,root,p.rootRadius,{}),"ORIGINAL_ROOT_FOOTPRINT");
        for(bool repaired:{false,true}){const auto solids=r.author.VegetationSolids(repaired);
            Need(ForestCoverClearance(root,p.crownRadius,solids)&&ForestCoverClearance(p.lower.center,
                (std::max)(p.lower.extents.x,p.lower.extents.z),solids),"BOTH_WORLD_ROUTE_ENVELOPE");}
        Need(PropClearsObjectives(p.lower),"FINAL_ROOT_MISSION");
        // 真實三角面 oracle 另核最低區域，安全高面用整個地形最高點界限排除。
        float high=-std::numeric_limits<float>::infinity();const auto& grid=terrain.Settings();
        for(unsigned z=0;z<grid.depth;++z)for(unsigned x=0;x<grid.width;++x)high=(std::max)(high,terrain.Height(x,z));
        for(unsigned slot=0;slot<3;++slot){const auto& mesh=slot==0?p.rootedTrunk:assets.canopy[slot];const auto matrix=p.pose.Matrix();
            std::vector<Vec3> world;world.reserve(mesh.vertices.size());for(const auto& v:mesh.vertices){
                auto v3=TransformPoint(matrix,{v.x,v.y,v.z});Need(terrain.Contains(v3.x,v3.z),"FULL_MESH_TERRAIN_DOMAIN");world.push_back(v3);}
            for(std::size_t face=0;face<mesh.indices.size();face+=3){const std::array<Vec3,3> triangle{{world[mesh.indices[face]],world[mesh.indices[face+1]],world[mesh.indices[face+2]]}};
                const float lower=slot==1?.04f:-.04f;const bool cap=slot==0&&face/3>=5256;
                if(!cap&&(std::min)({triangle[0].y,triangle[1].y,triangle[2].y})-high>=lower)continue;
                const auto range=RootGroundOverlay(triangle,terrain);Need(range.minimum>=lower,"ALL_FACE_PENETRATION");
                if(cap)Need(range.maximum<=-.005f&&Cross(triangle[1]-triangle[0],triangle[2]-triangle[0]).y<0,"DERIVED_ROOT_CAP");}}
    }
    for(unsigned id=0;id<420;++id){const auto& p=r.author.understory.ferns[id];const auto& asset=assets.fernAssets[id%4];
        const auto move=std::find_if(r.recipe.fernMoves.begin(),r.recipe.fernMoves.end(),[&](const auto& m){return m.sourceId==id;});
        if(move==r.recipe.fernMoves.end())Need(Q::Detail::Same(p,old.author.understory.ferns[id]),"UNMOVED_420_SOURCE_BITS");
        else{const auto& before=old.author.understory.ferns[id];Need(Q::Detail::Same(p.rotation,before.rotation)&&Q::Detail::Same(p.scale,before.scale)&&
            std::hypot(p.position.x-before.position.x,p.position.z-before.position.z)<=4.000001f&&
            p.position.y==terrain.SampleHeight(p.position.x,p.position.z)&&terrain.SampleNormal(p.position.x,p.position.z).y>=.88f&&
            ForestCoverFootprint(terrain,p.position,asset.radius*p.scale.x,{}),"MOVED_FERN_POSE_FOOTPRINT");
            for(unsigned other=0;other<420;++other)if(other!=id){const auto& q=r.author.understory.ferns[other];const float d=std::hypot(p.position.x-q.position.x,p.position.z-q.position.z);
                Need(d>=1&&d>=.65f*(asset.radius*p.scale.x+assets.fernAssets[other%4].radius*q.scale.x),"MOVED_FERN_SPACING");}}
        const auto local=CanopyVolume::ConservativeLocalVolume(asset.radius,asset.minimumY,asset.height);
        for(const auto& proxy:r.proxies)Need(CanopyVolume::ClearsCanopyWood(p,local,proxy.parts),"ALL_FERN_ALL_TWELVE_WOOD");
    }
}
void FixedOutputAndGroups(const C::Result& r,const Q::Assets& assets){
    namespace G=GroupedCanopyVolume;
    Need(r.inputIdentity==C::FixedInputIdentity&&r.outputIdentity==C::FixedOutputIdentity&&
        r.woodFingerprint==C::FixedWoodFingerprint&&r.wood.size()==C::FixedWoodParts,"FIXED_RECIPE_OUTPUT_SEALS");
    Need(r.author.firs.firs.size()==C::FixedFirCount&&r.author.props.size()==C::FixedPropCount&&
        r.author.understory.saplings.size()==C::FixedSaplingCount&&r.author.middle.trees.size()==C::FixedMiddleCount,"FIXED_RETAINED_COUNTS");
    const auto body=r.author.WorldSolids(false),before=r.author.VegetationSolids(false),after=r.author.VegetationSolids(true);
    Need(body.size()==C::FixedBodyCount&&before.size()==C::FixedVegetationCount&&after.size()==before.size()+1&&
        before.size()+r.wood.size()==C::FixedCompleteBefore&&after.size()+r.wood.size()==C::FixedCompleteAfter,"BOTH_BRIDGE_FIXED_TOTALS");
    std::vector<G::Group> groups;for(const auto& p:r.proxies)groups.emplace_back(p.parts);
    // 自行按明確little-endian串流核對群組界線，不用 production Measure 產生 Expected。
    std::uint64_t hash=14695981039346656037ull;
    const auto word=[&](std::uint64_t v,unsigned bytes){for(unsigned b=0;b<bytes;++b){hash^=(v>>(b*8))&255u;hash*=1099511628211ull;}};
    word(0x47574356u,4);word(1,4);word(groups.size(),8);std::size_t total{};
    for(unsigned i=0;i<groups.size();++i){const auto group=groups[i];Need(group.size()==C::FixedGroupParts[i],"FIXED_GROUP_BOUNDARY_COUNTS");
        word(0x47525042u,4);word(i,8);word(group.size(),8);total+=group.size();
        for(const auto& b:group)for(float f:{b.center.x,b.center.y,b.center.z,b.extents.x,b.extents.y,b.extents.z})word(std::bit_cast<std::uint32_t>(f),4);
        std::cout<<"WOODLAND_GROUP id="<<i<<" parts="<<group.size()<<" spacing="<<r.proxies[i].spacing
            <<" proxy_fingerprint="<<r.proxies[i].fingerprint<<std::endl;}
    word(0x454e4447u,4);word(total,8);const G::Expected expected{12,C::FixedWoodParts,C::FixedGroupedWoodFingerprint};
    Need(total==C::FixedWoodParts&&hash==C::FixedGroupedWoodFingerprint,"INDEPENDENT_GROUP_FIXED_SEAL");const G::ValidatedWood all(groups,expected);
    for(unsigned i=0;i<420;++i){const auto& f=assets.fernAssets[i%4];Need(all.Clears(r.author.understory.ferns[i],
        CanopyVolume::ConservativeLocalVolume(f.radius,f.minimumY,f.height)),"ALL_420_ACTUAL_GROUPED_CLEARANCE");}
    auto truncated=groups;truncated.pop_back();bool rejected{};
    try{(void)G::ValidatedWood(truncated,expected);}catch(const std::invalid_argument& e){rejected=std::string_view(e.what())=="GROUPED_CANOPY_VOLUME_EXPECTED_GROUP_COUNT";}
    Need(rejected,"ACTUAL_MISSING_LAST_GROUP_REJECTED");
    auto reordered=groups;std::swap(reordered.front(),reordered.back());rejected=false;
    try{(void)G::ValidatedWood(reordered,expected);}catch(const std::invalid_argument& e){rejected=std::string_view(e.what())=="GROUPED_CANOPY_VOLUME_EXPECTED_FINGERPRINT";}
    Need(rejected,"ACTUAL_REORDERED_GROUPS_REJECTED");
    auto brokenLast=r.proxies.back().parts;brokenLast.back().extents.y=0;auto broken=groups;broken.back()=brokenLast;rejected=false;
    try{(void)G::ValidatedWood(broken,expected);}catch(const std::invalid_argument& e){rejected=std::string_view(e.what())=="GROUPED_CANOPY_VOLUME_INVALID_BOX";}
    Need(rejected,"ACTUAL_INVALID_FINAL_PART_REJECTED");
    std::cout<<"WOODLAND_FIXED_OUTPUT body="<<body.size()<<" vegetation="<<before.size()<<" complete_before="<<C::FixedCompleteBefore
        <<" complete_after="<<C::FixedCompleteAfter<<" groups=12 group_fingerprint="<<hash
        <<" all420_grouped=true actual_group_negatives=3 downstream_qualified=false"<<std::endl;
}
}
int wmain(int argc,wchar_t** argv){try{
    Need(argc==3&&std::wstring_view(argv[1])==L"--runtime-assets","USAGE_RUNTIME_ASSETS");
    const auto root=std::filesystem::path(argv[2]);Need(root.is_absolute(),"ABSOLUTE_ASSETS_REQUIRED");
    W::Budget budget;const auto assets=Q::LoadAssets(root);const auto terrain=CreateTerrain();
    const auto source=Q::CreateSourceContext(terrain,assets,budget);
    std::vector<W::Placement> fixed;for(const auto& p:FallenLogPlacements()){W::Placement q;q.propId=p.propId;q.anchor=p.authoredAnchor;q.pose=p.pose;q.collider=p.collider;fixed.push_back(q);}
    const auto context=Q::CreateContext(terrain,assets,source,fixed,budget);Q::ValidateContext(context,terrain,assets,budget);
    EmitCanonicalSnapshot(context,assets);
    const auto input=Q::ContextFingerprint(context);
    for(unsigned i=0;i<8;++i){const auto& p=context.canopy.plants[i];std::cout<<"EXISTING_CANOPY id="<<i<<" pose=";Words(EntranceWorldDetail::PoseWords(p.pose));
        std::cout<<" box=";Words(EntranceWorldDetail::BoxWords(p.lower));std::cout<<" radius="<<p.crownRadius<<std::endl;}
    for(unsigned i=0;i<4;++i){const auto& a=C::Anchors[i];std::cout<<"FIXED_NEW_ANCHOR id="<<i<<" x="<<a.x<<" z="<<a.z<<" yaw="<<a.yaw<<" scale="<<a.scale<<std::endl;}
    const auto inventory=[&](const char* role,unsigned id,const Transform3D& pose,const BoxCollider3D& box){
        std::cout<<"RETAINED_SOURCE role="<<role<<" id="<<id<<" pose=";Words(EntranceWorldDetail::PoseWords(pose));
        std::cout<<" box=";Words(EntranceWorldDetail::BoxWords(box));std::cout<<std::endl;};
    for(unsigned i=0;i<context.author.firs.firs.size();++i){const auto& p=context.author.firs.firs[i];inventory("Fir",context.author.firSourceIds[i],p.render,p.collider);}
    for(unsigned i=0;i<context.author.props.size();++i){const auto& p=context.author.props[i];inventory(p.FallenLog()?"FallenLog":"Rock",context.author.propSourceIds[i],p.transform,p.collider);}
    for(unsigned i=0;i<context.author.understory.saplings.size();++i){const auto& p=context.author.understory.saplings[i];inventory("Sapling",context.author.saplingSourceIds[i],p,SaplingCollider(p));}
    for(unsigned i=0;i<context.author.middle.trees.size();++i){const auto& p=context.author.middle.trees[i];inventory("Middle",context.author.middleSourceIds[i],p.transform,p.trunk);}
    auto bad=context.canopy;bad.plants.pop_back();
    Reject([&]{(void)C::AuthorRecipe(terrain,context.author,bad,assets.canopy,assets.derivedStem,assets.fernAssets);},"canonical-eight-placement");
    // 保持數量與排序仍不代表是同一來源；不得讓新 recipe 替錯 ID 重新背書。
    auto shiftedIds=context.author;for(auto& id:shiftedIds.firSourceIds)id+=1000;
    Reject([&]{(void)C::AuthorRecipe(terrain,shiftedIds,context.canopy,assets.canopy,assets.derivedStem,assets.fernAssets);},"canonical-source-identity");
    auto shiftedRender=context.author;shiftedRender.firs.firs[0].render.position.x+=1.f;
    Reject([&]{(void)C::AuthorRecipe(terrain,shiftedRender,context.canopy,assets.canopy,assets.derivedStem,assets.fernAssets);},"canonical-source-identity");
    auto wrongRole=context.author;wrongRole.props[0].mesh=0;
    Reject([&]{(void)C::AuthorRecipe(terrain,wrongRole,context.canopy,assets.canopy,assets.derivedStem,assets.fernAssets);},"canonical-source-identity");
    auto wrongProp=context.author;wrongProp.props[0].transform.rotation.y+=.1f;
    Reject([&]{(void)C::AuthorRecipe(terrain,wrongProp,context.canopy,assets.canopy,assets.derivedStem,assets.fernAssets);},"canonical-source-identity");
    auto wrongBroadleaf=context.author;wrongBroadleaf.broadleaf[0].transform.position.x+=.5f;
    Reject([&]{(void)C::AuthorRecipe(terrain,wrongBroadleaf,context.canopy,assets.canopy,assets.derivedStem,assets.fernAssets);},"canonical-source-identity");
    auto wrongMiddle=context.author;wrongMiddle.middle.trees[0].transform.rotation.x=.01f;
    Reject([&]{(void)C::AuthorRecipe(terrain,wrongMiddle,context.canopy,assets.canopy,assets.derivedStem,assets.fernAssets);},"canonical-source-identity");
    auto wrongEight=context.canopy;wrongEight.plants[0].pose.rotation.x=.01f;
    Reject([&]{(void)C::AuthorRecipe(terrain,context.author,wrongEight,assets.canopy,assets.derivedStem,assets.fernAssets);},"canonical-source-identity");
    auto smallerFern=assets.fernAssets;smallerFern[0].radius*=.9f;
    Reject([&]{(void)C::AuthorRecipe(terrain,context.author,context.canopy,assets.canopy,assets.derivedStem,smallerFern);},"canonical-source-identity");
    auto missingFernGeometry=assets.fernAssets;missingFernGeometry[0].vertices=0;
    Reject([&]{(void)C::AuthorRecipe(terrain,context.author,context.canopy,assets.canopy,assets.derivedStem,missingFernGeometry);},"canonical-source-identity");
    auto badAuthor=context.author;badAuthor.firSourceIds[1]=badAuthor.firSourceIds[0];
    Reject([&]{(void)C::AuthorRecipe(terrain,badAuthor,context.canopy,assets.canopy,assets.derivedStem,assets.fernAssets);},"source-id-order");
    auto badFerns=assets.fernAssets;badFerns[0].radius=std::numeric_limits<float>::quiet_NaN();
    Reject([&]{(void)C::AuthorRecipe(terrain,context.author,context.canopy,assets.canopy,assets.derivedStem,badFerns);},"fern-asset");
    auto badMeshes=assets.canopy;badMeshes[0].vertices[0].x=std::nextafter(badMeshes[0].vertices[0].x,INFINITY);
    Reject([&]{(void)C::AuthorRecipe(terrain,context.author,context.canopy,badMeshes,assets.derivedStem,assets.fernAssets);},"raw-three-slot-root-stem-seal");
    C::Recipe empty;
    Reject([&]{(void)C::ApplyRecipe(terrain,context.author,context.canopy,assets.canopy,assets.derivedStem,assets.fernAssets,empty);},"recipe-input-identity");
    PreselectAnchors(context,assets,terrain);
    const auto authored=C::AuthorRecipe(terrain,context.author,context.canopy,assets.canopy,assets.derivedStem,assets.fernAssets);
    const auto recipe=C::FixedRecipe();Need(recipe.inputIdentity==authored.inputIdentity&&recipe.withdrawals==authored.withdrawals&&
        recipe.fernMoves==authored.fernMoves,"CHECKED_IN_RECIPE_EXACT_AUTHOR_MATCH");
    Emit(recipe);
    const auto result=C::ApplyRecipe(terrain,context.author,context.canopy,assets.canopy,assets.derivedStem,assets.fernAssets,recipe);
    Independent(result,context,terrain,assets);
    FixedOutputAndGroups(result,assets);
    const auto repeat=C::ApplyRecipe(terrain,context.author,context.canopy,assets.canopy,assets.derivedStem,assets.fernAssets,recipe);
    Need(result.outputIdentity==repeat.outputIdentity&&result.woodFingerprint==repeat.woodFingerprint,"FIXED_RECIPE_REPEAT");
    Need(Q::ContextFingerprint(context)==input,"INPUT_CONTEXT_MUTATED");
    auto altered=recipe;altered.withdrawals.push_back({});
    Reject([&]{(void)C::ApplyRecipe(terrain,context.author,context.canopy,assets.canopy,assets.derivedStem,assets.fernAssets,altered);},"exact-occupied-withdrawal-table");
    std::cout<<"WOODLAND_CANOPY_CPU_PASS appended=4 total=12 negative_count="<<negatives<<" fixed_recipe=true input_identity="<<result.inputIdentity
        <<" output_identity="<<result.outputIdentity<<" wood_parts="<<result.wood.size()<<" wood_fingerprint="<<result.woodFingerprint
        <<" withdrawals="<<recipe.withdrawals.size()<<" fern_moves="<<recipe.fernMoves.size()<<" native_passed=false full_world_qualified=false"<<std::endl;
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<std::endl;return 2;}}
