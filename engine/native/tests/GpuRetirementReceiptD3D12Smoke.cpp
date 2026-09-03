#include "Aevrynd/Renderer.h"
#include <chrono>
#include <cstddef>
#include <iostream>
#include <memory>
#include <optional>
#include <thread>
#include <windows.h>

using namespace Aevrynd;

namespace {
bool ValidationReceiptMatches(const RendererStats& stats){
 wchar_t expected[8]{};const DWORD length=GetEnvironmentVariableW(L"AEVRYND_EXPECT_GPU_VALIDATION_MODE",expected,DWORD(std::size(expected)));
 if(length==6&&lstrcmpW(expected,L"normal")==0)return!stats.gpuValidationRequested&&!stats.gpuValidationAvailable&&!stats.gpuValidationExecuted&&!stats.debugLayerEnabled&&!stats.gpuValidationPassed&&!stats.debugErrorCount&&!stats.debugWarningCount;
 if(length==3&&lstrcmpW(expected,L"gbv")==0)return stats.gpuValidationRequested&&stats.gpuValidationAvailable&&stats.gpuValidationExecuted&&stats.debugLayerEnabled&&stats.gpuValidationPassed&&!stats.debugErrorCount&&!stats.debugWarningCount;
 return false;
}

bool PollComplete(D3D12Renderer& renderer,GpuRetirementReceipt3D receipt){
 const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(10);
 while(std::chrono::steady_clock::now()<deadline){if(renderer.IsRetirementComplete3D(receipt))return true;std::this_thread::yield();}
 return false;
}
}

int main(){
 HWND window=CreateWindowExW(0,L"STATIC",L"AevryndGpuRetirementReceiptD3D12Smoke",WS_OVERLAPPED,0,0,320,180,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
 if(!window)return 1;
 int result{};GpuRetirementReceipt3D stale{};
 try{
  {
   D3D12Renderer renderer;renderer.Initialize(window,320,180,320,180,false);renderer.SetQualitySettings({false,false,false,false});
   DrawList draw{DrawListCapacityProfile::Minimal2D};draw.Quad({8,8},{40,30},{.2f,.5f,.8f,1});renderer.Render(draw,{0,0,0,1});
   const auto memoryBefore=renderer.MemoryStats();const auto statsBefore=renderer.GetStats();
   const auto first=renderer.CaptureRetirementFence3D();const auto second=renderer.CaptureRetirementFence3D();const auto memoryAfter=renderer.MemoryStats();const auto statsAfter=renderer.GetStats();
   if(!first||!second||first.DeviceEpoch()!=second.DeviceEpoch()||second.FenceValue()!=first.FenceValue()+1){std::cerr<<"Retirement receipts were not exact and monotonic\n";result=2;}
   if(!result&&(memoryAfter.trackedCapacityBytes!=memoryBefore.trackedCapacityBytes||memoryAfter.reclaimableCapacityBytes!=memoryBefore.reclaimableCapacityBytes||statsAfter.submittedFrames!=statsBefore.submittedFrames||statsAfter.meshUploadBatches!=statsBefore.meshUploadBatches||statsAfter.materialTextureUploadBatches!=statsBefore.materialTextureUploadBatches)){std::cerr<<"Capture allocated resources or submitted unrelated work\n";result=3;}
   // 完成必須由 receipt 對應的 queue fence 證明，這裡刻意不呼叫 WaitIdle。
   if(!result&&(!PollComplete(renderer,first)||!PollComplete(renderer,second))){std::cerr<<"Retirement receipt did not complete without WaitIdle\n";result=4;}
   renderer.TrimTransientMemory();if(!result&&!renderer.IsRetirementComplete3D(second)){std::cerr<<"Trim invalidated a completed receipt\n";result=5;}
   stale=second;renderer.WaitIdle();
   const auto validation=renderer.GetStats();if(!result&&!ValidationReceiptMatches(validation)){std::cerr<<"GPU validation receipt mismatch requested="<<validation.gpuValidationRequested<<" available="<<validation.gpuValidationAvailable<<" executed="<<validation.gpuValidationExecuted<<" passed="<<validation.gpuValidationPassed<<" errors="<<validation.debugErrorCount<<" warnings="<<validation.debugWarningCount<<'\n';result=6;}
  }
  alignas(D3D12Renderer) std::byte rendererStorage[sizeof(D3D12Renderer)]{};D3D12Renderer* resetRenderer{};GpuRetirementReceipt3D addressStale{};
  try{
   resetRenderer=std::construct_at(reinterpret_cast<D3D12Renderer*>(rendererStorage));resetRenderer->Initialize(window,320,180,320,180,false);
   if(!result&&resetRenderer->IsRetirementComplete3D(stale)){std::cerr<<"A stale device-epoch receipt completed after reset\n";result=7;}
   const GpuRetirementReceipt3D empty{};resetRenderer->WaitIdle();if(!result&&(resetRenderer->IsRetirementComplete3D(empty)||resetRenderer->IsRetirementComplete3D(stale))){std::cerr<<"WaitIdle accepted an invalid or stale receipt\n";result=8;}
   addressStale=resetRenderer->CaptureRetirementFence3D();if(!result&&(!addressStale||!PollComplete(*resetRenderer,addressStale))){std::cerr<<"Post-reset retirement receipt failed\n";result=9;}resetRenderer->WaitIdle();std::destroy_at(resetRenderer);resetRenderer=nullptr;
   // 同一記憶體位址重建 renderer 後，舊 receipt 仍必須被 device epoch 拒絕。
   resetRenderer=std::construct_at(reinterpret_cast<D3D12Renderer*>(rendererStorage));resetRenderer->Initialize(window,320,180,320,180,false);
   if(!result&&(resetRenderer->IsRetirementComplete3D(addressStale)||addressStale.DeviceEpoch()==resetRenderer->CaptureRetirementFence3D().DeviceEpoch())){std::cerr<<"Address-reused renderer accepted a stale receipt\n";result=11;}
   resetRenderer->WaitIdle();std::destroy_at(resetRenderer);resetRenderer=nullptr;
  }catch(...){if(resetRenderer)std::destroy_at(resetRenderer);throw;}
 }catch(const std::exception& error){std::cerr<<error.what()<<'\n';result=10;}
 DestroyWindow(window);return result;
}
