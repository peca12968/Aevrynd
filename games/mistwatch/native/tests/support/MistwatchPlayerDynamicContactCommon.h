#pragma once
// 真 Player／Explorer 診斷；高放圓柱只測支援 API，不聲稱完整世界 NPC 可位於此處。
#include "MistwatchPlayerLocomotion.h"
#include "MistwatchExplorerNapeHairBinding.h"
#include "MistwatchExplorerBackpack.h"
#include <iomanip>
#include <iostream>
#include <stdexcept>

using namespace Aevrynd;
using namespace Mistwatch;
constexpr float H=LocomotionMotor::FixedStep;
const std::span<const BoxCollider3D> NoBoxes{};
const std::span<const ConvexAabbSweep3D* const> NoHulls{};
void Need(bool value,const char* why){if(!value)throw std::runtime_error(why);}
struct Fixture {
    HeightField3D terrain{{{-30,0,-30},128,128,1}};
    CharacterController3D actor;
    Evasion evasion;
    Explorer explorer;
    PlayerLocomotion owner;
    Fixture(){
        terrain.Fill(0);
        auto settings=actor.Settings();settings.maximumTerrainUphillGrade=1;settings.maximumTerrainStandingGrade=1;
        actor=CharacterController3D(settings);actor.Reset({});
        Need(actor.Step({},NoBoxes,NoHulls,terrain,H)&&actor.Grounded(),"ACTUAL_STATIC_TERRAIN_GROUND_NOT_ESTABLISHED");
        ExplorerNapeHairBinding::Apply(explorer);
        const auto bag=BackpackFit1250::Apply(explorer,{+[](void*){},nullptr});
        Need(explorer.model.Valid()&&explorer.model.bindMesh.vertices.size()==9281&&
            explorer.model.bindMesh.indices.size()==37032&&bag.records==9281,"ACTUAL_NAPE_BACKPACK_MODEL_NOT_ESTABLISHED");
        explorer.Update({},0,false);
        unsigned line{};ExplorerFootPlacementDiagnostic diagnostic;
        const bool placed=explorer.PlaceFeet(actor.Position(),true,false,terrain,NoBoxes,NoHulls,&line,&diagnostic);
        std::cout<<"SETUP_POSE accepted="<<placed<<" failure_line="<<line<<" reason="<<unsigned(diagnostic.reason)
            <<" joints="<<explorer.model.skeleton.joints.size()<<" vertices="<<explorer.model.bindMesh.vertices.size()
            <<" indices="<<explorer.model.bindMesh.indices.size()<<" grounded="<<actor.Grounded()<<'\n';
        Need(placed,"ACTUAL_EXPLORER_INITIAL_POSE_REJECTED");
    }
};
void PrintStep(const char* name,unsigned tick,const Fixture& f,const CharacterMotionState3D& before,
    const CharacterControllerStats3D& beforeStats,const Evasion& beforeEvasion,
    const PlayerLocomotionDynamicResult& result,CharacterDynamicContactState3D contact){
    const auto after=f.actor.CaptureMotionState();const auto stats=f.actor.Stats();
    std::cout<<"PLAYER_HEAD_STEP case="<<name<<" tick="<<tick<<" accepted="<<result.motion.accepted
        <<" pose_rejected="<<result.motion.poseRejected<<" pose_failure_line="<<result.motion.poseFailureLine
        <<" pose_reason="<<unsigned(result.motion.footPlacementDiagnostic.reason)<<" failed_substep="<<result.motion.failedSubstep
        <<" steps="<<result.motion.steps<<" dashing="<<result.motion.dashing
        <<" published_contact_valid="<<result.motion.accepted<<" above="<<contact.blockedAbove
        <<" below="<<contact.blockedBelow<<" side="<<contact.blockedSide
        <<" contact_status="<<(result.contact?int(*result.contact):-1)
        <<" before_px="<<before.feet.x<<" before_py="<<before.feet.y<<" before_pz="<<before.feet.z
        <<" px="<<after.feet.x<<" py="<<after.feet.y<<" pz="<<after.feet.z
        <<" before_vy="<<before.velocity.y<<" vy="<<after.velocity.y
        <<" before_remaining="<<beforeEvasion.remaining<<" remaining="<<f.evasion.remaining
        <<" before_cooldown="<<beforeEvasion.cooldown<<" cooldown="<<f.evasion.cooldown
        <<" before_grounded="<<before.grounded<<" grounded="<<after.grounded
        <<" before_jumps="<<beforeStats.jumps<<" jumps="<<stats.jumps
        <<" before_landings="<<beforeStats.landings<<" landings="<<stats.landings
        <<" pose_grounded="<<f.explorer.gaitGrounded<<" pose_initialized="<<f.explorer.feetInitialized<<'\n';
}
