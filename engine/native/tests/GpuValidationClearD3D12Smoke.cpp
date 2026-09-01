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
  DrawList clearDraw{DrawListCapacityProfile::Minimal2D};DrawList draw{DrawListCapacityProfile::Minimal2D};draw.SetViewProjection(Matrix4::Identity());draw.SetBackfaceCulling(false);draw.WorldTriangle({-.95f,-.95f,.5f},{-.9f,-.85f,.5f},{-.85f,-.95f,.5f},{1,1,1,1});
  const auto temporary=std::filesystem::temp_directory_path();const auto dark=temporary/L"aevrynd-clear-dark.png",bright=temporary/L"aevrynd-clear-bright.png";std::error_code error;std::filesystem::remove(dark,error);std::filesystem::remove(bright,error);
  renderer.CaptureNextFrame(dark);renderer.Render(clearDraw,{.01f,.02f,.03f,1});renderer.WaitIdle();
  renderer.CaptureNextFrame(bright);renderer.Render(clearDraw,{.35f,.15f,.05f,1});renderer.WaitIdle();
  renderer.Render(draw,{0,0,0,1});renderer.WaitIdle();const RendererStats baseline=renderer.GetStats();
  for(unsigned frame=1;frame<32;++frame)renderer.Render(draw,{0,0,0,1});renderer.WaitIdle();const RendererStats steady=renderer.GetStats();
  const bool darkExists=std::filesystem::is_regular_file(dark),brightExists=std::filesystem::is_regular_file(bright);const uint64_t darkHash=darkExists?HashFile(dark):0,brightHash=brightExists?HashFile(bright):0;const bool captures=darkExists&&brightExists&&darkHash!=brightHash;std::filesystem::remove(dark,error);std::filesystem::remove(bright,error);
  const bool normalStrict=steady.gpuValidationRequested&&steady.gpuValidationAvailable&&steady.gpuValidationExecuted&&steady.gpuValidationPassed&&steady.debugErrorCount==0&&steady.debugWarningCount==0;
  if(!captures||!normalStrict||steady.successfulFrameCaptures!=baseline.successfulFrameCaptures||steady.hdrSceneColorAllocations!=baseline.hdrSceneColorAllocations||steady.hdrSceneColorReleases!=baseline.hdrSceneColorReleases||steady.hdrSceneColorBytes!=baseline.hdrSceneColorBytes||steady.multisampleAllocations!=baseline.multisampleAllocations||steady.multisampleReleases!=baseline.multisampleReleases||steady.multisampleResidentBytes!=baseline.multisampleResidentBytes){
   // 中文：失敗時保留完整常駐資源計數，避免把生命週期錯誤誤判為 GBV clear 警告。
   std::cerr<<"steady-state mismatch captures="<<captures<<" normalStrict="<<normalStrict<<" warnings="<<steady.debugWarningCount<<" errors="<<steady.debugErrorCount<<" darkHash="<<darkHash<<" brightHash="<<brightHash<<" captureBase="<<baseline.successfulFrameCaptures<<" captureNow="<<steady.successfulFrameCaptures<<" hdrAlloc="<<baseline.hdrSceneColorAllocations<<"/"<<steady.hdrSceneColorAllocations<<" hdrRelease="<<baseline.hdrSceneColorReleases<<"/"<<steady.hdrSceneColorReleases<<" hdrBytes="<<baseline.hdrSceneColorBytes<<"/"<<steady.hdrSceneColorBytes<<" msaaAlloc="<<baseline.multisampleAllocations<<"/"<<steady.multisampleAllocations<<" msaaRelease="<<baseline.multisampleReleases<<"/"<<steady.multisampleReleases<<" msaaBytes="<<baseline.multisampleResidentBytes<<"/"<<steady.multisampleResidentBytes<<'\n';result=2;
  }
  ReflectionProbeCaptureSettings3D probe{};probe.resolution=32;renderer.SetReflectionProbeCaptureBudget(16*1024*1024);const auto handle=renderer.CreateReflectionProbeCapture3D(probe);if(!handle||!renderer.RequestReflectionProbeCaptureReadback3D(handle))result=3;
  D3D12RendererTestAccess3D::SetFixedProbeClearMismatch(renderer,true);renderer.Render(draw,{.02f,.03f,.04f,1});renderer.WaitIdle();D3D12RendererTestAccess3D::SetFixedProbeClearMismatch(renderer,false);
  const auto readback=renderer.TakeReflectionProbeCaptureReadback3D(handle);const RendererStats injected=renderer.GetStats();constexpr uint32_t mismatch=820;const bool strictGate=injected.gpuValidationRequested&&injected.gpuValidationAvailable&&injected.gpuValidationExecuted&&injected.gpuValidationPassed&&injected.debugErrorCount==0&&injected.debugWarningCount==0;
  for(size_t index=0;index<injected.debugWarningIdCount;++index)std::cerr<<"GBV warning id="<<injected.debugWarningIds[index]<<" occurrences="<<injected.debugWarningOccurrences[index]<<" description="<<injected.debugWarningDescriptions[index]<<'\n';
  if(!readback||!HasWarning(injected,mismatch)||strictGate)result=4;
  renderer.DestroyReflectionProbeCapture3D(handle);renderer.WaitIdle();std::cout<<"frames="<<steady.submittedFrames<<" hdr="<<steady.hdrSceneColorBytes<<" msaa="<<steady.multisampleResidentBytes<<" captures="<<steady.successfulFrameCaptures<<" probeReadback="<<bool(readback)<<" warning820="<<HasWarning(injected,mismatch)<<" strict="<<strictGate<<'\n';
 }DestroyWindow(window);return result;
}
