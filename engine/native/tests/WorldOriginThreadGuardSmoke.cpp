#include <Aevrynd/WorldOriginRuntime3D.h>
#include <atomic>
#include <iostream>
#include <thread>
using namespace Aevrynd;

struct RejectingParticipant final:IWorldOriginParticipant3D {bool PrepareWorldOriginRebase3D(const WorldOriginRebaseEvent3D&)override{return false;}void CommitWorldOriginRebase3D(const WorldOriginRebaseEvent3D&)noexcept override{}void AbortWorldOriginRebase3D()noexcept override{}};
int main(){
 DrawList draw;draw.SetOriginGeneration3D(1);draw.MeshInstance({1},CreateTranslationMatrix({64,0,0}));const auto frozen=draw.FreezeMeshInstances3D();draw.SetStaticMeshSubmission3D(frozen);const auto revision=draw.StaticMeshSubmissionRevision3D();
 WorldOriginRuntimeParticipant3D runtime({nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,&draw});const WorldOriginRebaseEvent3D event{{{},1},{{{1,0,0},{}},2},{32,0,0}};std::atomic_bool foreignAccepted{true};std::jthread worker([&]{foreignAccepted=runtime.PrepareWorldOriginRebase3D(event);runtime.CommitWorldOriginRebase3D(event);runtime.AbortWorldOriginRebase3D();});worker.join();if(foreignAccepted||draw.OriginGeneration3D()!=1||draw.CameraCut3D()||draw.StaticMeshSubmissionRevision3D()!=revision)return 1;
 RejectingParticipant reject;WorldOriginCoordinator3D coordinator;if(!coordinator.Register(runtime)||!coordinator.Register(reject)||!coordinator.Request({{1,0,0},{}})||coordinator.ApplyFrameBoundary())return 2;if(draw.OriginGeneration3D()!=1||draw.CameraCut3D()||draw.StaticMeshSubmissionRevision3D()!=revision||coordinator.Origin().generation!=1)return 3;
 std::cout<<"world origin owner-thread and atomic abort passed\n";
}
