#include "MistwatchFrameInputLatch.h"
#include "MistwatchPlayback.h"
#include <iostream>
#include <stdexcept>
#include <vector>

namespace {
using Aevrynd::Input;using Aevrynd::Vec3;using Mistwatch::FrameInputLatch;using Mistwatch::FrameEdgeBatch;using Mistwatch::FrameEdge;
unsigned checks{};
constexpr std::array<unsigned,8> EdgeKeys{'Q',VK_SPACE,'E','F','1','2','3',VK_F5};
void Need(bool value,const char* why){++checks;if(!value)throw std::runtime_error(why);}
bool Same(Vec3 a,Vec3 b){return a.x==b.x&&a.y==b.y&&a.z==b.z;}
void Batch(Input& input,unsigned mask){
    // 真設備可在兩次固定更新之間放開再按下；不直接偽造 pressed 內部欄位。
    input.ConsumePresses();for(unsigned key:EdgeKeys)input.SetKey(key,false);
    for(unsigned i=0;i<EdgeKeys.size();++i)if(mask&(1u<<i))input.SetKey(EdgeKeys[i],true);
}
void HeldRejectedAndReplay(){
    Input input;FrameInputLatch latch;input.BeginFrame();input.SetKey('Q',true);input.SetKey('W',true);
    Need(input.Pressed('Q')&&latch.Observe(input,{1,0,0})&&latch.Count()==1,"first held edge");
    for(unsigned update=0;update<64;++update){
        input.ConsumePresses();input.BeginFrame();input.SetKey('Q',true);
        input.SetKey('W',false);input.SetKey('A',(update%2)==0);input.SetKey('D',(update%2)!=0);
        Need(input.Down('Q')&&!input.Pressed('Q'),"held WM-repeat created Pressed");
        Need(latch.Observe(input,{0,0,1})&&latch.Count()==1,"held rejected update enqueued duplicate");
        Need(latch.Front().mask==1u&&Same(latch.Front().dodgeDirection,{1,0,0}),"old evade intent changed with current held direction");
        Need(!input.Down('W')&&(input.Down('A')||input.Down('D')),"held direction not current input");
    }
    input.ConsumePresses();input.SetKey('Q',false);Need(latch.Observe(input,{0,0,1})&&latch.Count()==1,"release made edge");
    input.ConsumePresses();input.SetKey('Q',true);Need(latch.Observe(input,{-1,0,0})&&latch.Count()==2,"legitimate repress lost");
    latch.CommitWorld();Need(latch.Count()==1&&Same(latch.Front().dodgeDirection,{-1,0,0}),"repress FIFO order");
    latch.CommitWorld();Need(latch.Count()==0,"second edge not consumed");
    input.ConsumePresses();input.SetKey('W',true);input.SetKey('A',true);input.SetKey(VK_SHIFT,true);input.SetKey(VK_CONTROL,true);
    Need(latch.Observe(input,{0,0,-1})&&latch.Count()==0,"held-only movement queued stale direction");
    // 與原 InputReplay::Tick 相同：先 Consume，再一次套用最終 held 集合。
    Input replay;FrameInputLatch replayLatch;
    for(unsigned update=0;update<24;++update){
        replay.ConsumePresses();std::array<bool,256> desired{};desired['W']=true;
        desired['E']=update<8||(update>=16&&update<24);
        for(unsigned key=0;key<desired.size();++key)replay.SetKey(key,desired[key]);
        Need(replayLatch.Observe(replay,{0,0,1}),"replay equivalent observe");
        Need(replayLatch.Count()==(update<16?1u:2u),"replay held edge duplicated");
    }
    std::cout<<"HELD_REJECT_REPLAY_PASS rejected_updates=64 replay_updates=24 legitimate_repress=1 held_direction_queued=0\n";
}
void FifoAndWrap(){
    Input input;FrameInputLatch latch;std::vector<FrameEdgeBatch> expected;
    const auto append=[&](unsigned ordinal){const unsigned bit=ordinal%8;
        const unsigned mask=(1u<<bit)|((ordinal%3)==0?(1u<<((bit+3)%8)):0u);
        const Vec3 direction{float(ordinal),0,-float(ordinal)};Batch(input,mask);
        Need(latch.Observe(input,direction),"FIFO append");expected.push_back({mask,direction});
    };
    for(unsigned i=0;i<16;++i)append(i);Need(latch.Count()==16&&!latch.Overflowed(),"capacity16 before wrap");
    for(unsigned i=0;i<6;++i){Need(latch.Front().mask==expected[i].mask&&Same(latch.Front().dodgeDirection,expected[i].dodgeDirection),"FIFO initial order");
        const auto count=latch.Count();latch.CommitWorld();Need(latch.Count()+1==count,"success did not pop exactly once");}
    for(unsigned i=16;i<22;++i)append(i);Need(latch.Count()==16,"wrapped capacity");
    for(unsigned i=6;i<22;++i){Need(latch.Front().mask==expected[i].mask&&Same(latch.Front().dodgeDirection,expected[i].dodgeDirection),"FIFO wrapped/multimask order");
        const auto count=latch.Count();for(unsigned rejected=0;rejected<3;++rejected)(void)latch.Front();
        Need(latch.Count()==count,"rejected Front popped input");latch.CommitWorld();Need(latch.Count()+1==count,"wrapped commit pop count");}
    Need(latch.Count()==0&&latch.Front().mask==0,"drain left stale batch");latch.CommitWorld();Need(latch.Count()==0,"empty commit underflow");
    std::cout<<"FIFO_WRAP_PASS batches=22 initial_capacity=16 wrap_appends=6 multimask_same_batch=1 success_pops=22 reject_pops=0\n";
}
void CameraOnly(){
    Input input;FrameInputLatch latch;input.SetMouse({100,100});input.SetMouseButton(true);input.SetMouse({108,96});
    input.AddMouseWheel(2);input.SetKey(VK_SPACE,true);Need(latch.Observe(input,{}),"camera initial observe");
    Need(latch.Count()==1&&latch.Drag().x==8&&latch.Drag().y==-4&&latch.Wheel()==2,"camera accumulation initial");
    latch.CommitCamera();Need(latch.Count()==1&&latch.Front().Pressed(FrameEdge::Jump)&&latch.Drag().x==0&&latch.Drag().y==0&&latch.Wheel()==0,"camera-only popped world edge");
    Need(input.Down(VK_SPACE)&&input.MouseDown(),"camera commit changed held device state");
    input.ConsumePresses();input.SetKey(VK_SPACE,true);input.SetMouse({111,97});input.AddMouseWheel(1);
    Need(latch.Observe(input,{})&&latch.Count()==1&&latch.Drag().x==3&&latch.Drag().y==1&&latch.Wheel()==1,"camera next update duplicated cleared motion");
    input.ConsumePresses();input.SetMouse({110,99});input.AddMouseWheel(.5f);Need(latch.Observe(input,{}),"retained camera observe");
    Need(latch.Count()==1&&latch.Drag().x==2&&latch.Drag().y==3&&latch.Wheel()==1.5f,"rejected camera deltas not retained exactly once");
    latch.CommitWorld();Need(latch.Count()==0&&latch.Drag().x==0&&latch.Drag().y==0&&latch.Wheel()==0,"world commit did not clear camera and pop once");
    std::cout<<"CAMERA_ONLY_PASS world_edge_retained=1 held_device_unchanged=1 rejected_mouse_retained=1 successful_world_pop=1\n";
}
void OverflowAndCancel(){
    Input input;FrameInputLatch latch;std::array<FrameEdgeBatch,16> originals{};
    for(unsigned i=0;i<16;++i){const unsigned mask=1u<<(i%8);const Vec3 direction{float(i),0,0};Batch(input,mask);
        Need(latch.Observe(input,direction),"overflow fixture fill");originals[i]={mask,direction};}
    const unsigned refused=(1u<<unsigned(FrameEdge::Jump))|(1u<<unsigned(FrameEdge::Save));Batch(input,refused);
    Need(!latch.Observe(input,{99,0,0})&&latch.Overflowed()&&latch.Count()==16&&latch.UnacceptedMask()==refused,"17th overflow did not preserve original16/report rejected mask");
    auto inspection=latch;for(unsigned i=0;i<16;++i){Need(inspection.Front().mask==originals[i].mask&&Same(inspection.Front().dodgeDirection,originals[i].dodgeDirection),"overflow rewrote original batch");inspection.CommitWorld();}
    input.ConsumePresses();Need(!latch.Observe(input,{})&&latch.Count()==16&&latch.UnacceptedMask()==refused,"sticky overflow changed original state");
    bool started=true,paused=false;input.SetKey(VK_ESCAPE,true);
    Mistwatch::UpdatePlaybackControl(started,paused,input.Focused()&&!input.FocusLost(),input.Pressed(VK_RETURN),input.Pressed(VK_ESCAPE));
    Need(paused,"actual pause control failed to cancel overflow");if(paused)latch.Clear();
    Need(!latch.Overflowed()&&latch.Count()==0&&latch.UnacceptedMask()==0&&latch.Front().mask==0,"pause Clear left overflow/old edge");
    input.ConsumePresses();input.SetKey(VK_ESCAPE,false);input.SetKey(VK_ESCAPE,true);
    Mistwatch::UpdatePlaybackControl(started,paused,input.Focused()&&!input.FocusLost(),false,input.Pressed(VK_ESCAPE));
    Need(!paused,"actual pause control failed resume");Batch(input,1u<<unsigned(FrameEdge::Interact));
    Need(latch.Observe(input,{})&&latch.Count()==1&&latch.Front().Pressed(FrameEdge::Interact),"new edge after overflow cancel failed");
    latch.Clear();input.ConsumePresses();input.SetKey('E',true);
    Need(latch.Observe(input,{})&&latch.Count()==0,"reset Clear replayed still-held pre-reset edge");
    input.ConsumePresses();input.SetKey('E',false);input.SetKey('E',true);Need(latch.Observe(input,{})&&latch.Count()==1,"new edge after reset failed");
    input.SetFocused(false);Mistwatch::UpdatePlaybackControl(started,paused,input.Focused()&&!input.FocusLost(),false,false);if(paused)latch.Clear();
    Need(paused&&latch.Count()==0&&!input.Down('E'),"focus loss did not cancel queued edge and held key");
    input.SetFocused(true);Mistwatch::UpdatePlaybackControl(started,paused,input.Focused()&&!input.FocusLost(),false,false);
    Need(paused&&input.FocusLost(),"focus regain silently resumed");input.ConsumePresses();input.SetKey(VK_ESCAPE,true);
    Mistwatch::UpdatePlaybackControl(started,paused,input.Focused()&&!input.FocusLost(),false,input.Pressed(VK_ESCAPE));
    Need(!paused,"focus-resume manual pause edge failed");Batch(input,1u<<unsigned(FrameEdge::Attack));
    Need(latch.Observe(input,{})&&latch.Count()==1&&latch.Front().Pressed(FrameEdge::Attack),"fresh post-focus edge failed");
    std::cout<<"OVERFLOW_CANCEL_PASS original16_preserved=1 refused_mask="<<refused<<" sticky_until_clear=1 pause_resume=1 reset=1 focus_resume=1\n";
}
void NoIndependentIdentity(){
    Input input;FrameInputLatch latch;input.SetKey('E',true);
    Need(latch.Observe(input,{})&&latch.Observe(input,{})&&latch.Count()==2,"standalone API no-ID limitation changed");
    std::cout<<"API_SCOPE_OBSERVED same_pressed_double_observe_batches=2 independent_dedup_id=0 current_main_double_observe_proved=0\n";
}
}
int main()try{
    HeldRejectedAndReplay();FifoAndWrap();CameraOnly();OverflowAndCancel();NoIndependentIdentity();
    std::cout<<"FRAME_INPUT_LATCH_CPU_PASS checks="<<checks<<" exact_frozen_latch=1 real_Input_API=1 windows_created=0 global_input_injected=0 game_or_GPU_executed=0 main_or_native_qualified=0\n";return 0;
}catch(const std::exception& e){std::cerr<<"FIRST_DIVERGENCE: "<<e.what()<<" checks="<<checks<<'\n';return 1;}
