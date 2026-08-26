#include <Aevrynd/Renderer.h>
#include <array>
#include <chrono>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <vector>

using namespace Aevrynd;

struct CacheDirectoryGuard{
    std::filesystem::path path;
    ~CacheDirectoryGuard(){std::error_code error;std::filesystem::remove_all(path,error);}
};

int main(){
    HWND window=CreateWindowExW(0,L"STATIC",L"AevryndTextureUploadStreamingSmoke",WS_OVERLAPPED,0,0,640,360,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
    if(!window){std::cerr<<"Hidden texture upload window creation failed\n";return 1;}
    int result=0;
    try{
        CacheDirectoryGuard cache{std::filesystem::temp_directory_path()/("AevryndTextureUploadStreamingTest-"+std::to_string(GetCurrentProcessId()))};std::error_code cleanupError;std::filesystem::remove_all(cache.path,cleanupError);D3D12Renderer renderer;renderer.SetMaterialDerivedDataCacheDirectory(cache.path);renderer.Initialize(window,640,360,640,360,false);const std::filesystem::path textureRoot=std::filesystem::path{AEVRYND_SOURCE_DIR}/"engine"/"assets"/"material-packs"/"ambientcg"/"ground037"/"1k";const std::filesystem::path baseColor=textureRoot/"Ground037_1K-JPG_Color.jpg",roughness=textureRoot/"Ground037_1K-JPG_Roughness.jpg",normal=textureRoot/"Ground037_1K-JPG_NormalDX.jpg";
        const std::array<Vertex3D,3> vertices={Vertex3D{-.25f,-.25f,.5f,0,0xffffffffu},Vertex3D{0,.25f,.5f,0,0xffffffffu},Vertex3D{.25f,-.25f,.5f,0,0xffffffffu}};constexpr std::array<uint32_t,3> indices={0,1,2};const MeshHandle3D mesh=renderer.CreateMesh(vertices,indices);if(!mesh){std::cerr<<"Texture test mesh creation failed\n";result=2;}
        std::array<uint64_t,3> createNanoseconds{};uint64_t decodedAfterCold{};constexpr std::array<unsigned,3> resolutions={512,256,128};
        for(unsigned wave=0;!result&&wave<3;++wave){
            std::vector<MaterialTextureSetHandle3D> textureSets;textureSets.reserve(resolutions.size());DrawList draw;const auto start=std::chrono::steady_clock::now();for(unsigned resolution:resolutions){const MaterialTextureSetHandle3D textures=renderer.CreateMaterialTextureSet({baseColor,roughness,resolution,normal});if(!textures){std::cerr<<"Texture upload creation failed\n";result=3;break;}textureSets.push_back(textures);draw.MeshInstance(mesh,Matrix4::Identity(),{1,1,1,1},{},MeshRenderLayer3D::Opaque,textures);}createNanoseconds[wave]=uint64_t(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now()-start).count());if(result)break;
            const RendererStats queued=renderer.GetStats();const uint64_t expectedUploads=uint64_t(wave+1)*resolutions.size()*3;if(queued.materialTextureUploads!=expectedUploads||queued.materialTextureUploadBatches!=wave||!queued.materialTextureUploadStagingBytes||queued.materialTextureUploadStagingBytes>D3D12Renderer::MaxMaterialTextureUploadStagingBytes||queued.materialTextureUploadContextCreations!=1){std::cerr<<"Textures were not retained as one bounded batch\n";result=4;break;}if(wave==0){decodedAfterCold=queued.totalMaterialTextureDecodedBytes;if(queued.materialDerivedDataCacheHits||queued.materialDerivedDataCacheMisses!=9||queued.materialDerivedDataCacheWrites!=9||queued.materialDerivedDataCacheInvalidations||!decodedAfterCold){std::cerr<<"Cold material cache population failed\n";result=9;break;}}else if(wave==1){if(queued.materialDerivedDataCacheHits!=9||queued.materialDerivedDataCacheMisses!=9||queued.materialDerivedDataCacheWrites!=9||queued.materialDerivedDataCacheInvalidations||queued.totalMaterialTextureDecodedBytes!=decodedAfterCold){std::cerr<<"Warm material cache lookup failed\n";result=10;break;}}else if(queued.materialDerivedDataCacheHits!=17||queued.materialDerivedDataCacheMisses!=9||queued.materialDerivedDataCacheWrites!=10||queued.materialDerivedDataCacheInvalidations!=1||queued.totalMaterialTextureDecodedBytes<=decodedAfterCold){std::cerr<<"Corrupt material cache recovery failed\n";result=11;break;}
            renderer.Render(draw,{.02f,.03f,.04f,1});renderer.WaitIdle();const RendererStats completed=renderer.GetStats();if(completed.materialTextureUploadBatches!=wave+1||completed.completedMaterialTextureUploadBatches!=wave+1||completed.materialTextureUploadStagingBytes||completed.lastSubmittedMeshInstances!=resolutions.size()||completed.totalDroppedMeshInstances){std::cerr<<"Texture upload batch did not complete before use\n";result=5;break;}
            for(MaterialTextureSetHandle3D textures:textureSets)renderer.DestroyMaterialTextureSet(textures);renderer.WaitIdle();const RendererStats released=renderer.GetStats();if(released.residentMaterialTextureSets||released.residentMaterialTextureReferences||released.residentTextureBytes||released.pendingGpuReleaseBytes){std::cerr<<"Texture upload resources were not released\n";result=6;break;}if(wave==1){bool corrupted{};for(const auto& entry:std::filesystem::directory_iterator(cache.path))if(entry.path().extension()==".avbc"){std::ofstream output(entry.path(),std::ios::binary|std::ios::trunc);output<<"invalid";corrupted=bool(output);break;}if(!corrupted){std::cerr<<"Material cache corruption setup failed\n";result=12;break;}}
        }
        renderer.DestroyMesh(mesh);renderer.WaitIdle();const RendererStats stats=renderer.GetStats();if(!result&&(stats.materialTextureUploadContextReuses<2||stats.materialTextureUploadPressureWaits||!stats.materialDerivedDataCacheReadBytes||!stats.materialDerivedDataCacheWrittenBytes)){std::cerr<<"Texture upload or material cache reuse failed\n";result=7;}std::cout<<"textureUploads="<<stats.materialTextureUploads<<" batches="<<stats.materialTextureUploadBatches<<" completedBatches="<<stats.completedMaterialTextureUploadBatches<<" peakStagingBytes="<<stats.peakMaterialTextureUploadStagingBytes<<" contextCreations="<<stats.materialTextureUploadContextCreations<<" contextReuses="<<stats.materialTextureUploadContextReuses<<" cacheHits="<<stats.materialDerivedDataCacheHits<<" cacheMisses="<<stats.materialDerivedDataCacheMisses<<" cacheInvalidations="<<stats.materialDerivedDataCacheInvalidations<<" coldSetCreateUs="<<createNanoseconds[0]/resolutions.size()/1000<<" warmSetCreateUs="<<createNanoseconds[1]/resolutions.size()/1000<<'\n';
    }catch(const std::exception& error){std::cerr<<"Texture upload streaming test failed: "<<error.what()<<'\n';result=8;}
    DestroyWindow(window);return result;
}
