#include <Aevrynd/ThirdPersonCameraMotion3D.h>
#include "MistwatchCamera.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string_view>
#include <vector>

namespace {
using namespace Aevrynd;
constexpr float tick=1.f/60.f,tau=6.2831853f;
unsigned checks{},steps{};

void Require(bool condition,std::string_view name){
    ++checks;
    if(!condition)throw std::runtime_error(std::string(name));
}
float Yaw(const Camera3D& camera){const auto offset=camera.position-camera.target;return std::atan2(offset.x,-offset.z);}
float Pitch(const Camera3D& camera){const auto offset=camera.position-camera.target;return std::atan2(offset.y,std::hypot(offset.x,offset.z));}
float Range(const Camera3D& camera){return Length(camera.position-camera.target);}
bool Same(const ThirdPersonCameraMotionResult3D& a,const ThirdPersonCameraMotionResult3D& b,float tolerance=.00002f){
    return a.accepted==b.accepted&&a.status==b.status&&a.subjectVisible==b.subjectVisible&&
        Length(a.camera.position-b.camera.position)<=tolerance&&Length(a.camera.target-b.camera.target)<=tolerance&&
        std::abs(a.displacement-b.displacement)<=tolerance;
}

// 小型固定地形與盒子，避免森林內容變動令測試失去可重現性。
struct Fixture {
    HeightField3D terrain{{{-10,0,-10},81,81,.25f}};
    std::vector<BoxCollider3D> boxes;
    std::array<Vec3,7> samples=Mistwatch::PlayerCameraSamples({},1.8f);
    Camera3D initial{},wanted{};
    ThirdPersonCameraMotion3D motion;
};

void SafeStep(const Camera3D& previous,const ThirdPersonCameraMotionResult3D& result,
    std::span<const Vec3> samples,std::span<const BoxCollider3D> boxes,const HeightField3D& terrain){
    Require(result.accepted,"expected an accepted safe camera step");
    CollisionWorld3D independent;
    for(const auto& box:boxes)independent.CreateBox({box});
    const auto delta=result.camera.position-previous.position;
    Require(!independent.SweepSphere(previous.position,.16f,delta),"independent sphere sweep found camera collision");
    const float allowance=.5f+Length(result.camera.target-previous.target)+.00002f;
    Require(Length(delta)<=allowance,"camera exceeded fixed-tick displacement allowance");
    for(const auto sample:samples)Require(ProjectToViewport(sample,result.camera,{1280,720}).visible,"subject sample left viewport");
    // 這是獨立有限取樣 oracle，不宣稱涵蓋任意窄峰；下方峰值案例使用 .25 公尺網格。
    for(unsigned i=0;i<=128;++i){
        const auto point=previous.position+delta*(float(i)/128.f);
        if(terrain.Contains(point.x,point.z)){
            const float height=terrain.SampleHeight(point.x,point.z);
            Require(std::isfinite(height)&&point.y>=height+.16f-.00002f,"independent terrain samples found unsafe step");
        }
    }
    ++steps;
}

Fixture Active(float yaw=0){
    Fixture f;
    f.initial=MakeThirdPersonCamera({0,1.2f,0},yaw,.3f,6);
    const float c=std::cos(yaw),s=std::sin(yaw);
    const Vec3 unrotated{.38f,f.initial.position.y,-6.f*std::cos(.3f)};
    f.boxes.push_back({{unrotated.x*c-unrotated.z*s,unrotated.y,unrotated.x*s+unrotated.z*c},{.06f,.25f,.06f}});
    const auto cut=f.motion.Update(f.initial,f.samples,f.boxes,{},f.terrain,tick);
    Require(cut.accepted&&cut.status==ThirdPersonCameraMotionStatus3D::ResetCut,"fixture safe initial cut rejected");
    f.wanted=MakeThirdPersonCamera(f.initial.target,yaw+.34906585f,.3f,6,f.initial.projection);
    CollisionWorld3D independent;
    for(const auto& box:f.boxes)independent.CreateBox({box});
    const auto direct=MakeThirdPersonCamera(f.initial.target,yaw+2.4f*tick,.3f,6,f.initial.projection);
    Require(bool(independent.SweepSphere(f.initial.position,.16f,direct.position-f.initial.position)),"fixture did not obstruct direct angular step");
    const auto prepare=f.motion.Update(f.wanted,f.samples,f.boxes,{},f.terrain,tick);
    SafeStep(f.initial,prepare,f.samples,f.boxes,f.terrain);
    Require(prepare.displacement>.0001f&&std::abs(std::remainder(Yaw(prepare.camera)-yaw,tau))<.0001f&&
        (std::abs(Pitch(prepare.camera)-.3f)>.0001f||std::abs(Range(prepare.camera)-6.f)>.0001f),
        "fixture did not enter active route preparation");
    std::cout<<"ACTIVE_ROUTE yaw="<<yaw<<" pitch="<<Pitch(prepare.camera)<<" range="<<Range(prepare.camera)<<'\n';
    return f;
}

void PreserveAfterRejected(std::string_view label,float dt,bool projectionChange=false){
    auto f=Active();auto control=f.motion;auto request=f.wanted;
    if(projectionChange)request.projection.nearPlane+=.01f;
    const auto before=f.motion.Camera();
    const auto rejected=f.motion.Update(request,f.samples,f.boxes,{},f.terrain,dt);
    Require(!rejected.accepted&&Length(f.motion.Camera().position-before.position)==0,"rejected call changed accepted camera");
    Require(rejected.status==(dt==0?ThirdPersonCameraMotionStatus3D::Paused:ThirdPersonCameraMotionStatus3D::InvalidInput),"wrong rejected-input status");
    for(unsigned i=0;i<24;++i){
        const auto previous=f.motion.Camera();
        const auto a=f.motion.Update(f.wanted,f.samples,f.boxes,{},f.terrain,tick);
        const auto b=control.Update(f.wanted,f.samples,f.boxes,{},f.terrain,tick);
        Require(Same(a,b),"rejected call altered subsequent cached-route behavior");
        SafeStep(previous,a,f.samples,f.boxes,f.terrain);
    }
    std::cout<<"PRESERVED "<<label<<'\n';
}

void Superseded(std::string_view label,int kind){
    auto f=Active();const auto before=f.motion.Camera();
    float yaw=Yaw(before),pitch=.3f,range=6;
    if(kind==1)yaw+=.01f;
    if(kind==2)yaw-=.15f;
    if(kind==3){yaw=Yaw(f.wanted);pitch=.45f;}
    if(kind==4){yaw=Yaw(f.wanted);range=5.f;}
    const auto request=MakeThirdPersonCamera(before.target,yaw,pitch,range,before.projection);
    ThirdPersonCameraMotion3D fresh;
    Require(fresh.Update(before,f.samples,f.boxes,{},f.terrain,tick).accepted,"fresh comparator initial cut rejected");
    const auto result=f.motion.Update(request,f.samples,f.boxes,{},f.terrain,tick);
    const auto expected=fresh.Update(request,f.samples,f.boxes,{},f.terrain,tick);
    Require(Same(result,expected),"superseded request continued stale cached route");
    SafeStep(before,result,f.samples,f.boxes,f.terrain);
    const float change=std::remainder(Yaw(result.camera)-Yaw(before),tau);
    if(kind==0)Require(std::abs(change)<.00002f,"stop request continued angular movement");
    if(kind==1)Require(change>=-.00002f&&change<=.01002f,"shortened request overshot its heading");
    if(kind==2)Require(change<-.0001f,"reversal did not change angular direction");
    std::cout<<"SUPERSEDED "<<label<<" yaw_change="<<change<<'\n';
}

void SubjectChanges(){
    for(bool crouch:std::array{false,true}){
        auto f=Active();const Vec3 translation{.02f,0,.01f};
        const float height=crouch?1.4f:1.8f;
        const auto samples=Mistwatch::PlayerCameraSamples(translation,height);
        const auto target=translation+Vec3{0,crouch?.85f:1.2f,0};
        const auto request=MakeThirdPersonCamera(target,Yaw(f.wanted),.3f,6,f.wanted.projection);
        for(unsigned i=0;i<24;++i){
            const auto previous=f.motion.Camera();
            const auto result=f.motion.Update(request,samples,f.boxes,{},f.terrain,tick);
            SafeStep(previous,result,samples,f.boxes,f.terrain);
            Require(Length(result.camera.target-target)<.00002f,"translated/crouched target not adopted");
        }
        std::cout<<"SUBJECT_CHANGE crouch="<<crouch<<'\n';
    }
}

void ChangedFutureExit(bool terrainBarrier){
    auto f=Active();const auto before=f.motion.Camera();
    if(terrainBarrier){
        // x=.75 的整列高峰堵住既有出口，仍離目前準備位置與下一小步很遠。
        for(unsigned z=0;z<f.terrain.Settings().depth;++z)Require(f.terrain.SetHeight(43,z,100.f),"failed to create terrain barrier");
    }else f.boxes.push_back({{.7f,10,0},{.03f,20,20}});
    const auto result=f.motion.Update(f.wanted,f.samples,f.boxes,{},f.terrain,tick);
    SafeStep(before,result,f.samples,f.boxes,f.terrain);
    Require(Length(result.camera.position-before.position)<.00002f,"continued radial preparation after future exit was blocked");
    Require(result.status==ThirdPersonCameraMotionStatus3D::Held,"blocked future exit did not hold safely");
    std::cout<<"FUTURE_EXIT_BLOCKED terrain="<<terrainBarrier<<'\n';
}

void CorruptTerrain(){
    auto f=Active();const auto before=f.motion.Camera();
    auto corrupt=f.terrain;corrupt.Fill(std::numeric_limits<float>::quiet_NaN());
    const auto rejected=f.motion.Update(f.wanted,f.samples,f.boxes,{},corrupt,tick);
    Require(!rejected.accepted&&Length(f.motion.Camera().position-before.position)==0,"corrupt terrain accepted motion or changed camera");
    const auto recovered=f.motion.Update(f.wanted,f.samples,f.boxes,{},f.terrain,tick);
    SafeStep(before,recovered,f.samples,f.boxes,f.terrain);
    std::cout<<"CORRUPT_TERRAIN rejected_then_recovered\n";
}

void ExplicitReset(){
    auto f=Active();f.motion.Reset();
    Require(!f.motion.Initialized(),"Reset retained initialized state");
    Require(!f.motion.Update(f.initial,f.samples,f.boxes,{},f.terrain,0).accepted&&!f.motion.Initialized(),"paused Reset initialized camera");
    const auto cut=f.motion.Update(f.initial,f.samples,f.boxes,{},f.terrain,tick);
    Require(cut.accepted&&cut.status==ThirdPersonCameraMotionStatus3D::ResetCut&&Length(cut.camera.position-f.initial.position)==0,"Reset did not establish explicit fresh cut");
    ThirdPersonCameraMotion3D fresh;
    fresh.Update(f.initial,f.samples,f.boxes,{},f.terrain,tick);
    for(unsigned i=0;i<24;++i){
        const auto previous=f.motion.Camera();
        const auto a=f.motion.Update(f.wanted,f.samples,f.boxes,{},f.terrain,tick);
        const auto b=fresh.Update(f.wanted,f.samples,f.boxes,{},f.terrain,tick);
        Require(Same(a,b),"Reset retained old route behavior");SafeStep(previous,a,f.samples,f.boxes,f.terrain);
    }
    std::cout<<"RESET cache_behavior_matches_fresh\n";
}

void WrappedRequest(){
    auto f=Active(3.12f);auto wrapped=f.motion;
    const auto alternate=MakeThirdPersonCamera(f.wanted.target,Yaw(f.wanted)-tau,.3f,6,f.wanted.projection);
    float travel{};float previousYaw=Yaw(f.motion.Camera());
    for(unsigned i=0;i<120;++i){
        const auto previous=f.motion.Camera();
        const auto a=f.motion.Update(f.wanted,f.samples,f.boxes,{},f.terrain,tick);
        const auto b=wrapped.Update(alternate,f.samples,f.boxes,{},f.terrain,tick);
        Require(Same(a,b,.0002f),"equivalent wrapped yaw changed route behavior");
        SafeStep(previous,a,f.samples,f.boxes,f.terrain);
        travel+=std::remainder(Yaw(a.camera)-previousYaw,tau);previousYaw=Yaw(a.camera);
    }
    Require(travel>.25f&&std::abs(std::remainder(Yaw(f.wanted)-previousYaw,tau))<.03f,"wrap crossing stalled or reversed route");
    std::cout<<"YAW_WRAP signed_travel="<<travel<<'\n';
}
}

int main(){
    try{
        PreserveAfterRejected("pause",0);
        PreserveAfterRejected("negative_dt",-1);
        PreserveAfterRejected("large_dt",.2f);
        PreserveAfterRejected("nan_dt",std::numeric_limits<float>::quiet_NaN());
        PreserveAfterRejected("infinite_dt",std::numeric_limits<float>::infinity());
        PreserveAfterRejected("projection_change",tick,true);
        Superseded("stop",0);Superseded("shorten",1);Superseded("reverse",2);
        Superseded("pitch",3);Superseded("zoom",4);
        SubjectChanges();ChangedFutureExit(false);ChangedFutureExit(true);
        CorruptTerrain();ExplicitReset();WrappedRequest();
        std::cout<<"Route context PASS; checks="<<checks<<" independently_validated_steps="<<steps<<'\n';
        return 0;
    }catch(const std::exception& error){
        std::cerr<<"FIRST_DIVERGENCE: "<<error.what()<<" checks="<<checks<<" independently_validated_steps="<<steps<<'\n';
        return 1;
    }
}
