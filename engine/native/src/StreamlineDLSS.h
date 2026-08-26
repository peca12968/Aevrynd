#pragma once
#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl/client.h>
#include <cstddef>
#include <cstdint>

namespace Aevrynd {
struct StreamlineResourceStats {
    size_t residentBytes{};
    uint64_t allocations{};
    uint64_t releases{};
    bool resourcesResident{};
};
class StreamlineDLSS {
public:
    bool InitializeFramework();
    bool InitializeDevice(ID3D12Device*,IDXGIAdapter1*,uint32_t outputWidth,uint32_t outputHeight);
    bool Resize(uint32_t outputWidth,uint32_t outputHeight);
    void Shutdown();
    void BeginScene(ID3D12GraphicsCommandList*,const float clear[4]);
    bool Evaluate(ID3D12GraphicsCommandList*,ID3D12Resource* backBuffer);
    bool ValidateOutput();
    bool Supported()const{return supported_;}bool Enabled()const{return enabled_&&supported_;}
    StreamlineResourceStats Stats()const{return{residentBytes_,resourceAllocations_,resourceReleases_,residentBytes_>0};}
    void Toggle();
    uint32_t RenderWidth()const{return Enabled()?renderWidth_:outputWidth_;}
    uint32_t RenderHeight()const{return Enabled()?renderHeight_:outputHeight_;}
    D3D12_CPU_DESCRIPTOR_HANDLE DepthStencilView()const{return dsvHeap_?dsvHeap_->GetCPUDescriptorHandleForHeapStart():D3D12_CPU_DESCRIPTOR_HANDLE{};}
private:
    bool CreateResources(uint32_t outputWidth,uint32_t outputHeight);
    void ReleaseResources();
    bool framework_{},supported_{},enabled_{},reset_{true},outputValidated_{};uint32_t outputWidth_{},outputHeight_{},renderWidth_{},renderHeight_{},frameIndex_{},blackFrames_{};size_t residentBytes_{};uint64_t resourceAllocations_{},resourceReleases_{};
    Microsoft::WRL::ComPtr<ID3D12Resource> input_,output_,motion_,depth_,validationReadback_;
    Microsoft::WRL::ComPtr<ID3D12Device> device_;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> rtvHeap_,dsvHeap_;
    UINT rtvStep_{};
};
}
