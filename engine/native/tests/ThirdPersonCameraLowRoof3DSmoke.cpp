#include <Aevrynd/ThirdPersonCameraMotion3D.h>
#include <array>
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
    unsigned hidden{},held{},visibleTail{};
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
    std::cout<<"LOW_ROOF_PASS steps=109 moving=49 global_input=0 gpu=0\n";return 0;
}catch(const std::exception& error){std::cerr<<"FIRST_DIVERGENCE "<<error.what()<<'\n';return 1;}}
