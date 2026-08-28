#include "Aevrynd/Renderer.h"
#include <Windows.h>
#include <algorithm>
#include <filesystem>
#include <iostream>
#include <vector>

using namespace Aevrynd;

namespace {
uint64_t Median(std::vector<uint64_t> values){if(values.empty())return 0;std::ranges::sort(values);return values[values.size()/2];}
}

int main(){
    HWND window=CreateWindowExW(0,L"STATIC",L"AevryndHeightFogBenchmark",WS_OVERLAPPED,0,0,1280,720,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);if(!window)return 1;
    int result{};{
        D3D12Renderer renderer;renderer.Initialize(window,1280,720,1280,720,false);renderer.SetQualitySettings({false,false,false,false});
        DrawList draw{DrawListCapacityProfile::Minimal2D};draw.WorldTriangle({-1.f,-1.f,.5f},{0.f,1.f,.5f},{1.f,-1.f,.5f},{.92f,.88f,.78f,1.f});
        Atmosphere3D atmosphere{};atmosphere.enabled=true;atmosphere.cameraPosition={0.f,4.f,-80.f};atmosphere.fogStart=10000.f;atmosphere.fogEnd=20000.f;atmosphere.fogColor={.08f,.12f,.16f,1.f};
        auto sample=[&](float density,const std::filesystem::path& capture){atmosphere.heightFogDensity=density;draw.SetAtmosphere(atmosphere);std::vector<uint64_t> timings;for(int frame=0;frame<48;++frame){renderer.Render(draw,{.02f,.03f,.04f,1.f});if(frame>=16&&renderer.GetStats().lastGpuFrameNanoseconds)timings.push_back(renderer.GetStats().lastGpuFrameNanoseconds);}renderer.CaptureNextFrame(capture);renderer.Render(draw,{.02f,.03f,.04f,1.f});renderer.WaitIdle();return std::pair{Median(std::move(timings)),renderer.GetStats()};};
        const auto root=std::filesystem::temp_directory_path();const auto baselinePath=root/"aevrynd-height-fog-off.png",fogPath=root/"aevrynd-height-fog-on.png";const auto [baselineGpu,baseline]=sample(0.f,baselinePath);const auto [fogGpu,fog]=sample(.018f,fogPath);
        const int64_t gpuDelta=int64_t(fogGpu)-int64_t(baselineGpu);std::cout<<"HeightFog3DBenchmark gpuOffUs="<<baselineGpu/1000<<" gpuOnUs="<<fogGpu/1000<<" gpuDeltaUs="<<gpuDelta/1000<<" luminanceOff="<<baseline.lastCapturedAverageLuminance<<" luminanceOn="<<fog.lastCapturedAverageLuminance<<" residentDeltaBytes="<<int64_t(fog.hdrSceneColorBytes)-int64_t(baseline.hdrSceneColorBytes)<<'\n';
        if(!baselineGpu||!fogGpu||baseline.successfulFrameCaptures<1||fog.successfulFrameCaptures<2||!(fog.lastCapturedAverageLuminance<baseline.lastCapturedAverageLuminance*.9)||fog.hdrSceneColorBytes!=baseline.hdrSceneColorBytes)result=2;
        std::error_code error;std::filesystem::remove(baselinePath,error);std::filesystem::remove(fogPath,error);
    }DestroyWindow(window);return result;
}
