#include <Aevrynd/EnvironmentIbl3D.h>
#include <Aevrynd/Mesh3D.h>
#include <Aevrynd/Renderer.h>
#include <Windows.h>
#include <Psapi.h>
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <vector>

using namespace Aevrynd;

namespace {
void Put32(std::vector<std::byte>& bytes,size_t offset,uint32_t value){for(unsigned i=0;i<4;++i)bytes[offset+i]=std::byte(value>>(i*8));}
bool WriteDds(const std::filesystem::path& path,uint32_t width,uint32_t height,uint32_t format,bool cube,uint32_t mipCount,bool unit=false){size_t payload{};for(uint32_t face=0;face<(cube?6u:1u);++face)for(uint32_t mip=0;mip<mipCount;++mip){const uint32_t w=(std::max)(1u,width>>mip),h=(std::max)(1u,height>>mip);payload+=cube?size_t((w+3)/4)*((h+3)/4)*16:size_t(w)*h*4;}std::vector<std::byte> bytes(148+payload);Put32(bytes,0,0x20534444);Put32(bytes,4,124);Put32(bytes,12,height);Put32(bytes,16,width);Put32(bytes,28,mipCount);Put32(bytes,76,32);Put32(bytes,80,4);Put32(bytes,84,0x30315844);Put32(bytes,128,format);Put32(bytes,132,3);Put32(bytes,136,cube?4:0);Put32(bytes,140,1);if(unit)for(size_t offset=148;offset+3<bytes.size();offset+=4){bytes[offset]=std::byte{0};bytes[offset+1]=std::byte{0x3c};}std::ofstream output(path,std::ios::binary);output.write(reinterpret_cast<const char*>(bytes.data()),std::streamsize(bytes.size()));return bool(output);}
bool CreateDistinctCube(const std::filesystem::path& source,const std::filesystem::path& destination){std::ifstream input(source,std::ios::binary|std::ios::ate);if(!input)return false;const auto size=input.tellg();if(size<=148)return false;std::vector<char> bytes(static_cast<size_t>(size));input.seekg(0);input.read(bytes.data(),size);for(size_t offset=148;offset<bytes.size();++offset)bytes[offset]=char(uint8_t(bytes[offset])^uint8_t(0x5au+(offset&7u)));std::ofstream output(destination,std::ios::binary);output.write(bytes.data(),std::streamsize(bytes.size()));return bool(output);}
uint64_t Hash(const std::filesystem::path& path){std::ifstream input(path,std::ios::binary);uint64_t hash=1469598103934665603ull;char byte{};while(input.get(byte))hash=(hash^uint8_t(byte))*1099511628211ull;return hash;}
uint64_t Median(std::vector<uint64_t> values){std::ranges::sort(values);return values.empty()?0:values[values.size()/2];}
}

