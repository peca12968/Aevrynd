#include <Aevrynd/WorldOriginRuntime3D.h>
#include <cmath>

namespace Aevrynd {
namespace {bool Finite(Vec3 value){return std::isfinite(value.x)&&std::isfinite(value.y)&&std::isfinite(value.z);}}
bool WorldOriginRuntimeParticipant3D::PrepareWorldOriginRebase3D(const WorldOriginRebaseEvent3D& event){
 if(std::this_thread::get_id()!=owner_||prepared_||!event||!Finite(event.localDelta))return false;
 const Vec3 delta=event.localDelta;
 if((bindings_.scene&&!bindings_.scene->CanRebaseOrigin(delta))||(bindings_.vegetation&&!bindings_.vegetation->CanRebaseOrigin(delta))||(bindings_.collision&&!bindings_.collision->CanRebaseOrigin(delta))||(bindings_.physics&&!bindings_.physics->CanRebaseOrigin(delta))||(bindings_.navigation&&!bindings_.navigation->CanRebaseOrigin(delta))||(bindings_.controller&&!bindings_.controller->CanRebaseOrigin(delta)))return false;
 if(bindings_.camera&&(!Finite(bindings_.camera->position-delta)||!Finite(bindings_.camera->target-delta)))return false;
 if(bindings_.draw&&!bindings_.draw->PrepareWorldOriginShift3D(delta,event.current.generation))return false;
 if(bindings_.engine&&!bindings_.engine->PrepareRebaseReflectionProbeCaptures3D(delta,event.current.generation)){if(bindings_.draw)bindings_.draw->AbortPreparedWorldOriginShift3D();return false;}
 prepared_=true;return true;
}
void WorldOriginRuntimeParticipant3D::CommitWorldOriginRebase3D(const WorldOriginRebaseEvent3D& event)noexcept{
 if(std::this_thread::get_id()!=owner_||!prepared_)return;
 const Vec3 delta=event.localDelta;
 if(bindings_.scene)bindings_.scene->RebaseOrigin(delta);if(bindings_.vegetation)bindings_.vegetation->RebaseOrigin(delta);if(bindings_.collision)bindings_.collision->RebaseOrigin(delta);if(bindings_.physics)bindings_.physics->RebaseOrigin(delta);if(bindings_.navigation)bindings_.navigation->RebaseOrigin(delta);if(bindings_.controller)bindings_.controller->RebaseOrigin(delta);if(bindings_.camera){bindings_.camera->position=bindings_.camera->position-delta;bindings_.camera->target=bindings_.camera->target-delta;}if(bindings_.draw)bindings_.draw->CommitPreparedWorldOriginShift3D();if(bindings_.engine)bindings_.engine->CommitPreparedReflectionProbeCaptureRebase3D();prepared_=false;
}
void WorldOriginRuntimeParticipant3D::AbortWorldOriginRebase3D()noexcept{if(std::this_thread::get_id()!=owner_)return;if(bindings_.draw)bindings_.draw->AbortPreparedWorldOriginShift3D();if(bindings_.engine)bindings_.engine->AbortPreparedReflectionProbeCaptureRebase3D();prepared_=false;}
}
