#include <Aevrynd/Renderer.h>
#include <algorithm>
#include <chrono>
#include <filesystem>
#include <iostream>
#include <vector>
using namespace Aevrynd;
static uint64_t Median(std::vector<uint64_t> values){std::sort(values.begin(),values.end());return values.empty()?0:values[values.size()/2];}
int main(){HINSTANCE instance=GetModuleHandleW(nullptr);
 const wchar_t* className=L"AevryndBloomBenchmark";WNDCLASSW wc{};wc.lpfnWndProc=DefWindowProcW;wc.hInstance=instance;wc.lpszClassName=className;RegisterClassW(&wc);HWND window=CreateWindowW(className,L"Bloom benchmark",WS_OVERLAPPED,0,0,640,360,nullptr,nullptr,instance,nullptr);if(!window)return 1;
 D3D12Renderer renderer;renderer.Initialize(window,640,360,640,360,false);renderer.SetQualitySettings({false,false,false,false});DrawList draw{DrawListCapacityProfile::Minimal2D};draw.WorldTriangle({-.08f,-.08f,.5f},{0,.08f,.5f},{.08f,-.08f,.5f},{8.f,6.f,2.f,1.f});
 struct Result{uint64_t gpu{},cpu{};RendererStats stats{};};auto run=[&](bool enabled,const std::filesystem::path& capture){draw.SetBloom({enabled,.2f,.5f,.5f,2.5f});std::vector<uint64_t> gpu,cpu;for(int i=0;i<64;++i){const auto begin=std::chrono::steady_clock::now();renderer.Render(draw,{.01f,.01f,.015f,1});const auto end=std::chrono::steady_clock::now();if(i>=16){cpu.push_back(uint64_t(std::chrono::duration_cast<std::chrono::nanoseconds>(end-begin).count()));if(renderer.GetStats().lastGpuFrameNanoseconds)gpu.push_back(renderer.GetStats().lastGpuFrameNanoseconds);}}renderer.CaptureNextFrame(capture);renderer.Render(draw,{.01f,.01f,.015f,1});renderer.WaitIdle();return Result{Median(gpu),Median(cpu),renderer.GetStats()};};
 auto off=run(false,"build-render-bloom/bloom-disabled.png"),on=run(true,"build-render-bloom/bloom-enabled.png");DestroyWindow(window);std::cout<<"bloom_off_gpu_ns="<<off.gpu<<" bloom_on_gpu_ns="<<on.gpu<<" gpu_delta_ns="<<(on.gpu>off.gpu?on.gpu-off.gpu:0)<<" cpu_off_ns="<<off.cpu<<" cpu_on_ns="<<on.cpu<<" resident_bytes="<<on.stats.bloomResidentBytes<<" samples="<<on.stats.bloomTextureSamples<<" luminance_off="<<off.stats.lastCapturedAverageLuminance<<" luminance_on="<<on.stats.lastCapturedAverageLuminance<<'\n';return !on.stats.bloomEnabled||off.stats.bloomEnabled||on.stats.bloomResidentBytes||on.stats.successfulFrameCaptures<2||on.stats.lastCapturedAverageLuminance<=off.stats.lastCapturedAverageLuminance?2:0;
}
