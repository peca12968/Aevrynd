#include "MistwatchCamera.h"
#include "MistwatchWorld.h"
#include "MistwatchRangerHut.h"
#include <array>
#include <iostream>

int main() {
    using namespace Aevrynd;
    const auto terrain=Mistwatch::CreateTerrain();
    const auto solids=Mistwatch::WorldSolids(true);
    const std::array roofs{Mistwatch::WorkshopSolids()[0],Mistwatch::RangerHutSolids()[0],Mistwatch::RepairShelterSolids()[0]};
    const std::array cameraOnly{Mistwatch::RangerHutCameraRoofSolid()};
    CollisionWorld3D world;
    for(const auto& box:solids)world.CreateBox({box});
    for(const auto& box:cameraOnly)world.CreateBox({box});
    for(unsigned i=0;i<roofs.size();++i){
        const auto& routed=Mistwatch::CameraCeilingSolids()[i];
        if(Length(routed.center-roofs[i].center)>1e-5f||Length(routed.extents-roofs[i].extents)>1e-5f)return 1;
    }
    unsigned framingFailures{},views{};
    float minimum=100;
    for(unsigned site=0;site<3;++site) {
      const auto p=Mistwatch::Session::relaySites[site];
      const auto baseHit=world.Raycast(MakeRay3D(p+Vec3{2,.35f,0},{-1,0,0}),2);
      if(!baseHit||std::abs(baseHit.distance-1.35f)>.001f){std::cerr<<"Relay base collision missing\n";return 3;}
      for(float approach:std::array{1.15f,-1.15f}){
        auto feet=p+Vec3{approach,0,0};
        feet.y=terrain.SampleHeight(feet.x,feet.z);
        for(unsigned frame=0;frame<36;++frame) {
            Mistwatch::OrbitCamera orbit;orbit.yaw=1.25f+float(frame)*6.2831853f/36.f;
            const auto target=feet+Vec3{0,1.2f,0};
            auto expected=MakeThirdPersonCamera(target,orbit.yaw,orbit.pitch,orbit.distance);
            expected=ConstrainThirdPersonCameraCeiling(expected,roofs);
            expected=ConstrainThirdPersonCamera(expected,target,solids);
            expected=ConstrainThirdPersonCameraCeiling(expected,cameraOnly);
            expected=ConstrainThirdPersonCamera(expected,target,cameraOnly);
            expected=ConstrainThirdPersonCamera(expected,target,terrain);
            const auto actual=Mistwatch::BuildPlayerCamera(feet,1.8f,orbit,solids,terrain,cameraOnly);
            const auto cameraOffset=actual.position-actual.target;
            if(std::abs(std::remainder(Mistwatch::PlayerCameraMovementYaw(actual,orbit.yaw)-std::atan2(cameraOffset.x,-cameraOffset.z),6.2831853f))>1e-5f)return 4;
            const auto movementYaw=Mistwatch::PlayerCameraMovementYaw(actual,orbit.yaw);
            const Vec3 forward{-std::sin(movementYaw),0,std::cos(movementYaw)};
            const Vec3 right{std::cos(movementYaw),0,std::sin(movementYaw)};
            const auto look=Normalize(Vec3{actual.target.x-actual.position.x,0,actual.target.z-actual.position.z});
            if(Dot(forward,look)<.99999f||Dot(right,Normalize(Cross(Vec3{0,1,0},look)))<.99999f)return 5;
            const auto samples=std::array{feet+Vec3{0,.2f,0},feet+Vec3{0,1.8f,0},feet+Vec3{0,.9f,0},
                feet+Vec3{.22f,1.35f,0},feet+Vec3{-.22f,1.35f,0},feet+Vec3{0,1.35f,.22f},feet+Vec3{0,1.35f,-.22f}};
            bool clear=Length(actual.position-target)>=1.5f&&!world.OverlapsBox(actual.position,{.08f,.08f,.08f});
            for(const auto sample:samples){
                const auto offset=sample-actual.position;
                clear&=ProjectToViewport(sample,actual,{1280,720}).visible&&
                    !world.Raycast(MakeRay3D(actual.position,offset),Length(offset)-.03f);
            }
            if(!clear){++framingFailures;std::cerr<<"framing_failure site="<<site<<" approach="<<approach<<" frame="<<frame<<'\n';}
            minimum=(std::min)(minimum,Length(actual.position-target));++views;
            if(site==0&&approach>0&&(frame==18||frame==20))std::cout<<"ridge_frame="<<frame<<" actual_distance="<<Length(actual.position-target)
                <<" roofs_only_distance="<<Length(expected.position-target)<<" actual_y="<<actual.position.y
                <<" roofs_only_y="<<expected.position.y<<'\n';
        }
      }
    }
    if(framingFailures){std::cerr<<"FIRST_DIVERGENCE: player framing cropped/too close; views="<<framingFailures<<'\n';return 2;}
    std::cout<<"Relay framing PASS; views="<<views<<" minimum_distance="<<minimum<<" equipment collision retained; both approaches\n";
    return 0;
}
