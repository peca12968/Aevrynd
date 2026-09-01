#include "Aevrynd/Renderer.h"
#include <iostream>
#include <limits>
#include <windows.h>

using namespace Aevrynd;

int main(){
 HWND window=CreateWindowExW(0,L"STATIC",L"AevryndObjectIdFresh",WS_OVERLAPPED,0,0,64,64,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);if(!window)return 1;int result{};
 try{
  D3D12Renderer renderer;renderer.Initialize(window,64,64,64,64,false);renderer.SetQualitySettings({false,false,false,false});
  DrawList empty;empty.SetViewProjection(Matrix4::Identity());const ObjectIdCaptureRequestMetadata3D emptyMetadata{1,2,3,ReadbackEmptyReason3D::EmptyScene};if(!renderer.RequestObjectIdReadback3D(emptyMetadata))result=2;renderer.Render(empty,{0,0,0,1});const auto emptyCapture=renderer.TakeObjectIdReadback3D();if(!emptyCapture||!emptyCapture->Complete()||emptyCapture->requestMetadata!=emptyMetadata||emptyCapture->semanticPixelCount||!emptyCapture->tokenMap.empty())result=3;
  const uint32_t normal=PackNormalSnorm8({0,0,-1},0);const Vertex3D vertices[]={{-.8f,-.8f,.3f,normal,0xffffffffu,0,0},{-.8f,.8f,.3f,normal,0xffffffffu,0,1},{.8f,.8f,.3f,normal,0xffffffffu,1,1},{.8f,-.8f,.3f,normal,0xffffffffu,1,0}};const uint32_t indices[]={0,1,2,0,2,3};const auto mesh=renderer.CreateMesh(vertices,indices);DrawList opaque;opaque.SetViewProjection(Matrix4::Identity());opaque.SetBackfaceCulling(false);const RenderObjectIdentity3D identity{{1,2},3};opaque.MeshInstance(mesh,Matrix4::Identity(),{1,1,1,1},{},MeshRenderLayer3D::Opaque,{},.25f,false,MaterialClass3D::Surface,std::numeric_limits<float>::quiet_NaN(),false,1.f,0,false,DirectionalShadowAllCascades3D,ShadowCascadeSelection3D::Automatic,Matrix4::Identity(),false,1.f,MaterialAlphaMode3D::Opaque,.5f,false,identity);const ObjectIdCaptureRequestMetadata3D metadata{4,5,6};if(!renderer.RequestObjectIdReadback3D(metadata))result=4;renderer.Render(opaque,{.02f,.03f,.04f,1});const auto capture=renderer.TakeObjectIdReadback3D();if(!capture||!capture->Complete()||!capture->semanticPixelCount||capture->tokenMap.size()!=1)result=5;renderer.DestroyMesh(mesh);renderer.WaitIdle();const auto stats=renderer.GetStats();const bool requireGpuValidation=GetEnvironmentVariableW(L"AEVRYND_GPU_VALIDATION",nullptr,0)>0;if(requireGpuValidation&&(!stats.gpuValidationRequested||!stats.gpuValidationAvailable||!stats.gpuValidationExecuted||!stats.debugLayerEnabled||!stats.gpuValidationPassed||stats.debugErrorCount||stats.debugWarningCount)){std::cerr<<"requested="<<stats.gpuValidationRequested<<" available="<<stats.gpuValidationAvailable<<" executed="<<stats.gpuValidationExecuted<<" errors="<<stats.debugErrorCount<<" warnings="<<stats.debugWarningCount<<" first="<<stats.debugFirstErrorId<<'\n';result=6;}
 }catch(const std::exception& error){std::cerr<<error.what()<<'\n';result=7;}DestroyWindow(window);return result;
}
