#pragma once
#include "Math.h"
#include "FootSwingReplan3D.h"
#include <algorithm>
#include <limits>

namespace Aevrynd {
struct FootSwingSample3D {
    Vec3 positionWorld{},velocityWorld{};
    double unusedSeconds{};
    bool planted{};
};

// 只負責已選定世界落點之間的連續軌跡；呼叫端仍須驗證落點、掃掠、骨長與鞋底碰撞。
// 支撐時鎖世界位置。Begin 不覆寫進行中的一步；明確 Retarget 才能以連續位置／速度／加速度重新規劃。
class FootSwingTrajectory3D {
public:
    bool Reset(Vec3 anchorWorld)noexcept{
        if(!Finite(anchorWorld))return false;
        start_=end_=anchorWorld;elapsed_=duration_=height_=0;speedBound_=0;ready_=true;active_=false;replanned_=false;
        return true;
    }
    bool Begin(Vec3 landingWorld,double durationSeconds,double liftHeight,double maximumSpeed)noexcept{
        if(!ready_||active_||!Finite(landingWorld)||!std::isfinite(durationSeconds)||durationSeconds<=0||
            !std::isfinite(liftHeight)||liftHeight<0||!std::isfinite(maximumSpeed)||maximumSpeed<=0)return false;
        const double dx=double(landingWorld.x)-end_.x,dy=double(landingWorld.y)-end_.y,dz=double(landingWorld.z)-end_.z;
        // 五次位置曲線最大導數 15/8；六次抬腳曲線最大導數 192/(25*sqrt(5))。
        const double bound=(1.875*std::hypot(dx,dy,dz)+LiftDerivativeBound*liftHeight)/durationSeconds;
        if(!std::isfinite(bound)||bound>maximumSpeed||bound>std::numeric_limits<float>::max()||
           liftHeight>std::numeric_limits<float>::max()-std::max(double(end_.y),double(landingWorld.y)))return false;
        start_=end_;end_=landingWorld;elapsed_=0;duration_=durationSeconds;height_=liftHeight;
        speedBound_=bound;active_=true;replanned_=false;return true;
    }
    // 此 API 不替呼叫端決定何時換腳；新路徑仍必須通過世界碰撞與兩腿可達檢查。
    bool Retarget(Vec3 landingWorld,double durationSeconds,double liftHeight,double maximumSpeed)noexcept{
        if(!ready_||!active_)return false;
        FootSwingReplanSample3D current;
        if(!CurrentSample(current))return false;
        FootSwingReplan3D candidate;
        if(!candidate.Begin(current,landingWorld,durationSeconds,liftHeight,maximumSpeed))return false;
        replan_=candidate;replanned_=true;initialSample_=current;
        start_=current.positionWorld;end_=landingWorld;elapsed_=0;duration_=durationSeconds;height_=liftHeight;
        speedBound_=candidate.SpeedBound();return true;
    }
    bool CurrentSample(FootSwingReplanSample3D& output)const noexcept{
        if(!ready_)return false;
        if(!active_){output={end_,{}, {}};return true;}
        if(replanned_)return replan_.Evaluate(elapsed_,output);
        auto copy=*this;FootSwingSample3D sample;
        if(!copy.Advance(0,sample))return false;
        const double u=elapsed_/duration_,v=1-u;
        const double second=60*u*v*(1-2*u)/(duration_*duration_);
        const double liftSecond=384*height_*u*v*((1-2*u)*(1-2*u)-u*v)/(duration_*duration_);
        const Vec3 acceleration{float((double(end_.x)-start_.x)*second),
            float((double(end_.y)-start_.y)*second+liftSecond),float((double(end_.z)-start_.z)*second)};
        if(!Finite(acceleration))return false;
        output={sample.positionWorld,sample.velocityWorld,acceleration};return true;
    }
    bool Replanned()const noexcept{return replanned_;}
    // 呼叫端可驗證剩餘整段的投影範圍，不能假設中途重規劃仍在起終點線段內。
    bool RemainingProjectionRange(Vec3 axis,double& lower,double& upper)const noexcept{
        if(!ready_)return false;
        if(replanned_&&active_)return replan_.ProjectionRange(axis,elapsed_,duration_,lower,upper);
        FootSwingReplan3D curve;
        if(!curve.Begin({active_?start_:end_,{},{}},end_,active_?duration_:1.,active_?height_:0.,
            std::numeric_limits<float>::max()))return false;
        return curve.ProjectionRange(axis,active_?elapsed_:0.,active_?duration_:1.,lower,upper);
    }
    FootSwingReplanSample3D InitialSample()const noexcept{return replanned_?initialSample_:FootSwingReplanSample3D{start_,{}, {}};}
    // 消耗到落地邊界為止，保留剩餘時間給呼叫端；失敗不變更物件與輸出。
    // 已落地時不消耗時間，呼叫端必須停止或有限次安排下一步，不可只用餘量作無界迴圈。
    bool Advance(double seconds,FootSwingSample3D& output)noexcept{
        if(!ready_||!std::isfinite(seconds)||seconds<0)return false;
        if(!active_){output={end_,{},seconds,true};return true;}
        const double used=std::min(seconds,duration_-elapsed_),elapsed=elapsed_+used;
        if(used>0&&(elapsed==elapsed_||seconds-used==seconds))return false;
        FootSwingSample3D candidate;
        candidate.unusedSeconds=seconds-used;
        if(elapsed>=duration_){candidate.positionWorld=end_;candidate.planted=true;}
        else if(replanned_){
            FootSwingReplanSample3D point;
            if(!replan_.Evaluate(elapsed,point))return false;
            candidate.positionWorld=point.positionWorld;candidate.velocityWorld=point.velocityWorld;
        }else{
            const double u=elapsed/duration_,v=1-u;
            const double smooth=u*u*u*(10+u*(-15+6*u));
            const double derivative=30*u*u*v*v;
            const double lift=64*height_*u*u*u*v*v*v;
            const double liftDerivative=192*height_*u*u*v*v*(1-2*u);
            const auto coordinate=[&](float from,float to){return float(double(from)+(double(to)-from)*smooth);};
            const auto velocity=[&](float from,float to){return float((double(to)-from)*derivative/duration_);};
            candidate.positionWorld={coordinate(start_.x,end_.x),float(double(start_.y)+(double(end_.y)-start_.y)*smooth+lift),coordinate(start_.z,end_.z)};
            candidate.velocityWorld={velocity(start_.x,end_.x),float(((double(end_.y)-start_.y)*derivative+liftDerivative)/duration_),velocity(start_.z,end_.z)};
            if(!Finite(candidate.positionWorld)||!Finite(candidate.velocityWorld))return false;
        }
        elapsed_=elapsed;active_=!candidate.planted;output=candidate;return true;
    }
    bool Ready()const noexcept{return ready_;}
    bool Active()const noexcept{return active_;}
    Vec3 Start()const noexcept{return start_;}
    Vec3 Landing()const noexcept{return end_;}
    double Elapsed()const noexcept{return elapsed_;}
    double Duration()const noexcept{return duration_;}
    double LiftHeight()const noexcept{return height_;}
    // 連續曲線的速度界；float 世界位置比較須另保留與座標 ULP 相依的量化誤差。
    double SpeedBound()const noexcept{return speedBound_;}
private:
    static bool Finite(Vec3 p)noexcept{return std::isfinite(p.x)&&std::isfinite(p.y)&&std::isfinite(p.z);}
    static constexpr double LiftDerivativeBound=3.434600413439677;
    Vec3 start_{},end_{};
    double elapsed_{},duration_{},height_{},speedBound_{};
    bool ready_{},active_{};
    FootSwingReplan3D replan_;
    FootSwingReplanSample3D initialSample_{};
    bool replanned_{};
};
}
