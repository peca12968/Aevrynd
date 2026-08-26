#pragma once
#include "Math.h"
#include <algorithm>
#include <cmath>

namespace Aevrynd {
struct Camera2D {
    Vec2 position{640.f,360.f};
    Vec2 viewport{1280.f,720.f};
    float zoom{1.f};
    float rotation{};
};

struct CameraRigSettings2D {
    float positionResponse{5.f};
    float zoomResponse{6.f};
    float rotationResponse{6.f};
    float minimumZoom{.5f};
    float maximumZoom{2.f};
};

class CameraRig2D {
public:
    explicit CameraRig2D(CameraRigSettings2D settings={}):settings_(settings){Reset({1280,720});}
    void Reset(Vec2 viewport){Reset(viewport,viewport*.5f);}
    void Reset(Vec2 viewport,Vec2 position){camera_.viewport={(std::max)(1.f,viewport.x),(std::max)(1.f,viewport.y)};camera_.position=position;camera_.zoom=1.f;camera_.rotation=0.f;}
    Camera2D Update(Vec2 target,float targetZoom,float targetRotation,float deltaTime){const float dt=Clamp(deltaTime,0.f,.1f),positionBlend=1.f-std::exp(-(std::max)(0.f,settings_.positionResponse)*dt),zoomBlend=1.f-std::exp(-(std::max)(0.f,settings_.zoomResponse)*dt),rotationBlend=1.f-std::exp(-(std::max)(0.f,settings_.rotationResponse)*dt);camera_.position=camera_.position+(target-camera_.position)*positionBlend;const float zoom=Clamp(targetZoom,(std::min)(settings_.minimumZoom,settings_.maximumZoom),(std::max)(settings_.minimumZoom,settings_.maximumZoom));camera_.zoom+= (zoom-camera_.zoom)*zoomBlend;camera_.rotation+=std::remainder(targetRotation-camera_.rotation,6.283185307f)*rotationBlend;return camera_;}
    void SetCamera(Camera2D camera){camera.viewport={(std::max)(1.f,camera.viewport.x),(std::max)(1.f,camera.viewport.y)};camera.zoom=Clamp(camera.zoom,(std::min)(settings_.minimumZoom,settings_.maximumZoom),(std::max)(settings_.minimumZoom,settings_.maximumZoom));camera_=camera;}
    const Camera2D& Camera()const{return camera_;}
    Transform2D DrawTransform(float parallax=1.f,Vec2 screenOffset={})const{const Vec2 center=camera_.viewport*.5f,viewOrigin=center+(camera_.position-center)*parallax;Transform2D transform{};transform.scale={camera_.zoom,camera_.zoom};transform.rotation=-camera_.rotation;const Vec2 transformedOrigin=TransformPoint(transform,viewOrigin);transform.translation=center-transformedOrigin+screenOffset;return transform;}
    Vec2 WorldToScreen(Vec2 world,float parallax=1.f)const{return TransformPoint(DrawTransform(parallax),world);}
    Vec2 ScreenToWorld(Vec2 screen,float parallax=1.f)const{const Vec2 center=camera_.viewport*.5f,viewOrigin=center+(camera_.position-center)*parallax,local=(screen-center)*(1.f/(std::max)(.0001f,camera_.zoom));const float cosine=std::cos(camera_.rotation),sine=std::sin(camera_.rotation);return{local.x*cosine-local.y*sine+viewOrigin.x,local.x*sine+local.y*cosine+viewOrigin.y};}
private:
    CameraRigSettings2D settings_{};
    Camera2D camera_{};
};
}
