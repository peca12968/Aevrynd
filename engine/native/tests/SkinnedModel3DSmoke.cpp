#include <Aevrynd/SkinnedModel3D.h>
#include <chrono>
#include <cmath>
#include <iostream>
#include <limits>

using namespace Aevrynd;
namespace {
bool Near(float left,float right,float tolerance=.0001f){return std::abs(left-right)<=tolerance;}
SkinnedModel3D CreateModel(){
    SkinnedModel3D model;const uint32_t normal=(PackNormalSnorm8({0,0,1})&0x00ffffffu)|0x81000000u;
    model.bindMesh.vertices={{0,0,0,normal,0xff102030u,.1f,.2f},{1,0,0,normal,0xff405060u,.3f,.4f},{0,1,0,normal,0xff708090u,.5f,.6f}};model.bindMesh.indices={0,1,2};
    model.skeleton.joints={{-1,{}},{0,{{1,0,0},{},{1,1,1}}}};
    model.influences={{{0,0,0,0},{65535,0,0,0}},{{1,0,0,0},{65535,0,0,0}},{{1,0,0,0},{32768,32767,0,0}}};
    model.inverseBindMatrices={Matrix4::Identity(),CreateTranslationMatrix({-1,0,0})};return model;
}
AnimationPose3D BindPose(){AnimationPose3D pose;pose.globalMatrices={Matrix4::Identity(),CreateTranslationMatrix({1,0,0})};return pose;}
}

