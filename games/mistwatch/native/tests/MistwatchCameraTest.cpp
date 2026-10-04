#include "MistwatchCamera.h"
#include "MistwatchWorld.h"
#include "MistwatchRangerHut.h"
#include <Aevrynd/ThirdPersonCameraMotion3D.h>
#include <iostream>
#include <array>
bool CameraOverlaps(Aevrynd::Vec3 point,const Aevrynd::BoxCollider3D& solid) {
    return std::abs(point.x-solid.center.x)<solid.extents.x+.08f &&
        std::abs(point.y-solid.center.y)<solid.extents.y+.08f &&
        std::abs(point.z-solid.center.z)<solid.extents.z+.08f;
}
int main() {
    const auto terrain=Mistwatch::CreateTerrain();
    const auto solids=Mistwatch::WorldSolids(false);
    const std::array extra{Mistwatch::RangerHutCameraRoofSolid()};
    Aevrynd::CollisionWorld3D world;
    for(const auto& solid:solids)world.CreateBox({solid});
    for(const auto& solid:extra)world.CreateBox({solid});
    const auto feet=Mistwatch::Session::repairSites[0]+Aevrynd::Vec3{.75f,0,0};
    Mistwatch::OrbitCamera orbit;
    orbit.yaw=-.65f;
    const auto camera=Mistwatch::BuildPlayerCamera(feet,1.4f,orbit,solids,terrain);
    const auto distance=Aevrynd::Length(camera.position-camera.target);
    std::cout<<"shelter_camera_distance="<<distance<<" camera_height="<<camera.position.y<<'\n';
    if(distance<1.5f) {
        std::cerr<<"FIRST_DIVERGENCE: shelter ceiling retracts player camera into explorer\n";
        return 1;
    }
    const auto initialize=[&](const Aevrynd::Camera3D& candidate){
        Aevrynd::ThirdPersonCameraMotion3D motion;
        const auto result=motion.Update(candidate,Mistwatch::PlayerCameraSamples(feet,1.4f),solids,
            Mistwatch::CameraCeilingSolids(),terrain,1.f/60.f,extra);
        return result.accepted&&result.status==Aevrynd::ThirdPersonCameraMotionStatus3D::ResetCut&&
            !world.SweepSphere(candidate.position,.16f,{})&&
            Aevrynd::ThirdPersonCameraPathClear3D(candidate.position,candidate.position,solids,terrain,extra,.16f);
    };
    if(!initialize(camera)){
        std::cerr<<"FIRST_DIVERGENCE: static shelter camera cannot initialize full-volume fixed-step camera\n";return 10;
    }
    float minimumDistance=100;
    unsigned views{};
    for(unsigned angle=0;angle<36;++angle)for(float pitch:std::array{-.15f,.3f,1.1f})for(float zoom:std::array{2.5f,6.f,10.f}) {
        orbit.yaw=float(angle)*6.2831853f/36.f;orbit.pitch=pitch;orbit.distance=zoom;
        const auto view=Mistwatch::BuildPlayerCamera(feet,1.4f,orbit,solids,terrain);
        const float range=Aevrynd::Length(view.position-view.target);
        minimumDistance=(std::min)(minimumDistance,range);
        if(range<1.5f||view.position.y>1.25001f||view.position.y<.2f) {
            std::cerr<<"Shelter orbit clearance failed: angle="<<angle<<" pitch="<<pitch<<" range="<<range<<'\n';return 2;
        }
        for(const auto& solid:solids)if(CameraOverlaps(view.position,solid))return 3;
        if(!initialize(view)){
            std::cerr<<"Shelter fixed-step initialization rejected: angle="<<angle<<" pitch="<<pitch<<" zoom="<<zoom<<'\n';return 11;
        }
        for(float pointY:std::array{0.f,1.4f}) {
            if(!Aevrynd::ProjectToViewport(feet+Aevrynd::Vec3{0,pointY,0},view,{1280,720}).visible) {
                std::cerr<<"Shelter orbit crops head or feet: angle="<<angle<<" pitch="<<pitch<<'\n';return 4;
            }
        }
        ++views;
    }
    // 室外不套用棚內仰角限制，站姿與既有鏡頭逐項相符。
    orbit={};orbit.yaw=.4f;
    const Aevrynd::Vec3 outside{0,0,6};
    const auto oldTarget=outside+Aevrynd::Vec3{0,1.2f,0};
    auto previous=Aevrynd::MakeThirdPersonCamera(oldTarget,orbit.yaw,orbit.pitch,orbit.distance);
    previous=Aevrynd::ConstrainThirdPersonCamera(previous,oldTarget,solids);
    previous=Aevrynd::ConstrainThirdPersonCamera(previous,oldTarget,terrain);
    const auto standing=Mistwatch::BuildPlayerCamera(outside,1.8f,orbit,solids,terrain);
    if(Aevrynd::Length(standing.position-previous.position)>1e-5f||Aevrynd::Length(standing.target-previous.target)>1e-5f)return 5;
    const auto crouching=Mistwatch::BuildPlayerCamera(outside,1.4f,orbit,solids,terrain);
    if(std::abs(crouching.target.y-.85f)>1e-5f||std::abs(crouching.position.y-(.85f+std::sin(orbit.pitch)*orbit.distance))>1e-5f)return 6;
    // 真正沿入口蹲行，不以 teleport 代替通行；每一幀檢查鏡頭不進屋頂或牆面。
    Aevrynd::CharacterController3D actor;actor.Reset(feet+Aevrynd::Vec3{4,0,0});actor.SetHeight(1.4f);
    Aevrynd::ThirdPersonCameraMotion3D entranceCamera;
    Aevrynd::Camera3D priorCamera{};
    Aevrynd::Vec3 priorFeet=actor.Position();
    unsigned continuousFrames{};
    const auto advanceCamera=[&](){
        const auto request=Mistwatch::BuildPlayerCameraMotionRequest(actor.Position(),actor.Settings().height,orbit,
            entranceCamera.Initialized(),solids,terrain,extra);
        const auto result=entranceCamera.Update(request,Mistwatch::PlayerCameraSamples(actor.Position(),actor.Settings().height),
            solids,Mistwatch::CameraCeilingSolids(),terrain,1.f/60.f,extra);
        if(!result.accepted||world.SweepSphere(result.camera.position,.16f,{})||
            (continuousFrames&&(Aevrynd::Length(result.camera.position-priorCamera.position)>
                .50001f+Aevrynd::Length(actor.Position()-priorFeet)||
                world.SweepSphere(priorCamera.position,.16f,result.camera.position-priorCamera.position))))return false;
        priorCamera=result.camera;priorFeet=actor.Position();++continuousFrames;
        return true;
    };
    if(!advanceCamera())return 12;
    unsigned entranceFrames{};
    for(unsigned tick=0;tick<100;++tick) {
        actor.Step({{-1,0,0},false,.45f},solids,terrain,1.f/60.f);
        const auto view=Mistwatch::BuildPlayerCamera(actor.Position(),actor.Settings().height,orbit,solids,terrain);
        for(const auto& solid:solids)if(CameraOverlaps(view.position,solid))return 7;
        if(!advanceCamera()){
            std::cerr<<"FIRST_DIVERGENCE: continuous shelter entrance camera rejected tick="<<tick<<'\n';return 13;
        }
        ++entranceFrames;
    }
    if(actor.Position().x>feet.x+.02f||actor.TrySetHeight(1.8f,solids))return 8;
    for(unsigned tick=0;tick<100;++tick){
        actor.Step({{1,0,0},false,.45f},solids,terrain,1.f/60.f);
        if(!advanceCamera()){
            std::cerr<<"FIRST_DIVERGENCE: continuous shelter exit camera rejected tick="<<tick<<'\n';return 14;
        }
    }
    if(!actor.TrySetHeight(1.8f,solids))return 9;
    if(!advanceCamera())return 15;
    std::cout<<"shelter_views="<<views<<" minimum_camera_distance="<<minimumDistance
        <<" entrance_frames="<<entranceFrames<<" outdoor_standing_unchanged=1 full_volume_initializations="<<views+1
        <<" continuous_entry_exit_frames="<<continuousFrames<<'\n';
    return 0;
}
