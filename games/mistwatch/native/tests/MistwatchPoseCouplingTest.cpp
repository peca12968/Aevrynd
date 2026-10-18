#include "MistwatchLocomotionMotor.h"
#include "MistwatchExplorer.h"
#include <iostream>
#include <stdexcept>
using namespace Aevrynd;using namespace Mistwatch;
int main()try{
    HeightField3D terrain({{-20,0,-20},128,128,1});terrain.Fill(0);
    CharacterController3D actor;actor.Reset({0,0,0});
    if(!actor.Step({}, {},{},terrain,LocomotionMotor::FixedStep))throw std::runtime_error("initial contact");
    LocomotionMotor motor;Explorer explorer;
    if(!explorer.PlaceFeet(actor.Position(),actor.Grounded(),false,terrain,{}))throw std::runtime_error("initial pose");
    SkinningWorkspace3D workspace;Mesh3D mesh;
    for(unsigned tick=0;tick<840;++tick){
        LocomotionInput input;input.enableRunFlight=true;
        input.movement=tick<360?Vec3{0,0,1}:tick<540?Vec3{}:Vec3{0,0,-1};
        const auto before=actor.Position();
        if(!motor.Step(actor,input,{}, {},terrain))throw std::runtime_error("motor");
        explorer.Update(actor.Position()-before,LocomotionMotor::FixedStep,false);
        if(!explorer.PlaceFeet(actor.Position(),actor.Grounded(),false,terrain,{})){
            std::cerr<<"POSE_COUPLING_DIVERGENCE tick="<<tick<<" reason=existing_placefeet root="<<actor.Position().x<<','<<actor.Position().y<<','<<actor.Position().z<<" grounded="<<actor.Grounded()<<" motor_phase="<<static_cast<unsigned>(motor.Phase())<<'\n';return 2;
        }
        if(!SkinModelInto(explorer.model,explorer.pose,workspace,mesh))throw std::runtime_error("skin");
        float minimum=100;
        for(unsigned side=0;side<2;++side)for(auto sole:explorer.soleOffsets[side]){
            const auto local=TransformPoint(explorer.pose.globalMatrices[11+side*3],sole);
            const auto world=TransformPoint(CreateTransformMatrix(actor.Position(),{0,explorer.yaw,0},{1,1,1}),local);
            minimum=(std::min)(minimum,world.y);
        }
        if(minimum<-.005f){std::cerr<<"POSE_COUPLING_DIVERGENCE tick="<<tick<<" reason=sole_penetration min_y="<<minimum<<" grounded="<<actor.Grounded()<<" motor_phase="<<static_cast<unsigned>(motor.Phase())<<'\n';return 3;}
    }
    std::cout<<"POSE_COUPLING_PASS ticks=840\n";return 0;
}catch(const std::exception& e){std::cerr<<"POSE_COUPLING_FAIL "<<e.what()<<'\n';return 1;}
