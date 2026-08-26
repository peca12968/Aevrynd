#include <Aevrynd/Scene3D.h>
#include <chrono>
#include <cstdint>
#include <iostream>

using namespace Aevrynd;

int main(){
    Scene3D scene;for(int z=-40;z<=40;++z)for(int x=-40;x<=40;++x){const float height=1.f+float((x*x+z*z)%5)*.4f;scene.Box({float(x)*2.5f,height*.5f,float(z)*2.5f},{1.5f,height,1.5f},{.2f,.7f,.8f,1});}
    Camera3D camera=MakeIsometricCamera({0,1,0},48,32);constexpr Vec2 Viewport{1280,720};constexpr int Iterations=120;DrawList culledDraw,fullDraw;
    // 先建立兩種路徑的容量，避免容器首次配置扭曲裁切成本比較。
    Scene3DStats culledStats=scene.Submit(culledDraw,camera,Viewport);Scene3DStats fullStats=scene.Submit(fullDraw,camera,Viewport,{false});culledDraw.Clear();fullDraw.Clear();
    auto measure=[&](DrawList& draw,Scene3DSubmissionOptions options){const auto start=std::chrono::steady_clock::now();for(int index=0;index<Iterations;++index){draw.Clear();scene.Submit(draw,camera,Viewport,options);}return uint64_t(std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now()-start).count())/Iterations;};
    const uint64_t culledAverageUs=measure(culledDraw,{}),fullAverageUs=measure(fullDraw,{false});auto boxCpu=std::make_shared<Mesh3D>(Mesh3D::CreateUnitBox());scene.SetBoxMesh({{1},boxCpu->Bounds(),boxCpu->vertices.size(),boxCpu->indices.size(),boxCpu});DrawList instancedDraw;Scene3DStats instancedStats=scene.Submit(instancedDraw,camera,Viewport);instancedDraw.Clear();const uint64_t instancedAverageUs=measure(instancedDraw,{});
    const uint64_t culledBytes=culledDraw.worldVertices.size()*sizeof(Vertex3D)+culledDraw.worldIndices.size()*sizeof(uint32_t),fullBytes=fullDraw.worldVertices.size()*sizeof(Vertex3D)+fullDraw.worldIndices.size()*sizeof(uint32_t),reductionPermille=fullBytes?(fullBytes-culledBytes)*1000/fullBytes:0;
    const uint64_t instancedBytes=instancedDraw.meshInstances.size()*(sizeof(Matrix4)+sizeof(uint32_t)),instanceReductionPermille=culledBytes?(culledBytes-instancedBytes)*1000/culledBytes:0;
    std::cout<<"input="<<culledStats.meshInstances<<" visible="<<culledStats.visibleMeshInstances<<" culled="<<culledStats.culledMeshInstances<<" culledBytes="<<culledBytes<<" fullBytes="<<fullBytes<<" reductionPermille="<<reductionPermille<<" instancedBytes="<<instancedBytes<<" instanceReductionPermille="<<instanceReductionPermille<<" instancedAverageUs="<<instancedAverageUs<<" expandedAverageUs="<<culledAverageUs<<" fullAverageUs="<<fullAverageUs<<'\n';
    if(culledStats.meshInstances!=6561||culledStats.visibleMeshInstances==0||culledStats.culledMeshInstances==0||fullStats.visibleMeshInstances!=6561||culledBytes>=fullBytes||reductionPermille<800||instancedStats.visibleMeshInstances!=culledStats.visibleMeshInstances||instancedDraw.meshInstances.size()!=culledStats.visibleMeshInstances||!instancedDraw.worldVertices.empty()||instancedBytes>=culledBytes||instanceReductionPermille<800)return 1;
    return 0;
}
