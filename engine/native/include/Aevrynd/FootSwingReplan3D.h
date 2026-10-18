#pragma once
#include "Math.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

namespace Aevrynd {
struct FootSwingReplanSample3D {
    Vec3 positionWorld{},velocityWorld{},accelerationWorld{};
};
enum class FootSwingReplanStatus3D { Ready,InvalidInput,NumericRange,SpeedExceeded,SpeedUncertain };
struct FootSwingReplanAssessment3D {
    FootSwingReplanStatus3D status{FootSwingReplanStatus3D::InvalidInput};
    double speedLowerBound{},speedUpperBound{};
};

// 世界空間 C2 重規劃；不負責接觸、腿長或人體動作驗收。所有迴圈與堆疊大小均有固定上限。
class FootSwingReplan3D {
    struct D3 { double x{},y{},z{}; };
    template<std::size_t N> using Points=std::array<D3,N>;
public:
    // 只在整條曲線的速度上界已獲證明時提交；拒絕不變更既有曲線。
    bool Begin(FootSwingReplanSample3D start,Vec3 landing,double duration,double lift,double maxSpeed)noexcept{
        FootSwingReplan3D candidate;
        if(Build(start,landing,duration,lift,maxSpeed,candidate).status!=FootSwingReplanStatus3D::Ready)return false;
        *this=candidate;return true;
    }
    // 診斷不寫入曲線：真超速有曲線取樣下界證據；不確定只表示有限細分仍無法證明。
    static FootSwingReplanAssessment3D Assess(FootSwingReplanSample3D start,Vec3 landing,double duration,double lift,double maxSpeed)noexcept{
        FootSwingReplan3D candidate;return Build(start,landing,duration,lift,maxSpeed,candidate);
    }
    bool Evaluate(double elapsed,FootSwingReplanSample3D& output)const noexcept{
        if(!ready_||!std::isfinite(elapsed)||elapsed<0||elapsed>duration_)return false;
        if(elapsed==0){output=start_;return true;}
        if(elapsed==duration_){output={landing_,{}, {}};return true;}
        const double u=elapsed/duration_;
        const auto p=Add(ToDouble(start_.positionWorld),At(position_,u));
        const auto v=At(velocity_,u),a=At(acceleration_,u);
        if(!Representable(p)||!Representable(v)||!Representable(a))return false;
        const FootSwingReplanSample3D candidate{ToFloat(p),ToFloat(v),ToFloat(a)};
        output=candidate;return true;
    }
    // 軸不必正規化；傳回世界位置內積界，涵蓋解析曲線及 Evaluate 的 float 量化。
    // 先切割相對起點控制點，再投影凸包；不以起終點或有限取樣代替整段界限。
    bool ProjectionRange(Vec3 axis,double fromSeconds,double toSeconds,double& lower,double& upper)const noexcept{
        const auto direction=ToDouble(axis);
        if(!ready_||!Finite(direction)||(axis.x==0&&axis.y==0&&axis.z==0)||
           !std::isfinite(fromSeconds)||!std::isfinite(toSeconds)||fromSeconds<0||
           fromSeconds>toSeconds||toSeconds>duration_||&lower==&upper)return false;
        const auto origin=ToDouble(start_.positionWorld);
        auto segment=position_;
        const bool exactEndpoint=fromSeconds==toSeconds&&(fromSeconds==0||fromSeconds==duration_);
        if(fromSeconds==toSeconds){
            // 零長區間不除以 duration-from；兩端使用公開 Evaluate 的精確端點。
            const auto point=exactEndpoint?(fromSeconds==0?D3{}:Sub(ToDouble(landing_),origin)):
                At(position_,fromSeconds/duration_);
            segment.fill(point);
        }else{
            Points<7> left{},right{};
            if(fromSeconds>0){Split(segment,fromSeconds/duration_,left,right);segment=right;}
            if(toSeconds<duration_){
                Split(segment,(toSeconds-fromSeconds)/(duration_-fromSeconds),left,right);segment=left;
            }
        }
        double low=std::numeric_limits<double>::infinity(),high=-low,arithmeticScale=0,quantizationScale=0;
        const auto absoluteDot=[&](D3 p){return std::abs(direction.x*p.x)+std::abs(direction.y*p.y)+std::abs(direction.z*p.z);};
        const auto dot=[&](D3 p){return direction.x*p.x+direction.y*p.y+direction.z*p.z;};
        // 原曲線尺度涵蓋切割參數與 de Casteljau 捨入；局部凸包尺度涵蓋輸出量化。
        for(auto p:position_)arithmeticScale=std::max(arithmeticScale,absoluteDot(origin)+absoluteDot(p));
        const double originProjection=dot(origin);
        for(auto p:segment){
            const double projected=originProjection+dot(p);
            if(!std::isfinite(projected))return false;
            low=std::min(low,projected);high=std::max(high,projected);
            quantizationScale=std::max(quantizationScale,absoluteDot(Add(origin,p)));
        }
        const double padding=arithmeticScale*(4096*std::numeric_limits<double>::epsilon())+
            (exactEndpoint?0:quantizationScale*(2*double(std::numeric_limits<float>::epsilon())))+
            (exactEndpoint?0:(std::abs(direction.x)+std::abs(direction.y)+std::abs(direction.z))*double(std::numeric_limits<float>::denorm_min()));
        low=std::nextafter(low-padding,-std::numeric_limits<double>::infinity());
        high=std::nextafter(high+padding,std::numeric_limits<double>::infinity());
        if(!std::isfinite(low)||!std::isfinite(high))return false;
        lower=low;upper=high;return true;
    }
    bool Ready()const noexcept{return ready_;}
    double Duration()const noexcept{return duration_;}
    // 連續解析曲線上界；float 世界位置差仍須另計座標 ULP，不能當作碰撞容差。
    double SpeedBound()const noexcept{return speedBound_;}
private:
    static D3 ToDouble(Vec3 a)noexcept{return {a.x,a.y,a.z};}
    static Vec3 ToFloat(D3 a)noexcept{return {float(a.x),float(a.y),float(a.z)};}
    static D3 Add(D3 a,D3 b)noexcept{return {a.x+b.x,a.y+b.y,a.z+b.z};}
    static D3 Sub(D3 a,D3 b)noexcept{return {a.x-b.x,a.y-b.y,a.z-b.z};}
    static D3 Mul(D3 a,double s)noexcept{return {a.x*s,a.y*s,a.z*s};}
    static D3 Blend(D3 a,D3 b,double u)noexcept{return Add(Mul(a,1-u),Mul(b,u));}
    static double Norm(D3 a)noexcept{return std::hypot(a.x,a.y,a.z);}
    static bool Finite(D3 a)noexcept{return std::isfinite(a.x)&&std::isfinite(a.y)&&std::isfinite(a.z);}
    static bool Representable(D3 a)noexcept{
        constexpr double limit=std::numeric_limits<float>::max();
        return Finite(a)&&std::abs(a.x)<=limit&&std::abs(a.y)<=limit&&std::abs(a.z)<=limit;
    }
    template<std::size_t N> static D3 At(Points<N> points,double u)noexcept{
        for(std::size_t count=N;count>1;--count)for(std::size_t i=0;i+1<count;++i)points[i]=Blend(points[i],points[i+1],u);
        return points[0];
    }
    template<std::size_t N> static void Split(Points<N> work,double u,Points<N>& left,Points<N>& right)noexcept{
        left[0]=work[0];right[N-1]=work[N-1];
        for(std::size_t row=1;row<N;++row){
            for(std::size_t i=0;i<N-row;++i)work[i]=Blend(work[i],work[i+1],u);
            left[row]=work[0];right[N-1-row]=work[N-1-row];
        }
    }
    // 速度為五次 Bezier；細分後控制點凸包仍包住整段，不以稀疏取樣冒充上界。
    static void Bound(const Points<6>& points,unsigned depth,double& upper,double& lower)noexcept{
        lower=std::max({lower,Norm(points.front()),Norm(points.back())});
        if(depth==0){for(auto p:points)upper=std::max(upper,Norm(p));return;}
        auto work=points;Points<6> left{},right{};left[0]=work[0];right[5]=work[5];
        for(unsigned row=1;row<6;++row){
            for(unsigned i=0;i<6-row;++i)work[i]=Blend(work[i],work[i+1],.5);
            left[row]=work[0];right[5-row]=work[5-row];
        }
        Bound(left,depth-1,upper,lower);Bound(right,depth-1,upper,lower);
    }
    static FootSwingReplanAssessment3D Build(FootSwingReplanSample3D start,Vec3 landing,double duration,double lift,double maxSpeed,FootSwingReplan3D& candidate)noexcept{
        using Status=FootSwingReplanStatus3D;
        if(!Finite(ToDouble(start.positionWorld))||!Finite(ToDouble(start.velocityWorld))||!Finite(ToDouble(start.accelerationWorld))||
           !Finite(ToDouble(landing))||!std::isfinite(duration)||duration<=0||!std::isfinite(lift)||lift<0||!std::isfinite(maxSpeed)||maxSpeed<=0)return {Status::InvalidInput};
        const double initialSpeed=Norm(ToDouble(start.velocityWorld));
        if(initialSpeed>maxSpeed)return {Status::SpeedExceeded,initialSpeed,initialSpeed};
        candidate.start_=start;candidate.landing_=landing;candidate.duration_=duration;
        // 先以起點為原點，避免大世界座標消去細小初速度與加速度。
        const auto v=ToDouble(start.velocityWorld),a=ToDouble(start.accelerationWorld);
        const auto end=Sub(ToDouble(landing),ToDouble(start.positionWorld));
        Points<6> quintic{{{},Mul(v,duration/5),Add(Mul(v,duration*.4),Mul(Mul(a,duration),duration/20)),end,end,end}};
        for(auto p:quintic)if(!Finite(p))return {Status::NumericRange};
        candidate.position_[0]=quintic[0];candidate.position_[6]=quintic[5];
        for(unsigned i=1;i<6;++i)candidate.position_[i]=Blend(quintic[i],quintic[i-1],double(i)/6);
        // B(6,3)=20u³(1-u)³，因此中間控制點加 64H/20；端點 p/v/a 不變。
        candidate.position_[3].y+=3.2*lift;
        for(auto p:candidate.position_)if(!Representable(Add(ToDouble(start.positionWorld),p)))return {Status::NumericRange};
        for(unsigned i=0;i<6;++i){
            candidate.velocity_[i]=Mul(Sub(candidate.position_[i+1],candidate.position_[i]),6/duration);
            if(!Representable(candidate.velocity_[i]))return {Status::NumericRange};
        }
        for(unsigned i=0;i<5;++i){
            candidate.acceleration_[i]=Mul(Sub(candidate.velocity_[i+1],candidate.velocity_[i]),5/duration);
            if(!Representable(candidate.acceleration_[i]))return {Status::NumericRange};
        }
        double upper=0,lower=0,scale=0;
        for(auto p:candidate.velocity_)scale=std::max(scale,Norm(p));
        Bound(candidate.velocity_,8,upper,lower);
        // 涵蓋有限次 double 插值的捨入；接近界線時寧可回報不確定，不偽稱真超速。
        const double pad=scale*(4096*std::numeric_limits<double>::epsilon());
        upper=scale==0?0:std::nextafter(upper+pad,std::numeric_limits<double>::infinity());
        lower=std::max(initialSpeed,std::max(0.,lower-pad));
        if(!std::isfinite(upper))return {Status::NumericRange};
        if(lower>maxSpeed)return {Status::SpeedExceeded,lower,upper};
        if(upper>maxSpeed)return {Status::SpeedUncertain,lower,upper};
        candidate.speedBound_=upper;candidate.ready_=true;return {Status::Ready,lower,upper};
    }
    FootSwingReplanSample3D start_{};
    Vec3 landing_{};
    Points<7> position_{};
    Points<6> velocity_{};
    Points<5> acceleration_{};
    double duration_{},speedBound_{};
    bool ready_{};
};
}
