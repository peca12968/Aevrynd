#pragma once
#include "HeightField3D.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>

namespace Aevrynd {
struct TerrainUphillLimitResult3D {
    bool valid{},limited{};
    float fraction{};
    Vec2 gradient{};
    unsigned cellX{},cellZ{},triangle{};
    std::size_t cellsChecked{};
};
// 只限制沿高度場上升的水平路徑；呼叫端負責接地／橋面／空中資格與碰撞交易。
// 逐格裁切實際 a/c/b、b/c/d 三角面，不使用平滑法線或端點平均坡度代替。
// 非有限、界外或超過固定預算均回傳 invalid，不能被呼叫端當成可自由通行。
inline TerrainUphillLimitResult3D LimitTerrainUphill3D(const HeightField3D& terrain,Vec3 from,
    Vec3 displacement,float maximumGrade=1.f) noexcept {
    const auto finite=[](Vec3 p){return std::isfinite(p.x)&&std::isfinite(p.y)&&std::isfinite(p.z);};
    const auto& s=terrain.Settings();
    if(!finite(from)||!finite(displacement)||!finite(s.origin)||!std::isfinite(maximumGrade)||maximumGrade<0||
       !std::isfinite(s.cellSize)||s.cellSize<=0||s.width<2||s.depth<2||s.width>1000000||s.depth>1000000||
       std::uint64_t(s.width)*s.depth!=terrain.SampleCount())return {};
    const auto coordinate=[](float origin,unsigned i,float spacing){return double(origin+float(i)*spacing);};
    const double lastX=coordinate(s.origin.x,s.width-1,s.cellSize),lastZ=coordinate(s.origin.z,s.depth-1,s.cellSize);
    const double endX=double(from.x)+displacement.x,endZ=double(from.z)+displacement.z;
    if(!std::isfinite(lastX)||!std::isfinite(lastZ)||lastX<=s.origin.x||lastZ<=s.origin.z||
       from.x<s.origin.x||from.x>lastX||from.z<s.origin.z||from.z>lastZ||
       endX<s.origin.x||endX>lastX||endZ<s.origin.z||endZ>lastZ)return {};
    // 二分尋找閉區間相交格；恰在格線上時，同時保留兩側格子。
    const auto firstCell=[&](double p,float origin,unsigned count){
        unsigned lo=0,hi=count-1;
        while(lo<hi){const unsigned mid=lo+(hi-lo)/2;
            if(coordinate(origin,mid,s.cellSize)<p)lo=mid+1;else hi=mid;}
        return lo?lo-1:0u;
    };
    const auto lastCell=[&](double p,float origin,unsigned count){
        unsigned lo=0,hi=count-1;
        while(lo<hi){const unsigned mid=lo+(hi-lo+1)/2;
            if(coordinate(origin,mid,s.cellSize)<=p)lo=mid;else hi=mid-1;}
        return (std::min)(lo,count-2);
    };
    const unsigned x0=firstCell((std::min)(double(from.x),endX),s.origin.x,s.width);
    const unsigned x1=lastCell((std::max)(double(from.x),endX),s.origin.x,s.width);
    const unsigned z0=firstCell((std::min)(double(from.z),endZ),s.origin.z,s.depth);
    const unsigned z1=lastCell((std::max)(double(from.z),endZ),s.origin.z,s.depth);
    if(x0>x1||z0>z1||std::uint64_t(x1-x0+1)*std::uint64_t(z1-z0+1)>4096)return {};
    TerrainUphillLimitResult3D result;result.valid=true;result.fraction=1;
    double earliest=2;
    for(unsigned z=z0;z<=z1;++z)for(unsigned x=x0;x<=x1;++x){
        ++result.cellsChecked;
        const double left=coordinate(s.origin.x,x,s.cellSize),right=coordinate(s.origin.x,x+1,s.cellSize);
        const double back=coordinate(s.origin.z,z,s.cellSize),front=coordinate(s.origin.z,z+1,s.cellSize);
        const double dx=right-left,dz=front-back;
        if(!std::isfinite(left)||!std::isfinite(right)||!std::isfinite(back)||!std::isfinite(front)||dx<=0||dz<=0)return {};
        double enter=0,leave=1;
        const auto clip=[&](double start,double delta,double low,double high){
            if(delta==0)return start>=low&&start<=high;
            double a=(low-start)/delta,b=(high-start)/delta;if(a>b)std::swap(a,b);
            enter=(std::max)(enter,a);leave=(std::min)(leave,b);return enter<=leave;
        };
        if(!clip(from.x,displacement.x,left,right)||!clip(from.z,displacement.z,back,front))continue;
        const double a=terrain.Height(x,z),b=terrain.Height(x+1,z),c=terrain.Height(x,z+1),d=terrain.Height(x+1,z+1);
        if(!std::isfinite(a)||!std::isfinite(b)||!std::isfinite(c)||!std::isfinite(d))return {};
        const double diagonalStart=(double(from.x)-left)/dx+(double(from.z)-back)/dz-1;
        const double diagonalDelta=double(displacement.x)/dx+double(displacement.z)/dz;
        for(unsigned triangle=0;triangle<2;++triangle){
            double begin=enter,finish=leave;
            // 第一面 u+v<=1；第二面 u+v>=1。對角線本身屬於兩個閉三角面。
            const double sign=triangle?-1.0:1.0,value=sign*diagonalStart,delta=sign*diagonalDelta;
            if(delta==0){if(value>0)continue;}
            else {const double crossing=-value/delta;if(delta>0)finish=(std::min)(finish,crossing);else begin=(std::max)(begin,crossing);}
            if(begin>finish)continue;
            const double gx=triangle?(d-c)/dx:(b-a)/dx,gz=triangle?(d-b)/dz:(c-a)/dz;
            const double grade=std::hypot(gx,gz),rise=gx*displacement.x+gz*displacement.z;
            if(!std::isfinite(grade)||!std::isfinite(rise))return {};
            if(grade<=double(maximumGrade)||rise<=0||begin>=earliest)continue;
            if(std::abs(gx)>std::numeric_limits<float>::max()||std::abs(gz)>std::numeric_limits<float>::max())return {};
            earliest=begin;result.limited=true;result.gradient={float(gx),float(gz)};
            result.cellX=x;result.cellZ=z;result.triangle=triangle;
        }
    }
    if(result.limited){
        // 即使 double 交點向 float 捨入到上側，也向零退一個可表示 fraction。
        const float at=float(std::clamp(earliest,0.0,1.0));
        result.fraction=at>0?std::nextafter(at,0.f):0.f;
    }
    return result;
}
}
