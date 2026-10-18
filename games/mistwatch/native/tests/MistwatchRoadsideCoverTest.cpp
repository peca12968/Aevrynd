#include "MistwatchRoadsideCover.h"
#include "fallen/MistwatchAuthoredNativeExpected.h"
#include <iostream>
using namespace Aevrynd;
using namespace Mistwatch;
namespace Q=::Mistwatch::AuthoredQualification;
namespace W=::Mistwatch::FallenWorldSeat;
int wmain(int argc,wchar_t** argv)try{
    const auto need=[](bool ok,const char* why){if(!ok)throw std::runtime_error(why);};
    need(argc==3&&std::wstring_view(argv[1])==L"--runtime-assets","Absolute runtime usage");
    const auto root=std::filesystem::path(argv[2]);need(root.is_absolute(),"Absolute assets required");
    const auto terrain=CreateTerrain();const auto assets=Q::LoadAssets(root);W::Budget budget;
    const auto source=Q::CreateSourceContext(terrain,assets,budget);std::vector<W::Placement> placements;
    for(const auto& p:FallenLogPlacements()){W::Placement q;q.propId=p.propId;q.anchor=p.authoredAnchor;q.pose=p.pose;q.collider=p.collider;placements.push_back(q);}
    const auto c=Q::CreateContext(terrain,assets,source,placements,budget);
    Q::ValidateContext(c,terrain,assets,budget);const auto seal=Q::ContextFingerprint(c);
    const auto before=W::Boxes(c.worlds[0]),after=W::Boxes(c.worlds[1]);
    const auto out=CreateRoadsideCover(terrain,c.retention.cover,c.author.understory,c.colonies,c.shrubs,
        assets.grassAssets,assets.fernAssets,before,after);
    need(out.attempted==1344&&out.left+out.right==out.plants.size(),"Finite two-sided author");
    need(out.attempted==out.plants.size()+out.rejectedRoute+out.rejectedWorld+out.rejectedTerrain+out.rejectedVegetation,"Complete attempt accounting");
    unsigned newShoulder{};std::array<unsigned,5> variants{};
    for(unsigned i=0;i<out.plants.size();++i){const auto& p=out.plants[i];const auto r=p.transform.position;
        need(p.variant<5&&std::abs(r.x)<=6.34f&&r.z>=-6.09f&&r.z<=27.29f,"Fixed authored domain");++variants[p.variant];
        // 獨立主路及橫向通路投影驗證，不讓新增角色政策假冒舊大地物門。
        need(std::abs(r.x)-p.radius>=1.8f,"Full grass footprint in main trail");
        // 沿完整 z 包絡密取樣真彎路界，不只重用作者的導數公式。
        for(unsigned sample=0;sample<=32;++sample){const float z=r.z-p.radius+2*p.radius*float(sample)/32.f;
            need(std::abs(r.x-.35f*std::sin(z*.19f))-p.radius>=2.1f+.15f*std::sin(z*.31f)+.08f,"Wind footprint crosses visible winding trail");}
        for(const auto point:Session::supplies){
            need(std::hypot((std::max)(0.f,std::abs(r.x-point.x)-p.radius),
                (std::max)(0.f,std::abs(r.z-point.z)-p.radius))>=3.f,"Supply approach hidden");
            need(!(r.z-p.radius<point.z+2&&r.z+p.radius>point.z-2&&
                r.x-p.radius<(std::max)(0.f,point.x)+2&&r.x+p.radius>(std::min)(0.f,point.x)-2),"Cross trail hidden");
        }
        const auto& a=assets.grassAssets[p.variant];
        need(p.radius==a.radius*p.transform.scale.x+.19f,"World wind must not shrink with instance scale");
        const CanopyVolume::PlantVolume volume{r,CanopyVolume::ConservativeLocalVolume(a.radius+.19f/p.transform.scale.x,
            a.minimumY,a.maximumY+.011f/p.transform.scale.x),p.transform.scale.x,p.transform.rotation};
        need(CanopyVolume::ClearsCanopyWood(volume,before)&&CanopyVolume::ClearsCanopyWood(volume,after),"Both full world states intersect");
        need(ForestCoverFootprint(terrain,r,p.radius,{}),"Actual complete terrain footprint");
        const auto separation=[&](Aevrynd::Vec3 other,float otherRadius){
            need(std::hypot(r.x-other.x,r.z-other.z)>=.65f*(p.radius+otherRadius),"Supplemental root conflicts with existing vegetation");};
        for(const auto& old:c.retention.cover.plants)separation(old.transform.position,old.radius+.19f);
        for(unsigned j=0;j<c.author.understory.ferns.size();++j){const auto& old=c.author.understory.ferns[j];
            separation(old.position,assets.fernAssets[j%4].radius*old.scale.x+.19f);}
        for(const auto& old:c.colonies.plants)separation(old.transform.position,old.radius+.19f);
        for(const auto& old:c.shrubs.plants)separation(old.pose.position,old.radius+.19f);
        for(unsigned j=0;j<i;++j)need(std::hypot(r.x-out.plants[j].transform.position.x,r.z-out.plants[j].transform.position.z)>=
            .65f*(p.radius+out.plants[j].radius),"Supplemental roots stacked");
        if(!PropClearsObjectives({r,{p.radius,1,p.radius}}))++newShoulder;
    }
    need(newShoulder>=64,"New author missed the actual nine-metre gap");
    // 原大地物仍禁止 (3,0,0)，新低草允許；中央、補給與水岸必須拒絕。
    need(!PropClearsObjectives({{3,0,0},{.35f,1,.35f}})&&RoadsideCoverRouteClearance({3,0,0},.35f),"Role-specific policy not explicit");
    need(!RoadsideCoverRouteClearance({1.9f,0,0},.35f)&&!RoadsideCoverRouteClearance(Session::supplies[0],.35f)&&
        !RoadsideCoverRouteClearance({3,0,35},.35f),"Negative route controls accepted");
    bool rejected=false;try{(void)CreateRoadsideCover(terrain,c.retention.cover,c.author.understory,c.colonies,c.shrubs,
        assets.grassAssets,assets.fernAssets,std::span<const BoxCollider3D>(before).first(before.size()-1),after);
    }catch(const std::invalid_argument& e){rejected=std::string_view(e.what())=="ROADSIDE_COMPLETE_TWO_STATE_WORLD_REQUIRED";}
    need(rejected&&Q::ContextFingerprint(c)==seal,"Missing world accepted or original mutated");
    const float nan=std::numeric_limits<float>::quiet_NaN();unsigned invalidControls{};
    const auto rejectSource=[&](const ForestCoverResult& cover,const ForestUnderstory& under,
        const FernColonyResult& colony,const ShrubCanopyResult& shrub,const char* reason){
        bool failed=false;try{(void)CreateRoadsideCover(terrain,cover,under,colony,shrub,
            assets.grassAssets,assets.fernAssets,before,after);}
        catch(const std::invalid_argument& e){need(std::string_view(e.what())==reason,"Wrong invalid source rejection");failed=true;}
        need(failed&&Q::ContextFingerprint(c)==seal,"Malformed vegetation accepted or canonical mutated");++invalidControls;
    };
    auto badCover=c.retention.cover;badCover.plants[0].transform.position.x=nan;
    rejectSource(badCover,c.author.understory,c.colonies,c.shrubs,"ROADSIDE_EXISTING_GRASS_INVALID");
    badCover=c.retention.cover;badCover.plants[0].radius=nan;
    rejectSource(badCover,c.author.understory,c.colonies,c.shrubs,"ROADSIDE_EXISTING_GRASS_INVALID");
    auto badUnder=c.author.understory;badUnder.ferns[0].position.z=nan;
    rejectSource(c.retention.cover,badUnder,c.colonies,c.shrubs,"ROADSIDE_EXISTING_FERN_INVALID");
    badUnder=c.author.understory;badUnder.ferns[0].scale.x=-1;
    rejectSource(c.retention.cover,badUnder,c.colonies,c.shrubs,"ROADSIDE_EXISTING_FERN_INVALID");
    badUnder=c.author.understory;badUnder.ferns[0].scale.y*=2;
    rejectSource(c.retention.cover,badUnder,c.colonies,c.shrubs,"ROADSIDE_EXISTING_FERN_INVALID");
    auto badColony=c.colonies;badColony.plants[0].radius=nan;
    rejectSource(c.retention.cover,c.author.understory,badColony,c.shrubs,"ROADSIDE_EXISTING_COLONY_INVALID");
    auto badShrub=c.shrubs;badShrub.plants[0].pose.position.x=nan;
    rejectSource(c.retention.cover,c.author.understory,c.colonies,badShrub,"ROADSIDE_EXISTING_SHRUB_INVALID");
    badShrub=c.shrubs;badShrub.plants[0].radius=0;
    rejectSource(c.retention.cover,c.author.understory,c.colonies,badShrub,"ROADSIDE_EXISTING_SHRUB_INVALID");
    need(invalidControls==8,"Eight malformed source cases required");
    std::cout<<"ROADSIDE_EXPECTED roadside_profile="<<RoadsideCoverProfile<<'\n'
        <<"ROADSIDE_EXPECTED roadside_population="<<out.plants.size()<<'\n'
        <<"ROADSIDE_EXPECTED roadside_fingerprint="<<out.fingerprint<<'\n'
        <<"ROADSIDE_EXPECTED roadside_left="<<out.left<<'\n'
        <<"ROADSIDE_EXPECTED roadside_right="<<out.right<<'\n'
        <<"ROADSIDE_EXPECTED roadside_attempted="<<out.attempted<<'\n';
    std::cout<<"ROADSIDE_CPU_PASS original_unchanged=1 both_worlds=1 negative_controls=4 invalid_source_controls=8 existing_groups=4 new_shoulder="<<newShoulder<<" native=0 art=0\n";return 0;
}catch(const std::exception& e){std::cerr<<"ROADSIDE_FIRST_DIVERGENCE "<<e.what()<<'\n';return 1;}
