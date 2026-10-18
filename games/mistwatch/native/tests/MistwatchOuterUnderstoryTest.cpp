#include "MistwatchOuterUnderstory.h"
#include "MistwatchForestHabitatFloor.h"
#include <Aevrynd/MeshLoader.h>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <map>
#include <string>

namespace {
using namespace Aevrynd;
namespace U=Mistwatch::OuterUnderstory;
namespace S=Mistwatch::LandscapeStandFill;
std::size_t checks{},negatives{};
unsigned differentialOverlays{},differentialSupports{};
unsigned supportObservations{};
unsigned restingFootFailures{},windBoundOnlyFootFailures{};
void Need(bool yes,const char* reason){++checks;if(!yes)throw std::runtime_error(reason);}
std::uint64_t Hash(const Mesh3D& mesh){
    std::uint64_t h=14695981039346656037ull;const auto word=[&](std::uint32_t value){for(unsigned i=0;i<4;++i){h^=(value>>(i*8))&255u;h*=1099511628211ull;}};
    word(static_cast<std::uint32_t>(mesh.vertices.size()));word(static_cast<std::uint32_t>(mesh.indices.size()));
    for(const auto& v:mesh.vertices){for(float f:{v.x,v.y,v.z,v.u,v.v})word(std::bit_cast<std::uint32_t>(f));word(v.normal);word(v.color);}for(auto i:mesh.indices)word(i);return h;
}
std::uint64_t Hash(const U::Plan& plan){
    std::uint64_t h=14695981039346656037ull;const auto word=[&](std::uint32_t value){for(unsigned i=0;i<4;++i){h^=(value>>(i*8))&255u;h*=1099511628211ull;}};
    for(const unsigned char c:plan.proposalRevision)word(c);
    word(static_cast<std::uint32_t>(plan.footHintFingerprint));word(static_cast<std::uint32_t>(plan.footHintFingerprint>>32));
    word(static_cast<unsigned>(plan.plants.size()));
    for(const auto& p:plan.plants){for(float f:{p.pose.position.x,p.pose.position.y,p.pose.position.z,p.pose.rotation.x,p.pose.rotation.y,p.pose.rotation.z,p.pose.scale.x,p.pose.scale.y,p.pose.scale.z})word(std::bit_cast<std::uint32_t>(f));
        word(p.slot);word(p.band);word(p.cluster);word(p.anchorTree);for(float f:{p.groundRoot.x,p.groundRoot.y,p.groundRoot.z,p.radius})word(std::bit_cast<std::uint32_t>(f));}
    return h;
}
struct Assets {
    std::array<Mesh3D,11> plants;std::array<Mesh3D,5> trees;
    U::Sources Bind()const{U::Sources s;for(unsigned i=0;i<11;++i){s.plants[i]=&plants[i];s.fingerprints[i]=Hash(plants[i]);}
        for(unsigned i=0;i<5;++i){s.trees[i]=&trees[i];s.treeFingerprints[i]=Hash(trees[i]);}return s;}
};
Mesh3D Read(const std::filesystem::path& path){auto loaded=LoadAevryndMeshFile(path);Need(bool(loaded)&&loaded.mesh.Valid(),"missing actual runtime source");return std::move(loaded.mesh);}
Assets Load(const std::filesystem::path& root){Assets a;
    for(unsigned i=0;i<5;++i){a.plants[i]=Read(root/"mistwatch-grass"/"meshes"/Mistwatch::GrassMeshName(i));(void)Mistwatch::NormalizeGrassAsset(a.plants[i]);}
    for(unsigned i=0;i<4;++i){a.plants[5+i]=Read(root/"mistwatch-ferns"/"meshes"/Mistwatch::FernMeshName(i));Mistwatch::NormalizeFernAsset(a.plants[5+i]);}
    for(unsigned i=0;i<2;++i){a.plants[9+i]=Read(root/"mistwatch-nettle"/"meshes"/Mistwatch::NettleMeshName(i+2));(void)Mistwatch::NormalizeNettleAsset(a.plants[9+i],i+2);}
    constexpr std::array<const char*,4> names{"bark","twig","dead-branches","trunk-c"};
    for(unsigned i=0;i<5;++i){const unsigned slot=i==4?1:i;const std::string lod=i==4?"1":"2";
        a.trees[i]=Read(root/"mistwatch-forest"/"meshes"/("fir-tree-01-c-lod"+lod+"-node0-mesh0-primitive"+std::to_string(slot)+"-material"+std::to_string(slot)+"-fir-tree-01-"+names[slot]+".aemesh"));}
    return a;
}
MeshBounds3D Bounds(const Assets& assets){Vec3 lo{10000,10000,10000},hi{-10000,-10000,-10000};
    for(const auto& mesh:assets.trees)for(const auto& v:mesh.vertices){lo.x=(std::min)(lo.x,v.x);lo.y=(std::min)(lo.y,v.y);lo.z=(std::min)(lo.z,v.z);
        hi.x=(std::max)(hi.x,v.x);hi.y=(std::max)(hi.y,v.y);hi.z=(std::max)(hi.z,v.z);}
    const auto extent=(hi-lo)*.5f;return {(hi+lo)*.5f,Length(extent),extent};}
template<class F>void Reject(F action,const char* reason){try{action();}catch(const std::invalid_argument& e){Need(std::string_view(e.what())==reason,"wrong rejection reason");++negatives;return;}throw std::runtime_error("negative accepted");}
// 參考使用4m浮點索引和獨立double重心方程，不呼叫候選的8m rational surface。
class ReferenceSurface {
    const Mesh3D& mesh_;std::map<std::pair<int,int>,std::vector<std::size_t>> cells_;
public:
    explicit ReferenceSurface(const Mesh3D& mesh):mesh_(mesh){for(std::size_t f=0;f<mesh.indices.size();f+=3){
        const auto& a=mesh.vertices[mesh.indices[f]];const auto& b=mesh.vertices[mesh.indices[f+1]];const auto& c=mesh.vertices[mesh.indices[f+2]];
        const int x0=static_cast<int>(std::floor((std::min)({a.x,b.x,c.x})/4))-1,x1=static_cast<int>(std::floor((std::max)({a.x,b.x,c.x})/4))+1;
        const int z0=static_cast<int>(std::floor((std::min)({a.z,b.z,c.z})/4))-1,z1=static_cast<int>(std::floor((std::max)({a.z,b.z,c.z})/4))+1;
        for(int z=z0;z<=z1;++z)for(int x=x0;x<=x1;++x)cells_[{x,z}].push_back(f);}}
    double Height(double x,double z)const{const auto it=cells_.find({static_cast<int>(std::floor(x/4)),static_cast<int>(std::floor(z/4))});Need(it!=cells_.end(),"reference surface missing cell");
        bool found=false;double result{};
        for(const auto f:it->second){const auto& a=mesh_.vertices[mesh_.indices[f]];const auto& b=mesh_.vertices[mesh_.indices[f+1]];const auto& c=mesh_.vertices[mesh_.indices[f+2]];
            const double det=(double(b.z)-c.z)*(double(a.x)-c.x)+(double(c.x)-b.x)*(double(a.z)-c.z);
            const double wa=((double(b.z)-c.z)*(x-c.x)+(double(c.x)-b.x)*(z-c.z))/det;
            const double wb=((double(c.z)-a.z)*(x-c.x)+(double(a.x)-c.x)*(z-c.z))/det,wc=1-wa-wb;
            if(wa< -1e-9||wb< -1e-9||wc< -1e-9)continue;const double h=wa*a.y+wb*b.y+wc*c.y;
            if(found)Need(std::abs(h-result)<1e-5,"ambiguous reference triangle height");else{found=true;result=h;}}
        Need(found,"reference actual triangle missing");return result;
    }
};
Mesh3D Plane(float slope=0){Mesh3D mesh;for(const auto p:std::array<Vec3,4>{{{-2,-2*slope,-2},{2,2*slope,-2},{-2,-2*slope,2},{2,2*slope,2}}})
        mesh.vertices.push_back({p.x,p.y,p.z,PackNormalSnorm8(Normalize({-slope,1,0})),0xffffffffu});mesh.indices={0,2,1,1,2,3};return mesh;}
Mesh3D Blade(float shift=0){Mesh3D mesh;
    for(const auto p:std::array<Vec3,3>{{{-.01f,shift,0},{.01f,shift,0},{0,.4f+shift,0}}})mesh.vertices.push_back({p.x,p.y,p.z,PackNormalSnorm8({0,0,1},1),0xffffffffu});
    mesh.indices={0,1,2};return mesh;}
void SyntheticControls(){
    // 這些微型幾何僅驗支撐算法，不是正式世界或視覺證據。
    const auto flat=Plane(),steep=Plane(.6f),blade=Blade();const Mistwatch::LandscapeSpeciesDetail::Surface surface(flat),slope(steep);
    U::Plant plant;plant.pose.scale={1,1,1};plant.radius=.3f;
    const auto safe=U::Detail::Support(surface,blade,plant);Need(safe.safe&&safe.minimumGapBound>=-.04f&&safe.maximumFootGapBound<=.04f,"synthetic upright base support");
    Need(!U::Detail::Support(slope,blade,plant).safe,"steep actual face rejected");++negatives;
    Need(!U::Detail::Support(surface,Blade(.10f),plant).safe,"floating real base rejected");++negatives;
    Need(!U::Detail::Support(surface,Blade(-.10f),plant).safe,"buried real base rejected");++negatives;
    auto flexible=blade;for(auto& v:flexible.vertices)v.normal=PackNormalSnorm8({0,0,1},0);
    const auto diagnosed=U::Detail::Support(surface,flexible,plant,true,true);
    const auto regular=U::Detail::Support(surface,flexible,plant);
    Need(!diagnosed.safe&&diagnosed.rejection==U::Reject::Foot&&diagnosed.restingFootObserved&&!diagnosed.restingFootAboveLimit&&
        diagnosed.maximumRestingFootGap<=U::ContactLimit&&diagnosed.maximumFootGapBound>U::ContactLimit,"resting-foot diagnostic confused wind bound with static gap");
    Need(diagnosed.safe==regular.safe&&diagnosed.rejection==regular.rejection&&diagnosed.maximumFootGapBound==regular.maximumFootGapBound,"diagnostic changed acceptance or first rejection");
    const auto moderate=Plane(.1f);const Mistwatch::LandscapeSpeciesDetail::Surface tilted(moderate);auto displaced=plant;
    displaced.pose.position.x=-.2f;displaced.radius=.5f;
    const auto realFloat=U::Detail::Support(tilted,blade,displaced,true,true);
    Need(!realFloat.safe&&realFloat.rejection==U::Reject::Foot&&realFloat.restingFootObserved&&realFloat.restingFootAboveLimit&&
        realFloat.maximumRestingFootGap>U::ContactLimit,"resting-foot diagnostic ignored actual stationary floating");++negatives;
    auto hole=flat;hole.indices.resize(3);const Mistwatch::LandscapeSpeciesDetail::Surface missing(hole);
    Need(!U::Detail::Support(missing,blade,plant).safe,"incomplete footprint coverage rejected");++negatives;
    auto peak=flat;peak.vertices.push_back({0,.3f,0,PackNormalSnorm8({0,1,0}),0xffffffffu});peak.indices={0,2,4,2,3,4,3,1,4,1,0,4};
    const Mistwatch::LandscapeSpeciesDetail::Surface peaked(peak);Need(!U::Detail::Support(peaked,blade,plant).safe,"interior peak not hidden by footprint corners");++negatives;
    const Mistwatch::CanopyVolume::PlantLocalVolume volume{.5f,0,1};plant.pose.position={-53.55f,0,0};plant.groundRoot=plant.pose.position;plant.radius=.69f;
    Need(!U::Detail::Domain(plant,volume),"wind envelope core violation rejected");++negatives;
    plant.pose.position.x=-55;Need(U::Detail::Domain(plant,volume),"whole outer envelope accepted");
    const std::array<BoxCollider3D,1> box{{{{-55.65f,.5f,0},{.03f,.3f,.03f}}}};
    Need(!Mistwatch::CanopyVolume::ClearsCanopyWood(U::Detail::Volume(plant,volume),box),"wind envelope wood contact rejected");++negatives;
}
void OverlayEquivalence(const Mesh3D& mesh){
    namespace D=Mistwatch::LandscapeSpeciesDetail;
    const D::Surface surface(mesh);U::SupportProof statistics;
    const std::array<D::P,4> footprint{D::Exact::ToExact(Vec3{-2,0,-2}),D::Exact::ToExact(Vec3{2,0,-2}),
        D::Exact::ToExact(Vec3{2,0,2}),D::Exact::ToExact(Vec3{-2,0,2})};
    U::Detail::GroundCache cache(surface,footprint,statistics);
    std::vector<D::T> faces;
    for(const auto points:std::array<std::array<Vec3,3>,7>{{
        {{{-.7f,.1f,-.6f},{.8f,.2f,-.5f},{.1f,.3f,.9f}}},
        {{{0,0,0},{0,.4f,0},{0,.1f,1}}},
        {{{-2,0,-2},{-2,.2f,-2},{-2,.4f,-2}}},
        {{{-2,.2f,-2},{2,.2f,2},{2,.2f,2}}},
        {{{-1.9f,0,-1.9f},{-1.8f,.1f,-1.9f},{-1.9f,.2f,-1.8f}}},
        {{{1.9f,0,1.9f},{1.8f,.1f,1.9f},{1.9f,.2f,1.8f}}},
        {{{-2,0,2},{-2,.2f,2},{-1.9f,.2f,2}}}
    }})faces.push_back({D::Exact::ToExact(points[0]),D::Exact::ToExact(points[1]),D::Exact::ToExact(points[2])});
    // 微小精確分離與相同邊接觸都必須保留正確的空／非空結果。
    auto separated=faces[2];for(auto& p:separated)p[0]=p[0]-D::R::Float(.000000001f);faces.push_back(separated);
    // 非零的極小投影面積仍必須使用精確平面，不得套 epsilon 當成線。
    const D::T tiny{D::Exact::ToExact(Vec3{0,.01f,0}),D::Exact::ToExact(Vec3{.0000001f,.03f,0}),D::Exact::ToExact(Vec3{0,.04f,.0000001f})};
    faces.push_back(tiny);faces.push_back({tiny[2],tiny[1],tiny[0]});
    const std::array<U::Detail::WeightedPoint,3> raw{{
        {D::Exact::ToExact(Vec3{-.7f,.01f,-.3f}),D::R::Float(.2f)},
        {D::Exact::ToExact(Vec3{.8f,.09f,-.2f}),D::R::Float(.7f)},
        {D::Exact::ToExact(Vec3{.1f,.3f,.9f}),D::R::Float(.9f)}}};
    const auto clipped=U::Detail::Foot(raw,D::R::Float(.035f));Need(clipped.size()>=3,"rational clipped plane control missing");
    for(std::size_t i=1;i+1<clipped.size();++i)faces.push_back({clipped[0].point,clipped[i].point,clipped[i+1].point});
    for(const auto& face:faces)for(const float shift:{-.041f,0.f,.043f}){
        auto shifted=face;for(auto& point:shifted)point[1]=point[1]+D::R::Float(shift);
        const auto old=D::Overlay(shifted,surface),now=cache.Overlay(shifted);
        Need(old.points==now.points,"cached exact overlay contribution count changed");
        if(old.points)Need(Compare(old.minimum,now.minimum)==0&&Compare(old.maximum,now.maximum)==0,"cached exact overlay range changed");
        ++differentialOverlays;
    }
    Need(statistics.aabbRejected>0&&statistics.exactOverlayFaces>0,"both exact broad/narrow paths exercised");
    Need(statistics.groundHeightPlanes==statistics.cachedGroundFaces&&statistics.sourceHeightPlanes>0&&statistics.cachedHeightEvaluations>0,"actual exact height caches not exercised");
}
void SupportEquivalence(const Mesh3D& landscape,const Mesh3D& source,U::Plant plant){
    const Mistwatch::LandscapeSpeciesDetail::Surface surface(landscape);
    const auto old=U::Detail::Support(surface,source,plant,false),now=U::Detail::Support(surface,source,plant,true);
    Need(old.safe==now.safe&&old.rejection==now.rejection&&old.minimumNormalY==now.minimumNormalY&&
        old.maximumHeightDelta==now.maximumHeightDelta&&old.minimumGapBound==now.minimumGapBound&&
        old.maximumFootGapBound==now.maximumFootGapBound&&old.footprintFaces==now.footprintFaces&&
        old.evaluatedFaces==now.evaluatedFaces&&old.skippedFaces==now.skippedFaces&&old.footPieces==now.footPieces,"complete cached support result changed");
    ++differentialSupports;
}
void CacheControls(){
    const auto flat=Plane(),tilted=Plane(.2f);auto hole=flat;hole.indices.resize(3);
    auto peak=flat;peak.vertices.push_back({0,.3f,0,PackNormalSnorm8({0,1,0}),0xffffffffu});peak.indices={0,2,4,2,3,4,3,1,4,1,0,4};
    const std::array<const Mesh3D*,4> surfaces{&flat,&tilted,&hole,&peak};
    for(const auto* mesh:surfaces)OverlayEquivalence(*mesh);
    U::Plant plant;plant.pose.scale={1,1,1};plant.radius=.3f;
    for(const auto* mesh:surfaces)for(const float y:{-.1f,0.f,.1f})SupportEquivalence(*mesh,Blade(y),plant);
    auto wind=Blade();wind.vertices.back().normal=PackNormalSnorm8({0,0,1},0);
    for(const auto* mesh:{&flat,&tilted})SupportEquivalence(*mesh,wind,plant);
    std::cout<<"OUTER_UNDERSTORY_CACHE_DIFFERENTIAL_PASS overlays="<<differentialOverlays<<" supports="<<differentialSupports<<std::endl;
}
Mesh3D RankingGrid(float originX,float originZ,unsigned cells,float step,bool slope=false,bool peak=false){
    Mesh3D mesh;
    for(unsigned z=0;z<=cells;++z)for(unsigned x=0;x<=cells;++x){
        const float px=originX+static_cast<float>(x)*step,pz=originZ+static_cast<float>(z)*step;
        float height=slope?.8f*(std::max)(0.f,px+90.f):.125f*px-.0625f*pz;
        if(peak)height=(x==5&&z==5)?.3f:0.f;
        mesh.vertices.push_back({px,height,pz,PackNormalSnorm8({0,1,0}),0xffffffffu});
    }
    for(unsigned z=0;z<cells;++z)for(unsigned x=0;x<cells;++x){const unsigned a=z*(cells+1)+x,b=a+1,c=a+cells+1,d=c+1;
        mesh.indices.insert(mesh.indices.end(),{a,c,b,b,c,d});}
    return mesh;
}
void RankingControls(){
    namespace R=U::TerrainAware;
    const auto mesh=RankingGrid(-16,-16,16,2);R::Statistics stats;const R::Index index(mesh,stats);const ReferenceSurface reference(mesh);
    unsigned samples{};
    // 真面頂點、共用邊、8m格界及格界前後都與独立重心方程比較。
    for(double x:{-16.,-8.,-8.000001,-7.999999,0.,7.999999,8.,8.000001,16.})
        for(double z:{-16.,-8.,0.,8.,16.}){const auto point=index.At(x,z,stats);Need(point.has_value(),"ranking lost real triangle seam");
            Need(std::abs(point->height-reference.Height(x,z))<1e-10&&std::abs(point->dx-.125)<1e-10&&std::abs(point->dz+.0625)<1e-10,"ranking independent actual triangle sample");++samples;}
    Need(!index.At(16.01,0,stats)&&!index.At(std::numeric_limits<double>::quiet_NaN(),0,stats),"ranking missing sample is not safe");++negatives;
    auto bad=mesh;bad.vertices[0].x=std::numeric_limits<float>::quiet_NaN();Reject([&]{R::Statistics s;(void)R::Index(bad,s);},"OUTER_RANK_SURFACE_VERTEX");
    bad=mesh;bad.indices={0,0,0};Reject([&]{R::Statistics s;(void)R::Index(bad,s);},"OUTER_RANK_PROJECTED_FACE");
    bad=mesh;bad.indices.pop_back();Reject([&]{R::Statistics s;(void)R::Index(bad,s);},"OUTER_RANK_SURFACE_TOPOLOGY");
    auto overlap=Plane();for(auto& vertex:overlap.vertices){vertex.x*=4;vertex.z*=4;}
    const auto count=static_cast<unsigned>(overlap.vertices.size());const auto oldVertices=overlap.vertices;
    for(auto vertex:oldVertices){vertex.y+=1;overlap.vertices.push_back(vertex);}
    overlap.indices.insert(overlap.indices.end(),{count,count+2,count+1,count+1,count+2,count+3});
    R::Statistics overlappingStats;const R::Index overlapping(overlap,overlappingStats);
    Need(!overlapping.At(0,0,overlappingStats),"ranking conflicting overlapping surfaces cannot suggest height");++negatives;
    auto large=Plane();for(auto& vertex:large.vertices){vertex.x*=100;vertex.z*=100;}
    Reject([&]{R::Statistics s;(void)R::Index(large,s);},"OUTER_RANK_FACE_RESOURCE_BOUND");
    const auto ground=RankingGrid(-106,0,16,2,true);R::Statistics ranking;const R::Index terrain(ground,ranking);
    const Transform3D tree{{-90,0,16},{},{1,1,1}};
    std::array<Mistwatch::CanopyVolume::PlantLocalVolume,11> volumes;volumes.fill({.2f,0,1});
    const auto centres=R::RankCentres(terrain,tree,520,0,volumes,ranking);
    const auto again=R::RankCentres(terrain,tree,520,0,volumes,ranking);
    Need(centres.front().worstSpeciesCapacity>0,"ranking did not prefer available flatter slope");
    const auto steep=std::find_if(centres.begin(),centres.end(),[](const auto& c){return c.ordinal==32;});
    Need(steep!=centres.end()&&steep->inDomain&&steep->worstSpeciesCapacity<centres.front().worstSpeciesCapacity,"ranking steep candidate precedes flat candidate");
    for(unsigned i=0;i<R::CentreCount;++i){const auto& a=centres[i];const auto& b=again[i];
        Need(a.ordinal==b.ordinal&&std::bit_cast<std::uint32_t>(a.position.x)==std::bit_cast<std::uint32_t>(b.position.x)&&
            std::bit_cast<std::uint32_t>(a.position.z)==std::bit_cast<std::uint32_t>(b.position.z)&&a.score==b.score&&
            a.worstSpeciesCapacity==b.worstSpeciesCapacity&&a.suggestedTotal==b.suggestedTotal,"fixed centre ranking replay changed");}
    R::RootPool pool(terrain,centres.front(),volumes,ranking),replayed(terrain,again.front(),volumes,ranking);
    for(unsigned slot=0;slot<11;++slot){std::array<bool,R::RootCount> seen{};
        for(unsigned i=0;i<R::RootCount;++i){const auto a=pool.Next(slot),b=replayed.Next(slot);Need(a&&b&&!seen[a->ordinal],"root pool duplicated proposal");seen[a->ordinal]=true;
            Need(a->ordinal==b->ordinal&&a->position.x==b->position.x&&a->position.z==b->position.z&&a->scale==b->scale&&a->yaw==b->yaw&&a->score==b->score,"root pool replay depends on consumed proposals");
            Need(a->position.y==0&&std::hypot(double(a->position.x)-centres.front().position.x,double(a->position.z)-centres.front().position.z)<=4.00001,"ranking fabricated root height or larger search disk");
            if(slot<5)Need(a->scale==Mistwatch::ForestCoverScale(a->position),"ranking shrank grass scale");
            else if(slot<9)Need(a->scale>=.6f&&a->scale<=.78f,"ranking shrank fern scale");
            else {const float height=a->scale*(volumes[slot].maximumY-volumes[slot].minimumY);
                Need(slot==9?(height>=.26f&&height<=.35f):(height>=.10f&&height<=.16f),"ranking shrank broadleaf scale");}}
        Need(!pool.Next(slot),"root pool exceeded fixed256 proposals");}
    Reject([&]{(void)pool.Next(11);},"OUTER_RANK_SLOT");
    Reject([&]{(void)R::RankCentres(terrain,tree,519,0,volumes,ranking);},"OUTER_RANK_ANCHOR");
    Reject([&]{(void)R::RankCentres(terrain,tree,520,6,volumes,ranking);},"OUTER_RANK_ANCHOR");
    Reject([&]{(void)R::MakeRoot(centres.front(),256,256,0,volumes[0]);},"OUTER_RANK_ROOT_INPUT");
    // 五點近似看不見內部峰谷時，完整rational Support仍必須拒絕。
    const auto peaked=RankingGrid(-.5f,-.5f,8,.125f,false,true);R::Statistics peakStats;const R::Index peakIndex(peaked,peakStats);
    R::Root proposal;proposal.radius=.4f;R::ScoreRoot(peakIndex,proposal,peakStats);
    Need(proposal.suggested,"adversarial interior peak not exercising approximate blind spot");
    U::Plant plant;plant.radius=.4f;plant.pose.scale={1,1,1};const Mistwatch::LandscapeSpeciesDetail::Surface actual(peaked);
    Need(!U::Detail::Support(actual,Blade(),plant).safe,"approximate ranking bypassed exact interior peak rejection");++negatives;
    Need(U::Target==384&&U::PlantsPerCluster==16&&U::ClustersPerBand==4&&U::MaximumProposals==24576,"ranking changed fixed population or proposal limits");
    std::cout<<"OUTER_TERRAIN_RANK_CONTROLS_PASS independent_samples="<<samples<<" root_replays="<<11*256
        <<" preview_queries="<<ranking.previewRoots<<" ranked_queries="<<ranking.rankedRoots<<" negative_checks="<<negatives<<std::endl;
}
void FootHintControls(const Assets& assets){
    namespace R=U::TerrainAware;
    const auto sources=assets.Bind();const auto volumes=U::Detail::ValidateSources(sources);
    R::Statistics statistics,replayStatistics;const R::FootHints hints(sources,volumes,statistics),again(sources,volumes,replayStatistics);
    Need(statistics.footLayers==176&&statistics.footSummaryPoints>0&&hints.Fingerprint()==again.Fingerprint()&&
        statistics.footSummaryPoints==replayStatistics.footSummaryPoints,"true-source16-layer summaries replay");
    unsigned cases{};std::uint64_t checkedPoints{};
    for(unsigned slot=0;slot<11;++slot){const auto& species=hints.At(slot);const auto& source=assets.plants[slot];
        Need(species.pivot==(slot>=9?double(volumes[slot].minimumY):0),"foot hint wrong broadleaf pivot");
        for(const float scale:{static_cast<float>(species.minimumScale),static_cast<float>((species.minimumScale+species.maximumScale)*.5),static_cast<float>(species.maximumScale)})
            for(const float yaw:{-1.1f,1.1f})for(const R::Sample plane:std::array<R::Sample,3>{{{0,0,0,1},{0,.1,-.08,.99},{0,-.15,.025,.98}}}){
                const double gradient=std::hypot(plane.dx,plane.dz),bend=.002;
                const double upper=hints.Gap(slot,scale,yaw,plane,gradient,bend,statistics);
                Need(std::isfinite(upper)&&upper<1e11,"legal scale lost foot summary");
                const double limit=double(.035f)/scale;
                const double cosine=std::cos(double(yaw)),sine=std::sin(double(yaw));
                const auto verify=[&](const std::array<double,4>& point){
                    const double worldX=double(scale)*(cosine*point[0]+sine*point[2]);
                    const double worldZ=double(scale)*(-sine*point[0]+cosine*point[2]);
                    const double gap=double(scale)*point[1]-plane.dx*worldX-plane.dz*worldZ+
                        point[3]*(double(U::WindRadius)*gradient+double(U::WindUp)+.000001)+bend;
                    // 只比較 double 排序模型，不把此容差帶入真 rational Support。
                    Need(gap<=upper+1e-9,"foot proxy understated independent planar vertex/edge gap");
                    bool covered=false;
                    for(const auto& layer:species.layers)if(layer.populated&&point[0]>=layer.minimumX-1e-10&&point[0]<=layer.maximumX+1e-10&&
                        point[2]>=layer.minimumZ-1e-10&&point[2]<=layer.maximumZ+1e-10&&point[1]>=layer.minimumY-1e-10&&point[1]<=layer.maximumY+1e-10&&
                        point[3]<=layer.maximumWind+1e-10)covered=true;
                    Need(covered,"source slab omitted independent clipped edge or boundary point");++checkedPoints;
                };
                // 獨立直接列三角形頂點與底部裁線的交點，不呼叫摘要裁切程式。
                for(std::size_t f=0;f<source.indices.size();f+=3){std::array<std::array<double,4>,3> triangle{};
                    for(unsigned k=0;k<3;++k){const auto& v=source.vertices[source.indices[f+k]];const auto n=UnpackNormalSnorm8(v.normal);
                        triangle[k]={v.x,double(v.y)-species.pivot,v.z,n.w>=0?double(1.f-std::clamp(n.w,0.f,1.f)):0};}
                    for(unsigned k=0;k<3;++k){const auto& a=triangle[k];const auto& b=triangle[(k+1)%3];
                        if(a[1]<=limit)verify(a);
                        if((a[1]<limit&&b[1]>limit)||(a[1]>limit&&b[1]<limit)){
                            const double ratio=(limit-a[1])/(b[1]-a[1]);std::array<double,4> crossing{};
                            for(unsigned j=0;j<4;++j)crossing[j]=(1-ratio)*a[j]+ratio*b[j];crossing[1]=limit;verify(crossing);}
                    }
                }++cases;
            }
    }
    auto missing=sources;missing.plants[0]=nullptr;Reject([&]{R::Statistics s;(void)R::FootHints(missing,volumes,s);},"OUTER_FOOT_HINT_SOURCE");
    auto broken=assets.plants[0];broken.vertices.back().x=std::numeric_limits<float>::quiet_NaN();missing=sources;missing.plants[0]=&broken;
    Reject([&]{R::Statistics s;(void)R::FootHints(missing,volumes,s);},"OUTER_FOOT_HINT_POINT");
    Reject([&]{(void)hints.At(11);},"OUTER_FOOT_HINT_SLOT");
    auto badVolumes=volumes;badVolumes[0].maximumY=badVolumes[0].minimumY;
    Reject([&]{R::Statistics s;(void)R::FootHints(sources,badVolumes,s);},"OUTER_FOOT_HINT_VOLUME");
    badVolumes=volumes;badVolumes[0].maximumY=std::numeric_limits<float>::quiet_NaN();
    Reject([&]{R::Statistics s;(void)R::FootHints(sources,badVolumes,s);},"OUTER_FOOT_HINT_VOLUME");
    badVolumes=volumes;badVolumes[0].minimumY=-std::numeric_limits<float>::max();badVolumes[0].maximumY=std::numeric_limits<float>::max();
    Reject([&]{R::Statistics s;(void)R::FootHints(sources,badVolumes,s);},"OUTER_FOOT_HINT_VOLUME");
    Reject([&]{(void)hints.Gap(0,1,std::numeric_limits<float>::quiet_NaN(),R::Sample{},0,0,statistics);},"OUTER_FOOT_HINT_QUERY");
    Need(hints.Gap(0,.1f,0,R::Sample{},0,0,statistics)==1e12,"out-of-range scale silently truncated foot");++negatives;

    // 少量真平地必須優先於候選更多、但底部餘裕較差的緩坡。
    auto ground=RankingGrid(-96,0,16,2);for(auto& v:ground.vertices)v.y=.15f*(std::max)(0.f,v.x+90.f);
    const auto blade=Blade();U::Sources tinySources;std::array<Mistwatch::CanopyVolume::PlantLocalVolume,11> tinyVolumes;
    for(unsigned slot=0;slot<11;++slot){tinySources.plants[slot]=&blade;tinyVolumes[slot]={.01f,0,.4f};}
    R::Statistics fixtureStats;const R::Index index(ground,fixtureStats);const R::FootHints tinyHints(tinySources,tinyVolumes,fixtureStats);
    const Transform3D tree{{-90,0,16},{},{1,1,1}};
    const auto coarse=R::RankCentres(index,tree,520,0,tinyVolumes,fixtureStats);
    const auto refined=R::RankCentres(index,tree,520,0,tinyVolumes,fixtureStats,&tinyHints);
    R::Centre sparseFlat,denseSlope;sparseFlat.inDomain=denseSlope.inDomain=true;
    sparseFlat.worstSpeciesCapacity=3;denseSlope.worstSpeciesCapacity=32;
    sparseFlat.worstFootGap=tinyHints.Gap(0,.5f,0,R::Sample{0,0,0,1},0,0,fixtureStats);
    denseSlope.worstFootGap=tinyHints.Gap(0,.5f,0,R::Sample{0,.15,0,.99},.15,0,fixtureStats);
    Need(sparseFlat.worstFootGap<denseSlope.worstFootGap&&R::CentreBefore(sparseFlat,denseSlope)&&
        !R::CentreBefore(denseSlope,sparseFlat),"foot margin still loses to generous coarse capacity");
    Need(refined.front().inDomain,"source-foot ranking selected missing domain");
    for(const auto& centre:refined)if(centre.inDomain)Need(refined.front().worstFootGap<=centre.worstFootGap,"source-foot worst-species minimum not first");
    const auto replay=R::RankCentres(index,tree,520,0,tinyVolumes,fixtureStats,&tinyHints);
    for(unsigned i=0;i<R::CentreCount;++i)Need(refined[i].ordinal==replay[i].ordinal&&refined[i].worstFootGap==replay[i].worstFootGap,"source-foot centre order not repeatable");
    R::RootPool hintedPool(index,refined.front(),tinyVolumes,fixtureStats,&tinyHints),hintedReplay(index,replay.front(),tinyVolumes,fixtureStats,&tinyHints);
    for(unsigned slot=0;slot<11;++slot){std::array<bool,R::RootCount> seen{};double previous=-1e12;bool missingSeen=false;
        for(unsigned i=0;i<R::RootCount;++i){const auto a=hintedPool.Next(slot),b=hintedReplay.Next(slot);
            Need(a&&b&&a->ordinal==b->ordinal&&!seen[a->ordinal]&&a->position.x==b->position.x&&a->position.z==b->position.z&&
                a->scale==b->scale&&a->yaw==b->yaw&&a->footGap==b->footGap&&a->sampled==b->sampled,"hinted256-root pool altered replay or uniqueness");seen[a->ordinal]=true;
            if(a->sampled){Need(!missingSeen&&a->footGap>=previous,"hinted root gaps not ordered");previous=a->footGap;}else missingSeen=true;
        }Need(!hintedPool.Next(slot),"hinted root pool exceeded256");
    }
    std::cout<<"OUTER_SOURCE_FOOT_HINT_CONTROLS_PASS layers="<<statistics.footLayers<<" points="<<statistics.footSummaryPoints
        <<" cases="<<cases<<" independent_points="<<checkedPoints<<" hinted_root_replays=2816 hash="<<hints.Fingerprint()
        <<" coarse_capacity="<<coarse.front().worstSpeciesCapacity<<" refined_capacity="<<refined.front().worstSpeciesCapacity
        <<" coarse_x="<<coarse.front().position.x<<" refined_x="<<refined.front().position.x
        <<" sparse_gap="<<sparseFlat.worstFootGap<<" dense_gap="<<denseSlope.worstFootGap
        <<" refined_gap="<<refined.front().worstFootGap<<" native_passed=false"<<std::endl;
}
void Progress(const U::Plan& plan){
    std::cout<<"OUTER_UNDERSTORY_PROGRESS proposals="<<plan.proposals<<" plants="<<plan.plants.size()<<" rolled_back="<<plan.rolledBack;
    for(unsigned i=0;i<11;++i)std::cout<<" slot"<<i<<"="<<plan.slotProposals[i]<<" support"<<i<<"="<<plan.slotSupportChecks[i];
    for(unsigned i=0;i<static_cast<unsigned>(U::Reject::Count);++i)std::cout<<" rejected"<<i<<"="<<plan.rejected[i];
    std::cout<<" rank_centres="<<plan.ranking.centreScans<<" rank_preview_roots="<<plan.ranking.previewRoots<<" rank_roots="<<plan.ranking.rankedRoots
        <<" rank_samples="<<plan.ranking.sampleQueries<<" rank_faces="<<plan.ranking.faceQueries
        <<" foot_layers="<<plan.ranking.footLayers<<" foot_points="<<plan.ranking.footSummaryPoints
        <<" foot_queries="<<plan.ranking.footMarginQueries<<" foot_hint_hash="<<plan.footHintFingerprint;
    std::cout<<std::endl;
}
void ObserveSupport(const U::Plant& p){
    ++supportObservations;
    if(p.support.restingFootObserved){if(p.support.restingFootAboveLimit)++restingFootFailures;else ++windBoundOnlyFootFailures;}
    if(supportObservations>32)return;
    const auto& proof=p.support;
    std::cout<<std::setprecision(17)<<"OUTER_ACTUAL_SUPPORT slot="<<p.slot<<" safe="<<proof.safe<<" reject="<<static_cast<unsigned>(proof.rejection)
        <<" root_x="<<p.groundRoot.x<<" root_y="<<p.groundRoot.y<<" root_z="<<p.groundRoot.z<<" radius="<<p.radius
        <<" normal_y="<<proof.minimumNormalY<<" height_delta="<<proof.maximumHeightDelta<<" min_gap="<<proof.minimumGapBound
        <<" max_foot_gap="<<proof.maximumFootGapBound<<" rest_observed="<<proof.restingFootObserved<<" rest_above_limit="<<proof.restingFootAboveLimit
        <<" max_rest_gap="<<proof.maximumRestingFootGap<<" overlay_faces="<<proof.exactOverlayFaces<<std::endl;
}
void SourceFeasibility(const Assets& assets){
    auto flat=Plane();for(auto& vertex:flat.vertices){vertex.x*=8;vertex.z*=8;}
    const Mistwatch::LandscapeSpeciesDetail::Surface surface(flat);
    const auto sources=assets.Bind();
    const auto volumes=U::Detail::ValidateSources(sources);
    unsigned zeroSafe{},bottomSafe{},overlayCases{};
    for(unsigned slot=0;slot<11;++slot){
        const auto& volume=volumes[slot];const float scale=slot<5?Mistwatch::ForestCoverScale({120,0,120}):slot<9?.6f:.3f/(volume.maximumY-volume.minimumY);
        for(unsigned pivot=0;pivot<2;++pivot){
            U::Plant plant;plant.slot=slot;plant.radius=volume.radius*scale+U::WindRadius;
            plant.pose={{0,pivot?-volume.minimumY*scale:0,0},{0,1.1f,0},{scale,scale,scale}};
            const auto proof=U::Detail::Support(surface,assets.plants[slot],plant);
            const auto original=U::Detail::Support(surface,assets.plants[slot],plant,false);
            Need(proof.safe==original.safe&&proof.rejection==original.rejection&&proof.minimumGapBound==original.minimumGapBound&&
                proof.maximumFootGapBound==original.maximumFootGapBound,"actual flat source cached overlay changed rejection");
            namespace D=Mistwatch::LandscapeSpeciesDetail;
            const auto centre=plant.groundRoot;const float radius=plant.radius;U::SupportProof cacheStats;
            const std::array<D::P,4> square{D::Exact::ToExact(Vec3{centre.x-radius,0,centre.z-radius}),D::Exact::ToExact(Vec3{centre.x+radius,0,centre.z-radius}),
                D::Exact::ToExact(Vec3{centre.x+radius,0,centre.z+radius}),D::Exact::ToExact(Vec3{centre.x-radius,0,centre.z+radius})};
            U::Detail::GroundCache cache(surface,square,cacheStats);const auto matrix=plant.pose.Matrix();
            const auto& source=assets.plants[slot];const auto size=source.indices.size();
            for(std::size_t offset:{std::size_t{0},(size/6)*3,size-3}){
                D::T triangle;for(unsigned k=0;k<3;++k){const auto& v=source.vertices[source.indices[offset+k]];
                    triangle[k]=D::Exact::ToExact(TransformPoint(matrix,{v.x,v.y,v.z}));}
                for(const float shift:{-.041f,0.f,.043f}){auto shifted=triangle;for(auto& point:shifted)point[1]=point[1]+D::R::Float(shift);
                    const auto before=D::Overlay(shifted,surface),after=cache.Overlay(shifted);
                    Need(before.points==after.points&&(!before.points||(Compare(before.minimum,after.minimum)==0&&Compare(before.maximum,after.maximum)==0)),"actual source exact plane range differs from oracle");++differentialOverlays;}
            }
            if(proof.exactOverlayFaces)++overlayCases;
            if(proof.safe){if(pivot)++bottomSafe;else ++zeroSafe;}
            std::cout<<std::setprecision(17)<<"OUTER_SOURCE_FEASIBILITY slot="<<slot<<" pivot="<<(pivot?"bottom-aligned":"zero")
                <<" safe="<<proof.safe<<" reject="<<static_cast<unsigned>(proof.rejection)<<" scale="<<scale
                <<" local_min_y="<<volume.minimumY<<" local_max_y="<<volume.maximumY
                <<" min_gap_bound="<<proof.minimumGapBound<<" max_foot_gap_bound="<<proof.maximumFootGapBound
                <<" cache_faces="<<proof.cachedGroundFaces<<" overlay_faces="<<proof.exactOverlayFaces
                <<" ground_planes="<<proof.groundHeightPlanes<<" source_planes="<<proof.sourceHeightPlanes<<" height_evaluations="<<proof.cachedHeightEvaluations
                <<" foot_pieces="<<proof.footPieces<<" native_passed=false"<<std::endl;
        }
    }
    Need(overlayCases>0,"real source must execute cached exact overlay");
    std::cout<<"OUTER_SOURCE_FEASIBILITY_COMPLETE cases=22 zero_pivot_safe="<<zeroSafe<<" bottom_aligned_safe="<<bottomSafe
        <<" actual_overlay_cases="<<overlayCases<<" candidate_complete=false native_passed=false"<<std::endl;
}
}
int main(int argc,char** argv)try{
    Need((argc==3||argc==4)&&std::string_view(argv[1])=="--runtime-assets"&&(argc==3||std::string_view(argv[3])=="--source-feasibility"||std::string_view(argv[3])=="--candidate-prefix"),"usage: --runtime-assets ASSETS_DIRECTORY [--source-feasibility|--candidate-prefix]");SyntheticControls();CacheControls();RankingControls();
    const auto assets=Load(std::filesystem::path(argv[2]));const auto sources=assets.Bind();
    FootHintControls(assets);
    if(argc==4&&std::string_view(argv[3])=="--source-feasibility"){SourceFeasibility(assets);return 0;}
    const auto terrain=Mistwatch::CreateTerrain(),wrongTerrain=HeightField3D{};
    const auto trees=S::Create(terrain,Mistwatch::LandscapeTrees(terrain),Bounds(assets));Need(trees.complete&&trees.combined.size()==616,"canonical616 required");
    auto mesh=Mistwatch::CreateLandscapeMesh(terrain,true,Mistwatch::ForestFloorMaskSelector::ForestWood,Mistwatch::BankCoverageSelector::Patchy);
    (void)Mistwatch::ApplyForestHabitatFloor(mesh,terrain,{},true);const auto oldMesh=Hash(mesh);
    const auto before=Mistwatch::WorldSolids(false),after=Mistwatch::WorldSolids(true);
    if(argc==4){
        const auto prefix=U::Create(terrain,mesh,trees,sources,before,after,128,Progress,ObserveSupport);
        Need(prefix.proposals==128&&!prefix.complete&&Hash(mesh)==oldMesh,"bounded prefix preserves candidate gate and landscape");
        Need(prefix.proposalRevision==U::TerrainAware::Revision&&prefix.ranking.centreScans<=96*48&&prefix.ranking.rankedRoots<=96*256*11,"prefix bounded ranking ledger");
        Progress(prefix);
        std::cout<<"OUTER_PREFIX_DIAGNOSTIC_COMPLETE proposals="<<prefix.proposals<<" plants="<<prefix.plants.size()<<" observations="<<supportObservations
            <<" resting_foot_failures="<<restingFootFailures<<" wind_bound_only_foot_failures="<<windBoundOnlyFootFailures
            <<" proposal_revision="<<prefix.proposalRevision<<" candidate_complete=false native_passed=false"<<std::endl;return 0;
    }
    const auto create=[&](unsigned limit){return U::Create(terrain,mesh,trees,sources,before,after,limit);};
    const auto zero=create(0);Need(zero.plants.empty()&&zero.clusters.empty()&&!zero.complete&&zero.proposals==0,"zero limit is explicit shortfall");
    // 有界的真實草／蕨／寬葉差分；不把這四株當成完整群落或美術驗收。
    const auto localVolumes=U::Detail::ValidateSources(sources);const Mistwatch::LandscapeSpeciesDetail::Surface actualSurface(mesh);
    for(const unsigned slot:{0u,3u,5u,9u}){
        U::Plant sample;sample.slot=slot;sample.groundRoot=trees.combined[520+slot].position;sample.groundRoot.x+=3.f;
        sample.groundRoot.y=static_cast<float>(actualSurface.At(sample.groundRoot).Approximate());
        const auto& volume=localVolumes[slot];const float scale=slot<5?Mistwatch::ForestCoverScale(sample.groundRoot):slot<9?.6f:.3f/(volume.maximumY-volume.minimumY);
        sample.pose={sample.groundRoot,{0,1.1f,0},{scale,scale,scale}};sample.radius=volume.radius*scale+U::WindRadius;
        if(slot>=9)sample.pose.position.y-=volume.minimumY*scale;
        SupportEquivalence(mesh,*sources.plants[slot],sample);
    }
    std::cout<<"OUTER_UNDERSTORY_ACTUAL_SUPPORT_DIFFERENTIAL_PASS supports="<<differentialSupports<<std::endl;
    Reject([&]{(void)create(24577);},"OUTER_PLANTS_PROPOSAL_LIMIT");
    Reject([&]{(void)U::Create(wrongTerrain,mesh,trees,sources,before,after,0);},"OUTER_PLANTS_TERRAIN_LAYOUT");
    auto badSources=sources;badSources.plants[0]=nullptr;Reject([&]{(void)U::Create(terrain,mesh,trees,badSources,before,after,0);},"OUTER_PLANTS_SOURCE_TOPOLOGY");
    badSources=sources;badSources.fingerprints[10]^=1;Reject([&]{(void)U::Create(terrain,mesh,trees,badSources,before,after,0);},"OUTER_PLANTS_SOURCE_IDENTITY");
    auto broken=assets.plants[1];broken.vertices.back().u=std::numeric_limits<float>::quiet_NaN();badSources=sources;badSources.plants[1]=&broken;
    Reject([&]{(void)U::Create(terrain,mesh,trees,badSources,before,after,0);},"OUTER_PLANTS_SOURCE_VERTEX");
    broken=assets.plants[1];broken.vertices.back().normal=0;badSources.plants[1]=&broken;
    Reject([&]{(void)U::Create(terrain,mesh,trees,badSources,before,after,0);},"OUTER_PLANTS_SOURCE_VERTEX");
    badSources=sources;badSources.treeFingerprints[4]^=1;Reject([&]{(void)U::Create(terrain,mesh,trees,badSources,before,after,0);},"OUTER_PLANTS_TREE_IDENTITY");
    auto badTrees=trees;badTrees.combined[0].position.x+=1;Reject([&]{(void)U::Create(terrain,mesh,badTrees,sources,before,after,0);},"OUTER_SOURCE_POSE");
    badTrees=trees;badTrees.combined[520]=badTrees.combined[521];Reject([&]{(void)U::Create(terrain,mesh,badTrees,sources,before,after,0);},"OUTER_SUFFIX_POSE");
    badTrees=trees;badTrees.addedBands[0]=5;Reject([&]{(void)U::Create(terrain,mesh,badTrees,sources,before,after,0);},"OUTER_BANDS");
    auto badMesh=mesh;badMesh.indices.pop_back();const auto malformedBefore=Hash(badMesh);
    Reject([&]{(void)U::Create(terrain,badMesh,trees,sources,before,after,0);},"OUTER_PLANTS_SURFACE_TOPOLOGY");Need(Hash(badMesh)==malformedBefore,"invalid surface partially changed");
    badMesh=mesh;badMesh.vertices.back().y+=.1f;Reject([&]{(void)U::Create(terrain,badMesh,trees,sources,before,after,0);},"OUTER_PLANTS_SURFACE_VERTEX");
    const std::array<BoxCollider3D,1> invalidWorld{{{{0,0,0},{0,1,1}}}};
    Reject([&]{(void)U::Create(terrain,mesh,trees,sources,invalidWorld,after,0);},"OUTER_PLANTS_WORLD_INPUT");
    std::cout<<"OUTER_UNDERSTORY_CPU_START max_proposals=24576 target=384 native_passed=false"<<std::endl;
    const auto result=U::Create(terrain,mesh,trees,sources,before,after,U::MaximumProposals,Progress);Need(result.proposals<=24576&&result.plants.size()<=384&&result.clusters.size()<=24,"finite population bounds");
    Need(result.fingerprint==Hash(result)&&result.surfaceFingerprint==oldMesh&&result.poseFingerprint==trees.fingerprint,"independent output identity");
    unsigned rejects{};for(auto value:result.rejected)rejects+=value;
    Need(result.proposals==rejects+result.acceptedCandidates&&result.acceptedCandidates==result.plants.size()+result.rolledBack,"complete proposal accounting");
    const ReferenceSurface reference(mesh);std::array<unsigned,11> variants{};std::array<unsigned,6> bands{};std::uint64_t triangles{};double maximumRootError{},minimumSampleGap=1000;
    for(std::size_t i=0;i<result.plants.size();++i){const auto& p=result.plants[i];Need(p.slot<11&&p.band<6&&p.cluster<result.clusters.size()&&p.anchorTree>=520&&p.anchorTree<616,"plant identity range");
        Need(trees.addedBands[p.anchorTree-520]==p.band&&p.support.safe,"actual anchor band and support");++variants[p.slot];++bands[p.band];triangles+=U::TriangleCounts[p.slot];
        const double err=std::abs(double(p.groundRoot.y)-reference.Height(p.groundRoot.x,p.groundRoot.z));maximumRootError=(std::max)(maximumRootError,err);Need(err<.00005,"independent triangle root height");
        const auto transform=p.pose.Matrix();const auto& source=*sources.plants[p.slot];
        for(const auto& v:source.vertices){const auto world=TransformPoint(transform,{v.x,v.y,v.z});const double gap=world.y-reference.Height(world.x,world.z);
            minimumSampleGap=(std::min)(minimumSampleGap,gap);Need(gap>=-.04001,"independent actual vertex penetration");
            if(world.y<=p.groundRoot.y+.035f)Need(gap<=.04001,"independent actual base floating");
            Need(world.x>=-200&&world.x<=200&&world.z>=-170&&world.z<=250&&
                (world.x< -53||world.x>53||world.z< -23||world.z>103),"complete transformed source domain");}
        for(std::size_t f=0;f<source.indices.size();f+=111){const auto a=TransformPoint(transform,{source.vertices[source.indices[f]].x,source.vertices[source.indices[f]].y,source.vertices[source.indices[f]].z});
            const auto b=TransformPoint(transform,{source.vertices[source.indices[f+1]].x,source.vertices[source.indices[f+1]].y,source.vertices[source.indices[f+1]].z});
            const auto c=TransformPoint(transform,{source.vertices[source.indices[f+2]].x,source.vertices[source.indices[f+2]].y,source.vertices[source.indices[f+2]].z});
            for(const auto point:{(a+b)*.5f,(a+c)*.5f,(b+c)*.5f,(a+b+c)*(1.f/3.f)})Need(point.y-reference.Height(point.x,point.z)>=-.04002,"independent edge/centroid support");}
        for(std::size_t j=0;j<i;++j)Need(std::hypot(double(p.groundRoot.x)-result.plants[j].groundRoot.x,double(p.groundRoot.z)-result.plants[j].groundRoot.z)>=.65*(double(p.radius)+result.plants[j].radius),"independent mixed root separation");
    }
    Need(variants==result.variants&&bands==result.bandPlants&&triangles==result.instanceTriangles,"independent species and triangle ledger");
    for(unsigned i=0;i<result.clusters.size();++i){const auto& c=result.clusters[i];Need(c.count==16&&c.first==i*16&&c.band<6&&c.proposalEnd<=result.proposals,"complete cluster boundaries");
        for(unsigned j=0;j<16;++j)Need(result.plants[c.first+j].slot==U::ClusterSlots[j]&&result.plants[c.first+j].cluster==i,"fixed full cluster composition");}
    if(!result.clusters.empty()){const auto prefix=create(result.clusters.front().proposalEnd);Need(prefix.plants.size()==16&&prefix.clusters.size()==1&&!prefix.complete,"bounded committed prefix");
        U::Plan expected;expected.footHintFingerprint=result.footHintFingerprint;expected.plants.assign(result.plants.begin(),result.plants.begin()+16);Need(Hash(prefix)==Hash(expected),"prefix replay identity");}
    const auto one=create(1),oneAgain=create(1);Need(!one.complete&&one.fingerprint==oneAgain.fingerprint&&one.proposals==oneAgain.proposals&&one.rejected==oneAgain.rejected,"bounded deterministic shortfall");
    Need(Hash(mesh)==oldMesh,"original world surface modified");for(unsigned i=0;i<11;++i)Need(Hash(assets.plants[i])==sources.fingerprints[i],"source plant modified");
    for(unsigned i=0;i<5;++i)Need(Hash(assets.trees[i])==sources.treeFingerprints[i],"source tree modified");
    if(result.complete){Need(result.clusters.size()==24&&result.plants.size()==384&&triangles==638304,"complete fixed product budget");
        for(unsigned i=0;i<11;++i)Need(variants[i]==(i<5?48u:24u),"complete exact variant quota");
        for(unsigned i=0;i<6;++i)Need(bands[i]==64&&result.bandClusters[i]==4,"complete six-band quota");}
    std::cout<<std::setprecision(17)<<(result.complete?"OUTER_UNDERSTORY_CPU_PASS":"OUTER_UNDERSTORY_CPU_SHORTFALL")
        <<" complete="<<result.complete<<" plants="<<result.plants.size()<<" clusters="<<result.clusters.size()<<" proposals="<<result.proposals
        <<" accepted_candidates="<<result.acceptedCandidates<<" rolled_back="<<result.rolledBack<<" failed_clusters="<<result.failedClusters<<" rejected_anchors="<<result.rejectedAnchors
        <<" triangles="<<triangles<<" fingerprint="<<result.fingerprint<<" pose_fingerprint="<<result.poseFingerprint<<" surface_fingerprint="<<result.surfaceFingerprint
        <<" max_root_error="<<maximumRootError<<" min_sample_gap="<<minimumSampleGap;
    for(unsigned i=0;i<6;++i)std::cout<<" band"<<i<<"="<<bands[i];for(unsigned i=0;i<11;++i)std::cout<<" variant"<<i<<"="<<variants[i]<<" source"<<i<<"="<<sources.fingerprints[i];
    for(unsigned i=0;i<static_cast<unsigned>(U::Reject::Count);++i)std::cout<<" rejected"<<i<<"="<<result.rejected[i];
    for(unsigned i=0;i<11;++i)std::cout<<" slot_proposals"<<i<<"="<<result.slotProposals[i]<<" slot_supports"<<i<<"="<<result.slotSupportChecks[i];
    std::cout<<" differential_overlays="<<differentialOverlays<<" differential_supports="<<differentialSupports;
    std::cout<<" negative_checks="<<negatives<<" checks="<<checks<<" native_passed=false\n";return result.complete?0:2;
}catch(const std::exception& e){std::cerr<<"OUTER_UNDERSTORY_CPU_FAIL "<<e.what()<<" checks="<<checks<<'\n';return 1;}
