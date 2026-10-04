#include <Aevrynd/ThirdPersonCameraMotion3D.h>
#include "MistwatchCamera.h"
#include "MistwatchRangerHut.h"
#include <iostream>
int main(){
    using namespace Aevrynd;
    const auto terrain=Mistwatch::CreateTerrain();const auto solids=Mistwatch::WorldSolids(true);
    const std::array extra{Mistwatch::RangerHutCameraRoofSolid()};
    CollisionWorld3D world;for(const auto& box:solids)world.CreateBox({box});for(const auto& box:extra)world.CreateBox({box});
    float worst{},minimumTravel=100;unsigned views{};
    for(unsigned site=0;site<3;++site)for(float side:std::array{1.15f,-1.15f})for(float direction:std::array{1.f,-1.f}){
    auto feet=Mistwatch::Session::relaySites[site]+Vec3{side,0,0};feet.y=terrain.SampleHeight(feet.x,feet.z);
    const auto samples=Mistwatch::PlayerCameraSamples(feet,1.8f);
    ThirdPersonCameraMotion3D motion;float previousYaw{},travel{},maximum{};unsigned held{},longest{},consecutive{};
    Camera3D previous{};std::array<float,60> progressWindow{};unsigned progressIndex{};float windowTravel{};
    float startYaw=1.25f;
    {
        Mistwatch::OrbitCamera initial;initial.yaw=startYaw;
        const auto cut=motion.Update(Mistwatch::BuildPlayerCamera(feet,1.8f,initial,solids,terrain,extra),samples,
            solids,Mistwatch::CameraCeilingSolids(),terrain,1.f/60,extra);
        if(!cut.accepted)return 5;
        // 安全起始朝向之後，玩家要求同向旋轉 350 度；不把初始避障切鏡計入轉向成果。
        startYaw=Mistwatch::PlayerCameraMovementYaw(cut.camera,0);
        std::cout<<"INITIAL_HEADING site="<<site<<" side="<<side<<" raw=1.25 safe="<<startYaw<<'\n';
    }
    for(unsigned frame=0;frame<36;++frame)for(unsigned tick=0;tick<30;++tick){
        Mistwatch::OrbitCamera orbit;orbit.yaw=startYaw+direction*float(frame)*6.2831853f/36.f;
        const auto requested=Mistwatch::BuildPlayerCameraMotionRequest(feet,1.8f,orbit,motion.Initialized(),solids,terrain,extra);
        const auto result=motion.Update(requested,samples,
            solids,Mistwatch::CameraCeilingSolids(),terrain,1.f/60,extra);
        if(!result.accepted){std::cerr<<"Rejected frame="<<frame<<" tick="<<tick<<'\n';return 2;}
        const auto yaw=Mistwatch::PlayerCameraMovementYaw(result.camera,0);
        if(frame||tick){
            const float delta=std::remainder(yaw-previousYaw,6.2831853f);travel+=delta;
            windowTravel-=progressWindow[progressIndex%60];progressWindow[progressIndex%60]=delta*direction;
            windowTravel+=progressWindow[progressIndex%60];++progressIndex;
            const float outstanding=std::remainder(orbit.yaw-yaw,6.2831853f)*direction;
            if(delta*direction<-.005f||(progressIndex>=60&&outstanding>.1f&&windowTravel<.005f)){
                std::cerr<<"FIRST_DIVERGENCE: raw heading reversed or made no actual angular progress frame="<<frame<<" tick="<<tick<<'\n';return 7;
            }
            if(world.SweepSphere(previous.position,.16f,result.camera.position-previous.position)||result.displacement>.5f){std::cerr<<"Stepped swept-volume violation\n";return 3;}
        }
        for(const auto sample:samples)if(!ProjectToViewport(sample,result.camera,{1280,720}).visible)return 4;
        previousYaw=yaw;previous=result.camera;++views;
        maximum=(std::max)(maximum,result.displacement);
        if(result.status==ThirdPersonCameraMotionStatus3D::Held){++held;++consecutive;longest=(std::max)(longest,consecutive);}else consecutive=0;
    }
    std::cout<<"Stepped orbit site="<<site<<" side="<<side<<" direction="<<direction<<" travel="<<travel<<" max_tick_displacement="<<maximum<<" held_ticks="<<held<<" longest_hold="<<longest<<'\n';
    if(travel*direction<5.5f||longest>60){std::cerr<<"FIRST_DIVERGENCE: safe transition stalled before completing orbit\n";return 1;}
    if(std::abs(std::remainder(startYaw+direction*35.f*6.2831853f/36.f-previousYaw,6.2831853f))>.1f){std::cerr<<"FIRST_DIVERGENCE: final raw heading not reached\n";return 6;}
    worst=(std::max)(worst,maximum);minimumTravel=(std::min)(minimumTravel,travel*direction);
    }
    std::cout<<"Stepped transition PASS; views="<<views<<" maximum_tick_displacement="<<worst<<" minimum_orbit_travel="<<minimumTravel<<'\n';
    return 0;
}
