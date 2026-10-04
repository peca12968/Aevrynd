#include <Aevrynd/ThirdPersonCameraMotion3D.h>
#include <array>
#include <iostream>
#include <limits>

int main(){
    using namespace Aevrynd;
    const std::array samples{Vec3{0,.2f,0},Vec3{0,1.8f,0},Vec3{0,.9f,0}};
    HeightField3D terrain({{-50,0,-50},101,101,1});
    const auto desired=MakeThirdPersonCamera({0,1.2f,0},0,.3f,6);
    unsigned checks{},failures{};
    const auto check=[&](bool ok,const char* name){++checks;if(!ok){++failures;std::cerr<<"FIRST_DIVERGENCE: "<<name<<'\n';}};
    ThirdPersonCameraMotion3D paused;
    const auto zero=paused.Update(desired,samples,{}, {},terrain,0);
    check(!zero.accepted&&!paused.Initialized()&&zero.status==ThirdPersonCameraMotionStatus3D::Paused,"zero dt initialized camera while paused");
    const std::array pointBox{BoxCollider3D{{0,1,0},{0,0,0}}};
    check(!ThirdPersonCameraPathClear3D({-.120004f,1,0},{-.119999f,1,0},pointBox,terrain),"tiny nonzero segment entered expanded box");
    const float nan=std::numeric_limits<float>::quiet_NaN();
    for(unsigned corner=0;corner<4;++corner){
        HeightField3D corrupt({{0,0,0},2,2,1});corrupt.SetHeight(corner%2,corner/2,nan);
        check(!ThirdPersonCameraPathClear3D({.25f,2,.25f},{.75f,2,.75f},{},corrupt),"non-first NaN terrain corner accepted");
    }
    ThirdPersonCameraMotion3D motion;
    auto initial=motion.Update(desired,samples,{}, {},terrain,1.f/60);
    check(initial.accepted&&initial.status==ThirdPersonCameraMotionStatus3D::ResetCut,"valid explicit initial cut rejected");
    const auto original=motion.Camera();
    auto distant=desired;distant.position.x+=10;distant.target.x+=10;
    std::array distantSamples=samples;for(auto& p:distantSamples)p.x+=10;
    auto teleport=motion.Update(distant,distantSamples,{}, {},terrain,1.f/60);
    check(!teleport.accepted&&teleport.status==ThirdPersonCameraMotionStatus3D::Blocked&&Length(motion.Camera().position-original.position)==0,"implicit teleport bypassed continuity");
    motion.Reset();teleport=motion.Update(distant,distantSamples,{}, {},terrain,1.f/60);
    check(teleport.accepted&&teleport.status==ThirdPersonCameraMotionStatus3D::ResetCut,"explicit teleport Reset rejected");
    for(float dt:std::array{0.f,-1.f,.5f,nan,std::numeric_limits<float>::infinity()}){
        auto result=motion.Update(distant,distantSamples,{}, {},terrain,dt);
        check(!result.accepted&&Length(result.camera.position-motion.Camera().position)==0,"invalid or paused dt changed camera");
    }
    const std::array invalidCeiling{BoxCollider3D{{0,5,0},{-1,1,1}}};
    check(motion.Update(distant,distantSamples,{},invalidCeiling,terrain,1.f/60).status==ThirdPersonCameraMotionStatus3D::InvalidInput,"invalid ceiling silently skipped");
    auto badUp=desired;badUp.up={};
    check(motion.Update(badUp,samples,{}, {},terrain,1.f/60).status==ThirdPersonCameraMotionStatus3D::InvalidInput,"zero camera up accepted");
    const std::array nearCorner{BoxCollider3D{desired.position+Vec3{.14f,0,0},{0,0,0}}};
    motion.Reset();
    check(!motion.Update(desired,samples,nearCorner,{},terrain,1.f/60).accepted,"projection near-plane corner volume omitted");
    const std::array thinWall{BoxCollider3D{{0,2,0},{.001f,1,1}}};
    check(!ThirdPersonCameraPathClear3D({-2,2,0},{2,2,0},thinWall,terrain),"safe endpoints tunneled through thin wall");
    HeightField3D peak({{0,0,0},5,3,1});peak.SetHeight(2,1,2);
    check(!ThirdPersonCameraPathClear3D({0,1,1},{4,1,1},{},peak),"safe endpoints tunneled through terrain peak");
    HeightField3D invalidGrid({{nan,0,0},2,2,1});
    check(!ThirdPersonCameraPathClear3D({0,2,0},{1,2,1},{},invalidGrid),"invalid grid origin accepted");
    check(!ThirdPersonCameraPathClear3D({-49,100,-49},{49,100,49},{},terrain),"over-budget terrain traversal accepted");
    const std::array occluder{BoxCollider3D{{0,1.5f,-3},{1,2,.1f}}};
    motion.Reset();motion.Update(desired,samples,occluder,{},terrain,1.f/60);
    const auto occluded=motion.Update(desired,samples,occluder,{},terrain,1.f/60);
    check(occluded.accepted&&!occluded.subjectVisible&&occluded.status==ThirdPersonCameraMotionStatus3D::TrackingOccluded,"body occlusion mislabeled as visible");
    auto changedProjection=desired;changedProjection.projection.verticalFieldOfView+=.1f;
    check(motion.Update(changedProjection,samples,{}, {},terrain,1.f/60).status==ThirdPersonCameraMotionStatus3D::InvalidInput,"ordinary tracking changed projection");
    motion.Reset();motion.Update(desired,samples,{}, {},terrain,1.f/60);
    auto previous=motion.Camera();
    CollisionWorld3D world;
    for(unsigned tick=0;tick<720;++tick){
        const float height=tick>=240&&tick<480?1.4f:1.8f;
        const Vec3 feet{float(tick)*.03f,0,0};
        const std::array movingSamples{feet+Vec3{0,.2f,0},feet+Vec3{0,height,0},feet+Vec3{0,height*.5f,0}};
        const auto target=feet+Vec3{0,height<1.5f?.85f:1.2f,0};
        const auto wanted=MakeThirdPersonCamera(target,3.12f+float(tick)*.01f,.3f,6);
        const auto result=motion.Update(wanted,movingSamples,{}, {},terrain,1.f/60);
        if(!result.accepted||result.displacement>.55f||world.SweepSphere(previous.position,.155f,result.camera.position-previous.position)){
            check(false,"moving/crouching subject transition failed");break;
        }
        previous=result.camera;
        if(tick==719)check(true,"moving/crouching sequence passed");
    }
    std::cout<<"Motion contract checks="<<checks<<" failures="<<failures<<'\n';
    return failures?1:0;
}
