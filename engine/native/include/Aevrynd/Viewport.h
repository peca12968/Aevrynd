#pragma once
#include <algorithm>
#include "Math.h"

namespace Aevrynd {
struct CanvasViewport {
    float x{},y{},width{},height{},scale{1};
    constexpr bool Contains(Vec2 point)const{return point.x>=x&&point.y>=y&&point.x<x+width&&point.y<y+height;}
};

// 維持邏輯畫布比例，將多餘區域保留為 letterbox，避免 2D 與 3D 畫面變形。
inline CanvasViewport CalculateCanvasViewport(Vec2 logicalSize,Vec2 outputSize){
    logicalSize.x=(std::max)(1.f,logicalSize.x);logicalSize.y=(std::max)(1.f,logicalSize.y);
    outputSize.x=(std::max)(1.f,outputSize.x);outputSize.y=(std::max)(1.f,outputSize.y);
    const float scale=(std::min)(outputSize.x/logicalSize.x,outputSize.y/logicalSize.y);
    const Vec2 size{logicalSize.x*scale,logicalSize.y*scale};
    return{(outputSize.x-size.x)*.5f,(outputSize.y-size.y)*.5f,size.x,size.y,scale};
}

inline Vec2 MapClientToCanvas(Vec2 point,const CanvasViewport& viewport){
    return{(point.x-viewport.x)/viewport.scale,(point.y-viewport.y)/viewport.scale};
}
}
