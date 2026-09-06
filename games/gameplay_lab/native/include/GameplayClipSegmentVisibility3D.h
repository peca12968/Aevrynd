#pragma once
#include <Aevrynd/Math.h>
#include <array>
#include <cmath>

namespace GameplayLab {
enum class GameplayClipSegmentVisibility3D { Invalid, Outside, Intersects };

// 僅判斷齊次線段是否接觸 D3D 封閉裁切體積，不代表三角形或 GPU 像素遮罩。
// 不先除以 w；因此相機後方端點與跨越近裁切面的線段亦可正確處理。
inline GameplayClipSegmentVisibility3D ClassifyGameplayClipSegmentVisibility3D(
    Aevrynd::Vec4 first,Aevrynd::Vec4 second) noexcept {
    using Result=GameplayClipSegmentVisibility3D;
    const auto finite=[](Aevrynd::Vec4 p){
        return std::isfinite(p.x)&&std::isfinite(p.y)&&std::isfinite(p.z)&&std::isfinite(p.w);
    };
    if(!finite(first)||!finite(second))return Result::Invalid;
    // 先提升成 double 才做加減，避免有限 float 在平面距離計算時溢位。
    const auto planes=[](Aevrynd::Vec4 p){
        const double x=p.x,y=p.y,z=p.z,w=p.w;
        return std::array<double,6>{w+x,w-x,w+y,w-y,z,w-z};
    };
    const auto a=planes(first),b=planes(second);
    double enter=0.,leave=1.;
    for(std::size_t index=0;index<a.size();++index){
        if(a[index]<0.&&b[index]<0.)return Result::Outside;
        if(a[index]>=0.&&b[index]>=0.)continue;
        // 異側端點的分母必不為零；平行且共面的線段在上方直接保留。
        const double crossing=a[index]/(a[index]-b[index]);
        if(a[index]<0.){if(crossing>enter)enter=crossing;}
        else if(crossing<leave)leave=crossing;
        if(enter>leave)return Result::Outside;
    }
    // 相等代表僅接觸邊界，仍屬相交；w=0 的齊次原點亦按封閉體積處理。
    return Result::Intersects;
}
}