int main(){
    SkinnedModel3D model=CreateModel();if(!model.Valid()){std::cerr<<"Valid skinned model was rejected\n";return 1;}
    SkinningWorkspace3D workspace;Mesh3D output;SkinningStats3D stats;if(!SkinModelInto(model,BindPose(),workspace,output,&stats)||output.vertices.size()!=3||output.indices!=model.bindMesh.indices||stats.vertices!=3||stats.influences!=4||stats.outputBytes!=3*sizeof(Vertex3D)+3*sizeof(uint32_t)){std::cerr<<"Bind pose skinning failed\n";return 2;}
    for(std::size_t index=0;index<output.vertices.size();++index){const Vertex3D& source=model.bindMesh.vertices[index],& skinned=output.vertices[index];if(!Near(source.x,skinned.x)||!Near(source.y,skinned.y)||!Near(source.z,skinned.z)||(source.normal&0xff000000u)!=(skinned.normal&0xff000000u)||source.color!=skinned.color||source.u!=skinned.u||source.v!=skinned.v){std::cerr<<"Bind attributes changed\n";return 3;}}
    AnimationPose3D moved=BindPose();moved.globalMatrices[1]=CreateTranslationMatrix({3,0,0});if(!SkinModelInto(model,moved,workspace,output,&stats)||!Near(output.vertices[1].x,3.f)||!Near(output.vertices[2].x,2.f*32768.f/65535.f,.0001f)){std::cerr<<"Single joint or blended skinning failed\n";return 4;}
    const Vec4 normal=UnpackNormalSnorm8(output.vertices[2].normal);if(!Near(normal.x,0,.01f)||!Near(normal.y,0,.01f)||normal.z<.99f||(output.vertices[2].normal&0xff000000u)!=0x81000000u){std::cerr<<"Skinned normal semantics failed\n";return 5;}
    Mesh3D sentinel=Mesh3D::CreateUnitBox();const std::size_t sentinelVertices=sentinel.vertices.size(),sentinelIndices=sentinel.indices.size();SkinnedModel3D invalid=model;invalid.influences[0].weights[0]=1;if(invalid.Valid()||SkinModelInto(invalid,moved,workspace,sentinel)||sentinel.vertices.size()!=sentinelVertices||sentinel.indices.size()!=sentinelIndices){std::cerr<<"Invalid weight atomicity failed\n";return 6;}
    invalid=model;invalid.influences[1].joints[0]=2;if(invalid.Valid()){std::cerr<<"Invalid joint was accepted\n";return 7;}invalid=model;invalid.inverseBindMatrices[0][0][0]=(std::numeric_limits<float>::quiet_NaN)();if(invalid.Valid()){std::cerr<<"Non-finite matrix was accepted\n";return 8;}
    SkinnedModelLimits3D tiny;tiny.maximumVertices=2;if(model.Valid(tiny)||SkinModelInto(model,moved,workspace,sentinel,nullptr,tiny)){std::cerr<<"Model limits were ignored\n";return 9;}AnimationPose3D invalidPose=moved;invalidPose.globalMatrices.pop_back();if(SkinModelInto(model,invalidPose,workspace,sentinel)){std::cerr<<"Invalid pose was accepted\n";return 10;}
    if(!SkinModelInto(model,moved,workspace,output)||!SkinModelInto(model,moved,workspace,output)){std::cerr<<"Workspace reuse setup failed\n";return 11;}const ReusableMemoryStats reused=workspace.MemoryStats();if(!reused.retainedCapacityBytes||!SkinModelInto(model,moved,workspace,output)||workspace.MemoryStats().retainedCapacityBytes!=reused.retainedCapacityBytes){std::cerr<<"Workspace capacity was not reused\n";return 12;}
    constexpr std::size_t BenchmarkTriangles=10000,BenchmarkFrames=60;SkinnedModel3D benchmark;benchmark.skeleton=model.skeleton;benchmark.inverseBindMatrices=model.inverseBindMatrices;benchmark.bindMesh.vertices.reserve(BenchmarkTriangles*3);benchmark.bindMesh.indices.reserve(BenchmarkTriangles*3);benchmark.influences.reserve(BenchmarkTriangles*3);for(std::size_t triangle=0;triangle<BenchmarkTriangles;++triangle){const std::uint32_t base=std::uint32_t(benchmark.bindMesh.vertices.size());for(std::size_t vertex=0;vertex<3;++vertex){Vertex3D item=model.bindMesh.vertices[vertex];item.x+=float(triangle%100)*.02f;item.z+=float(triangle/100)*.02f;benchmark.bindMesh.vertices.push_back(item);benchmark.influences.push_back(model.influences[vertex]);}benchmark.bindMesh.indices.insert(benchmark.bindMesh.indices.end(),{base,base+1,base+2});}SkinningWorkspace3D benchmarkWorkspace;Mesh3D benchmarkOutput;if(!SkinModelInto(benchmark,moved,benchmarkWorkspace,benchmarkOutput)||!SkinModelInto(benchmark,moved,benchmarkWorkspace,benchmarkOutput)){std::cerr<<"Large skinning warmup failed\n";return 14;}const std::size_t benchmarkCapacity=benchmarkWorkspace.MemoryStats().retainedCapacityBytes;const auto benchmarkStart=std::chrono::steady_clock::now();for(std::size_t frame=0;frame<BenchmarkFrames;++frame)if(!SkinModelInto(benchmark,moved,benchmarkWorkspace,benchmarkOutput)){std::cerr<<"Large skinning iteration failed\n";return 15;}const std::uint64_t averageMicroseconds=std::uint64_t(std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now()-benchmarkStart).count())/BenchmarkFrames;if(benchmarkOutput.vertices.size()!=BenchmarkTriangles*3||benchmarkWorkspace.MemoryStats().retainedCapacityBytes!=benchmarkCapacity||averageMicroseconds>8000){std::cerr<<"Bounded skinning throughput failed averageUs="<<averageMicroseconds<<'\n';return 16;}benchmarkWorkspace.TrimUnusedMemory();
    const std::size_t released=workspace.TrimUnusedMemory();const ReusableMemoryStats trimmed=workspace.MemoryStats();if(!released||trimmed.retainedCapacityBytes||trimmed.reclaimableCapacityBytes||trimmed.lastReleasedBytes!=released||trimmed.trimCount!=1){std::cerr<<"Workspace trim telemetry failed\n";return 13;}
    std::cout<<"vertices="<<stats.vertices<<" influences="<<stats.influences<<" bytes="<<stats.outputBytes<<" average30kUs="<<averageMicroseconds<<" released="<<released<<'\n';return 0;
}
