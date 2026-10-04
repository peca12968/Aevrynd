#include <Aevrynd/CharacterController3D.h>
#include <array>
#include <iostream>
#include <limits>

int main(){
    using namespace Aevrynd;
    HeightField3D terrain({{-20,0,-20},81,81,.5f});
    const Vec3 target{0,1.2f,0};
    const std::array samples{Vec3{0,.2f,0},Vec3{0,1.8f,0},Vec3{-.22f,1.35f,0},Vec3{.22f,1.35f,0}};
    const auto unchanged=ResolveThirdPersonFraming3D(target,0,.3f,6,samples,{},{},terrain);
    const auto requested=MakeThirdPersonCamera(target,0,.3f,6);
    if(!unchanged.subjectVisible||unchanged.candidatesTested!=1||unchanged.yawAdjustment!=0||
       Length(unchanged.camera.position-requested.position)>1e-5f)return 1;
    const std::array solids{BoxCollider3D{{0,1,-1.5f},{.65f,1,.15f}}};
    const auto recovered=ResolveThirdPersonFraming3D(target,0,.3f,6,samples,solids,{},terrain);
    if(!recovered.subjectVisible||recovered.candidatesTested<=1||recovered.candidatesTested>37||
       std::abs(recovered.yawAdjustment)>1.570797f||Length(recovered.camera.position-target)<1.5f)return 2;
    CollisionWorld3D world;world.CreateBox({solids[0]});
    for(const auto point:samples){
        const auto offset=point-recovered.camera.position;
        if(!ProjectToViewport(point,recovered.camera,{1280,720}).visible||
           world.Raycast(MakeRay3D(recovered.camera.position,offset),Length(offset)-.03f))return 3;
    }
    const auto repeat=ResolveThirdPersonFraming3D(target,0,.3f,6,samples,solids,{},terrain);
    if(repeat.yawAdjustment!=recovered.yawAdjustment||repeat.candidatesTested!=recovered.candidatesTested||
       Length(repeat.camera.position-recovered.camera.position)!=0)return 4;
    const auto noSearch=ResolveThirdPersonFraming3D(target,0,.3f,6,samples,solids,{},terrain,{}, {1280,720},1.5f,1.57f,0);
    if(noSearch.subjectVisible||noSearch.candidatesTested!=1)return 5;
    const std::array enclosure{BoxCollider3D{{0,1,0},{1,2,1}}};
    const auto trapped=ResolveThirdPersonFraming3D(target,0,.3f,6,samples,enclosure,{},terrain);
    const auto constrained=ConstrainThirdPersonCamera(requested,target,enclosure);
    if(trapped.subjectVisible||trapped.candidatesTested!=37||Length(trapped.camera.position-constrained.position)>1e-5f)return 6;
    const auto empty=ResolveThirdPersonFraming3D(target,0,.3f,6,{},solids,{},terrain);
    const std::array invalid{Vec3{std::numeric_limits<float>::quiet_NaN(),1,0}};
    const auto nonfinite=ResolveThirdPersonFraming3D(target,0,.3f,6,invalid,solids,{},terrain);
    const auto excessive=ResolveThirdPersonFraming3D(target,0,.3f,6,samples,solids,{},terrain,{}, {1280,720},1.5f,1.57f,37);
    if(empty.subjectVisible||nonfinite.subjectVisible||excessive.subjectVisible||excessive.candidatesTested)return 7;
    for(unsigned steps:std::array{19u,36u}){
        const auto oversized=ResolveThirdPersonFraming3D(target,0,.3f,6,samples,solids,{},terrain,{}, {1280,720},1.5f,1.57f,steps);
        if(oversized.subjectVisible||oversized.candidatesTested)return 9;
    }
    const std::array invalidBoxes{BoxCollider3D{{0,1,0},{-1,1,1}}};
    const auto invalidBounds=ResolveThirdPersonFraming3D(target,0,.3f,6,samples,invalidBoxes,{},terrain);
    if(invalidBounds.subjectVisible||invalidBounds.candidatesTested)return 8;
    std::cout<<"Third-person framing PASS; clear unchanged, blocked recovered, deterministic, bounded, invalid rejected, trapped explicitly failed\n";
    return 0;
}
