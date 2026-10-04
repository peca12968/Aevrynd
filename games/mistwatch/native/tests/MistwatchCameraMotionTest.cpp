#include <Aevrynd/ThirdPersonCameraMotion3D.h>
#include "MistwatchCamera.h"
#include "MistwatchRangerHut.h"
#include <iostream>

int main(){
    using namespace Aevrynd;
    const auto terrain=Mistwatch::CreateTerrain();
    const auto solids=Mistwatch::WorldSolids(true);
    const std::array extra{Mistwatch::RangerHutCameraRoofSolid()};
    CollisionWorld3D world;
    for(const auto& box:solids)world.CreateBox({box});
    for(const auto& box:extra)world.CreateBox({box});
    float worst{};
    unsigned blockedViews{};
    for(unsigned site=0;site<3;++site)for(float approach:std::array{1.15f,-1.15f})for(float direction:std::array{1.f,-1.f}){
        auto feet=Mistwatch::Session::relaySites[site]+Vec3{approach,0,0};feet.y=terrain.SampleHeight(feet.x,feet.z);
        ThirdPersonCameraMotion3D state;Camera3D previous{};
        Mistwatch::OrbitCamera initialOrbit;initialOrbit.yaw=1.25f;
        const auto initial=state.Update(Mistwatch::BuildPlayerCameraMotionRequest(feet,1.8f,initialOrbit,false,solids,terrain,extra),
            Mistwatch::PlayerCameraSamples(feet,1.8f),solids,Mistwatch::CameraCeilingSolids(),terrain,1.f/60.f,extra);
        if(!initial.accepted)return 5;
        const float startYaw=Mistwatch::PlayerCameraMovementYaw(initial.camera,initialOrbit.yaw);
        for(unsigned tick=0;tick<720;++tick){
            Mistwatch::OrbitCamera orbit;orbit.yaw=startYaw+direction*float(tick)*6.2831853f/720.f;
            const auto desired=Mistwatch::BuildPlayerCameraMotionRequest(feet,1.8f,orbit,true,solids,terrain,extra);
            const std::array samples{feet+Vec3{0,.2f,0},feet+Vec3{0,1.8f,0},feet+Vec3{0,.9f,0},
                feet+Vec3{.22f,1.35f,0},feet+Vec3{-.22f,1.35f,0},feet+Vec3{0,1.35f,.22f},feet+Vec3{0,1.35f,-.22f}};
            const auto motion=state.Update(desired,samples,solids,Mistwatch::CameraCeilingSolids(),terrain,1.f/60.f,extra);
            if(!motion.accepted){std::cerr<<"Rejected tick="<<tick<<" status="<<int(motion.status)<<'\n';return 4;}
            const auto camera=motion.camera;
            if(tick){
                const float shift=Length(camera.position-previous.position);worst=(std::max)(worst,shift);
                if(shift>.5f||world.SweepSphere(previous.position,.16f,camera.position-previous.position)){
                    std::cerr<<"Transition failure site="<<site<<" side="<<approach<<" tick="<<tick<<" displacement="<<shift<<'\n';return 1;
                }
            }
            if(Length(camera.position-camera.target)<1.5f||!ProjectToViewport(feet+Vec3{0,.2f,0},camera,{1280,720}).visible||
                !ProjectToViewport(feet+Vec3{0,1.8f,0},camera,{1280,720}).visible)return 2;
            const auto ray=MakeRay3D(camera.position,feet+Vec3{0,.9f,0}-camera.position);
            for(const auto& box:solids)if(RayIntersectsAabb(ray,box.center,box.extents,Length(feet+Vec3{0,.9f,0}-camera.position)-.03f)){++blockedViews;break;}
            previous=camera;
        }
    }
    const auto desired=MakeThirdPersonCamera({0,1.2f,6},0,.3f,6);
    ThirdPersonCameraMotion3D exceptional;
    const std::array samples{Vec3{0,.2f,6},Vec3{0,1.8f,6}};
    const auto initial=exceptional.Update(desired,samples,solids,Mistwatch::CameraCeilingSolids(),terrain,1.f/60.f,extra).camera;
    auto other=desired;other.position=MakeThirdPersonCamera(desired.target,1,.3f,6).position;
    const auto delayed=exceptional.Update(other,samples,solids,Mistwatch::CameraCeilingSolids(),terrain,.5f,extra);
    if(delayed.accepted||delayed.status!=ThirdPersonCameraMotionStatus3D::InvalidInput||Length(delayed.camera.position-initial.position)>1e-5f){std::cerr<<"FIRST_DIVERGENCE: oversized dt advanced camera\n";return 3;}
    std::cout<<"Relay transition PASS; views=8640 max_displacement="<<worst<<" temporary_occluded_views="<<blockedViews<<'\n';
    return 0;
}
