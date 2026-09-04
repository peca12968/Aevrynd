#include "StreamlineDLSS.h"

namespace Aevrynd {
StreamlineDLSS::~StreamlineDLSS(){Shutdown();}
bool StreamlineDLSS::InitializeFramework(){return false;}
bool StreamlineDLSS::InitializeDevice(ID3D12Device*,IDXGIAdapter1*,uint32_t outputWidth,uint32_t outputHeight){outputWidth_=outputWidth;outputHeight_=outputHeight;renderWidth_=outputWidth;renderHeight_=outputHeight;return false;}
HRESULT StreamlineDLSS::CreateFactory(UINT flags,REFIID interfaceId,void** factory)const{return CreateDXGIFactory2(flags,interfaceId,factory);}
HRESULT StreamlineDLSS::CreateDevice(IUnknown* adapter,D3D_FEATURE_LEVEL featureLevel,REFIID interfaceId,void** device)const{return D3D12CreateDevice(adapter,featureLevel,interfaceId,device);}
bool StreamlineDLSS::Resize(uint32_t outputWidth,uint32_t outputHeight){outputWidth_=outputWidth;outputHeight_=outputHeight;renderWidth_=outputWidth;renderHeight_=outputHeight;return false;}
void StreamlineDLSS::Shutdown(){ReleaseResources();framework_=false;supported_=false;enabled_=false;api_=nullptr;}
void StreamlineDLSS::BeginScene(ID3D12GraphicsCommandList*,const float[4]){}
bool StreamlineDLSS::Evaluate(ID3D12GraphicsCommandList*){return false;}
StreamlineSemanticReplayReceipt StreamlineDLSS::BeginSemanticReplay(ID3D12GraphicsCommandList*){semanticReplayReceipt_={};return semanticReplayReceipt_;}
void StreamlineDLSS::FinishSemanticReplay(ID3D12GraphicsCommandList*){semanticReplayActive_=false;}
void StreamlineDLSS::FinishComposite(ID3D12GraphicsCommandList*){}
bool StreamlineDLSS::ValidateOutput(){return true;}
void StreamlineDLSS::Toggle(){}
bool StreamlineDLSS::CreateResources(uint32_t,uint32_t){return false;}
void StreamlineDLSS::ReleaseResources(){input_.Reset();output_.Reset();motion_.Reset();depth_.Reset();validationReadback_.Reset();rtvHeap_.Reset();dsvHeap_.Reset();device_.Reset();validationFrames_=blackFrames_=0;residentBytes_=0;}
}
