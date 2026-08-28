#include <Aevrynd/AnimationGraph3D.h>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>
#include <atomic>
#include <cstdlib>
#include <new>
using namespace Aevrynd;
namespace {std::atomic<std::uint64_t> allocationCount{};}
void* operator new(std::size_t size){allocationCount.fetch_add(1,std::memory_order_relaxed);if(void* p=std::malloc(size))return p;throw std::bad_alloc();}
void operator delete(void* p)noexcept{std::free(p);}void operator delete(void* p,std::size_t)noexcept{std::free(p);}
namespace {
bool Near(float a,float b,float e=.001f){return std::abs(a-b)<=e;}
AnimationClip3D Clip(float x,float duration=1){AnimationClip3D clip;clip.duration=duration;clip.looping=false;AnimationTrack3D track;track.joint=0;AnimationKeyframe3D first,last;first.time=0;first.transform.translation.x=x;last.time=duration;last.transform.translation.x=x;track.keyframes={first,last};clip.tracks.push_back(std::move(track));return clip;}
std::uint64_t HashPose(const AnimationPose3D& pose){std::uint64_t h=14695981039346656037ull;for(const auto& t:pose.localTransforms){const float v[]={t.translation.x,t.translation.y,t.translation.z,t.rotation.x,t.rotation.y,t.rotation.z,t.scale.x,t.scale.y,t.scale.z,t.quaternion.x,t.quaternion.y,t.quaternion.z,t.quaternion.w};for(float f:v)for(unsigned char b:std::span(reinterpret_cast<const unsigned char*>(&f),sizeof(f))){h^=b;h*=1099511628211ull;}}return h;}
}
int main(){
 Skeleton3D skeleton{{{-1,{}},{0,{}}}};const SkeletonCompatibilityId3D id{7};
 auto idle=Clip(0),walk=Clip(1),run=Clip(3),upper=Clip(0);upper.tracks[0].joint=1;upper.tracks[0].keyframes[0].transform.translation.y=2;upper.tracks[0].keyframes[1].transform.translation.y=2;
 const AnimationBlendSample1D3D samples[]={{0,&idle},{1,&walk},{3,&run}};const AnimationGraphState3D states[]={{"Locomotion",samples},{"Run",std::span(samples+2,1)}};
 AnimationGraph3D graph;AnimationGraphError3D error;AnimationPose3D pose;if(!graph.Initialize(skeleton,id,states,&error))return 1;
 graph.SetParameter(0);if(!graph.EvaluateInto(skeleton,id,pose)||!Near(pose.localTransforms[0].translation.x,0))return 2;
 graph.SetParameter(2);if(!graph.EvaluateInto(skeleton,id,pose)||!Near(pose.localTransforms[0].translation.x,2))return 3;
 graph.SetParameter(99);if(!graph.EvaluateInto(skeleton,id,pose)||!Near(pose.localTransforms[0].translation.x,3))return 4;
 std::array<float,2> mask{0,1};if(!graph.SetUpperBodyLayer({&upper,mask,.5f},&error)||!graph.EvaluateInto(skeleton,id,pose)||!Near(pose.localTransforms[1].translation.y,1))return 5;
 // 初始化後修改來源，圖表仍使用自己擁有的資料。
 walk.tracks[0].keyframes[0].transform.translation.x=99;graph.ClearUpperBodyLayer();graph.SetParameter(1);if(!graph.EvaluateInto(skeleton,id,pose)||!Near(pose.localTransforms[0].translation.x,1))return 6;
 std::uint64_t hashes[2]{};for(int pass=0;pass<2;++pass){AnimationGraph3D deterministic;if(!deterministic.Initialize(skeleton,id,states))return 7;AnimationPose3D p;if(!deterministic.EvaluateInto(skeleton,id,p))return 8;const auto* local=p.localTransforms.data();const auto* global=p.globalMatrices.data();std::uint64_t h=14695981039346656037ull;const auto memory=deterministic.MemoryStats().retainedCapacityBytes,allocations=allocationCount.load();for(int frame=0;frame<10000;++frame){deterministic.SetParameter(float(frame%300)/100);deterministic.Update(1.f/60);if(!deterministic.EvaluateInto(skeleton,id,p)||deterministic.MemoryStats().retainedCapacityBytes!=memory||p.localTransforms.data()!=local||p.globalMatrices.data()!=global)return 9;h^=HashPose(p);h*=1099511628211ull;}if(allocationCount.load()!=allocations)return 10;hashes[pass]=h;}if(hashes[0]!=hashes[1])return 11;
 // 不同週期樣本共享正規化相位。
 auto phaseA=Clip(0,1),phaseB=Clip(0,2);phaseA.tracks[0].keyframes[1].transform.translation.x=1;phaseB.tracks[0].keyframes[1].transform.translation.x=1;const AnimationBlendSample1D3D phaseSamples[]={{0,&phaseA},{1,&phaseB}};AnimationGraph3D phaseGraph;if(!phaseGraph.Initialize(skeleton,id,std::array<AnimationGraphState3D,1>{{{"Phase",phaseSamples}}}))return 12;phaseGraph.SetParameter(.5f);phaseGraph.Update(.75f);if(!phaseGraph.EvaluateInto(skeleton,id,pose)||!Near(pose.localTransforms[0].translation.x,.5f))return 13;
 graph.SetParameter(0);graph.EvaluateInto(skeleton,id,pose);if(!graph.SetState(1,1,false))return 12;graph.Update(.4f);graph.EvaluateInto(skeleton,id,pose);const float before=pose.localTransforms[0].translation.x;if(!graph.SetState(0,1,false)||!graph.EvaluateInto(skeleton,id,pose)||!Near(before,pose.localTransforms[0].translation.x))return 13;
 const AnimationBlendSample1D3D duplicate[]={{0,&idle},{0,&run}};const std::array<AnimationGraphState3D,1> bad{{{"bad",duplicate}}};if(graph.Initialize(skeleton,id,bad,&error)||error!=AnimationGraphError3D::DuplicateSample||!graph.EvaluateInto(skeleton,id,pose))return 14;
 auto nan=idle;nan.duration=(std::numeric_limits<float>::quiet_NaN)();const AnimationBlendSample1D3D badSample[]={{0,&nan}};AnimationGraph3D invalid;if(invalid.Initialize(skeleton,id,std::array<AnimationGraphState3D,1>{{{"nan",badSample}}},&error))return 15;if(graph.EvaluateInto(skeleton,{8},pose))return 16;const Quaternion3D q{0,0,0,1},negative{0,0,0,-1};if(std::abs(SlerpQuaternion3D(q,negative,.5f).w)<.999f)return 17;
 // 所有有限值與 layer 失敗都必須原子保留既有狀態。
 AnimationGraph3D partial;if(!partial.Initialize(skeleton,id,states))return 18;std::array<float,2> fullMask{1,1};if(!partial.SetUpperBodyLayer({&upper,fullMask,.5f}))return 19;partial.SetParameter(0);if(!partial.EvaluateInto(skeleton,id,pose)||!Near(pose.localTransforms[0].translation.x,0)||!Near(pose.localTransforms[1].translation.y,1))return 20;if(!partial.SetState(1,1,false))return 21;partial.Update(.4f);partial.EvaluateInto(skeleton,id,pose);const auto layeredBefore=HashPose(pose);if(!partial.SetState(0,1,false)||!partial.EvaluateInto(skeleton,id,pose)||HashPose(pose)!=layeredBefore)return 22;
 const auto poseBefore=HashPose(pose);Matrix4 badRoot=Matrix4::Identity();badRoot[0][0]=(std::numeric_limits<float>::quiet_NaN)();if(graph.EvaluateInto(skeleton,id,pose,badRoot)||HashPose(pose)!=poseBefore)return 23;
 auto badTime=idle;badTime.tracks[0].keyframes[0].time=(std::numeric_limits<float>::infinity)();const AnimationBlendSample1D3D badTimeSample[]={{0,&badTime}};if(invalid.Initialize(skeleton,id,std::array<AnimationGraphState3D,1>{{{"time",badTimeSample}}}))return 19;
 auto badTransform=idle;badTransform.tracks[0].keyframes[0].transform.translation.x=(std::numeric_limits<float>::quiet_NaN)();const AnimationBlendSample1D3D badTransformSample[]={{0,&badTransform}};if(invalid.Initialize(skeleton,id,std::array<AnimationGraphState3D,1>{{{"transform",badTransformSample}}}))return 20;
 auto badSkeleton=skeleton;badSkeleton.joints[0].bindTransform.scale.x=0;if(invalid.Initialize(badSkeleton,id,states))return 21;
 std::array<float,1> wrongMask{1};if(graph.SetUpperBodyLayer({&upper,wrongMask,.5f},&error)||error!=AnimationGraphError3D::InvalidLayer)return 22;std::array<float,2> nanMask{0,(std::numeric_limits<float>::quiet_NaN)()};if(graph.SetUpperBodyLayer({&upper,nanMask,.5f},&error))return 23;
 Skeleton3D overflowSkeleton{{{-1,{}},{0,{}}}};overflowSkeleton.joints[0].bindTransform.scale={1e30f,1e30f,1e30f};overflowSkeleton.joints[1].bindTransform.scale={1e30f,1e30f,1e30f};AnimationClip3D overflowClip;overflowClip.duration=1;const AnimationBlendSample1D3D overflowSample[]={{0,&overflowClip}};AnimationGraph3D overflowGraph;if(!overflowGraph.Initialize(overflowSkeleton,{9},std::array<AnimationGraphState3D,1>{{{"overflow",overflowSample}}}))return 24;AnimationPose3D sentinel=pose;const auto sentinelHash=HashPose(sentinel);if(overflowGraph.EvaluateInto(overflowSkeleton,{9},sentinel)||HashPose(sentinel)!=sentinelHash)return 25;
 graph.Update((std::numeric_limits<float>::infinity)());if(!graph.EvaluateInto(skeleton,id,pose))return 26;
 const auto retained=graph.MemoryStats().retainedCapacityBytes;graph.Reset();const auto reset=graph.MemoryStats();if(!reset.reclaimableCapacityBytes||reset.retainedCapacityBytes<retained)return 27;const auto released=graph.TrimUnusedMemory();if(!released||graph.MemoryStats().retainedCapacityBytes>=reset.retainedCapacityBytes)return 28;
 std::cout<<"deterministicHash="<<hashes[0]<<" retained="<<retained<<" released="<<released<<'\n';return 0;
}
