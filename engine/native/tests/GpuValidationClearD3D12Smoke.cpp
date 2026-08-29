#include <Aevrynd/Renderer.h>
#include <Windows.h>
#include <array>
#include <filesystem>
#include <fstream>
#include <iostream>

using namespace Aevrynd;

namespace {
uint64_t HashFile(const std::filesystem::path& path){std::ifstream input(path,std::ios::binary);uint64_t hash=1469598103934665603ull;char byte{};while(input.get(byte)){hash^=uint8_t(byte);hash*=1099511628211ull;}return hash;}
bool HasWarning(const RendererStats& stats,uint32_t id){for(size_t index=0;index<stats.debugWarningIdCount;++index)if(stats.debugWarningIds[index]==id)return true;return false;}
}

int main(){
 SetEnvironmentVariableW(L"AEVRYND_GPU_VALIDATION",L"1");
 HWND window=CreateWindowExW(0,L"STATIC",L"AevryndGpuValidationClear",WS_OVERLAPPED,0,0,320,180,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);if(!window)return 1;
 int result{};{
  D3D12Renderer renderer;renderer.Initialize(window,320,180,320,180,false);renderer.SetQualitySettings({false,false,true,false});
  DrawList draw{DrawListCapacityProfile::Minimal2D};draw.SetViewProjection(Matrix4::Identity());draw.SetBackfaceCulling(false);draw.WorldTriangle({-.9f,-.8f,.5f},{0,.9f,.5f},{.9f,-.8f,.5f},{1,1,1,1});
  const auto temporary=std::filesystem::temp_directory_path();const auto dark=temporary/L"aevrynd-clear-dark.png",bright=temporary/L"aevrynd-clear-bright.png";std::error_code error;std::filesystem::remove(dark,error);std::filesystem::remove(bright,error);
  renderer.CaptureNextFrame(dark);renderer.Render(draw,{.01f,.02f,.03f,1});renderer.WaitIdle();const RendererStats baseline=renderer.GetStats();
  renderer.CaptureNextFrame(bright);renderer.Render(draw,{.35f,.15f,.05f,1});renderer.WaitIdle();
  for(unsigned frame=2;frame<32;++frame)renderer.Render(draw,frame&1?Color{.35f,.15f,.05f,1}:Color{.01f,.02f,.03f,1});renderer.WaitIdle();const RendererStats steady=renderer.GetStats();
  const bool captures=std::filesystem::is_regular_file(dark)&&std::filesystem::is_regular_file(bright)&&HashFile(dark)!=HashFile(bright);std::filesystem::remove(dark,error);std::filesystem::remove(bright,error);
  if(!captures||steady.successfulFrameCaptures!=baseline.successfulFrameCaptures+1||steady.hdrSceneColorAllocations!=baseline.hdrSceneColorAllocations||steady.hdrSceneColorReleases!=baseline.hdrSceneColorReleases||steady.hdrSceneColorBytes!=baseline.hdrSceneColorBytes||steady.multisampleAllocations!=baseline.multisampleAllocations||steady.multisampleReleases!=baseline.multisampleReleases||steady.multisampleResidentBytes!=baseline.multisampleResidentBytes)result=2;
  ReflectionProbeCaptureSettings3D probe{};probe.resolution=32;renderer.SetReflectionProbeCaptureBudget(16*1024*1024);const auto handle=renderer.CreateReflectionProbeCapture3D(probe);if(!handle||!renderer.RequestReflectionProbeCaptureReadback3D(handle))result=3;
  D3D12RendererTestAccess3D::SetFixedProbeClearMismatch(renderer,true);renderer.Render(draw,{.02f,.03f,.04f,1});renderer.WaitIdle();D3D12RendererTestAccess3D::SetFixedProbeClearMismatch(renderer,false);
  const auto readback=renderer.TakeReflectionProbeCaptureReadback3D(handle);const RendererStats injected=renderer.GetStats();constexpr uint32_t mismatch=820;const bool strictGate=injected.gpuValidationRequested&&injected.gpuValidationAvailable&&injected.gpuValidationExecuted&&injected.gpuValidationPassed&&injected.debugErrorCount==0&&injected.debugWarningCount==0;
  if(!readback||!HasWarning(injected,mismatch)||strictGate)result=4;
  renderer.DestroyReflectionProbeCapture3D(handle);renderer.WaitIdle();std::cout<<"frames="<<steady.submittedFrames<<" hdr="<<steady.hdrSceneColorBytes<<" msaa="<<steady.multisampleResidentBytes<<" captures="<<steady.successfulFrameCaptures<<" probeReadback="<<bool(readback)<<" warning820="<<HasWarning(injected,mismatch)<<" strict="<<strictGate<<'\n';
 }DestroyWindow(window);return result;
}
