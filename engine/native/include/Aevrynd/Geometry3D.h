#pragma once
#include "Camera.h"
#include "GeometryNormals3D.h"
#include <algorithm>
#include <cmath>
#include <limits>

namespace Aevrynd {
struct Ray3D {Vec3 origin{},direction{0,0,1};};
// 有限線段以 double slab 裁切；包括零長度與微小位移，不略去末端或 Normalize epsilon。
inline bool SegmentIntersectsAabb3D(Vec3 start,Vec3 end,Vec3 center,Vec3 extents) noexcept {
    const auto finite=[](Vec3 p){return std::isfinite(p.x)&&std::isfinite(p.y)&&std::isfinite(p.z);};
    if(!finite(start)||!finite(end)||!finite(center)||!finite(extents)||extents.x<0||extents.y<0||extents.z<0)return true;
    double enter=0,exit=1;
    const auto clip=[&](double a,double b,double c,double extent){
        const double delta=b-a,low=c-extent,high=c+extent;
        if(delta==0)return a>=low&&a<=high;
        double first=(low-a)/delta,last=(high-a)/delta;if(first>last)std::swap(first,last);
        enter=(std::max)(enter,first);exit=(std::min)(exit,last);return enter<=exit;
    };
    return clip(start.x,end.x,center.x,extents.x)&&clip(start.y,end.y,center.y,extents.y)&&clip(start.z,end.z,center.z,extents.z);
}
struct RayTriangleHit3D {
    float distance{std::numeric_limits<float>::infinity()};
    Vec3 barycentric{},normal{};
    explicit operator bool()const{return std::isfinite(distance);}
};

inline Ray3D MakeRay3D(Vec3 origin,Vec3 direction){return{origin,Normalize(direction)};}

// 畫面座標可由透視與正交相機產生一致的世界射線，供選取、武器與相機碰撞共用。
inline Ray3D ScreenPointToRay(const Camera3D& camera,Vec2 viewport,Vec2 screenPoint){
    const float width=(std::max)(1.f,viewport.x),height=(std::max)(1.f,viewport.y),aspect=width/height;
    const float normalizedX=screenPoint.x/width*2.f-1.f,normalizedY=1.f-screenPoint.y/height*2.f;
    Vec3 forward=Normalize(camera.target-camera.position);if(Length(forward)<.0001f)forward={0,0,1};Vec3 upSource=Normalize(camera.up);if(Length(upSource)<.0001f||std::abs(Dot(upSource,forward))>.999f)upSource=std::abs(forward.z)<.999f?Vec3{0,0,1}:Vec3{0,1,0};const Vec3 right=Normalize(Cross(upSource,forward)),up=Cross(forward,right);
    if(camera.projection.mode==ProjectionMode::Orthographic){const float halfHeight=(std::max)(.001f,camera.projection.orthographicHeight)*.5f;return{camera.position+right*(normalizedX*halfHeight*aspect)+up*(normalizedY*halfHeight),forward};}
    const float halfHeight=std::tan(Clamp(camera.projection.verticalFieldOfView,.01f,3.13f)*.5f);return MakeRay3D(camera.position,forward+right*(normalizedX*halfHeight*aspect)+up*(normalizedY*halfHeight));
}

// Slab 測試保留射線進入距離，可在精確三角形測試前快速排除大多數物件。
inline bool RayIntersectsAabb(const Ray3D& ray,Vec3 center,Vec3 extents,float maximumDistance=std::numeric_limits<float>::infinity(),float* entryDistance=nullptr){
    float nearDistance=0,farDistance=(std::max)(0.f,maximumDistance);const Vec3 minimum=center-extents,maximum=center+extents;
    const auto axis=[&](float origin,float direction,float low,float high){if(std::abs(direction)<.000001f)return origin>=low&&origin<=high;float first=(low-origin)/direction,second=(high-origin)/direction;if(first>second)std::swap(first,second);nearDistance=(std::max)(nearDistance,first);farDistance=(std::min)(farDistance,second);return nearDistance<=farDistance;};
    if(!axis(ray.origin.x,ray.direction.x,minimum.x,maximum.x)||!axis(ray.origin.y,ray.direction.y,minimum.y,maximum.y)||!axis(ray.origin.z,ray.direction.z,minimum.z,maximum.z))return false;if(entryDistance)*entryDistance=nearDistance;return true;
}

// 保留原 Moller-Trumbore 接受範圍；先提升原 float 座標再求邊，避免接縫上的單精度漏判。
// 這是中間計算精度改良，並非任意三角形的 watertight／精確述詞保證。
inline RayTriangleHit3D RayIntersectsTriangle(const Ray3D& ray,Vec3 a,Vec3 b,Vec3 c,float maximumDistance=std::numeric_limits<float>::infinity(),bool cullBackfaces=false){
    const auto finite=[](Vec3 value){return std::isfinite(value.x)&&std::isfinite(value.y)&&std::isfinite(value.z);};
    if(!finite(ray.origin)||!finite(ray.direction)||!finite(a)||!finite(b)||!finite(c)||
        std::isnan(maximumDistance)||maximumDistance<0.f)return{};
    struct PreciseVector{double x,y,z;};
    const auto promote=[](Vec3 value){return PreciseVector{double(value.x),double(value.y),double(value.z)};};
    const auto subtract=[](PreciseVector left,PreciseVector right){return PreciseVector{left.x-right.x,left.y-right.y,left.z-right.z};};
    const auto crossProduct=[](PreciseVector left,PreciseVector right){return PreciseVector{left.y*right.z-left.z*right.y,left.z*right.x-left.x*right.z,left.x*right.y-left.y*right.x};};
    const auto dotProduct=[](PreciseVector left,PreciseVector right){return left.x*right.x+left.y*right.y+left.z*right.z;};
    const auto vertex=promote(a),direction=promote(ray.direction);
    const auto edgeB=subtract(promote(b),vertex),edgeC=subtract(promote(c),vertex);
    const auto cross=crossProduct(direction,edgeC);
    const double determinant=dotProduct(edgeB,cross),originalGate=double(.000001f);
    if(cullBackfaces?determinant<=originalGate:std::abs(determinant)<=originalGate)return{};
    const double inverse=1./determinant;
    const auto offset=subtract(promote(ray.origin),vertex);
    const double barycentricB=dotProduct(offset,cross)*inverse;
    if(barycentricB<0.||barycentricB>1.)return{};
    const auto side=crossProduct(offset,edgeB);
    const double barycentricC=dotProduct(direction,side)*inverse;
    if(barycentricC<0.||barycentricB+barycentricC>1.)return{};
    const double distance=dotProduct(edgeC,side)*inverse;
    if(!std::isfinite(distance)||distance<0.||distance>double(maximumDistance)||
        distance>double((std::numeric_limits<float>::max)()))return{};
    Vec3 normal{};
    if(!TryComputeTriangleNormal(a,b,c,normal))return{};
    if(dotProduct(promote(normal),direction)>0.)normal=normal*-1.f;
    return{float(distance),{float(1.-(barycentricB+barycentricC)),float(barycentricB),float(barycentricC)},normal};
}
}
