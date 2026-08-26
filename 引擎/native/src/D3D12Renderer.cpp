#include "Aevrynd/Renderer.h"
#include "StreamlineDLSS.h"
#include <d3d12.h>
#include <dxgi1_6.h>
#include <d3dcompiler.h>
#include <wrl/client.h>
#include <array>
#include <cstring>
#include <stdexcept>
#include <unordered_map>

using Microsoft::WRL::ComPtr;
namespace Aevrynd {
static void Check(HRESULT hr,const char* message){if(FAILED(hr))throw std::runtime_error(message);}

void DrawList::Triangle(Vec2 a,Vec2 b,Vec2 c,Color color){vertices.push_back({a.x,a.y,color});vertices.push_back({b.x,b.y,color});vertices.push_back({c.x,c.y,color});}
void DrawList::Quad(Vec2 c,Vec2 s,Color col){Vec2 a{c.x-s.x*.5f,c.y-s.y*.5f},b{c.x+s.x*.5f,c.y-s.y*.5f},d{c.x-s.x*.5f,c.y+s.y*.5f},e{c.x+s.x*.5f,c.y+s.y*.5f};Triangle(a,b,e,col);Triangle(a,e,d,col);}
void DrawList::Diamond(Vec2 c,float rx,float ry,Color col){Triangle({c.x,c.y-ry},{c.x+rx,c.y},{c.x,c.y+ry},col);Triangle({c.x,c.y-ry},{c.x,c.y+ry},{c.x-rx,c.y},col);}
void DrawList::IsoPrism(Vec2 c,float rx,float ry,float h,Color top,Color left,Color right){Vec2 t{c.x,c.y-ry-h},r{c.x+rx,c.y-h},b{c.x,c.y+ry-h},l{c.x-rx,c.y-h},bottom{c.x,c.y+ry},bottomLeft{c.x-rx,c.y},bottomRight{c.x+rx,c.y};Triangle(l,b,bottom,left);Triangle(l,bottom,bottomLeft,left);Triangle(b,r,bottomRight,right);Triangle(b,bottomRight,bottom,right);Triangle(t,r,b,top);Triangle(t,b,l,top);}
void DrawList::Ellipse(Vec2 c,float rx,float ry,Color col,int n){for(int i=0;i<n;i++){float a=float(i)*6.2831853f/n,b=float(i+1)*6.2831853f/n;Triangle(c,{c.x+std::cos(a)*rx,c.y+std::sin(a)*ry},{c.x+std::cos(b)*rx,c.y+std::sin(b)*ry},col);}}
void DrawList::Circle(Vec2 c,float r,Color col,int n){Ellipse(c,r,r,col,n);}
void DrawList::Line(Vec2 a,Vec2 b,float w,Color col){Vec2 n=Normalize({-(b.y-a.y),b.x-a.x})*(w*.5f);Triangle(a+n,b+n,b-n,col);Triangle(a+n,b-n,a-n,col);}

static const std::unordered_map<char,std::array<unsigned char,7>> Font={
 {'A',{14,17,17,31,17,17,17}},{'B',{30,17,17,30,17,17,30}},{'C',{14,17,16,16,16,17,14}},
 {'D',{30,17,17,17,17,17,30}},{'E',{31,16,16,30,16,16,31}},{'F',{31,16,16,30,16,16,16}},
 {'G',{14,17,16,23,17,17,15}},{'H',{17,17,17,31,17,17,17}},{'I',{14,4,4,4,4,4,14}},
 {'J',{7,2,2,2,18,18,12}},{'K',{17,18,20,24,20,18,17}},{'L',{16,16,16,16,16,16,31}},
 {'M',{17,27,21,21,17,17,17}},{'N',{17,25,21,19,17,17,17}},{'O',{14,17,17,17,17,17,14}},
 {'P',{30,17,17,30,16,16,16}},{'Q',{14,17,17,17,21,18,13}},{'R',{30,17,17,30,20,18,17}},
 {'S',{15,16,16,14,1,1,30}},{'T',{31,4,4,4,4,4,4}},{'U',{17,17,17,17,17,17,14}},
 {'V',{17,17,17,17,17,10,4}},{'W',{17,17,17,21,21,21,10}},{'X',{17,17,10,4,10,17,17}},
 {'Y',{17,17,10,4,4,4,4}},{'Z',{31,1,2,4,8,16,31}},
 {'0',{14,17,19,21,25,17,14}},{'1',{4,12,4,4,4,4,14}},{'2',{14,17,1,2,4,8,31}},
 {'3',{30,1,1,14,1,1,30}},{'4',{2,6,10,18,31,2,2}},{'5',{31,16,16,30,1,1,30}},
 {'6',{14,16,16,30,17,17,14}},{'7',{31,1,2,4,8,8,8}},{'8',{14,17,17,14,17,17,14}},
 {'9',{14,17,17,15,1,1,14}},{':',{0,4,4,0,4,4,0}},{'-',{0,0,0,31,0,0,0}},
 {'/',{1,2,4,8,16,0,0}},{'.',{0,0,0,0,0,12,12}},{' ',{0,0,0,0,0,0,0}}
};
struct SystemGlyph{int width{},height{},advance{18};std::vector<unsigned char> pixels;};
static const SystemGlyph& LoadSystemGlyph(uint32_t codepoint){
 static std::unordered_map<uint32_t,SystemGlyph> cache;auto found=cache.find(codepoint);if(found!=cache.end())return found->second;
 static HDC dc=[](){HDC value=CreateCompatibleDC(nullptr);HFONT font=CreateFontW(-18,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,CHINESEBIG5_CHARSET,OUT_TT_PRECIS,CLIP_DEFAULT_PRECIS,ANTIALIASED_QUALITY,DEFAULT_PITCH|FF_DONTCARE,L"Microsoft JhengHei UI");SelectObject(value,font);return value;}();
 SystemGlyph glyph;GLYPHMETRICS metrics{};MAT2 matrix{};matrix.eM11.value=1;matrix.eM22.value=1;DWORD bytes=GetGlyphOutlineW(dc,codepoint,GGO_GRAY8_BITMAP,&metrics,0,nullptr,&matrix);if(bytes!=GDI_ERROR){glyph.width=int(metrics.gmBlackBoxX);glyph.height=int(metrics.gmBlackBoxY);glyph.advance=(std::max)(int(metrics.gmCellIncX),glyph.width);glyph.pixels.resize(bytes);GetGlyphOutlineW(dc,codepoint,GGO_GRAY8_BITMAP,&metrics,bytes,glyph.pixels.data(),&matrix);}return cache.emplace(codepoint,std::move(glyph)).first->second;
}
static uint32_t NextUtf8(std::string_view text,size_t& i){unsigned char a=static_cast<unsigned char>(text[i++]);if(a<0x80)return a;if((a&0xe0)==0xc0&&i<text.size())return((a&0x1f)<<6)|(static_cast<unsigned char>(text[i++])&0x3f);if((a&0xf0)==0xe0&&i+1<text.size()){uint32_t v=(a&0x0f)<<12;v|=(static_cast<unsigned char>(text[i++])&0x3f)<<6;v|=static_cast<unsigned char>(text[i++])&0x3f;return v;}if((a&0xf8)==0xf0&&i+2<text.size()){uint32_t v=(a&7)<<18;v|=(static_cast<unsigned char>(text[i++])&0x3f)<<12;v|=(static_cast<unsigned char>(text[i++])&0x3f)<<6;v|=static_cast<unsigned char>(text[i++])&0x3f;return v;}return'?';}
void DrawList::Text(Vec2 p,std::string_view text,float s,Color col){for(size_t i=0;i<text.size();){uint32_t code=NextUtf8(text,i);if(code<128){char ch=char(code);ch=ch>='a'&&ch<='z'?char(ch-32):ch;auto it=Font.find(ch);if(it!=Font.end())for(int y=0;y<7;y++)for(int x=0;x<5;x++)if(it->second[y]&(1<<(4-x)))Quad({p.x+x*s,p.y+y*s},{s*.82f,s*.82f},col);p.x+=6*s;continue;}const auto& glyph=LoadSystemGlyph(code);float pixel=7.f*s/18.f;int pitch=(glyph.width+3)&~3;for(int y=0;y<glyph.height;y++)for(int x=0;x<glyph.width;x++){unsigned char coverage=glyph.pixels.empty()?0:glyph.pixels[size_t(y)*pitch+x];if(coverage)Quad({p.x+(x+.5f)*pixel,p.y+(y+.5f)*pixel},{pixel,pixel},{col.r,col.g,col.b,col.a*(std::min)(1.f,coverage/64.f)});}p.x+=(glyph.advance+1)*pixel;}}

struct D3D12Renderer::Impl{
 static constexpr unsigned Frames=2; static constexpr size_t MaxVertices=240000;
 unsigned width{},height{},frame{};UINT rtvStep{};HANDLE fenceEvent{};UINT64 fenceValue{};
 ComPtr<IDXGIFactory6> factory;ComPtr<ID3D12Device> device;ComPtr<ID3D12CommandQueue> queue;ComPtr<IDXGISwapChain3> swap;
 ComPtr<ID3D12DescriptorHeap> rtvHeap;std::array<ComPtr<ID3D12Resource>,Frames> targets;std::array<ComPtr<ID3D12CommandAllocator>,Frames> alloc;
 ComPtr<ID3D12GraphicsCommandList> list;ComPtr<ID3D12Fence> fence;ComPtr<ID3D12RootSignature> root;ComPtr<ID3D12PipelineState> pipeline;ComPtr<ID3D12Resource> vertexBuffer;Vertex* mapped{};StreamlineDLSS dlss;
 D3D12_VERTEX_BUFFER_VIEW vbv{};
 void Wait(){const UINT64 v=++fenceValue;Check(queue->Signal(fence.Get(),v),"Signal failed");if(fence->GetCompletedValue()<v){Check(fence->SetEventOnCompletion(v,fenceEvent),"Fence event failed");WaitForSingleObject(fenceEvent,INFINITE);}frame=swap->GetCurrentBackBufferIndex();}
 void Targets(){D3D12_CPU_DESCRIPTOR_HANDLE h=rtvHeap->GetCPUDescriptorHandleForHeapStart();for(unsigned i=0;i<Frames;i++){Check(swap->GetBuffer(i,IID_PPV_ARGS(&targets[i])),"Back buffer failed");device->CreateRenderTargetView(targets[i].Get(),nullptr,h);h.ptr+=rtvStep;}}
};

D3D12Renderer::D3D12Renderer():impl_(std::make_unique<Impl>()){} D3D12Renderer::~D3D12Renderer(){if(impl_&&impl_->device){impl_->Wait();if(impl_->vertexBuffer)impl_->vertexBuffer->Unmap(0,nullptr);impl_->dlss.Shutdown();}if(impl_&&impl_->fenceEvent)CloseHandle(impl_->fenceEvent);}
void D3D12Renderer::Initialize(HWND hwnd,unsigned width,unsigned height){auto& d=*impl_;d.width=width;d.height=height;
 d.dlss.InitializeFramework();
#ifdef _DEBUG
 ComPtr<ID3D12Debug> debug;if(SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debug))))debug->EnableDebugLayer();
