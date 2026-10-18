#include <Aevrynd/CharacterController3D.h>
#include <CharacterDynamicContact3D.h>
#include <array>
#include <bit>
#include <iostream>
#include <stdexcept>

namespace {
using namespace Aevrynd;
void Need(bool value,const char* reason){if(!value)throw std::runtime_error(reason);}
std::array<std::uint64_t,18> State(const CharacterController3D& c){
    const auto m=c.CaptureMotionState();const auto s=c.Stats();
    const auto bits=[](float value){return std::uint64_t(std::bit_cast<std::uint32_t>(value));};
    return {bits(m.feet.x),bits(m.feet.y),bits(m.feet.z),bits(m.velocity.x),bits(m.velocity.y),bits(m.velocity.z),
        bits(m.height),bits(m.yaw),m.grounded,s.steps,s.collisions,s.jumps,s.landings,s.terrainContacts,
        s.directTicks,s.directBlocked,s.directOverlaps,c.StateHash()};
}
CharacterController3D Ready(const HeightField3D& terrain){
    CharacterControllerSettings3D settings;settings.maximumTerrainUphillGrade=.5f;settings.maximumTerrainStandingGrade=.5f;
    CharacterController3D c(settings);const Vec3 feet{-.48f,0,0};c.Reset(feet);
    Need(c.TryRestoreMotionState({feet,{},settings.height,0,true},{},{},terrain),"FIXTURE_GROUNDED_RESTORE_FAILED");
    return c;
}
bool StaticStep(CharacterController3D& c,std::span<const BoxCollider3D> boxes,const HeightField3D& terrain){
#if CANDIDATE
    return c.Step({{1,0,0}},boxes,{},terrain,std::span<const DynamicCylinder3D>{},1.f/120.f)==CharacterDynamicStepStatus3D::Applied;
#else
    return c.Step({{1,0,0}},boxes,{},terrain,1.f/120.f);
#endif
}
}
int main(){
    try{
        HeightField3D terrain({{-16,0,-16},33,33,1});terrain.Fill(0);
        const std::array<BoxCollider3D,2> low{{{{0,-.5f,0},{16,.5f,16}},{{.5f,.05f,0},{.5f,.05f,2}}}};
        const std::array<BoxCollider3D,2> tall{{low[0],{{.5f,1.5f,0},{.5f,1.5f,2}}}};
        const DynamicCylinder3D body{{-.48f,2.7f,0},1.3f,.8f};
        auto empty=Ready(terrain),wall=Ready(terrain);
        Need(StaticStep(empty,low,terrain),"EMPTY_DYNAMIC_LOW_STEP_BASELINE_FAILED");
        Need(StaticStep(wall,tall,terrain),"EMPTY_DYNAMIC_SAFE_WALL_STOP_FAILED");
        Need(wall.Position().x<=-.45f&&wall.Position().x>-.451f&&wall.Position().y==0&&wall.Grounded(),
            "STATIC_SAFE_WALL_STOP_PRECONDITION_FAILED");
        auto subject=Ready(terrain);BoxCylinderContact3D mainQuery,optionalUp;
        Need(QueryBoxCylinderContact3D(subject.Center(),subject.Extents(),{.05f,-.01f,0},body,mainQuery)&&
            mainQuery.kind==BoxCylinderContactKind3D::Clear,"ACTUAL_DOWNWARD_MAIN_SWEEP_NOT_CLEAR");
        Need(QueryBoxCylinderContact3D(subject.Center(),subject.Extents(),{0,.20f,0},body,optionalUp)&&
            optionalUp.kind==BoxCylinderContactKind3D::VerticalEntry,"OPTIONAL_STEP_UP_NOT_VERTICAL_ENTRY");
        std::cout<<"PRECONDITIONS main_clear=1 optional_up_vertical=1 optional_up_first="<<optionalUp.first
            <<" empty_low_x="<<empty.Position().x<<" empty_low_y="<<empty.Position().y
            <<" safe_wall_x="<<wall.Position().x<<" safe_wall_y="<<wall.Position().y<<'\n';
#if CANDIDATE
        const std::array<DynamicCylinder3D,1> bodies{body};
        auto safeWall=Ready(terrain);
        Need(safeWall.Step({{1,0,0}},tall,{},terrain,bodies,1.f/120.f)==CharacterDynamicStepStatus3D::Applied,
            "NPC_BLOCKED_NORMAL_STATIC_WALL_STOP");
        Need(safeWall.Position().x<=-.45f&&safeWall.Position().x>-.451f&&safeWall.Position().y==0&&safeWall.Grounded(),
            "NPC_SAFE_WALL_STOP_NOT_GROUNDED");
        CharacterDirectDisplacementPreviewToken3D token;
        Need(subject.PreviewDirectDisplacementTick({1,{},0},{},token),"PREVIEW_SETUP_FAILED");
        const auto before=State(subject);
        const auto result=subject.Step({{1,0,0}},low,{},terrain,bodies,1.f/120.f);
        std::cout<<"OPTIONAL_STEP_RESULT status="<<unsigned(result)<<" x="<<subject.Position().x<<" y="<<subject.Position().y
            <<" whole_state_unchanged="<<(State(subject)==before)
            <<" pending_preview_preserved="<<subject.CanCommitDirectDisplacementPreview(token)<<'\n';
        if(result==CharacterDynamicStepStatus3D::UnresolvedVerticalEntry){
            Need(State(subject)==before&&subject.CanCommitDirectDisplacementPreview(token),"REJECTION_NOT_ATOMIC");
            std::cout<<"OPTIONAL_STEP_TRIAL_FALSE_WHOLE_TRANSACTION_REJECTION_RED_REPRODUCED\n";
            return 1;
        }
        Need(result==CharacterDynamicStepStatus3D::Applied,"OPTIONAL_STEP_DIFFERENT_UNRESOLVED_FAILURE");
        Need(subject.Position().x<=-.45f&&subject.Position().x>-.451f&&subject.Position().y==0&&subject.Grounded(),
            "OPTIONAL_STEP_MUST_BE_SKIPPED_NOT_COMMITTED");
        std::cout<<"OPTIONAL_STEP_TRIAL_SKIPPED_WITH_SAFE_MAIN_MOVEMENT\n";
        return 0;
#else
        std::cout<<"ORIGINAL_F_STATIC_BASELINES_AND_QUERY_PRECONDITIONS_PASS\n";
        return 0;
#endif
    }catch(const std::exception& e){std::cerr<<"FIXTURE_DIVERGENCE "<<e.what()<<'\n';return 2;}
}
