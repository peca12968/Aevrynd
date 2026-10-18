#include <Aevrynd/FootSwingTrajectory3D.h>
#include <array>
#include <iostream>
#include <stdexcept>
#include <limits>
using namespace Aevrynd;
unsigned checks{},samples{};
void Need(bool value,const char* message){++checks;if(!value)throw std::runtime_error(message);}
bool Same(Vec3 a,Vec3 b){return a.x==b.x&&a.y==b.y&&a.z==b.z;}
bool Same(const FootSwingSample3D& a,const FootSwingSample3D& b){return Same(a.positionWorld,b.positionWorld)&&Same(a.velocityWorld,b.velocityWorld)&&a.unusedSeconds==b.unusedSeconds&&a.planted==b.planted;}
bool Same(const FootSwingTrajectory3D& a,const FootSwingTrajectory3D& b){return a.Ready()==b.Ready()&&a.Active()==b.Active()&&Same(a.Start(),b.Start())&&Same(a.Landing(),b.Landing())&&a.Elapsed()==b.Elapsed()&&a.Duration()==b.Duration()&&a.LiftHeight()==b.LiftHeight()&&a.SpeedBound()==b.SpeedBound();}
int main()try{
    const double nan=std::numeric_limits<double>::quiet_NaN(),inf=std::numeric_limits<double>::infinity();
    FootSwingTrajectory3D path;FootSwingSample3D result{{1,2,3},{4,5,6},7,true};const auto sentinel=result;
    Need(!path.Advance(.01,result)&&Same(result,sentinel),"uninitialized output unchanged");
    Need(!path.Begin({1,0,0},.2,.1,10),"uninitialized begin rejected");
    Need(path.Reset({-1,3,2}),"reset");
    Need(path.Advance(.25,result)&&Same(result.positionWorld,{-1,3,2})&&result.planted&&result.unusedSeconds==.25,"stance locks world and consumes no time");
    for(const auto bad:std::array<std::array<double,3>,9>{{{0,.1,10},{-.1,.1,10},{nan,.1,10},{inf,.1,10},{.2,-.1,10},{.2,nan,10},{.2,.1,0},{.2,.1,inf},{.2,.1,nan}}}){
        const auto before=path;Need(!path.Begin({},bad[0],bad[1],bad[2])&&Same(path,before),"invalid begin atomic");
    }
    const auto before=path;
    Need(!path.Begin({100,3,2},.01,.1,10)&&Same(path,before),"speed bound rejects teleport");
    Need(!path.Reset({float(nan),0,0})&&Same(path,before),"invalid reset atomic");
    Need(!path.Begin({0,float(inf),0},.1,.1,10)&&Same(path,before),"invalid landing atomic");
    Need(!path.Begin({},1.e-300,1,10)&&Same(path,before),"tiny duration rejected by speed");
    Need(path.Begin({0,3,2},.5,.14,10),"begin bounded step");
    Need(path.Advance(0,result)&&Same(result.positionWorld,{-1,3,2})&&Same(result.velocityWorld,{})&&!result.planted,"zero-time begins at true anchor");
    const auto started=path;
    Need(!path.Begin({5,0,0},1,0,10)&&Same(path,started),"active swing cannot be replaced by input");
    for(double dt:{-1.,nan,inf}){result=sentinel;Need(!path.Advance(dt,result)&&Same(path,started)&&Same(result,sentinel),"invalid advance atomic");}
    Need(path.Advance(.25,result)&&std::abs(result.positionWorld.x+.5f)<1.e-6f&&std::abs(result.positionWorld.y-3.14f)<1.e-6f,"midpoint and lift");
    Need(path.Advance(.4,result)&&Same(result.positionWorld,{0,3,2})&&Same(result.velocityWorld,{})&&result.planted&&std::abs(result.unusedSeconds-.15)<1.e-12,"boundary returns remainder and exact endpoint");
    Need(path.Advance(.1,result)&&Same(result.positionWorld,{0,3,2})&&result.unusedSeconds==.1,"finished anchor stays locked");
    // 不同頻率下每步位移受同一速度界限約束，而非出現與 dt 無關的瞬移。
    for(unsigned hz:{30u,60u,120u,240u})for(Vec3 landing:{Vec3{.6f,0,0},Vec3{-.4f,.2f,.3f},Vec3{0,-.3f,-.6f},Vec3{0,0,0}}){
        FootSwingTrajectory3D curve;Need(curve.Reset({}),"rate reset");Need(curve.Begin(landing,.6,.14,10),"rate begin");
        Vec3 previous{};const double dt=1./hz;double elapsed=0;float maxStep=0;
        for(unsigned i=0;i<hz;++i){
            Need(curve.Advance(dt,result),"rate advance");elapsed+=dt;++samples;
            const float step=Length(result.positionWorld-previous);maxStep=std::max(maxStep,step);
            Need(step<=curve.SpeedBound()*dt+2.e-6,"displacement follows bound times dt");
            Need(Length(result.velocityWorld)<=curve.SpeedBound()+2.e-6,"analytic velocity bound");
            Need(result.positionWorld.y>=std::min(0.f,landing.y)-1.e-6f,"nonnegative lift above interpolated baseline");
            if(elapsed>.600001)Need(result.planted&&Same(result.positionWorld,landing)&&Same(result.velocityWorld,{}),"completed without reset after stopped input");
            previous=result.positionWorld;
        }
        std::cout<<"SWING_RATE hz="<<hz<<" end="<<landing.x<<','<<landing.y<<','<<landing.z<<" max_step="<<maxStep<<" speed_bound="<<curve.SpeedBound()<<'\n';
    }
    // 邊界附近速度趨零；抬腳與水平位置皆以零速度、零加速度接回站立。
    for(double epsilon:{1.e-3,1.e-4,1.e-5}){
        FootSwingTrajectory3D start,end;Need(start.Reset({})&&end.Reset({}),"endpoint reset");
        Need(start.Begin({.6f,.2f,.1f},1,.14,10)&&end.Begin({.6f,.2f,.1f},1,.14,10),"endpoint begin");
        Need(start.Advance(epsilon,result)&&Length(result.velocityWorld)<100*epsilon*epsilon,"start velocity quadratic limit");
        Need(end.Advance(1-epsilon,result)&&Length(result.velocityWorld)<100*epsilon*epsilon,"end velocity quadratic limit");
    }
    // 時間拆分不變；零時間不推進，並可用剩餘時間啟動下一步。
    FootSwingTrajectory3D one,split;Need(one.Reset({})&&split.Reset({}),"split reset");
    Need(one.Begin({.6f,.1f,.2f},1,.14,10)&&split.Begin({.6f,.1f,.2f},1,.14,10),"split begin");
    FootSwingSample3D expected;Need(one.Advance(.375,expected),"one sample");
    for(unsigned i=0;i<3;++i)Need(split.Advance(.125,result),"split sample");
    Need(Same(result,expected)&&Same(one,split),"equal elapsed equal state");
    const auto frozen=split;Need(split.Advance(0,result)&&Same(split,frozen)&&Same(result,expected),"zero-time does not progress");
    Need(split.Reset({3,4,5})&&!split.Active()&&split.Elapsed()==0,"load reset abandons previous swing");
    Need(split.Advance(0,result)&&Same(result.positionWorld,{3,4,5})&&result.planted,"load anchor");
    // 拒絕無法在目前時間尺度表示進展的區間，不讓排程器反覆處理相同剩餘時間。
    FootSwingTrajectory3D tiny;Need(tiny.Reset({})&&tiny.Begin({},1.e-20,0,1),"tiny no-motion setup");
    const auto tinyBefore=tiny;result=sentinel;
    Need(!tiny.Advance(.1,result)&&Same(tiny,tinyBefore)&&Same(result,sentinel),"unrepresentable remainder atomic rejection");
    FootSwingTrajectory3D slow;Need(slow.Reset({})&&slow.Begin({1,0,0},1,0,2)&&slow.Advance(.5,result),"elapsed resolution setup");
    const auto slowBefore=slow;result=sentinel;
    Need(!slow.Advance(1.e-20,result)&&Same(slow,slowBefore)&&Same(result,sentinel),"unrepresentable elapsed atomic rejection");
    // 大座標的小時間步會量化；以兩端各一 ULP 保守涵蓋，不放寬真碰撞門檻。
    FootSwingTrajectory3D quantized;Need(quantized.Reset({100,0,0})&&quantized.Begin({101,0,0},1,0,2),"quantized setup");
    Need(quantized.Advance(.100016,result),"quantized start");const auto previous=result.positionWorld;
    Need(quantized.Advance(.000001,result),"quantized next");
    const double ulp=double(std::nextafter(previous.x,std::numeric_limits<float>::infinity()))-previous.x+
        double(std::nextafter(result.positionWorld.x,std::numeric_limits<float>::infinity()))-result.positionWorld.x;
    Need(Length(result.positionWorld-previous)<=quantized.SpeedBound()*.000001+ulp,"ULP-aware motion bound");
    // .1 與十次 .01 僅保證數值接近；不宣稱不同累加順序逐位元相同。
    FootSwingTrajectory3D decimalOne,decimalSplit;
    Need(decimalOne.Reset({})&&decimalSplit.Reset({})&&decimalOne.Begin({1,0,0},1,.14,3)&&decimalSplit.Begin({1,0,0},1,.14,3),"decimal setup");
    Need(decimalOne.Advance(.1,expected),"decimal one");
    for(unsigned i=0;i<10;++i)Need(decimalSplit.Advance(.01,result),"decimal split");
    Need(Length(result.positionWorld-expected.positionWorld)<1.e-6f&&std::abs(decimalOne.Elapsed()-decimalSplit.Elapsed())<1.e-14,"decimal numeric agreement only");
    // 涵蓋目前走、跑、蹲的短半週時間；這仍不是落點可達或人體動作驗收。
    for(double duration:{.07,.105,.233})for(unsigned hz:{30u,60u,120u}){
        FootSwingTrajectory3D fast;Need(fast.Reset({})&&fast.Begin({.6f,0,0},duration,.14,30),"short step setup");
        Vec3 previousFast{};const double dt=1./hz;
        for(unsigned i=0;i<hz;++i){
            Need(fast.Advance(dt,result),"short step progress");
            Need(Length(result.positionWorld-previousFast)<=fast.SpeedBound()*dt+2.e-6,"short step displacement bound");
            previousFast=result.positionWorld;++samples;
        }
        Need(!fast.Active()&&result.planted&&Same(result.positionWorld,{.6f,0,0}),"short step landed exactly");
    }
    std::cout<<"FOOT_SWING_TRAJECTORY_PASS checks="<<checks<<" samples="<<samples<<" collision_and_game_integration_accepted=false\n";
    return 0;
}catch(const std::exception& e){std::cerr<<"FOOT_SWING_TRAJECTORY_FAIL "<<e.what()<<'\n';return 1;}