#endif
 UINT flags=0;Check(CreateDXGIFactory2(flags,IID_PPV_ARGS(&d.factory)),"DXGI factory failed");
 ComPtr<IDXGIAdapter1> adapter;for(UINT i=0;d.factory->EnumAdapterByGpuPreference(i,DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,IID_PPV_ARGS(&adapter))!=DXGI_ERROR_NOT_FOUND;i++){DXGI_ADAPTER_DESC1 desc{};adapter->GetDesc1(&desc);if(!(desc.Flags&DXGI_ADAPTER_FLAG_SOFTWARE)&&SUCCEEDED(D3D12CreateDevice(adapter.Get(),D3D_FEATURE_LEVEL_12_0,IID_PPV_ARGS(&d.device))))break;adapter.Reset();}
 if(!d.device)Check(D3D12CreateDevice(nullptr,D3D_FEATURE_LEVEL_12_0,IID_PPV_ARGS(&d.device)),"DirectX 12 device unavailable");d.dlss.InitializeDevice(d.device.Get(),adapter.Get(),width,height);
 D3D12_COMMAND_QUEUE_DESC q{};q.Type=D3D12_COMMAND_LIST_TYPE_DIRECT;Check(d.device->CreateCommandQueue(&q,IID_PPV_ARGS(&d.queue)),"Queue failed");
 DXGI_SWAP_CHAIN_DESC1 sc{};sc.Width=width;sc.Height=height;sc.Format=DXGI_FORMAT_R8G8B8A8_UNORM;sc.SampleDesc.Count=1;sc.BufferUsage=DXGI_USAGE_RENDER_TARGET_OUTPUT;sc.BufferCount=Impl::Frames;sc.SwapEffect=DXGI_SWAP_EFFECT_FLIP_DISCARD;
 ComPtr<IDXGISwapChain1> swap1;Check(d.factory->CreateSwapChainForHwnd(d.queue.Get(),hwnd,&sc,nullptr,nullptr,&swap1),"Swap chain failed");Check(swap1.As(&d.swap),"Swap cast failed");d.frame=d.swap->GetCurrentBackBufferIndex();
 D3D12_DESCRIPTOR_HEAP_DESC hd{};hd.NumDescriptors=Impl::Frames;hd.Type=D3D12_DESCRIPTOR_HEAP_TYPE_RTV;Check(d.device->CreateDescriptorHeap(&hd,IID_PPV_ARGS(&d.rtvHeap)),"RTV heap failed");d.rtvStep=d.device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);d.Targets();
 for(auto& a:d.alloc)Check(d.device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,IID_PPV_ARGS(&a)),"Allocator failed");Check(d.device->CreateCommandList(0,D3D12_COMMAND_LIST_TYPE_DIRECT,d.alloc[0].Get(),nullptr,IID_PPV_ARGS(&d.list)),"Command list failed");d.list->Close();
 const char* vs="struct V{float2 p:POSITION;float4 c:COLOR;};struct O{float4 p:SV_POSITION;float4 c:COLOR;};O main(V i){O o;o.p=float4(i.p.x/640-1,1-i.p.y/360,0,1);o.c=i.c;return o;}";
 const char* ps="struct I{float4 p:SV_POSITION;float4 c:COLOR;};float4 main(I i):SV_TARGET{return i.c;}";ComPtr<ID3DBlob> vblob,pblob,error;Check(D3DCompile(vs,strlen(vs),nullptr,nullptr,nullptr,"main","vs_5_0",0,0,&vblob,&error),"Vertex shader failed");Check(D3DCompile(ps,strlen(ps),nullptr,nullptr,nullptr,"main","ps_5_0",0,0,&pblob,&error),"Pixel shader failed");
 D3D12_ROOT_SIGNATURE_DESC rs{};rs.Flags=D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;ComPtr<ID3DBlob> sig;Check(D3D12SerializeRootSignature(&rs,D3D_ROOT_SIGNATURE_VERSION_1,&sig,&error),"Root signature serialize failed");Check(d.device->CreateRootSignature(0,sig->GetBufferPointer(),sig->GetBufferSize(),IID_PPV_ARGS(&d.root)),"Root signature failed");
 D3D12_INPUT_ELEMENT_DESC input[]={{"POSITION",0,DXGI_FORMAT_R32G32_FLOAT,0,0,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0},{"COLOR",0,DXGI_FORMAT_R32G32B32A32_FLOAT,0,8,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0}};
 D3D12_GRAPHICS_PIPELINE_STATE_DESC p{};p.InputLayout={input,2};p.pRootSignature=d.root.Get();p.VS={vblob->GetBufferPointer(),vblob->GetBufferSize()};p.PS={pblob->GetBufferPointer(),pblob->GetBufferSize()};p.RasterizerState={D3D12_FILL_MODE_SOLID,D3D12_CULL_MODE_NONE,FALSE,0,0.f,0.f,TRUE,FALSE,FALSE,0,D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF};p.BlendState.AlphaToCoverageEnable=FALSE;p.BlendState.IndependentBlendEnable=FALSE;auto& rt=p.BlendState.RenderTarget[0];rt.BlendEnable=TRUE;rt.SrcBlend=D3D12_BLEND_SRC_ALPHA;rt.DestBlend=D3D12_BLEND_INV_SRC_ALPHA;rt.BlendOp=D3D12_BLEND_OP_ADD;rt.SrcBlendAlpha=D3D12_BLEND_ONE;rt.DestBlendAlpha=D3D12_BLEND_ZERO;rt.BlendOpAlpha=D3D12_BLEND_OP_ADD;rt.RenderTargetWriteMask=D3D12_COLOR_WRITE_ENABLE_ALL;p.SampleMask=UINT_MAX;p.PrimitiveTopologyType=D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;p.NumRenderTargets=1;p.RTVFormats[0]=DXGI_FORMAT_R8G8B8A8_UNORM;p.SampleDesc.Count=1;Check(d.device->CreateGraphicsPipelineState(&p,IID_PPV_ARGS(&d.pipeline)),"Pipeline failed");
 D3D12_HEAP_PROPERTIES hp{};hp.Type=D3D12_HEAP_TYPE_UPLOAD;D3D12_RESOURCE_DESC rd{};rd.Dimension=D3D12_RESOURCE_DIMENSION_BUFFER;rd.Width=Impl::MaxVertices*sizeof(Vertex);rd.Height=1;rd.DepthOrArraySize=1;rd.MipLevels=1;rd.SampleDesc.Count=1;rd.Layout=D3D12_TEXTURE_LAYOUT_ROW_MAJOR;Check(d.device->CreateCommittedResource(&hp,D3D12_HEAP_FLAG_NONE,&rd,D3D12_RESOURCE_STATE_GENERIC_READ,nullptr,IID_PPV_ARGS(&d.vertexBuffer)),"Vertex buffer failed");Check(d.vertexBuffer->Map(0,nullptr,reinterpret_cast<void**>(&d.mapped)),"Map failed");d.vbv={d.vertexBuffer->GetGPUVirtualAddress(),UINT(rd.Width),sizeof(Vertex)};Check(d.device->CreateFence(0,D3D12_FENCE_FLAG_NONE,IID_PPV_ARGS(&d.fence)),"Fence failed");d.fenceEvent=CreateEvent(nullptr,FALSE,FALSE,nullptr);
}
void D3D12Renderer::Render(const DrawList& dl,Color clear){auto& d=*impl_;size_t count=(std::min)(dl.vertices.size(),Impl::MaxVertices);memcpy(d.mapped,dl.vertices.data(),count*sizeof(Vertex));Check(d.alloc[d.frame]->Reset(),"Allocator reset failed");Check(d.list->Reset(d.alloc[d.frame].Get(),d.pipeline.Get()),"List reset failed");float c[4]={clear.r,clear.g,clear.b,clear.a};
 const bool useDLSS=d.dlss.Enabled();D3D12_RESOURCE_BARRIER toRT{};D3D12_CPU_DESCRIPTOR_HANDLE rtv{};
 if(useDLSS){d.dlss.BeginScene(d.list.Get(),c);}else{toRT.Type=D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;toRT.Transition={d.targets[d.frame].Get(),D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,D3D12_RESOURCE_STATE_PRESENT,D3D12_RESOURCE_STATE_RENDER_TARGET};d.list->ResourceBarrier(1,&toRT);rtv=d.rtvHeap->GetCPUDescriptorHandleForHeapStart();rtv.ptr+=SIZE_T(d.frame)*d.rtvStep;d.list->ClearRenderTargetView(rtv,c,0,nullptr);d.list->OMSetRenderTargets(1,&rtv,FALSE,nullptr);}
 const unsigned rw=useDLSS?d.dlss.RenderWidth():d.width,rh=useDLSS?d.dlss.RenderHeight():d.height;D3D12_VIEWPORT vp{0,0,float(rw),float(rh),0,1};D3D12_RECT sr{0,0,LONG(rw),LONG(rh)};d.list->RSSetViewports(1,&vp);d.list->RSSetScissorRects(1,&sr);d.list->SetGraphicsRootSignature(d.root.Get());d.list->SetPipelineState(d.pipeline.Get());d.list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);d.list->IASetVertexBuffers(0,1,&d.vbv);d.list->DrawInstanced(UINT(count),1,0,0);
 if(useDLSS){if(!d.dlss.Evaluate(d.list.Get(),d.targets[d.frame].Get()))d.dlss.Toggle();}else{std::swap(toRT.Transition.StateBefore,toRT.Transition.StateAfter);d.list->ResourceBarrier(1,&toRT);}Check(d.list->Close(),"Close failed");ID3D12CommandList* lists[]={d.list.Get()};d.queue->ExecuteCommandLists(1,lists);Check(d.swap->Present(1,0),"Present failed");d.Wait();if(useDLSS)d.dlss.ValidateOutput();}
void D3D12Renderer::WaitIdle(){if(impl_&&impl_->queue)impl_->Wait();}void D3D12Renderer::Resize(unsigned,unsigned){}
bool D3D12Renderer::SupportsDLSS()const{return impl_&&impl_->dlss.Supported();}bool D3D12Renderer::DLSSOn()const{return impl_&&impl_->dlss.Enabled();}void D3D12Renderer::ToggleDLSS(){if(impl_)impl_->dlss.Toggle();}unsigned D3D12Renderer::RenderWidth()const{return impl_?(impl_->dlss.Enabled()?impl_->dlss.RenderWidth():impl_->width):0;}unsigned D3D12Renderer::RenderHeight()const{return impl_?(impl_->dlss.Enabled()?impl_->dlss.RenderHeight():impl_->height):0;}
}
