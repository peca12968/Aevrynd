#pragma once
#include "HeightField3D.h"
#include <array>
#include <cmath>
#include <limits>
namespace Aevrynd {
// 沿高度場實際三角形裁切平面水面，保持與渲染／碰撞查詢相同的岸線。
inline Mesh3D CreateHeightFieldWaterMesh3D(const HeightField3D& terrain,float waterHeight,float textureScale=.25f) {
    Mesh3D mesh;
    if(!std::isfinite(waterHeight) || !std::isfinite(textureScale) || textureScale<=0 ||
       terrain.SampleCount()>1048576)return mesh;
    const auto& settings=terrain.Settings();
    const double endX=double(settings.origin.x)+double(settings.width-1)*settings.cellSize;
    const double endZ=double(settings.origin.z)+double(settings.depth-1)*settings.cellSize;
    const double maximum=(std::max)({std::abs(double(settings.origin.x)),std::abs(double(settings.origin.z)),std::abs(endX),std::abs(endZ)});
    if(!std::isfinite(maximum) || maximum*textureScale>double((std::numeric_limits<float>::max)()))return mesh;
    const auto emit=[&](Vec3 a,Vec3 b,Vec3 c) {
        const std::array<Vec3,3> input{a,b,c};
        std::array<Vec3,4> clipped{};
        unsigned count=0;
        for(unsigned edge=0;edge<3;++edge) {
            const auto from=input[edge],to=input[(edge+1)%3];
            const bool fromWet=from.y<waterHeight,toWet=to.y<waterHeight;
            if(fromWet)clipped[count++]=from;
            if(fromWet!=toWet) {
                const double amount=(double(waterHeight)-from.y)/(double(to.y)-from.y);
                clipped[count++]={float(double(from.x)+(double(to.x)-from.x)*amount),waterHeight,
                    float(double(from.z)+(double(to.z)-from.z)*amount)};
            }
        }
        if(count<3)return;
        for(unsigned triangle=1;triangle+1<count;++triangle) {
            auto first=clipped[0],second=clipped[triangle],third=clipped[triangle+1];
            first.y=second.y=third.y=waterHeight;
            if(Cross(second-first,third-first).y<=.00000001f)continue;
            const auto base=uint32_t(mesh.vertices.size());
            for(const auto p:{first,second,third})
                mesh.vertices.push_back({p.x,p.y,p.z,PackNormalSnorm8({0,1,0}),0xffffffffu,p.x*textureScale,p.z*textureScale});
            mesh.indices.insert(mesh.indices.end(),{base,base+1,base+2});
        }
    };
    for(unsigned z=0;z+1<settings.depth;++z)for(unsigned x=0;x+1<settings.width;++x) {
        const auto a=terrain.SamplePosition(x,z),b=terrain.SamplePosition(x+1,z);
        const auto c=terrain.SamplePosition(x,z+1),d=terrain.SamplePosition(x+1,z+1);
        emit(a,c,b);emit(b,c,d);
    }
    return mesh;
}
}
