#include <Aevrynd/ThirdPersonCameraMotion3D.h>
#include <array>
#include <bit>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace Aevrynd;
void Need(bool v,const char* why){if(!v)throw std::runtime_error(why);}
bool Same(float a,float b){return std::bit_cast<unsigned>(a)==std::bit_cast<unsigned>(b);}
bool Same(Vec3 a,Vec3 b){return Same(a.x,b.x)&&Same(a.y,b.y)&&Same(a.z,b.z);}
bool Same(const Camera3D& a,const Camera3D& b){return Same(a.position,b.position)&&Same(a.target,b.target)&&Same(a.up,b.up);}
int main(){try{
    const float dt=1.f/60;
    HeightField3D terrain({{-50,0,-50},101,101,1});
    const std::array samples{Vec3{0,.2f,0},Vec3{0,1.8f,0},Vec3{0,.9f,0},
        Vec3{.22f,1.35f,0},Vec3{-.22f,1.35f,0},Vec3{0,1.35f,.22f},Vec3{0,1.35f,-.22f}};
    const std::array solids{BoxCollider3D{{0,1.5f,-3},{1,2,.1f}}};
    const auto desired=MakeThirdPersonCamera({0,1.2f,0},0,.3f,6);
    ThirdPersonCameraMotion3D control,noisy;
    for(auto* camera:{&control,&noisy}){
        const auto cut=camera->Update(desired,samples,solids,{},terrain,dt);
        Need(cut.accepted&&cut.status==ThirdPersonCameraMotionStatus3D::ResetCut,"initial explicit cut");
    }
    const auto initial=control.Camera();unsigned rejected{},firstMove{},firstVisible{};
    for(unsigned tick=1;tick<=180;++tick){
        const auto prior=noisy.Camera();
        for(float rejectedDt:std::array{0.f,-1.f,.2f,std::numeric_limits<float>::quiet_NaN()}){
            const auto r=noisy.Update(desired,samples,solids,{},terrain,rejectedDt);
            Need(!r.accepted&&Same(prior,noisy.Camera()),"paused/invalid mutated camera");++rejected;
        }
        auto teleport=desired;teleport.target.x+=10;teleport.position.x+=10;
        std::array shifted=samples;for(auto& p:shifted)p.x+=10;
        const auto blocked=noisy.Update(teleport,shifted,solids,{},terrain,dt);
        Need(!blocked.accepted&&Same(prior,noisy.Camera()),"blocked request mutated camera");++rejected;
        const auto a=control.Update(desired,samples,solids,{},terrain,dt);
        const auto b=noisy.Update(desired,samples,solids,{},terrain,dt);
        Need(a.accepted&&b.accepted,"stationary accepted tracking");
        Need(Same(a.camera,b.camera)&&a.status==b.status&&a.subjectVisible==b.subjectVisible,
             "rejected or paused call consumed quiet grace or route");
        Need(ThirdPersonCameraPathClear3D(prior.position,b.camera.position,solids,terrain,{},.16f),
             "recovery camera volume crossed wall");
        Need(b.displacement<=.50001f,"recovery exceeded original displacement limit");
        if(!Same(initial.position,a.camera.position)&&!firstMove)firstMove=tick;
        if(a.subjectVisible&&!firstVisible)firstVisible=tick;
        if(tick<=59)Need(Same(initial.position,a.camera.position),"auto recovery ignored one-second request priority");
    }
    Need(firstMove>=60&&firstVisible>0,"no real recovery after quiet grace");
    // 恢復後 Reset 必須重新取得手動優先權，不沿用舊 quiet 計時。
    noisy.Reset();Need(noisy.Update(desired,samples,solids,{},terrain,dt).accepted,"reset cut");
    for(unsigned tick=1;tick<=59;++tick){
        const auto r=noisy.Update(desired,samples,solids,{},terrain,dt);
        Need(r.accepted&&Same(initial.position,r.camera.position),"Reset retained old quiet grace");
    }
    std::cout<<"QUIET_CONTRACT_PASS accepted_trace=180 rejected_insertions="<<rejected
        <<" reset_trace=59 first_recovery_tick="<<firstMove<<" first_visible_tick="<<firstVisible
        <<" source=V219 gpu=0 native=0\n";return 0;
}catch(const std::exception& e){std::cerr<<"FIRST_DIVERGENCE "<<e.what()<<'\n';return 1;}}
