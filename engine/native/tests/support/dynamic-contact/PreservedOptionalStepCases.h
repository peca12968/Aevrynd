#include "../../../src/CharacterController3D.cpp"
#define main RunPreservedOptionalStepRegression
#include "OptionalStepTrial.h"
#undef main
#include <limits>

int RunPreservedEarliestCases();
namespace ReviewOptional {
using namespace Aevrynd;
using Kind=BoxCylinderContactKind3D;
using Status=CharacterDynamicStepStatus3D;
unsigned helperKinds{},phaseCases{},orderedNumericFailures{},atomicMainFailures{};
void Require(bool value,const char* reason){if(!value)throw std::runtime_error(reason);}
CharacterController3D Grounded(const HeightField3D& terrain,float x){
    CharacterControllerSettings3D settings;settings.maximumTerrainUphillGrade=.5f;settings.maximumTerrainStandingGrade=.5f;
    CharacterController3D c(settings);const Vec3 feet{x,0,0};c.Reset(feet);
    Require(c.TryRestoreMotionState({feet,{},settings.height,0,true},{},{},terrain),"OPTIONAL_GROUNDED_RESTORE_FAILED");return c;
}
void Probe(Vec3 center,Vec3 extents,Vec3 delta,const DynamicCylinder3D& body,Kind expected){
    BoxCylinderContact3D contact;
    Require(QueryBoxCylinderContact3D(center,extents,delta,body,contact)&&contact.kind==expected,"OPTIONAL_HELPER_QUERY_KIND_WRONG");
    Status status=Status::Applied;bool blocked=false;const std::array<DynamicCylinder3D,1> bodies{body};
    Require(DynamicOptionalStepClear(center,extents,delta,bodies,&status,blocked)&&
        blocked==(expected!=Kind::Clear)&&status==Status::Applied,"OPTIONAL_VALID_CONTACT_LEAKED_FAILURE_STATUS");
    ++helperKinds;
}
void AllKindsAndFailures(){
    const DynamicCylinder3D body{{0,1,0},1,1};
    Probe({-4,.9f,0},{.45f,.9f,.45f},{-1,0,0},body,Kind::Clear);
    Probe({-1.65f,.9f,0},{.45f,.9f,.45f},{1,0,0},body,Kind::SideEntry);
    Probe({0,3,0},{.45f,.9f,.45f},{0,-1,0},body,Kind::VerticalEntry);
    Probe({-1.5f,2,0},{.5f,.5f,.5f},{1,-1,0},{{0,.5f,0},.5f,.5f},Kind::SimultaneousEntry);
    Probe({0,1,0},{.5f,.5f,.5f},{},body,Kind::StartedOverlapping);
    const DynamicCylinder3D blocked{{0,.5f,0},.5f,.5f};
    const float nan=std::numeric_limits<float>::quiet_NaN();
    for(const auto bad:{DynamicCylinder3D{{1.e9f,1,0},.0001f,.5f},DynamicCylinder3D{{10,1,0},nan,.5f}}){
        for(const bool reverse:{false,true}){
            const std::array<DynamicCylinder3D,2> bodies=reverse?std::array<DynamicCylinder3D,2>{bad,blocked}:
                std::array<DynamicCylinder3D,2>{blocked,bad};
            Status status=Status::Applied;bool isBlocked=false;
            Require(!DynamicOptionalStepClear({-1.5f,2,0},{.5f,.5f,.5f},{1,-1,0},bodies,&status,isBlocked)&&
                status==Status::UnresolvedNumerics,"OPTIONAL_BLOCKED_SOURCE_SWALLOWED_NUMERICAL_FAILURE");
            ++orderedNumericFailures;
        }
    }
}
void Phase(const HeightField3D& terrain,const DynamicCylinder3D& body,float startX,std::array<Kind,3> expected,const char* name){
    const std::array<BoxCollider3D,2> boxes{{{{0,-.5f,0},{16,.5f,16}},{{.5f,.05f,0},{.5f,.05f,2}}}};
    const Vec3 extents{.45f,.9f,.45f},center{startX,.9f,0},up{0,.20f,0},forward{.025f,0,0};
    const std::array<Vec3,3> starts{center,center+up,center+up+forward};
    const std::array<Vec3,3> deltas{up,forward,Vec3{0,-.0999f,0}};
    for(unsigned i=0;i<3;++i){
        BoxCylinderContact3D q;
        if(!QueryBoxCylinderContact3D(starts[i],extents,deltas[i],body,q)||q.kind!=expected[i]){
            std::cerr<<name<<" segment="<<i<<" actual="<<unsigned(q.kind)<<" expected="<<unsigned(expected[i])<<'\n';
            throw std::runtime_error("OPTIONAL_PHASE_PRECONDITION_WRONG");
        }
    }
    const DynamicCylinder3D distantBody{{10,1,10},.5f,.5f};
    for(const bool reverse:{false,true}){
        const std::array<DynamicCylinder3D,2> bodies=reverse?std::array<DynamicCylinder3D,2>{body,distantBody}:
            std::array<DynamicCylinder3D,2>{distantBody,body};
        auto c=Grounded(terrain,startX);CharacterDirectDisplacementPreviewToken3D token;
        Require(c.PreviewDirectDisplacementTick({1,{},0},{},token),"OPTIONAL_PHASE_PENDING_PREVIEW_FAILED");
        const auto before=State(c);const auto oldSteps=c.Stats().steps;
        const auto result=c.Step({{1,0,0}},boxes,{},terrain,bodies,1.f/120.f);
        std::cout<<"OPTIONAL_PHASE name="<<name<<" reverse="<<reverse<<" status="<<unsigned(result)
            <<" x="<<c.Position().x<<" y="<<c.Position().y<<'\n';
        Require(result==Status::Applied&&c.Position().x<=-.45f&&c.Position().x>-.451f&&c.Position().y==0&&c.Grounded(),
            "OPTIONAL_BLOCKED_ROUTE_NOT_SKIPPED_WITH_SAFE_STOP");
        Require(State(c)!=before&&c.Stats().steps==oldSteps+1&&!c.CanCommitDirectDisplacementPreview(token),
            "OPTIONAL_APPLIED_STEP_DID_NOT_COMMIT_OR_INVALIDATE_PREVIEW");
        ++phaseCases;
    }
}
void Atomic(CharacterController3D& c,CharacterControl3D control,const HeightField3D& terrain,
    std::span<const DynamicCylinder3D> bodies,Status expected){
    CharacterDirectDisplacementPreviewToken3D token;
    Require(c.PreviewDirectDisplacementTick({1,{},0},{},token),"MAIN_NEGATIVE_PENDING_PREVIEW_FAILED");
    const auto before=State(c);const auto memory=c.MemoryStats().retainedCapacityBytes;
    const auto actual=c.Step(control,{}, {},terrain,bodies,.1f);
    if(actual!=expected)std::cerr<<"mandatory status="<<unsigned(actual)<<" expected="<<unsigned(expected)<<'\n';
    Require(actual==expected&&State(c)==before&&c.CanCommitDirectDisplacementPreview(token)&&
        c.MemoryStats().retainedCapacityBytes==memory,"MANDATORY_UNRESOLVED_NOT_ATOMIC");
    ++atomicMainFailures;
}
void Mandatory(const HeightField3D& terrain){
    const std::array<DynamicCylinder3D,1> vertical{{{{0,1,0},1,1}}};
    CharacterController3D falling;falling.Reset({0,2.1f,0});
    Atomic(falling,{},terrain,vertical,Status::UnresolvedVerticalEntry);
    CharacterControllerSettings3D settings;settings.radius=.5f;settings.height=1;settings.moveSpeed=10;settings.gravity=0;
    CharacterController3D simultaneous(settings);const Vec3 feet{-1.5f,1.5f,0};simultaneous.Reset(feet);
    Require(simultaneous.TryRestoreMotionState({feet,{0,-10,0},1,0,false},{},{},terrain),"MAIN_SIMULTANEOUS_RESTORE_FAILED");
    const std::array<DynamicCylinder3D,1> small{{{{0,.5f,0},.5f,.5f}}};
    Atomic(simultaneous,{{1,0,0}},terrain,small,Status::UnresolvedSimultaneousEntry);
    auto numerical=Grounded(terrain,-3);
    const std::array<DynamicCylinder3D,1> extreme{{{{1.e9f,1,0},.0001f,.5f}}};
    Atomic(numerical,{{1,0,0}},terrain,extreme,Status::UnresolvedNumerics);
    auto invalid=Grounded(terrain,-3);auto malformed=extreme;malformed[0].radius=0;
    Atomic(invalid,{{1,0,0}},terrain,malformed,Status::InvalidInput);
}
}
int RunPreservedOptionalStepCandidate(){
    try{
        ReviewOptional::Require(RunPreservedEarliestCases()==0,"PRESERVED_EARLIEST_11_11_PARITY_FAILED");
        ReviewOptional::Require(RunPreservedOptionalStepRegression()==0,"ORIGINAL_OPTIONAL_RED_FIXTURE_NOT_GREEN");
        HeightField3D terrain({{-16,0,-16},33,33,1});terrain.Fill(0);
        using Kind=BoxCylinderContactKind3D;
        ReviewOptional::AllKindsAndFailures();
        ReviewOptional::Phase(terrain,{{-.48f,2.7f,0},1.3f,.8f},-.48f,
            {Kind::VerticalEntry,Kind::StartedOverlapping,Kind::StartedOverlapping},"up");
        ReviewOptional::Phase(terrain,{{.105f,2,0},.1f,.1f},-.46f,
            {Kind::Clear,Kind::SideEntry,Kind::StartedOverlapping},"forward");
        ReviewOptional::Phase(terrain,{{.105f,.10f,0},.1f,.05f},-.46f,
            {Kind::Clear,Kind::Clear,Kind::VerticalEntry},"down");
        ReviewOptional::Mandatory(terrain);
        ReviewOptional::Require(ReviewOptional::helperKinds==5&&ReviewOptional::phaseCases==6&&
            ReviewOptional::orderedNumericFailures==4&&ReviewOptional::atomicMainFailures==4,"OPTIONAL_COVERAGE_COUNT_WRONG");
        std::cout<<"OPTIONAL_STEP_GREEN_CPU_PASS preserved_earliest_positive=11 preserved_earliest_negative=11 empty_parity=1536"
            <<" original_new_red_to_green=1 valid_helper_kinds=5 optional_phase_order_cases=6"
            <<" ordered_query_failure_cases=4 mandatory_atomic_negative_cases=4 pending_preview_controls=10\n";
        return 0;
    }catch(const std::exception& error){std::cerr<<"OPTIONAL_GREEN_DIVERGENCE "<<error.what()<<'\n';return 2;}
}
