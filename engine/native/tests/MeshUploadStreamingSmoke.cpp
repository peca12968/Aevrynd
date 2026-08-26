#include <Aevrynd/Renderer.h>
#include <array>
#include <chrono>
#include <exception>
#include <iostream>
#include <vector>

using namespace Aevrynd;

int main(){
    HWND window=CreateWindowExW(0,L"STATIC",L"AevryndMeshUploadStreamingSmoke",WS_OVERLAPPED,0,0,640,360,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
    if(!window){std::cerr<<"Hidden mesh upload window creation failed\n";return 1;}
    int result=0;
    try{
        D3D12Renderer renderer;renderer.Initialize(window,640,360,640,360,false);
        constexpr size_t MeshCount=64;
        constexpr std::array<uint32_t,3> Indices={0,1,2};
        constexpr size_t VertexBytes=sizeof(Vertex3D)*3,IndexBytes=sizeof(uint32_t)*3,MeshUploadBytes=((VertexBytes+alignof(uint32_t)-1)&~size_t(alignof(uint32_t)-1))+IndexBytes;
        uint64_t totalCreateNanoseconds{};
        for(unsigned wave=0;wave<2;++wave){
            std::vector<MeshHandle3D> meshes;meshes.reserve(MeshCount);DrawList draw;
            const auto start=std::chrono::steady_clock::now();
            for(size_t index=0;index<MeshCount;++index){
                const uint32_t color=0xff000000u|uint32_t(wave*MeshCount+index+1);const std::array<Vertex3D,3> vertices={Vertex3D{-.25f,-.25f,.5f,0,color},Vertex3D{0,.25f,.5f,0,color},Vertex3D{.25f,-.25f,.5f,0,color}};const MeshHandle3D mesh=renderer.CreateMesh(vertices,Indices);if(!mesh){std::cerr<<"Mesh upload creation failed\n";result=2;break;}meshes.push_back(mesh);draw.MeshInstance(mesh,Matrix4::Identity());
            }
            totalCreateNanoseconds+=uint64_t(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now()-start).count());
            if(result)break;
            const RendererStats queued=renderer.GetStats();const uint64_t expectedUploads=uint64_t(wave+1)*MeshCount;if(queued.meshUploads!=expectedUploads||queued.meshUploadBatches!=wave||queued.meshUploadStagingBytes!=MeshCount*MeshUploadBytes||queued.peakMeshUploadStagingBytes<MeshCount*MeshUploadBytes||queued.meshUploadContextCreations!=1||queued.meshUploadBufferAllocations!=expectedUploads||queued.meshUploadStagingResources!=MeshCount||queued.peakMeshUploadStagingResources>MeshCount||queued.residentMeshResources!=MeshCount||queued.meshResourceAllocations!=expectedUploads||queued.peakResidentMeshResources>MeshCount){std::cerr<<"Mesh uploads were not retained as one bounded batch\n";result=3;break;}
            renderer.Render(draw,{.02f,.03f,.04f,1});renderer.WaitIdle();const RendererStats completed=renderer.GetStats();if(completed.meshUploadBatches!=wave+1||completed.completedMeshUploadBatches!=wave+1||completed.meshUploadStagingBytes||completed.meshUploadStagingResources||completed.lastSubmittedMeshInstances!=MeshCount||completed.totalDroppedMeshInstances){std::cerr<<"Mesh upload batch did not complete before retirement\n";result=4;break;}
            for(MeshHandle3D mesh:meshes)renderer.DestroyMesh(mesh);renderer.WaitIdle();const RendererStats released=renderer.GetStats();if(released.residentMeshes||released.residentMeshReferences||released.residentMeshBytes||released.residentMeshResources||released.pendingGpuReleaseBytes){std::cerr<<"Mesh upload resources were not released\n";result=5;break;}
        }
        const RendererStats stats=renderer.GetStats();if(!result&&(stats.meshUploadContextReuses<1||stats.meshUploadBufferAllocations!=stats.meshUploads||stats.peakMeshUploadStagingResources!=MeshCount||stats.meshResourceAllocations!=stats.meshUploads||stats.peakResidentMeshResources!=MeshCount)){std::cerr<<"Mesh upload context or buffer sharing failed\n";result=6;}
        std::cout<<"meshUploads="<<stats.meshUploads<<" batches="<<stats.meshUploadBatches<<" completedBatches="<<stats.completedMeshUploadBatches<<" peakStagingBytes="<<stats.peakMeshUploadStagingBytes<<" bufferAllocations="<<stats.meshUploadBufferAllocations<<" peakStagingResources="<<stats.peakMeshUploadStagingResources<<" residentAllocations="<<stats.meshResourceAllocations<<" peakResidentResources="<<stats.peakResidentMeshResources<<" contextCreations="<<stats.meshUploadContextCreations<<" contextReuses="<<stats.meshUploadContextReuses<<" averageCreateUs="<<totalCreateNanoseconds/(MeshCount*2)/1000<<'\n';
    }catch(const std::exception& error){std::cerr<<"Mesh upload streaming test failed: "<<error.what()<<'\n';result=7;}
    DestroyWindow(window);return result;
}
