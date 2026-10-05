#pragma once
#include "HeightField3D.h"
#include <algorithm>
#include <array>
#include <cmath>

namespace Aevrynd {
enum class HeightFieldLineStatus3D {Clear,Occluded,OutOfBounds,InvalidInput,BudgetExceeded};
// 使用 CreateMesh 的實際 float 頂點與 a/c/b、b/c/d 分片，不以等距抽樣或格子最高點代替。
// 只遍歷線段包絡最多4096格，逐格裁切邊界與對角線；每片高度差為線性，端點即可證明整段。
inline HeightFieldLineStatus3D QueryHeightFieldLine3D(Vec3 start,Vec3 end,const HeightField3D& terrain) noexcept {
    using Status=HeightFieldLineStatus3D;
    const auto finite=[](Vec3 p){return std::isfinite(p.x)&&std::isfinite(p.y)&&std::isfinite(p.z);};
    const auto& grid=terrain.Settings();
    if(!finite(start)||!finite(end)||!finite(grid.origin)||grid.width<2||grid.depth<2||grid.width>1000000||grid.depth>1000000||
        !std::isfinite(grid.cellSize)||grid.cellSize<=0||terrain.SampleCount()!=std::size_t(grid.width)*grid.depth)return Status::InvalidInput;
    if(!terrain.Contains(start.x,start.z)||!terrain.Contains(end.x,end.z))return Status::OutOfBounds;
    const auto coordinate=[&](float origin,unsigned i){return double(origin+float(i)*grid.cellSize);};
    const auto index=[&](float p,float origin,unsigned count){
        unsigned low=0,high=count-1;
        while(low<high){const unsigned mid=low+(high-low+1)/2;
            if(coordinate(origin,mid)<=p)low=mid;else high=mid-1;}
        return (std::min)(low,count-2);
    };
    const unsigned x0=index((std::min)(start.x,end.x),grid.origin.x,grid.width),x1=index((std::max)(start.x,end.x),grid.origin.x,grid.width);
    const unsigned z0=index((std::min)(start.z,end.z),grid.origin.z,grid.depth),z1=index((std::max)(start.z,end.z),grid.origin.z,grid.depth);
    if(std::uint64_t(x1-x0+1)*std::uint64_t(z1-z0+1)>4096)return Status::BudgetExceeded;
    const double dx=double(end.x)-start.x,dy=double(end.y)-start.y,dz=double(end.z)-start.z;
    bool occluded{};
    for(unsigned z=z0;z<=z1;++z)for(unsigned x=x0;x<=x1;++x){
        const double cx=coordinate(grid.origin.x,x),cz=coordinate(grid.origin.z,z);
        const double nextX=coordinate(grid.origin.x,x+1),nextZ=coordinate(grid.origin.z,z+1);
        const double cellX=nextX-cx,cellZ=nextZ-cz;
        if(!std::isfinite(nextX)||!std::isfinite(nextZ)||cellX<=0||cellZ<=0)return Status::InvalidInput;
        double enter=0,exit=1;
        const auto clip=[&](double a,double delta,double low,double high){
            if(delta==0)return a>=low&&a<=high;
            double first=(low-a)/delta,last=(high-a)/delta;if(first>last)std::swap(first,last);
            enter=(std::max)(enter,first);exit=(std::min)(exit,last);return enter<=exit;
        };
        if(!clip(start.x,dx,cx,nextX)||!clip(start.z,dz,cz,nextZ))continue;
        const std::array<double,4> h{terrain.Height(x,z),terrain.Height(x+1,z),terrain.Height(x,z+1),terrain.Height(x+1,z+1)};
        for(const auto value:h)if(!std::isfinite(value))return Status::InvalidInput;
        const double u0=(double(start.x)-cx)/cellX,v0=(double(start.z)-cz)/cellZ,du=dx/cellX,dv=dz/cellZ;
        const auto blocked=[&](double t){
            const double u=std::clamp(u0+du*t,0.0,1.0),v=std::clamp(v0+dv*t,0.0,1.0);
            const double height=u+v<=1?h[0]+(h[1]-h[0])*u+(h[2]-h[0])*v:
                h[3]+(h[2]-h[3])*(1-u)+(h[1]-h[3])*(1-v);
            return double(start.y)+dy*t<=height;
        };
        occluded|=blocked(enter)||blocked(exit);
        if(du+dv!=0){const double diagonal=(1-u0-v0)/(du+dv);if(diagonal>enter&&diagonal<exit)occluded|=blocked(diagonal);}
    }
    return occluded?Status::Occluded:Status::Clear;
}
}
