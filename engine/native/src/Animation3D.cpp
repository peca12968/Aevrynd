#include <Aevrynd/Animation3D.h>
#include <algorithm>
#include <cmath>

namespace Aevrynd {
namespace {
constexpr float Pi=3.14159265358979323846f;
float Lerp(float from,float to,float amount){return from+(to-from)*amount;}
float LerpAngle(float from,float to,float amount){float difference=std::fmod(to-from+Pi,Pi*2.f);if(difference<0)difference+=Pi*2.f;difference-=Pi;return from+difference*amount;}
void BuildGlobalMatrices(const Skeleton3D& skeleton,AnimationPose3D& pose,Matrix4 root){pose.globalMatrices.resize(skeleton.joints.size());for(std::size_t index=0;index<skeleton.joints.size();++index){const int parent=skeleton.joints[index].parent;const Matrix4 local=pose.localTransforms[index].Matrix();pose.globalMatrices[index]=parent<0?root*local:pose.globalMatrices[std::size_t(parent)]*local;}}
float SampleTime(const AnimationClip3D& clip,float time){if(clip.looping){time=std::fmod(time,clip.duration);if(time<0)time+=clip.duration;return time;}return Clamp(time,0.f,clip.duration);}
JointTransform3D SampleTrack(const AnimationTrack3D& track,float time,float duration,bool looping){if(track.keyframes.size()==1)return track.keyframes.front().transform;const AnimationKeyframe3D& first=track.keyframes.front();const AnimationKeyframe3D& last=track.keyframes.back();if(looping&&(time<first.time||time>=last.time)&&last.time<duration){float sample=time;if(sample<first.time)sample+=duration;const float span=duration-last.time+first.time;return InterpolateJointTransform(last.transform,first.transform,span>.000001f?(sample-last.time)/span:0.f);}auto upper=std::upper_bound(track.keyframes.begin(),track.keyframes.end(),time,[](float value,const AnimationKeyframe3D& frame){return value<frame.time;});if(upper==track.keyframes.begin())return upper->transform;if(upper==track.keyframes.end())return track.keyframes.back().transform;const AnimationKeyframe3D& to=*upper;const AnimationKeyframe3D& from=*(upper-1);const float span=to.time-from.time;return InterpolateJointTransform(from.transform,to.transform,span>.000001f?(time-from.time)/span:0.f);}
}

bool Skeleton3D::Valid()const{if(joints.empty())return false;for(std::size_t index=0;index<joints.size();++index){const int parent=joints[index].parent;if(parent>=int(index)||parent< -1)return false;}return true;}

bool AnimationClip3D::Valid(const Skeleton3D& skeleton)const{if(!skeleton.Valid()||duration<=0.f)return false;for(std::size_t trackIndex=0;trackIndex<tracks.size();++trackIndex){const AnimationTrack3D& track=tracks[trackIndex];if(track.joint>=skeleton.joints.size()||track.keyframes.empty())return false;for(std::size_t previousTrack=0;previousTrack<trackIndex;++previousTrack)if(tracks[previousTrack].joint==track.joint)return false;float previous=-1.f;for(const AnimationKeyframe3D& frame:track.keyframes){if(frame.time<0.f||frame.time>duration||frame.time<previous)return false;previous=frame.time;}}return true;}

JointTransform3D InterpolateJointTransform(JointTransform3D from,JointTransform3D to,float amount){amount=Clamp(amount,0.f,1.f);return{{Lerp(from.translation.x,to.translation.x,amount),Lerp(from.translation.y,to.translation.y,amount),Lerp(from.translation.z,to.translation.z,amount)},{LerpAngle(from.rotation.x,to.rotation.x,amount),LerpAngle(from.rotation.y,to.rotation.y,amount),LerpAngle(from.rotation.z,to.rotation.z,amount)},{Lerp(from.scale.x,to.scale.x,amount),Lerp(from.scale.y,to.scale.y,amount),Lerp(from.scale.z,to.scale.z,amount)}};}

void SampleAnimationClipInto(const Skeleton3D& skeleton,const AnimationClip3D& clip,float time,AnimationPose3D& output,Matrix4 root){if(!skeleton.Valid()){output.localTransforms.clear();output.globalMatrices.clear();return;}output.localTransforms.resize(skeleton.joints.size());for(std::size_t index=0;index<skeleton.joints.size();++index)output.localTransforms[index]=skeleton.joints[index].bindTransform;if(clip.Valid(skeleton)){time=SampleTime(clip,time);for(const AnimationTrack3D& track:clip.tracks)output.localTransforms[track.joint]=SampleTrack(track,time,clip.duration,clip.looping);}BuildGlobalMatrices(skeleton,output,root);}

AnimationPose3D SampleAnimationClip(const Skeleton3D& skeleton,const AnimationClip3D& clip,float time,Matrix4 root){AnimationPose3D pose;SampleAnimationClipInto(skeleton,clip,time,pose,root);return pose;}

void BlendAnimationPosesInto(const Skeleton3D& skeleton,const AnimationPose3D& from,const AnimationPose3D& to,float amount,AnimationPose3D& output,Matrix4 root){if(!skeleton.Valid()){output.localTransforms.clear();output.globalMatrices.clear();return;}output.localTransforms.resize(skeleton.joints.size());for(std::size_t index=0;index<skeleton.joints.size();++index){const JointTransform3D left=index<from.localTransforms.size()?from.localTransforms[index]:skeleton.joints[index].bindTransform;const JointTransform3D right=index<to.localTransforms.size()?to.localTransforms[index]:skeleton.joints[index].bindTransform;output.localTransforms[index]=InterpolateJointTransform(left,right,amount);}BuildGlobalMatrices(skeleton,output,root);}

AnimationPose3D BlendAnimationPoses(const Skeleton3D& skeleton,const AnimationPose3D& from,const AnimationPose3D& to,float amount,Matrix4 root){AnimationPose3D pose;BlendAnimationPosesInto(skeleton,from,to,amount,pose,root);return pose;}

void AnimationPlayer3D::Play(const AnimationClip3D& clip,float blendDuration,bool restart){if(current_==&clip&&!restart)return;previous_=current_;previousTime_=currentTime_;current_=&clip;currentTime_=0;blendDuration_=(std::max)(0.f,blendDuration);blendRemaining_=previous_?blendDuration_:0.f;}
void AnimationPlayer3D::Update(float deltaTime){deltaTime=(std::max)(0.f,deltaTime);currentTime_+=deltaTime;previousTime_+=deltaTime;blendRemaining_=(std::max)(0.f,blendRemaining_-deltaTime);if(blendRemaining_<=0)previous_=nullptr;}
float AnimationPlayer3D::BlendAmount()const{return !previous_||blendDuration_<=0.f?1.f:1.f-blendRemaining_/blendDuration_;}
void AnimationPlayer3D::EvaluateInto(const Skeleton3D& skeleton,AnimationPose3D& output,Matrix4 root)const{if(!current_){const AnimationClip3D bindClip;SampleAnimationClipInto(skeleton,bindClip,0,output,root);return;}if(!previous_){SampleAnimationClipInto(skeleton,*current_,currentTime_,output,root);return;}SampleAnimationClipInto(skeleton,*current_,currentTime_,currentPoseScratch_,root);SampleAnimationClipInto(skeleton,*previous_,previousTime_,previousPoseScratch_,root);BlendAnimationPosesInto(skeleton,previousPoseScratch_,currentPoseScratch_,BlendAmount(),output,root);}

AnimationPose3D AnimationPlayer3D::Evaluate(const Skeleton3D& skeleton,Matrix4 root)const{AnimationPose3D pose;EvaluateInto(skeleton,pose,root);return pose;}
}
