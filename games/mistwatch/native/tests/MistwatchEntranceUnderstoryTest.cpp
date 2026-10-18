#include "MistwatchEntranceUnderstory.h"
#include "fallen/MistwatchAuthoredNativeExpected.h"
#include <iostream>
using namespace Aevrynd;
using namespace Mistwatch;
namespace Q=::Mistwatch::AuthoredQualification;
namespace W=::Mistwatch::FallenWorldSeat;
static void PrintUnderstoryTrace(const EntranceUnderstoryTrace& trace) {
    std::cout<<"UNDERSTORY_TRACE schema=1 mask_bits=grass:1,roadside:2,fern:4,colony:8,old_shrub:16,new_shrub:32 records="<<trace.attempts.size()<<'\n';
    for(unsigned cluster=0;cluster<6;++cluster){
        std::array<unsigned,5> stages{};std::array<unsigned,64> masks{},samples{};std::array<unsigned,6> overlapping{};
        for(const auto& p:trace.attempts)if(p.cluster==cluster){
            ++stages[static_cast<unsigned>(p.stage)];
            if(p.stage==EntranceUnderstoryStage::Vegetation){++masks[p.vegetationMask];
                for(unsigned bit=0;bit<6;++bit)if(p.vegetationMask&(1u<<bit))++overlapping[bit];}
        }
        std::cout<<"UNDERSTORY_TRACE_CLUSTER id="<<cluster<<" route="<<stages[0]<<" world="<<stages[1]
            <<" terrain="<<stages[2]<<" vegetation="<<stages[3]<<" accepted="<<stages[4]<<'\n';
        for(unsigned bit=0;bit<6;++bit)std::cout<<"UNDERSTORY_TRACE_OVERLAP cluster="<<cluster<<" bit="<<(1u<<bit)<<" count="<<overlapping[bit]<<'\n';
        for(unsigned mask=1;mask<64;++mask)if(masks[mask])std::cout<<"UNDERSTORY_TRACE_MASK cluster="<<cluster<<" mask="<<mask<<" count="<<masks[mask]<<'\n';
        for(const auto& p:trace.attempts)if(p.cluster==cluster&&(p.stage==EntranceUnderstoryStage::Accepted||
            (p.stage==EntranceUnderstoryStage::Vegetation&&samples[p.vegetationMask]++<2)))
            std::cout<<"UNDERSTORY_TRACE_POINT cluster="<<cluster<<" attempt="<<p.ordinal<<" stage="<<static_cast<unsigned>(p.stage)
                <<" mask="<<p.vegetationMask<<" x="<<p.root.x<<" y="<<p.root.y<<" z="<<p.root.z
                <<" radius="<<p.radius<<" height="<<p.height<<" variant="<<p.variant<<'\n';
    }
    std::cout.flush();
}
int wmain(int argc,wchar_t** argv)try{
    const auto need=[](bool ok,const char* why){if(!ok)throw std::runtime_error(why);};
    need(argc==3&&std::wstring_view(argv[1])==L"--runtime-assets","Absolute runtime usage");
    const auto root=std::filesystem::path(argv[2]);need(root.is_absolute(),"Absolute assets required");
    const auto terrain=CreateTerrain();const auto assets=Q::LoadAssets(root);W::Budget budget;
    const auto source=Q::CreateSourceContext(terrain,assets,budget);std::vector<W::Placement> placements;
    for(const auto& p:FallenLogPlacements()){W::Placement q;q.propId=p.propId;q.anchor=p.authoredAnchor;q.pose=p.pose;q.collider=p.collider;placements.push_back(q);}
    const auto c=Q::CreateContext(terrain,assets,source,placements,budget);Q::ValidateContext(c,terrain,assets,budget);
    const auto seal=Q::ContextFingerprint(c);const auto before=W::Boxes(c.worlds[0]),after=W::Boxes(c.worlds[1]);
    std::array<NettleAssetBounds,2> bounds;std::array<std::size_t,2> broadleafTriangles{};
    for(unsigned i=0;i<2;++i){const auto v=RoadsideBroadleafVariants[i];
        auto mesh=LoadAevryndMeshFile(root/"mistwatch-nettle/meshes"/NettleMeshName(v));
        need(bool(mesh),"Actual broadleaf source missing");NormalizeNettleAsset(mesh.mesh,v);bounds[i]=MeasureNettleAssetBounds(mesh.mesh);
        broadleafTriangles[i]=mesh.mesh.indices.size()/3;need(broadleafTriangles[i]==NettleTriangleCounts[v],"Actual broadleaf topology");}
    const auto roadside=CreateRoadsideBroadleaf(terrain,c.retention.cover,c.author.understory,c.colonies,c.shrubs,
        assets.grassAssets,assets.fernAssets,bounds,before,after);
    const auto roadsideSeal=roadside.fingerprint;
    const auto create=[&](const ShrubCanopyResult& shrubs,const RoadsideCoverResult& cover,std::span<const BoxCollider3D> world,EntranceUnderstoryTrace* trace=nullptr){
        return CreateEntranceUnderstory(terrain,c.retention.cover,c.author.understory,c.colonies,shrubs,
            assets.grassAssets,assets.fernAssets,cover,world,after,trace);};
    EntranceUnderstoryTrace trace;EntranceUnderstoryResult out;
    try{out=create(c.shrubs,roadside,before,&trace);}catch(const std::runtime_error& e){
        PrintUnderstoryTrace(trace);const std::string expected=e.what();bool sameFailure=false;
        try{(void)create(c.shrubs,roadside,before);}catch(const std::runtime_error& original){sameFailure=expected==original.what();}
        need(sameFailure&&Q::ContextFingerprint(c)==seal,"Trace changed fail-fast outcome or original world");
        std::cout<<"UNDERSTORY_TRACE_FAILFAST_MATCH=1 diagnosis_only=1 passed=0\n";throw;
    }
    need(trace.attempts.size()==out.attempted&&trace.attempts.size()<=6*2048,"Bounded trace attempt accounting");
    need(out.plants.size()==24&&out.clusterIds.size()==24&&out.left==12&&out.right==12,"Six balanced four-shrub groups");
    need(out.attempted<=6*2048&&out.attempted==24+out.rejectedRoute+out.rejectedWorld+out.rejectedTerrain+out.rejectedVegetation,"Complete bounded attempt accounting");
    need(out.suppressedGrassIds.size()<=180&&out.suppressedBroadleafIds.size()<=48,"Bounded five-percent grass and half-roadside replacement");
    const auto validateSuppression=[&](const std::vector<ForestCoverPlant>& soft,const std::vector<unsigned>& ids,float wind){
        for(unsigned i=0;i<ids.size();++i)need(ids[i]<soft.size()&&(!i||ids[i]>ids[i-1]),"Suppression IDs must be sorted, unique and in source range");
        for(unsigned i=0;i<soft.size();++i){const auto& p=soft[i];bool conflicts=false;
            for(const auto& newPlant:out.plants)conflicts|=std::hypot(p.transform.position.x-newPlant.pose.position.x,
                p.transform.position.z-newPlant.pose.position.z)<(std::max)(.22f,.65f*(newPlant.radius+p.radius+wind));
            need(conflicts==std::binary_search(ids.begin(),ids.end(),i),"Suppression must remove every and only conflicting soft instance");}
    };
    validateSuppression(c.retention.cover.plants,out.suppressedGrassIds,.19f);
    validateSuppression(roadside.plants,out.suppressedBroadleafIds,0.f);
    std::uint64_t suppressionHash=14695981039346656037ull;
    const auto mixSuppression=[&](std::uint32_t word){suppressionHash^=word;suppressionHash*=1099511628211ull;};
    mixSuppression(0x47524153u);mixSuppression(static_cast<std::uint32_t>(out.suppressedGrassIds.size()));
    for(const auto id:out.suppressedGrassIds)mixSuppression(id);
    mixSuppression(0x42524f41u);mixSuppression(static_cast<std::uint32_t>(out.suppressedBroadleafIds.size()));
    for(const auto id:out.suppressedBroadleafIds)mixSuppression(id);
    need(suppressionHash==out.suppressionFingerprint,"Typed suppression fingerprint");
    std::array<unsigned,6> groups{};unsigned innerShoulder{};
    for(unsigned i=0;i<out.plants.size();++i){const auto& p=out.plants[i];
        need(p.variant<4&&out.clusterIds[i]<6,"Variant and group range");const unsigned group=out.clusterIds[i];++groups[group];
        const auto& a=c.shrubs.variants[p.variant];const auto r=p.pose.position;const float s=p.pose.scale.x;
        const auto centre=EntranceUnderstoryCentres[group];const auto half=EntranceUnderstoryHalfExtents[group];
        need(std::abs(r.x-centre.x)<=half.x+.00001f&&std::abs(r.z-centre.z)<=half.z+.00001f,"World fixed cluster envelope");
        need(p.pose.scale.y==s&&p.pose.scale.z==s&&p.pose.rotation.x==0&&p.pose.rotation.z==0,"Undeformed original meshes");
        need(std::abs(r.y+a.minimumY*s-terrain.SampleHeight(r.x,r.z))<.00001f,"Lowest real geometry grounded");
        need((a.maximumY-a.minimumY)*s>=.64999f&&(a.maximumY-a.minimumY)*s<=1.00001f,"Visible waist-height silhouette");
        need(p.radius==a.radius*s+.19f,"Complete world wind radius");
        for(unsigned sample=0;sample<=32;++sample){const float z=r.z-p.radius+2*p.radius*float(sample)/32.f;
            need(std::abs(r.x-.35f*std::sin(z*.19f))-p.radius>=2.1f+.15f*std::sin(z*.31f)+.08f,"Complete visible main road clear");}
        need(RoadsideCoverRouteClearance(r,p.radius),"Complete mission branches clear");
        for(const auto point:Session::supplies){
            need(std::hypot((std::max)(0.f,std::abs(r.x-point.x)-p.radius),
                (std::max)(0.f,std::abs(r.z-point.z)-p.radius))>=3.f,"Supply interaction approach clear");
            need(!(r.z-p.radius<point.z+2&&r.z+p.radius>point.z-2&&r.x-p.radius<(std::max)(0.f,point.x)+2&&
                r.x+p.radius>(std::min)(0.f,point.x)-2),"Supply lateral access clear");}
        const CanopyVolume::PlantVolume volume{r,CanopyVolume::ConservativeLocalVolume(a.radius+.19f/s,
            a.minimumY,a.maximumY+.011f/s),s,p.pose.rotation};
        need(CanopyVolume::ClearsCanopyWood(volume,before)&&CanopyVolume::ClearsCanopyWood(volume,after),"Complete world wood, both bridge states");
        need(ForestCoverFootprint(terrain,{r.x,terrain.SampleHeight(r.x,r.z),r.z},p.radius,{}),"Full true terrain footprint");
        const auto separated=[&](Vec3 other,float minimum){need(std::hypot(r.x-other.x,r.z-other.z)>=minimum,"Original or new vegetation root conflict");};
        for(unsigned j=0;j<c.retention.cover.plants.size();++j)if(!std::binary_search(out.suppressedGrassIds.begin(),out.suppressedGrassIds.end(),j)){
            const auto& old=c.retention.cover.plants[j];separated(old.transform.position,(std::max)(.22f,.65f*(p.radius+old.radius+.19f)));}
        for(unsigned j=0;j<roadside.plants.size();++j)if(!std::binary_search(out.suppressedBroadleafIds.begin(),out.suppressedBroadleafIds.end(),j)){
            const auto& old=roadside.plants[j];separated(old.transform.position,(std::max)(.22f,.65f*(p.radius+old.radius)));}
        for(unsigned j=0;j<c.author.understory.ferns.size();++j){const auto& old=c.author.understory.ferns[j];
            separated(old.position,p.radius+assets.fernAssets[j%4].radius*old.scale.x+.19f+.03f);}
        for(const auto& old:c.colonies.plants)separated(old.transform.position,p.radius+old.radius+.19f+.03f);
        for(const auto& old:c.shrubs.plants)separated(old.pose.position,p.radius+old.radius+.19f+.03f);
        for(unsigned j=0;j<i;++j)separated(out.plants[j].pose.position,p.radius+out.plants[j].radius+.03f);
        if(!PropClearsObjectives({r,{p.radius,1,p.radius}}))++innerShoulder;
    }
    for(unsigned i=0;i<6;++i)need(groups[i]==4&&groups[i]==out.clusterCounts[i],"Every world group complete");
    need(innerShoulder>=12,"New layer must occupy actual bare shoulder");
    const auto repeat=create(c.shrubs,roadside,before);
    need(repeat.fingerprint==out.fingerprint&&repeat.attempted==out.attempted&&repeat.clusterIds==out.clusterIds,"Deterministic author rerun");
    need(repeat.suppressedGrassIds==out.suppressedGrassIds&&repeat.suppressedBroadleafIds==out.suppressedBroadleafIds&&
        repeat.suppressionFingerprint==out.suppressionFingerprint,"Deterministic suppression selection");
    for(unsigned i=0;i<24;++i){const auto& a=out.plants[i];const auto& b=repeat.plants[i];
        need(a.variant==b.variant&&a.radius==b.radius&&a.pose.position.x==b.pose.position.x&&a.pose.position.y==b.pose.position.y&&
            a.pose.position.z==b.pose.position.z&&a.pose.rotation.y==b.pose.rotation.y&&a.pose.scale.x==b.pose.scale.x,"Deterministic full transforms");}
    unsigned negatives{};const auto reject=[&](const ShrubCanopyResult& shrub,const RoadsideCoverResult& cover,std::span<const BoxCollider3D> world,const char* expected){
        bool rejected=false;try{(void)create(shrub,cover,world);}catch(const std::invalid_argument& e){need(std::string_view(e.what())==expected,"Wrong invalid input rejection");rejected=true;}
        need(rejected,"Malformed input accepted");++negatives;};
    reject(c.shrubs,roadside,std::span<const BoxCollider3D>(before).first(before.size()-1),"ROADSIDE_COMPLETE_TWO_STATE_WORLD_REQUIRED");
    auto bad=roadside;bad.plants.pop_back();reject(c.shrubs,bad,before,"ENTRANCE_UNDERSTORY_COMPLETE_BROADLEAF_REQUIRED");
    bad=roadside;bad.plants[0].radius=std::numeric_limits<float>::quiet_NaN();reject(c.shrubs,bad,before,"ENTRANCE_UNDERSTORY_INVALID_BROADLEAF");
    auto shrub=c.shrubs;shrub.variants[0].radius*=.5f;reject(shrub,roadside,before,"ENTRANCE_UNDERSTORY_ACTUAL_GEOMETRY_REQUIRED");
    shrub=c.shrubs;shrub.variants[0].minimumY+=.1f;reject(shrub,roadside,before,"ENTRANCE_UNDERSTORY_ACTUAL_GEOMETRY_REQUIRED");
    need(!RoadsideCoverRouteClearance({0,0,0},.7f)&&!RoadsideCoverRouteClearance(Session::supplies[0],.7f)&&
        !RoadsideCoverRouteClearance({4,0,35},.7f),"Forbidden road, objective, river remain forbidden");
    need(Q::ContextFingerprint(c)==seal&&roadside.fingerprint==roadsideSeal,"Original complete context unchanged");
    const auto originalAgain=CreateRoadsideBroadleaf(terrain,c.retention.cover,c.author.understory,c.colonies,c.shrubs,
        assets.grassAssets,assets.fernAssets,bounds,before,after);
    need(originalAgain.fingerprint==roadsideSeal&&originalAgain.plants.size()==96,"Existing 96 broadleaf retained");
    std::array<unsigned,5> baselineVariants{},drawVariants{};unsigned baselineGrass{},drawGrass{},drawBroadleaf{};
    std::size_t baselineTriangles{},drawGrassTriangles{},drawBroadleafTriangles{};
    for(unsigned variant=0;variant<5;++variant)need(assets.grass[variant].indices.size()/3==GrassTriangleCounts[variant],"Actual scanned grass triangles");
    const Vec3 initialFeet=Session{}.feet;
    for(unsigned i=0;i<c.retention.cover.plants.size();++i){const auto& p=c.retention.cover.plants[i];
        if(!GrassRootSelected(p.transform.position,initialFeet))continue;
        const auto triangles=assets.grass[p.variant].indices.size()/3;++baselineGrass;++baselineVariants[p.variant];baselineTriangles+=triangles;
        if(std::binary_search(out.suppressedGrassIds.begin(),out.suppressedGrassIds.end(),i))continue;
        ++drawGrass;++drawVariants[p.variant];drawGrassTriangles+=triangles;
    }
    for(unsigned i=0;i<roadside.plants.size();++i){const auto& p=roadside.plants[i];
        if(std::binary_search(out.suppressedBroadleafIds.begin(),out.suppressedBroadleafIds.end(),i)||!GrassRootSelected(p.transform.position,initialFeet))continue;
        ++drawBroadleaf;drawBroadleafTriangles+=broadleafTriangles[p.variant];}
    std::cout<<"DRAW_EXPECTED submitted_grass="<<drawGrass<<'\n'
        <<"DRAW_EXPECTED submitted_grass_triangles="<<drawGrassTriangles<<'\n'
        <<"DRAW_EXPECTED submitted_grass_variants="<<drawVariants[0]<<','<<drawVariants[1]<<','<<drawVariants[2]<<','<<drawVariants[3]<<','<<drawVariants[4]<<'\n'
        <<"DRAW_EXPECTED roadside_broadleaf_submitted="<<drawBroadleaf<<'\n'
        <<"DRAW_EXPECTED roadside_broadleaf_submitted_triangles="<<drawBroadleafTriangles<<'\n'
        <<"BASELINE_DRAW_EXPECTED submitted_grass="<<baselineGrass<<'\n'
        <<"BASELINE_DRAW_EXPECTED submitted_grass_triangles="<<baselineTriangles<<'\n'
        <<"BASELINE_DRAW_EXPECTED submitted_grass_variants="<<baselineVariants[0]<<','<<baselineVariants[1]<<','<<baselineVariants[2]<<','<<baselineVariants[3]<<','<<baselineVariants[4]<<'\n';
    std::cout<<"UNDERSTORY_SUPPRESSED_GRASS_IDS=";for(unsigned i=0;i<out.suppressedGrassIds.size();++i)std::cout<<(i?",":"")<<out.suppressedGrassIds[i];
    std::cout<<"\nUNDERSTORY_SUPPRESSED_BROADLEAF_IDS=";for(unsigned i=0;i<out.suppressedBroadleafIds.size();++i)std::cout<<(i?",":"")<<out.suppressedBroadleafIds[i];std::cout<<'\n';
    std::cout<<"UNDERSTORY_EXPECTED entrance_understory_population="<<out.plants.size()<<'\n'
        <<"UNDERSTORY_EXPECTED entrance_understory_fingerprint="<<out.fingerprint<<'\n'
        <<"UNDERSTORY_EXPECTED entrance_understory_attempted="<<out.attempted<<'\n'
        <<"UNDERSTORY_EXPECTED entrance_understory_left="<<out.left<<'\n'
        <<"UNDERSTORY_EXPECTED entrance_understory_right="<<out.right<<'\n'
        <<"UNDERSTORY_EXPECTED entrance_understory_clusters="<<out.clusterCounts.size()<<'\n'
        <<"UNDERSTORY_EXPECTED entrance_understory_suppressed_grass="<<out.suppressedGrassIds.size()<<'\n'
        <<"UNDERSTORY_EXPECTED entrance_understory_suppressed_broadleaf="<<out.suppressedBroadleafIds.size()<<'\n'
        <<"UNDERSTORY_EXPECTED entrance_understory_suppression_fingerprint="<<out.suppressionFingerprint<<'\n'
        <<"UNDERSTORY_CPU_PASS original_unchanged=1 broadleaf_unchanged=1 both_worlds=1 negative_controls="<<negatives
        <<" visible_height_min=.65 inner_shoulder="<<innerShoulder<<" native=0 art=0\n";return 0;
}catch(const std::exception& e){std::cerr<<"UNDERSTORY_FIRST_DIVERGENCE "<<e.what()<<'\n';return 1;}
