#include <Aevrynd/FootSwingReplan3D.h>
#include <array>
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace Aevrynd;
unsigned checks{},samples{};
void Need(bool value,const char* message){++checks;if(!value)throw std::runtime_error(message);}
bool Same(Vec3 a,Vec3 b){return a.x==b.x&&a.y==b.y&&a.z==b.z;}
bool Same(const FootSwingReplanSample3D& a,const FootSwingReplanSample3D& b){return Same(a.positionWorld,b.positionWorld)&&Same(a.velocityWorld,b.velocityWorld)&&Same(a.accelerationWorld,b.accelerationWorld);}
bool Same(const FootSwingReplan3D& a,const FootSwingReplan3D& b){
    if(a.Ready()!=b.Ready()||a.Duration()!=b.Duration()||a.SpeedBound()!=b.SpeedBound())return false;
    for(double t:{0.,.13,.5,.89,1.}){FootSwingReplanSample3D x,y;if(a.Evaluate(t*a.Duration(),x)!=b.Evaluate(t*b.Duration(),y)||!Same(x,y))return false;}
    return true;
}
int main()try{
    using Status=FootSwingReplanStatus3D;
    const double nan=std::numeric_limits<double>::quiet_NaN(),inf=std::numeric_limits<double>::infinity();
    FootSwingReplan3D curve;
    FootSwingReplanSample3D output{{1,2,3},{4,5,6},{7,8,9}},start{{.2f,.1f,-.3f},{.8f,.2f,-.4f},{-.5f,.7f,.3f}};
    const auto sentinel=output;
    Need(!curve.Evaluate(0,output)&&Same(output,sentinel),"uninitialized output atomic");
    const Vec3 landing{.7f,.2f,.3f};
    Need(curve.Begin(start,landing,.8,.14,20),"initial C2 curve");
    Need(curve.Evaluate(0,output)&&Same(output,start),"exact start position velocity acceleration");
    Need(curve.Evaluate(.8,output)&&Same(output.positionWorld,landing)&&Same(output.velocityWorld,{})&&Same(output.accelerationWorld,{}),"exact settled endpoint");
    const auto original=curve;
    for(const auto bad:std::array<std::array<double,3>,12>{{{0,.1,20},{-1,.1,20},{nan,.1,20},{inf,.1,20},{.8,-1,20},{.8,nan,20},{.8,inf,20},{.8,.1,0},{.8,.1,-1},{.8,.1,nan},{.8,.1,inf},{1.e-300,.1,20}}})
        Need(!curve.Begin(start,landing,bad[0],bad[1],bad[2])&&Same(curve,original),"invalid or overflow begin atomic");
    for(unsigned member=0;member<4;++member){auto bad=start;auto badEnd=landing;
        if(member==0)bad.positionWorld.x=float(nan);if(member==1)bad.velocityWorld.y=float(inf);if(member==2)bad.accelerationWorld.z=float(nan);if(member==3)badEnd.x=float(inf);
        Need(!curve.Begin(bad,badEnd,.8,.1,20)&&Same(curve,original),"nonfinite vector atomic");
    }
    Need(!curve.Begin(start,landing,1.e300,.1,20)&&Same(curve,original),"duration multiplication overflow atomic");
    Need(!curve.Begin(start,landing,1.,1.e300,20)&&Same(curve,original),"lift overflow atomic");
    for(double t:{-1.,nan,inf,.80000001}){output=sentinel;Need(!curve.Evaluate(t,output)&&Same(output,sentinel)&&Same(curve,original),"invalid evaluation atomic");}
    // 中途接續初 p/v/a，而非把速度加速度清零；多次換落點仍保持同一接縫契約。
    for(unsigned iteration=0;iteration<12;++iteration){
        FootSwingReplanSample3D seam,atZero,nearStart;
        Need(curve.Evaluate(curve.Duration()*.37,seam),"sample replanning seam");
        const Vec3 target=seam.positionWorld+Vec3{iteration%2?.12f:-.09f,-.015f,.08f};
        Need(curve.Begin(seam,target,.45,.025,80),"repeated replan");
        Need(curve.Evaluate(0,atZero)&&Same(atZero,seam),"replan exact C2 seam");
        Need(curve.Evaluate(1.e-6,nearStart),"near seam sample");
        Need(Length(nearStart.positionWorld-seam.positionWorld)<1.e-4f&&Length(nearStart.velocityWorld-seam.velocityWorld)<.002f&&Length(nearStart.accelerationWorld-seam.accelerationWorld)<.1f,"near seam tends to same p v a");
        Need(curve.Evaluate(.45,output)&&Same(output.positionWorld,target)&&Same(output.velocityWorld,{})&&Same(output.accelerationWorld,{}),"replanned exact settled end");
    }
    // 確認速度上界與實際導數；未知不可被誤記成真超速或通過。
    auto assessment=FootSwingReplan3D::Assess({}, {1,0,0},1,0,2);
    Need(assessment.status==Status::Ready&&assessment.speedUpperBound>=1.875&&assessment.speedUpperBound<1.876,"subdivision tightens convex hull bound");
    assessment=FootSwingReplan3D::Assess({}, {1,0,0},1,0,1.87);
    Need(assessment.status==Status::SpeedExceeded&&assessment.speedLowerBound>1.87,"actual sampled speed exceeds limit");
    assessment=FootSwingReplan3D::Assess({}, {1,0,0},1,0,1.875);
    Need(assessment.status==Status::SpeedUncertain&&assessment.speedLowerBound<=1.875&&assessment.speedUpperBound>1.875,"boundary remains conservatively uncertain");
    Need(!curve.Begin({}, {1,0,0},1,0,1.87),"over speed refused");
    FootSwingReplanSample3D tooFast{{},{7,0,0},{}};
    Need(FootSwingReplan3D::Assess(tooFast,{},1,0,6).status==Status::SpeedExceeded,"initial velocity cannot be hidden by replanning");
    FootSwingReplan3D still;Need(still.Begin({{4,5,6},{},{}},{4,5,6},1,0,1)&&still.SpeedBound()==0,"stationary zero speed bound");
    // 每個頻率真的取不同時刻；共同絕對時刻應一致，非依前次取樣累加內部時間。
    for(unsigned hz:{30u,60u,120u,240u}){
        FootSwingReplan3D path;Need(path.Begin(start,landing,1,.14,20),"rate setup");
        auto previous=start;double previousTime=0;
        for(unsigned i=1;i<=hz;++i){
            const double t=double(i)/hz,dt=t-previousTime;
            Need(path.Evaluate(t,output),"rate evaluation");++samples;
            Need(Length(output.positionWorld-previous.positionWorld)<=path.SpeedBound()*dt+2.e-6,"speed bounds displacement");
            Need(Length(output.velocityWorld)<=path.SpeedBound()+2.e-6,"speed bounds velocity");
            if(i<hz&&i>1){
                FootSwingReplanSample3D before,after;const double epsilon=1.e-4;
                Need(path.Evaluate(t-epsilon,before)&&path.Evaluate(t+epsilon,after),"derivative neighbors");
                Need(Length((after.velocityWorld-before.velocityWorld)*float(.5/epsilon)-output.accelerationWorld)<.015f,"analytic acceleration matches derivative");
                Need(Length((after.positionWorld-before.positionWorld)*float(.5/epsilon)-output.velocityWorld)<.002f,"analytic velocity matches derivative");
            }
            previous=output;previousTime=t;
        }
        FootSwingReplanSample3D direct,split;
        Need(path.Evaluate(.5,direct),"common absolute time");
        for(unsigned i=0;i<=hz/2;++i)Need(path.Evaluate(double(i)/hz,split),"split observations");
        Need(Same(direct,split),"sampling frequency does not mutate curve");
        std::cout<<"REPLAN_RATE hz="<<hz<<" speed_bound="<<path.SpeedBound()<<'\n';
    }
    // 抬腳項兩端二階導數為零；接近端點時確實收斂，不只端點特判。
    FootSwingReplan3D endCurve;Need(endCurve.Begin({}, {.6f,.2f,.1f},1,.14,10),"endpoint limit setup");
    for(double epsilon:{1.e-3,1.e-4,1.e-5}){
        Need(endCurve.Evaluate(epsilon,output)&&Length(output.velocityWorld)<100*epsilon*epsilon&&Length(output.accelerationWorld)<150*epsilon,"start C2 limit");
        Need(endCurve.Evaluate(1-epsilon,output)&&Length(output.velocityWorld)<100*epsilon*epsilon&&Length(output.accelerationWorld)<150*epsilon,"end C2 limit");
    }
    // 初速度使重規劃途中越過起終點；只檢查兩端的轉向限制會漏掉此反例。
    FootSwingReplan3D overshoot;
    Need(overshoot.Begin({{-.12f,0,0},{6,0,0},{}},{-.12f,0,0},.2,0,20),"projection overshoot setup");
    double lower=0,upper=0;
    Need(overshoot.ProjectionRange({1,0,0},0,.2,lower,upper),"whole projection");
    Need(overshoot.Evaluate(.05,output)&&output.positionWorld.x>.10f&&output.positionWorld.x<.103f,"endpoint convexity counterexample");
    Need(upper>=double(output.positionWorld.x)&&lower<=-.12f,"range includes excursion outside endpoints");
    const double wholeWidth=upper-lower;
    Need(overshoot.ProjectionRange({1,0,0},.045,.055,lower,upper)&&upper-lower<wholeWidth*.1,"cut interval tightens hull");
    Need(overshoot.ProjectionRange({1,0,0},.2,.2,lower,upper)&&lower<=double(-.12f)&&upper>=double(-.12f)&&upper-lower<1.e-9,"exact ending zero length interval");
    Need(overshoot.ProjectionRange({1,0,0},0,0,lower,upper)&&lower<=double(-.12f)&&upper>=double(-.12f)&&upper-lower<1.e-9,"exact starting zero length interval");
    // 非單位、負向與斜向投影，整段／子段／單點皆涵蓋實際 float 取樣。
    FootSwingReplan3D projected;Need(projected.Begin(start,landing,1,.14,20),"projection axes setup");
    for(Vec3 axis:{Vec3{1,0,0},Vec3{-3,0,0},Vec3{.2f,2,-.7f},Vec3{0,1,0}})
        for(const auto interval:std::array<std::array<double,2>,6>{{{0,1},{0,.27},{.21,.83},{.87,1},{.37,.37},{1,1}}}){
            Need(projected.ProjectionRange(axis,interval[0],interval[1],lower,upper),"projection interval accepted");
            for(unsigned i=0;i<=256;++i){
                const double t=i==256?interval[1]:interval[0]+(interval[1]-interval[0])*(double(i)/256);
                Need(projected.Evaluate(t,output),"projection reference sample");
                const auto p=output.positionWorld;
                const double value=double(axis.x)*p.x+double(axis.y)*p.y+double(axis.z)*p.z;
                Need(value>=lower&&value<=upper,"all evaluated samples enclosed without test tolerance");++samples;
            }
        }
    // 大座標取消與非零細小軸；投影不得先做 float 內積或正規化軸。
    FootSwingReplan3D translated;Need(translated.Begin({{1000000,1000000,0},{},{}},{1000001,1000000,0},1,0,3),"large translated setup");
    for(Vec3 axis:{Vec3{1,-1,0},Vec3{1.e-30f,0,0}}){
        Need(translated.ProjectionRange(axis,.12,.13,lower,upper),"translated projection");
        for(unsigned i=0;i<=32;++i){Need(translated.Evaluate(.12+.01*(double(i)/32),output),"translated sample");
            const double value=double(axis.x)*output.positionWorld.x+double(axis.y)*output.positionWorld.y;
            Need(value>=lower&&value<=upper,"translation quantization enclosed");}
    }
    for(Vec3 axis:{Vec3{},Vec3{float(nan),0,0},Vec3{0,float(inf),0}}){
        lower=123;upper=456;Need(!projected.ProjectionRange(axis,0,1,lower,upper)&&lower==123&&upper==456,"invalid axis output atomic");
    }
    for(const auto interval:std::array<std::array<double,2>,7>{{{-1,0},{.7,.6},{0,1.01},{nan,1},{0,nan},{inf,inf},{0,inf}}}){
        lower=123;upper=456;Need(!projected.ProjectionRange({1,0,0},interval[0],interval[1],lower,upper)&&lower==123&&upper==456,"invalid interval output atomic");
    }
    FootSwingReplan3D unready;lower=123;upper=456;
    Need(!unready.ProjectionRange({1,0,0},0,0,lower,upper)&&lower==123&&upper==456,"unready projection atomic");
    Need(!projected.ProjectionRange({1,0,0},0,1,lower,lower)&&lower==123,"aliased outputs rejected unchanged");
    std::cout<<"FOOT_SWING_REPLAN_PASS checks="<<checks<<" samples="<<samples<<" collision_natural_motion_and_game_accepted=false\n";
    return 0;
}catch(const std::exception& e){std::cerr<<"FOOT_SWING_REPLAN_FAIL "<<e.what()<<'\n';return 1;}
