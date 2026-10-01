#include "MistwatchBoar.h"
#include "MistwatchWorld.h"
#include <Aevrynd/SkinnedModelAsset.h>
#include <iostream>
#include <limits>
int main() {
    using namespace Aevrynd;
    Mistwatch::Boar boar;
    if(!boar.model.Valid()||boar.model.skeleton.joints.size()!=16)return 1;
    const auto bounds=boar.model.bindMesh.Bounds();
    if(bounds.extents.z<.9f||bounds.extents.z>1.2f||bounds.extents.x<.38f||bounds.extents.y>.65f)return 2;
    if(boar.model.bindMesh.vertices.size()>8000||boar.model.bindMesh.indices.size()/3>13000)return 3;
    std::vector<std::byte> encoded;
    if(!EncodeSkinnedModelAsset(boar.model,encoded))return 4;
    const auto loaded=LoadSkinnedModelAsset(encoded);
    if(!loaded||loaded.model.bindMesh.indices!=boar.model.bindMesh.indices||
        loaded.model.influences.size()!=boar.model.influences.size())return 5;
    for(std::size_t i=0;i<boar.model.bindMesh.vertices.size();++i) {
        const auto& a=boar.model.bindMesh.vertices[i];const auto& b=loaded.model.bindMesh.vertices[i];
        if(a.x!=b.x||a.y!=b.y||a.z!=b.z||a.normal!=b.normal||a.color!=b.color)return 6;
    }
    const auto terrain=Mistwatch::CreateTerrain();
    Vec3 feet{0,0,48};
    std::array<Vec3,4> previousTargets{};
    std::array<bool,4> previousPlanted{};
    SkinningWorkspace3D workspace;Mesh3D posed;
    unsigned locked{},lifted{};float maximumError{},maximumSlip{};
    for(unsigned tick=0;tick<360;++tick) {
        const Vec3 movement=tick<240?Vec3{0,0,.012f}:Vec3{};
        feet=feet+movement;
        if(!boar.Update(movement,1.f/60.f,Mistwatch::EnemyPhase::Pursuit,0,{0,0,1})||!boar.PlaceFeet(feet,terrain))return 7;
        const auto world=CreateTransformMatrix(feet,{0,boar.yaw,0},{1,1,1});
        for(unsigned leg=0;leg<4;++leg) {
            const auto ankle=TransformPoint(world*boar.pose.globalMatrices[5+leg*3],{});
            maximumError=(std::max)(maximumError,Length(ankle-boar.footTargets[leg]));
            if(boar.planted[leg]&&previousPlanted[leg]&&Length(boar.footTargets[leg]-previousTargets[leg])<.001f) {
                ++locked;maximumSlip=(std::max)(maximumSlip,Length(ankle-previousTargets[leg]));
            }
            if(!boar.planted[leg]&&ankle.y>.12f)++lifted;
            previousTargets[leg]=boar.footTargets[leg];previousPlanted[leg]=boar.planted[leg];
        }
        if(!SkinModelInto(boar.model,boar.pose,workspace,posed))return 8;
        const auto posedBounds=posed.Bounds();
        if(posedBounds.center.y-posedBounds.extents.y<-.02f||posedBounds.center.y+posedBounds.extents.y>1.3f)return 9;
    }
    if(locked<200||lifted<100||maximumError>.003f||maximumSlip>.003f) {
        std::cerr<<"Boar foot divergence: locked="<<locked<<" lifted="<<lifted<<" error="<<maximumError<<" slip="<<maximumSlip<<'\n';return 10;
    }
    for(float yaw:{0.f,1.5707963f,3.14159265f,-1.5707963f}) {
        boar={};feet={12,terrain.SampleHeight(12,48),48};
        const Vec3 look{std::sin(yaw),0,std::cos(yaw)};
        if(!boar.Update({},.02f,Mistwatch::EnemyPhase::Windup,.1f,look)||!boar.PlaceFeet(feet,terrain))return 11;
        const auto world=CreateTransformMatrix(feet,{0,boar.yaw,0},{1,1,1});
        for(unsigned leg=0;leg<4;++leg) {
            const auto ankle=TransformPoint(world*boar.pose.globalMatrices[5+leg*3],{});
            if(Length(ankle-boar.footTargets[leg])>.005f)return 12;
        }
    }
    boar={};
    if(!boar.Update({},.02f,Mistwatch::EnemyPhase::Windup,.05f,{0,0,1}))return 13;
    const float windupPitch=boar.pose.localTransforms[1].rotation.x;
    if(boar.brace<.9f||!boar.Update({},.02f,Mistwatch::EnemyPhase::Recovery,1,{0,0,1})||
        boar.pose.localTransforms[1].rotation.x>=windupPitch)return 14;
    const auto oldYaw=boar.yaw;
    if(boar.Update({},std::numeric_limits<float>::quiet_NaN(),Mistwatch::EnemyPhase::Pursuit,0,{0,0,1})||boar.yaw!=oldYaw)return 15;
    if(boar.Update({},.02f,static_cast<Mistwatch::EnemyPhase>(99),0,{0,0,1}))return 16;
    Mistwatch::Encounter enemy;
    boar={};unsigned pursuitFrames{},windupFrames{},recoveryFrames{};
    for(unsigned tick=0;tick<600;++tick) {
        const auto before=enemy.feet;
        const Vec3 player{0,0,43};
        enemy.Tick(player,1.f/60.f,false);
        if(!boar.Update(enemy.feet-before,1.f/60.f,enemy.phase,(std::max)(0.f,enemy.phaseTime),
            (enemy.phase==Mistwatch::EnemyPhase::Windup?enemy.strikeTarget:player)-enemy.feet)||!boar.PlaceFeet(enemy.feet,terrain)) {
            std::cerr<<"Live encounter pose failed tick="<<tick<<" phase="<<unsigned(enemy.phase)<<" feet z="<<enemy.feet.z<<'\n';return 17;
        }
        if(enemy.phase==Mistwatch::EnemyPhase::Pursuit)++pursuitFrames;
        if(enemy.phase==Mistwatch::EnemyPhase::Windup)++windupFrames;
        if(enemy.phase==Mistwatch::EnemyPhase::Recovery)++recoveryFrames;
        if(!SkinModelInto(boar.model,boar.pose,workspace,posed))return 18;
    }
    if(pursuitFrames<50||windupFrames<100||recoveryFrames<100) {
        std::cerr<<"Phase coverage: pursuit="<<pursuitFrames<<" windup="<<windupFrames<<" recovery="<<recoveryFrames<<'\n';return 19;
    }
    std::cout<<"Boar PASS; joints=16; vertices="<<boar.model.bindMesh.vertices.size()<<" triangles="<<boar.model.bindMesh.indices.size()/3
        <<" locked="<<locked<<" lifted="<<lifted<<" maxFootError="<<maximumError<<" maxSlip="<<maximumSlip<<'\n';
}
