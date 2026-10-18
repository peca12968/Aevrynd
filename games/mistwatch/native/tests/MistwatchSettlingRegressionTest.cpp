#include "MistwatchPlayerLocomotion.h"
#include <iostream>
#include <stdexcept>
using namespace Aevrynd;using namespace Mistwatch;
namespace {
unsigned cases{},steps{};float peakFootStep{};
void Need(bool good,const char* text){if(!good)throw std::runtime_error(text);}
struct Fixture{
 HeightField3D terrain{{{-30,0,-30},128,128,1}};
 CharacterController3D actor;Evasion evasion;Explorer explorer;PlayerLocomotion owner;
 Fixture(){terrain.Fill(0);actor.Reset({});Need(actor.Step({},{},{},terrain,LocomotionMotor::FixedStep),"initial contact");
 explorer.Update({},0,false);Need(explorer.PlaceFeet(actor.Position(),true,false,terrain,{},{}),"initial pose");}
 void Step(PlayerLocomotionInput input={}){
 auto r=owner.Advance(actor,evasion,explorer,input,LocomotionMotor::FixedStep,{},{},terrain,[]{return true;});++steps;
 if(!r.accepted)std::cerr<<"case="<<cases<<" step="<<steps<<" line="<<r.poseFailureLine<<'\n';
 Need(r.accepted,"actual owner step");
 for(unsigned s=0;s<2;++s){
 const auto h=TransformPoint(explorer.pose.globalMatrices[9+s*3],{}),k=TransformPoint(explorer.pose.globalMatrices[10+s*3],{}),a=TransformPoint(explorer.pose.globalMatrices[11+s*3],{});
 Need(std::abs(Length(h-k)-.4f)<.0001f&&std::abs(Length(k-a)-.38f)<.0001f,"bone lengths preserved");
 }}
 void Rest(){
 bool stopped=false;Vec3 fixed{};unsigned starts=0;std::array<Vec3,2> last{};
 for(unsigned tick=0;tick<360;++tick){
 const auto old=std::array{explorer.footSwings[0].Active(),explorer.footSwings[1].Active()};
 Step();
 if(stopped){
 Need(Length(actor.Position()-fixed)<.000001f,"rest root drift");
 Need(!(explorer.footSwings[0].Active()&&explorer.footSwings[1].Active()),"two feet swinging");
 for(unsigned s=0;s<2;++s){
 const float displacement=Length(explorer.footTargets[s]-last[s]);peakFootStep=(std::max)(peakFootStep,displacement);
 Need(displacement<.06f,"idle foot teleport");
 starts+=!old[s]&&explorer.footSwings[s].Active();
 }
 }else if(Length(owner.Motor().Velocity())==0&&actor.Grounded()){stopped=true;fixed=actor.Position();}
 last=explorer.footTargets;
 if(tick>240)Need(!explorer.footSwings[0].Active()&&!explorer.footSwings[1].Active(),"idle does not chatter");
 }
 Need(stopped&&starts<=3,"bounded settle steps");
 const auto inverse=CreateRotationMatrix({0,-explorer.yaw,0})*CreateTranslationMatrix(actor.Position()*-1.f);
 for(unsigned s=0;s<2;++s){
 auto a=TransformPoint(inverse,explorer.footTargets[s]);
 Need(std::abs(a.x-(s?.12f:-.12f))<.005f&&std::abs(a.z)<.02f,"natural settled feet");
 }
 }
};
}
int main()try{
 for(float sign:{1.f,-1.f}){
 ++cases;Fixture f;
 const unsigned trailing=sign>0?0u:1u,other=1-trailing;
 Need(f.explorer.footSwings[trailing].Reset({-.2f*sign,.1f,-.5f}),"trailing fixture");
 Need(f.explorer.footSwings[other].Reset({-.05f*sign,.1f,0}),"inside fixture");
 f.explorer.footTargets[trailing]=f.explorer.footSwings[trailing].Landing();
 f.explorer.footTargets[other]=f.explorer.footSwings[other].Landing();
 f.Rest();
 }
 for(bool running:{false,true})for(unsigned phase=0;phase<8;++phase)for(float angle:{0.f,1.570796327f,3.141592654f}){
 ++cases;Fixture f;PlayerLocomotionInput move;move.movement={0,0,1};move.running=running;move.speedMultiplier=running?1.5f:1.f;
 for(unsigned t=0;t<180+phase*4;++t)f.Step(move);
 move.movement={std::sin(angle),0,std::cos(angle)};
 for(unsigned t=0;t<180;++t)f.Step(move);
 f.Rest();
 }
 for(unsigned delay:{0u,8u,24u})for(float angle:{0.f,1.570796327f,3.141592654f}){
 ++cases;Fixture f;PlayerLocomotionInput move;move.movement={0,0,1};
 for(unsigned t=0;t<240;++t)f.Step(move);
 bool found=false;
 for(unsigned t=0;t<240;++t){f.Step();if(Length(f.owner.Motor().Velocity())==0&&(f.explorer.footSwings[0].Active()||f.explorer.footSwings[1].Active())){found=true;break;}}
 Need(found,"restart must cover active foot path");
 for(unsigned t=0;t<delay;++t)f.Step();
 move.movement={std::sin(angle),0,std::cos(angle)};move.running=true;move.speedMultiplier=1.5f;
 for(unsigned t=0;t<300;++t)f.Step(move);
 f.Rest();
 }
 std::cout<<"SETTLEMENT_COVERAGE_PASS cases="<<cases<<" steps="<<steps<<" peak_idle_foot_step="<<peakFootStep<<'\n';return 0;
}catch(const std::exception& e){std::cerr<<"SETTLEMENT_FIRST_DIVERGENCE "<<e.what()<<'\n';return 1;}
