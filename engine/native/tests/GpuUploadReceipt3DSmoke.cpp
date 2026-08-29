#include "Aevrynd/Renderer.h"
#include <array>
#include <filesystem>
#include <iostream>
#include <windows.h>

using namespace Aevrynd;

int main(){
 HWND window=CreateWindowExW(0,L"STATIC",L"AevryndGpuUploadReceipt3DSmoke",WS_OVERLAPPED,0,0,320,180,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
 if(!window)return 1;
 int result{};
 try{
  D3D12Renderer renderer;renderer.Initialize(window,320,180,320,180,false);
  const std::array<Vertex3D,3> vertices={Vertex3D{-.25f,-.25f,.5f,0,0xffffffffu},Vertex3D{0,.25f,.5f,0,0xffffffffu},Vertex3D{.25f,-.25f,.5f,0,0xffffffffu}};
  constexpr std::array<uint32_t,3> indices={0,1,2};
  const MeshHandle3D mesh=renderer.CreateMesh(vertices,indices);const GpuUploadReceipt3D meshReceipt=renderer.SubmitPendingUploads3D();
  const RendererStats meshSubmitted=renderer.GetStats();const bool firstMeshState=renderer.IsUploadComplete(meshReceipt);const RendererStats meshQueried=renderer.GetStats();
  if(!mesh||!meshReceipt||meshSubmitted.meshUploadBatches!=1||meshQueried.meshUploadBatches!=meshSubmitted.meshUploadBatches){std::cerr<<"Mesh receipt did not submit exactly one upload batch\n";result=2;}
  renderer.WaitIdle();if(!result&&!renderer.IsUploadComplete(meshReceipt)){std::cerr<<"Mesh receipt did not observe the completed fence\n";result=3;}
  const MeshHandle3D cachedMesh=renderer.CreateMesh(vertices,indices);const GpuUploadReceipt3D cachedMeshReceipt=renderer.SubmitPendingUploads3D();if(!result&&(cachedMesh!=mesh||cachedMeshReceipt||!renderer.IsUploadComplete(cachedMeshReceipt))){std::cerr<<"Cached mesh unexpectedly queued an upload\n";result=4;}
  const std::filesystem::path root=std::filesystem::path{AEVRYND_SOURCE_DIR}/"engine"/"assets"/"material-packs"/"ambientcg"/"ground037"/"1k";
  MaterialTextureSetDefinition3D definition{root/"Ground037_1K-JPG_Color.jpg",root/"Ground037_1K-JPG_Roughness.jpg",64,root/"Ground037_1K-JPG_NormalDX.jpg"};
  const MaterialTextureSetHandle3D textures=renderer.CreateMaterialTextureSet(definition);const GpuUploadReceipt3D textureReceipt=renderer.SubmitPendingUploads3D();const RendererStats textureSubmitted=renderer.GetStats();const bool firstTextureState=renderer.IsUploadComplete(textureReceipt);const RendererStats textureQueried=renderer.GetStats();
  if(!result&&(!textures||!textureReceipt||textureSubmitted.materialTextureUploadBatches!=1||textureQueried.materialTextureUploadBatches!=textureSubmitted.materialTextureUploadBatches)){std::cerr<<"Texture receipt did not submit exactly one upload batch\n";result=5;}
  renderer.WaitIdle();if(!result&&!renderer.IsUploadComplete(textureReceipt)){std::cerr<<"Texture receipt did not observe the completed fence\n";result=6;}
  const auto cachedTextures=renderer.CreateMaterialTextureSet(definition);const GpuUploadReceipt3D cachedTextureReceipt=renderer.SubmitPendingUploads3D();if(!result&&(cachedTextures!=textures||cachedTextureReceipt||!renderer.IsUploadComplete(cachedTextureReceipt))){std::cerr<<"Cached texture unexpectedly queued an upload\n";result=7;}
  std::array<MeshHandle3D,4> batchedMeshes{};const uint64_t batchesBefore=renderer.GetStats().meshUploadBatches;for(size_t index=0;index<batchedMeshes.size();++index){auto changed=vertices;changed[0].x+=float(index+1)*.01f;batchedMeshes[index]=renderer.CreateMesh(changed,indices);}const GpuUploadReceipt3D batchReceipt=renderer.SubmitPendingUploads3D();const uint64_t batchesAfter=renderer.GetStats().meshUploadBatches;if(!result&&(!batchReceipt||batchesAfter!=batchesBefore+1)){std::cerr<<"Explicit upload submission did not preserve batching\n";result=9;}renderer.WaitIdle();if(!result&&!renderer.IsUploadComplete(batchReceipt)){std::cerr<<"Explicit batch receipt did not complete\n";result=10;}
  for(MeshHandle3D batched:batchedMeshes)renderer.DestroyMesh(batched);
  renderer.DestroyMaterialTextureSet(cachedTextures);renderer.DestroyMaterialTextureSet(textures);renderer.DestroyMesh(cachedMesh);renderer.DestroyMesh(mesh);renderer.WaitIdle();
  D3D12Renderer otherRenderer;if(!result&&otherRenderer.IsUploadComplete(meshReceipt)){std::cerr<<"Receipt from another device epoch was accepted\n";result=11;}
  std::cout<<"meshFence="<<meshReceipt.fenceValue<<" textureFence="<<textureReceipt.fenceValue<<" firstMeshComplete="<<firstMeshState<<" firstTextureComplete="<<firstTextureState<<'\n';
 }catch(const std::exception& error){std::cerr<<error.what()<<'\n';result=8;}
 DestroyWindow(window);return result;
}
