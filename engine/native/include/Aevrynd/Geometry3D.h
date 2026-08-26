#pragma once
#include "Camera.h"
#include <algorithm>
#include <cmath>
#include <limits>

namespace Aevrynd {
struct Ray3D {Vec3 origin{},direction{0,0,1};};
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

// Moller-Trumbore 測試回傳距離、重心座標與朝向射線的表面法線。
inline RayTriangleHit3D RayIntersectsTriangle(const Ray3D& ray,Vec3 a,Vec3 b,Vec3 c,float maximumDistance=std::numeric_limits<float>::infinity(),bool cullBackfaces=false){
    const Vec3 edgeB=b-a,edgeC=c-a,cross=Cross(ray.direction,edgeC);const float determinant=Dot(edgeB,cross);if(cullBackfaces?determinant<=.000001f:std::abs(determinant)<=.000001f)return{};const float inverse=1.f/determinant;const Vec3 offset=ray.origin-a;const float barycentricB=Dot(offset,cross)*inverse;if(barycentricB<0||barycentricB>1)return{};const Vec3 side=Cross(offset,edgeB);const float barycentricC=Dot(ray.direction,side)*inverse;if(barycentricC<0||barycentricB+barycentricC>1)return{};const float distance=Dot(edgeC,side)*inverse;if(distance<0||distance>maximumDistance)return{};Vec3 normal=Normalize(Cross(edgeB,edgeC));if(Dot(normal,ray.direction)>0)normal=normal*-1.f;return{distance,{1.f-barycentricB-barycentricC,barycentricB,barycentricC},normal};
}
}
