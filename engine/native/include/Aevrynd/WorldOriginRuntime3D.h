#pragma once
#include "CharacterController3D.h"
#include "Navigation3D.h"
#include "PhysicsWorld3D.h"
#include "SceneECS3D.h"
#include "Vegetation3D.h"
#include "WorldOrigin3D.h"

namespace Aevrynd {
struct WorldOriginRuntimeBindings3D {SceneECSRegistry3D* scene{};ChunkedVegetationWorld3D* vegetation{};CollisionWorld3D* collision{};PhysicsWorld3D* physics{};NavigationGrid3D* navigation{};CharacterController3D* controller{};Camera3D* camera{};DrawList* draw{};};
class WorldOriginRuntimeParticipant3D final:public IWorldOriginParticipant3D {
public:
 explicit WorldOriginRuntimeParticipant3D(WorldOriginRuntimeBindings3D bindings):bindings_(bindings){}
 bool PrepareWorldOriginRebase3D(const WorldOriginRebaseEvent3D& event)override;
 void CommitWorldOriginRebase3D(const WorldOriginRebaseEvent3D& event)noexcept override;
 void AbortWorldOriginRebase3D()noexcept override{prepared_=false;}
private:WorldOriginRuntimeBindings3D bindings_{};bool prepared_{};
};
}
