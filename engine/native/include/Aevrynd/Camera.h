#pragma once
#include "Math.h"
#include <algorithm>
#include <cmath>

namespace Aevrynd {
enum class ProjectionMode {Perspective,Orthographic};

struct CameraProjection {
    ProjectionMode mode{ProjectionMode::Perspective};
    float verticalFieldOfView{1.04719755f};
    float orthographicHeight{20.f};
    float nearPlane{.1f};
    float farPlane{1000.f};
};

struct Camera3D {
    Vec3 position{};
    Vec3 target{0,0,1};
    Vec3 up{0,1,0};
    CameraProjection projection{};
};

struct ProjectedPoint {
    Vec2 position{};
    float depth{};
    bool visible{};
};

// 使用左手座標與 DirectX 的 0 到 1 深度範圍，讓 2D、2.5D 與 3D 共用同一套相機數學。
inline Matrix4 CreateViewMatrix(const Camera3D& camera){Vec3 forward=Normalize(camera.target-camera.position);if(Length(forward)<.0001f)forward={0,0,1};Vec3 upSource=Normalize(camera.up);if(Length(upSource)<.0001f||std::abs(Dot(upSource,forward))>.999f)upSource=std::abs(forward.z)<.999f?Vec3{0,0,1}:Vec3{0,1,0};Vec3 right=Normalize(Cross(upSource,forward));Vec3 up=Cross(forward,right);Matrix4 result=Matrix4::Identity();result[0][0]=right.x;result[0][1]=right.y;result[0][2]=right.z;result[0][3]=-Dot(right,camera.position);result[1][0]=up.x;result[1][1]=up.y;result[1][2]=up.z;result[1][3]=-Dot(up,camera.position);result[2][0]=forward.x;result[2][1]=forward.y;result[2][2]=forward.z;result[2][3]=-Dot(forward,camera.position);return result;}

inline Matrix4 CreateProjectionMatrix(const CameraProjection& projection,float aspectRatio){const float nearPlane=(std::max)(.001f,projection.nearPlane);const float farPlane=(std::max)(nearPlane+.001f,projection.farPlane);const float aspect=(std::max)(.001f,aspectRatio);Matrix4 result{};if(projection.mode==ProjectionMode::Orthographic){const float height=(std::max)(.001f,projection.orthographicHeight);const float width=height*aspect;result[0][0]=2.f/width;result[1][1]=2.f/height;result[2][2]=1.f/(farPlane-nearPlane);result[2][3]=-nearPlane/(farPlane-nearPlane);result[3][3]=1.f;}else{const float fieldOfView=Clamp(projection.verticalFieldOfView,.01f,3.13f);const float yScale=1.f/std::tan(fieldOfView*.5f);result[0][0]=yScale/aspect;result[1][1]=yScale;result[2][2]=farPlane/(farPlane-nearPlane);result[2][3]=-nearPlane*farPlane/(farPlane-nearPlane);result[3][2]=1.f;}return result;}

inline Matrix4 CreateViewProjectionMatrix(const Camera3D& camera,float aspectRatio){return CreateProjectionMatrix(camera.projection,aspectRatio)*CreateViewMatrix(camera);}

// 使用視圖空間平面測試球體，讓透視與正交相機共用保守且不漏畫的物件裁切。
inline bool SphereIntersectsFrustum(Vec3 center,float radius,const Camera3D& camera,float aspectRatio){
    const Vec3 viewCenter=TransformPoint(CreateViewMatrix(camera),center);const float safeRadius=(std::max)(0.f,std::abs(radius));
    const float nearPlane=(std::max)(.001f,camera.projection.nearPlane),farPlane=(std::max)(nearPlane+.001f,camera.projection.farPlane),aspect=(std::max)(.001f,aspectRatio);
    if(viewCenter.z+safeRadius<nearPlane||viewCenter.z-safeRadius>farPlane)return false;
    if(camera.projection.mode==ProjectionMode::Orthographic){const float halfHeight=(std::max)(.001f,camera.projection.orthographicHeight)*.5f,halfWidth=halfHeight*aspect;return std::abs(viewCenter.x)<=halfWidth+safeRadius&&std::abs(viewCenter.y)<=halfHeight+safeRadius;}
    const float tangentY=std::tan(Clamp(camera.projection.verticalFieldOfView,.01f,3.13f)*.5f),tangentX=tangentY*aspect;
    const float horizontalAllowance=safeRadius*std::sqrt(1.f+tangentX*tangentX),verticalAllowance=safeRadius*std::sqrt(1.f+tangentY*tangentY);
    return viewCenter.z*tangentX-std::abs(viewCenter.x)+horizontalAllowance>=0.f&&viewCenter.z*tangentY-std::abs(viewCenter.y)+verticalAllowance>=0.f;
}

inline ProjectedPoint ProjectToViewport(Vec3 worldPosition,const Camera3D& camera,Vec2 viewport){Vec4 clip=CreateViewProjectionMatrix(camera,viewport.x/(std::max)(1.f,viewport.y))*Vec4{worldPosition.x,worldPosition.y,worldPosition.z,1.f};if(std::abs(clip.w)<.00001f)return{};const float inverseW=1.f/clip.w;Vec3 normalized{clip.x*inverseW,clip.y*inverseW,clip.z*inverseW};return{{(normalized.x*.5f+.5f)*viewport.x,(.5f-normalized.y*.5f)*viewport.y},normalized.z,clip.w>0&&normalized.x>=-1&&normalized.x<=1&&normalized.y>=-1&&normalized.y<=1&&normalized.z>=0&&normalized.z<=1};}

inline Camera3D MakeFirstPersonCamera(Vec3 position,float yaw,float pitch,CameraProjection projection={}){const float safePitch=Clamp(pitch,-1.553343f,1.553343f);const float cosine=std::cos(safePitch);Vec3 forward{std::sin(yaw)*cosine,std::sin(safePitch),std::cos(yaw)*cosine};return{position,position+forward,{0,1,0},projection};}

inline Camera3D MakeThirdPersonCamera(Vec3 target,float yaw,float pitch,float distance,CameraProjection projection={}){const float safePitch=Clamp(pitch,-1.553343f,1.553343f);const float cosine=std::cos(safePitch);Vec3 offset{std::sin(yaw)*cosine,std::sin(safePitch),-std::cos(yaw)*cosine};return{target+offset*(std::max)(.01f,distance),target,{0,1,0},projection};}

inline Camera3D MakeOverheadCamera(Vec3 target,float height,bool orthographic=true){CameraProjection projection{};projection.mode=orthographic?ProjectionMode::Orthographic:ProjectionMode::Perspective;projection.orthographicHeight=(std::max)(1.f,height*1.5f);return{{target.x,target.y+(std::max)(.01f,height),target.z},target,{0,0,1},projection};}

inline Camera3D MakeIsometricCamera(Vec3 target,float distance,float orthographicHeight){CameraProjection projection{};projection.mode=ProjectionMode::Orthographic;projection.orthographicHeight=(std::max)(1.f,orthographicHeight);Vec3 direction=Normalize(Vec3{1.f,1.f,-1.f});return{target+direction*(std::max)(.01f,distance),target,{0,1,0},projection};}
}
