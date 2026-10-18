#include <Aevrynd/ThirdPersonCameraMotion3D.h>
#include <array>
#include <bit>
#include <limits>
#include <cmath>
#include <iostream>
#include <stdexcept>
using namespace Aevrynd;
namespace {
void Need(bool value,const char* message){if(!value)throw std::runtime_error(message);}
auto Samples(Vec3 feet){
    return std::array{feet+Vec3{0,.2f,0},feet+Vec3{0,1.4f,0},feet+Vec3{0,.7f,0},
        feet+Vec3{.22f,1.05f,0},feet+Vec3{-.22f,1.05f,0},feet+Vec3{0,1.05f,.22f},feet+Vec3{0,1.05f,-.22f}};
}

bool Same(float a,float b){return std::bit_cast<std::uint32_t>(a)==std::bit_cast<std::uint32_t>(b);}
bool Same(Vec3 a,Vec3 b){return Same(a.x,b.x)&&Same(a.y,b.y)&&Same(a.z,b.z);}
bool Same(const Camera3D& a,const Camera3D& b){
    return Same(a.position,b.position)&&Same(a.target,b.target)&&Same(a.up,b.up)&&a.projection.mode==b.projection.mode&&
        Same(a.projection.verticalFieldOfView,b.projection.verticalFieldOfView)&&Same(a.projection.orthographicHeight,b.projection.orthographicHeight)&&
        Same(a.projection.nearPlane,b.projection.nearPlane)&&Same(a.projection.farPlane,b.projection.farPlane);
}
bool Same(const ThirdPersonCameraMotionResult3D& a,const ThirdPersonCameraMotionResult3D& b){
    return Same(a.camera,b.camera)&&a.status==b.status&&a.accepted==b.accepted&&a.subjectVisible==b.subjectVisible&&Same(a.displacement,b.displacement);
}
// 所有負控只操作public copy；主109步的state與輸入不受影響。
unsigned Controls(const ThirdPersonCameraMotion3D& held,const Camera3D& wanted,const std::array<Vec3,7>& samples,
    const std::array<BoxCollider3D,4>& boxes,const std::array<BoxCollider3D,1>& roofs,const HeightField3D& terrain,float dt){
    unsigned controls{};
    for(float rejectedDt:std::array{0.f,-1.f,std::numeric_limits<float>::quiet_NaN()}){
        auto probe=held,control=held;const auto before=probe.Camera();
        const auto rejected=probe.Update(wanted,samples,boxes,roofs,terrain,rejectedDt);
        Need(!rejected.accepted&&Same(before,probe.Camera()),"WORLD_HOLD_REJECTED_CALL_CHANGED_CAMERA");
        Need(rejected.status==(rejectedDt==0?ThirdPersonCameraMotionStatus3D::Paused:ThirdPersonCameraMotionStatus3D::InvalidInput),
            "WORLD_HOLD_REJECTED_CALL_STATUS");
        const auto a=probe.Update(wanted,samples,boxes,roofs,terrain,dt);
        const auto b=control.Update(wanted,samples,boxes,roofs,terrain,dt);
        Need(Same(a,b),"WORLD_HOLD_REJECTED_CALL_CHANGED_CONTINUATION");++controls;
    }
    const std::array manual{
        MakeThirdPersonCamera(wanted.target,.15f,.3f,6,wanted.projection),
        MakeThirdPersonCamera(wanted.target,0,.3f,5,wanted.projection)};
    for(const auto& request:manual){
        auto probe=held;ThirdPersonCameraMotion3D fresh;
        const auto cut=fresh.Update(held.Camera(),samples,boxes,roofs,terrain,dt);
        Need(cut.accepted&&cut.status==ThirdPersonCameraMotionStatus3D::ResetCut,"WORLD_HOLD_MANUAL_CONTROL_CUT_REJECTED");
        const auto expected=fresh.Update(request,samples,boxes,roofs,terrain,dt);
        const auto actual=probe.Update(request,samples,boxes,roofs,terrain,dt);
        Need(actual.accepted&&Same(actual,expected),"WORLD_HOLD_SWALLOWED_MANUAL_YAW_OR_ZOOM");++controls;
    }
    auto reset=held;reset.Reset();
    Need(!reset.Initialized(),"WORLD_HOLD_RESET_RETAINED_INITIALIZATION");
    const auto paused=reset.Update(wanted,samples,boxes,roofs,terrain,0);
    Need(!paused.accepted&&!reset.Initialized()&&paused.status==ThirdPersonCameraMotionStatus3D::Paused,
        "WORLD_HOLD_PAUSED_RESET_INITIALIZED");
    const auto cut=reset.Update(held.Camera(),samples,boxes,roofs,terrain,dt);
    Need(cut.accepted&&cut.status==ThirdPersonCameraMotionStatus3D::ResetCut&&Same(cut.camera,held.Camera()),
        "WORLD_HOLD_RESET_DID_NOT_REESTABLISH_EXPLICIT_CUT");++controls;
    return controls;
}
}
int main(){try{
    constexpr float dt=1.f/60.f;
    HeightField3D terrain({{-50,0,-20},201,241,.5f});
    // 三面牆與 1.45 公尺低頂棚，東側開口；不用任何遊戲任務或原始碼內部狀態。
    const std::array boxes{BoxCollider3D{{0,1.55f,18},{2.5f,.1f,2}},
        BoxCollider3D{{-2.4f,.7f,18},{.1f,.7f,2}},
        BoxCollider3D{{0,.7f,16.1f},{2.5f,.7f,.1f}},
        BoxCollider3D{{0,.7f,19.9f},{2.5f,.7f,.1f}}};
    const std::array roofs{boxes[0]};
    CollisionWorld3D collision;
    for(const auto& box:boxes)collision.CreateBox({box});
    ThirdPersonCameraMotion3D motion;
    Vec3 feet{3.2f,0,17.818428f};
    Need(motion.Update(MakeThirdPersonCamera(feet+Vec3{0,.85f,0},0,.3f,6),Samples(feet),boxes,roofs,terrain,dt).accepted,
         "initial east-entry camera rejected");
    unsigned hidden{},held{},visibleTail{},controls{};
    for(unsigned tick=0;tick<109;++tick){
        if(tick>=60)feet.x=3.2f-(float(tick-59)/49.f)*2.44751091f;
        const auto before=motion.Camera();
        const auto wanted=MakeThirdPersonCamera(feet+Vec3{0,.85f,0},0,.3f,6);
        const auto samples=Samples(feet);
        const auto result=motion.Update(wanted,samples,boxes,roofs,terrain,dt);
        Need(result.accepted,"moving low-roof update rejected");
        Need(result.displacement<=30.f*dt+Length(result.camera.target-before.target)+.00002f,"low-roof displacement exceeded original rate");
        Need(!collision.SweepSphere(before.position,.16f,result.camera.position-before.position),"low-roof camera volume crossed wall");
        Need(ThirdPersonCameraPathClear3D(before.position,result.camera.position,boxes,terrain,{},.16f),"low-roof whole-volume path rejected");
        Need(Length(result.camera.position-result.camera.target)>=1.5f,"low-roof subject range too short");
        for(const auto sample:samples)Need(ProjectToViewport(sample,result.camera,{1280,720}).visible,"low-roof subject left viewport");
        if(tick==70){
            std::cout<<"WORLD_ANCHOR_TICK70 accepted="<<result.accepted<<" status="<<unsigned(result.status)
                <<" visible="<<result.subjectVisible<<" previous_world_position="<<Same(before.position,result.camera.position)<<'\n';
            Need(result.status==ThirdPersonCameraMotionStatus3D::Held&&result.subjectVisible&&Same(before.position,result.camera.position),
                "WORLD_ANCHOR_TICK70_NOT_VISIBLE_HELD_AT_PREVIOUS_WORLD_POSITION");
            controls=Controls(motion,wanted,samples,boxes,roofs,terrain,dt);
        }
        hidden+=!result.subjectVisible;held+=result.status==ThirdPersonCameraMotionStatus3D::Held;
        if(tick>=100)visibleTail+=result.subjectVisible;
        if(tick==108){
            std::cout<<"LOW_ROOF_RESULT visible="<<result.subjectVisible<<" camera="<<result.camera.position.x<<','
                <<result.camera.position.y<<','<<result.camera.position.z<<" hidden="<<hidden<<" held="<<held
                <<" visible_tail="<<visibleTail<<'\n';
            Need(result.subjectVisible,"moving subject still hidden inside east-entry low roof");
            Need(visibleTail==9,"visible recovery was lost again while moving toward the low-roof goal");
        }
    }
    std::cout<<"LOW_ROOF_PASS steps=109 moving=49 global_input=0 gpu=0\n";
    std::cout<<"WORLD_ANCHOR_SUMMARY hidden="<<hidden<<" held="<<held<<" visible_tail="<<visibleTail<<" controls="<<controls<<'\n';
    Need(hidden<=12,"WORLD_ANCHOR_HIDDEN_NOT_REDUCED_BY_HALF");
    Need(controls==6,"WORLD_ANCHOR_CONTROLS_INCOMPLETE");
    std::cout<<"WORLD_ANCHOR_CAMERA_PASS steps=109 first_edge=70 hidden="<<hidden<<" maximum_hidden=12 visible_tail="<<visibleTail
        <<" controls="<<controls<<" candidate_world=0 gpu=0\n";return 0;
}catch(const std::exception& error){std::cerr<<"FIRST_DIVERGENCE "<<error.what()<<'\n';return 1;}}
