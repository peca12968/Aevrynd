#define main ExistingSideControllerMain
#include "ControllerContactCases.h"
#undef main

void RunEarliestExtraCases();
int RunPreservedEarliestCases(){
    try{
        Need(ExistingSideControllerMain()==0,"EXISTING_CANDIDATE_SUITE_FAILED");
        const auto terrain=Terrain();CharacterController3D baseline,subject;
        const Vec3 feet{-1.8f,1.3028f,0};
        const CharacterMotionState3D state{feet,{0,-1,0},1.8f,0,false};
        for(auto* c:{&baseline,&subject}){
            c->Reset(feet);Need(c->TryRestoreMotionState(state,NoBoxes,NoHulls,terrain),"OCCLUDED_RESTORE_FAILED");
        }
        const std::array<BoxCollider3D,1> wall{{{{-1.31f,2,0},{.01f,2,10}}}};
        const std::array<DynamicCylinder3D,1> body{{{{0,.5f,0},1.3f,.8f}}};
        const CharacterControl3D control{{1,0,0},false,4};const float dt=1.f/120.f;
        const float h=dt/2.f;const Vec3 desired{24.f*h,(-1.f-20.f*h)*h,0};
        BoxCylinderContact3D full;
        Need(QueryBoxCylinderContact3D(subject.Center(),subject.Extents(),desired,body[0],full)&&
            full.kind==BoxCylinderContactKind3D::VerticalEntry,"OCCLUDED_EXPECTED_FULL_SEGMENT_VERTICAL_MISSING");
        const double wallFirst=(double(wall[0].center.x)-wall[0].extents.x-subject.Extents().x-feet.x)/desired.x;
        Need(wallFirst>0&&wallFirst<full.first,"OCCLUDED_STATIC_WALL_NOT_EARLIER");
        Need(baseline.Step(control,wall,NoHulls,terrain,std::span<const DynamicCylinder3D>{},dt)==Status::Applied,
            "OCCLUDED_ORIGINAL_STATIC_PATH_FAILED");
        Separated(baseline,body);Need(baseline.Position().y<feet.y,"OCCLUDED_BASELINE_NOT_DESCENDING");
        [[maybe_unused]] const auto before=Snapshot(subject);const auto status=subject.Step(control,wall,NoHulls,terrain,body,dt);
        std::cout<<"OCCLUDED_VERTICAL_EVENT wall_first="<<wallFirst<<" dynamic_first="<<full.first
            <<" baseline_x="<<baseline.Position().x<<" baseline_y="<<baseline.Position().y
            <<" actual_status="<<unsigned(status)<<" actual_x="<<subject.Position().x<<" actual_y="<<subject.Position().y<<'\n';
#if GREEN
        Need(status==Status::Applied,"OCCLUDED_LATER_VERTICAL_STILL_REJECTED");
        const float limit=wall[0].center.x-wall[0].extents.x-subject.Extents().x;
        Need(subject.Position().x<=limit&&subject.Position().x>limit-.001f&&subject.Position().y<feet.y&&
            subject.Position().y<body[0].center.y+body[0].halfHeight&&!subject.Grounded(),"OCCLUDED_SAFE_STOP_DESCENT_OR_SUPPORT_WRONG");
        Separated(subject,body);
        RunEarliestExtraCases();
        std::cout<<"OCCLUDED_LATER_VERTICAL_CONTROLLER_GREEN_CPU_PASS existing_suite=7positive7negative empty_parity=1536\n";
        return 0;
#else
        Need(status==Status::UnresolvedVerticalEntry&&Snapshot(subject)==before,"OCCLUDED_SPECIFIC_REJECTION_RED_NOT_REPRODUCED");
        std::cout<<"OCCLUDED_LATER_VERTICAL_CONTROLLER_RED_REPRODUCED later_unreachable_event_rejected_whole_step=1\n";
        return 1;
#endif
    }catch(const std::exception& e){std::cerr<<"UNEXPECTED_OCCLUDED_FIXTURE_DIVERGENCE "<<e.what()<<'\n';return 2;}
}
