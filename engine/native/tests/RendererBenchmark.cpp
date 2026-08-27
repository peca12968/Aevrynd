#include <Aevrynd/Renderer.h>
#include <cstdint>
#include <exception>
#include <iostream>

using namespace Aevrynd;

int main(){
    HWND window=CreateWindowExW(0,L"STATIC",L"AevryndRendererBenchmark",WS_OVERLAPPED,0,0,1280,720,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
    if(!window){std::cerr<<"Hidden benchmark window creation failed\n";return 1;}
    int result=0;
    try{
        D3D12Renderer renderer;renderer.Initialize(window,1280,720,1280,720,false);if(renderer.DLSSOn())renderer.ToggleDLSS();const bool streamlineLoaded=GetModuleHandleW(L"sl.interposer.dll")!=nullptr;
        DrawList draw;draw.Quad({640,360},{1280,720},{.02f,.04f,.05f,1});constexpr uint64_t Pure2DFrames=16;for(uint64_t frame=0;frame<Pure2DFrames;++frame)renderer.Render(draw,{.02f,.04f,.05f,1});renderer.WaitIdle();renderer.Resize(1024,768);renderer.Resize(1280,720);renderer.Resize(1280,720);const RendererStats pure2DStats=renderer.GetStats();if(pure2DStats.hdrSceneColorBytes||pure2DStats.hdrSceneColorAllocations||pure2DStats.hdrSceneColorReleases||pure2DStats.hdrPostProcessPasses||pure2DStats.resizeCount!=2||pure2DStats.outputWidth!=1280||pure2DStats.outputHeight!=720){std::cerr<<"Pure 2D path allocated an unused HDR scene target\n";result=2;}
        draw.WorldTriangle({-.5f,-.5f,.5f},{0,.5f,.5f},{.5f,-.5f,.5f},{.2f,.8f,.7f,1});constexpr uint64_t SceneFrames=240,Frames=Pure2DFrames+SceneFrames;for(uint64_t frame=0;frame<80;++frame)renderer.Render(draw,{.02f,.04f,.05f,1});renderer.Resize(1024,768);for(uint64_t frame=80;frame<160;++frame)renderer.Render(draw,{.02f,.04f,.05f,1});renderer.Resize(1600,900);for(uint64_t frame=160;frame<SceneFrames;++frame)renderer.Render(draw,{.02f,.04f,.05f,1});renderer.WaitIdle();
        RendererStats stats=renderer.GetStats();const uint64_t averageUs=stats.resourceWaits?stats.totalResourceWaitNanoseconds/stats.resourceWaits/1000:0,ratePermille=stats.submittedFrames?stats.resourceWaits*1000/stats.submittedFrames:0,gpuAverageUs=stats.gpuTimingSamples?stats.totalGpuFrameNanoseconds/stats.gpuTimingSamples/1000:0;
        std::cout<<"frames="<<stats.submittedFrames<<" bufferedFrames="<<D3D12Renderer::BufferedFrames<<" waits="<<stats.resourceWaits<<" waitRatePermille="<<ratePermille<<" averageUs="<<averageUs<<" peakUs="<<stats.peakResourceWaitNanoseconds/1000<<" gpuSamples="<<stats.gpuTimingSamples<<" gpuLastUs="<<stats.lastGpuFrameNanoseconds/1000<<" gpuAverageUs="<<gpuAverageUs<<" gpuPeakUs="<<stats.peakGpuFrameNanoseconds/1000<<" uploadBytes="<<stats.lastUploadBytes<<" frameUploadResidentBytes="<<stats.frameUploadResidentBytes<<" frameUploadGrowths="<<stats.frameUploadGrowths<<" streamlineLoaded="<<streamlineLoaded<<" dlssBytes="<<stats.dlssResidentBytes<<" dlssAllocations="<<stats.dlssResourceAllocations<<'\n';
        if(stats.submittedFrames!=Frames||stats.gpuTimingSamples!=Frames||!stats.lastGpuFrameNanoseconds||stats.totalGpuFrameNanoseconds<stats.peakGpuFrameNanoseconds||stats.totalDroppedVertices||stats.totalDroppedWorldVertices||stats.totalDroppedWorldIndices||stats.resizeCount!=4||stats.outputWidth!=1600||stats.outputHeight!=900||stats.canvasViewport.x!=0||stats.canvasViewport.y!=0||!stats.linearHdrSceneComposition||stats.hdrSceneColorBytes!=1600ull*900*8||stats.hdrSceneColorAllocations!=3||stats.hdrSceneColorReleases!=2||stats.hdrPostProcessPasses!=SceneFrames||stats.frameUploadResidentBytes!=D3D12Renderer::InitialFrameUploadBytes||stats.peakFrameUploadResidentBytes!=D3D12Renderer::InitialFrameUploadBytes||stats.frameUploadGrowths||stats.frameUploadTrims||streamlineLoaded||renderer.DLSSModuleRequested()||stats.dlssModuleRequested||stats.dlssFrameworkLoaded||renderer.SupportsDLSS()||stats.dlssResidentBytes||stats.dlssResourceAllocations||stats.dlssResourceReleases||stats.dlssResourcesResident)result=2;
    }catch(const std::exception& error){std::cerr<<"Renderer benchmark failed: "<<error.what()<<'\n';result=3;}
    DestroyWindow(window);return result;
}
