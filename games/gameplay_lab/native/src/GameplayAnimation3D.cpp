#include "GameplayAnimation3D.h"

#include <algorithm>
#include <cmath>

using namespace Aevrynd;
namespace GameplayLab {
float GameplayPresentationDelta3D(float simulationDelta) noexcept {
    return std::isfinite(simulationDelta)&&simulationDelta>0.f?Aevrynd::Clamp(simulationDelta,.001f,.1f):1.f/60.f;
}
namespace {
FallbackHumanoidClip3D ClipId(AnimationState3D state){return FallbackHumanoidClip3D(std::uint8_t(state));}
}

bool GameplayAnimation3D::Initialize(const SkinnedModelAssetData& asset,std::string* error){
    if(error)error->clear();HumanoidSkeletonMap3D rig;FallbackHumanoidAnimationSet3D clips;
    if(!ResolveCharacterRig3D(asset,rig)||!CreateFallbackHumanoidAnimationSet3D(asset,rig,clips,error))return false;
    const AnimationBlendSample1D3D samples[]={{0.f,&clips.clips[std::size_t(FallbackHumanoidClip3D::Idle)]},{1.5f,&clips.clips[std::size_t(FallbackHumanoidClip3D::Walk)]},{4.f,&clips.clips[std::size_t(FallbackHumanoidClip3D::Run)]}};
    const AnimationGraphState3D states[]={{"FallbackLocomotion",samples}};AnimationGraph3D graph;
    if(!graph.Initialize(asset.model.skeleton,asset.model.skeletonCompatibilityId,states)){if(error)*error="Fallback locomotion graph initialization failed";return false;}
    skeleton_=asset.model.skeleton;compatibility_=asset.model.skeletonCompatibilityId;rig_=rig;clips_=std::move(clips);graph_=std::move(graph);state_=AnimationState3D::Idle;time_=0;initialized_=true;return true;
}

const AnimationClip3D& GameplayAnimation3D::Clip(AnimationState3D state)const{return clips_.clips[std::size_t(ClipId(state))];}
void GameplayAnimation3D::SetState(AnimationState3D state,bool restart){if(state_!=state||restart){state_=state;time_=0;}}
void GameplayAnimation3D::Tick(float deltaTime){if(!initialized_)return;const float dt=Clamp(std::isfinite(deltaTime)?deltaTime:0.f,0.f,.1f);time_+=dt;if(state_==AnimationState3D::Idle||state_==AnimationState3D::Walk||state_==AnimationState3D::Run){const float speed=state_==AnimationState3D::Idle?0.f:state_==AnimationState3D::Walk?1.5f:4.f;graph_.SetParameter(speed);graph_.Update(dt);}}
void GameplayAnimation3D::SynchronizeNormalizedPhase(float phase){if(initialized_&&std::isfinite(phase))time_=Clamp(phase,0.f,1.f)*Duration();}
bool GameplayAnimation3D::EvaluateInto(AnimationPose3D& output,Matrix4 root){if(!initialized_)return false;if(state_==AnimationState3D::Idle||state_==AnimationState3D::Walk||state_==AnimationState3D::Run)return graph_.EvaluateInto(skeleton_,compatibility_,output,root);SampleAnimationClipInto(skeleton_,Clip(state_),time_,output,root);return output.localTransforms.size()==skeleton_.joints.size()&&output.globalMatrices.size()==skeleton_.joints.size();}
float GameplayAnimation3D::Duration()const{return initialized_?Clip(state_).duration:0.f;}
float GameplayAnimation3D::NormalizedPhase()const{const float duration=Duration();if(duration<=0)return 0;return Clip(state_).looping?std::fmod((std::max)(0.f,time_),duration)/duration:Clamp(time_/duration,0.f,1.f);}
}
