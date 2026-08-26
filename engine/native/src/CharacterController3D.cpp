#include "Aevrynd/CharacterController3D.h"
#include "Aevrynd/Geometry3D.h"
#include <algorithm>
#include <cmath>

namespace Aevrynd {
void CharacterController3D::Reset(Vec3 feetPosition){position_=feetPosition;velocity_={};grounded_=false;stats_={};}

void CharacterController3D::Step(CharacterControl3D control,std::span<const BoxCollider3D> colliders,float deltaTime){
    const float dt=Clamp(deltaTime,0.f,.1f);Vec3 movement{control.movement.x,0,control.movement.z};const float length=Length(movement);if(length>1)movement=movement*(1.f/length);velocity_.x=movement.x*settings_.moveSpeed;velocity_.z=movement.z*settings_.moveSpeed;
    if(control.jump&&grounded_){velocity_.y=settings_.jumpSpeed;grounded_=false;++stats_.jumps;}velocity_.y=(std::max)(-settings_.maximumFallSpeed,velocity_.y-settings_.gravity*dt);const bool wasGrounded=grounded_;grounded_=false;const Vec3 extents=Extents();
    // 分軸解算靜態 AABB，避免牆面碰撞同時抵銷未受阻擋方向的移動。
    for(unsigned axis:{1u,0u,2u}){float* coordinate=axis==0?&position_.x:axis==1?&position_.y:&position_.z;float* speed=axis==0?&velocity_.x:axis==1?&velocity_.y:&velocity_.z;const float displacement=*speed*dt;if(std::abs(displacement)<.000001f)continue;*coordinate+=displacement;for(const BoxCollider3D& collider:colliders){Vec3 center=Center(),delta=center-collider.center;if(axis!=1&&center.y-extents.y>=collider.center.y+collider.extents.y-.001f)continue;if(std::abs(delta.x)>=extents.x+collider.extents.x-.0001f||std::abs(delta.y)>=extents.y+collider.extents.y-.0001f||std::abs(delta.z)>=extents.z+collider.extents.z-.0001f)continue;const float colliderCoordinate=axis==0?collider.center.x:axis==1?collider.center.y:collider.center.z,colliderExtent=axis==0?collider.extents.x:axis==1?collider.extents.y:collider.extents.z,characterExtent=axis==0?extents.x:axis==1?extents.y:extents.z;float corrected=displacement>0?colliderCoordinate-colliderExtent-characterExtent:colliderCoordinate+colliderExtent+characterExtent;if(axis==1)corrected-=extents.y;*coordinate=corrected;if(axis==1&&displacement<=0)grounded_=true;*speed=0;++stats_.collisions;}}
    if(grounded_&&!wasGrounded)++stats_.landings;++stats_.steps;
}

Camera3D ConstrainThirdPersonCamera(Camera3D camera,Vec3 target,std::span<const BoxCollider3D> colliders,float padding){const Vec3 offset=camera.position-target;const float desiredDistance=Length(offset);if(desiredDistance<=.001f)return camera;const Ray3D ray=MakeRay3D(target,offset);float nearest=desiredDistance;for(const BoxCollider3D& collider:colliders){float distance{};if(RayIntersectsAabb(ray,collider.center,collider.extents,nearest,&distance))nearest=(std::min)(nearest,(std::max)(.1f,distance-(std::max)(0.f,padding)));}camera.position=target+ray.direction*nearest;camera.target=target;return camera;}
}