int main(){
    const std::filesystem::path artifacts=AEVRYND_TEST_ARTIFACT_DIR,temp=artifacts/L"fixtures";std::error_code error;std::filesystem::create_directories(temp,error);if(error)return 1;
    const auto directional=std::filesystem::path(AEVRYND_SOURCE_DIR)/L"engine/native/tests/fixtures/environment-directional-bc6h.dds",localDirectional=temp/L"local-distinct-bc6h.dds",brdf=temp/L"unit-brdf.dds";if(!std::filesystem::is_regular_file(directional)||!CreateDistinctCube(directional,localDirectional)||!WriteDds(brdf,64,64,34,false,1,true))return 2;
    EnvironmentLightingStore3D store;const auto global=store.Create({directional,directional,brdf,0}),local=store.Create({localDirectional,localDirectional,brdf,1.5707963268f});if(!global||!local)return 3;
    HWND window=CreateWindowExW(0,L"STATIC",L"AevryndLocalReflectionProbe",WS_OVERLAPPED,0,0,1920,1080,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);if(!window)return 4;
    int result{};{
        D3D12Renderer renderer;renderer.Initialize(window,1920,1080,1920,1080,false);renderer.SetQualitySettings({false,false,false,false});
        // 中文：enhanced world root固定64 DWORD；probe切換不得依賴前一draw殘留的shared constants。
        if(renderer.GetStats().rootSignatureDwords!=EnhancedWorldRootDwords3D)result=9;
        DrawList pure{DrawListCapacityProfile::Minimal2D};pure.Quad({0,0},{16,16},{1,1,1,1});renderer.Render(pure,{0,0,0,1});renderer.WaitIdle();if(renderer.GetStats().localReflectionProbeBufferBytes||renderer.GetStats().localReflectionProbesEnabled)result=5;
        const Mesh3D sphereData=Mesh3D::CreateUnitSphere(48,32);const auto sphere=renderer.CreateMesh(sphereData.vertices,sphereData.indices);if(!sphere||!renderer.CreateEnvironmentLighting(store,global)||!renderer.CreateEnvironmentLighting(store,local)||!renderer.SetEnvironmentLighting(global))result=6;
        auto makeDraw=[&](bool probe,bool reverse=false){DrawList draw{DrawListCapacityProfile::Minimal2D};draw.SetViewProjection(Matrix4::Identity());draw.SetBackfaceCulling(false);draw.SetDirectionalLight({{0,-1,0},1,{1,1,1,1},0});for(int x=-2;x<=2;++x)draw.MeshInstance(sphere,CreateTransformMatrix({float(x)*.34f,0,.62f},{},{.28f,.28f,.28f}),{1,1,1,1},{.08f,1,0,1},MeshRenderLayer3D::Opaque,{},.25f,false,MaterialClass3D::Surface,.62f,false);if(probe){const LocalReflectionProbe3D low{global,{-2,-2,-1},{2,2,2},{0,0,.5f},1},high{local,{-1.5f,-1.5f,-1},{1.5f,1.5f,2},{.45f,0,.5f},2};if(reverse){draw.AddLocalReflectionProbe(high);draw.AddLocalReflectionProbe(low);}else{draw.AddLocalReflectionProbe(low);draw.AddLocalReflectionProbe(high);}}return draw;};
        const auto globalPath=artifacts/L"local-probe-disabled.png",localPath=artifacts/L"local-probe-enabled.png",repeatPath=artifacts/L"local-probe-enabled-repeat.png",reversePath=artifacts/L"local-probe-reordered.png";
        struct Sample{uint64_t gpu{},cpu{};RendererStats stats{};};auto sample=[&](DrawList& draw,const std::filesystem::path& path){std::vector<uint64_t> gpu,cpu;for(int frame=0;frame<32;++frame){const auto start=std::chrono::steady_clock::now();renderer.Render(draw,{.006f,.009f,.014f,1});const auto stop=std::chrono::steady_clock::now();if(frame>=16){cpu.push_back(uint64_t(std::chrono::duration_cast<std::chrono::nanoseconds>(stop-start).count()));if(renderer.GetStats().lastGpuFrameNanoseconds)gpu.push_back(renderer.GetStats().lastGpuFrameNanoseconds);}}renderer.CaptureNextFrame(path);renderer.Render(draw,{.006f,.009f,.014f,1});renderer.WaitIdle();return Sample{Median(std::move(gpu)),Median(std::move(cpu)),renderer.GetStats()};};
        auto disabled=makeDraw(false),enabled=makeDraw(true),reordered=makeDraw(true,true);const auto off=sample(disabled,globalPath),on=sample(enabled,localPath),repeat=sample(enabled,repeatPath),order=sample(reordered,reversePath);const auto offHash=Hash(globalPath),onHash=Hash(localPath),repeatHash=Hash(repeatPath),orderHash=Hash(reversePath),gpuDelta=on.gpu>off.gpu?on.gpu-off.gpu:0;
        const auto allocations=order.stats.localReflectionProbeBufferAllocations;renderer.Resize(1280,720);renderer.Render(enabled,{.006f,.009f,.014f,1});renderer.WaitIdle();const auto resized=renderer.GetStats();renderer.Render(disabled,{.006f,.009f,.014f,1});renderer.WaitIdle();renderer.TrimTransientMemory();const auto trimmed=renderer.GetStats();PROCESS_MEMORY_COUNTERS_EX memory{};GetProcessMemoryInfo(GetCurrentProcess(),reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&memory),sizeof(memory));
        std::cout<<"LocalReflectionProbeD3D12 gpuOffUs="<<off.gpu/1000<<" gpuOnUs="<<on.gpu/1000<<" gpuDeltaUs="<<gpuDelta/1000<<" cpuOffUs="<<off.cpu/1000<<" cpuOnUs="<<on.cpu/1000<<" bufferBytes="<<on.stats.localReflectionProbeBufferBytes<<" draws="<<on.stats.localReflectionProbeDraws<<" selections="<<on.stats.localReflectionProbeSelections<<" workingSetBytes="<<memory.WorkingSetSize<<" hashes="<<offHash<<','<<onHash<<','<<repeatHash<<','<<orderHash<<" captures="<<artifacts.string()<<'\n';
        if(result==0&&(!off.gpu||!on.gpu||gpuDelta>1000000ull||off.stats.localReflectionProbesEnabled||off.stats.localReflectionProbeBufferBytes||!on.stats.localReflectionProbesEnabled||on.stats.localReflectionProbeCount!=2||on.stats.localReflectionProbeBufferBytes!=4608||on.stats.residentEnvironmentLightingSets!=2||on.stats.residentEnvironmentLightingDescriptors!=6||!on.stats.localReflectionProbeDraws||!on.stats.localReflectionProbeSelections||offHash==onHash||onHash!=repeatHash||onHash!=orderHash||resized.localReflectionProbeBufferAllocations!=allocations||resized.localReflectionProbeBufferBytes!=4608||trimmed.localReflectionProbeBufferBytes||trimmed.localReflectionProbeBufferReleases==0))result=7;
        renderer.SetEnvironmentLighting({});renderer.DestroyEnvironmentLighting(local);renderer.DestroyEnvironmentLighting(global);renderer.DestroyMesh(sphere);renderer.WaitIdle();renderer.TrimEnvironmentLighting();if(renderer.GetStats().residentEnvironmentLightingBytes||renderer.GetStats().residentMeshBytes)result=8;
    }DestroyWindow(window);return result;
}
