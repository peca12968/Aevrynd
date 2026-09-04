#pragma once
#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl/client.h>
#include <cstddef>
#include <cstdint>

namespace Aevrynd {
struct StreamlineApi;
struct StreamlineResourceStats {
    size_t residentBytes{};
    uint64_t allocations{};
    uint64_t releases{};
    bool resourcesResident{};
};
struct StreamlineSemanticReplayReceipt {
    uint32_t renderWidth{},renderHeight{},outputWidth{},outputHeight{},frameIndex{};
    uint64_t resourceGeneration{},resizeGeneration{};
    bool available{},currentFrame{},sampleable{},transitioned{};
    [[nodiscard]] bool Complete()const noexcept{return available&&currentFrame&&sampleable&&transitioned&&renderWidth&&renderHeight&&outputWidth&&outputHeight&&resourceGeneration&&resizeGeneration;}
};
class StreamlineDLSS {
public:
    StreamlineDLSS()=default;
    StreamlineDLSS(const StreamlineDLSS&)=delete;
    StreamlineDLSS& operator=(const StreamlineDLSS&)=delete;
    ~StreamlineDLSS();
    bool InitializeFramework();
    bool InitializeDevice(ID3D12Device*,IDXGIAdapter1*,uint32_t outputWidth,uint32_t outputHeight);
    HRESULT CreateFactory(UINT flags,REFIID interfaceId,void** factory)const;
    HRESULT CreateDevice(IUnknown* adapter,D3D_FEATURE_LEVEL featureLevel,REFIID interfaceId,void** device)const;
    bool Resize(uint32_t outputWidth,uint32_t outputHeight);
    void Shutdown();
    void BeginScene(ID3D12GraphicsCommandList*,const float clear[4]);
    bool Evaluate(ID3D12GraphicsCommandList*);
    StreamlineSemanticReplayReceipt BeginSemanticReplay(ID3D12GraphicsCommandList*);
    void FinishSemanticReplay(ID3D12GraphicsCommandList*);
    void FinishComposite(ID3D12GraphicsCommandList*);
    bool ValidateOutput();
    bool Supported()const{return supported_;}bool Enabled()const{return enabled_&&supported_;}
    bool FrameworkLoaded()const{return framework_;}
    StreamlineResourceStats Stats()const{return{residentBytes_,resourceAllocations_,resourceReleases_,residentBytes_>0};}
    void Toggle();
    uint32_t RenderWidth()const{return Enabled()?renderWidth_:outputWidth_;}
    uint32_t RenderHeight()const{return Enabled()?renderHeight_:outputHeight_;}
    D3D12_CPU_DESCRIPTOR_HANDLE SceneRenderTargetView()const{return rtvHeap_?rtvHeap_->GetCPUDescriptorHandleForHeapStart():D3D12_CPU_DESCRIPTOR_HANDLE{};}
    ID3D12Resource* OutputResource()const{return output_.Get();}
    ID3D12Resource* InputResource()const{return input_.Get();}
    ID3D12Resource* DepthResource()const{return depth_.Get();}
    StreamlineSemanticReplayReceipt SemanticReplayReceipt()const{return semanticReplayReceipt_;}
    D3D12_CPU_DESCRIPTOR_HANDLE DepthStencilView()const{return dsvHeap_?dsvHeap_->GetCPUDescriptorHandleForHeapStart():D3D12_CPU_DESCRIPTOR_HANDLE{};}
private:
    bool CreateResources(uint32_t outputWidth,uint32_t outputHeight);
    void ReleaseResources();
    bool framework_{},supported_{},enabled_{},reset_{true},outputValidated_{},semanticReplayActive_{},semanticReplayPending_{},featureEvaluated_{};uint32_t outputWidth_{},outputHeight_{},renderWidth_{},renderHeight_{},frameIndex_{},lastEvaluatedFrame_{},validationFrames_{},blackFrames_{};size_t residentBytes_{};uint64_t resourceAllocations_{},resourceReleases_{},resourceGeneration_{},resizeGeneration_{};
    StreamlineSemanticReplayReceipt semanticReplayReceipt_{};
    Microsoft::WRL::ComPtr<ID3D12Resource> input_,output_,motion_,depth_,validationReadback_;
    Microsoft::WRL::ComPtr<ID3D12Device> device_;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> rtvHeap_,dsvHeap_;
    StreamlineApi* api_{};
    UINT rtvStep_{};
};
}
