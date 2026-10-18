#include "MistwatchBoar.h"
#include "MistwatchWorld.h"
#include <Aevrynd/SkinnedModelAsset.h>
#include <iostream>
#include <limits>
namespace {
bool LoadedBoarArticulation(const Aevrynd::SkinnedModel3D& model,const Aevrynd::HeightField3D& terrain) {
    using namespace Aevrynd;
    constexpr std::array<unsigned,3> channels{1,2,15};
    struct PhaseCase {Mistwatch::EnemyPhase phase;float remaining;};
    constexpr std::array<PhaseCase,5> phases{{
        {Mistwatch::EnemyPhase::Pursuit,0.f},{Mistwatch::EnemyPhase::Windup,.85f},
        {Mistwatch::EnemyPhase::Windup,.425f},{Mistwatch::EnemyPhase::Windup,.05f},
        {Mistwatch::EnemyPhase::Recovery,1.f}}};
    const auto sameMatrix=[](const Matrix4& a,const Matrix4& b) {
        for(unsigned r=0;r<4;++r)for(unsigned c=0;c<4;++c)
            if(!std::isfinite(a[r][c])||!std::isfinite(b[r][c])||std::abs(a[r][c]-b[r][c])>1e-5f)return false;
        return true;
    };
    if(!model.Valid()||model.skeleton.joints.size()!=16)return false;
    // 這是目前本作 AESK1 的 identity bind 契約，不假稱支援任意第三方旋轉骨架。
    for(const auto& joint:model.skeleton.joints) {
        const auto& bind=joint.bindTransform;
        if(!bind.usesQuaternion||bind.quaternion.x!=0||bind.quaternion.y!=0||
            bind.quaternion.z!=0||bind.quaternion.w!=1)return false;
    }
    std::vector<std::byte> immutableModel;
    if(!EncodeSkinnedModelAsset(model,immutableModel))return false;
    const Vec3 feet{12,terrain.SampleHeight(12,48),48};
    unsigned poses{},weightedChecks{};
    for(const auto& phase:phases)for(float beforeCycle:{0.f,1.f,4.f}) {
        Mistwatch::Boar runtime;runtime.model=model;runtime.cycle=beforeCycle;
        if(!runtime.Update({0,0,.012f},1.f/60.f,phase.phase,phase.remaining,{0,0,1})||
            !runtime.PlaceFeet(feet,terrain))return false;
        // 預期旋轉由固定輸入獨立推導；不可把正在受測的 Euler 欄位當成期望值。
        const float brace=phase.phase==Mistwatch::EnemyPhase::Windup?
            std::clamp(1.f-phase.remaining/.85f,0.f,1.f):0.f;
        const float cycle=std::fmod(beforeCycle+.012f*6.f,6.2831853f);
        const std::array<Vec3,3> angles{{
            {phase.phase==Mistwatch::EnemyPhase::Recovery?-.13f:.12f+.36f*brace,0,0},
            {-.1f*brace,0,0},{0,0,.15f*std::sin(cycle)}}};
        std::array<Matrix4,3> expectedGlobal{};
        for(unsigned c=0;c<3;++c) {
            const auto& bind=model.skeleton.joints[channels[c]].bindTransform;
            const auto local=CreateTransformMatrix(bind.translation,angles[c],bind.scale);
            expectedGlobal[c]=(c==1?expectedGlobal[0]:runtime.pose.globalMatrices[0])*local;
            if(!sameMatrix(runtime.pose.localTransforms[channels[c]].Matrix(),local)||
                !sameMatrix(runtime.pose.globalMatrices[channels[c]],expectedGlobal[c]))return false;
        }
        const auto world=CreateTransformMatrix(feet,{0,runtime.yaw,0},{1,1,1});
        for(unsigned leg=0;leg<4;++leg)
            if(Length(TransformPoint(world*runtime.pose.globalMatrices[5+leg*3],{})-runtime.footTargets[leg])>.005f)return false;
        SkinningWorkspace3D workspace;Mesh3D skinned;
        if(!SkinModelInto(runtime.model,runtime.pose,workspace,skinned))return false;
        std::array<unsigned,3> weighted{};
        for(std::size_t v=0;v<model.bindMesh.vertices.size();++v) {
            const auto& source=model.bindMesh.vertices[v];const auto& influence=model.influences[v];
            Vec3 expected{};bool affected{};
            for(unsigned slot=0;slot<4;++slot)if(influence.weights[slot]) {
                const unsigned joint=influence.joints[slot];auto global=runtime.pose.globalMatrices[joint];
                for(unsigned c=0;c<3;++c)if(joint==channels[c]){global=expectedGlobal[c];++weighted[c];affected=true;}
                expected=expected+TransformPoint(global*model.inverseBindMatrices[joint],{source.x,source.y,source.z})*
                    (float(influence.weights[slot])/65535.f);
            }
            const auto& actual=skinned.vertices[v];const float error=Length(Vec3{actual.x,actual.y,actual.z}-expected);
            if(!std::isfinite(error)||error>1e-5f)return false;
            if(affected)++weightedChecks;
        }
        for(unsigned count:weighted)if(!count)return false;
        std::vector<std::byte> after;
        if(!EncodeSkinnedModelAsset(runtime.model,after)||after!=immutableModel)return false;
        ++poses;
    }
    std::cout<<"BOAR_AESK1_ROUNDTRIP_ARTICULATION_CPU_PASS poses="<<poses
        <<" channels=3 local_matrices="<<poses*3<<" global_matrices="<<poses*3
        <<" weighted_vertex_checks="<<weightedChecks<<'\n';
    return poses==15;
}
}
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
    if(!LoadedBoarArticulation(loaded.model,terrain)) {std::cerr<<"Loaded boar articulation divergence\n";return 20;}
    std::cout<<"Boar PASS; joints=16; vertices="<<boar.model.bindMesh.vertices.size()<<" triangles="<<boar.model.bindMesh.indices.size()/3
        <<" locked="<<locked<<" lifted="<<lifted<<" maxFootError="<<maximumError<<" maxSlip="<<maximumSlip<<'\n';
}
