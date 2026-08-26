#pragma once
#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl/client.h>
#include <cstdint>

namespace Aevrynd {
class StreamlineDLSS {
public:
    bool InitializeFramework();
    bool InitializeDevice(ID3D12Device*,IDXGIAdapter1*,uint32_t outputWidth,uint32_t outputHeight);
    void Shutdown();
    void BeginScene(ID3D12GraphicsCommandList*,const float clear[4]);
    bool Evaluate(ID3D12GraphicsCommandList*,ID3D12Resource* backBuffer);
    void ValidateOutput();
    bool Supported()const{return supported_;}bool Enabled()const{return enabled_&&supported_;}
    void Toggle();
    uint32_t RenderWidth()const{return Enabled()?renderWidth_:outputWidth_;}
    uint32_t RenderHeight()const{return Enabled()?renderHeight_:outputHeight_;}
private:
    bool framework_{},supported_{},enabled_{true},reset_{true},outputValidated_{};uint32_t outputWidth_{},outputHeight_{},renderWidth_{},renderHeight_{},frameIndex_{},blackFrames_{};
    Microsoft::WRL::ComPtr<ID3D12Resource> input_,output_,motion_,depth_,validationReadback_;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> rtvHeap_,dsvHeap_;
    UINT rtvStep_{};
};
}
