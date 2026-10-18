#include "MistwatchBridgeRepairPreparation.h"
#include "MistwatchExplorerNapeHairBinding.h"
#include "MistwatchExplorerBackpack.h"
#include <Aevrynd/SkinnedModelAsset.h>
#include <bit>
#include <iomanip>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <type_traits>

namespace BridgeRepairTest {
using namespace Mistwatch;
using namespace Aevrynd;
constexpr auto Fir=FirLayoutSelector::ForestWood;
constexpr auto Fallen=FallenLogProfile::LegacyCylinder;
unsigned cases{},canonicalRejected{},accepted{},cameraRejected{},hullRejected{},ulpCases{};
void Need(bool value,const char* why){if(!value)throw std::runtime_error(why);}
struct Words {
    std::vector<std::uint64_t> data;
    void Add(std::uint64_t v){data.push_back(v);}
    void F(float v){Add(std::bit_cast<std::uint32_t>(v));}
    void V(Vec3 p){F(p.x);F(p.y);F(p.z);}
    void M(const Matrix4& m){for(const auto& row:m.values)for(float v:row)F(v);}
    void Pose(const AnimationPose3D& p){
        Add(p.localTransforms.size());Add(p.globalMatrices.size());
        for(const auto& t:p.localTransforms){V(t.translation);V(t.rotation);V(t.scale);
            F(t.quaternion.x);F(t.quaternion.y);F(t.quaternion.z);F(t.quaternion.w);Add(t.usesQuaternion);}
        for(const auto& m:p.globalMatrices)M(m);
    }
    template<class T>void SameObjectBytes(const T& value){
        static_assert(std::is_trivially_copyable_v<T>);
        // 僅比較同一物件呼叫前後的原始位元；不作跨物件或持久化身分。
        for(auto b:std::as_bytes(std::span(&value,1)))Add(std::to_integer<unsigned char>(b));
    }
    void ExplorerState(const Explorer& e){
        Pose(e.pose);F(e.phase);F(e.yaw);F(e.requestedYaw);
        for(auto p:e.footTargets)V(p);for(bool b:e.planted)Add(b);
        V(e.lastFeet);Add(e.feetInitialized);F(e.locomotionDistance);F(e.locomotionSeconds);V(e.locomotionVelocity);
        for(const auto& swing:e.footSwings)SameObjectBytes(swing);
        for(float v:e.footFacing)F(v);for(float v:e.swingFacingStart)F(v);for(float v:e.swingFacingEnd)F(v);
        Add(e.gaitGrounded);Add(e.nextFoot);
        // 固定幾何沒有被複製、換模型或換鞋底容器。
        Add(reinterpret_cast<std::uintptr_t>(e.model.bindMesh.vertices.data()));Add(e.model.bindMesh.vertices.size());
        Add(reinterpret_cast<std::uintptr_t>(e.model.skeleton.joints.data()));Add(e.model.skeleton.joints.size());
        for(const auto& sole:e.soleOffsets){Add(reinterpret_cast<std::uintptr_t>(sole.data()));Add(sole.size());}
    }
    void World(std::span<const BoxCollider3D> boxes){Add(boxes.size());for(const auto& b:boxes){V(b.center);V(b.extents);}}
};
struct Live {
    Session session;Encounter enemy;Evasion evasion;
    CharacterController3D controller;PlayerLocomotion player;Explorer explorer;Boar boar;OrbitCamera orbit;
    std::vector<BoxCollider3D> solids=WorldSolids(false,Fir,Fallen);
    ThirdPersonCameraMotion3D camera;ThirdPersonFramingResult3D framing;ThirdPersonCameraMotionResult3D result;Camera3D rendered;
    SavedGameCommitTargets Targets(){return {session,enemy,evasion,controller,explorer,boar,orbit,solids,camera,framing,result,rendered,&player};}
    std::vector<std::uint64_t> State()const{
        Words w;w.Add(controller.StateHash());w.Add(player.StateHash());
        const auto stats=controller.Stats();w.Add(stats.steps);w.Add(stats.collisions);w.Add(stats.jumps);w.Add(stats.landings);
        w.Add(stats.terrainContacts);w.Add(stats.directTicks);w.Add(stats.directBlocked);w.Add(stats.directOverlaps);
        w.ExplorerState(explorer);w.Pose(boar.pose);w.V(evasion.direction);w.F(evasion.remaining);w.F(evasion.cooldown);
        w.V(enemy.feet);w.V(enemy.strikeTarget);w.Add(enemy.health);w.Add(static_cast<unsigned>(enemy.phase));
        w.F(enemy.phaseTime);w.F(enemy.playerCooldown);w.Add(enemy.countered);w.World(solids);
        for(unsigned char b:session.Serialize())w.Add(b);
        w.F(orbit.yaw);w.F(orbit.pitch);w.F(orbit.distance);
        w.SameObjectBytes(camera);w.SameObjectBytes(framing);w.SameObjectBytes(result);w.SameObjectBytes(rendered);
        return w.data;
    }
};
void Bind(Explorer& explorer){
    ExplorerNapeHairBinding::Apply(explorer);
    BackpackFit1250::Apply(explorer,{+[](void*){},nullptr});
    Need(explorer.model.Valid(),"real bound Explorer model invalid");
}
void PrepareCanonical(Live& live,float z,float yaw,const HeightField3D& terrain,const SkinnedModel3D& rawBoar){
    live.boar.model=rawBoar;Bind(live.explorer);live.orbit.yaw=yaw;
    SaveGame saved;saved.session.collected=7;saved.session.repairMaterials=7;
    saved.session.feet={0,terrain.SampleHeight(0,z),z};
    Need(saved.Valid()&&saved.session.CurrentStage()==Stage::Bridge,"canonical bridge snapshot invalid");
    auto prepared=PrepareSavedGame(saved,{live.controller,live.explorer,live.boar,live.orbit},terrain,{}, {},Fir,Fallen);
    Need(bool(prepared),"matched canonical real PrepareSavedGame rejected");
    SwapPreparedSavedGame(*prepared.candidate,live.Targets());
    PlayerLocomotionInput input;
    const auto moved=live.player.Advance(live.controller,live.evasion,live.explorer,input,1.f/60.f,live.solids,{},terrain,[]{return true;});
    Need(moved.accepted&&moved.steps==2&&!moved.poseRejected,"matched real Player/IK did not accept two fixed steps");
    live.session.feet=live.controller.Position();
    Need(!LoadPreparationDetail::BodyOverlaps(live.controller.Center(),live.controller.Extents(),live.solids),"matched old world overlaps");
}
void Preview(Live& live,const HeightField3D& terrain,CharacterDirectDisplacementPreviewToken3D& token){
    Need(live.controller.PreviewDirectDisplacementTick({live.controller.Stats().directTicks+1,{},0},live.solids,terrain,token),"real pending preview unavailable");
    Need(live.controller.CanCommitDirectDisplacementPreview(token),"new real pending preview is not committable");
}
void CheckRejected(Live& live,const HeightField3D& terrain,std::span<const BoxCollider3D> world,
    std::span<const ConvexAabbSweep3D* const> hulls,const Camera3D& camera,std::span<const BoxCollider3D> cameraOnly,
    BridgeRepairPlayerReason expected,const char* name){
    CharacterDirectDisplacementPreviewToken3D token;Preview(live,terrain,token);
    const auto before=live.State();const Words beforeWorld=[&]{Words w;w.World(world);return w;}();
    const auto result=PrepareBridgeRepairPlayer(live.controller,live.explorer,world,hulls,terrain,camera,cameraOnly);
    Words afterWorld;afterWorld.World(world);
    Need(!result&&result.reason==expected,"helper rejected for wrong reason");
    Need(live.State()==before&&beforeWorld.data==afterWorld.data,"rejected helper changed exact live state or candidate world");
    Need(live.controller.CanCommitDirectDisplacementPreview(token),"helper invalidated outstanding input preview");
    Need(live.controller.CancelDirectDisplacementPreview(token),"cannot cancel retained input preview");
    ++cases;std::cout<<"CASE "<<name<<" reason="<<unsigned(result.reason)<<" unchanged=1 pending_preview=1\n";
}
void Canonical(float z,float yaw,bool safe,const HeightField3D& terrain,const SkinnedModel3D& rawBoar){
    Live live;PrepareCanonical(live,z,yaw,terrain,rawBoar);
    auto bridge=live.session;Need(bridge.RepairBridge(),"actual RepairBridge rejected");
    auto nextWorld=WorldSolids(true,Fir,Fallen);
    Need(live.solids.size()==223&&nextWorld.size()==224,"canonical fallback population changed");
    const std::array cameraOnly{RangerHutCameraRoofSolid()};
    const auto& camera=live.camera.Camera();
    Need(ThirdPersonCameraPathClear3D(camera.position,camera.position,nextWorld,terrain,cameraOnly,.16f),"matched source old camera gate rejected");
    const bool overlap=LoadPreparationDetail::BodyOverlaps(live.controller.Center(),live.controller.Extents(),nextWorld);
    Need(overlap!=safe,"matched canonical overlap counterexample changed");
    if(!safe){CheckRejected(live,terrain,nextWorld,{},camera,cameraOnly,BridgeRepairPlayerReason::BodyOverlap,"canonical-deck");++canonicalRejected;}
    else {
        const auto physics=live.controller.StateHash(),owner=live.player.StateHash();
        const auto model=live.explorer.model.bindMesh.vertices.data();
        const auto result=PrepareBridgeRepairPlayer(live.controller,live.explorer,nextWorld,{},terrain,camera,cameraOnly);
        Need(bool(result)&&result.reason==BridgeRepairPlayerReason::Accepted,"safe31 rejected true candidate IK");
        Need(live.controller.StateHash()==physics&&live.player.StateHash()==owner&&live.explorer.model.bindMesh.vertices.data()==model,"accepted helper modified physics/input/immutable model");
        live.session=bridge;live.solids.swap(nextWorld);
        Need(live.session.bridgeRepaired&&!LoadPreparationDetail::BodyOverlaps(live.controller.Center(),live.controller.Extents(),live.solids),"safe31 cannot commit repaired world");
        Need(live.explorer.pose.globalMatrices.size()==15&&live.explorer.feetInitialized,"safe31 lacks real accepted IK pose");
        ++cases;++accepted;std::cout<<"CASE safe31 reason=0 commit=1 actual_feet_y="<<live.controller.Position().y<<'\n';
    }
    std::cout<<"CANONICAL z="<<z<<" yaw="<<yaw<<" old_body_clear=1 camera_path_clear=1 new_body_overlap="<<overlap<<'\n';
}
void CameraOnly(const HeightField3D& terrain,const SkinnedModel3D& rawBoar){
    Live live;PrepareCanonical(live,31,0,terrain,rawBoar);const auto world=WorldSolids(true,Fir,Fallen);
    const auto camera=live.camera.Camera();const std::array obstacle{BoxCollider3D{camera.position,{.3f,.3f,.3f}}};
    Need(!LoadPreparationDetail::BodyOverlaps(live.controller.Center(),live.controller.Extents(),world),"camera fixture body not clear");
    Need(!ThirdPersonCameraPathClear3D(camera.position,camera.position,world,terrain,obstacle,.16f),"real camera-only obstruction is clear");
    CheckRejected(live,terrain,world,{},camera,obstacle,BridgeRepairPlayerReason::CameraBlocked,"camera-only");++cameraRejected;
}
void Flat(Live& live,float x,const HeightField3D& terrain){
    live.solids.clear();Bind(live.explorer);live.controller.Reset({x,0,0});
    live.controller.Step({},live.solids,terrain,1.f/120.f);
    Need(live.controller.Grounded(),"flat source controller not grounded");
    live.explorer.Update({},1.f/120.f,false);
    Need(live.explorer.PlaceFeet(live.controller.Position(),true,false,terrain,live.solids),"flat source actual Explorer/IK rejected");
    live.session.feet=live.controller.Position();
}
void LocalCases(){
    HeightField3D terrain({{-16,0,-16},33,33,1});terrain.Fill(0);
    const Camera3D camera{{10,10,-10},{0,1,0}};
    const std::array wall{BoxCollider3D{{0,1,0},{2,1,4}}};const float boundary=2.f+.45f;
    for(float x:{std::nextafter(boundary,0.f),boundary,std::nextafter(boundary,(std::numeric_limits<float>::infinity)())}){
        Live live;Flat(live,x,terrain);const bool overlap=LoadPreparationDetail::BodyOverlaps(live.controller.Center(),live.controller.Extents(),wall);
        Need(overlap==(x<boundary),"strict one-ULP body overlap classification changed");
        if(overlap)CheckRejected(live,terrain,wall,{},camera,{},BridgeRepairPlayerReason::BodyOverlap,"ULP-inside");
        else {const auto physics=live.controller.StateHash(),owner=live.player.StateHash();
            const auto r=PrepareBridgeRepairPlayer(live.controller,live.explorer,wall,{},terrain,camera);
            Need(bool(r)&&r.reason==BridgeRepairPlayerReason::Accepted,"strict boundary/outside true IK rejected");
            Need(physics==live.controller.StateHash()&&owner==live.player.StateHash(),"ULP accepted helper changed physics/input");
            ++cases;++accepted;std::cout<<"CASE ULP-clear x="<<x<<" accepted=1\n";}
        ++ulpCases;
    }
    const auto cube=[](Vec3 center){std::array<Vec3,8> points{};unsigned i{};
        for(float x:{-.1f,.1f})for(float y:{-.1f,.1f})for(float z:{-.1f,.1f})points[i++]=center+Vec3{x,y,z};return points;};
    for(bool intersects:{true,false}){
        Live live;Flat(live,0,terrain);ConvexAabbSweep3D hull;const auto vertices=cube(intersects?Vec3{0,.9f,0}:Vec3{5,2,5});
        Need(hull.Prepare(vertices),"real convex hull preparation failed");const std::array<const ConvexAabbSweep3D*,1> hulls{&hull};
        Need(LoadPreparationDetail::BodyOverlaps(live.controller.Center(),live.controller.Extents(),{},hulls)==intersects,"actual roof hull overlap changed");
        if(intersects){CheckRejected(live,terrain,{},hulls,camera,{},BridgeRepairPlayerReason::BodyOverlap,"convex-roof");++hullRejected;}
        else {const auto r=PrepareBridgeRepairPlayer(live.controller,live.explorer,{},hulls,terrain,camera);
            Need(bool(r)&&r.reason==BridgeRepairPlayerReason::Accepted,"separated real hull/IK positive rejected");
            ++cases;++accepted;std::cout<<"CASE separated-roof accepted=1\n";}
    }
}
}
int main(int argc,char** argv)try{
    using namespace BridgeRepairTest;Need(argc==2,"usage: MistwatchBridgeRepairPreparationTest raw-boar.aeskin");
    std::cout<<std::boolalpha<<std::setprecision(9);
    const auto raw=LoadSkinnedModelAssetFile(argv[1]);Need(bool(raw)&&raw.model.Valid()&&raw.model.skeleton.joints.size()==16,"raw AESK1 boar missing; fallback forbidden");
    const auto terrain=CreateTerrain();Canonical(31,0,true,terrain,raw.model);
    for(float z:{32.125f,32.5f})for(float yaw:{0.f,-.65f})Canonical(z,yaw,false,terrain,raw.model);
    CameraOnly(terrain,raw.model);LocalCases();
    Need(cases==11&&canonicalRejected==4&&accepted==4&&cameraRejected==1&&hullRejected==1&&ulpCases==3,"fixture inventory incomplete");
    std::cout<<"BRIDGE_REPAIR_PREPARATION_CPU_PASS cases="<<cases<<" canonical_body_rejected="<<canonicalRejected
        <<" accepted="<<accepted<<" camera_rejected="<<cameraRejected<<" hull_rejected="<<hullRejected<<" strict_ulp="<<ulpCases
        <<" actual_main=0 authored8853=0 IK_failure_branch_tested=0 GPU=0\n";return 0;
}catch(const std::exception& error){std::cerr<<"FIRST_DIVERGENCE: "<<error.what()<<'\n';return 2;}
