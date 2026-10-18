#include "MistwatchRoadsideBroadleaf.h"
#include "MistwatchForestGrassShape.h"
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
    const auto c=Q::CreateContext(terrain,assets,source,placements,budget);Q::ValidateContext(c,terrain,assets,budget);
    const auto seal=Q::ContextFingerprint(c);const auto before=W::Boxes(c.worlds[0]),after=W::Boxes(c.worlds[1]);
    std::array<NettleAssetBounds,2> bounds;std::array<std::uint64_t,2> identities{};
    for(unsigned i=0;i<2;++i){
        const auto v=RoadsideBroadleafVariants[i];auto mesh=LoadAevryndMeshFile(root/"mistwatch-nettle/meshes"/NettleMeshName(v));
        need(bool(mesh),"Actual broadleaf source missing");NormalizeNettleAsset(mesh.mesh,v);
        bounds[i]=MeasureNettleAssetBounds(mesh.mesh);identities[i]=ForestGrassShapeFingerprint(mesh.mesh);
    }
    const auto create=[&](const std::array<NettleAssetBounds,2>& b,std::span<const BoxCollider3D> world){
        return CreateRoadsideBroadleaf(terrain,c.retention.cover,c.author.understory,c.colonies,c.shrubs,
            assets.grassAssets,assets.fernAssets,b,world,after);};
    const auto out=create(bounds,before);
    need(out.plants.size()==96&&out.left==48&&out.right==48&&out.attempted<=2048,"Finite balanced broadleaf population");
    need(out.attempted==out.plants.size()+out.rejectedRoute+out.rejectedWorld+out.rejectedTerrain+out.rejectedVegetation,"Attempt accounting");
    std::array<unsigned,2> variants{};std::uint64_t triangles{};unsigned newShoulder{};
    for(unsigned i=0;i<out.plants.size();++i){const auto& p=out.plants[i];const auto r=p.transform.position;
        need(p.variant<2&&std::abs(r.x)<=6.01f&&r.z>=-6.41f&&r.z<=23.31f,"Fixed world shoulder domain");
        const auto& a=bounds[p.variant];++variants[p.variant];triangles+=NettleTriangleCounts[RoadsideBroadleafVariants[p.variant]];
        const float s=p.transform.scale.x,ground=terrain.SampleHeight(r.x,r.z),height=(a.maximumY-a.minimumY)*s;
        need(std::abs(r.y+a.minimumY*s-ground)<.00001f,"Actual lowest source vertex must ground");
        need(height>=(p.variant==0?.25999f:.09999f)&&height<=(p.variant==0?.35001f:.16001f),"Distinct low species height");
        need(p.radius==a.radius*s+.19f,"Wind radius must be world space");
        for(unsigned sample=0;sample<=32;++sample){const float z=r.z-p.radius+2*p.radius*float(sample)/32.f;
            need(std::abs(r.x-.35f*std::sin(z*.19f))-p.radius>=2.1f+.15f*std::sin(z*.31f)+.08f,"Complete winding trail footprint");}
        for(const auto point:Session::supplies){
            need(std::hypot((std::max)(0.f,std::abs(r.x-point.x)-p.radius),
                (std::max)(0.f,std::abs(r.z-point.z)-p.radius))>=3.f,"Supply approach covered");
            need(!(r.z-p.radius<point.z+2&&r.z+p.radius>point.z-2&&
                r.x-p.radius<(std::max)(0.f,point.x)+2&&r.x+p.radius>(std::min)(0.f,point.x)-2),"Supply cross trail covered");
        }
        const CanopyVolume::PlantVolume volume{r,CanopyVolume::ConservativeLocalVolume(a.radius+.19f/s,
            a.minimumY,a.maximumY+.011f/s),s,p.transform.rotation};
        need(CanopyVolume::ClearsCanopyWood(volume,before)&&CanopyVolume::ClearsCanopyWood(volume,after),"Both full world volumes required");
        need(ForestCoverFootprint(terrain,{r.x,ground,r.z},p.radius,{}),"Complete terrain footprint");
        const auto separation=[&](Vec3 other,float otherRadius){
            need(std::hypot(r.x-other.x,r.z-other.z)>=.65f*(p.radius+otherRadius),"Existing vegetation conflict");};
        for(const auto& old:c.retention.cover.plants)separation(old.transform.position,old.radius+.19f);
        for(unsigned j=0;j<c.author.understory.ferns.size();++j){const auto& old=c.author.understory.ferns[j];
            separation(old.position,assets.fernAssets[j%4].radius*old.scale.x+.19f);}
        for(const auto& old:c.colonies.plants)separation(old.transform.position,old.radius+.19f);
        for(const auto& old:c.shrubs.plants)separation(old.pose.position,old.radius+.19f);
        for(unsigned j=0;j<i;++j)separation(out.plants[j].transform.position,out.plants[j].radius);
        if(!PropClearsObjectives({r,{p.radius,1,p.radius}}))++newShoulder;
    }
    need(variants[0]>=16&&variants[1]>=16&&newShoulder>=48,"Real two-species shoulder required");
    unsigned negativeControls{};
    const auto reject=[&](const std::array<NettleAssetBounds,2>& b,std::span<const BoxCollider3D> world,const char* reason){
        bool rejected=false;try{(void)create(b,world);}catch(const std::invalid_argument& e){need(std::string_view(e.what())==reason,"Wrong exact rejection");rejected=true;}
        need(rejected&&Q::ContextFingerprint(c)==seal,"Malformed input accepted or source mutated");++negativeControls;
    };
    reject(bounds,std::span<const BoxCollider3D>(before).first(before.size()-1),"ROADSIDE_COMPLETE_TWO_STATE_WORLD_REQUIRED");
    auto bad=bounds;bad[0].radius=std::numeric_limits<float>::quiet_NaN();reject(bad,before,"ROADSIDE_BROADLEAF_ACTUAL_VOLUME_REQUIRED");
    bad=bounds;bad[1].maximumY=bad[1].minimumY;reject(bad,before,"ROADSIDE_BROADLEAF_ACTUAL_VOLUME_REQUIRED");
    bad=bounds;bad[0].minimumY=std::numeric_limits<float>::infinity();reject(bad,before,"ROADSIDE_BROADLEAF_ACTUAL_VOLUME_REQUIRED");
    need(Q::ContextFingerprint(c)==seal,"Original population/world changed");
    std::cout<<"BROADLEAF_EXPECTED roadside_broadleaf_profile="<<RoadsideBroadleafProfile<<'\n'
        <<"BROADLEAF_EXPECTED roadside_broadleaf_population="<<out.plants.size()<<'\n'
        <<"BROADLEAF_EXPECTED roadside_broadleaf_fingerprint="<<out.fingerprint<<'\n'
        <<"BROADLEAF_EXPECTED roadside_broadleaf_left="<<out.left<<'\n'
        <<"BROADLEAF_EXPECTED roadside_broadleaf_right="<<out.right<<'\n'
        <<"BROADLEAF_EXPECTED roadside_broadleaf_attempted="<<out.attempted<<'\n'
        <<"BROADLEAF_EXPECTED roadside_broadleaf_materials=1\n"
        <<"BROADLEAF_EXPECTED roadside_broadleaf_submitted_triangles="<<triangles<<'\n';
    for(unsigned i=0;i<2;++i)std::cout<<"BROADLEAF_EXPECTED roadside_broadleaf_mesh_"<<i<<"_identity="<<identities[i]<<'\n'
        <<"BROADLEAF_EXPECTED roadside_broadleaf_mesh_"<<i<<"_triangles="<<NettleTriangleCounts[RoadsideBroadleafVariants[i]]<<'\n';
    std::cout<<"BROADLEAF_CPU_PASS original_unchanged=1 both_worlds=1 negative_controls="<<negativeControls
        <<" existing_groups=4 new_shoulder="<<newShoulder<<" native=0 art=0\n";return 0;
}catch(const std::exception& e){std::cerr<<"BROADLEAF_FIRST_DIVERGENCE "<<e.what()<<'\n';return 1;}
