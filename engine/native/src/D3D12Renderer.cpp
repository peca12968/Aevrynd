#include "Aevrynd/Renderer.h"
#include "Aevrynd/FrameCapture3D.h"
#include "StreamlineDLSS.h"
#include "CanvasPixelShader.h"
#include "CanvasVertexShader.h"
#include "InstancedVertexShader.h"
#include "WorldPixelShader.h"
#include "WorldVertexShader.h"
#include "ShadowWorldVertexShader.h"
#include "ShadowInstancedVertexShader.h"
#include "PostProcessPixelShader.h"
#include "PostProcessVertexShader.h"
#include "AmbientOcclusionVertexShader.h"
#include "AmbientOcclusionPixelShader.h"
#include "AmbientOcclusionBilateralBlurPixelShader.h"
#include "AmbientOcclusionDepthWorldVertexShader.h"
#include "AmbientOcclusionDepthInstancedVertexShader.h"
#include "AmbientOcclusionDepthPixelShader.h"
#include "SkinnedVertexShader.h"
#include "ShadowSkinnedVertexShader.h"
#include "AmbientOcclusionDepthSkinnedVertexShader.h"
#include <d3d12.h>
#include <dxgi1_6.h>
#include <wincodec.h>
#include <wrl/client.h>
#include <algorithm>
#include <array>
#include <chrono>
#include <cstring>
#include <cwctype>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <unordered_map>
#include <utility>

using Microsoft::WRL::ComPtr;
namespace Aevrynd {
static void Check(HRESULT hr,const char* message){if(FAILED(hr))throw std::runtime_error(message);}
Matrix4 CreateDirectionalLightViewProjection3D(Vec3 center,Vec3 direction,float extent=80.f){direction=Normalize(direction);if(Length(direction)<.001f)direction={.4f,-1.f,.3f};const Vec3 forward=direction;Vec3 right=Normalize(Cross({0,1,0},forward));if(Length(right)<.001f)right={1,0,0};const Vec3 up=Cross(forward,right);const float texelWorldSize=extent*2.f/2048.f;const float rightCoordinate=Dot(center,right),upCoordinate=Dot(center,up);center=center+right*(SnapDirectionalShadowCoordinate3D(rightCoordinate,texelWorldSize)-rightCoordinate)+up*(SnapDirectionalShadowCoordinate3D(upCoordinate,texelWorldSize)-upCoordinate);const Vec3 eye=center-direction*100.f;Matrix4 view=Matrix4::Identity();view[0][0]=right.x;view[0][1]=right.y;view[0][2]=right.z;view[0][3]=-Dot(right,eye);view[1][0]=up.x;view[1][1]=up.y;view[1][2]=up.z;view[1][3]=-Dot(up,eye);view[2][0]=forward.x;view[2][1]=forward.y;view[2][2]=forward.z;view[2][3]=-Dot(forward,eye);Matrix4 projection{};projection[0][0]=1.f/extent;projection[1][1]=1.f/extent;projection[2][2]=1.f/240.f;projection[2][3]=40.f/240.f;projection[3][3]=1.f;return projection*view;}
struct ResidentMeshBounds3D {Vec3 center{},extents{};};
bool CalculateResidentMeshBounds3D(std::span<const Vertex3D> vertices,ResidentMeshBounds3D& result){
 if(vertices.empty())return false;double minimum[3]={vertices.front().x,vertices.front().y,vertices.front().z},maximum[3]={minimum[0],minimum[1],minimum[2]};
 for(const Vertex3D& vertex:vertices){const double values[3]={vertex.x,vertex.y,vertex.z};for(size_t axis=0;axis<3;++axis){if(!std::isfinite(values[axis]))return false;minimum[axis]=(std::min)(minimum[axis],values[axis]);maximum[axis]=(std::max)(maximum[axis],values[axis]);}}
 double center[3]{},extents[3]{};for(size_t axis=0;axis<3;++axis){center[axis]=minimum[axis]+(maximum[axis]-minimum[axis])*.5;extents[axis]=(maximum[axis]-minimum[axis])*.5;if(!std::isfinite(center[axis])||!std::isfinite(extents[axis])||std::abs(center[axis])>(std::numeric_limits<float>::max)()||extents[axis]>(std::numeric_limits<float>::max)())return false;}
 result={{float(center[0]),float(center[1]),float(center[2])},{float(extents[0]),float(extents[1]),float(extents[2])}};return true;
}
struct DirectionalShadowLightSpaceMaskResult3D {uint8_t mask{DirectionalShadowAllCascades3D};bool fallback{};};
DirectionalShadowLightSpaceMaskResult3D DirectionalShadowLightSpaceMask3D(const ResidentMeshBounds3D& bounds,const Matrix4& transform,const Matrix4 (&lightViewProjection)[2]){
 constexpr float MarginXY=2.f*(1.45f+1.f)/2048.f,MarginZ=1e-5f;
 auto finite=[](float value){return std::isfinite(value);};for(float value:{bounds.center.x,bounds.center.y,bounds.center.z,bounds.extents.x,bounds.extents.y,bounds.extents.z})if(!finite(value))return{DirectionalShadowAllCascades3D,true};if(bounds.extents.x<0||bounds.extents.y<0||bounds.extents.z<0)return{DirectionalShadowAllCascades3D,true};
 uint8_t mask{};for(size_t cascade=0;cascade<2;++cascade){const Matrix4 clipFromLocal=lightViewProjection[cascade]*transform;for(const auto& row:clipFromLocal.values)for(float value:row)if(!finite(value))return{DirectionalShadowAllCascades3D,true};if(std::abs(clipFromLocal[3][0])+std::abs(clipFromLocal[3][1])+std::abs(clipFromLocal[3][2])>1e-5f)return{DirectionalShadowAllCascades3D,true};const Vec4 center=clipFromLocal*Vec4{bounds.center.x,bounds.center.y,bounds.center.z,1};if(!finite(center.x)||!finite(center.y)||!finite(center.z)||!finite(center.w)||center.w<=0)return{DirectionalShadowAllCascades3D,true};const Vec3 extent{std::abs(clipFromLocal[0][0])*bounds.extents.x+std::abs(clipFromLocal[0][1])*bounds.extents.y+std::abs(clipFromLocal[0][2])*bounds.extents.z,std::abs(clipFromLocal[1][0])*bounds.extents.x+std::abs(clipFromLocal[1][1])*bounds.extents.y+std::abs(clipFromLocal[1][2])*bounds.extents.z,std::abs(clipFromLocal[2][0])*bounds.extents.x+std::abs(clipFromLocal[2][1])*bounds.extents.y+std::abs(clipFromLocal[2][2])*bounds.extents.z};if(!finite(extent.x)||!finite(extent.y)||!finite(extent.z))return{DirectionalShadowAllCascades3D,true};const bool overlaps=center.x+extent.x>=-center.w-MarginXY&&center.x-extent.x<=center.w+MarginXY&&center.y+extent.y>=-center.w-MarginXY&&center.y-extent.y<=center.w+MarginXY&&center.z+extent.z>=-MarginZ&&center.z-extent.z<=center.w+MarginZ;if(overlaps)mask|=uint8_t(1u<<cascade);}
 return{mask,false};
}
struct GpuMeshInstance {Matrix4 transform;Vec4 normalRows[3];uint32_t tint,surface,textureData;};
struct GpuSkinnedInstance {GpuMeshInstance instance;uint32_t skinPaletteBase{};};
static_assert(sizeof(GpuMeshInstance)==D3D12Renderer::MeshInstanceUploadBytes);
static_assert(sizeof(GpuSkinnedInstance)==128);
static_assert(offsetof(GpuMeshInstance,textureData)==120);
constexpr size_t MaterialDescriptorCount3D=D3D12Renderer::MaxMaterialTextureSets*9;
constexpr size_t ShadowNullSrvDescriptor3D=MaterialDescriptorCount3D;
constexpr size_t ShadowLiveSrvDescriptor3D=ShadowNullSrvDescriptor3D+1;
constexpr size_t SceneCbvDescriptorBase3D=ShadowLiveSrvDescriptor3D+1;
constexpr size_t ShadowCbvDescriptorBase3D=SceneCbvDescriptorBase3D+D3D12Renderer::BufferedFrames;
constexpr size_t TextureDescriptorCount3D=ShadowCbvDescriptorBase3D+D3D12Renderer::BufferedFrames;
constexpr size_t HdrSceneSrvDescriptor3D=TextureDescriptorCount3D;
constexpr size_t DlssOutputSrvDescriptor3D=HdrSceneSrvDescriptor3D+1;
constexpr size_t DlssInputSrvDescriptor3D=DlssOutputSrvDescriptor3D+1;
constexpr size_t AmbientOcclusionNullSrvDescriptor3D=DlssInputSrvDescriptor3D+1;
constexpr size_t AmbientOcclusionDepthSrvDescriptor3D=AmbientOcclusionNullSrvDescriptor3D+1;
constexpr size_t AmbientOcclusionRawSrvDescriptor3D=AmbientOcclusionDepthSrvDescriptor3D+1;
constexpr size_t AmbientOcclusionBlurSrvDescriptor3D=AmbientOcclusionRawSrvDescriptor3D+1;
constexpr size_t WaterDepthNullSrvDescriptor3D=AmbientOcclusionBlurSrvDescriptor3D+1;
constexpr size_t TotalTextureDescriptorCount3D=WaterDepthNullSrvDescriptor3D+1;
constexpr size_t SceneConstantStride3D=512;
struct SceneConstants3D {std::array<float,84> values{};};
static_assert(sizeof(SceneConstants3D)==336);
bool InvertMatrix4x4(const Matrix4& source,Matrix4& result){
 double augmented[4][8]{};for(size_t row=0;row<4;++row){for(size_t column=0;column<4;++column)augmented[row][column]=source[row][column];augmented[row][row+4]=1.;}
 for(size_t column=0;column<4;++column){size_t pivot=column;for(size_t row=column+1;row<4;++row)if(std::abs(augmented[row][column])>std::abs(augmented[pivot][column]))pivot=row;if(std::abs(augmented[pivot][column])<1e-12)return false;if(pivot!=column)for(size_t value=0;value<8;++value)std::swap(augmented[pivot][value],augmented[column][value]);const double divisor=augmented[column][column];for(double& value:augmented[column])value/=divisor;for(size_t row=0;row<4;++row){if(row==column)continue;const double factor=augmented[row][column];for(size_t value=0;value<8;++value)augmented[row][value]-=factor*augmented[column][value];}}
 for(size_t row=0;row<4;++row)for(size_t column=0;column<4;++column){const double value=augmented[row][column+4];if(!std::isfinite(value)||std::abs(value)>(std::numeric_limits<float>::max)())return false;result[row][column]=float(value);}return true;
}
void CreateEnhancedRootSignatures3D(ID3D12Device* device,ComPtr<ID3D12RootSignature>& worldRoot,ComPtr<ID3D12RootSignature>& ambientOcclusionRoot){
 D3D12_DESCRIPTOR_RANGE sceneRange{D3D12_DESCRIPTOR_RANGE_TYPE_CBV,1,0,0,D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND},materialRanges[2]{{D3D12_DESCRIPTOR_RANGE_TYPE_SRV,3,0,0,D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND},{D3D12_DESCRIPTOR_RANGE_TYPE_SRV,6,4,0,D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND}},shadowRange{D3D12_DESCRIPTOR_RANGE_TYPE_SRV,1,3,0,D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND},shadowConstantsRange{D3D12_DESCRIPTOR_RANGE_TYPE_CBV,1,1,0,D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND},ambientWorldRange{D3D12_DESCRIPTOR_RANGE_TYPE_SRV,1,11,0,D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND},waterDepthRange{D3D12_DESCRIPTOR_RANGE_TYPE_SRV,1,12,0,D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND};
 D3D12_ROOT_PARAMETER parameters[8]{};parameters[0].ParameterType=D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;parameters[0].DescriptorTable={1,&sceneRange};parameters[0].ShaderVisibility=D3D12_SHADER_VISIBILITY_ALL;parameters[1].ParameterType=D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;parameters[1].DescriptorTable={2,materialRanges};parameters[1].ShaderVisibility=D3D12_SHADER_VISIBILITY_PIXEL;parameters[2].ParameterType=D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;parameters[2].DescriptorTable={1,&shadowRange};parameters[2].ShaderVisibility=D3D12_SHADER_VISIBILITY_PIXEL;parameters[3].ParameterType=D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;parameters[3].DescriptorTable={1,&shadowConstantsRange};parameters[3].ShaderVisibility=D3D12_SHADER_VISIBILITY_ALL;parameters[4].ParameterType=D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;parameters[4].Constants={2,0,2};parameters[4].ShaderVisibility=D3D12_SHADER_VISIBILITY_ALL;parameters[5].ParameterType=D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;parameters[5].DescriptorTable={1,&ambientWorldRange};parameters[5].ShaderVisibility=D3D12_SHADER_VISIBILITY_PIXEL;parameters[6].ParameterType=D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;parameters[6].DescriptorTable={1,&waterDepthRange};parameters[6].ShaderVisibility=D3D12_SHADER_VISIBILITY_PIXEL;parameters[7].ParameterType=D3D12_ROOT_PARAMETER_TYPE_SRV;parameters[7].Descriptor={13,0};parameters[7].ShaderVisibility=D3D12_SHADER_VISIBILITY_VERTEX;
 D3D12_STATIC_SAMPLER_DESC samplers[3]{};samplers[0].Filter=D3D12_FILTER_ANISOTROPIC;samplers[0].AddressU=samplers[0].AddressV=samplers[0].AddressW=D3D12_TEXTURE_ADDRESS_MODE_WRAP;samplers[0].MaxAnisotropy=8;samplers[0].ComparisonFunc=D3D12_COMPARISON_FUNC_ALWAYS;samplers[0].MaxLOD=D3D12_FLOAT32_MAX;samplers[0].ShaderVisibility=D3D12_SHADER_VISIBILITY_PIXEL;samplers[1].Filter=D3D12_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT;samplers[1].AddressU=samplers[1].AddressV=samplers[1].AddressW=D3D12_TEXTURE_ADDRESS_MODE_BORDER;samplers[1].BorderColor=D3D12_STATIC_BORDER_COLOR_OPAQUE_WHITE;samplers[1].ComparisonFunc=D3D12_COMPARISON_FUNC_LESS_EQUAL;samplers[1].ShaderRegister=1;samplers[1].ShaderVisibility=D3D12_SHADER_VISIBILITY_PIXEL;samplers[2].Filter=D3D12_FILTER_MIN_MAG_MIP_POINT;samplers[2].AddressU=samplers[2].AddressV=samplers[2].AddressW=D3D12_TEXTURE_ADDRESS_MODE_CLAMP;samplers[2].MaxLOD=D3D12_FLOAT32_MAX;samplers[2].ShaderRegister=2;samplers[2].ShaderVisibility=D3D12_SHADER_VISIBILITY_PIXEL;
 D3D12_ROOT_SIGNATURE_DESC signature{8,parameters,3,samplers,D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT};ComPtr<ID3DBlob> blob;Check(D3D12SerializeRootSignature(&signature,D3D_ROOT_SIGNATURE_VERSION_1,&blob,nullptr),"Enhanced world root signature serialize failed");worldRoot.Reset();Check(device->CreateRootSignature(0,blob->GetBufferPointer(),blob->GetBufferSize(),IID_PPV_ARGS(&worldRoot)),"Enhanced world root signature failed");
 D3D12_DESCRIPTOR_RANGE aoSceneRange{D3D12_DESCRIPTOR_RANGE_TYPE_CBV,1,0,0,D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND},aoTextureRange{D3D12_DESCRIPTOR_RANGE_TYPE_SRV,2,0,0,D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND};D3D12_ROOT_PARAMETER aoParameters[4]{};aoParameters[0].ParameterType=D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;aoParameters[0].DescriptorTable={1,&aoSceneRange};aoParameters[0].ShaderVisibility=D3D12_SHADER_VISIBILITY_ALL;aoParameters[1].ParameterType=D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;aoParameters[1].DescriptorTable={1,&aoTextureRange};aoParameters[1].ShaderVisibility=D3D12_SHADER_VISIBILITY_PIXEL;aoParameters[2].ParameterType=D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;aoParameters[2].Constants={1,0,32};aoParameters[2].ShaderVisibility=D3D12_SHADER_VISIBILITY_PIXEL;aoParameters[3].ParameterType=D3D12_ROOT_PARAMETER_TYPE_SRV;aoParameters[3].Descriptor={13,0};aoParameters[3].ShaderVisibility=D3D12_SHADER_VISIBILITY_VERTEX;D3D12_STATIC_SAMPLER_DESC aoSampler{};aoSampler.Filter=D3D12_FILTER_MIN_MAG_MIP_POINT;aoSampler.AddressU=aoSampler.AddressV=aoSampler.AddressW=D3D12_TEXTURE_ADDRESS_MODE_CLAMP;aoSampler.MaxLOD=D3D12_FLOAT32_MAX;aoSampler.ShaderVisibility=D3D12_SHADER_VISIBILITY_PIXEL;D3D12_ROOT_SIGNATURE_DESC aoSignature{4,aoParameters,1,&aoSampler,D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT};blob.Reset();Check(D3D12SerializeRootSignature(&aoSignature,D3D_ROOT_SIGNATURE_VERSION_1,&blob,nullptr),"Ambient occlusion root signature serialize failed");Check(device->CreateRootSignature(0,blob->GetBufferPointer(),blob->GetBufferSize(),IID_PPV_ARGS(&ambientOcclusionRoot)),"Ambient occlusion root signature failed");
}
struct InstanceGroup3D {std::vector<GpuMeshInstance> instances;uint64_t lastUsedFrame{};};
struct InstanceGroupKey3D {uint32_t mesh{},texture{};uint8_t shadowCascadeMask{},doubleSided{};bool operator==(const InstanceGroupKey3D&)const=default;};
struct InstanceGroupKeyHasher3D {size_t operator()(const InstanceGroupKey3D& key)const{uint64_t value=uint64_t(key.texture)<<32|key.mesh;value^=uint64_t(key.shadowCascadeMask)<<61;value^=uint64_t(key.doubleSided)<<60;value^=value>>33;value*=0xff51afd7ed558ccdull;value^=value>>33;return size_t(value);}};
struct MeshCacheKey3D {uint64_t primary{},secondary{};size_t vertexCount{},indexCount{};bool operator==(const MeshCacheKey3D&)const=default;};
struct MeshCacheKeyHasher3D {size_t operator()(const MeshCacheKey3D& key)const{return size_t(key.primary^(key.secondary+0x9e3779b97f4a7c15ull+(key.primary<<6)+(key.primary>>2)));}};
MeshCacheKey3D CreateMeshCacheKey(std::span<const Vertex3D> vertices,std::span<const uint32_t> indices){MeshCacheKey3D key{1469598103934665603ull,1099511628211ull,vertices.size(),indices.size()};auto mix=[&](std::span<const std::byte> bytes){for(std::byte item:bytes){const uint64_t value=std::to_integer<uint8_t>(item);key.primary=(key.primary^value)*1099511628211ull;key.secondary^=value+0x9e3779b97f4a7c15ull+(key.secondary<<6)+(key.secondary>>2);}};mix(std::as_bytes(vertices));mix(std::as_bytes(indices));return key;}
MeshCacheKey3D CreateSkinnedMeshCacheKey(std::span<const SkinnedVertex3D> vertices,std::span<const uint32_t> indices,uint32_t jointCount){MeshCacheKey3D key{1469598103934665603ull^jointCount,1099511628211ull,vertices.size(),indices.size()};auto mix=[&](std::span<const std::byte> bytes){for(std::byte item:bytes){const uint64_t value=std::to_integer<uint8_t>(item);key.primary=(key.primary^value)*1099511628211ull;key.secondary^=value+0x9e3779b97f4a7c15ull+(key.secondary<<6)+(key.secondary>>2);}};mix(std::as_bytes(vertices));mix(std::as_bytes(indices));return key;}
struct ResidentMesh3D {ComPtr<ID3D12Resource> buffer;MeshCacheKey3D cacheKey{};ResidentMeshBounds3D bounds{};D3D12_VERTEX_BUFFER_VIEW vertexView{};D3D12_INDEX_BUFFER_VIEW indexView{};UINT indexCount{},references{1};size_t bytes{};};
struct ResidentSkinnedMesh3D {ComPtr<ID3D12Resource> buffer;MeshCacheKey3D cacheKey{};D3D12_VERTEX_BUFFER_VIEW vertexView{};D3D12_INDEX_BUFFER_VIEW indexView{};UINT indexCount{},jointCount{},references{1};size_t bytes{};};
struct ResidentMaterialTextureSet3D {ComPtr<ID3D12Heap> heap;std::array<ComPtr<ID3D12Resource>,9> textures;std::wstring cacheKey;uint32_t descriptorOffset{},references{1};size_t bytes{},allocationBytes{};};
struct RetiredGpuResources3D {ComPtr<ID3D12Heap> heap;std::array<ComPtr<ID3D12Resource>,9> resources;UINT64 fenceValue{};size_t bytes{};uint32_t textureSlot{UINT_MAX};bool textures{};};
struct MeshUploadContext3D {ComPtr<ID3D12CommandAllocator> allocator;ComPtr<ID3D12GraphicsCommandList> list;};
struct MeshUploadBatch3D {MeshUploadContext3D context;std::vector<ComPtr<ID3D12Resource>> staging;size_t bytes{},meshes{};};
struct PendingMeshUpload3D {MeshUploadContext3D context;std::vector<ComPtr<ID3D12Resource>> staging;UINT64 fenceValue{};size_t bytes{},meshes{};};
struct TextureUploadBatch3D {MeshUploadContext3D context;std::vector<ComPtr<ID3D12Resource>> staging;size_t bytes{},textures{};};
struct PendingTextureUpload3D {MeshUploadContext3D context;std::vector<ComPtr<ID3D12Resource>> staging;UINT64 fenceValue{};size_t bytes{},textures{};};
struct ActiveMeshGroup3D {ResidentMesh3D* mesh{};MaterialTextureSetHandle3D textures{};size_t instanceOffset{},instanceCount{};uint8_t shadowCascadeMask{DirectionalShadowAllCascades3D};bool doubleSided{};};
struct ActiveSkinnedMesh3D {ResidentSkinnedMesh3D* mesh{};MaterialTextureSetHandle3D textures{};size_t instanceOffset{};UINT firstIndex{},indexCount{};uint8_t shadowCascadeMask{};bool doubleSided{};};
struct ImageMip3D {unsigned width{},height{};std::vector<uint8_t> pixels;};
struct CompressedImageMip3D {unsigned width{},height{};size_t rowBytes{};std::vector<uint8_t> pixels;};
struct ImageExtent3D {unsigned width{},height{};explicit operator bool()const{return width&&height;}};
struct DecodedImage3D {std::vector<ImageMip3D> mips;bool Valid()const{return!mips.empty()&&!mips.front().pixels.empty();}size_t Bytes()const{size_t bytes{};for(const ImageMip3D& mip:mips)bytes+=mip.pixels.size();return bytes;}};
enum class ImageMipMode3D:uint8_t {Srgb,Linear,Normal};
struct CompressedImage3D {std::vector<CompressedImageMip3D> mips;bool Valid()const{return!mips.empty()&&!mips.front().pixels.empty();}size_t Bytes()const{size_t bytes{};for(const CompressedImageMip3D& mip:mips)bytes+=mip.pixels.size();return bytes;}};
enum class MaterialCacheLoadResult3D:uint8_t {Miss,Hit,Invalid};
struct MaterialCacheHeader3D {uint32_t magic{},version{},mode{},mipCount{};uint64_t key{};};
struct MaterialCacheMipHeader3D {uint32_t width{},height{};uint64_t rowBytes{},dataBytes{};};
struct MaterialCacheTrimResult3D {size_t residentBytes{};uint64_t evictions{};uint64_t releasedBytes{};};
struct ScopedComApartment3D {bool active{};ScopedComApartment3D():active(SUCCEEDED(CoInitializeEx(nullptr,COINIT_MULTITHREADED))){}~ScopedComApartment3D(){if(active)CoUninitialize();}};

constexpr uint32_t MaterialCacheMagic3D=0x43425641u,MaterialCacheVersion3D=1;
std::wstring NormalizeMaterialPath(const std::filesystem::path& path);
std::filesystem::path DefaultMaterialCacheDirectory(){std::array<wchar_t,32768> value{};const DWORD length=GetEnvironmentVariableW(L"LOCALAPPDATA",value.data(),DWORD(value.size()));return length&&length<value.size()?std::filesystem::path(value.data())/L"Aevrynd"/L"DerivedDataCache"/L"Materials":std::filesystem::path{};}
uint64_t MaterialCacheKey3D(const std::filesystem::path& path,unsigned maximumResolution,ImageMipMode3D mode){uint64_t hash=1469598103934665603ull;auto mix=[&](auto value){const uint8_t* bytes=reinterpret_cast<const uint8_t*>(&value);for(size_t index=0;index<sizeof(value);++index)hash=(hash^bytes[index])*1099511628211ull;};const std::wstring normalized=NormalizeMaterialPath(path);for(wchar_t character:normalized)mix(character);std::error_code error;const uint64_t fileBytes=std::filesystem::file_size(path,error);if(error)return 0;const auto writeTime=std::filesystem::last_write_time(path,error);if(error)return 0;const auto writeTicks=writeTime.time_since_epoch().count();mix(fileBytes);mix(writeTicks);mix(maximumResolution);mix(mode);mix(MaterialCacheVersion3D);return hash;}
std::filesystem::path MaterialCacheFile3D(const std::filesystem::path& directory,uint64_t key){return directory/(std::to_wstring(key)+L".avbc");}
MaterialCacheTrimResult3D TrimMaterialCache3D(const std::filesystem::path& directory,size_t budgetBytes){MaterialCacheTrimResult3D result{};if(directory.empty())return result;struct Entry{std::filesystem::path path;std::filesystem::file_time_type writeTime;size_t bytes{};};std::vector<Entry> entries;std::error_code error;for(std::filesystem::directory_iterator iterator(directory,error),end;!error&&iterator!=end;iterator.increment(error)){const auto& entry=*iterator;if(!entry.is_regular_file(error)||error){error.clear();continue;}if(entry.path().extension()!=L".avbc")continue;const uintmax_t fileBytes=entry.file_size(error);if(error||fileBytes>(std::numeric_limits<size_t>::max)()){error.clear();continue;}const auto writeTime=entry.last_write_time(error);if(error){error.clear();continue;}entries.push_back({entry.path(),writeTime,size_t(fileBytes)});result.residentBytes+=size_t(fileBytes);}if(result.residentBytes<=budgetBytes)return result;std::ranges::sort(entries,{},&Entry::writeTime);for(const Entry& entry:entries){if(result.residentBytes<=budgetBytes)break;if(std::filesystem::remove(entry.path,error)){result.residentBytes-=entry.bytes;result.releasedBytes+=entry.bytes;++result.evictions;}error.clear();}return result;}
void TouchMaterialCache3D(const std::filesystem::path& directory,uint64_t key){if(directory.empty()||!key)return;std::error_code error;std::filesystem::last_write_time(MaterialCacheFile3D(directory,key),std::filesystem::file_time_type::clock::now(),error);}
size_t MaterialCacheFileBytes3D(const std::filesystem::path& directory,uint64_t key){std::error_code error;const uintmax_t bytes=std::filesystem::file_size(MaterialCacheFile3D(directory,key),error);return error||bytes>(std::numeric_limits<size_t>::max)()?0:size_t(bytes);}
MaterialCacheLoadResult3D LoadMaterialCache3D(const std::filesystem::path& directory,uint64_t key,ImageExtent3D extent,ImageMipMode3D mode,CompressedImage3D& image,size_t& loadedBytes){if(directory.empty()||!key)return MaterialCacheLoadResult3D::Miss;const std::filesystem::path file=MaterialCacheFile3D(directory,key);std::ifstream input(file,std::ios::binary);if(!input)return MaterialCacheLoadResult3D::Miss;MaterialCacheHeader3D header{};input.read(reinterpret_cast<char*>(&header),sizeof(header));unsigned expectedMips=1;for(ImageExtent3D mip=extent;mip.width>1||mip.height>1;mip.width=(std::max)(1u,mip.width/2),mip.height=(std::max)(1u,mip.height/2))++expectedMips;if(!input||header.magic!=MaterialCacheMagic3D||header.version!=MaterialCacheVersion3D||header.mode!=uint32_t(mode)||header.mipCount!=expectedMips||header.key!=key)return MaterialCacheLoadResult3D::Invalid;CompressedImage3D loaded{};loaded.mips.reserve(header.mipCount);const size_t blockBytes=mode==ImageMipMode3D::Normal?16:8;for(uint32_t index=0;index<header.mipCount;++index){MaterialCacheMipHeader3D mipHeader{};input.read(reinterpret_cast<char*>(&mipHeader),sizeof(mipHeader));const size_t expectedRowBytes=std::size_t((extent.width+3)/4)*blockBytes,expectedDataBytes=expectedRowBytes*((extent.height+3)/4);if(!input||mipHeader.width!=extent.width||mipHeader.height!=extent.height||mipHeader.rowBytes!=expectedRowBytes||mipHeader.dataBytes!=expectedDataBytes||expectedDataBytes>D3D12Renderer::MaxMaterialTextureUploadStagingBytes)return MaterialCacheLoadResult3D::Invalid;CompressedImageMip3D mip{extent.width,extent.height,expectedRowBytes};mip.pixels.resize(expectedDataBytes);input.read(reinterpret_cast<char*>(mip.pixels.data()),std::streamsize(expectedDataBytes));if(!input)return MaterialCacheLoadResult3D::Invalid;loadedBytes+=expectedDataBytes;loaded.mips.push_back(std::move(mip));extent.width=(std::max)(1u,extent.width/2);extent.height=(std::max)(1u,extent.height/2);}if(input.peek()!=std::char_traits<char>::eof())return MaterialCacheLoadResult3D::Invalid;image=std::move(loaded);return MaterialCacheLoadResult3D::Hit;}
bool StoreMaterialCache3D(const std::filesystem::path& directory,uint64_t key,ImageMipMode3D mode,const CompressedImage3D& image,size_t& storedBytes){if(directory.empty()||!key||!image.Valid())return false;std::error_code error;std::filesystem::create_directories(directory,error);if(error)return false;const std::filesystem::path target=MaterialCacheFile3D(directory,key);std::filesystem::path temporary=target;temporary+=L"."+std::to_wstring(GetCurrentProcessId())+L".tmp";std::ofstream output(temporary,std::ios::binary|std::ios::trunc);const MaterialCacheHeader3D header{MaterialCacheMagic3D,MaterialCacheVersion3D,uint32_t(mode),uint32_t(image.mips.size()),key};output.write(reinterpret_cast<const char*>(&header),sizeof(header));for(const CompressedImageMip3D& mip:image.mips){const MaterialCacheMipHeader3D mipHeader{mip.width,mip.height,mip.rowBytes,mip.pixels.size()};output.write(reinterpret_cast<const char*>(&mipHeader),sizeof(mipHeader));output.write(reinterpret_cast<const char*>(mip.pixels.data()),std::streamsize(mip.pixels.size()));storedBytes+=mip.pixels.size();}output.close();if(!output){std::filesystem::remove(temporary,error);return false;}if(!MoveFileExW(temporary.c_str(),target.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)){std::filesystem::remove(temporary,error);return false;}return true;}

std::wstring NormalizeMaterialPath(const std::filesystem::path& path){std::error_code error;std::filesystem::path normalized=std::filesystem::weakly_canonical(path,error);if(error){error.clear();normalized=std::filesystem::absolute(path,error);if(error)normalized=path;}std::wstring value=normalized.lexically_normal().native();std::transform(value.begin(),value.end(),value.begin(),[](wchar_t character){return wchar_t(std::towlower(character));});return value;}
std::wstring MaterialTextureCacheKey(const MaterialTextureSetDefinition3D& definition,unsigned maximumResolution){std::wstring key;auto append=[&](const std::filesystem::path& path){key+=NormalizeMaterialPath(path);key.push_back(L'\0');};append(definition.baseColorPath);append(definition.roughnessPath);append(definition.normalPath);for(const auto& layer:definition.additionalLayers){append(layer.baseColorPath);append(layer.roughnessPath);append(layer.normalPath);}key+=std::to_wstring(maximumResolution);return key;}
ImageExtent3D ScaledImageExtent(unsigned sourceWidth,unsigned sourceHeight,unsigned maximumResolution){if(!sourceWidth||!sourceHeight)return{};maximumResolution=(std::max)(1u,maximumResolution);const float scale=(std::min)(1.f,float(maximumResolution)/float((std::max)(sourceWidth,sourceHeight)));return{(std::max)(1u,unsigned(std::lround(float(sourceWidth)*scale))),(std::max)(1u,unsigned(std::lround(float(sourceHeight)*scale)))};}
size_t CompressedImageMipChainBytes(ImageExtent3D extent,size_t blockBytes){size_t bytes{};while(extent){bytes+=std::size_t((extent.width+3)/4)*((extent.height+3)/4)*blockBytes;if(extent.width==1&&extent.height==1)break;extent.width=(std::max)(1u,extent.width/2);extent.height=(std::max)(1u,extent.height/2);}return bytes;}
unsigned ImageMipCount3D(ImageExtent3D extent){unsigned count=1;while(extent.width>1||extent.height>1){extent.width=(std::max)(1u,extent.width/2);extent.height=(std::max)(1u,extent.height/2);++count;}return count;}
ImageExtent3D ProbeImageRgba8(const std::filesystem::path& path,unsigned maximumResolution){if(path.empty()||!std::filesystem::is_regular_file(path))return{};ScopedComApartment3D apartment;ComPtr<IWICImagingFactory> factory;ComPtr<IWICBitmapDecoder> decoder;ComPtr<IWICBitmapFrameDecode> frame;if(FAILED(CoCreateInstance(CLSID_WICImagingFactory,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&factory)))||FAILED(factory->CreateDecoderFromFilename(path.c_str(),nullptr,GENERIC_READ,WICDecodeMetadataCacheOnLoad,&decoder))||FAILED(decoder->GetFrame(0,&frame)))return{};UINT width{},height{};if(FAILED(frame->GetSize(&width,&height)))return{};return ScaledImageExtent(width,height,maximumResolution);}

uint16_t PackRgb565(uint8_t red,uint8_t green,uint8_t blue){return uint16_t(((uint32_t(red)*31+127)/255)<<11|((uint32_t(green)*63+127)/255)<<5|((uint32_t(blue)*31+127)/255));}
std::array<uint8_t,3> UnpackRgb565(uint16_t packed){return{uint8_t(((packed>>11)&31)*255/31),uint8_t(((packed>>5)&63)*255/63),uint8_t((packed&31)*255/31)};}
void CompressBc1Block(const std::array<uint8_t,64>& block,uint8_t* output){std::array<uint8_t,3> minimum={255,255,255},maximum={};for(size_t pixel=0;pixel<16;++pixel)for(size_t channel=0;channel<3;++channel){minimum[channel]=(std::min)(minimum[channel],block[pixel*4+channel]);maximum[channel]=(std::max)(maximum[channel],block[pixel*4+channel]);}uint16_t endpoint0=PackRgb565(maximum[0],maximum[1],maximum[2]),endpoint1=PackRgb565(minimum[0],minimum[1],minimum[2]);if(endpoint0<endpoint1)std::swap(endpoint0,endpoint1);if(endpoint0==endpoint1){if(endpoint0<0xffff)++endpoint0;else --endpoint1;}const auto color0=UnpackRgb565(endpoint0),color1=UnpackRgb565(endpoint1);std::array<std::array<uint8_t,3>,4> palette={color0,color1,std::array<uint8_t,3>{uint8_t((2*uint16_t(color0[0])+color1[0])/3),uint8_t((2*uint16_t(color0[1])+color1[1])/3),uint8_t((2*uint16_t(color0[2])+color1[2])/3)},std::array<uint8_t,3>{uint8_t((color0[0]+2*uint16_t(color1[0]))/3),uint8_t((color0[1]+2*uint16_t(color1[1]))/3),uint8_t((color0[2]+2*uint16_t(color1[2]))/3)}};uint32_t indices{};for(size_t pixel=0;pixel<16;++pixel){uint32_t bestError=UINT_MAX,best{};for(uint32_t candidate=0;candidate<4;++candidate){const int red=int(block[pixel*4])-palette[candidate][0],green=int(block[pixel*4+1])-palette[candidate][1],blue=int(block[pixel*4+2])-palette[candidate][2];const uint32_t error=uint32_t(red*red+green*green+blue*blue);if(error<bestError){bestError=error;best=candidate;}}indices|=best<<uint32_t(pixel*2);}output[0]=uint8_t(endpoint0);output[1]=uint8_t(endpoint0>>8);output[2]=uint8_t(endpoint1);output[3]=uint8_t(endpoint1>>8);for(size_t byte=0;byte<4;++byte)output[4+byte]=uint8_t(indices>>uint32_t(byte*8));}
void CompressBc4Block(const std::array<uint8_t,64>& block,size_t channel,uint8_t* output){uint8_t minimum=255,maximum=0;for(size_t pixel=0;pixel<16;++pixel){minimum=(std::min)(minimum,block[pixel*4+channel]);maximum=(std::max)(maximum,block[pixel*4+channel]);}output[0]=maximum;output[1]=minimum;std::array<uint8_t,8> palette={maximum,minimum};if(maximum>minimum)for(size_t index=2;index<8;++index)palette[index]=uint8_t(((8-index)*uint16_t(maximum)+(index-1)*uint16_t(minimum))/7);else{for(size_t index=2;index<6;++index)palette[index]=uint8_t(((6-index)*uint16_t(maximum)+(index-1)*uint16_t(minimum))/5);palette[6]=0;palette[7]=255;}uint64_t indices{};for(size_t pixel=0;pixel<16;++pixel){uint32_t bestError=UINT_MAX,best{};for(uint32_t candidate=0;candidate<8;++candidate){const int difference=int(block[pixel*4+channel])-palette[candidate];const uint32_t error=uint32_t(difference*difference);if(error<bestError){bestError=error;best=candidate;}}indices|=uint64_t(best)<<uint32_t(pixel*3);}for(size_t byte=0;byte<6;++byte)output[2+byte]=uint8_t(indices>>uint32_t(byte*8));}
// 材質在匯入階段轉成 GPU 原生區塊壓縮格式，避免未壓縮 RGBA mip 長期占用顯示記憶體。
CompressedImage3D CompressImage(const DecodedImage3D& image,ImageMipMode3D mode){CompressedImage3D result;if(!image.Valid())return result;result.mips.reserve(image.mips.size());const size_t blockBytes=mode==ImageMipMode3D::Normal?16:8;for(const ImageMip3D& source:image.mips){CompressedImageMip3D destination{};destination.width=source.width;destination.height=source.height;const unsigned blockWidth=(source.width+3)/4,blockHeight=(source.height+3)/4;destination.rowBytes=std::size_t(blockWidth)*blockBytes;destination.pixels.resize(destination.rowBytes*blockHeight);for(unsigned blockY=0;blockY<blockHeight;++blockY)for(unsigned blockX=0;blockX<blockWidth;++blockX){std::array<uint8_t,64> block{};for(unsigned y=0;y<4;++y)for(unsigned x=0;x<4;++x){const unsigned sourceX=(std::min)(source.width-1,blockX*4+x),sourceY=(std::min)(source.height-1,blockY*4+y);memcpy(block.data()+(y*4+x)*4,source.pixels.data()+(std::size_t(sourceY)*source.width+sourceX)*4,4);}uint8_t* output=destination.pixels.data()+std::size_t(blockY)*destination.rowBytes+std::size_t(blockX)*blockBytes;if(mode==ImageMipMode3D::Srgb)CompressBc1Block(block,output);else if(mode==ImageMipMode3D::Linear)CompressBc4Block(block,0,output);else{CompressBc4Block(block,0,output);CompressBc4Block(block,1,output+8);}}result.mips.push_back(std::move(destination));}return result;}

void GenerateImageMips(DecodedImage3D& image,ImageMipMode3D mode){
    while(image.Valid()&&(image.mips.back().width>1||image.mips.back().height>1)){const ImageMip3D& source=image.mips.back();ImageMip3D destination{(std::max)(1u,source.width/2),(std::max)(1u,source.height/2)};destination.pixels.resize(std::size_t(destination.width)*destination.height*4);for(unsigned y=0;y<destination.height;++y)for(unsigned x=0;x<destination.width;++x){float channels[4]{};for(unsigned sampleY=0;sampleY<2;++sampleY)for(unsigned sampleX=0;sampleX<2;++sampleX){const unsigned sourceX=(std::min)(source.width-1,x*2+sampleX),sourceY=(std::min)(source.height-1,y*2+sampleY);const uint8_t* pixel=source.pixels.data()+(std::size_t(sourceY)*source.width+sourceX)*4;for(unsigned channel=0;channel<4;++channel){float value=float(pixel[channel])/255.f;if(mode==ImageMipMode3D::Srgb&&channel<3)value=std::pow(value,2.2f);else if(mode==ImageMipMode3D::Normal&&channel<3)value=value*2.f-1.f;channels[channel]+=value;}}uint8_t* output=destination.pixels.data()+(std::size_t(y)*destination.width+x)*4;if(mode==ImageMipMode3D::Normal){Vec3 normal=Normalize({channels[0],channels[1],channels[2]});if(Length(normal)<.0001f)normal={0,0,1};output[0]=uint8_t(std::lround(Clamp(normal.x*.5f+.5f,0.f,1.f)*255.f));output[1]=uint8_t(std::lround(Clamp(normal.y*.5f+.5f,0.f,1.f)*255.f));output[2]=uint8_t(std::lround(Clamp(normal.z*.5f+.5f,0.f,1.f)*255.f));}else for(unsigned channel=0;channel<3;++channel){float value=channels[channel]*.25f;if(mode==ImageMipMode3D::Srgb)value=std::pow((std::max)(0.f,value),1.f/2.2f);output[channel]=uint8_t(std::lround(Clamp(value,0.f,1.f)*255.f));}output[3]=uint8_t(std::lround(Clamp(channels[3]*.25f,0.f,1.f)*255.f));}image.mips.push_back(std::move(destination));}
}

DecodedImage3D DecodeImageRgba8(const std::filesystem::path& path,unsigned maximumResolution){
    if(path.empty()||!std::filesystem::is_regular_file(path))return{};
    ScopedComApartment3D apartment;
    ComPtr<IWICImagingFactory> factory;ComPtr<IWICBitmapDecoder> decoder;ComPtr<IWICBitmapFrameDecode> frame;ComPtr<IWICBitmapScaler> scaler;ComPtr<IWICFormatConverter> converter;DecodedImage3D result{};
    if(FAILED(CoCreateInstance(CLSID_WICImagingFactory,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&factory)))||FAILED(factory->CreateDecoderFromFilename(path.c_str(),nullptr,GENERIC_READ,WICDecodeMetadataCacheOnLoad,&decoder))||FAILED(decoder->GetFrame(0,&frame)))return{};
    UINT sourceWidth{},sourceHeight{};frame->GetSize(&sourceWidth,&sourceHeight);const ImageExtent3D extent=ScaledImageExtent(sourceWidth,sourceHeight,maximumResolution);ImageMip3D base{extent.width,extent.height};IWICBitmapSource* source=frame.Get();if(base.width!=sourceWidth||base.height!=sourceHeight){if(FAILED(factory->CreateBitmapScaler(&scaler))||FAILED(scaler->Initialize(frame.Get(),base.width,base.height,WICBitmapInterpolationModeFant)))return{};source=scaler.Get();}
    if(FAILED(factory->CreateFormatConverter(&converter))||FAILED(converter->Initialize(source,GUID_WICPixelFormat32bppRGBA,WICBitmapDitherTypeNone,nullptr,0,WICBitmapPaletteTypeCustom)))return{};base.pixels.resize(std::size_t(base.width)*base.height*4);if(FAILED(converter->CopyPixels(nullptr,base.width*4,UINT(base.pixels.size()),base.pixels.data())))return{};result.mips.push_back(std::move(base));return result;
}

static size_t SystemGlyphCacheTrackedBytes();
static size_t SystemGlyphCacheReclaimableBytes(size_t retainedGlyphs);
TransientMemoryStats DrawList::MemoryStats()const{const bool frameProfile=capacityProfile_==DrawListCapacityProfile::Frame;const size_t vertexBaseline=frameProfile?InitialVertexCapacity:Minimal2DVertexCapacity,worldVertexBaseline=frameProfile?InitialVertexCapacity:0,worldIndexBaseline=frameProfile?InitialWorldIndexCapacity:0,instanceBaseline=frameProfile?InitialMeshInstanceCapacity:0,lightBaseline=frameProfile?InitialPointLightCapacity:0,spanBaseline=frameProfile?InitialAdditiveSpanCapacity:0;const size_t tracked=vertices.capacity()*sizeof(Vertex)+worldVertices.capacity()*sizeof(Vertex3D)+worldIndices.capacity()*sizeof(uint32_t)+meshInstances.capacity()*sizeof(MeshInstance3D)+skinnedMeshInstances.capacity()*sizeof(SkinnedMeshInstance3D)+skinPaletteJoints.capacity()*sizeof(SkinPaletteJoint3D)+pointLights_.capacity()*sizeof(PointLight3D)+additiveSpans_.capacity()*sizeof(DrawSpan2D)+SystemGlyphCacheTrackedBytes();auto reclaimable=[]<class T>(const std::vector<T>& values,size_t baseline){return values.size()<=baseline&&values.capacity()>baseline*2?(values.capacity()-baseline)*sizeof(T):size_t{};};const size_t available=reclaimable(vertices,vertexBaseline)+reclaimable(worldVertices,worldVertexBaseline)+reclaimable(worldIndices,worldIndexBaseline)+reclaimable(meshInstances,instanceBaseline)+reclaimable(skinnedMeshInstances,0)+reclaimable(skinPaletteJoints,0)+reclaimable(pointLights_,lightBaseline)+reclaimable(additiveSpans_,spanBaseline)+SystemGlyphCacheReclaimableBytes(RetainedSystemGlyphsAfterTrim);return{tracked,available,lastReleasedBytes_,trimCount_};}
void DrawList::TrimTransientMemory(){Clear();const size_t before=MemoryStats().trackedCapacityBytes;const bool frameProfile=capacityProfile_==DrawListCapacityProfile::Frame;auto reset=[]<class T>(std::vector<T>& values,size_t baseline){if(values.capacity()<=baseline*2)return;std::vector<T> replacement;replacement.reserve(baseline);values.swap(replacement);};reset(vertices,frameProfile?InitialVertexCapacity:Minimal2DVertexCapacity);reset(worldVertices,frameProfile?InitialVertexCapacity:0);reset(worldIndices,frameProfile?InitialWorldIndexCapacity:0);reset(meshInstances,frameProfile?InitialMeshInstanceCapacity:0);reset(skinnedMeshInstances,0);reset(skinPaletteJoints,0);reset(pointLights_,frameProfile?InitialPointLightCapacity:0);reset(additiveSpans_,frameProfile?InitialAdditiveSpanCapacity:0);TrimSystemGlyphCache();const size_t after=MemoryStats().trackedCapacityBytes;lastReleasedBytes_=before>after?before-after:0;++trimCount_;}
DrawList::PrimitiveClipResult DrawList::ClassifyTransformedBounds(Vec2 minimum,Vec2 maximum)const{if(!clipEnabled_)return PrimitiveClipResult::Contained;const std::array<Vec2,4> corners={ApplyTransform2D(minimum),ApplyTransform2D({maximum.x,minimum.y}),ApplyTransform2D(maximum),ApplyTransform2D({minimum.x,maximum.y})};if(std::ranges::any_of(corners,[](Vec2 point){return!std::isfinite(point.x)||!std::isfinite(point.y);}))return PrimitiveClipResult::Partial;const auto [minimumX,maximumX]=std::ranges::minmax(corners,{},&Vec2::x);const auto [minimumY,maximumY]=std::ranges::minmax(corners,{},&Vec2::y);if(maximumX.x<clipMinimum_.x||minimumX.x>clipMaximum_.x||maximumY.y<clipMinimum_.y||minimumY.y>clipMaximum_.y)return PrimitiveClipResult::Culled;if(minimumX.x>=clipMinimum_.x&&maximumX.x<=clipMaximum_.x&&minimumY.y>=clipMinimum_.y&&maximumY.y<=clipMaximum_.y)return PrimitiveClipResult::Contained;return PrimitiveClipResult::Partial;}
void DrawList::RecordSubmittedVertices(size_t firstVertex,size_t vertexCount){if(blendMode_!=BlendMode2D::Additive||!vertexCount)return;stats_.additiveTriangles+=vertexCount/3;stats_.totalAdditiveTriangles+=vertexCount/3;if(!additiveSpans_.empty()&&additiveSpans_.back().firstVertex+additiveSpans_.back().vertexCount==firstVertex)additiveSpans_.back().vertexCount+=vertexCount;else{additiveSpans_.push_back({firstVertex,vertexCount});++stats_.additiveSpans;++stats_.totalAdditiveSpans;}}
void DrawList::SubmitContainedTriangle(Vec2 a,Vec2 b,Vec2 c,uint32_t packedColor){++stats_.inputTriangles;++stats_.totalInputTriangles;const size_t firstVertex=vertices.size();vertices.push_back({a.x,a.y,packedColor});vertices.push_back({b.x,b.y,packedColor});vertices.push_back({c.x,c.y,packedColor});RecordSubmittedVertices(firstVertex,3);++stats_.submittedTriangles;stats_.peakSubmittedTriangles=(std::max)(stats_.peakSubmittedTriangles,stats_.submittedTriangles);}
void DrawList::SubmitGradientTriangle(Vec2 a,uint32_t colorA,Vec2 b,uint32_t colorB,Vec2 c,uint32_t colorC,bool contained){++stats_.inputTriangles;++stats_.totalInputTriangles;if(!contained&&clipEnabled_){const float minimumX=(std::min)({a.x,b.x,c.x}),maximumX=(std::max)({a.x,b.x,c.x}),minimumY=(std::min)({a.y,b.y,c.y}),maximumY=(std::max)({a.y,b.y,c.y});if(maximumX<clipMinimum_.x||minimumX>clipMaximum_.x||maximumY<clipMinimum_.y||minimumY>clipMaximum_.y){++stats_.culledTriangles;++stats_.totalCulledTriangles;return;}}const size_t firstVertex=vertices.size();vertices.push_back({a.x,a.y,colorA});vertices.push_back({b.x,b.y,colorB});vertices.push_back({c.x,c.y,colorC});RecordSubmittedVertices(firstVertex,3);++stats_.submittedTriangles;stats_.peakSubmittedTriangles=(std::max)(stats_.peakSubmittedTriangles,stats_.submittedTriangles);}
void DrawList::SubmitContainedTextQuad(Vec2 center,Vec2 size,Color color){const std::array<Vec2,4> points={ApplyTransform2D({center.x-size.x*.5f,center.y-size.y*.5f}),ApplyTransform2D({center.x+size.x*.5f,center.y-size.y*.5f}),ApplyTransform2D({center.x+size.x*.5f,center.y+size.y*.5f}),ApplyTransform2D({center.x-size.x*.5f,center.y+size.y*.5f})};const uint32_t packed=PackColorRgba8(color);RecordSharedVertexTransforms(4,2);RecordPrimitiveColorPacks(1,1);++stats_.primitiveContained;stats_.primitiveContainedTriangles+=2;stats_.totalPrimitiveContainedTriangles+=2;++stats_.textContainedSpans;stats_.textContainmentBypassedTriangles+=2;++stats_.totalTextContainedSpans;stats_.totalTextContainmentBypassedTriangles+=2;SubmitContainedTriangle(points[0],points[1],points[2],packed);SubmitContainedTriangle(points[0],points[2],points[3],packed);}
void DrawList::CullTextQuad(){stats_.inputTriangles+=2;stats_.totalInputTriangles+=2;stats_.culledTriangles+=2;stats_.totalCulledTriangles+=2;++stats_.primitiveCulled;stats_.primitiveCulledTriangles+=2;stats_.totalPrimitiveCulledTriangles+=2;++stats_.textCulledSpans;stats_.textCullingBypassedTriangles+=2;++stats_.totalTextCulledSpans;stats_.totalTextCullingBypassedTriangles+=2;}
void DrawList::SubmitTransformedTriangle(Vec2 a,Vec2 b,Vec2 c,float alpha,uint32_t packedColor){if(alpha<=0.f){++stats_.inputTriangles;++stats_.totalInputTriangles;++stats_.culledTriangles;++stats_.totalCulledTriangles;return;}if(clipEnabled_){const float minimumX=(std::min)({a.x,b.x,c.x}),maximumX=(std::max)({a.x,b.x,c.x}),minimumY=(std::min)({a.y,b.y,c.y}),maximumY=(std::max)({a.y,b.y,c.y});if(maximumX<clipMinimum_.x||minimumX>clipMaximum_.x||maximumY<clipMinimum_.y||minimumY>clipMaximum_.y){++stats_.inputTriangles;++stats_.totalInputTriangles;++stats_.culledTriangles;++stats_.totalCulledTriangles;return;}}SubmitContainedTriangle(a,b,c,packedColor);}
void DrawList::RecordSharedVertexTransforms(size_t transforms,size_t saved){stats_.sharedVertexTransforms+=transforms;stats_.sharedVertexTransformsSaved+=saved;stats_.totalSharedVertexTransforms+=transforms;stats_.totalSharedVertexTransformsSaved+=saved;}
void DrawList::RecordPrimitiveColorPacks(size_t packs,size_t saved){stats_.primitiveColorPacks+=packs;stats_.primitiveColorPacksSaved+=saved;stats_.totalPrimitiveColorPacks+=packs;stats_.totalPrimitiveColorPacksSaved+=saved;}
DrawList::EllipseStepResult DrawList::EllipseStep(int segments){++ellipseStepCacheClock_;EllipseStepCacheEntry* replacement=&ellipseStepCache_.front();for(EllipseStepCacheEntry& entry:ellipseStepCache_){if(entry.segments==segments){entry.lastUsed=ellipseStepCacheClock_;++stats_.ellipseStepCacheHits;++stats_.totalEllipseStepCacheHits;return{entry.cosine,entry.sine,true};}if(!entry.segments){replacement=&entry;break;}if(entry.lastUsed<replacement->lastUsed)replacement=&entry;}const float step=6.2831853f/float(segments);*replacement={segments,std::cos(step),std::sin(step),ellipseStepCacheClock_};++stats_.ellipseStepCacheMisses;++stats_.totalEllipseStepCacheMisses;return{replacement->cosine,replacement->sine,false};}
void DrawList::Triangle(Vec2 a,Vec2 b,Vec2 c,Color color){SubmitTransformedTriangle(ApplyTransform2D(a),ApplyTransform2D(b),ApplyTransform2D(c),color.a,PackColorRgba8(color));}
DrawList::PrimitiveClipResult DrawList::ClassifyTransformedPrimitive(std::span<const Vec2> points,float alpha,size_t triangles){++stats_.primitiveCullTests;PrimitiveClipResult result=PrimitiveClipResult::Partial;if(alpha<=0.f)result=PrimitiveClipResult::Culled;else if(!clipEnabled_)result=PrimitiveClipResult::Contained;else if(!points.empty()&&!std::ranges::any_of(points,[](Vec2 point){return!std::isfinite(point.x)||!std::isfinite(point.y);})){const auto [minimumX,maximumX]=std::ranges::minmax(points,{},&Vec2::x);const auto [minimumY,maximumY]=std::ranges::minmax(points,{},&Vec2::y);if(maximumX.x<clipMinimum_.x||minimumX.x>clipMaximum_.x||maximumY.y<clipMinimum_.y||minimumY.y>clipMaximum_.y)result=PrimitiveClipResult::Culled;else if(minimumX.x>=clipMinimum_.x&&maximumX.x<=clipMaximum_.x&&minimumY.y>=clipMinimum_.y&&maximumY.y<=clipMaximum_.y)result=PrimitiveClipResult::Contained;}if(result==PrimitiveClipResult::Culled){stats_.inputTriangles+=triangles;stats_.totalInputTriangles+=triangles;stats_.culledTriangles+=triangles;stats_.totalCulledTriangles+=triangles;++stats_.primitiveCulled;stats_.primitiveCulledTriangles+=triangles;stats_.totalPrimitiveCulledTriangles+=triangles;}else if(result==PrimitiveClipResult::Contained){++stats_.primitiveContained;stats_.primitiveContainedTriangles+=triangles;stats_.totalPrimitiveContainedTriangles+=triangles;}return result;}
DrawList::PrimitiveClipResult DrawList::ClassifyTessellatedPrimitive(Vec2 minimum,Vec2 maximum,Color color,size_t triangles){if(color.a<=0.f||!clipEnabled_)return ClassifyTransformedPrimitive({},color.a,triangles);const std::array<Vec2,4> corners={ApplyTransform2D(minimum),ApplyTransform2D({maximum.x,minimum.y}),ApplyTransform2D(maximum),ApplyTransform2D({minimum.x,maximum.y})};return ClassifyTransformedPrimitive(corners,color.a,triangles);}
void DrawList::WorldTriangle(Vec3 a,Vec3 b,Vec3 c,Color color){const uint32_t base=uint32_t(worldVertices.size()),packed=PackColorRgba8(color),normal=PackNormalSnorm8(Normalize(Cross(b-a,c-a)));worldVertices.push_back({a.x,a.y,a.z,normal,packed});worldVertices.push_back({b.x,b.y,b.z,normal,packed});worldVertices.push_back({c.x,c.y,c.z,normal,packed});worldIndices.insert(worldIndices.end(),{base,base+1,base+2});}
void DrawList::Quad(Vec2 c,Vec2 s,Color col){const std::array<Vec2,4> points={ApplyTransform2D({c.x-s.x*.5f,c.y-s.y*.5f}),ApplyTransform2D({c.x+s.x*.5f,c.y-s.y*.5f}),ApplyTransform2D({c.x+s.x*.5f,c.y+s.y*.5f}),ApplyTransform2D({c.x-s.x*.5f,c.y+s.y*.5f})};RecordSharedVertexTransforms(4,2);const PrimitiveClipResult visibility=ClassifyTransformedPrimitive(points,col.a,2);if(visibility==PrimitiveClipResult::Culled)return;const uint32_t packed=PackColorRgba8(col);RecordPrimitiveColorPacks(1,1);if(visibility==PrimitiveClipResult::Contained){SubmitContainedTriangle(points[0],points[1],points[2],packed);SubmitContainedTriangle(points[0],points[2],points[3],packed);}else{SubmitTransformedTriangle(points[0],points[1],points[2],col.a,packed);SubmitTransformedTriangle(points[0],points[2],points[3],col.a,packed);}}
void DrawList::GradientQuad(Vec2 c,Vec2 s,Color top,Color bottom){const std::array<Vec2,4> points={ApplyTransform2D({c.x-s.x*.5f,c.y-s.y*.5f}),ApplyTransform2D({c.x+s.x*.5f,c.y-s.y*.5f}),ApplyTransform2D({c.x+s.x*.5f,c.y+s.y*.5f}),ApplyTransform2D({c.x-s.x*.5f,c.y+s.y*.5f})};RecordSharedVertexTransforms(4,2);const PrimitiveClipResult visibility=ClassifyTransformedPrimitive(points,(std::max)(top.a,bottom.a),2);if(visibility==PrimitiveClipResult::Culled)return;const uint32_t packedTop=PackColorRgba8(top),packedBottom=PackColorRgba8(bottom);RecordPrimitiveColorPacks(2,2);const bool contained=visibility==PrimitiveClipResult::Contained;SubmitGradientTriangle(points[0],packedTop,points[1],packedTop,points[2],packedBottom,contained);SubmitGradientTriangle(points[0],packedTop,points[2],packedBottom,points[3],packedBottom,contained);}
void DrawList::Diamond(Vec2 c,float rx,float ry,Color col){const std::array<Vec2,4> points={ApplyTransform2D({c.x,c.y-ry}),ApplyTransform2D({c.x+rx,c.y}),ApplyTransform2D({c.x,c.y+ry}),ApplyTransform2D({c.x-rx,c.y})};RecordSharedVertexTransforms(4,2);const PrimitiveClipResult visibility=ClassifyTransformedPrimitive(points,col.a,2);if(visibility==PrimitiveClipResult::Culled)return;const uint32_t packed=PackColorRgba8(col);RecordPrimitiveColorPacks(1,1);if(visibility==PrimitiveClipResult::Contained){SubmitContainedTriangle(points[0],points[1],points[2],packed);SubmitContainedTriangle(points[0],points[2],points[3],packed);}else{SubmitTransformedTriangle(points[0],points[1],points[2],col.a,packed);SubmitTransformedTriangle(points[0],points[2],points[3],col.a,packed);}}
void DrawList::IsoPrism(Vec2 c,float rx,float ry,float h,Color top,Color left,Color right){const std::array<Vec2,7> points={ApplyTransform2D({c.x,c.y-ry-h}),ApplyTransform2D({c.x+rx,c.y-h}),ApplyTransform2D({c.x,c.y+ry-h}),ApplyTransform2D({c.x-rx,c.y-h}),ApplyTransform2D({c.x,c.y+ry}),ApplyTransform2D({c.x-rx,c.y}),ApplyTransform2D({c.x+rx,c.y})};RecordSharedVertexTransforms(7,11);const float maximumAlpha=(std::max)({top.a,left.a,right.a});const PrimitiveClipResult visibility=ClassifyTransformedPrimitive(points,maximumAlpha,6);if(visibility==PrimitiveClipResult::Culled)return;const uint32_t packedTop=PackColorRgba8(top),packedLeft=PackColorRgba8(left),packedRight=PackColorRgba8(right);RecordPrimitiveColorPacks(3,3);auto submit=[&](Vec2 a,Vec2 b,Vec2 d,Color color,uint32_t packed){if(visibility==PrimitiveClipResult::Contained&&color.a>0.f)SubmitContainedTriangle(a,b,d,packed);else SubmitTransformedTriangle(a,b,d,color.a,packed);};submit(points[3],points[2],points[4],left,packedLeft);submit(points[3],points[4],points[5],left,packedLeft);submit(points[2],points[1],points[6],right,packedRight);submit(points[2],points[6],points[4],right,packedRight);submit(points[0],points[1],points[2],top,packedTop);submit(points[0],points[2],points[3],top,packedTop);}
void DrawList::IsoFrustum(Vec2 c,float rx,float ry,float topScale,float h,Color top,Color lightSide,Color darkSide,int n){if(n<3)return;n=(std::min)(n,64);rx=std::abs(rx);ry=std::abs(ry);topScale=(std::max)(0.f,topScale);h=(std::max)(0.f,h);const size_t segments=size_t(n),triangles=segments*3;const float maximumAlpha=(std::max)({top.a,lightSide.a,darkSide.a});const float maximumRadiusScale=(std::max)(1.f,topScale);const PrimitiveClipResult visibility=ClassifyTessellatedPrimitive({c.x-rx*maximumRadiusScale,c.y-h-ry*topScale},{c.x+rx*maximumRadiusScale,c.y+ry},{0,0,0,maximumAlpha},triangles);if(visibility==PrimitiveClipResult::Culled)return;const EllipseStepResult step=EllipseStep(n);const size_t evaluations=step.cached?0:2,savedEvaluations=segments*4-evaluations;stats_.ellipseSegments+=segments;stats_.ellipseTrigonometricEvaluations+=evaluations;stats_.ellipseTrigonometricEvaluationsSaved+=savedEvaluations;stats_.totalEllipseSegments+=segments;stats_.totalEllipseTrigonometricEvaluationsSaved+=savedEvaluations;const uint32_t packedTop=PackColorRgba8(top),packedLight=PackColorRgba8(lightSide),packedDark=PackColorRgba8(darkSide);RecordSharedVertexTransforms(segments*2+1,segments*7-1);RecordPrimitiveColorPacks(3,triangles-3);const Vec2 transformedTopCenter=ApplyTransform2D({c.x,c.y-h});std::array<Vec2,64> bottom{},topRing{};std::array<float,64> directionX{};Vec2 unit{1,0};for(int index=0;index<n;++index){bottom[index]=ApplyTransform2D({c.x+unit.x*rx,c.y+unit.y*ry});topRing[index]=ApplyTransform2D({c.x+unit.x*rx*topScale,c.y-h+unit.y*ry*topScale});directionX[index]=unit.x;unit={unit.x*step.cosine-unit.y*step.sine,unit.x*step.sine+unit.y*step.cosine};}auto submit=[&](Vec2 a,Vec2 b,Vec2 d,Color color,uint32_t packed){if(visibility==PrimitiveClipResult::Contained&&color.a>0.f)SubmitContainedTriangle(a,b,d,packed);else SubmitTransformedTriangle(a,b,d,color.a,packed);};for(int index=0;index<n;++index){const int next=(index+1)%n;const bool lit=directionX[index]+directionX[next]>=0;const Color side=lit?lightSide:darkSide;const uint32_t packedSide=lit?packedLight:packedDark;submit(bottom[index],bottom[next],topRing[next],side,packedSide);submit(bottom[index],topRing[next],topRing[index],side,packedSide);}for(int index=0;index<n;++index)submit(transformedTopCenter,topRing[index],topRing[(index+1)%n],top,packedTop);}
void DrawList::Ellipse(Vec2 c,float rx,float ry,Color col,int n){if(n<=0)return;const float extentX=std::abs(rx),extentY=std::abs(ry);const size_t segments=size_t(n);const PrimitiveClipResult visibility=ClassifyTessellatedPrimitive({c.x-extentX,c.y-extentY},{c.x+extentX,c.y+extentY},col,segments);if(visibility==PrimitiveClipResult::Culled)return;const EllipseStepResult step=EllipseStep(n);const size_t evaluations=step.cached?0:2,savedEvaluations=segments*4-evaluations;stats_.ellipseSegments+=segments;stats_.ellipseTrigonometricEvaluations+=evaluations;stats_.ellipseTrigonometricEvaluationsSaved+=savedEvaluations;stats_.totalEllipseSegments+=segments;stats_.totalEllipseTrigonometricEvaluationsSaved+=savedEvaluations;Vec2 current{rx,0};const Vec2 transformedCenter=ApplyTransform2D(c),transformedFirst=ApplyTransform2D(c+current);const uint32_t packed=PackColorRgba8(col);Vec2 transformedCurrent=transformedFirst;RecordSharedVertexTransforms(segments+1,segments*2-1);RecordPrimitiveColorPacks(1,segments-1);for(int i=0;i<n;++i){const Vec2 next=i+1==n?Vec2{rx,0}:Vec2{current.x*step.cosine-current.y*step.sine,current.x*step.sine+current.y*step.cosine};const Vec2 transformedNext=i+1==n?transformedFirst:ApplyTransform2D(c+next);if(visibility==PrimitiveClipResult::Contained)SubmitContainedTriangle(transformedCenter,transformedCurrent,transformedNext,packed);else SubmitTransformedTriangle(transformedCenter,transformedCurrent,transformedNext,col.a,packed);current=next;transformedCurrent=transformedNext;}}
void DrawList::RadialGradientEllipse(Vec2 c,float rx,float ry,Color inner,Color outer,int n){if(n<3)return;rx=std::abs(rx);ry=std::abs(ry);const size_t segments=size_t(n);const PrimitiveClipResult visibility=ClassifyTessellatedPrimitive({c.x-rx,c.y-ry},{c.x+rx,c.y+ry},{0,0,0,(std::max)(inner.a,outer.a)},segments);if(visibility==PrimitiveClipResult::Culled)return;const EllipseStepResult step=EllipseStep(n);const size_t evaluations=step.cached?0:2,savedEvaluations=segments*4-evaluations;stats_.ellipseSegments+=segments;stats_.ellipseTrigonometricEvaluations+=evaluations;stats_.ellipseTrigonometricEvaluationsSaved+=savedEvaluations;stats_.totalEllipseSegments+=segments;stats_.totalEllipseTrigonometricEvaluationsSaved+=savedEvaluations;const Vec2 transformedCenter=ApplyTransform2D(c),transformedFirst=ApplyTransform2D({c.x+rx,c.y});Vec2 unit{1,0},transformedCurrent=transformedFirst;const uint32_t packedInner=PackColorRgba8(inner),packedOuter=PackColorRgba8(outer);RecordSharedVertexTransforms(segments+1,segments*2-1);RecordPrimitiveColorPacks(2,segments*2-2);const bool contained=visibility==PrimitiveClipResult::Contained;for(int index=0;index<n;++index){const Vec2 nextUnit=index+1==n?Vec2{1,0}:Vec2{unit.x*step.cosine-unit.y*step.sine,unit.x*step.sine+unit.y*step.cosine};const Vec2 transformedNext=index+1==n?transformedFirst:ApplyTransform2D({c.x+nextUnit.x*rx,c.y+nextUnit.y*ry});SubmitGradientTriangle(transformedCenter,packedInner,transformedCurrent,packedOuter,transformedNext,packedOuter,contained);unit=nextUnit;transformedCurrent=transformedNext;}}
void DrawList::RadialLights(std::span<const RadialLight2D> lights,size_t maximumLights){
    struct SelectedLight {size_t index{};double score{};};
    struct BlendModeRestore {BlendMode2D& target;BlendMode2D previous;~BlendModeRestore(){target=previous;}};
    std::array<SelectedLight,MaxRadialLightsPerBatch> selected{};
    maximumLights=(std::min)(maximumLights,MaxRadialLightsPerBatch);
    size_t selectedCount=0,visibleCount=0,additionalVertices=0;
    ++stats_.radialLightBatches;++stats_.totalRadialLightBatches;stats_.radialLightCandidates+=lights.size();stats_.totalRadialLightCandidates+=lights.size();
    for(size_t index=0;index<lights.size();++index){
        const RadialLight2D& light=lights[index];
        const float rx=std::abs(light.radius.x),ry=std::abs(light.radius.y);
        const bool valid=std::isfinite(light.center.x)&&std::isfinite(light.center.y)&&std::isfinite(rx)&&std::isfinite(ry)&&std::isfinite(light.color.r)&&std::isfinite(light.color.g)&&std::isfinite(light.color.b)&&std::isfinite(light.color.a)&&std::isfinite(light.intensity)&&rx>0&&ry>0&&light.color.a>0&&light.intensity>0&&light.segments>=3;
        if(!valid){++stats_.radialLightsCulled;++stats_.totalRadialLightsCulled;continue;}
        const float effectiveAlpha=Clamp(light.color.a*light.intensity,0.f,1.f);
        const std::array<Vec2,4> corners={ApplyTransform2D({light.center.x-rx,light.center.y-ry}),ApplyTransform2D({light.center.x+rx,light.center.y-ry}),ApplyTransform2D({light.center.x+rx,light.center.y+ry}),ApplyTransform2D({light.center.x-rx,light.center.y+ry})};
        if(std::ranges::any_of(corners,[](Vec2 point){return!std::isfinite(point.x)||!std::isfinite(point.y);})) {++stats_.radialLightsCulled;++stats_.totalRadialLightsCulled;continue;}
        const auto [minimumX,maximumX]=std::ranges::minmax(corners,{},&Vec2::x);const auto [minimumY,maximumY]=std::ranges::minmax(corners,{},&Vec2::y);
        const double visibleWidth=clipEnabled_?(std::max)(0.0,double((std::min)(maximumX.x,clipMaximum_.x)-(std::max)(minimumX.x,clipMinimum_.x))):double(maximumX.x-minimumX.x);
        const double visibleHeight=clipEnabled_?(std::max)(0.0,double((std::min)(maximumY.y,clipMaximum_.y)-(std::max)(minimumY.y,clipMinimum_.y))):double(maximumY.y-minimumY.y);
        if(visibleWidth<=0||visibleHeight<=0){++stats_.radialLightsCulled;++stats_.totalRadialLightsCulled;continue;}
        ++visibleCount;if(!maximumLights)continue;
        const double score=double(effectiveAlpha)*visibleWidth*visibleHeight;
        size_t destination=selectedCount;while(destination>0&&score>selected[destination-1].score)--destination;if(destination>=maximumLights)continue;
        const size_t newCount=(std::min)(maximumLights,selectedCount+1);for(size_t move=newCount-1;move>destination;--move)selected[move]=selected[move-1];selected[destination]={index,score};selectedCount=newCount;
    }
    const size_t dropped=visibleCount-selectedCount;stats_.radialLightsBudgetDropped+=dropped;stats_.totalRadialLightsBudgetDropped+=dropped;stats_.radialLightsSubmitted+=selectedCount;stats_.totalRadialLightsSubmitted+=selectedCount;
    for(size_t selection=0;selection<selectedCount;++selection)additionalVertices+=size_t((std::clamp)(lights[selected[selection].index].segments,3,64))*3;
    vertices.reserve(vertices.size()+additionalVertices);
    const BlendModeRestore restore{blendMode_,blendMode_};blendMode_=BlendMode2D::Additive;
    for(size_t selection=0;selection<selectedCount;++selection){const RadialLight2D& light=lights[selected[selection].index];Color inner=light.color;inner.a=Clamp(inner.a*light.intensity,0.f,1.f);RadialGradientEllipse(light.center,light.radius.x,light.radius.y,inner,{inner.r,inner.g,inner.b,0},(std::clamp)(light.segments,3,64));}
}
void DrawList::EllipseOutline(Vec2 c,float rx,float ry,float width,Color col,int n){if(n<3||width<=0)return;rx=std::abs(rx);ry=std::abs(ry);const size_t segments=size_t(n),triangles=segments*2;const float margin=width*.5f;const PrimitiveClipResult visibility=ClassifyTessellatedPrimitive({c.x-rx-margin,c.y-ry-margin},{c.x+rx+margin,c.y+ry+margin},col,triangles);if(visibility==PrimitiveClipResult::Culled)return;const EllipseStepResult step=EllipseStep(n);const size_t evaluations=step.cached?0:2,savedEvaluations=segments*4-evaluations;stats_.ellipseSegments+=segments;stats_.ellipseOutlineSegments+=segments;stats_.ellipseTrigonometricEvaluations+=evaluations;stats_.ellipseTrigonometricEvaluationsSaved+=savedEvaluations;stats_.totalEllipseSegments+=segments;stats_.totalEllipseOutlineSegments+=segments;stats_.totalEllipseTrigonometricEvaluationsSaved+=savedEvaluations;const uint32_t packed=PackColorRgba8(col);RecordSharedVertexTransforms(segments*4,segments*2);RecordPrimitiveColorPacks(1,segments-1);Vec2 current{rx,0};for(int index=0;index<n;++index){const Vec2 next=index+1==n?Vec2{rx,0}:Vec2{current.x*step.cosine-current.y*step.sine,current.x*step.sine+current.y*step.cosine};const Vec2 normal=Normalize(Vec2{-(next.y-current.y),next.x-current.x})*margin;const std::array<Vec2,4> points={ApplyTransform2D(c+current-normal),ApplyTransform2D(c+current+normal),ApplyTransform2D(c+next+normal),ApplyTransform2D(c+next-normal)};PrimitiveClipResult segmentVisibility=visibility;if(visibility==PrimitiveClipResult::Partial)segmentVisibility=ClassifyTransformedPrimitive(points,col.a,2);if(segmentVisibility==PrimitiveClipResult::Contained){SubmitContainedTriangle(points[0],points[1],points[2],packed);SubmitContainedTriangle(points[0],points[2],points[3],packed);}else if(segmentVisibility==PrimitiveClipResult::Partial){SubmitTransformedTriangle(points[0],points[1],points[2],col.a,packed);SubmitTransformedTriangle(points[0],points[2],points[3],col.a,packed);}current=next;}}
void DrawList::ProjectedEllipseShadow(Vec2 base,float rx,float ry,Vec2 lightDirection,float height,float opacity,int segments){const Vec2 direction=Normalize(lightDirection);Ellipse(base+direction*(std::max)(0.f,height),(std::max)(0.f,rx),(std::max)(0.f,ry),{0,0,0,Clamp(opacity,0.f,1.f)},segments);}
void DrawList::Circle(Vec2 c,float r,Color col,int n){Ellipse(c,r,r,col,n);}
void DrawList::Line(Vec2 a,Vec2 b,float w,Color col){const Vec2 n=Normalize(Vec2{-(b.y-a.y),b.x-a.x})*(w*.5f);const std::array<Vec2,4> points={ApplyTransform2D(a+n),ApplyTransform2D(b+n),ApplyTransform2D(b-n),ApplyTransform2D(a-n)};RecordSharedVertexTransforms(4,2);const PrimitiveClipResult visibility=ClassifyTransformedPrimitive(points,col.a,2);if(visibility==PrimitiveClipResult::Culled)return;const uint32_t packed=PackColorRgba8(col);RecordPrimitiveColorPacks(1,1);if(visibility==PrimitiveClipResult::Contained){SubmitContainedTriangle(points[0],points[1],points[2],packed);SubmitContainedTriangle(points[0],points[2],points[3],packed);}else{SubmitTransformedTriangle(points[0],points[1],points[2],col.a,packed);SubmitTransformedTriangle(points[0],points[2],points[3],col.a,packed);}}
void DrawList::Geometry(const Geometry2D& geometry){const std::span<const Vertex> source=geometry.Vertices();if(source.empty())return;++stats_.geometryBatches;stats_.geometryVertices+=source.size();const Bounds2D bounds=geometry.Bounds();const std::array<Vec2,4> corners={ApplyTransform2D(bounds.minimum),ApplyTransform2D({bounds.maximum.x,bounds.minimum.y}),ApplyTransform2D(bounds.maximum),ApplyTransform2D({bounds.minimum.x,bounds.maximum.y})};const std::size_t triangles=source.size()/3;const PrimitiveClipResult visibility=ClassifyTransformedPrimitive(corners,1.f,triangles);if(visibility==PrimitiveClipResult::Culled){++stats_.geometryCulledBatches;return;}vertices.reserve(vertices.size()+source.size());if(transformIdentity_&&visibility==PrimitiveClipResult::Contained){const size_t firstVertex=vertices.size();vertices.insert(vertices.end(),source.begin(),source.end());RecordSubmittedVertices(firstVertex,source.size());stats_.geometryTransformsSaved+=source.size();stats_.inputTriangles+=triangles;stats_.totalInputTriangles+=triangles;stats_.submittedTriangles+=triangles;stats_.peakSubmittedTriangles=(std::max)(stats_.peakSubmittedTriangles,stats_.submittedTriangles);return;}for(std::size_t index=0;index<source.size();index+=3){const Vertex& a=source[index];const Vertex& b=source[index+1];const Vertex& c=source[index+2];const Vec2 transformedA=ApplyTransform2D({a.x,a.y}),transformedB=ApplyTransform2D({b.x,b.y}),transformedC=ApplyTransform2D({c.x,c.y});++stats_.inputTriangles;++stats_.totalInputTriangles;bool culled=false;if(visibility==PrimitiveClipResult::Partial){const float maximumX=(std::max)({transformedA.x,transformedB.x,transformedC.x}),minimumX=(std::min)({transformedA.x,transformedB.x,transformedC.x}),maximumY=(std::max)({transformedA.y,transformedB.y,transformedC.y}),minimumY=(std::min)({transformedA.y,transformedB.y,transformedC.y});culled=maximumX<clipMinimum_.x||minimumX>clipMaximum_.x||maximumY<clipMinimum_.y||minimumY>clipMaximum_.y;}if(culled){++stats_.culledTriangles;++stats_.totalCulledTriangles;continue;}const size_t firstVertex=vertices.size();vertices.push_back({transformedA.x,transformedA.y,a.color});vertices.push_back({transformedB.x,transformedB.y,b.color});vertices.push_back({transformedC.x,transformedC.y,c.color});RecordSubmittedVertices(firstVertex,3);++stats_.submittedTriangles;stats_.peakSubmittedTriangles=(std::max)(stats_.peakSubmittedTriangles,stats_.submittedTriangles);}}

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
struct GlyphSpan{uint16_t x{},y{},length{};uint8_t coverage{};};
struct SystemGlyph{int width{},height{},advance{18};size_t sourcePixels{};std::vector<GlyphSpan> spans;};
struct CachedSystemGlyph{SystemGlyph glyph;uint64_t lastUse{};size_t bytes{};};
class SystemGlyphCache2D{
public:
 SystemGlyphCache2D()=default;
 ~SystemGlyphCache2D(){if(dc_&&previousFont_)SelectObject(dc_,previousFont_);if(font_)DeleteObject(font_);if(dc_)DeleteDC(dc_);}
 const SystemGlyph& Load(uint32_t codepoint){if(auto found=entries_.find(codepoint);found!=entries_.end()){found->second.lastUse=++clock_;++stats_.hits;return found->second.glyph;}EnsureGdi();++stats_.misses;SystemGlyph glyph;GLYPHMETRICS metrics{};MAT2 matrix{};matrix.eM11.value=1;matrix.eM22.value=1;const DWORD bytes=dc_?GetGlyphOutlineW(dc_,codepoint,GGO_GRAY8_BITMAP,&metrics,0,nullptr,&matrix):GDI_ERROR;if(bytes!=GDI_ERROR){glyph.width=int(metrics.gmBlackBoxX);glyph.height=int(metrics.gmBlackBoxY);glyph.advance=(std::max)(int(metrics.gmCellIncX),glyph.width);std::vector<unsigned char> pixels(bytes);GetGlyphOutlineW(dc_,codepoint,GGO_GRAY8_BITMAP,&metrics,bytes,pixels.data(),&matrix);const int pitch=(glyph.width+3)&~3;for(int y=0;y<glyph.height;++y){int x=0;while(x<glyph.width){const unsigned char source=pixels[size_t(y)*pitch+x];const uint8_t level=source?uint8_t((std::min)(8u,(unsigned(source)+7u)/8u)):0;if(!level){++x;continue;}const int start=x;while(++x<glyph.width){const unsigned char next=pixels[size_t(y)*pitch+x];const uint8_t nextLevel=next?uint8_t((std::min)(8u,(unsigned(next)+7u)/8u)):0;if(nextLevel!=level)break;}glyph.spans.push_back({uint16_t(start),uint16_t(y),uint16_t(x-start),uint8_t(level*8)});glyph.sourcePixels+=size_t(x-start);}}}CachedSystemGlyph cached{std::move(glyph),++clock_};cached.bytes=sizeof(uint32_t)+sizeof(CachedSystemGlyph)+cached.glyph.spans.capacity()*sizeof(GlyphSpan);stats_.residentBytes+=cached.bytes;entries_.emplace(codepoint,std::move(cached));while(entries_.size()>1&&(entries_.size()>DrawList::MaxSystemGlyphCacheEntries||stats_.residentBytes>DrawList::MaxSystemGlyphCacheBytes))EvictOldest();RefreshPeaks();return entries_.find(codepoint)->second.glyph;}
 size_t Trim(size_t retainedGlyphs){retainedGlyphs=(std::min)(retainedGlyphs,DrawList::MaxSystemGlyphCacheEntries);const size_t before=stats_.residentBytes;while(entries_.size()>retainedGlyphs)EvictOldest();if(entries_.empty()&&entries_.bucket_count()>128)entries_.rehash(0);++stats_.trims;RefreshResident();return before-stats_.residentBytes;}
 size_t ReclaimableBytes(size_t retainedGlyphs)const{return entries_.size()>retainedGlyphs?stats_.residentBytes:0;}
 SystemGlyphCacheStats2D Stats()const{return stats_;}
private:
 void EnsureGdi(){if(dc_)return;dc_=CreateCompatibleDC(nullptr);font_=CreateFontW(-18,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,CHINESEBIG5_CHARSET,OUT_TT_PRECIS,CLIP_DEFAULT_PRECIS,ANTIALIASED_QUALITY,DEFAULT_PITCH|FF_DONTCARE,L"Microsoft JhengHei UI");if(dc_&&font_)previousFont_=SelectObject(dc_,font_);}
 void EvictOldest(){if(entries_.empty())return;auto oldest=std::ranges::min_element(entries_,{},[](const auto& entry){return entry.second.lastUse;});stats_.residentBytes-=oldest->second.bytes;stats_.releasedBytes+=oldest->second.bytes;++stats_.evictions;entries_.erase(oldest);}
 void RefreshResident(){stats_.residentGlyphs=entries_.size();}
 void RefreshPeaks(){RefreshResident();stats_.peakResidentGlyphs=(std::max)(stats_.peakResidentGlyphs,stats_.residentGlyphs);stats_.peakResidentBytes=(std::max)(stats_.peakResidentBytes,stats_.residentBytes);}
 std::unordered_map<uint32_t,CachedSystemGlyph> entries_;HDC dc_{};HFONT font_{};HGDIOBJ previousFont_{};uint64_t clock_{};SystemGlyphCacheStats2D stats_{};
};
static SystemGlyphCache2D& SystemGlyphCache(){static SystemGlyphCache2D cache;return cache;}
static const SystemGlyph& LoadSystemGlyph(uint32_t codepoint){
 return SystemGlyphCache().Load(codepoint);
}
static size_t SystemGlyphCacheTrackedBytes(){return SystemGlyphCache().Stats().residentBytes;}
static size_t SystemGlyphCacheReclaimableBytes(size_t retainedGlyphs){return SystemGlyphCache().ReclaimableBytes(retainedGlyphs);}
SystemGlyphCacheStats2D DrawList::SystemGlyphCacheStats(){return SystemGlyphCache().Stats();}
size_t DrawList::TrimSystemGlyphCache(size_t retainedGlyphs){return SystemGlyphCache().Trim(retainedGlyphs);}
static uint32_t NextUtf8(std::string_view text,size_t& i){unsigned char a=static_cast<unsigned char>(text[i++]);if(a<0x80)return a;if((a&0xe0)==0xc0&&i<text.size())return((a&0x1f)<<6)|(static_cast<unsigned char>(text[i++])&0x3f);if((a&0xf0)==0xe0&&i+1<text.size()){uint32_t v=(a&0x0f)<<12;v|=(static_cast<unsigned char>(text[i++])&0x3f)<<6;v|=static_cast<unsigned char>(text[i++])&0x3f;return v;}if((a&0xf8)==0xf0&&i+2<text.size()){uint32_t v=(a&7)<<18;v|=(static_cast<unsigned char>(text[i++])&0x3f)<<12;v|=(static_cast<unsigned char>(text[i++])&0x3f)<<6;v|=static_cast<unsigned char>(text[i++])&0x3f;return v;}return'?';}
void DrawList::Text(Vec2 p,std::string_view text,float s,Color col){++stats_.textContainmentTests;++stats_.totalTextContainmentTests;PrimitiveClipResult visibility=col.a>0.f?PrimitiveClipResult::Contained:PrimitiveClipResult::Culled;if(col.a>0.f&&clipEnabled_){const Vec2 size=MeasureText(text,s),end=p+size;const float padding=std::abs(s)*.5f;visibility=ClassifyTransformedBounds({(std::min)(p.x,end.x)-padding,(std::min)(p.y,end.y)-padding},{(std::max)(p.x,end.x)+padding,(std::max)(p.y,end.y)+padding});}auto spanQuad=[&](Vec2 center,Vec2 size,Color color){if(visibility==PrimitiveClipResult::Contained)SubmitContainedTextQuad(center,size,color);else if(visibility==PrimitiveClipResult::Culled)CullTextQuad();else Quad(center,size,color);};for(size_t i=0;i<text.size();){uint32_t code=NextUtf8(text,i);++stats_.textGlyphs;if(code<128){char ch=char(code);ch=ch>='a'&&ch<='z'?char(ch-32):ch;auto it=Font.find(ch);if(it!=Font.end())for(int y=0;y<7;++y){const unsigned char row=it->second[y];int x=0;while(x<5){if(!(row&(1<<(4-x)))){++x;continue;}const int start=x;while(++x<5&&row&(1<<(4-x))){}const int length=x-start;stats_.textSourcePixels+=length;++stats_.textSpans;stats_.totalTextSourcePixels+=length;++stats_.totalTextSpans;const float width=(float(length-1)+.82f)*s;spanQuad({p.x+(float(start)+float(length-1)*.5f)*s,p.y+y*s},{width,s*.82f},col);}}p.x+=6*s;continue;}const SystemGlyph& glyph=LoadSystemGlyph(code);const float pixel=7.f*s/18.f;stats_.textSourcePixels+=glyph.sourcePixels;stats_.textSpans+=glyph.spans.size();stats_.totalTextSourcePixels+=glyph.sourcePixels;stats_.totalTextSpans+=glyph.spans.size();for(const GlyphSpan&span:glyph.spans){const float width=float(span.length)*pixel;spanQuad({p.x+(float(span.x)+float(span.length)*.5f)*pixel,p.y+(float(span.y)+.5f)*pixel},{width,pixel},{col.r,col.g,col.b,col.a*(std::min)(1.f,float(span.coverage)/64.f)});}p.x+=(glyph.advance+1)*pixel;}}
Vec2 DrawList::MeasureText(std::string_view text,float s)const{float width=0,height=7.f*s;for(size_t index=0;index<text.size();){const uint32_t code=NextUtf8(text,index);if(code<128){width+=6.f*s;continue;}const SystemGlyph&glyph=LoadSystemGlyph(code);const float pixel=7.f*s/18.f;width+=(glyph.advance+1)*pixel;height=(std::max)(height,float(glyph.height)*pixel);}return{width,height};}

struct D3D12Renderer::Impl{
 ComPtr<ID3D12PipelineState> ambientOcclusionWorldColorPipeline,ambientOcclusionInstancedColorPipeline,ambientOcclusionInstancedCulledColorPipeline;
 static constexpr unsigned Frames=D3D12Renderer::BufferedFrames;
 unsigned width{},height{},logicalWidth{1280},logicalHeight{720},frame{};UINT rtvStep{};HANDLE fenceEvent{};UINT64 nextFenceValue{1};bool dlssRequested{},dlssValidationComplete{};
 ComPtr<IDXGIFactory6> factory;ComPtr<ID3D12Device> device;ComPtr<ID3D12CommandQueue> queue;ComPtr<IDXGISwapChain3> swap;
 ComPtr<ID3D12DescriptorHeap> rtvHeap,dsvHeap,shadowDsvHeap,textureHeap;ComPtr<ID3D12Resource> depth,shadowDepth,ambientOcclusionRaw,ambientOcclusionBlur;std::array<ComPtr<ID3D12Resource>,Frames> targets;std::array<ComPtr<ID3D12CommandAllocator>,Frames> alloc;
 ComPtr<ID3D12GraphicsCommandList> list;ComPtr<ID3D12Fence> fence;ComPtr<ID3D12RootSignature> root,postProcessRoot,ambientOcclusionRoot;ComPtr<ID3D12PipelineState> pipeline,additivePipeline,worldPipeline,instancedPipeline,instancedCulledPipeline,instancedTransparentPipeline,instancedOverlayPipeline,skinnedPipeline,skinnedCulledPipeline,shadowWorldPipeline,shadowInstancedPipeline,shadowSkinnedPipeline,postProcessPipeline,ambientOcclusionDepthWorldPipeline,ambientOcclusionDepthInstancedPipeline,ambientOcclusionDepthInstancedCulledPipeline,ambientOcclusionDepthSkinnedPipeline,ambientOcclusionPipeline,ambientOcclusionBlurPipeline;ComPtr<ID3D12QueryHeap> timestampHeap;ComPtr<ID3D12Resource> timestampReadback,sceneConstants,shadowConstants,hdrSceneColor;std::array<ComPtr<ID3D12Resource>,Frames> vertexBuffers,worldVertexBuffers,worldIndexBuffers,instanceBuffers,skinnedInstanceBuffers,skinPaletteBuffers;std::array<Vertex*,Frames> mapped{};std::array<Vertex3D*,Frames> worldMapped{};std::array<uint32_t*,Frames> worldIndexMapped{};std::array<GpuMeshInstance*,Frames> instanceMapped{};std::array<GpuSkinnedInstance*,Frames> skinnedInstanceMapped{};std::array<SkinPaletteJoint3D*,Frames> skinPaletteMapped{};std::array<size_t,Frames> vertexCapacity{},worldVertexCapacity{},worldIndexCapacity{},instanceCapacity{},skinnedInstanceCapacity{},skinPaletteCapacity{};uint64_t* timestampMapped{};float* sceneConstantsMapped{};float* shadowConstantsMapped{};StreamlineDLSS dlss;MeshUploadBatch3D meshUploadBatch;std::vector<PendingMeshUpload3D> pendingMeshUploads;std::vector<MeshUploadContext3D> availableMeshUploadContexts;TextureUploadBatch3D textureUploadBatch;std::vector<PendingTextureUpload3D> pendingTextureUploads;std::vector<MeshUploadContext3D> availableTextureUploadContexts;
 std::array<D3D12_VERTEX_BUFFER_VIEW,Frames> vbv{},worldVbv{};std::array<D3D12_INDEX_BUFFER_VIEW,Frames> worldIbv{};std::array<UINT64,Frames> frameFenceValues{};std::array<bool,Frames> timestampValid{};uint64_t timestampFrequency{};UINT textureStep{};size_t activeShadowSrvDescriptor{ShadowNullSrvDescriptor3D};std::unordered_map<uint32_t,ResidentMesh3D> residentMeshes;std::unordered_map<MeshCacheKey3D,uint32_t,MeshCacheKeyHasher3D> meshCache;std::unordered_map<uint32_t,ResidentSkinnedMesh3D> residentSkinnedMeshes;std::unordered_map<MeshCacheKey3D,uint32_t,MeshCacheKeyHasher3D> skinnedMeshCache;std::unordered_map<uint32_t,ResidentMaterialTextureSet3D> residentMaterialTextureSets;std::unordered_map<std::wstring,uint32_t> materialTextureCache;std::unordered_map<InstanceGroupKey3D,InstanceGroup3D,InstanceGroupKeyHasher3D> instanceGroups,overlayInstanceGroups,shadowOnlyInstanceGroups;std::vector<ActiveMeshGroup3D> activeMeshGroups,activeOverlayMeshGroups,activeShadowOnlyMeshGroups;std::vector<uint32_t> freeTextureSlots;std::vector<RetiredGpuResources3D> retiredGpuResources;std::filesystem::path materialDerivedDataCacheDirectory{DefaultMaterialCacheDirectory()},pendingCapturePath{};size_t materialDerivedDataCacheBudgetBytes{D3D12Renderer::DefaultMaterialDerivedDataCacheBudgetBytes};bool materialDerivedDataCacheSurveyed{};size_t meshMemoryBudgetBytes{D3D12Renderer::DefaultMeshMemoryBudgetBytes},textureMemoryBudgetBytes{D3D12Renderer::DefaultMaterialTextureMemoryBudgetBytes},pendingMeshReleaseBytes{},pendingTextureReleaseBytes{},lastTransientMemoryReleasedBytes{};uint32_t nextMeshHandle{1},nextSkinnedMeshHandle{1},nextMaterialTextureSetHandle{1},nextTextureSlot{1};uint64_t transientMemoryTrimCount{};RendererStats stats{};
 size_t InstanceGroupCapacityBytes()const{size_t bytes{};const auto add=[&](const auto& groups){for(const auto& [key,group]:groups){(void)key;bytes+=group.instances.capacity()*sizeof(GpuMeshInstance);}};add(instanceGroups);add(overlayInstanceGroups);add(shadowOnlyInstanceGroups);return bytes;}
 void RefreshInstanceGroupStats(){stats.residentInstanceGroups=instanceGroups.size()+overlayInstanceGroups.size()+shadowOnlyInstanceGroups.size();stats.peakResidentInstanceGroups=(std::max)(stats.peakResidentInstanceGroups,stats.residentInstanceGroups);stats.residentInstanceGroupCapacityBytes=InstanceGroupCapacityBytes();stats.peakInstanceGroupCapacityBytes=(std::max)(stats.peakInstanceGroupCapacityBytes,stats.residentInstanceGroupCapacityBytes);}
 void TrimMaterialDerivedDataCache(){const MaterialCacheTrimResult3D trim=TrimMaterialCache3D(materialDerivedDataCacheDirectory,materialDerivedDataCacheBudgetBytes);stats.materialDerivedDataCacheResidentBytes=trim.residentBytes;stats.peakMaterialDerivedDataCacheResidentBytes=(std::max)(stats.peakMaterialDerivedDataCacheResidentBytes,trim.residentBytes+size_t(trim.releasedBytes));if(trim.evictions){stats.materialDerivedDataCacheEvictions+=trim.evictions;stats.materialDerivedDataCacheReleasedBytes+=trim.releasedBytes;++stats.materialDerivedDataCacheTrims;}materialDerivedDataCacheSurveyed=true;}
 template<class T>bool ReleaseFrameUploadBuffer(ComPtr<ID3D12Resource>& buffer,T*& mappedData,size_t& capacity){if(!buffer)return false;buffer->Unmap(0,nullptr);buffer.Reset();stats.frameUploadResidentBytes-=capacity*sizeof(T);mappedData=nullptr;capacity=0;++stats.frameUploadBufferReleases;return true;}
 template<class T>void EnsureFrameUploadBuffer(ComPtr<ID3D12Resource>& buffer,T*& mappedData,size_t& capacity,size_t required,size_t initial,size_t maximum){const size_t target=D3D12Renderer::NextFrameUploadCapacity(capacity?capacity:initial,required,maximum);if(buffer&&capacity>=target)return;D3D12_HEAP_PROPERTIES heap{};heap.Type=D3D12_HEAP_TYPE_UPLOAD;D3D12_RESOURCE_DESC desc{};desc.Dimension=D3D12_RESOURCE_DIMENSION_BUFFER;desc.Width=target*sizeof(T);desc.Height=1;desc.DepthOrArraySize=1;desc.MipLevels=1;desc.SampleDesc.Count=1;desc.Layout=D3D12_TEXTURE_LAYOUT_ROW_MAJOR;ComPtr<ID3D12Resource> replacement;T* replacementMapped{};Check(device->CreateCommittedResource(&heap,D3D12_HEAP_FLAG_NONE,&desc,D3D12_RESOURCE_STATE_GENERIC_READ,nullptr,IID_PPV_ARGS(&replacement)),"Frame upload buffer failed");Check(replacement->Map(0,nullptr,reinterpret_cast<void**>(&replacementMapped)),"Frame upload map failed");const bool growing=bool(buffer);if(growing)ReleaseFrameUploadBuffer(buffer,mappedData,capacity);buffer=std::move(replacement);mappedData=replacementMapped;capacity=target;stats.frameUploadResidentBytes+=capacity*sizeof(T);stats.peakFrameUploadResidentBytes=(std::max)(stats.peakFrameUploadResidentBytes,stats.frameUploadResidentBytes);++stats.frameUploadBufferAllocations;if(growing)++stats.frameUploadGrowths;}
 template<class T>bool TrimFrameUploadBuffer(ComPtr<ID3D12Resource>& buffer,T*& mappedData,size_t& capacity,size_t initial,size_t maximum){if(capacity<=initial)return false;D3D12_HEAP_PROPERTIES heap{};heap.Type=D3D12_HEAP_TYPE_UPLOAD;D3D12_RESOURCE_DESC desc{};desc.Dimension=D3D12_RESOURCE_DIMENSION_BUFFER;desc.Width=(std::min)(initial,maximum)*sizeof(T);desc.Height=1;desc.DepthOrArraySize=1;desc.MipLevels=1;desc.SampleDesc.Count=1;desc.Layout=D3D12_TEXTURE_LAYOUT_ROW_MAJOR;ComPtr<ID3D12Resource> replacement;T* replacementMapped{};Check(device->CreateCommittedResource(&heap,D3D12_HEAP_FLAG_NONE,&desc,D3D12_RESOURCE_STATE_GENERIC_READ,nullptr,IID_PPV_ARGS(&replacement)),"Frame upload trim buffer failed");Check(replacement->Map(0,nullptr,reinterpret_cast<void**>(&replacementMapped)),"Frame upload trim map failed");ReleaseFrameUploadBuffer(buffer,mappedData,capacity);buffer=std::move(replacement);mappedData=replacementMapped;capacity=(std::min)(initial,maximum);stats.frameUploadResidentBytes+=capacity*sizeof(T);++stats.frameUploadBufferAllocations;return true;}
 uint64_t WaitForValue(UINT64 value){if(value&&fence->GetCompletedValue()<value){const auto start=std::chrono::steady_clock::now();Check(fence->SetEventOnCompletion(value,fenceEvent),"Fence event failed");WaitForSingleObject(fenceEvent,INFINITE);return(std::max)(uint64_t(1),uint64_t(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now()-start).count()));}return 0;}
 void CollectRetiredGpuResources(){if(!fence||retiredGpuResources.empty())return;const UINT64 completed=fence->GetCompletedValue();for(auto iterator=retiredGpuResources.begin();iterator!=retiredGpuResources.end();){if(iterator->fenceValue>completed){++iterator;continue;}if(iterator->textureSlot!=UINT_MAX)freeTextureSlots.push_back(iterator->textureSlot);stats.pendingGpuReleaseBytes-=iterator->bytes;if(iterator->textures)pendingTextureReleaseBytes-=iterator->bytes;else pendingMeshReleaseBytes-=iterator->bytes;++stats.completedGpuReleaseBatches;iterator=retiredGpuResources.erase(iterator);}if(retiredGpuResources.empty()&&retiredGpuResources.capacity()>128){std::vector<RetiredGpuResources3D> compact;compact.reserve(32);retiredGpuResources.swap(compact);}}
 void CollectCompletedMeshUploads(){if(!fence||pendingMeshUploads.empty())return;const UINT64 completed=fence->GetCompletedValue();for(auto iterator=pendingMeshUploads.begin();iterator!=pendingMeshUploads.end();){if(iterator->fenceValue>completed){++iterator;continue;}stats.meshUploadStagingBytes-=iterator->bytes;stats.meshUploadStagingResources-=iterator->staging.size();++stats.completedMeshUploadBatches;if(availableMeshUploadContexts.size()<8)availableMeshUploadContexts.push_back(std::move(iterator->context));else{if(stats.residentMeshUploadContexts)--stats.residentMeshUploadContexts;++stats.meshUploadContextReleases;}iterator=pendingMeshUploads.erase(iterator);}if(pendingMeshUploads.empty()&&pendingMeshUploads.capacity()>32){std::vector<PendingMeshUpload3D> compact;compact.reserve(8);pendingMeshUploads.swap(compact);}}
 void CollectCompletedGpuWork(){CollectRetiredGpuResources();CollectCompletedMeshUploads();CollectCompletedTextureUploads();}
 void BeginMeshUploadBatch(){if(meshUploadBatch.context.list)return;CollectCompletedMeshUploads();if(!availableMeshUploadContexts.empty()){meshUploadBatch.context=std::move(availableMeshUploadContexts.back());availableMeshUploadContexts.pop_back();Check(meshUploadBatch.context.allocator->Reset(),"Mesh upload allocator reset failed");Check(meshUploadBatch.context.list->Reset(meshUploadBatch.context.allocator.Get(),nullptr),"Mesh upload list reset failed");++stats.meshUploadContextReuses;}else{Check(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,IID_PPV_ARGS(&meshUploadBatch.context.allocator)),"Mesh upload allocator failed");Check(device->CreateCommandList(0,D3D12_COMMAND_LIST_TYPE_DIRECT,meshUploadBatch.context.allocator.Get(),nullptr,IID_PPV_ARGS(&meshUploadBatch.context.list)),"Mesh upload list failed");++stats.meshUploadContextCreations;++stats.residentMeshUploadContexts;stats.peakResidentMeshUploadContexts=(std::max)(stats.peakResidentMeshUploadContexts,stats.residentMeshUploadContexts);}meshUploadBatch.staging.reserve(16);}
 bool SubmitMeshUploadBatch(){if(!meshUploadBatch.context.list||!meshUploadBatch.meshes)return false;if(pendingMeshUploads.size()==pendingMeshUploads.capacity())pendingMeshUploads.reserve((std::max)(size_t(8),pendingMeshUploads.capacity()*2));Check(meshUploadBatch.context.list->Close(),"Mesh upload close failed");ID3D12CommandList* lists[]={meshUploadBatch.context.list.Get()};queue->ExecuteCommandLists(1,lists);const UINT64 value=nextFenceValue++;Check(queue->Signal(fence.Get(),value),"Mesh upload signal failed");PendingMeshUpload3D pending{};pending.context=std::move(meshUploadBatch.context);pending.staging=std::move(meshUploadBatch.staging);pending.fenceValue=value;pending.bytes=meshUploadBatch.bytes;pending.meshes=meshUploadBatch.meshes;meshUploadBatch={};pendingMeshUploads.push_back(std::move(pending));++stats.meshUploadBatches;return true;}
 void PrepareMeshUpload(size_t bytes){CollectCompletedMeshUploads();if(meshUploadBatch.meshes&&(meshUploadBatch.meshes>=D3D12Renderer::MaxMeshUploadBatchMeshes||meshUploadBatch.bytes>=D3D12Renderer::MaxMeshUploadBatchBytes||bytes>D3D12Renderer::MaxMeshUploadBatchBytes-meshUploadBatch.bytes))SubmitMeshUploadBatch();while(!pendingMeshUploads.empty()&&(bytes>D3D12Renderer::MaxMeshUploadStagingBytes||stats.meshUploadStagingBytes>D3D12Renderer::MaxMeshUploadStagingBytes-bytes)){const uint64_t elapsed=WaitForValue(pendingMeshUploads.front().fenceValue);if(elapsed){++stats.meshUploadPressureWaits;stats.meshUploadPressureWaitNanoseconds+=elapsed;}CollectCompletedMeshUploads();}BeginMeshUploadBatch();const size_t required=meshUploadBatch.staging.size()+1;if(required>meshUploadBatch.staging.capacity())meshUploadBatch.staging.reserve((std::max)(required,(std::max)(size_t(16),meshUploadBatch.staging.capacity()*2)));}
 void CollectCompletedTextureUploads(){if(!fence||pendingTextureUploads.empty())return;const UINT64 completed=fence->GetCompletedValue();for(auto iterator=pendingTextureUploads.begin();iterator!=pendingTextureUploads.end();){if(iterator->fenceValue>completed){++iterator;continue;}stats.materialTextureUploadStagingBytes-=iterator->bytes;stats.materialTextureUploadStagingResources-=iterator->staging.size();++stats.completedMaterialTextureUploadBatches;if(availableTextureUploadContexts.size()<8)availableTextureUploadContexts.push_back(std::move(iterator->context));else{if(stats.residentMaterialTextureUploadContexts)--stats.residentMaterialTextureUploadContexts;++stats.materialTextureUploadContextReleases;}iterator=pendingTextureUploads.erase(iterator);}if(pendingTextureUploads.empty()&&pendingTextureUploads.capacity()>32){std::vector<PendingTextureUpload3D> compact;compact.reserve(8);pendingTextureUploads.swap(compact);}}
 void BeginTextureUploadBatch(){if(textureUploadBatch.context.list)return;CollectCompletedTextureUploads();if(!availableTextureUploadContexts.empty()){textureUploadBatch.context=std::move(availableTextureUploadContexts.back());availableTextureUploadContexts.pop_back();Check(textureUploadBatch.context.allocator->Reset(),"Texture upload allocator reset failed");Check(textureUploadBatch.context.list->Reset(textureUploadBatch.context.allocator.Get(),nullptr),"Texture upload list reset failed");++stats.materialTextureUploadContextReuses;}else{Check(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,IID_PPV_ARGS(&textureUploadBatch.context.allocator)),"Texture upload allocator failed");Check(device->CreateCommandList(0,D3D12_COMMAND_LIST_TYPE_DIRECT,textureUploadBatch.context.allocator.Get(),nullptr,IID_PPV_ARGS(&textureUploadBatch.context.list)),"Texture upload list failed");++stats.materialTextureUploadContextCreations;++stats.residentMaterialTextureUploadContexts;stats.peakResidentMaterialTextureUploadContexts=(std::max)(stats.peakResidentMaterialTextureUploadContexts,stats.residentMaterialTextureUploadContexts);}textureUploadBatch.staging.reserve(8);}
 bool SubmitTextureUploadBatch(){if(!textureUploadBatch.context.list||!textureUploadBatch.textures)return false;if(pendingTextureUploads.size()==pendingTextureUploads.capacity())pendingTextureUploads.reserve((std::max)(size_t(8),pendingTextureUploads.capacity()*2));Check(textureUploadBatch.context.list->Close(),"Texture upload close failed");ID3D12CommandList* lists[]={textureUploadBatch.context.list.Get()};queue->ExecuteCommandLists(1,lists);const UINT64 value=nextFenceValue++;Check(queue->Signal(fence.Get(),value),"Texture upload signal failed");PendingTextureUpload3D pending{};pending.context=std::move(textureUploadBatch.context);pending.staging=std::move(textureUploadBatch.staging);pending.fenceValue=value;pending.bytes=textureUploadBatch.bytes;pending.textures=textureUploadBatch.textures;textureUploadBatch={};pendingTextureUploads.push_back(std::move(pending));++stats.materialTextureUploadBatches;return true;}
 void PrepareTextureUpload(size_t bytes){CollectCompletedTextureUploads();if(textureUploadBatch.textures&&(textureUploadBatch.textures>=D3D12Renderer::MaxMaterialTextureUploadBatchTextures||textureUploadBatch.bytes>=D3D12Renderer::MaxMaterialTextureUploadBatchBytes||bytes>D3D12Renderer::MaxMaterialTextureUploadBatchBytes-textureUploadBatch.bytes))SubmitTextureUploadBatch();while(!pendingTextureUploads.empty()&&(bytes>D3D12Renderer::MaxMaterialTextureUploadStagingBytes||stats.materialTextureUploadStagingBytes>D3D12Renderer::MaxMaterialTextureUploadStagingBytes-bytes)){const uint64_t elapsed=WaitForValue(pendingTextureUploads.front().fenceValue);if(elapsed){++stats.materialTextureUploadPressureWaits;stats.materialTextureUploadPressureWaitNanoseconds+=elapsed;}CollectCompletedTextureUploads();}BeginTextureUploadBatch();if(textureUploadBatch.staging.size()==textureUploadBatch.staging.capacity())textureUploadBatch.staging.reserve((std::max)(size_t(8),textureUploadBatch.staging.capacity()*2));}
 void PrepareGpuRetirement(){if(retiredGpuResources.size()==retiredGpuResources.capacity())retiredGpuResources.reserve((std::max)(size_t(32),retiredGpuResources.capacity()*2));}
 void RetireGpuResources(RetiredGpuResources3D resources){resources.fenceValue=nextFenceValue>1?nextFenceValue-1:0;stats.pendingGpuReleaseBytes+=resources.bytes;stats.peakPendingGpuReleaseBytes=(std::max)(stats.peakPendingGpuReleaseBytes,stats.pendingGpuReleaseBytes);if(resources.textures)pendingTextureReleaseBytes+=resources.bytes;else pendingMeshReleaseBytes+=resources.bytes;++stats.deferredGpuReleaseBatches;retiredGpuResources.push_back(std::move(resources));}
 void WaitForFrame(unsigned index){const uint64_t elapsed=WaitForValue(frameFenceValues[index]);stats.lastResourceWaitNanoseconds=elapsed;if(elapsed){++stats.resourceWaits;stats.totalResourceWaitNanoseconds+=elapsed;stats.peakResourceWaitNanoseconds=(std::max)(stats.peakResourceWaitNanoseconds,elapsed);}CollectCompletedGpuWork();}
 void CollectGpuTiming(unsigned index){if(!timestampValid[index]||!timestampMapped||!timestampFrequency)return;const uint64_t begin=timestampMapped[index*2],end=timestampMapped[index*2+1];if(end>=begin){const uint64_t elapsed=(std::max)(uint64_t(1),uint64_t(double(end-begin)*1000000000.0/double(timestampFrequency)));stats.lastGpuFrameNanoseconds=elapsed;stats.totalGpuFrameNanoseconds+=elapsed;stats.peakGpuFrameNanoseconds=(std::max)(stats.peakGpuFrameNanoseconds,elapsed);++stats.gpuTimingSamples;}timestampValid[index]=false;}
 UINT64 SignalFrame(unsigned index){const UINT64 value=nextFenceValue++;Check(queue->Signal(fence.Get(),value),"Signal failed");frameFenceValues[index]=value;return value;}
 void Flush(){SubmitMeshUploadBatch();SubmitTextureUploadBatch();const UINT64 value=nextFenceValue++;Check(queue->Signal(fence.Get(),value),"Signal failed");WaitForValue(value);CollectCompletedGpuWork();}
 void Targets(){D3D12_CPU_DESCRIPTOR_HANDLE unorm=rtvHeap->GetCPUDescriptorHandleForHeapStart(),srgb=unorm;srgb.ptr+=SIZE_T(Frames)*rtvStep;D3D12_RENDER_TARGET_VIEW_DESC view{};view.ViewDimension=D3D12_RTV_DIMENSION_TEXTURE2D;for(unsigned i=0;i<Frames;i++){Check(swap->GetBuffer(i,IID_PPV_ARGS(&targets[i])),"Back buffer failed");view.Format=DXGI_FORMAT_R8G8B8A8_UNORM;device->CreateRenderTargetView(targets[i].Get(),&view,unorm);view.Format=DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;device->CreateRenderTargetView(targets[i].Get(),&view,srgb);unorm.ptr+=rtvStep;srgb.ptr+=rtvStep;}}
 void ReleaseDepth(){if(!depth)return;if(ambientOcclusionRaw)ReleaseAmbientOcclusion();depth.Reset();stats.nativeDepthBufferBytes=0;++stats.nativeDepthBufferReleases;}
 void CreateDepth(){ReleaseDepth();D3D12_HEAP_PROPERTIES heap{};heap.Type=D3D12_HEAP_TYPE_DEFAULT;D3D12_RESOURCE_DESC desc{};desc.Dimension=D3D12_RESOURCE_DIMENSION_TEXTURE2D;desc.Width=width;desc.Height=height;desc.DepthOrArraySize=1;desc.MipLevels=1;desc.Format=DXGI_FORMAT_R32_TYPELESS;desc.SampleDesc.Count=1;desc.Flags=D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;D3D12_CLEAR_VALUE clear{};clear.Format=DXGI_FORMAT_D32_FLOAT;clear.DepthStencil.Depth=1.f;Check(device->CreateCommittedResource(&heap,D3D12_HEAP_FLAG_NONE,&desc,D3D12_RESOURCE_STATE_DEPTH_WRITE,&clear,IID_PPV_ARGS(&depth)),"Depth buffer failed");D3D12_DEPTH_STENCIL_VIEW_DESC dsv{};dsv.Format=DXGI_FORMAT_D32_FLOAT;dsv.ViewDimension=D3D12_DSV_DIMENSION_TEXTURE2D;D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle=dsvHeap->GetCPUDescriptorHandleForHeapStart();device->CreateDepthStencilView(depth.Get(),&dsv,dsvHandle);dsv.Flags=D3D12_DSV_FLAG_READ_ONLY_DEPTH;dsvHandle.ptr+=device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);device->CreateDepthStencilView(depth.Get(),&dsv,dsvHandle);D3D12_SHADER_RESOURCE_VIEW_DESC srv{};srv.Format=DXGI_FORMAT_R32_FLOAT;srv.ViewDimension=D3D12_SRV_DIMENSION_TEXTURE2D;srv.Shader4ComponentMapping=D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;srv.Texture2D.MipLevels=1;D3D12_CPU_DESCRIPTOR_HANDLE handle=textureHeap->GetCPUDescriptorHandleForHeapStart();handle.ptr+=SIZE_T(AmbientOcclusionDepthSrvDescriptor3D)*textureStep;device->CreateShaderResourceView(depth.Get(),&srv,handle);stats.nativeDepthBufferBytes=size_t(width)*height*sizeof(float);++stats.nativeDepthBufferAllocations;}
 void ReleaseAmbientOcclusion(){if(!ambientOcclusionRaw&&!ambientOcclusionBlur)return;ambientOcclusionRaw.Reset();ambientOcclusionBlur.Reset();stats.ambientOcclusionBytes=0;stats.ambientOcclusionWidth=stats.ambientOcclusionHeight=0;++stats.ambientOcclusionReleases;}
 void CreateAmbientOcclusion(){ReleaseAmbientOcclusion();const unsigned aoWidth=(width+1)/2,aoHeight=(height+1)/2;D3D12_HEAP_PROPERTIES heap{};heap.Type=D3D12_HEAP_TYPE_DEFAULT;D3D12_RESOURCE_DESC desc{};desc.Dimension=D3D12_RESOURCE_DIMENSION_TEXTURE2D;desc.Width=aoWidth;desc.Height=aoHeight;desc.DepthOrArraySize=1;desc.MipLevels=1;desc.Format=DXGI_FORMAT_R8_UNORM;desc.SampleDesc.Count=1;desc.Flags=D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;D3D12_CLEAR_VALUE clear{};clear.Format=desc.Format;clear.Color[0]=clear.Color[1]=clear.Color[2]=clear.Color[3]=1.f;Check(device->CreateCommittedResource(&heap,D3D12_HEAP_FLAG_NONE,&desc,D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,&clear,IID_PPV_ARGS(&ambientOcclusionRaw)),"Ambient occlusion raw target failed");Check(device->CreateCommittedResource(&heap,D3D12_HEAP_FLAG_NONE,&desc,D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,&clear,IID_PPV_ARGS(&ambientOcclusionBlur)),"Ambient occlusion blur target failed");D3D12_SHADER_RESOURCE_VIEW_DESC srv{};srv.Format=desc.Format;srv.ViewDimension=D3D12_SRV_DIMENSION_TEXTURE2D;srv.Shader4ComponentMapping=D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;srv.Texture2D.MipLevels=1;D3D12_CPU_DESCRIPTOR_HANDLE srvHandle=textureHeap->GetCPUDescriptorHandleForHeapStart();srvHandle.ptr+=SIZE_T(AmbientOcclusionRawSrvDescriptor3D)*textureStep;device->CreateShaderResourceView(ambientOcclusionRaw.Get(),&srv,srvHandle);srvHandle.ptr+=textureStep;device->CreateShaderResourceView(ambientOcclusionBlur.Get(),&srv,srvHandle);D3D12_CPU_DESCRIPTOR_HANDLE rtv=rtvHeap->GetCPUDescriptorHandleForHeapStart();rtv.ptr+=SIZE_T(Frames*2+1)*rtvStep;device->CreateRenderTargetView(ambientOcclusionRaw.Get(),nullptr,rtv);rtv.ptr+=rtvStep;device->CreateRenderTargetView(ambientOcclusionBlur.Get(),nullptr,rtv);stats.ambientOcclusionWidth=aoWidth;stats.ambientOcclusionHeight=aoHeight;stats.ambientOcclusionBytes=AmbientOcclusionResidentBytes3D(width,height);++stats.ambientOcclusionAllocations;}
 void ReleaseHdrSceneColor(){if(!hdrSceneColor)return;hdrSceneColor.Reset();stats.hdrSceneColorBytes=0;++stats.hdrSceneColorReleases;}
 void CreateHdrSceneColor(){ReleaseHdrSceneColor();D3D12_HEAP_PROPERTIES heap{};heap.Type=D3D12_HEAP_TYPE_DEFAULT;D3D12_RESOURCE_DESC desc{};desc.Dimension=D3D12_RESOURCE_DIMENSION_TEXTURE2D;desc.Width=width;desc.Height=height;desc.DepthOrArraySize=1;desc.MipLevels=1;desc.Format=DXGI_FORMAT_R16G16B16A16_FLOAT;desc.SampleDesc.Count=1;desc.Flags=D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;D3D12_CLEAR_VALUE clear{};clear.Format=desc.Format;clear.Color[3]=1.f;Check(device->CreateCommittedResource(&heap,D3D12_HEAP_FLAG_NONE,&desc,D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,&clear,IID_PPV_ARGS(&hdrSceneColor)),"HDR scene color failed");D3D12_CPU_DESCRIPTOR_HANDLE rtv=rtvHeap->GetCPUDescriptorHandleForHeapStart();rtv.ptr+=SIZE_T(Frames*2)*rtvStep;device->CreateRenderTargetView(hdrSceneColor.Get(),nullptr,rtv);D3D12_SHADER_RESOURCE_VIEW_DESC srv{};srv.Format=desc.Format;srv.ViewDimension=D3D12_SRV_DIMENSION_TEXTURE2D;srv.Shader4ComponentMapping=D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;srv.Texture2D.MipLevels=1;D3D12_CPU_DESCRIPTOR_HANDLE handle=textureHeap->GetCPUDescriptorHandleForHeapStart();handle.ptr+=SIZE_T(HdrSceneSrvDescriptor3D)*textureStep;device->CreateShaderResourceView(hdrSceneColor.Get(),&srv,handle);stats.hdrSceneColorBytes=size_t(width)*height*8;++stats.hdrSceneColorAllocations;}
 void UpdateDlssSceneDescriptors(){D3D12_SHADER_RESOURCE_VIEW_DESC srv{};srv.Format=DXGI_FORMAT_R16G16B16A16_FLOAT;srv.ViewDimension=D3D12_SRV_DIMENSION_TEXTURE2D;srv.Shader4ComponentMapping=D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;srv.Texture2D.MipLevels=1;D3D12_CPU_DESCRIPTOR_HANDLE handle=textureHeap->GetCPUDescriptorHandleForHeapStart();handle.ptr+=SIZE_T(DlssOutputSrvDescriptor3D)*textureStep;device->CreateShaderResourceView(dlss.OutputResource(),&srv,handle);handle.ptr+=textureStep;device->CreateShaderResourceView(dlss.InputResource(),&srv,handle);}
 void WriteNullShadowSrv(size_t descriptor){if(!device||!textureHeap)return;D3D12_SHADER_RESOURCE_VIEW_DESC srv{};srv.Format=DXGI_FORMAT_R32_FLOAT;srv.ViewDimension=D3D12_SRV_DIMENSION_TEXTURE2DARRAY;srv.Shader4ComponentMapping=D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;srv.Texture2DArray.MipLevels=1;srv.Texture2DArray.ArraySize=2;D3D12_CPU_DESCRIPTOR_HANDLE handle=textureHeap->GetCPUDescriptorHandleForHeapStart();handle.ptr+=SIZE_T(descriptor)*textureStep;device->CreateShaderResourceView(nullptr,&srv,handle);}
 void ReleaseShadowDepth(){if(!shadowDepth)return;activeShadowSrvDescriptor=ShadowNullSrvDescriptor3D;shadowDepth.Reset();WriteNullShadowSrv(ShadowLiveSrvDescriptor3D);stats.directionalShadowMapBytes=0;++stats.directionalShadowMapReleases;++stats.directionalShadowDescriptorSwitches;}
 void CreateShadowDepth(){constexpr UINT ShadowSize=2048;if(shadowDepth)throw std::logic_error("Shadow depth already exists");D3D12_HEAP_PROPERTIES heap{};heap.Type=D3D12_HEAP_TYPE_DEFAULT;D3D12_RESOURCE_DESC desc{};desc.Dimension=D3D12_RESOURCE_DIMENSION_TEXTURE2D;desc.Width=ShadowSize;desc.Height=ShadowSize;desc.DepthOrArraySize=2;desc.MipLevels=1;desc.Format=DXGI_FORMAT_R32_TYPELESS;desc.SampleDesc.Count=1;desc.Flags=D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;D3D12_CLEAR_VALUE clear{};clear.Format=DXGI_FORMAT_D32_FLOAT;clear.DepthStencil.Depth=1.f;Check(device->CreateCommittedResource(&heap,D3D12_HEAP_FLAG_NONE,&desc,D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,&clear,IID_PPV_ARGS(&shadowDepth)),"Shadow depth buffer failed");D3D12_DEPTH_STENCIL_VIEW_DESC dsv{};dsv.Format=DXGI_FORMAT_D32_FLOAT;dsv.ViewDimension=D3D12_DSV_DIMENSION_TEXTURE2DARRAY;dsv.Texture2DArray.ArraySize=1;D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle=shadowDsvHeap->GetCPUDescriptorHandleForHeapStart();const UINT dsvStep=device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);for(UINT cascade=0;cascade<2;++cascade){dsv.Texture2DArray.FirstArraySlice=cascade;device->CreateDepthStencilView(shadowDepth.Get(),&dsv,dsvHandle);dsvHandle.ptr+=dsvStep;}D3D12_SHADER_RESOURCE_VIEW_DESC srv{};srv.Format=DXGI_FORMAT_R32_FLOAT;srv.ViewDimension=D3D12_SRV_DIMENSION_TEXTURE2DARRAY;srv.Shader4ComponentMapping=D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;srv.Texture2DArray.MipLevels=1;srv.Texture2DArray.ArraySize=2;D3D12_CPU_DESCRIPTOR_HANDLE handle=textureHeap->GetCPUDescriptorHandleForHeapStart();handle.ptr+=SIZE_T(ShadowLiveSrvDescriptor3D)*textureStep;device->CreateShaderResourceView(shadowDepth.Get(),&srv,handle);activeShadowSrvDescriptor=ShadowLiveSrvDescriptor3D;stats.directionalShadowMapBytes=ShadowSize*ShadowSize*sizeof(float)*2;++stats.directionalShadowMapAllocations;++stats.directionalShadowDescriptorSwitches;}
};

D3D12Renderer::D3D12Renderer():impl_(std::make_unique<Impl>()){} D3D12Renderer::~D3D12Renderer(){if(impl_&&impl_->device){impl_->Flush();for(unsigned index=0;index<Impl::Frames;++index)impl_->CollectGpuTiming(index);for(auto&buffer:impl_->vertexBuffers)if(buffer)buffer->Unmap(0,nullptr);for(auto&buffer:impl_->worldVertexBuffers)if(buffer)buffer->Unmap(0,nullptr);for(auto&buffer:impl_->worldIndexBuffers)if(buffer)buffer->Unmap(0,nullptr);for(auto&buffer:impl_->instanceBuffers)if(buffer)buffer->Unmap(0,nullptr);for(auto&buffer:impl_->skinnedInstanceBuffers)if(buffer)buffer->Unmap(0,nullptr);for(auto&buffer:impl_->skinPaletteBuffers)if(buffer)buffer->Unmap(0,nullptr);if(impl_->timestampReadback)impl_->timestampReadback->Unmap(0,nullptr);if(impl_->sceneConstants)impl_->sceneConstants->Unmap(0,nullptr);if(impl_->shadowConstants)impl_->shadowConstants->Unmap(0,nullptr);impl_->dlss.Shutdown();}if(impl_&&impl_->fenceEvent)CloseHandle(impl_->fenceEvent);}
size_t D3D12Renderer::DrawableWorldIndexCount(const std::vector<uint32_t>& indices,size_t vertexCount){const size_t limit=(std::min)(indices.size(),MaxWorldIndices);size_t count=0;while(count+2<limit&&indices[count]<vertexCount&&indices[count+1]<vertexCount&&indices[count+2]<vertexCount)count+=3;return count;}
void D3D12Renderer::Initialize(HWND hwnd,unsigned width,unsigned height,unsigned logicalWidth,unsigned logicalHeight,bool enableDLSS){auto& d=*impl_;d.width=(std::max)(1u,width);d.height=(std::max)(1u,height);d.logicalWidth=(std::max)(1u,logicalWidth);d.logicalHeight=(std::max)(1u,logicalHeight);width=d.width;height=d.height;d.stats.outputWidth=width;d.stats.outputHeight=height;d.stats.linearWorldBlending=true;d.stats.linearHdrSceneComposition=true;d.stats.worldSrgbRenderTargetViews=Impl::Frames;d.stats.meshMemoryBudgetBytes=d.meshMemoryBudgetBytes;d.stats.materialTextureMemoryBudgetBytes=d.textureMemoryBudgetBytes;d.stats.canvasViewport=CalculateCanvasViewport({float(d.logicalWidth),float(d.logicalHeight)},{float(width),float(height)});d.dlssRequested=enableDLSS;
 if(d.dlssRequested)d.dlss.InitializeFramework();
#ifdef _DEBUG
 ComPtr<ID3D12Debug> debug;if(SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debug))))debug->EnableDebugLayer();
#endif
 UINT flags=0;Check(d.dlss.CreateFactory(flags,IID_PPV_ARGS(&d.factory)),"DXGI factory failed");
 ComPtr<IDXGIAdapter1> adapter;for(UINT i=0;d.factory->EnumAdapterByGpuPreference(i,DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,IID_PPV_ARGS(&adapter))!=DXGI_ERROR_NOT_FOUND;i++){DXGI_ADAPTER_DESC1 desc{};adapter->GetDesc1(&desc);if(!(desc.Flags&DXGI_ADAPTER_FLAG_SOFTWARE)&&SUCCEEDED(d.dlss.CreateDevice(adapter.Get(),D3D_FEATURE_LEVEL_12_0,IID_PPV_ARGS(&d.device))))break;adapter.Reset();}
 if(!d.device&&SUCCEEDED(d.factory->EnumWarpAdapter(IID_PPV_ARGS(&adapter))))d.dlss.CreateDevice(adapter.Get(),D3D_FEATURE_LEVEL_12_0,IID_PPV_ARGS(&d.device));if(!d.device)Check(d.dlss.CreateDevice(nullptr,D3D_FEATURE_LEVEL_12_0,IID_PPV_ARGS(&d.device)),"DirectX 12 device unavailable");if(d.dlssRequested)d.dlss.InitializeDevice(d.device.Get(),adapter.Get(),width,height);
 D3D12_COMMAND_QUEUE_DESC q{};q.Type=D3D12_COMMAND_LIST_TYPE_DIRECT;Check(d.device->CreateCommandQueue(&q,IID_PPV_ARGS(&d.queue)),"Queue failed");Check(d.queue->GetTimestampFrequency(&d.timestampFrequency),"Timestamp frequency failed");
 DXGI_SWAP_CHAIN_DESC1 sc{};sc.Width=width;sc.Height=height;sc.Format=DXGI_FORMAT_R8G8B8A8_UNORM;sc.SampleDesc.Count=1;sc.BufferUsage=DXGI_USAGE_RENDER_TARGET_OUTPUT;sc.BufferCount=Impl::Frames;sc.SwapEffect=DXGI_SWAP_EFFECT_FLIP_DISCARD;
 ComPtr<IDXGISwapChain1> swap1;Check(d.factory->CreateSwapChainForHwnd(d.queue.Get(),hwnd,&sc,nullptr,nullptr,&swap1),"Swap chain failed");Check(swap1.As(&d.swap),"Swap cast failed");d.frame=d.swap->GetCurrentBackBufferIndex();
 D3D12_DESCRIPTOR_HEAP_DESC hd{};hd.NumDescriptors=Impl::Frames*2+3;hd.Type=D3D12_DESCRIPTOR_HEAP_TYPE_RTV;Check(d.device->CreateDescriptorHeap(&hd,IID_PPV_ARGS(&d.rtvHeap)),"RTV heap failed");d.rtvStep=d.device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);d.Targets();
 D3D12_DESCRIPTOR_HEAP_DESC depthHeap{};depthHeap.NumDescriptors=2;depthHeap.Type=D3D12_DESCRIPTOR_HEAP_TYPE_DSV;Check(d.device->CreateDescriptorHeap(&depthHeap,IID_PPV_ARGS(&d.dsvHeap)),"DSV heap failed");D3D12_DESCRIPTOR_HEAP_DESC shadowDepthHeap{};shadowDepthHeap.NumDescriptors=2;shadowDepthHeap.Type=D3D12_DESCRIPTOR_HEAP_TYPE_DSV;Check(d.device->CreateDescriptorHeap(&shadowDepthHeap,IID_PPV_ARGS(&d.shadowDsvHeap)),"Shadow DSV heap failed");
 D3D12_DESCRIPTOR_HEAP_DESC textureHeap{};textureHeap.NumDescriptors=UINT(TotalTextureDescriptorCount3D);textureHeap.Type=D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;textureHeap.Flags=D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;Check(d.device->CreateDescriptorHeap(&textureHeap,IID_PPV_ARGS(&d.textureHeap)),"Texture descriptor heap failed");d.textureStep=d.device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);D3D12_SHADER_RESOURCE_VIEW_DESC nullSrv{};nullSrv.Format=DXGI_FORMAT_R8G8B8A8_UNORM;nullSrv.ViewDimension=D3D12_SRV_DIMENSION_TEXTURE2D;nullSrv.Shader4ComponentMapping=D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;nullSrv.Texture2D.MipLevels=1;D3D12_CPU_DESCRIPTOR_HANDLE nullHandle=d.textureHeap->GetCPUDescriptorHandleForHeapStart();for(unsigned descriptor=0;descriptor<9;++descriptor){d.device->CreateShaderResourceView(nullptr,&nullSrv,nullHandle);nullHandle.ptr+=d.textureStep;}d.WriteNullShadowSrv(ShadowNullSrvDescriptor3D);d.WriteNullShadowSrv(ShadowLiveSrvDescriptor3D);D3D12_SHADER_RESOURCE_VIEW_DESC nullScalarSrv=nullSrv;nullScalarSrv.Format=DXGI_FORMAT_R32_FLOAT;nullHandle=d.textureHeap->GetCPUDescriptorHandleForHeapStart();nullHandle.ptr+=SIZE_T(AmbientOcclusionNullSrvDescriptor3D)*d.textureStep;for(unsigned descriptor=0;descriptor<5;++descriptor){d.device->CreateShaderResourceView(nullptr,&nullScalarSrv,nullHandle);nullHandle.ptr+=d.textureStep;}D3D12_HEAP_PROPERTIES constantHeap{};constantHeap.Type=D3D12_HEAP_TYPE_UPLOAD;D3D12_RESOURCE_DESC constantDesc{};constantDesc.Dimension=D3D12_RESOURCE_DIMENSION_BUFFER;constantDesc.Width=Impl::Frames*SceneConstantStride3D;constantDesc.Height=1;constantDesc.DepthOrArraySize=1;constantDesc.MipLevels=1;constantDesc.SampleDesc.Count=1;constantDesc.Layout=D3D12_TEXTURE_LAYOUT_ROW_MAJOR;Check(d.device->CreateCommittedResource(&constantHeap,D3D12_HEAP_FLAG_NONE,&constantDesc,D3D12_RESOURCE_STATE_GENERIC_READ,nullptr,IID_PPV_ARGS(&d.sceneConstants)),"Scene constants failed");Check(d.sceneConstants->Map(0,nullptr,reinterpret_cast<void**>(&d.sceneConstantsMapped)),"Scene constants map failed");constantDesc.Width=Impl::Frames*256;Check(d.device->CreateCommittedResource(&constantHeap,D3D12_HEAP_FLAG_NONE,&constantDesc,D3D12_RESOURCE_STATE_GENERIC_READ,nullptr,IID_PPV_ARGS(&d.shadowConstants)),"Shadow constants failed");Check(d.shadowConstants->Map(0,nullptr,reinterpret_cast<void**>(&d.shadowConstantsMapped)),"Shadow constants map failed");for(unsigned frameIndex=0;frameIndex<Impl::Frames;++frameIndex){D3D12_CONSTANT_BUFFER_VIEW_DESC cbv{};cbv.SizeInBytes=UINT(SceneConstantStride3D);D3D12_CPU_DESCRIPTOR_HANDLE cbvHandle=d.textureHeap->GetCPUDescriptorHandleForHeapStart();cbv.BufferLocation=d.sceneConstants->GetGPUVirtualAddress()+frameIndex*SceneConstantStride3D;cbvHandle.ptr+=SIZE_T(SceneCbvDescriptorBase3D+frameIndex)*d.textureStep;d.device->CreateConstantBufferView(&cbv,cbvHandle);cbv.SizeInBytes=256;cbv.BufferLocation=d.shadowConstants->GetGPUVirtualAddress()+frameIndex*256;cbvHandle=d.textureHeap->GetCPUDescriptorHandleForHeapStart();cbvHandle.ptr+=SIZE_T(ShadowCbvDescriptorBase3D+frameIndex)*d.textureStep;d.device->CreateConstantBufferView(&cbv,cbvHandle);}d.stats.sceneConstantBufferBytes=Impl::Frames*SceneConstantStride3D;d.stats.rootSignatureDwords=10;
 for(auto& a:d.alloc)Check(d.device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,IID_PPV_ARGS(&a)),"Allocator failed");Check(d.device->CreateCommandList(0,D3D12_COMMAND_LIST_TYPE_DIRECT,d.alloc[0].Get(),nullptr,IID_PPV_ARGS(&d.list)),"Command list failed");d.list->Close();
 D3D12_ROOT_PARAMETER rootParameters[5]{};D3D12_DESCRIPTOR_RANGE sceneConstantRange{};sceneConstantRange.RangeType=D3D12_DESCRIPTOR_RANGE_TYPE_CBV;sceneConstantRange.NumDescriptors=1;sceneConstantRange.BaseShaderRegister=0;rootParameters[0].ParameterType=D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;rootParameters[0].DescriptorTable={1,&sceneConstantRange};rootParameters[0].ShaderVisibility=D3D12_SHADER_VISIBILITY_ALL;D3D12_DESCRIPTOR_RANGE textureRanges[2]{};textureRanges[0].RangeType=D3D12_DESCRIPTOR_RANGE_TYPE_SRV;textureRanges[0].NumDescriptors=3;textureRanges[0].BaseShaderRegister=0;textureRanges[0].OffsetInDescriptorsFromTableStart=D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;textureRanges[1].RangeType=D3D12_DESCRIPTOR_RANGE_TYPE_SRV;textureRanges[1].NumDescriptors=6;textureRanges[1].BaseShaderRegister=4;textureRanges[1].OffsetInDescriptorsFromTableStart=D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;rootParameters[1].ParameterType=D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;rootParameters[1].DescriptorTable={2,textureRanges};rootParameters[1].ShaderVisibility=D3D12_SHADER_VISIBILITY_PIXEL;D3D12_DESCRIPTOR_RANGE shadowRange{};shadowRange.RangeType=D3D12_DESCRIPTOR_RANGE_TYPE_SRV;shadowRange.NumDescriptors=1;shadowRange.BaseShaderRegister=3;rootParameters[2].ParameterType=D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;rootParameters[2].DescriptorTable={1,&shadowRange};rootParameters[2].ShaderVisibility=D3D12_SHADER_VISIBILITY_PIXEL;D3D12_DESCRIPTOR_RANGE shadowConstantRange{};shadowConstantRange.RangeType=D3D12_DESCRIPTOR_RANGE_TYPE_CBV;shadowConstantRange.NumDescriptors=1;shadowConstantRange.BaseShaderRegister=1;rootParameters[3].ParameterType=D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;rootParameters[3].DescriptorTable={1,&shadowConstantRange};rootParameters[3].ShaderVisibility=D3D12_SHADER_VISIBILITY_ALL;rootParameters[4].ParameterType=D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;rootParameters[4].Constants={2,0,2};rootParameters[4].ShaderVisibility=D3D12_SHADER_VISIBILITY_ALL;D3D12_STATIC_SAMPLER_DESC samplers[2]{};auto& sampler=samplers[0];sampler.Filter=D3D12_FILTER_ANISOTROPIC;sampler.AddressU=sampler.AddressV=sampler.AddressW=D3D12_TEXTURE_ADDRESS_MODE_WRAP;sampler.MaxAnisotropy=8;sampler.ComparisonFunc=D3D12_COMPARISON_FUNC_ALWAYS;sampler.MaxLOD=D3D12_FLOAT32_MAX;sampler.ShaderVisibility=D3D12_SHADER_VISIBILITY_PIXEL;auto& shadowSampler=samplers[1];shadowSampler.Filter=D3D12_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT;shadowSampler.AddressU=shadowSampler.AddressV=shadowSampler.AddressW=D3D12_TEXTURE_ADDRESS_MODE_BORDER;shadowSampler.BorderColor=D3D12_STATIC_BORDER_COLOR_OPAQUE_WHITE;shadowSampler.ComparisonFunc=D3D12_COMPARISON_FUNC_LESS_EQUAL;shadowSampler.ShaderRegister=1;shadowSampler.ShaderVisibility=D3D12_SHADER_VISIBILITY_PIXEL;
 D3D12_ROOT_SIGNATURE_DESC rs{};rs.NumParameters=5;rs.pParameters=rootParameters;rs.NumStaticSamplers=2;rs.pStaticSamplers=samplers;rs.Flags=D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;ComPtr<ID3DBlob> sig;Check(D3D12SerializeRootSignature(&rs,D3D_ROOT_SIGNATURE_VERSION_1,&sig,nullptr),"Root signature serialize failed");Check(d.device->CreateRootSignature(0,sig->GetBufferPointer(),sig->GetBufferSize(),IID_PPV_ARGS(&d.root)),"Root signature failed");
 D3D12_DESCRIPTOR_RANGE postRange{};postRange.RangeType=D3D12_DESCRIPTOR_RANGE_TYPE_SRV;postRange.NumDescriptors=1;postRange.BaseShaderRegister=10;D3D12_ROOT_PARAMETER postParameters[2]{};postParameters[0].ParameterType=D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;postParameters[0].DescriptorTable={1,&postRange};postParameters[0].ShaderVisibility=D3D12_SHADER_VISIBILITY_PIXEL;postParameters[1].ParameterType=D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;postParameters[1].Constants={0,0,8};postParameters[1].ShaderVisibility=D3D12_SHADER_VISIBILITY_PIXEL;D3D12_STATIC_SAMPLER_DESC postSampler{};postSampler.Filter=D3D12_FILTER_MIN_MAG_LINEAR_MIP_POINT;postSampler.AddressU=postSampler.AddressV=postSampler.AddressW=D3D12_TEXTURE_ADDRESS_MODE_CLAMP;postSampler.MaxLOD=D3D12_FLOAT32_MAX;postSampler.ShaderVisibility=D3D12_SHADER_VISIBILITY_PIXEL;D3D12_ROOT_SIGNATURE_DESC postSignature{};postSignature.NumParameters=2;postSignature.pParameters=postParameters;postSignature.NumStaticSamplers=1;postSignature.pStaticSamplers=&postSampler;postSignature.Flags=D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;ComPtr<ID3DBlob> postBlob;Check(D3D12SerializeRootSignature(&postSignature,D3D_ROOT_SIGNATURE_VERSION_1,&postBlob,nullptr),"Post-process root signature serialize failed");Check(d.device->CreateRootSignature(0,postBlob->GetBufferPointer(),postBlob->GetBufferSize(),IID_PPV_ARGS(&d.postProcessRoot)),"Post-process root signature failed");
 CreateEnhancedRootSignatures3D(d.device.Get(),d.root,d.ambientOcclusionRoot);
 D3D12_INPUT_ELEMENT_DESC input[]={{"POSITION",0,DXGI_FORMAT_R32G32_FLOAT,0,0,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0},{"COLOR",0,DXGI_FORMAT_R8G8B8A8_UNORM,0,8,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0}};
 D3D12_GRAPHICS_PIPELINE_STATE_DESC p{};p.InputLayout={input,2};p.pRootSignature=d.root.Get();p.VS={CanvasVertexShader,sizeof(CanvasVertexShader)};p.PS={CanvasPixelShader,sizeof(CanvasPixelShader)};p.RasterizerState={D3D12_FILL_MODE_SOLID,D3D12_CULL_MODE_NONE,FALSE,0,0.f,0.f,TRUE,FALSE,FALSE,0,D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF};p.BlendState.AlphaToCoverageEnable=FALSE;p.BlendState.IndependentBlendEnable=FALSE;auto& rt=p.BlendState.RenderTarget[0];rt.BlendEnable=TRUE;rt.SrcBlend=D3D12_BLEND_SRC_ALPHA;rt.DestBlend=D3D12_BLEND_INV_SRC_ALPHA;rt.BlendOp=D3D12_BLEND_OP_ADD;rt.SrcBlendAlpha=D3D12_BLEND_ONE;rt.DestBlendAlpha=D3D12_BLEND_ZERO;rt.BlendOpAlpha=D3D12_BLEND_OP_ADD;rt.RenderTargetWriteMask=D3D12_COLOR_WRITE_ENABLE_ALL;p.SampleMask=UINT_MAX;p.PrimitiveTopologyType=D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;p.NumRenderTargets=1;p.RTVFormats[0]=DXGI_FORMAT_R8G8B8A8_UNORM;p.SampleDesc.Count=1;Check(d.device->CreateGraphicsPipelineState(&p,IID_PPV_ARGS(&d.pipeline)),"Pipeline failed");auto additive=p;additive.BlendState.RenderTarget[0].DestBlend=D3D12_BLEND_ONE;additive.BlendState.RenderTarget[0].DestBlendAlpha=D3D12_BLEND_ONE;Check(d.device->CreateGraphicsPipelineState(&additive,IID_PPV_ARGS(&d.additivePipeline)),"Additive pipeline failed");
 auto postProcess=p;postProcess.InputLayout={};postProcess.pRootSignature=d.postProcessRoot.Get();postProcess.VS={PostProcessVertexShader,sizeof(PostProcessVertexShader)};postProcess.PS={PostProcessPixelShader,sizeof(PostProcessPixelShader)};postProcess.BlendState.RenderTarget[0].BlendEnable=FALSE;postProcess.DepthStencilState.DepthEnable=FALSE;Check(d.device->CreateGraphicsPipelineState(&postProcess,IID_PPV_ARGS(&d.postProcessPipeline)),"Post-process pipeline failed");
 D3D12_INPUT_ELEMENT_DESC worldInput[]={{"POSITION",0,DXGI_FORMAT_R32G32B32_FLOAT,0,0,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0},{"NORMAL",0,DXGI_FORMAT_R8G8B8A8_SNORM,0,12,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0},{"COLOR",0,DXGI_FORMAT_R8G8B8A8_UNORM,0,16,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0},{"TEXCOORD",0,DXGI_FORMAT_R32G32_FLOAT,0,20,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0}};auto world=p;world.InputLayout={worldInput,4};world.VS={WorldVertexShader,sizeof(WorldVertexShader)};world.PS={WorldPixelShader,sizeof(WorldPixelShader)};world.RTVFormats[0]=DXGI_FORMAT_R16G16B16A16_FLOAT;world.DepthStencilState.DepthEnable=TRUE;world.DepthStencilState.DepthWriteMask=D3D12_DEPTH_WRITE_MASK_ALL;world.DepthStencilState.DepthFunc=D3D12_COMPARISON_FUNC_LESS;world.DepthStencilState.StencilEnable=FALSE;world.DSVFormat=DXGI_FORMAT_D32_FLOAT;Check(d.device->CreateGraphicsPipelineState(&world,IID_PPV_ARGS(&d.worldPipeline)),"World pipeline failed");
 D3D12_INPUT_ELEMENT_DESC instancedInput[]={{"POSITION",0,DXGI_FORMAT_R32G32B32_FLOAT,0,0,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0},{"NORMAL",0,DXGI_FORMAT_R8G8B8A8_SNORM,0,12,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0},{"COLOR",0,DXGI_FORMAT_R8G8B8A8_UNORM,0,16,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0},{"TEXCOORD",0,DXGI_FORMAT_R32G32_FLOAT,0,20,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0},{"INSTANCE",0,DXGI_FORMAT_R32G32B32A32_FLOAT,1,0,D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA,1},{"INSTANCE",1,DXGI_FORMAT_R32G32B32A32_FLOAT,1,16,D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA,1},{"INSTANCE",2,DXGI_FORMAT_R32G32B32A32_FLOAT,1,32,D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA,1},{"INSTANCE",3,DXGI_FORMAT_R32G32B32A32_FLOAT,1,48,D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA,1},{"NROW",0,DXGI_FORMAT_R32G32B32A32_FLOAT,1,64,D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA,1},{"NROW",1,DXGI_FORMAT_R32G32B32A32_FLOAT,1,80,D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA,1},{"NROW",2,DXGI_FORMAT_R32G32B32A32_FLOAT,1,96,D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA,1},{"TINT",0,DXGI_FORMAT_R8G8B8A8_UNORM,1,112,D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA,1},{"MATERIAL",0,DXGI_FORMAT_R8G8B8A8_UNORM,1,116,D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA,1},{"TEXTURE",0,DXGI_FORMAT_R32_UINT,1,120,D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA,1}};auto instanced=world;instanced.InputLayout={instancedInput,14};instanced.VS={InstancedVertexShader,sizeof(InstancedVertexShader)};Check(d.device->CreateGraphicsPipelineState(&instanced,IID_PPV_ARGS(&d.instancedPipeline)),"Instanced pipeline failed");auto instancedCulled=instanced;instancedCulled.RasterizerState.CullMode=D3D12_CULL_MODE_BACK;Check(d.device->CreateGraphicsPipelineState(&instancedCulled,IID_PPV_ARGS(&d.instancedCulledPipeline)),"Culled instanced pipeline failed");auto instancedTransparent=instanced;auto& transparentBlend=instancedTransparent.BlendState.RenderTarget[0];transparentBlend.BlendEnable=TRUE;transparentBlend.SrcBlend=D3D12_BLEND_SRC_ALPHA;transparentBlend.DestBlend=D3D12_BLEND_INV_SRC_ALPHA;transparentBlend.BlendOp=D3D12_BLEND_OP_ADD;transparentBlend.SrcBlendAlpha=D3D12_BLEND_ONE;transparentBlend.DestBlendAlpha=D3D12_BLEND_INV_SRC_ALPHA;transparentBlend.BlendOpAlpha=D3D12_BLEND_OP_ADD;instancedTransparent.DepthStencilState.DepthWriteMask=D3D12_DEPTH_WRITE_MASK_ZERO;Check(d.device->CreateGraphicsPipelineState(&instancedTransparent,IID_PPV_ARGS(&d.instancedTransparentPipeline)),"Transparent instanced pipeline failed");auto instancedOverlay=instanced;instancedOverlay.DepthStencilState.DepthWriteMask=D3D12_DEPTH_WRITE_MASK_ZERO;instancedOverlay.DepthStencilState.DepthFunc=D3D12_COMPARISON_FUNC_LESS_EQUAL;Check(d.device->CreateGraphicsPipelineState(&instancedOverlay,IID_PPV_ARGS(&d.instancedOverlayPipeline)),"Overlay instanced pipeline failed");auto shadowWorld=world;shadowWorld.VS={ShadowWorldVertexShader,sizeof(ShadowWorldVertexShader)};shadowWorld.PS={};shadowWorld.NumRenderTargets=0;shadowWorld.RTVFormats[0]=DXGI_FORMAT_UNKNOWN;shadowWorld.RasterizerState.DepthBias=1200;shadowWorld.RasterizerState.SlopeScaledDepthBias=1.5f;Check(d.device->CreateGraphicsPipelineState(&shadowWorld,IID_PPV_ARGS(&d.shadowWorldPipeline)),"Shadow world pipeline failed");auto shadowInstanced=shadowWorld;shadowInstanced.InputLayout={instancedInput,14};shadowInstanced.VS={ShadowInstancedVertexShader,sizeof(ShadowInstancedVertexShader)};Check(d.device->CreateGraphicsPipelineState(&shadowInstanced,IID_PPV_ARGS(&d.shadowInstancedPipeline)),"Shadow instanced pipeline failed");
 D3D12_INPUT_ELEMENT_DESC skinnedInput[]={{"POSITION",0,DXGI_FORMAT_R32G32B32_FLOAT,0,0,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0},{"NORMAL",0,DXGI_FORMAT_R8G8B8A8_SNORM,0,12,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0},{"COLOR",0,DXGI_FORMAT_R8G8B8A8_UNORM,0,16,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0},{"TEXCOORD",0,DXGI_FORMAT_R32G32_FLOAT,0,20,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0},{"TANGENT",0,DXGI_FORMAT_R8G8B8A8_SNORM,0,28,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0},{"BLENDINDICES",0,DXGI_FORMAT_R16G16B16A16_UINT,0,32,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0},{"BLENDWEIGHT",0,DXGI_FORMAT_R16G16B16A16_UNORM,0,40,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0},{"INSTANCE",0,DXGI_FORMAT_R32G32B32A32_FLOAT,1,0,D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA,1},{"INSTANCE",1,DXGI_FORMAT_R32G32B32A32_FLOAT,1,16,D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA,1},{"INSTANCE",2,DXGI_FORMAT_R32G32B32A32_FLOAT,1,32,D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA,1},{"INSTANCE",3,DXGI_FORMAT_R32G32B32A32_FLOAT,1,48,D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA,1},{"NROW",0,DXGI_FORMAT_R32G32B32A32_FLOAT,1,64,D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA,1},{"NROW",1,DXGI_FORMAT_R32G32B32A32_FLOAT,1,80,D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA,1},{"NROW",2,DXGI_FORMAT_R32G32B32A32_FLOAT,1,96,D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA,1},{"TINT",0,DXGI_FORMAT_R8G8B8A8_UNORM,1,112,D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA,1},{"MATERIAL",0,DXGI_FORMAT_R8G8B8A8_UNORM,1,116,D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA,1},{"TEXTURE",0,DXGI_FORMAT_R32_UINT,1,120,D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA,1},{"SKINBASE",0,DXGI_FORMAT_R32_UINT,1,124,D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA,1}};
 auto skinned=instanced;skinned.InputLayout={skinnedInput,UINT(std::size(skinnedInput))};skinned.VS={SkinnedVertexShader,sizeof(SkinnedVertexShader)};skinned.DepthStencilState.DepthFunc=D3D12_COMPARISON_FUNC_LESS_EQUAL;Check(d.device->CreateGraphicsPipelineState(&skinned,IID_PPV_ARGS(&d.skinnedPipeline)),"Skinned pipeline failed");auto skinnedCulled=skinned;skinnedCulled.RasterizerState.CullMode=D3D12_CULL_MODE_BACK;Check(d.device->CreateGraphicsPipelineState(&skinnedCulled,IID_PPV_ARGS(&d.skinnedCulledPipeline)),"Culled skinned pipeline failed");auto shadowSkinned=shadowWorld;shadowSkinned.InputLayout={skinnedInput,UINT(std::size(skinnedInput))};shadowSkinned.VS={ShadowSkinnedVertexShader,sizeof(ShadowSkinnedVertexShader)};Check(d.device->CreateGraphicsPipelineState(&shadowSkinned,IID_PPV_ARGS(&d.shadowSkinnedPipeline)),"Shadow skinned pipeline failed");
 auto aoWorldColor=world;aoWorldColor.DepthStencilState.DepthWriteMask=D3D12_DEPTH_WRITE_MASK_ZERO;aoWorldColor.DepthStencilState.DepthFunc=D3D12_COMPARISON_FUNC_EQUAL;Check(d.device->CreateGraphicsPipelineState(&aoWorldColor,IID_PPV_ARGS(&d.ambientOcclusionWorldColorPipeline)),"AO world color pipeline failed");auto aoInstancedColor=instanced;aoInstancedColor.DepthStencilState=aoWorldColor.DepthStencilState;Check(d.device->CreateGraphicsPipelineState(&aoInstancedColor,IID_PPV_ARGS(&d.ambientOcclusionInstancedColorPipeline)),"AO instanced color pipeline failed");auto aoInstancedCulledColor=aoInstancedColor;aoInstancedCulledColor.RasterizerState.CullMode=D3D12_CULL_MODE_BACK;Check(d.device->CreateGraphicsPipelineState(&aoInstancedCulledColor,IID_PPV_ARGS(&d.ambientOcclusionInstancedCulledColorPipeline)),"AO culled instanced color pipeline failed");
 auto aoDepthWorld=world;aoDepthWorld.pRootSignature=d.ambientOcclusionRoot.Get();aoDepthWorld.VS={AmbientOcclusionDepthWorldVertexShader,sizeof(AmbientOcclusionDepthWorldVertexShader)};aoDepthWorld.PS={AmbientOcclusionDepthPixelShader,sizeof(AmbientOcclusionDepthPixelShader)};aoDepthWorld.NumRenderTargets=0;aoDepthWorld.RTVFormats[0]=DXGI_FORMAT_UNKNOWN;Check(d.device->CreateGraphicsPipelineState(&aoDepthWorld,IID_PPV_ARGS(&d.ambientOcclusionDepthWorldPipeline)),"AO depth world pipeline failed");auto aoDepthInstanced=aoDepthWorld;aoDepthInstanced.InputLayout={instancedInput,14};aoDepthInstanced.VS={AmbientOcclusionDepthInstancedVertexShader,sizeof(AmbientOcclusionDepthInstancedVertexShader)};Check(d.device->CreateGraphicsPipelineState(&aoDepthInstanced,IID_PPV_ARGS(&d.ambientOcclusionDepthInstancedPipeline)),"AO depth instanced pipeline failed");auto aoDepthInstancedCulled=aoDepthInstanced;aoDepthInstancedCulled.RasterizerState.CullMode=D3D12_CULL_MODE_BACK;Check(d.device->CreateGraphicsPipelineState(&aoDepthInstancedCulled,IID_PPV_ARGS(&d.ambientOcclusionDepthInstancedCulledPipeline)),"AO culled depth pipeline failed");
 auto aoDepthSkinned=aoDepthWorld;aoDepthSkinned.InputLayout={skinnedInput,UINT(std::size(skinnedInput))};aoDepthSkinned.VS={AmbientOcclusionDepthSkinnedVertexShader,sizeof(AmbientOcclusionDepthSkinnedVertexShader)};Check(d.device->CreateGraphicsPipelineState(&aoDepthSkinned,IID_PPV_ARGS(&d.ambientOcclusionDepthSkinnedPipeline)),"AO depth skinned pipeline failed");
 auto aoPass=postProcess;aoPass.pRootSignature=d.ambientOcclusionRoot.Get();aoPass.VS={AmbientOcclusionVertexShader,sizeof(AmbientOcclusionVertexShader)};aoPass.PS={AmbientOcclusionPixelShader,sizeof(AmbientOcclusionPixelShader)};aoPass.RTVFormats[0]=DXGI_FORMAT_R8_UNORM;Check(d.device->CreateGraphicsPipelineState(&aoPass,IID_PPV_ARGS(&d.ambientOcclusionPipeline)),"Ambient occlusion pipeline failed");aoPass.PS={AmbientOcclusionBilateralBlurPixelShader,sizeof(AmbientOcclusionBilateralBlurPixelShader)};Check(d.device->CreateGraphicsPipelineState(&aoPass,IID_PPV_ARGS(&d.ambientOcclusionBlurPipeline)),"Ambient occlusion blur pipeline failed");
 D3D12_RESOURCE_DESC rd{};rd.Dimension=D3D12_RESOURCE_DIMENSION_BUFFER;rd.Height=1;rd.DepthOrArraySize=1;rd.MipLevels=1;rd.SampleDesc.Count=1;rd.Layout=D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
 D3D12_QUERY_HEAP_DESC queryDesc{};queryDesc.Type=D3D12_QUERY_HEAP_TYPE_TIMESTAMP;queryDesc.Count=Impl::Frames*2;Check(d.device->CreateQueryHeap(&queryDesc,IID_PPV_ARGS(&d.timestampHeap)),"Timestamp query heap failed");D3D12_HEAP_PROPERTIES readbackHeap{};readbackHeap.Type=D3D12_HEAP_TYPE_READBACK;rd.Width=sizeof(uint64_t)*Impl::Frames*2;Check(d.device->CreateCommittedResource(&readbackHeap,D3D12_HEAP_FLAG_NONE,&rd,D3D12_RESOURCE_STATE_COPY_DEST,nullptr,IID_PPV_ARGS(&d.timestampReadback)),"Timestamp readback failed");Check(d.timestampReadback->Map(0,nullptr,reinterpret_cast<void**>(&d.timestampMapped)),"Timestamp map failed");
 Check(d.device->CreateFence(0,D3D12_FENCE_FLAG_NONE,IID_PPV_ARGS(&d.fence)),"Fence failed");d.fenceEvent=CreateEvent(nullptr,FALSE,FALSE,nullptr);d.pendingMeshUploads.reserve(8);d.availableMeshUploadContexts.reserve(8);d.pendingTextureUploads.reserve(8);d.availableTextureUploadContexts.reserve(8);
}
// 靜態幾何只在建立時經 upload heap 搬入 DEFAULT heap，之後每幀不再重傳頂點與索引。
MeshHandle3D D3D12Renderer::CreateMesh(std::span<const Vertex3D> vertices,std::span<const uint32_t> indices){
 auto& d=*impl_;if(!d.device||vertices.empty()||indices.empty()||indices.size()%3!=0||std::any_of(indices.begin(),indices.end(),[&](uint32_t index){return index>=vertices.size();}))return{};
 ResidentMeshBounds3D bounds{};if(!CalculateResidentMeshBounds3D(vertices,bounds)){++d.stats.invalidMeshBoundsRejections;return{};}
 const size_t vertexBytes=vertices.size_bytes(),indexBytes=indices.size_bytes(),indexOffset=(vertexBytes+alignof(uint32_t)-1)&~size_t(alignof(uint32_t)-1),meshBytes=indexOffset+indexBytes;if(vertexBytes>UINT_MAX||indexBytes>UINT_MAX)return{};d.CollectCompletedGpuWork();const MeshCacheKey3D cacheKey=CreateMeshCacheKey(vertices,indices);if(const auto cached=d.meshCache.find(cacheKey);cached!=d.meshCache.end()){const auto resident=d.residentMeshes.find(cached->second);if(resident!=d.residentMeshes.end()){++resident->second.references;++d.stats.residentMeshReferences;++d.stats.meshCacheHits;return{cached->second};}d.meshCache.erase(cached);}if(meshBytes>d.meshMemoryBudgetBytes||d.stats.residentMeshBytes+d.pendingMeshReleaseBytes>d.meshMemoryBudgetBytes-meshBytes){++d.stats.meshBudgetRejections;return{};}d.residentMeshes.reserve(d.residentMeshes.size()+1);d.meshCache.reserve(d.meshCache.size()+1);
 D3D12_HEAP_PROPERTIES defaultHeap{},uploadHeap{};defaultHeap.Type=D3D12_HEAP_TYPE_DEFAULT;uploadHeap.Type=D3D12_HEAP_TYPE_UPLOAD;auto bufferDesc=[](size_t bytes){D3D12_RESOURCE_DESC desc{};desc.Dimension=D3D12_RESOURCE_DIMENSION_BUFFER;desc.Width=bytes;desc.Height=1;desc.DepthOrArraySize=1;desc.MipLevels=1;desc.SampleDesc.Count=1;desc.Layout=D3D12_TEXTURE_LAYOUT_ROW_MAJOR;return desc;};
 ResidentMesh3D mesh{};mesh.cacheKey=cacheKey;mesh.bounds=bounds;ComPtr<ID3D12Resource> upload;D3D12_RESOURCE_DESC meshDesc=bufferDesc(meshBytes);Check(d.device->CreateCommittedResource(&defaultHeap,D3D12_HEAP_FLAG_NONE,&meshDesc,D3D12_RESOURCE_STATE_COPY_DEST,nullptr,IID_PPV_ARGS(&mesh.buffer)),"Resident mesh buffer failed");Check(d.device->CreateCommittedResource(&uploadHeap,D3D12_HEAP_FLAG_NONE,&meshDesc,D3D12_RESOURCE_STATE_GENERIC_READ,nullptr,IID_PPV_ARGS(&upload)),"Resident mesh upload failed");
 void* mappedData{};Check(upload->Map(0,nullptr,&mappedData),"Resident mesh map failed");memcpy(mappedData,vertices.data(),vertexBytes);memcpy(static_cast<uint8_t*>(mappedData)+indexOffset,indices.data(),indexBytes);upload->Unmap(0,nullptr);const D3D12_GPU_VIRTUAL_ADDRESS meshAddress=mesh.buffer->GetGPUVirtualAddress();mesh.vertexView={meshAddress,UINT(vertexBytes),sizeof(Vertex3D)};mesh.indexView={meshAddress+indexOffset,UINT(indexBytes),DXGI_FORMAT_R32_UINT};mesh.indexCount=UINT(indices.size());mesh.bytes=meshBytes;
 // 先完成所有可能配置記憶體的容器操作，再把資源位址錄進 GPU command list，避免配置失敗留下懸空 copy 指令。
 d.PrepareMeshUpload(meshBytes);d.meshUploadBatch.staging.reserve(d.meshUploadBatch.staging.size()+1);const MeshHandle3D handle{d.nextMeshHandle};auto residentInsertion=d.residentMeshes.emplace(handle.value,std::move(mesh));if(!residentInsertion.second)return{};try{if(!d.meshCache.emplace(cacheKey,handle.value).second){d.residentMeshes.erase(residentInsertion.first);return{};}}catch(...){d.residentMeshes.erase(residentInsertion.first);throw;}++d.nextMeshHandle;ResidentMesh3D& resident=residentInsertion.first->second;auto* uploadList=d.meshUploadBatch.context.list.Get();uploadList->CopyBufferRegion(resident.buffer.Get(),0,upload.Get(),0,vertexBytes);uploadList->CopyBufferRegion(resident.buffer.Get(),indexOffset,upload.Get(),indexOffset,indexBytes);D3D12_RESOURCE_BARRIER barrier{};barrier.Type=D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;barrier.Transition={resident.buffer.Get(),D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,D3D12_RESOURCE_STATE_COPY_DEST,D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER|D3D12_RESOURCE_STATE_INDEX_BUFFER};uploadList->ResourceBarrier(1,&barrier);d.meshUploadBatch.staging.push_back(std::move(upload));d.meshUploadBatch.bytes+=meshBytes;++d.meshUploadBatch.meshes;d.stats.meshUploadStagingBytes+=meshBytes;d.stats.peakMeshUploadStagingBytes=(std::max)(d.stats.peakMeshUploadStagingBytes,d.stats.meshUploadStagingBytes);++d.stats.meshUploadStagingResources;d.stats.peakMeshUploadStagingResources=(std::max)(d.stats.peakMeshUploadStagingResources,d.stats.meshUploadStagingResources);++d.stats.meshUploadBufferAllocations;++d.stats.meshUploads;
 d.stats.residentMeshes=d.residentMeshes.size();++d.stats.residentMeshReferences;d.stats.residentMeshBytes+=meshBytes;++d.stats.residentMeshResources;d.stats.peakResidentMeshResources=(std::max)(d.stats.peakResidentMeshResources,d.stats.residentMeshResources);++d.stats.meshResourceAllocations;if(d.meshUploadBatch.meshes>=D3D12Renderer::MaxMeshUploadBatchMeshes||d.meshUploadBatch.bytes>=D3D12Renderer::MaxMeshUploadBatchBytes)d.SubmitMeshUploadBatch();return handle;
}

void D3D12Renderer::DestroyMesh(MeshHandle3D handle){if(!impl_||!handle)return;auto& d=*impl_;auto found=d.residentMeshes.find(handle.value);if(found==d.residentMeshes.end())return;if(found->second.references>1){--found->second.references;--d.stats.residentMeshReferences;return;}d.SubmitMeshUploadBatch();d.PrepareGpuRetirement();RetiredGpuResources3D retired{};retired.resources[0]=std::move(found->second.buffer);retired.bytes=found->second.bytes;d.stats.residentMeshBytes-=retired.bytes;--d.stats.residentMeshResources;--d.stats.residentMeshReferences;const auto cached=d.meshCache.find(found->second.cacheKey);if(cached!=d.meshCache.end()&&cached->second==handle.value)d.meshCache.erase(cached);d.residentMeshes.erase(found);for(auto* groups:{&d.instanceGroups,&d.overlayInstanceGroups,&d.shadowOnlyInstanceGroups})for(auto iterator=groups->begin();iterator!=groups->end();)if(iterator->first.mesh==handle.value)iterator=groups->erase(iterator);else++iterator;d.RefreshInstanceGroupStats();d.stats.residentMeshes=d.residentMeshes.size();d.RetireGpuResources(std::move(retired));}

SkinnedMeshHandle3D D3D12Renderer::CreateSkinnedMesh(std::span<const SkinnedVertex3D> vertices,std::span<const uint32_t> indices,uint32_t jointCount){if(!impl_||!impl_->device||vertices.empty()||indices.empty()||indices.size()%3||!jointCount||jointCount>256||std::any_of(indices.begin(),indices.end(),[&](uint32_t index){return index>=vertices.size();}))return{};for(const SkinnedVertex3D& vertex:vertices){uint32_t sum{};for(size_t influence=0;influence<4;++influence){if(vertex.joints[influence]>=jointCount)return{};sum+=vertex.weights[influence];}if(sum!=65535)return{};}auto& d=*impl_;const size_t vertexBytes=vertices.size_bytes(),indexBytes=indices.size_bytes(),indexOffset=(vertexBytes+3)&~size_t(3),bytes=indexOffset+indexBytes;if(vertexBytes>UINT_MAX||indexBytes>UINT_MAX||bytes>d.meshMemoryBudgetBytes||d.stats.residentMeshBytes+d.pendingMeshReleaseBytes>d.meshMemoryBudgetBytes-bytes)return{};const MeshCacheKey3D key=CreateSkinnedMeshCacheKey(vertices,indices,jointCount);if(const auto cached=d.skinnedMeshCache.find(key);cached!=d.skinnedMeshCache.end()){auto found=d.residentSkinnedMeshes.find(cached->second);if(found!=d.residentSkinnedMeshes.end()){++found->second.references;++d.stats.residentSkinnedMeshReferences;++d.stats.skinnedMeshCacheHits;return{cached->second};}d.skinnedMeshCache.erase(cached);}D3D12_HEAP_PROPERTIES defaultHeap{},uploadHeap{};defaultHeap.Type=D3D12_HEAP_TYPE_DEFAULT;uploadHeap.Type=D3D12_HEAP_TYPE_UPLOAD;D3D12_RESOURCE_DESC desc{};desc.Dimension=D3D12_RESOURCE_DIMENSION_BUFFER;desc.Width=bytes;desc.Height=1;desc.DepthOrArraySize=1;desc.MipLevels=1;desc.SampleDesc.Count=1;desc.Layout=D3D12_TEXTURE_LAYOUT_ROW_MAJOR;ResidentSkinnedMesh3D mesh{};mesh.cacheKey=key;mesh.indexCount=UINT(indices.size());mesh.jointCount=jointCount;mesh.bytes=bytes;Check(d.device->CreateCommittedResource(&defaultHeap,D3D12_HEAP_FLAG_NONE,&desc,D3D12_RESOURCE_STATE_COPY_DEST,nullptr,IID_PPV_ARGS(&mesh.buffer)),"Skinned mesh buffer failed");ComPtr<ID3D12Resource> upload;Check(d.device->CreateCommittedResource(&uploadHeap,D3D12_HEAP_FLAG_NONE,&desc,D3D12_RESOURCE_STATE_GENERIC_READ,nullptr,IID_PPV_ARGS(&upload)),"Skinned mesh upload failed");void* mapped{};Check(upload->Map(0,nullptr,&mapped),"Skinned mesh upload map failed");memcpy(mapped,vertices.data(),vertexBytes);memcpy(static_cast<uint8_t*>(mapped)+indexOffset,indices.data(),indexBytes);upload->Unmap(0,nullptr);mesh.vertexView={mesh.buffer->GetGPUVirtualAddress(),UINT(vertexBytes),sizeof(SkinnedVertex3D)};mesh.indexView={mesh.buffer->GetGPUVirtualAddress()+indexOffset,UINT(indexBytes),DXGI_FORMAT_R32_UINT};d.PrepareMeshUpload(bytes);const SkinnedMeshHandle3D handle{d.nextSkinnedMeshHandle++};auto [found,inserted]=d.residentSkinnedMeshes.emplace(handle.value,std::move(mesh));if(!inserted)return{};try{d.skinnedMeshCache.emplace(key,handle.value);d.meshUploadBatch.staging.push_back(upload);}catch(...){d.residentSkinnedMeshes.erase(found);throw;}d.meshUploadBatch.context.list->CopyBufferRegion(found->second.buffer.Get(),0,upload.Get(),0,bytes);D3D12_RESOURCE_BARRIER barrier{};barrier.Type=D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;barrier.Transition={found->second.buffer.Get(),D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,D3D12_RESOURCE_STATE_COPY_DEST,D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER|D3D12_RESOURCE_STATE_INDEX_BUFFER};d.meshUploadBatch.context.list->ResourceBarrier(1,&barrier);d.meshUploadBatch.bytes+=bytes;++d.meshUploadBatch.meshes;d.stats.meshUploadStagingBytes+=bytes;++d.stats.meshUploadStagingResources;d.stats.residentMeshBytes+=bytes;++d.stats.residentMeshResources;++d.stats.residentSkinnedMeshes;++d.stats.residentSkinnedMeshReferences;d.stats.residentSkinnedMeshBytes+=bytes;++d.stats.skinnedMeshUploads;return handle;}

void D3D12Renderer::DestroySkinnedMesh(SkinnedMeshHandle3D handle){if(!impl_||!handle)return;auto& d=*impl_;auto found=d.residentSkinnedMeshes.find(handle.value);if(found==d.residentSkinnedMeshes.end())return;if(found->second.references>1){--found->second.references;--d.stats.residentSkinnedMeshReferences;return;}d.SubmitMeshUploadBatch();d.PrepareGpuRetirement();RetiredGpuResources3D retired{};retired.resources[0]=std::move(found->second.buffer);retired.bytes=found->second.bytes;const auto cached=d.skinnedMeshCache.find(found->second.cacheKey);if(cached!=d.skinnedMeshCache.end()&&cached->second==handle.value)d.skinnedMeshCache.erase(cached);d.stats.residentMeshBytes-=retired.bytes;--d.stats.residentMeshResources;d.stats.residentSkinnedMeshBytes-=retired.bytes;--d.stats.residentSkinnedMeshes;--d.stats.residentSkinnedMeshReferences;++d.stats.skinnedMeshReleases;d.residentSkinnedMeshes.erase(found);d.RetireGpuResources(std::move(retired));}

void D3D12Renderer::SetMeshMemoryBudget(size_t bytes){if(!impl_)return;auto& d=*impl_;d.meshMemoryBudgetBytes=(std::max)(size_t(1),bytes);d.stats.meshMemoryBudgetBytes=d.meshMemoryBudgetBytes;}

MaterialTextureSetHandle3D D3D12Renderer::CreateMaterialTextureSet(const MaterialTextureSetDefinition3D& definition){
 auto& d=*impl_;if(!d.device||definition.baseColorPath.empty()||definition.roughnessPath.empty()||definition.normalPath.empty())return{};d.CollectCompletedGpuWork();const unsigned maximumResolution=(std::min)(4096u,(std::max)(1u,definition.maximumResolution));std::wstring cacheKey=MaterialTextureCacheKey(definition,maximumResolution);if(const auto cached=d.materialTextureCache.find(cacheKey);cached!=d.materialTextureCache.end()){const auto resident=d.residentMaterialTextureSets.find(cached->second);if(resident!=d.residentMaterialTextureSets.end()){++resident->second.references;++d.stats.residentMaterialTextureReferences;++d.stats.materialTextureCacheHits;return{cached->second};}d.materialTextureCache.erase(cached);}if(d.freeTextureSlots.empty()&&d.nextTextureSlot>=MaxMaterialTextureSets)return{};
 const bool anyAdditional=std::ranges::any_of(definition.additionalLayers,[](const auto& layer){return!layer.baseColorPath.empty()||!layer.roughnessPath.empty()||!layer.normalPath.empty();}),completeAdditional=std::ranges::all_of(definition.additionalLayers,[](const auto& layer){return!layer.baseColorPath.empty()&&!layer.roughnessPath.empty()&&!layer.normalPath.empty();});if(anyAdditional&&!completeAdditional)return{};const size_t textureCount=anyAdditional?9:3;const std::array<std::filesystem::path,9> paths={definition.baseColorPath,definition.roughnessPath,definition.normalPath,definition.additionalLayers[0].baseColorPath,definition.additionalLayers[0].roughnessPath,definition.additionalLayers[0].normalPath,definition.additionalLayers[1].baseColorPath,definition.additionalLayers[1].roughnessPath,definition.additionalLayers[1].normalPath};const std::array<ImageMipMode3D,9> mipModes={ImageMipMode3D::Srgb,ImageMipMode3D::Linear,ImageMipMode3D::Normal,ImageMipMode3D::Srgb,ImageMipMode3D::Linear,ImageMipMode3D::Normal,ImageMipMode3D::Srgb,ImageMipMode3D::Linear,ImageMipMode3D::Normal};const std::array<DXGI_FORMAT,9> formats={DXGI_FORMAT_BC1_UNORM_SRGB,DXGI_FORMAT_BC4_UNORM,DXGI_FORMAT_BC5_UNORM,DXGI_FORMAT_BC1_UNORM_SRGB,DXGI_FORMAT_BC4_UNORM,DXGI_FORMAT_BC5_UNORM,DXGI_FORMAT_BC1_UNORM_SRGB,DXGI_FORMAT_BC4_UNORM,DXGI_FORMAT_BC5_UNORM};
 std::array<ImageExtent3D,9> extents{};std::array<D3D12_RESOURCE_DESC,9> textureDescs{};std::array<UINT64,9> textureHeapOffsets{};size_t requestedBytes{};UINT64 textureHeapBytes{};for(size_t index=0;index<textureCount;++index){extents[index]=ProbeImageRgba8(paths[index],maximumResolution);if(!extents[index])return{};requestedBytes+=CompressedImageMipChainBytes(extents[index],index%3==2?16:8);D3D12_RESOURCE_DESC& textureDesc=textureDescs[index];textureDesc.Dimension=D3D12_RESOURCE_DIMENSION_TEXTURE2D;textureDesc.Width=extents[index].width;textureDesc.Height=extents[index].height;textureDesc.DepthOrArraySize=1;textureDesc.MipLevels=UINT16(ImageMipCount3D(extents[index]));textureDesc.Format=formats[index];textureDesc.SampleDesc.Count=1;const D3D12_RESOURCE_ALLOCATION_INFO allocation=d.device->GetResourceAllocationInfo(0,1,&textureDesc);if(allocation.SizeInBytes==UINT64_MAX||!allocation.Alignment)return{};textureHeapBytes=(textureHeapBytes+allocation.Alignment-1)&~(allocation.Alignment-1);textureHeapOffsets[index]=textureHeapBytes;if(allocation.SizeInBytes>UINT64_MAX-textureHeapBytes)return{};textureHeapBytes+=allocation.SizeInBytes;}
 if(textureHeapBytes>(std::numeric_limits<size_t>::max)())return{};const size_t allocationBytes=size_t(textureHeapBytes);if(allocationBytes>d.textureMemoryBudgetBytes||d.stats.residentTextureAllocationBytes+d.pendingTextureReleaseBytes>d.textureMemoryBudgetBytes-allocationBytes){++d.stats.materialTextureBudgetRejections;d.stats.materialTexturePreflightRejectedBytes+=requestedBytes;return{};}d.residentMaterialTextureSets.reserve(d.residentMaterialTextureSets.size()+1);d.materialTextureCache.reserve(d.materialTextureCache.size()+1);
 uint32_t textureSlot{};if(!d.freeTextureSlots.empty()){textureSlot=d.freeTextureSlots.back();d.freeTextureSlots.pop_back();}else textureSlot=d.nextTextureSlot++;ResidentMaterialTextureSet3D resident{};resident.cacheKey=std::move(cacheKey);resident.descriptorOffset=textureSlot*9;D3D12_HEAP_PROPERTIES defaultHeap{},uploadHeap{};defaultHeap.Type=D3D12_HEAP_TYPE_DEFAULT;uploadHeap.Type=D3D12_HEAP_TYPE_UPLOAD;D3D12_CPU_DESCRIPTOR_HANDLE descriptor=d.textureHeap->GetCPUDescriptorHandleForHeapStart();descriptor.ptr+=SIZE_T(resident.descriptorOffset)*d.textureStep;
 D3D12_HEAP_DESC textureHeapDesc{};textureHeapDesc.SizeInBytes=allocationBytes;textureHeapDesc.Properties=defaultHeap;textureHeapDesc.Flags=D3D12_HEAP_FLAG_ALLOW_ONLY_NON_RT_DS_TEXTURES;Check(d.device->CreateHeap(&textureHeapDesc,IID_PPV_ARGS(&resident.heap)),"Material texture heap allocation failed");resident.allocationBytes=allocationBytes;
 auto abandon=[&](){d.SubmitTextureUploadBatch();const bool hasResources=std::ranges::any_of(resident.textures,[](const auto& texture){return texture.Get()!=nullptr;});if(!hasResources){d.freeTextureSlots.push_back(textureSlot);return;}d.PrepareGpuRetirement();RetiredGpuResources3D retired{};retired.resources=std::move(resident.textures);retired.heap=std::move(resident.heap);retired.bytes=resident.allocationBytes;retired.textureSlot=textureSlot;retired.textures=true;d.RetireGpuResources(std::move(retired));};
 if(!d.materialDerivedDataCacheSurveyed)d.TrimMaterialDerivedDataCache();
 std::array<UINT64,9> materialUploadOffsets{},materialUploadBytes{};UINT64 materialSetUploadBytes{};for(size_t index=0;index<textureCount;++index){D3D12_RESOURCE_DESC textureDesc{};textureDesc.Dimension=D3D12_RESOURCE_DIMENSION_TEXTURE2D;textureDesc.Width=extents[index].width;textureDesc.Height=extents[index].height;textureDesc.DepthOrArraySize=1;textureDesc.MipLevels=UINT16(ImageMipCount3D(extents[index]));textureDesc.Format=formats[index];textureDesc.SampleDesc.Count=1;d.device->GetCopyableFootprints(&textureDesc,0,textureDesc.MipLevels,0,nullptr,nullptr,nullptr,&materialUploadBytes[index]);materialSetUploadBytes=(materialSetUploadBytes+UINT64(D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT-1))&~UINT64(D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT-1);materialUploadOffsets[index]=materialSetUploadBytes;materialSetUploadBytes+=materialUploadBytes[index];} if(materialSetUploadBytes>D3D12Renderer::MaxMaterialTextureUploadStagingBytes){abandon();return{};}
 ComPtr<ID3D12Resource> materialUpload;bool materialUploadRetained{};try{for(std::size_t index=0;index<textureCount;++index){CompressedImage3D compressed{};size_t decodedBytes{},cacheBytes{};const uint64_t derivedKey=MaterialCacheKey3D(paths[index],maximumResolution,mipModes[index]);const MaterialCacheLoadResult3D cacheResult=LoadMaterialCache3D(d.materialDerivedDataCacheDirectory,derivedKey,extents[index],mipModes[index],compressed,cacheBytes);if(cacheResult==MaterialCacheLoadResult3D::Hit){++d.stats.materialDerivedDataCacheHits;d.stats.materialDerivedDataCacheReadBytes+=cacheBytes;TouchMaterialCache3D(d.materialDerivedDataCacheDirectory,derivedKey);}else{if(!d.materialDerivedDataCacheDirectory.empty()){if(cacheResult==MaterialCacheLoadResult3D::Invalid)++d.stats.materialDerivedDataCacheInvalidations;else ++d.stats.materialDerivedDataCacheMisses;}DecodedImage3D image=DecodeImageRgba8(paths[index],maximumResolution);if(!image.Valid()){abandon();return{};}GenerateImageMips(image,mipModes[index]);decodedBytes=image.Bytes();d.stats.totalMaterialTextureDecodedBytes+=decodedBytes;d.stats.peakMaterialTextureCpuStagingBytes=(std::max)(d.stats.peakMaterialTextureCpuStagingBytes,decodedBytes);compressed=CompressImage(image,mipModes[index]);if(!compressed.Valid()){abandon();return{};}const size_t replacedCacheBytes=MaterialCacheFileBytes3D(d.materialDerivedDataCacheDirectory,derivedKey);size_t storedBytes{};if(d.materialDerivedDataCacheBudgetBytes&&StoreMaterialCache3D(d.materialDerivedDataCacheDirectory,derivedKey,mipModes[index],compressed,storedBytes)){++d.stats.materialDerivedDataCacheWrites;d.stats.materialDerivedDataCacheWrittenBytes+=storedBytes;const size_t storedFileBytes=MaterialCacheFileBytes3D(d.materialDerivedDataCacheDirectory,derivedKey);d.stats.materialDerivedDataCacheResidentBytes=d.stats.materialDerivedDataCacheResidentBytes>=replacedCacheBytes?d.stats.materialDerivedDataCacheResidentBytes-replacedCacheBytes:0;d.stats.materialDerivedDataCacheResidentBytes+=storedFileBytes;d.stats.peakMaterialDerivedDataCacheResidentBytes=(std::max)(d.stats.peakMaterialDerivedDataCacheResidentBytes,d.stats.materialDerivedDataCacheResidentBytes);if(d.stats.materialDerivedDataCacheResidentBytes>d.materialDerivedDataCacheBudgetBytes)d.TrimMaterialDerivedDataCache();}}const size_t compressedBytes=compressed.Bytes();d.stats.totalMaterialTextureCompressedBytes+=compressedBytes;d.stats.peakMaterialTextureCompressionStagingBytes=(std::max)(d.stats.peakMaterialTextureCompressionStagingBytes,decodedBytes+compressedBytes);resident.bytes+=compressedBytes;const UINT mipCount=UINT(compressed.mips.size());const D3D12_RESOURCE_DESC& textureDesc=textureDescs[index];if(mipCount!=textureDesc.MipLevels)throw std::runtime_error("Material texture mip count mismatch");Check(d.device->CreatePlacedResource(resident.heap.Get(),textureHeapOffsets[index],&textureDesc,D3D12_RESOURCE_STATE_COPY_DEST,nullptr,IID_PPV_ARGS(&resident.textures[index])),"Material texture placement failed");std::vector<D3D12_PLACED_SUBRESOURCE_FOOTPRINT> footprints(mipCount);std::vector<UINT> rowCounts(mipCount);std::vector<UINT64> rowBytes(mipCount);d.device->GetCopyableFootprints(&textureDesc,0,mipCount,materialUploadOffsets[index],footprints.data(),rowCounts.data(),rowBytes.data(),nullptr);if(!materialUpload){d.PrepareTextureUpload(size_t(materialSetUploadBytes));D3D12_RESOURCE_DESC uploadDesc{};uploadDesc.Dimension=D3D12_RESOURCE_DIMENSION_BUFFER;uploadDesc.Width=materialSetUploadBytes;uploadDesc.Height=1;uploadDesc.DepthOrArraySize=1;uploadDesc.MipLevels=1;uploadDesc.SampleDesc.Count=1;uploadDesc.Layout=D3D12_TEXTURE_LAYOUT_ROW_MAJOR;Check(d.device->CreateCommittedResource(&uploadHeap,D3D12_HEAP_FLAG_NONE,&uploadDesc,D3D12_RESOURCE_STATE_GENERIC_READ,nullptr,IID_PPV_ARGS(&materialUpload)),"Material texture upload allocation failed");}uint8_t* mapped{};Check(materialUpload->Map(0,nullptr,reinterpret_cast<void**>(&mapped)),"Material texture map failed");for(UINT mipIndex=0;mipIndex<mipCount;++mipIndex){const CompressedImageMip3D& mip=compressed.mips[mipIndex];const auto& footprint=footprints[mipIndex];if(rowBytes[mipIndex]!=mip.rowBytes||std::size_t(rowCounts[mipIndex])*mip.rowBytes!=mip.pixels.size())throw std::runtime_error("Compressed texture footprint mismatch");for(UINT row=0;row<rowCounts[mipIndex];++row)memcpy(mapped+footprint.Offset+std::size_t(row)*footprint.Footprint.RowPitch,mip.pixels.data()+std::size_t(row)*mip.rowBytes,mip.rowBytes);}materialUpload->Unmap(0,nullptr);if(!materialUploadRetained){d.textureUploadBatch.staging.push_back(materialUpload);d.textureUploadBatch.bytes+=size_t(materialSetUploadBytes);d.stats.materialTextureUploadStagingBytes+=size_t(materialSetUploadBytes);d.stats.peakMaterialTextureUploadStagingBytes=(std::max)(d.stats.peakMaterialTextureUploadStagingBytes,d.stats.materialTextureUploadStagingBytes);++d.stats.materialTextureUploadStagingResources;d.stats.peakMaterialTextureUploadStagingResources=(std::max)(d.stats.peakMaterialTextureUploadStagingResources,d.stats.materialTextureUploadStagingResources);++d.stats.materialTextureUploadBufferAllocations;materialUploadRetained=true;}++d.textureUploadBatch.textures;++d.stats.materialTextureUploads;auto* uploadList=d.textureUploadBatch.context.list.Get();ID3D12Resource* uploadResource=materialUpload.Get();for(UINT mipIndex=0;mipIndex<mipCount;++mipIndex){D3D12_TEXTURE_COPY_LOCATION destination{};destination.pResource=resident.textures[index].Get();destination.Type=D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;destination.SubresourceIndex=mipIndex;D3D12_TEXTURE_COPY_LOCATION source{};source.pResource=uploadResource;source.Type=D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;source.PlacedFootprint=footprints[mipIndex];uploadList->CopyTextureRegion(&destination,0,0,0,&source,nullptr);}D3D12_RESOURCE_BARRIER barrier{};barrier.Type=D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;barrier.Transition={resident.textures[index].Get(),D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,D3D12_RESOURCE_STATE_COPY_DEST,D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE};uploadList->ResourceBarrier(1,&barrier);if(index+1==textureCount&&(d.textureUploadBatch.textures>=D3D12Renderer::MaxMaterialTextureUploadBatchTextures||d.textureUploadBatch.bytes>=D3D12Renderer::MaxMaterialTextureUploadBatchBytes))d.SubmitTextureUploadBatch();D3D12_SHADER_RESOURCE_VIEW_DESC srv{};srv.Format=formats[index];srv.ViewDimension=D3D12_SRV_DIMENSION_TEXTURE2D;srv.Shader4ComponentMapping=D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;srv.Texture2D.MipLevels=mipCount;d.device->CreateShaderResourceView(resident.textures[index].Get(),&srv,descriptor);descriptor.ptr+=d.textureStep;}if(textureCount==3){const D3D12_CPU_DESCRIPTOR_HANDLE slotStart={d.textureHeap->GetCPUDescriptorHandleForHeapStart().ptr+SIZE_T(resident.descriptorOffset)*d.textureStep};for(size_t descriptorIndex=3;descriptorIndex<9;++descriptorIndex){const D3D12_CPU_DESCRIPTOR_HANDLE source={slotStart.ptr+SIZE_T(descriptorIndex%3)*d.textureStep},destination={slotStart.ptr+SIZE_T(descriptorIndex)*d.textureStep};d.device->CopyDescriptorsSimple(1,destination,source,D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);}}const MaterialTextureSetHandle3D handle{d.nextMaterialTextureSetHandle++};d.stats.residentTextureBytes+=resident.bytes;d.stats.residentTextureAllocationBytes+=resident.allocationBytes;d.stats.peakResidentTextureAllocationBytes=(std::max)(d.stats.peakResidentTextureAllocationBytes,d.stats.residentTextureAllocationBytes);++d.stats.residentMaterialTextureHeaps;++d.stats.materialTextureHeapAllocations;d.materialTextureCache.emplace(resident.cacheKey,handle.value);d.residentMaterialTextureSets.emplace(handle.value,std::move(resident));d.stats.residentMaterialTextureSets=d.residentMaterialTextureSets.size();++d.stats.residentMaterialTextureReferences;return handle;}catch(...){abandon();throw;}
}

void D3D12Renderer::DestroyMaterialTextureSet(MaterialTextureSetHandle3D handle){if(!impl_||!handle)return;auto& d=*impl_;auto found=d.residentMaterialTextureSets.find(handle.value);if(found==d.residentMaterialTextureSets.end())return;if(found->second.references>1){--found->second.references;--d.stats.residentMaterialTextureReferences;return;}d.SubmitTextureUploadBatch();d.PrepareGpuRetirement();RetiredGpuResources3D retired{};retired.resources=std::move(found->second.textures);retired.heap=std::move(found->second.heap);retired.bytes=found->second.allocationBytes;retired.textureSlot=found->second.descriptorOffset/9;retired.textures=true;d.stats.residentTextureBytes-=found->second.bytes;d.stats.residentTextureAllocationBytes-=found->second.allocationBytes;--d.stats.residentMaterialTextureHeaps;--d.stats.residentMaterialTextureReferences;const auto cached=d.materialTextureCache.find(found->second.cacheKey);if(cached!=d.materialTextureCache.end()&&cached->second==handle.value)d.materialTextureCache.erase(cached);d.residentMaterialTextureSets.erase(found);for(auto* groups:{&d.instanceGroups,&d.overlayInstanceGroups})for(auto iterator=groups->begin();iterator!=groups->end();)if(iterator->first.texture==handle.value)iterator=groups->erase(iterator);else++iterator;d.RefreshInstanceGroupStats();d.stats.residentMaterialTextureSets=d.residentMaterialTextureSets.size();d.RetireGpuResources(std::move(retired));}

void D3D12Renderer::SetMaterialTextureMemoryBudget(size_t bytes){if(!impl_)return;auto& d=*impl_;d.textureMemoryBudgetBytes=(std::max)(size_t(1),bytes);d.stats.materialTextureMemoryBudgetBytes=d.textureMemoryBudgetBytes;}
void D3D12Renderer::SetMaterialDerivedDataCacheDirectory(std::filesystem::path directory){if(!impl_)return;auto& d=*impl_;d.materialDerivedDataCacheDirectory=std::move(directory);d.materialDerivedDataCacheSurveyed=false;d.stats.materialDerivedDataCacheResidentBytes=0;}
void D3D12Renderer::SetMaterialDerivedDataCacheBudget(size_t bytes){if(!impl_)return;auto& d=*impl_;d.materialDerivedDataCacheBudgetBytes=bytes;d.materialDerivedDataCacheSurveyed=false;d.TrimMaterialDerivedDataCache();}

TransientMemoryStats D3D12Renderer::MemoryStats()const{
 if(!impl_)return{};const auto& d=*impl_;
 size_t bytes=d.stats.frameUploadResidentBytes+d.stats.hdrSceneColorBytes+d.stats.nativeDepthBufferBytes+d.stats.directionalShadowMapBytes+(d.instanceGroups.bucket_count()+d.overlayInstanceGroups.bucket_count()+d.shadowOnlyInstanceGroups.bucket_count())*sizeof(void*)+(d.activeMeshGroups.capacity()+d.activeOverlayMeshGroups.capacity()+d.activeShadowOnlyMeshGroups.capacity())*sizeof(ActiveMeshGroup3D)+d.meshUploadBatch.staging.capacity()*sizeof(ComPtr<ID3D12Resource>)+d.pendingMeshUploads.capacity()*sizeof(PendingMeshUpload3D)+d.availableMeshUploadContexts.capacity()*sizeof(MeshUploadContext3D)+d.textureUploadBatch.staging.capacity()*sizeof(ComPtr<ID3D12Resource>)+d.pendingTextureUploads.capacity()*sizeof(PendingTextureUpload3D)+d.availableTextureUploadContexts.capacity()*sizeof(MeshUploadContext3D)+d.retiredGpuResources.capacity()*sizeof(RetiredGpuResources3D)+d.freeTextureSlots.capacity()*sizeof(uint32_t),reclaimable{};
 for(unsigned index=0;index<Impl::Frames;++index){if(d.stats.lastSubmittedVertices<=D3D12Renderer::InitialFrameVertices&&d.vertexCapacity[index]>D3D12Renderer::InitialFrameVertices)reclaimable+=(d.vertexCapacity[index]-D3D12Renderer::InitialFrameVertices)*sizeof(Vertex);const bool idle3D=!d.stats.lastSubmittedWorldVertices&&!d.stats.lastSubmittedWorldIndices&&!d.stats.lastSubmittedMeshInstances&&!d.stats.lastSubmittedOverlayInstances&&!d.stats.lastSubmittedSkinnedInstances;if(idle3D){reclaimable+=d.worldVertexCapacity[index]*sizeof(Vertex3D)+d.worldIndexCapacity[index]*sizeof(uint32_t)+d.instanceCapacity[index]*sizeof(GpuMeshInstance)+d.skinnedInstanceCapacity[index]*sizeof(GpuSkinnedInstance)+d.skinPaletteCapacity[index]*sizeof(SkinPaletteJoint3D);}else{if(d.stats.lastSubmittedWorldVertices<=D3D12Renderer::InitialFrameWorldVertices&&d.worldVertexCapacity[index]>D3D12Renderer::InitialFrameWorldVertices)reclaimable+=(d.worldVertexCapacity[index]-D3D12Renderer::InitialFrameWorldVertices)*sizeof(Vertex3D);if(d.stats.lastSubmittedWorldIndices<=D3D12Renderer::InitialFrameWorldIndices&&d.worldIndexCapacity[index]>D3D12Renderer::InitialFrameWorldIndices)reclaimable+=(d.worldIndexCapacity[index]-D3D12Renderer::InitialFrameWorldIndices)*sizeof(uint32_t);if(d.stats.lastSubmittedMeshInstances+d.stats.lastSubmittedOverlayInstances<=D3D12Renderer::InitialFrameMeshInstances&&d.instanceCapacity[index]>D3D12Renderer::InitialFrameMeshInstances)reclaimable+=(d.instanceCapacity[index]-D3D12Renderer::InitialFrameMeshInstances)*sizeof(GpuMeshInstance);}}
 for(const auto* groups:{&d.instanceGroups,&d.overlayInstanceGroups,&d.shadowOnlyInstanceGroups})for(const auto& [key,group]:*groups){(void)key;bytes+=group.instances.capacity()*sizeof(GpuMeshInstance);if(group.instances.empty())reclaimable+=group.instances.capacity()*sizeof(GpuMeshInstance);}
 reclaimable+=d.availableMeshUploadContexts.capacity()*sizeof(MeshUploadContext3D)+d.availableTextureUploadContexts.capacity()*sizeof(MeshUploadContext3D);if(d.pendingMeshUploads.empty())reclaimable+=d.pendingMeshUploads.capacity()*sizeof(PendingMeshUpload3D);if(d.pendingTextureUploads.empty())reclaimable+=d.pendingTextureUploads.capacity()*sizeof(PendingTextureUpload3D);if(d.retiredGpuResources.empty())reclaimable+=d.retiredGpuResources.capacity()*sizeof(RetiredGpuResources3D);const bool idle3D=!d.stats.lastSubmittedWorldVertices&&!d.stats.lastSubmittedWorldIndices&&!d.stats.lastSubmittedMeshInstances&&!d.stats.lastSubmittedOverlayInstances&&!d.stats.lastSubmittedSkinnedInstances;if(d.hdrSceneColor&&idle3D)reclaimable+=d.stats.hdrSceneColorBytes;if(d.depth&&(idle3D||d.dlss.Enabled()))reclaimable+=d.stats.nativeDepthBufferBytes;if(d.shadowDepth&&!d.stats.lastDirectionalShadowCascades)reclaimable+=d.stats.directionalShadowMapBytes;
 bytes+=d.stats.ambientOcclusionBytes;if(d.ambientOcclusionRaw&&idle3D)reclaimable+=d.stats.ambientOcclusionBytes;
 return{bytes,reclaimable,d.lastTransientMemoryReleasedBytes,d.transientMemoryTrimCount};
}
void D3D12Renderer::TrimTransientMemory(){
 if(!impl_)return;auto& d=*impl_;const size_t before=MemoryStats().trackedCapacityBytes;const bool expanded=std::ranges::any_of(d.vertexCapacity,[](size_t capacity){return capacity>D3D12Renderer::InitialFrameVertices;})||std::ranges::any_of(d.worldVertexCapacity,[](size_t capacity){return capacity>D3D12Renderer::InitialFrameWorldVertices;})||std::ranges::any_of(d.worldIndexCapacity,[](size_t capacity){return capacity>D3D12Renderer::InitialFrameWorldIndices;})||std::ranges::any_of(d.instanceCapacity,[](size_t capacity){return capacity>D3D12Renderer::InitialFrameMeshInstances;});const bool idle3D=!d.stats.lastSubmittedWorldVertices&&!d.stats.lastSubmittedWorldIndices&&!d.stats.lastSubmittedMeshInstances&&!d.stats.lastSubmittedOverlayInstances&&!d.stats.lastSubmittedSkinnedInstances;const bool release3DUploads=idle3D&&(std::ranges::any_of(d.worldVertexBuffers,[](const auto& buffer){return bool(buffer);})||std::ranges::any_of(d.worldIndexBuffers,[](const auto& buffer){return bool(buffer);})||std::ranges::any_of(d.instanceBuffers,[](const auto& buffer){return bool(buffer);})||std::ranges::any_of(d.skinnedInstanceBuffers,[](const auto& buffer){return bool(buffer);})||std::ranges::any_of(d.skinPaletteBuffers,[](const auto& buffer){return bool(buffer);}));const bool releaseHdrScene=d.hdrSceneColor&&idle3D,releaseDepth=d.depth&&(idle3D||d.dlss.Enabled()),releaseShadowDepth=d.shadowDepth&&!d.stats.lastDirectionalShadowCascades;
 if(expanded||release3DUploads||releaseHdrScene||releaseDepth||releaseShadowDepth||d.ambientOcclusionRaw&&idle3D||!d.retiredGpuResources.empty()||d.stats.residentMeshUploadContexts||d.stats.residentMaterialTextureUploadContexts)d.Flush();
 if(releaseHdrScene)d.ReleaseHdrSceneColor();if(releaseDepth)d.ReleaseDepth();if(releaseShadowDepth)d.ReleaseShadowDepth();if(d.ambientOcclusionRaw&&idle3D)d.ReleaseAmbientOcclusion();
 if(expanded||release3DUploads){bool trimmed=false;for(unsigned i=0;i<Impl::Frames;++i){if(expanded){trimmed|=d.TrimFrameUploadBuffer(d.vertexBuffers[i],d.mapped[i],d.vertexCapacity[i],D3D12Renderer::InitialFrameVertices,D3D12Renderer::MaxVertices);if(d.vertexBuffers[i])d.vbv[i]={d.vertexBuffers[i]->GetGPUVirtualAddress(),UINT(d.vertexCapacity[i]*sizeof(Vertex)),sizeof(Vertex)};}if(release3DUploads){trimmed|=d.ReleaseFrameUploadBuffer(d.worldVertexBuffers[i],d.worldMapped[i],d.worldVertexCapacity[i]);d.worldVbv[i]={};trimmed|=d.ReleaseFrameUploadBuffer(d.worldIndexBuffers[i],d.worldIndexMapped[i],d.worldIndexCapacity[i]);d.worldIbv[i]={};trimmed|=d.ReleaseFrameUploadBuffer(d.instanceBuffers[i],d.instanceMapped[i],d.instanceCapacity[i]);trimmed|=d.ReleaseFrameUploadBuffer(d.skinnedInstanceBuffers[i],d.skinnedInstanceMapped[i],d.skinnedInstanceCapacity[i]);trimmed|=d.ReleaseFrameUploadBuffer(d.skinPaletteBuffers[i],d.skinPaletteMapped[i],d.skinPaletteCapacity[i]);}else if(expanded){trimmed|=d.TrimFrameUploadBuffer(d.worldVertexBuffers[i],d.worldMapped[i],d.worldVertexCapacity[i],D3D12Renderer::InitialFrameWorldVertices,D3D12Renderer::MaxWorldVertices);if(d.worldVertexBuffers[i])d.worldVbv[i]={d.worldVertexBuffers[i]->GetGPUVirtualAddress(),UINT(d.worldVertexCapacity[i]*sizeof(Vertex3D)),sizeof(Vertex3D)};trimmed|=d.TrimFrameUploadBuffer(d.worldIndexBuffers[i],d.worldIndexMapped[i],d.worldIndexCapacity[i],D3D12Renderer::InitialFrameWorldIndices,D3D12Renderer::MaxWorldIndices);if(d.worldIndexBuffers[i])d.worldIbv[i]={d.worldIndexBuffers[i]->GetGPUVirtualAddress(),UINT(d.worldIndexCapacity[i]*sizeof(uint32_t)),DXGI_FORMAT_R32_UINT};trimmed|=d.TrimFrameUploadBuffer(d.instanceBuffers[i],d.instanceMapped[i],d.instanceCapacity[i],D3D12Renderer::InitialFrameMeshInstances,D3D12Renderer::MaxMeshInstances);}}if(release3DUploads)d.stats.skinPaletteResidentBytes=0;if(trimmed)++d.stats.frameUploadTrims;}
 const size_t releasedMeshContexts=d.availableMeshUploadContexts.size(),releasedTextureContexts=d.availableTextureUploadContexts.size();d.stats.meshUploadContextReleases+=releasedMeshContexts;d.stats.materialTextureUploadContextReleases+=releasedTextureContexts;d.stats.residentMeshUploadContexts=d.stats.residentMeshUploadContexts>=releasedMeshContexts?d.stats.residentMeshUploadContexts-releasedMeshContexts:0;d.stats.residentMaterialTextureUploadContexts=d.stats.residentMaterialTextureUploadContexts>=releasedTextureContexts?d.stats.residentMaterialTextureUploadContexts-releasedTextureContexts:0;decltype(d.availableMeshUploadContexts){}.swap(d.availableMeshUploadContexts);decltype(d.availableTextureUploadContexts){}.swap(d.availableTextureUploadContexts);decltype(d.pendingMeshUploads){}.swap(d.pendingMeshUploads);decltype(d.pendingTextureUploads){}.swap(d.pendingTextureUploads);decltype(d.retiredGpuResources){}.swap(d.retiredGpuResources);
 decltype(d.instanceGroups){}.swap(d.instanceGroups);decltype(d.overlayInstanceGroups){}.swap(d.overlayInstanceGroups);decltype(d.shadowOnlyInstanceGroups){}.swap(d.shadowOnlyInstanceGroups);d.RefreshInstanceGroupStats();std::vector<ActiveMeshGroup3D> regular,overlay,shadowOnly;regular.reserve(64);overlay.reserve(64);shadowOnly.reserve(64);d.activeMeshGroups.swap(regular);d.activeOverlayMeshGroups.swap(overlay);d.activeShadowOnlyMeshGroups.swap(shadowOnly);const size_t after=MemoryStats().trackedCapacityBytes;d.lastTransientMemoryReleasedBytes=before>after?before-after:0;++d.transientMemoryTrimCount;
}

void D3D12Renderer::Render(const DrawList& dl,Color clear){
 // 相同 handle 的實例共用一次 indexed draw，逐幀只上傳模型矩陣、法線矩陣與 tint。
 auto &d = *impl_;
 const std::filesystem::path capturePath =
     std::exchange(d.pendingCapturePath, {});
 d.SubmitMeshUploadBatch();
 d.SubmitTextureUploadBatch();
 d.WaitForFrame(d.frame);
 d.CollectGpuTiming(d.frame);
 const size_t count = DrawableVertexCount(dl.vertices.size()),
              worldCount = DrawableWorldVertexCount(dl.worldVertices.size()),
              worldIndexCount =
                  DrawableWorldIndexCount(dl.worldIndices, worldCount);
 const uint64_t currentFrame = d.stats.submittedFrames;
 const Matrix4 lightViewProjection[2] = {
     CreateDirectionalLightViewProjection3D(
         dl.Atmosphere().cameraPosition, dl.DirectionalLight().direction, 38.f),
     CreateDirectionalLightViewProjection3D(dl.Atmosphere().cameraPosition,
                                            dl.DirectionalLight().direction,
                                            90.f)};
 auto prepareGroups = [&](auto &groups) {
   for (auto iterator = groups.begin(); iterator != groups.end();) {
     auto &group = iterator->second;
     group.instances.clear();
     if (currentFrame - group.lastUsedFrame >
         D3D12Renderer::InstanceGroupRetentionFrames) {
       d.stats.instanceGroupReleasedBytes +=
           group.instances.capacity() * sizeof(GpuMeshInstance);
       ++d.stats.instanceGroupEvictions;
       iterator = groups.erase(iterator);
     } else
       ++iterator;
   }
   if (groups.empty() && groups.bucket_count() > 128) {
     d.stats.instanceGroupReleasedBytes +=
         groups.bucket_count() * sizeof(void *);
     groups.rehash(0);
   }
 };
 prepareGroups(d.instanceGroups);
 prepareGroups(d.overlayInstanceGroups);
 prepareGroups(d.shadowOnlyInstanceGroups);
 struct TransparentSubmission {
   GpuMeshInstance gpu{};
   ResidentMesh3D *mesh{};
   MaterialTextureSetHandle3D textures{};
   float depth{};
 };
 std::vector<TransparentSubmission> transparentSubmissions;
 transparentSubmissions.reserve(
     (std::min)(dl.meshInstances.size(), MaxMeshInstances));
 d.stats.lastSubmittedFoliageInstances = 0;
 d.stats.lastSubmittedTerrainInstances = 0;
 size_t acceptedInstances = 0, acceptedOverlayInstances = 0,
        acceptedShadowOnlyInstances = 0, acceptedTexturedInstances = 0,
        droppedInstances = 0;
 d.stats.lastDirectionalShadowAutomaticMasks = 0;
 d.stats.lastDirectionalShadowManualMasks = 0;
 d.stats.lastDirectionalShadowLightSpaceRejected = {};
 d.stats.lastDirectionalShadowMaskFallbacks = 0;
 for (size_t index = 0; index < dl.meshInstances.size(); ++index) {
   if (acceptedInstances >= MaxMeshInstances) {
     droppedInstances += dl.meshInstances.size() - index;
     break;
   }
   const MeshInstance3D &instance = dl.meshInstances[index];
   const auto residentMesh = d.residentMeshes.find(instance.mesh.value);
   if (residentMesh == d.residentMeshes.end() ||
       (instance.layer == MeshRenderLayer3D::ShadowOnly && !instance.castsShadow)) {
     ++droppedInstances;
     continue;
   }
   const bool shadowOnly = instance.layer == MeshRenderLayer3D::ShadowOnly,
              validTextures = !shadowOnly && instance.textures &&
                              d.residentMaterialTextureSets.contains(
                                  instance.textures.value);
   const bool shadowLayer = instance.layer == MeshRenderLayer3D::Opaque || shadowOnly;
   uint8_t shadowCascadeMask{};
   if (shadowLayer && instance.castsShadow) {
     if (instance.shadowCascadeSelection == ShadowCascadeSelection3D::Manual) {
       shadowCascadeMask = instance.shadowCascadeMask & DirectionalShadowAllCascades3D;
       ++d.stats.lastDirectionalShadowManualMasks;
     } else {
       const DirectionalShadowLightSpaceMaskResult3D result =
           DirectionalShadowLightSpaceMask3D(residentMesh->second.bounds,
                                             instance.transform,
                                             lightViewProjection);
       shadowCascadeMask = result.mask;
       ++d.stats.lastDirectionalShadowAutomaticMasks;
       d.stats.lastDirectionalShadowMaskFallbacks += result.fallback;
       for (size_t cascade = 0; cascade < 2; ++cascade)
         d.stats.lastDirectionalShadowLightSpaceRejected[cascade] +=
             !result.fallback && !(result.mask & (1u << cascade));
     }
   }
   if (shadowOnly && !shadowCascadeMask) {
     ++droppedInstances;
     continue;
   }
   GpuMeshInstance gpu{};
   gpu.transform = instance.transform;
   for (size_t row = 0; row < 3; ++row)
     gpu.normalRows[row] = {instance.normalTransform[row][0],
                            instance.normalTransform[row][1],
                            instance.normalTransform[row][2], 0};
   gpu.tint = instance.tint;
   gpu.surface = instance.surface;
   const uint32_t packedTextureScale = uint32_t(
       std::lround(Clamp(instance.textureScale / 16.f, 0.f, 1.f) * 65535.f));
   gpu.textureData =
       PackMeshInstanceFlags3D(validTextures, instance.useUv,
                               instance.materialClass) |
       PackMeshInstanceLodData3D(instance.lodCoverage, instance.lodSeed) |
       (packedTextureScale << 16);
   if (instance.layer == MeshRenderLayer3D::Transparent) {
     transparentSubmissions.push_back(
         {gpu, &residentMesh->second,
          validTextures ? instance.textures : MaterialTextureSetHandle3D{},
          instance.cameraDepth});
   } else {
     auto &groups = shadowOnly ? d.shadowOnlyInstanceGroups
                    : instance.layer == MeshRenderLayer3D::Overlay
                        ? d.overlayInstanceGroups
                        : d.instanceGroups;
     const InstanceGroupKey3D groupKey{
         instance.mesh.value,
         validTextures ? instance.textures.value : 0,
         shadowCascadeMask,
         uint8_t(instance.materialClass == MaterialClass3D::Foliage)};
     auto [groupIterator, created] = groups.try_emplace(groupKey);
     if (created)
       ++d.stats.instanceGroupCreations;
     groupIterator->second.lastUsedFrame = currentFrame;
     groupIterator->second.instances.push_back(gpu);
   }
   acceptedOverlayInstances += instance.layer == MeshRenderLayer3D::Overlay;
   acceptedShadowOnlyInstances += shadowOnly;
   acceptedTexturedInstances += validTextures;
   d.stats.lastSubmittedFoliageInstances +=
       !shadowOnly && instance.materialClass == MaterialClass3D::Foliage;
   d.stats.lastSubmittedTerrainInstances +=
       !shadowOnly && instance.materialClass == MaterialClass3D::Terrain;
   ++acceptedInstances;
 }
 std::vector<ActiveSkinnedMesh3D> activeSkinnedMeshes;
 activeSkinnedMeshes.reserve((std::min)(dl.skinnedMeshInstances.size(),DrawList::MaximumSkinnedMeshInstancesPerFrame));
 auto resolveSkinnedInstance=[&](const SkinnedMeshInstance3D& instance,ResidentSkinnedMesh3D*& resident,uint32_t& indexCount){const auto found=d.residentSkinnedMeshes.find(instance.mesh.value);if(found==d.residentSkinnedMeshes.end()||instance.firstIndex>found->second.indexCount)return false;indexCount=instance.indexCount?instance.indexCount:found->second.indexCount-instance.firstIndex;if(indexCount>found->second.indexCount-instance.firstIndex||!indexCount||instance.firstIndex%3||indexCount%3||instance.palette.firstJoint>dl.skinPaletteJoints.size()||instance.palette.jointCount>dl.skinPaletteJoints.size()-instance.palette.firstJoint||instance.palette.jointCount<found->second.jointCount||instance.materialClass==MaterialClass3D::Water||instance.materialClass==MaterialClass3D::Terrain)return false;resident=&found->second;return true;};
 size_t validSkinnedInstances{};for(const SkinnedMeshInstance3D& instance:dl.skinnedMeshInstances){ResidentSkinnedMesh3D* resident{};uint32_t indexCount{};if(resolveSkinnedInstance(instance,resident,indexCount)&&++validSkinnedInstances>=DrawList::MaximumSkinnedMeshInstancesPerFrame)break;}
 if(validSkinnedInstances)d.EnsureFrameUploadBuffer(d.skinnedInstanceBuffers[d.frame],d.skinnedInstanceMapped[d.frame],d.skinnedInstanceCapacity[d.frame],validSkinnedInstances,DrawList::MaximumSkinPalettesPerFrame,DrawList::MaximumSkinnedMeshInstancesPerFrame);
 size_t acceptedSkinnedInstances{},acceptedTexturedSkinnedInstances{},acceptedSkinShadedInstances{};
 for(const SkinnedMeshInstance3D& instance:dl.skinnedMeshInstances){
   if(acceptedSkinnedInstances>=DrawList::MaximumSkinnedMeshInstancesPerFrame){++d.stats.rejectedSkinnedInstances;continue;}
   ResidentSkinnedMesh3D* mesh{};uint32_t indexCount{};if(!resolveSkinnedInstance(instance,mesh,indexCount)){++d.stats.rejectedSkinnedInstances;continue;}
   const bool validTextures=instance.textures&&d.residentMaterialTextureSets.contains(instance.textures.value);
   if(validTextures)++acceptedTexturedSkinnedInstances;
   else if(instance.textures)++d.stats.missingSkinnedTextureBindings;
   if(instance.shadingModel==CharacterShadingModel3D::Skin)++acceptedSkinShadedInstances;
   GpuSkinnedInstance gpu{};gpu.instance.transform=instance.transform;for(size_t row=0;row<3;++row)gpu.instance.normalRows[row]={instance.normalTransform[row][0],instance.normalTransform[row][1],instance.normalTransform[row][2],0};gpu.instance.tint=instance.tint;gpu.instance.surface=instance.surface;gpu.instance.textureData=PackSkinnedMeshInstanceData3D(validTextures,instance.useUv,instance.materialClass,instance.textureScale,instance.shadingModel);gpu.skinPaletteBase=instance.palette.firstJoint;
   d.skinnedInstanceMapped[d.frame][acceptedSkinnedInstances]=gpu;
   activeSkinnedMeshes.push_back({mesh,validTextures?instance.textures:MaterialTextureSetHandle3D{},acceptedSkinnedInstances,instance.firstIndex,indexCount,instance.castsShadow?uint8_t(instance.shadowCascadeMask&DirectionalShadowAllCascades3D):uint8_t{},instance.materialClass==MaterialClass3D::Foliage});
   ++acceptedSkinnedInstances;
 }
 if(acceptedSkinnedInstances){
   const size_t jointCount=(std::min)(dl.skinPaletteJoints.size(),DrawList::MaximumSkinJointsPerFrame);
   d.EnsureFrameUploadBuffer(d.skinPaletteBuffers[d.frame],d.skinPaletteMapped[d.frame],d.skinPaletteCapacity[d.frame],jointCount,jointCount,DrawList::MaximumSkinJointsPerFrame);
   memcpy(d.skinPaletteMapped[d.frame],dl.skinPaletteJoints.data(),jointCount*sizeof(SkinPaletteJoint3D));
   ++d.stats.skinPaletteUploads;d.stats.lastUploadedSkinJoints=jointCount;
 }else d.stats.lastUploadedSkinJoints=0;
 d.stats.lastSubmittedSkinnedInstances=acceptedSkinnedInstances;
 d.stats.lastSubmittedTexturedSkinnedInstances=acceptedTexturedSkinnedInstances;
 d.stats.lastSubmittedSkinShadedInstances=acceptedSkinShadedInstances;
 d.stats.skinPaletteResidentBytes=0;for(size_t capacity:d.skinPaletteCapacity)d.stats.skinPaletteResidentBytes+=capacity*sizeof(SkinPaletteJoint3D);d.stats.peakSkinPaletteResidentBytes=(std::max)(d.stats.peakSkinPaletteResidentBytes,d.stats.skinPaletteResidentBytes);
 d.RefreshInstanceGroupStats();
 if (count) {
   d.EnsureFrameUploadBuffer(d.vertexBuffers[d.frame], d.mapped[d.frame],
                             d.vertexCapacity[d.frame], count,
                             InitialFrameVertices, MaxVertices);
   d.vbv[d.frame] = {d.vertexBuffers[d.frame]->GetGPUVirtualAddress(),
                     UINT(d.vertexCapacity[d.frame] * sizeof(Vertex)),
                     sizeof(Vertex)};
 }
 if (worldCount) {
   d.EnsureFrameUploadBuffer(d.worldVertexBuffers[d.frame],
                             d.worldMapped[d.frame],
                             d.worldVertexCapacity[d.frame], worldCount,
                             InitialFrameWorldVertices, MaxWorldVertices);
   d.worldVbv[d.frame] = {
       d.worldVertexBuffers[d.frame]->GetGPUVirtualAddress(),
       UINT(d.worldVertexCapacity[d.frame] * sizeof(Vertex3D)),
       sizeof(Vertex3D)};
 }
 if (worldIndexCount) {
   d.EnsureFrameUploadBuffer(d.worldIndexBuffers[d.frame],
                             d.worldIndexMapped[d.frame],
                             d.worldIndexCapacity[d.frame], worldIndexCount,
                             InitialFrameWorldIndices, MaxWorldIndices);
   d.worldIbv[d.frame] = {
       d.worldIndexBuffers[d.frame]->GetGPUVirtualAddress(),
       UINT(d.worldIndexCapacity[d.frame] * sizeof(uint32_t)),
       DXGI_FORMAT_R32_UINT};
 }
 if (acceptedInstances)
   d.EnsureFrameUploadBuffer(d.instanceBuffers[d.frame],
                             d.instanceMapped[d.frame],
                             d.instanceCapacity[d.frame], acceptedInstances,
                             InitialFrameMeshInstances, MaxMeshInstances);
 const PointLightSelection3D<MaxPointLights> pointLights =
     SelectInfluentialPointLights<MaxPointLights>(
         dl.PointLights(), dl.Atmosphere().cameraPosition);
  SceneConstants3D scene{};
  const DirectionalLight3D &sceneLight = dl.DirectionalLight();
  const Atmosphere3D &sceneAtmosphere = dl.Atmosphere();
  const unsigned rw = d.dlss.Enabled() ? d.dlss.RenderWidth() : d.width,
                 rh = d.dlss.Enabled() ? d.dlss.RenderHeight() : d.height;
  const CanvasViewport worldCanvas = CalculateCanvasViewport(
      {float(d.logicalWidth), float(d.logicalHeight)}, {float(rw), float(rh)});
  Matrix4 inverseViewProjection{};
  const bool inverseViewProjectionValid =
      InvertMatrix4x4(dl.ViewProjection(), inverseViewProjection);
 memcpy(scene.values.data(), &dl.ViewProjection().values[0][0],
        sizeof(float) * 16);
 const float directionAmbient[4] = {sceneLight.direction.x,
                                    sceneLight.direction.y,
                                    sceneLight.direction.z, sceneLight.ambient},
             colorIntensity[4] = {sceneLight.color.r, sceneLight.color.g,
                                  sceneLight.color.b, sceneLight.intensity},
             fogColor[4] = {sceneAtmosphere.fogColor.r,
                            sceneAtmosphere.fogColor.g,
                            sceneAtmosphere.fogColor.b,
                            sceneAtmosphere.fogColor.a},
             cameraFogStart[4] = {sceneAtmosphere.cameraPosition.x,
                                  sceneAtmosphere.cameraPosition.y,
                                  sceneAtmosphere.cameraPosition.z,
                                  sceneAtmosphere.fogStart},
             fogEndEnabled[4] = {
                 sceneAtmosphere.fogEnd, sceneAtmosphere.enabled ? 1.f : 0.f,
                 float(pointLights.count), dl.EnvironmentTime()};
 memcpy(scene.values.data() + 16, directionAmbient, sizeof(directionAmbient));
 memcpy(scene.values.data() + 20, colorIntensity, sizeof(colorIntensity));
 memcpy(scene.values.data() + 24, fogColor, sizeof(fogColor));
 memcpy(scene.values.data() + 28, cameraFogStart, sizeof(cameraFogStart));
 memcpy(scene.values.data() + 32, fogEndEnabled, sizeof(fogEndEnabled));
 for (std::size_t index = 0; index < pointLights.count; ++index) {
   const PointLight3D &point = pointLights.lights[index];
   const float positionRadius[4] = {point.position.x, point.position.y,
                                    point.position.z, point.radius},
               pointIntensity[4] = {point.color.r, point.color.g, point.color.b,
                                    point.intensity};
   memcpy(scene.values.data() + 36 + index * 4, positionRadius,
          sizeof(positionRadius));
   memcpy(scene.values.data() + 48 + index * 4, pointIntensity,
          sizeof(pointIntensity));
 }
 scene.values[60] = std::exp2(dl.ExposureCompensation());
 const bool ambientOcclusionEnabled=dl.AmbientOcclusion().enabled&&!d.dlss.Enabled();
  scene.values[61]=ambientOcclusionEnabled?1.f:0.f;scene.values[62]=1.f/float((std::max)(1u,rw));scene.values[63]=1.f/float((std::max)(1u,rh));
  if(inverseViewProjectionValid)memcpy(scene.values.data()+64,&inverseViewProjection.values[0][0],sizeof(float)*16);
  scene.values[80]=worldCanvas.x/float(rw);scene.values[81]=worldCanvas.y/float(rh);scene.values[82]=inverseViewProjectionValid?worldCanvas.width/float(rw):0.f;scene.values[83]=inverseViewProjectionValid?worldCanvas.height/float(rh):0.f;
  memcpy(d.sceneConstantsMapped + d.frame * (SceneConstantStride3D/sizeof(float)), scene.values.data(),
         sizeof(scene));
 d.stats.lastExposureCompensationEv = dl.ExposureCompensation();
 d.stats.ambientOcclusionEnabled=ambientOcclusionEnabled;
 d.activeMeshGroups.clear();
 d.activeOverlayMeshGroups.clear();
 d.activeShadowOnlyMeshGroups.clear();
 std::vector<ActiveMeshGroup3D> activeTransparentMeshGroups;
 activeTransparentMeshGroups.reserve(transparentSubmissions.size());
 std::ranges::stable_sort(
     transparentSubmissions,
     [](const TransparentSubmission &left, const TransparentSubmission &right) {
       return left.depth > right.depth;
     });
 size_t instanceOffset = 0;
 auto activate = [&](auto &source, auto &active) {
   for (auto &[groupKey, group] : source) {
     auto &instances = group.instances;
     if (instances.empty())
       continue;
     const uint32_t meshHandle = groupKey.mesh,
                    textureHandle = groupKey.texture;
     auto mesh = d.residentMeshes.find(meshHandle);
     if (mesh == d.residentMeshes.end())
       continue;
     memcpy(d.instanceMapped[d.frame] + instanceOffset, instances.data(),
            instances.size() * sizeof(GpuMeshInstance));
     active.push_back({&mesh->second,
                       {textureHandle},
                       instanceOffset,
                       instances.size(),
                       groupKey.shadowCascadeMask,
                       groupKey.doubleSided != 0});
     instanceOffset += instances.size();
   }
 };
 activate(d.instanceGroups, d.activeMeshGroups);
 for (const TransparentSubmission &submission : transparentSubmissions) {
   d.instanceMapped[d.frame][instanceOffset] = submission.gpu;
   activeTransparentMeshGroups.push_back(
       {submission.mesh, submission.textures, instanceOffset, 1});
   ++instanceOffset;
 }
 activate(d.overlayInstanceGroups, d.activeOverlayMeshGroups);
 activate(d.shadowOnlyInstanceGroups, d.activeShadowOnlyMeshGroups);
 const DrawListStats2D drawStats = dl.Stats2D();
 auto forEachCanvasSpan = [&](auto &&callback) {
   size_t cursor = 0;
   for (const DrawSpan2D &span : dl.AdditiveSpans2D()) {
     if (span.firstVertex >= count)
       break;
     const size_t first = (std::max)(cursor, span.firstVertex),
                  available = count - span.firstVertex,
                  end = span.vertexCount >= available
                            ? count
                            : span.firstVertex + span.vertexCount;
     if (first > cursor)
       callback(cursor, first - cursor, false);
     if (end > first)
       callback(first, end - first, true);
     cursor = (std::max)(cursor, end);
   }
   if (cursor < count)
     callback(cursor, count - cursor, false);
 };
 uint32_t canvasDrawCalls = 0, additiveDrawCalls = 0;
 forEachCanvasSpan([&](size_t, size_t vertexCount, bool additive) {
   if (!vertexCount)
     return;
   ++canvasDrawCalls;
   additiveDrawCalls += additive;
 });
 d.stats.lastSubmittedVertices = count;
 d.stats.lastSubmittedWorldVertices = worldCount;
 d.stats.lastSubmittedWorldIndices = worldIndexCount;
 d.stats.lastSubmittedMeshInstances = acceptedInstances;
 d.stats.lastSubmittedOverlayInstances = acceptedOverlayInstances;
 d.stats.lastSubmittedShadowOnlyInstances = acceptedShadowOnlyInstances;
 d.stats.lastSubmittedTexturedMeshInstances = acceptedTexturedInstances;
 d.stats.lastSubmittedPointLights = pointLights.count;
 d.stats.lastPointLightCandidates = pointLights.candidates;
 d.stats.lastInputTriangles2D = drawStats.inputTriangles;
 d.stats.lastCulledTriangles2D = drawStats.culledTriangles;
 d.stats.lastAdditiveTriangles2D = drawStats.additiveTriangles;
 d.stats.lastAdditiveDrawCalls2D = additiveDrawCalls;
 d.stats.totalCulledTriangles2D += drawStats.culledTriangles;
 d.stats.totalAdditiveTriangles2D += drawStats.additiveTriangles;
 d.stats.lastUploadBytes = count * sizeof(Vertex) +
                           worldCount * sizeof(Vertex3D) +
                           worldIndexCount * sizeof(uint32_t) +
                           acceptedInstances * sizeof(GpuMeshInstance)+
                           acceptedSkinnedInstances*sizeof(GpuSkinnedInstance)+
                           d.stats.lastUploadedSkinJoints*sizeof(SkinPaletteJoint3D);
 d.stats.peakSubmittedVertices =
     (std::max)(d.stats.peakSubmittedVertices, count);
 d.stats.peakSubmittedWorldVertices =
     (std::max)(d.stats.peakSubmittedWorldVertices, worldCount);
 d.stats.peakSubmittedWorldIndices =
     (std::max)(d.stats.peakSubmittedWorldIndices, worldIndexCount);
 d.stats.peakSubmittedMeshInstances =
     (std::max)(d.stats.peakSubmittedMeshInstances, acceptedInstances);
 d.stats.totalDroppedVertices += dl.vertices.size() - count;
 d.stats.totalDroppedWorldVertices += dl.worldVertices.size() - worldCount;
 d.stats.totalDroppedWorldIndices += dl.worldIndices.size() - worldIndexCount;
 d.stats.totalDroppedMeshInstances += droppedInstances;
 d.stats.totalDroppedPointLights += pointLights.candidates - pointLights.count;
 d.stats.lastDrawCalls =
     canvasDrawCalls + uint32_t(worldCount > 0 && worldIndexCount > 0) +
     uint32_t(d.activeMeshGroups.size() + activeTransparentMeshGroups.size() +
              d.activeOverlayMeshGroups.size()+activeSkinnedMeshes.size());
 d.stats.lastBackfaceCulling = dl.BackfaceCulling();
 ++d.stats.submittedFrames;
 size_t actualAdditiveVertices = 0;
 forEachCanvasSpan([&](size_t, size_t vertexCount, bool additive) {
   if (additive)
     actualAdditiveVertices += vertexCount;
 });
 const size_t actualAdditiveTriangles = actualAdditiveVertices / 3;
 d.stats.totalAdditiveTriangles2D -= drawStats.additiveTriangles;
 d.stats.totalAdditiveTriangles2D += actualAdditiveTriangles;
 d.stats.lastAdditiveTriangles2D = actualAdditiveTriangles;
 if (count)
   memcpy(d.mapped[d.frame], dl.vertices.data(), count * sizeof(Vertex));
 if (worldCount)
   memcpy(d.worldMapped[d.frame], dl.worldVertices.data(),
          worldCount * sizeof(Vertex3D));
 if (worldIndexCount)
   memcpy(d.worldIndexMapped[d.frame], dl.worldIndices.data(),
          worldIndexCount * sizeof(uint32_t));
 Check(d.alloc[d.frame]->Reset(), "Allocator reset failed");
 Check(d.list->Reset(d.alloc[d.frame].Get(), d.pipeline.Get()),
       "List reset failed");
 const UINT timestampStart = d.frame * 2;
 d.list->EndQuery(d.timestampHeap.Get(), D3D12_QUERY_TYPE_TIMESTAMP,
                  timestampStart);
 float c[4] = {clear.r, clear.g, clear.b, clear.a};
 const bool hasShadowCaster =
     !d.activeShadowOnlyMeshGroups.empty() ||
     std::ranges::any_of(activeSkinnedMeshes,[](const ActiveSkinnedMesh3D& mesh){return mesh.shadowCascadeMask!=0;})||
     std::ranges::any_of(d.activeMeshGroups, [](const ActiveMeshGroup3D &group) {
       return group.shadowCascadeMask != 0;
     });
 const bool renderShadowMap =
     dl.DirectionalShadowMapEnabled() &&
     ((worldCount && worldIndexCount) || hasShadowCaster);
 d.stats.lastDirectionalShadowCascades = renderShadowMap ? 2 : 0;
 d.stats.directionalShadowCascadeSplit = 32.f;
 d.stats.directionalShadowTransitionWidth = 6.f;
 d.stats.directionalShadowBasePcfSamples = 12;
 d.stats.directionalShadowTransitionPcfSamples = 24;
 d.stats.lastDirectionalShadowCasters = 0;
 for (const ActiveMeshGroup3D &group : d.activeMeshGroups)
   if (group.shadowCascadeMask)
     d.stats.lastDirectionalShadowCasters += group.instanceCount;
 for (const ActiveMeshGroup3D &group : d.activeShadowOnlyMeshGroups)
   d.stats.lastDirectionalShadowCasters += group.instanceCount;
 for(const ActiveSkinnedMesh3D& mesh:activeSkinnedMeshes)if(mesh.shadowCascadeMask)++d.stats.lastDirectionalShadowCasters;
 d.stats.lastDirectionalShadowCasters += worldCount && worldIndexCount;
 d.stats.lastDirectionalShadowDrawCalls = 0;
 d.stats.lastDirectionalShadowCascadeInstances = {};
 d.stats.lastDirectionalShadowCascadeDrawCalls = {};
 d.stats.lastDirectionalShadowSharedInstances = 0;
 d.stats.lastDirectionalShadowInstancePassesSaved = 0;
 if (renderShadowMap) {
   const auto countGroup = [&](const ActiveMeshGroup3D &group) {
     if (group.shadowCascadeMask == DirectionalShadowAllCascades3D)
       d.stats.lastDirectionalShadowSharedInstances += group.instanceCount;
     for (size_t cascade = 0; cascade < 2; ++cascade)
       if (group.shadowCascadeMask & (1u << cascade)) {
         d.stats.lastDirectionalShadowCascadeInstances[cascade] += group.instanceCount;
         ++d.stats.lastDirectionalShadowCascadeDrawCalls[cascade];
       }
   };
   for (const ActiveMeshGroup3D &group : d.activeMeshGroups)
     countGroup(group);
   for (const ActiveMeshGroup3D &group : d.activeShadowOnlyMeshGroups)
     countGroup(group);
   for(const ActiveSkinnedMesh3D& mesh:activeSkinnedMeshes){for(size_t cascade=0;cascade<2;++cascade)if(mesh.shadowCascadeMask&(1u<<cascade)){++d.stats.lastDirectionalShadowCascadeInstances[cascade];++d.stats.lastDirectionalShadowCascadeDrawCalls[cascade];}}
   for (size_t cascade = 0; cascade < 2; ++cascade) {
     d.stats.lastDirectionalShadowCascadeDrawCalls[cascade] += size_t(worldCount && worldIndexCount);
     d.stats.lastDirectionalShadowDrawCalls += d.stats.lastDirectionalShadowCascadeDrawCalls[cascade];
   }
   const size_t maximumInstancePasses =
       (d.stats.lastDirectionalShadowCasters - size_t(worldCount && worldIndexCount)) * 2;
   const size_t submittedInstancePasses = d.stats.lastDirectionalShadowCascadeInstances[0] + d.stats.lastDirectionalShadowCascadeInstances[1];
   d.stats.lastDirectionalShadowInstancePassesSaved =
       maximumInstancePasses > submittedInstancePasses ? maximumInstancePasses - submittedInstancePasses : 0;
 }
 if (!renderShadowMap)
   d.stats.lastDirectionalShadowCasters = 0;
 if (renderShadowMap && !d.shadowDepth)
   d.CreateShadowDepth();
 const float shadowParameters[4] = {renderShadowMap ? 32.f : 0.f, .0015f,
                                    .00035f, 1.f / 2048.f};
 const float shadowTransition[4] = {6.f, 0, 0, 0};
 float *shadowConstants = d.shadowConstantsMapped + d.frame * 64;
 memcpy(shadowConstants, &lightViewProjection[0].values[0][0],
        sizeof(float) * 16);
 memcpy(shadowConstants + 16, &lightViewProjection[1].values[0][0],
        sizeof(float) * 16);
 memcpy(shadowConstants + 32, shadowParameters, sizeof(shadowParameters));
 memcpy(shadowConstants + 36, shadowTransition, sizeof(shadowTransition));
 if (renderShadowMap) {
   D3D12_RESOURCE_BARRIER shadowWrite{};
   shadowWrite.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
   shadowWrite.Transition = {d.shadowDepth.Get(),
                             D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
                             D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
                             D3D12_RESOURCE_STATE_DEPTH_WRITE};
   d.list->ResourceBarrier(1, &shadowWrite);
   D3D12_VIEWPORT shadowViewport{0, 0, 2048, 2048, 0, 1};
   D3D12_RECT shadowRect{0, 0, 2048, 2048};
   d.list->RSSetViewports(1, &shadowViewport);
   d.list->RSSetScissorRects(1, &shadowRect);
   D3D12_CPU_DESCRIPTOR_HANDLE shadowDsv =
       d.shadowDsvHeap->GetCPUDescriptorHandleForHeapStart();
   const UINT shadowDsvStep = d.device->GetDescriptorHandleIncrementSize(
       D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
   for (UINT cascade = 0; cascade < 2; ++cascade) {
     d.list->ClearDepthStencilView(shadowDsv, D3D12_CLEAR_FLAG_DEPTH, 1, 0, 0,
                                   nullptr);
     d.list->OMSetRenderTargets(0, nullptr, FALSE, &shadowDsv);
     d.list->SetGraphicsRootSignature(d.root.Get());
     D3D12_GPU_DESCRIPTOR_HANDLE sceneCbv =
                                     d.textureHeap
                                         ->GetGPUDescriptorHandleForHeapStart(),
                                 shadowCbv = sceneCbv;
     sceneCbv.ptr += UINT64(SceneCbvDescriptorBase3D + d.frame) * d.textureStep;
     shadowCbv.ptr +=
         UINT64(ShadowCbvDescriptorBase3D + d.frame) * d.textureStep;
     d.list->SetDescriptorHeaps(1, d.textureHeap.GetAddressOf());
     d.list->SetGraphicsRootDescriptorTable(0, sceneCbv);
     d.list->SetGraphicsRootDescriptorTable(3, shadowCbv);
     const UINT drawConstants[2] = {cascade, 0};
     d.list->SetGraphicsRoot32BitConstants(4, 2, drawConstants, 0);
     d.list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
     for (const ActiveMeshGroup3D &group : d.activeMeshGroups) {
       if (!(group.shadowCascadeMask & (1u << cascade)))
         continue;
       d.list->SetPipelineState(d.shadowInstancedPipeline.Get());
       D3D12_VERTEX_BUFFER_VIEW views[2] = {
           group.mesh->vertexView,
           {d.instanceBuffers[d.frame]->GetGPUVirtualAddress() +
                group.instanceOffset * sizeof(GpuMeshInstance),
            UINT(group.instanceCount * sizeof(GpuMeshInstance)),
            sizeof(GpuMeshInstance)}};
       d.list->IASetVertexBuffers(0, 2, views);
       d.list->IASetIndexBuffer(&group.mesh->indexView);
       d.list->DrawIndexedInstanced(group.mesh->indexCount,
                                    UINT(group.instanceCount), 0, 0, 0);
     }
     for (const ActiveMeshGroup3D &group : d.activeShadowOnlyMeshGroups) {
       if (!(group.shadowCascadeMask & (1u << cascade)))
         continue;
       d.list->SetPipelineState(d.shadowInstancedPipeline.Get());
       D3D12_VERTEX_BUFFER_VIEW views[2] = {
           group.mesh->vertexView,
           {d.instanceBuffers[d.frame]->GetGPUVirtualAddress() +
                group.instanceOffset * sizeof(GpuMeshInstance),
            UINT(group.instanceCount * sizeof(GpuMeshInstance)),
            sizeof(GpuMeshInstance)}};
       d.list->IASetVertexBuffers(0, 2, views);
       d.list->IASetIndexBuffer(&group.mesh->indexView);
       d.list->DrawIndexedInstanced(group.mesh->indexCount,
                                    UINT(group.instanceCount), 0, 0, 0);
     }
     if(!activeSkinnedMeshes.empty())d.list->SetGraphicsRootShaderResourceView(7,d.skinPaletteBuffers[d.frame]->GetGPUVirtualAddress());
     for(const ActiveSkinnedMesh3D& mesh:activeSkinnedMeshes){if(!(mesh.shadowCascadeMask&(1u<<cascade)))continue;d.list->SetPipelineState(d.shadowSkinnedPipeline.Get());D3D12_VERTEX_BUFFER_VIEW views[2]={mesh.mesh->vertexView,{d.skinnedInstanceBuffers[d.frame]->GetGPUVirtualAddress()+mesh.instanceOffset*sizeof(GpuSkinnedInstance),UINT(sizeof(GpuSkinnedInstance)),sizeof(GpuSkinnedInstance)}};d.list->IASetVertexBuffers(0,2,views);d.list->IASetIndexBuffer(&mesh.mesh->indexView);d.list->DrawIndexedInstanced(mesh.indexCount,1,mesh.firstIndex,0,0);}
     if (worldCount && worldIndexCount) {
       d.list->SetPipelineState(d.shadowWorldPipeline.Get());
       d.list->IASetVertexBuffers(0, 1, &d.worldVbv[d.frame]);
       d.list->IASetIndexBuffer(&d.worldIbv[d.frame]);
       d.list->DrawIndexedInstanced(UINT(worldIndexCount), 1, 0, 0, 0);
     }
     shadowDsv.ptr += shadowDsvStep;
   }
   std::swap(shadowWrite.Transition.StateBefore,
             shadowWrite.Transition.StateAfter);
   d.list->ResourceBarrier(1, &shadowWrite);
 }
 const bool useDLSS = d.dlss.Enabled();
 d.stats.worldSrgbRenderTargetViews = 0;
 bool dlssEvaluationFailed = false;
 const bool hasWorldPass =
     (worldCount && worldIndexCount) || !d.activeMeshGroups.empty() ||
     !activeSkinnedMeshes.empty()||!activeTransparentMeshGroups.empty() || !d.activeOverlayMeshGroups.empty();
 if (hasWorldPass && !useDLSS && !d.depth)
   d.CreateDepth();
 if (hasWorldPass && !useDLSS && !d.hdrSceneColor)
   d.CreateHdrSceneColor();
  const bool useAmbientOcclusion=ambientOcclusionEnabled&&(!d.activeMeshGroups.empty()||!activeSkinnedMeshes.empty()||worldCount&&worldIndexCount)&&inverseViewProjectionValid;
 if(useAmbientOcclusion&&!d.ambientOcclusionRaw)d.CreateAmbientOcclusion();
 D3D12_RESOURCE_BARRIER toRT{};
 toRT.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
 toRT.Transition = {
     d.targets[d.frame].Get(), D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
     D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET};
 d.list->ResourceBarrier(1, &toRT);
 D3D12_CPU_DESCRIPTOR_HANDLE
     rtv = d.rtvHeap->GetCPUDescriptorHandleForHeapStart(),
     dsv = useDLSS ? d.dlss.DepthStencilView()
                   : d.dsvHeap->GetCPUDescriptorHandleForHeapStart();
 rtv.ptr += SIZE_T(d.frame) * d.rtvStep;
 d.list->ClearRenderTargetView(rtv, c, 0, nullptr);
 d.list->OMSetRenderTargets(1, &rtv, FALSE, nullptr);
 D3D12_RESOURCE_BARRIER hdrWrite{};
 if (hasWorldPass) {
   if (useDLSS)
     d.dlss.BeginScene(d.list.Get(), c);
   else {
     hdrWrite.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
     hdrWrite.Transition = {d.hdrSceneColor.Get(),
                            D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
                            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
                            D3D12_RESOURCE_STATE_RENDER_TARGET};
     d.list->ResourceBarrier(1, &hdrWrite);
     D3D12_CPU_DESCRIPTOR_HANDLE hdrRtv =
         d.rtvHeap->GetCPUDescriptorHandleForHeapStart();
     hdrRtv.ptr += SIZE_T(Impl::Frames * 2) * d.rtvStep;
     d.list->ClearRenderTargetView(hdrRtv, c, 0, nullptr);
     d.list->ClearDepthStencilView(dsv, D3D12_CLEAR_FLAG_DEPTH, 1.f, 0, 0,
                                   nullptr);
     d.list->OMSetRenderTargets(1, &hdrRtv, FALSE, &dsv);
   }
 }
 D3D12_VIEWPORT vp{
     worldCanvas.x, worldCanvas.y, worldCanvas.width, worldCanvas.height, 0, 1};
 D3D12_RECT sr{LONG(worldCanvas.x), LONG(worldCanvas.y),
               LONG(worldCanvas.x + worldCanvas.width),
               LONG(worldCanvas.y + worldCanvas.height)};
 d.list->RSSetViewports(1, &vp);
 d.list->RSSetScissorRects(1, &sr);
 d.list->SetGraphicsRootSignature(d.root.Get());
 ID3D12DescriptorHeap *descriptorHeaps[] = {d.textureHeap.Get()};
 d.list->SetDescriptorHeaps(1, descriptorHeaps);
 d.list->SetGraphicsRootDescriptorTable(
     1, d.textureHeap->GetGPUDescriptorHandleForHeapStart());
 D3D12_GPU_DESCRIPTOR_HANDLE shadowSrv =
     d.textureHeap->GetGPUDescriptorHandleForHeapStart();
 shadowSrv.ptr += UINT64(d.activeShadowSrvDescriptor) * d.textureStep;
 d.list->SetGraphicsRootDescriptorTable(2, shadowSrv);
  D3D12_GPU_DESCRIPTOR_HANDLE ambientOcclusionSrv=d.textureHeap->GetGPUDescriptorHandleForHeapStart();ambientOcclusionSrv.ptr+=UINT64(useAmbientOcclusion?AmbientOcclusionBlurSrvDescriptor3D:AmbientOcclusionNullSrvDescriptor3D)*d.textureStep;d.list->SetGraphicsRootDescriptorTable(5,ambientOcclusionSrv);
  D3D12_GPU_DESCRIPTOR_HANDLE waterDepthSrv=d.textureHeap->GetGPUDescriptorHandleForHeapStart();waterDepthSrv.ptr+=UINT64(!useDLSS&&d.depth?AmbientOcclusionDepthSrvDescriptor3D:WaterDepthNullSrvDescriptor3D)*d.textureStep;d.list->SetGraphicsRootDescriptorTable(6,waterDepthSrv);
 d.list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
 D3D12_GPU_DESCRIPTOR_HANDLE sceneCbv=d.textureHeap->GetGPUDescriptorHandleForHeapStart(),shadowCbv=sceneCbv;
 if (hasWorldPass) {
   sceneCbv.ptr += UINT64(SceneCbvDescriptorBase3D + d.frame) * d.textureStep;
   shadowCbv.ptr += UINT64(ShadowCbvDescriptorBase3D + d.frame) * d.textureStep;
   d.list->SetGraphicsRootDescriptorTable(0, sceneCbv);
   d.list->SetGraphicsRootDescriptorTable(3, shadowCbv);
 }
 auto bindTextures = [&](MaterialTextureSetHandle3D handle) {
   D3D12_GPU_DESCRIPTOR_HANDLE descriptor =
       d.textureHeap->GetGPUDescriptorHandleForHeapStart();
   if (handle) {
     const auto found = d.residentMaterialTextureSets.find(handle.value);
     if (found != d.residentMaterialTextureSets.end())
       descriptor.ptr += UINT64(found->second.descriptorOffset) * d.textureStep;
   }
 d.list->SetGraphicsRootDescriptorTable(1, descriptor);
 };
 if(useAmbientOcclusion){
   // 先填入不透明深度，AO 才能在同一幀供正式 PBR 著色取樣。
   d.list->OMSetRenderTargets(0,nullptr,FALSE,&dsv);d.list->SetGraphicsRootSignature(d.ambientOcclusionRoot.Get());d.list->SetGraphicsRootDescriptorTable(0,sceneCbv);d.list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
   for(const ActiveMeshGroup3D& group:d.activeMeshGroups){d.list->SetPipelineState(dl.BackfaceCulling()&&!group.doubleSided?d.ambientOcclusionDepthInstancedCulledPipeline.Get():d.ambientOcclusionDepthInstancedPipeline.Get());D3D12_VERTEX_BUFFER_VIEW views[2]={group.mesh->vertexView,{d.instanceBuffers[d.frame]->GetGPUVirtualAddress()+group.instanceOffset*sizeof(GpuMeshInstance),UINT(group.instanceCount*sizeof(GpuMeshInstance)),sizeof(GpuMeshInstance)}};d.list->IASetVertexBuffers(0,2,views);d.list->IASetIndexBuffer(&group.mesh->indexView);d.list->DrawIndexedInstanced(group.mesh->indexCount,UINT(group.instanceCount),0,0,0);}
   if(!activeSkinnedMeshes.empty())d.list->SetGraphicsRootShaderResourceView(3,d.skinPaletteBuffers[d.frame]->GetGPUVirtualAddress());for(const ActiveSkinnedMesh3D& mesh:activeSkinnedMeshes){d.list->SetPipelineState(d.ambientOcclusionDepthSkinnedPipeline.Get());D3D12_VERTEX_BUFFER_VIEW views[2]={mesh.mesh->vertexView,{d.skinnedInstanceBuffers[d.frame]->GetGPUVirtualAddress()+mesh.instanceOffset*sizeof(GpuSkinnedInstance),UINT(sizeof(GpuSkinnedInstance)),sizeof(GpuSkinnedInstance)}};d.list->IASetVertexBuffers(0,2,views);d.list->IASetIndexBuffer(&mesh.mesh->indexView);d.list->DrawIndexedInstanced(mesh.indexCount,1,mesh.firstIndex,0,0);}
   if(worldCount&&worldIndexCount){d.list->SetPipelineState(d.ambientOcclusionDepthWorldPipeline.Get());d.list->IASetVertexBuffers(0,1,&d.worldVbv[d.frame]);d.list->IASetIndexBuffer(&d.worldIbv[d.frame]);d.list->DrawIndexedInstanced(UINT(worldIndexCount),1,0,0,0);}++d.stats.ambientOcclusionDepthPrepasses;
   D3D12_RESOURCE_BARRIER barriers[2]{};barriers[0].Type=D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;barriers[0].Transition={d.depth.Get(),D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,D3D12_RESOURCE_STATE_DEPTH_WRITE,D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE};barriers[1].Type=D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;barriers[1].Transition={d.ambientOcclusionRaw.Get(),D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,D3D12_RESOURCE_STATE_RENDER_TARGET};d.list->ResourceBarrier(2,barriers);
   const D3D12_VIEWPORT aoViewport{0,0,float(d.stats.ambientOcclusionWidth),float(d.stats.ambientOcclusionHeight),0,1};const D3D12_RECT aoScissor{0,0,LONG(d.stats.ambientOcclusionWidth),LONG(d.stats.ambientOcclusionHeight)};d.list->RSSetViewports(1,&aoViewport);d.list->RSSetScissorRects(1,&aoScissor);D3D12_CPU_DESCRIPTOR_HANDLE aoRawRtv=d.rtvHeap->GetCPUDescriptorHandleForHeapStart();aoRawRtv.ptr+=SIZE_T(Impl::Frames*2+1)*d.rtvStep;d.list->OMSetRenderTargets(1,&aoRawRtv,FALSE,nullptr);const float white[4]={1,1,1,1};d.list->ClearRenderTargetView(aoRawRtv,white,0,nullptr);d.list->SetGraphicsRootSignature(d.ambientOcclusionRoot.Get());d.list->SetGraphicsRootDescriptorTable(0,sceneCbv);D3D12_GPU_DESCRIPTOR_HANDLE aoInputs=d.textureHeap->GetGPUDescriptorHandleForHeapStart();aoInputs.ptr+=UINT64(AmbientOcclusionDepthSrvDescriptor3D)*d.textureStep;d.list->SetGraphicsRootDescriptorTable(1,aoInputs);
   std::array<float,32> aoConstants{};memcpy(aoConstants.data(),&inverseViewProjection.values[0][0],sizeof(float)*16);aoConstants[16]=1.f/float(d.width);aoConstants[17]=1.f/float(d.height);aoConstants[18]=1.f/float(d.stats.ambientOcclusionWidth);aoConstants[19]=1.f/float(d.stats.ambientOcclusionHeight);const AmbientOcclusionSettings3D& aoSettings=dl.AmbientOcclusion();aoConstants[20]=aoSettings.radius;aoConstants[21]=aoSettings.bias;aoConstants[22]=aoSettings.intensity;aoConstants[23]=1.25f;aoConstants[24]=worldCanvas.height*.75f;aoConstants[25]=32.f;aoConstants[28]=worldCanvas.x/float(rw);aoConstants[29]=worldCanvas.y/float(rh);aoConstants[30]=worldCanvas.width/float(rw);aoConstants[31]=worldCanvas.height/float(rh);d.list->SetGraphicsRoot32BitConstants(2,32,aoConstants.data(),0);d.list->SetPipelineState(d.ambientOcclusionPipeline.Get());d.list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);d.list->DrawInstanced(3,1,0,0);++d.stats.ambientOcclusionEvaluationPasses;
   barriers[0].Transition={d.ambientOcclusionRaw.Get(),D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,D3D12_RESOURCE_STATE_RENDER_TARGET,D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE};barriers[1].Transition={d.ambientOcclusionBlur.Get(),D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,D3D12_RESOURCE_STATE_RENDER_TARGET};d.list->ResourceBarrier(2,barriers);D3D12_CPU_DESCRIPTOR_HANDLE aoBlurRtv=aoRawRtv;aoBlurRtv.ptr+=d.rtvStep;d.list->OMSetRenderTargets(1,&aoBlurRtv,FALSE,nullptr);d.list->ClearRenderTargetView(aoBlurRtv,white,0,nullptr);aoConstants[24]=5.f;aoConstants[25]=aoConstants[26]=aoConstants[27]=0.f;d.list->SetGraphicsRoot32BitConstants(2,32,aoConstants.data(),0);d.list->SetPipelineState(d.ambientOcclusionBlurPipeline.Get());d.list->DrawInstanced(3,1,0,0);++d.stats.ambientOcclusionBlurPasses;
    barriers[0].Transition={d.ambientOcclusionBlur.Get(),D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,D3D12_RESOURCE_STATE_RENDER_TARGET,D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE};barriers[1].Transition={d.depth.Get(),D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,D3D12_RESOURCE_STATE_DEPTH_WRITE};d.list->ResourceBarrier(2,barriers);D3D12_CPU_DESCRIPTOR_HANDLE hdrRtv=d.rtvHeap->GetCPUDescriptorHandleForHeapStart();hdrRtv.ptr+=SIZE_T(Impl::Frames*2)*d.rtvStep;d.list->OMSetRenderTargets(1,&hdrRtv,FALSE,&dsv);d.list->RSSetViewports(1,&vp);d.list->RSSetScissorRects(1,&sr);d.list->SetGraphicsRootSignature(d.root.Get());d.list->SetGraphicsRootDescriptorTable(0,sceneCbv);d.list->SetGraphicsRootDescriptorTable(1,d.textureHeap->GetGPUDescriptorHandleForHeapStart());d.list->SetGraphicsRootDescriptorTable(2,shadowSrv);d.list->SetGraphicsRootDescriptorTable(3,shadowCbv);d.list->SetGraphicsRootDescriptorTable(5,ambientOcclusionSrv);d.list->SetGraphicsRootDescriptorTable(6,waterDepthSrv);d.list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
 }
 for (const ActiveMeshGroup3D &group : d.activeMeshGroups) {
   bindTextures(group.textures);
   d.list->SetPipelineState(useAmbientOcclusion?(dl.BackfaceCulling()&&!group.doubleSided?d.ambientOcclusionInstancedCulledColorPipeline.Get():d.ambientOcclusionInstancedColorPipeline.Get()):(dl.BackfaceCulling()&&!group.doubleSided?d.instancedCulledPipeline.Get():d.instancedPipeline.Get()));
   D3D12_VERTEX_BUFFER_VIEW views[2] = {
       group.mesh->vertexView,
       {d.instanceBuffers[d.frame]->GetGPUVirtualAddress() +
            group.instanceOffset * sizeof(GpuMeshInstance),
        UINT(group.instanceCount * sizeof(GpuMeshInstance)),
        sizeof(GpuMeshInstance)}};
   d.list->IASetVertexBuffers(0, 2, views);
   d.list->IASetIndexBuffer(&group.mesh->indexView);
   d.list->DrawIndexedInstanced(group.mesh->indexCount,
                                UINT(group.instanceCount), 0, 0, 0);
 }
 if(!activeSkinnedMeshes.empty())d.list->SetGraphicsRootShaderResourceView(7,d.skinPaletteBuffers[d.frame]->GetGPUVirtualAddress());
 d.stats.lastSkinnedMaterialDescriptorBinds=0;
 for(const ActiveSkinnedMesh3D& mesh:activeSkinnedMeshes){bindTextures(mesh.textures);if(mesh.textures){++d.stats.lastSkinnedMaterialDescriptorBinds;++d.stats.totalSkinnedMaterialDescriptorBinds;}d.list->SetPipelineState(dl.BackfaceCulling()&&!mesh.doubleSided?d.skinnedCulledPipeline.Get():d.skinnedPipeline.Get());D3D12_VERTEX_BUFFER_VIEW views[2]={mesh.mesh->vertexView,{d.skinnedInstanceBuffers[d.frame]->GetGPUVirtualAddress()+mesh.instanceOffset*sizeof(GpuSkinnedInstance),UINT(sizeof(GpuSkinnedInstance)),sizeof(GpuSkinnedInstance)}};d.list->IASetVertexBuffers(0,2,views);d.list->IASetIndexBuffer(&mesh.mesh->indexView);d.list->DrawIndexedInstanced(mesh.indexCount,1,mesh.firstIndex,0,0);}
 if (worldCount && worldIndexCount) {
   bindTextures({});
   d.list->SetPipelineState(useAmbientOcclusion?d.ambientOcclusionWorldColorPipeline.Get():d.worldPipeline.Get());
   d.list->IASetVertexBuffers(0, 1, &d.worldVbv[d.frame]);
   d.list->IASetIndexBuffer(&d.worldIbv[d.frame]);
   d.list->DrawIndexedInstanced(UINT(worldIndexCount), 1, 0, 0, 0);
 }
  const bool useDepthAwareWater=!useDLSS&&d.depth&&inverseViewProjectionValid&&!activeTransparentMeshGroups.empty();
  d.stats.depthAwareWaterEnabled=useDepthAwareWater;
  if(useDepthAwareWater){D3D12_RESOURCE_BARRIER depthRead{};depthRead.Type=D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;depthRead.Transition={d.depth.Get(),D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,D3D12_RESOURCE_STATE_DEPTH_WRITE,D3D12_RESOURCE_STATE_DEPTH_READ|D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE};d.list->ResourceBarrier(1,&depthRead);D3D12_CPU_DESCRIPTOR_HANDLE hdrRtv=d.rtvHeap->GetCPUDescriptorHandleForHeapStart();hdrRtv.ptr+=SIZE_T(Impl::Frames*2)*d.rtvStep;D3D12_CPU_DESCRIPTOR_HANDLE readOnlyDsv=d.dsvHeap->GetCPUDescriptorHandleForHeapStart();readOnlyDsv.ptr+=d.device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);d.list->OMSetRenderTargets(1,&hdrRtv,FALSE,&readOnlyDsv);++d.stats.depthAwareWaterPasses;}
  for (const ActiveMeshGroup3D &group : activeTransparentMeshGroups) {
   bindTextures(group.textures);
   d.list->SetPipelineState(d.instancedTransparentPipeline.Get());
   D3D12_VERTEX_BUFFER_VIEW views[2] = {
       group.mesh->vertexView,
       {d.instanceBuffers[d.frame]->GetGPUVirtualAddress() +
            group.instanceOffset * sizeof(GpuMeshInstance),
        UINT(group.instanceCount * sizeof(GpuMeshInstance)),
        sizeof(GpuMeshInstance)}};
   d.list->IASetVertexBuffers(0, 2, views);
   d.list->IASetIndexBuffer(&group.mesh->indexView);
   d.list->DrawIndexedInstanced(group.mesh->indexCount,
                                UINT(group.instanceCount), 0, 0, 0);
  }
  if(useDepthAwareWater){D3D12_RESOURCE_BARRIER depthWrite{};depthWrite.Type=D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;depthWrite.Transition={d.depth.Get(),D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,D3D12_RESOURCE_STATE_DEPTH_READ|D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,D3D12_RESOURCE_STATE_DEPTH_WRITE};d.list->ResourceBarrier(1,&depthWrite);D3D12_CPU_DESCRIPTOR_HANDLE hdrRtv=d.rtvHeap->GetCPUDescriptorHandleForHeapStart();hdrRtv.ptr+=SIZE_T(Impl::Frames*2)*d.rtvStep;d.list->OMSetRenderTargets(1,&hdrRtv,FALSE,&dsv);}
 for (const ActiveMeshGroup3D &group : d.activeOverlayMeshGroups) {
   bindTextures(group.textures);
   d.list->SetPipelineState(d.instancedOverlayPipeline.Get());
   D3D12_VERTEX_BUFFER_VIEW views[2] = {
       group.mesh->vertexView,
       {d.instanceBuffers[d.frame]->GetGPUVirtualAddress() +
            group.instanceOffset * sizeof(GpuMeshInstance),
        UINT(group.instanceCount * sizeof(GpuMeshInstance)),
        sizeof(GpuMeshInstance)}};
   d.list->IASetVertexBuffers(0, 2, views);
   d.list->IASetIndexBuffer(&group.mesh->indexView);
   d.list->DrawIndexedInstanced(group.mesh->indexCount,
                                UINT(group.instanceCount), 0, 0, 0);
 }
 const CanvasViewport outputCanvas =
     CalculateCanvasViewport({float(d.logicalWidth), float(d.logicalHeight)},
                             {float(d.width), float(d.height)});
 if (hasWorldPass) {
   D3D12_GPU_DESCRIPTOR_HANDLE hdrSrv =
       d.textureHeap->GetGPUDescriptorHandleForHeapStart();
   float sourceRect[4]{};
   if (useDLSS) {
     const bool evaluated = d.dlss.Evaluate(d.list.Get());
     dlssEvaluationFailed = !evaluated;
     hdrSrv.ptr += UINT64(evaluated ? DlssOutputSrvDescriptor3D
                                    : DlssInputSrvDescriptor3D) *
                   d.textureStep;
     if (evaluated) {
       const float scaleX = float(d.width) / d.dlss.RenderWidth(),
                   scaleY = float(d.height) / d.dlss.RenderHeight();
       sourceRect[0] = worldCanvas.x * scaleX;
       sourceRect[1] = worldCanvas.y * scaleY;
       sourceRect[2] = worldCanvas.width * scaleX;
       sourceRect[3] = worldCanvas.height * scaleY;
     } else {
       D3D12_RESOURCE_BARRIER fallbackRead{};
       fallbackRead.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
       fallbackRead.Transition = {d.dlss.InputResource(),
                                  D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
                                  D3D12_RESOURCE_STATE_RENDER_TARGET,
                                  D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE};
       d.list->ResourceBarrier(1, &fallbackRead);
       sourceRect[0] = worldCanvas.x;
       sourceRect[1] = worldCanvas.y;
       sourceRect[2] = worldCanvas.width;
       sourceRect[3] = worldCanvas.height;
     }
   } else {
     std::swap(hdrWrite.Transition.StateBefore, hdrWrite.Transition.StateAfter);
     d.list->ResourceBarrier(1, &hdrWrite);
     hdrSrv.ptr += UINT64(HdrSceneSrvDescriptor3D) * d.textureStep;
     sourceRect[0] = worldCanvas.x;
     sourceRect[1] = worldCanvas.y;
     sourceRect[2] = worldCanvas.width;
     sourceRect[3] = worldCanvas.height;
   }
   D3D12_VIEWPORT outputViewport{outputCanvas.x,
                                 outputCanvas.y,
                                 outputCanvas.width,
                                 outputCanvas.height,
                                 0,
                                 1};
   D3D12_RECT outputScissor{LONG(outputCanvas.x), LONG(outputCanvas.y),
                            LONG(outputCanvas.x + outputCanvas.width),
                            LONG(outputCanvas.y + outputCanvas.height)};
   d.list->RSSetViewports(1, &outputViewport);
   d.list->RSSetScissorRects(1, &outputScissor);
   d.list->OMSetRenderTargets(1, &rtv, FALSE, nullptr);
   d.list->SetGraphicsRootSignature(d.postProcessRoot.Get());
   d.list->SetGraphicsRootDescriptorTable(0, hdrSrv);
   const float compositeConstants[8] = {
       sourceRect[0],  sourceRect[1],  sourceRect[2],      sourceRect[3],
       outputCanvas.x, outputCanvas.y, outputCanvas.width, outputCanvas.height};
   d.list->SetGraphicsRoot32BitConstants(1, 8, compositeConstants, 0);
   d.list->SetPipelineState(d.postProcessPipeline.Get());
   d.list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
   d.list->DrawInstanced(3, 1, 0, 0);
   ++d.stats.hdrPostProcessPasses;
   if (useDLSS) {
     if (!dlssEvaluationFailed)
       d.dlss.FinishComposite(d.list.Get());
     else {
       D3D12_RESOURCE_BARRIER fallbackWrite{};
       fallbackWrite.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
       fallbackWrite.Transition = {d.dlss.InputResource(),
                                   D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
                                   D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
                                   D3D12_RESOURCE_STATE_RENDER_TARGET};
       d.list->ResourceBarrier(1, &fallbackWrite);
     }
   }
 }
 if (count) {
   D3D12_VIEWPORT outputViewport{outputCanvas.x,
                                 outputCanvas.y,
                                 outputCanvas.width,
                                 outputCanvas.height,
                                 0,
                                 1};
   D3D12_RECT outputScissor{LONG(outputCanvas.x), LONG(outputCanvas.y),
                            LONG(outputCanvas.x + outputCanvas.width),
                            LONG(outputCanvas.y + outputCanvas.height)};
   d.list->RSSetViewports(1, &outputViewport);
   d.list->RSSetScissorRects(1, &outputScissor);
   d.list->OMSetRenderTargets(1, &rtv, FALSE, nullptr);
   d.list->SetGraphicsRootSignature(d.root.Get());
   d.list->SetDescriptorHeaps(1, descriptorHeaps);
   const float logicalSize[2] = {float(d.logicalWidth), float(d.logicalHeight)};
   d.list->SetGraphicsRoot32BitConstants(4, 2, logicalSize, 0);
   d.list->IASetVertexBuffers(0, 1, &d.vbv[d.frame]);
   forEachCanvasSpan(
       [&](size_t firstVertex, size_t vertexCount, bool additive) {
         if (!vertexCount)
           return;
         d.list->SetPipelineState(additive ? d.additivePipeline.Get()
                                           : d.pipeline.Get());
         d.list->DrawInstanced(UINT(vertexCount), 1, UINT(firstVertex), 0);
       });
 }
 ComPtr<ID3D12Resource> captureReadback;D3D12_PLACED_SUBRESOURCE_FOOTPRINT captureFootprint{};UINT64 captureBytes{};
 if(!capturePath.empty()){const D3D12_RESOURCE_DESC targetDescription=d.targets[d.frame]->GetDesc();UINT rowCount{};UINT64 rowBytes{};d.device->GetCopyableFootprints(&targetDescription,0,1,0,&captureFootprint,&rowCount,&rowBytes,&captureBytes);D3D12_HEAP_PROPERTIES heap{};heap.Type=D3D12_HEAP_TYPE_READBACK;D3D12_RESOURCE_DESC buffer{};buffer.Dimension=D3D12_RESOURCE_DIMENSION_BUFFER;buffer.Width=captureBytes;buffer.Height=1;buffer.DepthOrArraySize=1;buffer.MipLevels=1;buffer.SampleDesc.Count=1;buffer.Layout=D3D12_TEXTURE_LAYOUT_ROW_MAJOR;Check(d.device->CreateCommittedResource(&heap,D3D12_HEAP_FLAG_NONE,&buffer,D3D12_RESOURCE_STATE_COPY_DEST,nullptr,IID_PPV_ARGS(&captureReadback)),"Frame capture readback failed");D3D12_RESOURCE_BARRIER toCopy{};toCopy.Type=D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;toCopy.Transition={d.targets[d.frame].Get(),D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,D3D12_RESOURCE_STATE_RENDER_TARGET,D3D12_RESOURCE_STATE_COPY_SOURCE};d.list->ResourceBarrier(1,&toCopy);D3D12_TEXTURE_COPY_LOCATION destination{};destination.pResource=captureReadback.Get();destination.Type=D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;destination.PlacedFootprint=captureFootprint;D3D12_TEXTURE_COPY_LOCATION source{};source.pResource=d.targets[d.frame].Get();source.Type=D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;source.SubresourceIndex=0;d.list->CopyTextureRegion(&destination,0,0,0,&source,nullptr);toCopy.Transition.StateBefore=D3D12_RESOURCE_STATE_COPY_SOURCE;toCopy.Transition.StateAfter=D3D12_RESOURCE_STATE_PRESENT;d.list->ResourceBarrier(1,&toCopy);}else{std::swap(toRT.Transition.StateBefore,toRT.Transition.StateAfter);d.list->ResourceBarrier(1,&toRT);}d.list->EndQuery(d.timestampHeap.Get(),D3D12_QUERY_TYPE_TIMESTAMP,timestampStart+1);d.list->ResolveQueryData(d.timestampHeap.Get(),D3D12_QUERY_TYPE_TIMESTAMP,timestampStart,2,d.timestampReadback.Get(),sizeof(uint64_t)*timestampStart);Check(d.list->Close(),"Close failed");ID3D12CommandList* lists[]={d.list.Get()};d.queue->ExecuteCommandLists(1,lists);const unsigned submittedFrame=d.frame;d.timestampValid[submittedFrame]=true;Check(d.swap->Present(1,0),"Present failed");const UINT64 submittedFence=d.SignalFrame(submittedFrame);d.frame=d.swap->GetCurrentBackBufferIndex();if(captureReadback){d.WaitForValue(submittedFence);void* mapped{};const D3D12_RANGE readRange{0,SIZE_T(captureBytes)};if(SUCCEEDED(captureReadback->Map(0,&readRange,&mapped))){const std::span pixels{static_cast<const std::uint8_t*>(mapped),size_t(captureBytes)};const FrameCaptureAnalysis3D analysis=AnalyzeFrameCaptureRgba8(d.width,d.height,captureFootprint.Footprint.RowPitch,pixels);const bool plausibleExposure=analysis.valid&&analysis.averageLuminance>.01&&analysis.averageLuminance<.99&&analysis.luminanceVariance>.00001&&analysis.darkPixelFraction<.98&&analysis.brightPixelFraction<.98;const bool encoded=EncodeFrameCapturePng3D(capturePath,d.width,d.height,captureFootprint.Footprint.RowPitch,pixels);if(plausibleExposure&&encoded){++d.stats.successfulFrameCaptures;d.stats.lastCapturedAverageLuminance=analysis.averageLuminance;d.stats.lastCapturedLuminanceVariance=analysis.luminanceVariance;d.stats.lastCapturedDarkPixelFraction=analysis.darkPixelFraction;d.stats.lastCapturedBrightPixelFraction=analysis.brightPixelFraction;}else ++d.stats.rejectedFrameCaptures;const D3D12_RANGE writtenRange{};captureReadback->Unmap(0,&writtenRange);}else ++d.stats.rejectedFrameCaptures;}if(dlssEvaluationFailed){d.WaitForValue(submittedFence);d.dlss.Toggle();d.UpdateDlssSceneDescriptors();d.dlssValidationComplete=false;}else if(useDLSS&&!d.dlssValidationComplete){const uint64_t elapsed=d.WaitForValue(submittedFence);d.stats.lastValidationWaitNanoseconds=elapsed;if(elapsed){++d.stats.validationWaits;d.stats.totalValidationWaitNanoseconds+=elapsed;d.stats.peakValidationWaitNanoseconds=(std::max)(d.stats.peakValidationWaitNanoseconds,elapsed);}d.dlssValidationComplete=d.dlss.ValidateOutput();}
}
bool D3D12Renderer::CaptureNextFrame(const std::filesystem::path& path){if(!impl_||!impl_->device||path.empty())return false;impl_->pendingCapturePath=path;return true;}
void D3D12Renderer::WaitIdle(){if(impl_&&impl_->queue){impl_->Flush();for(unsigned index=0;index<Impl::Frames;++index)impl_->CollectGpuTiming(index);}}
void D3D12Renderer::Resize(unsigned width,unsigned height){if(!impl_||!width||!height)return;auto& d=*impl_;if(!d.swap||!d.device||width==d.width&&height==d.height)return;d.Flush();for(unsigned index=0;index<Impl::Frames;++index){d.CollectGpuTiming(index);d.targets[index].Reset();d.frameFenceValues[index]=0;}const bool recreateDepth=bool(d.depth),recreateHdrScene=bool(d.hdrSceneColor),recreateAmbientOcclusion=bool(d.ambientOcclusionRaw);d.ReleaseDepth();d.ReleaseHdrSceneColor();d.ReleaseAmbientOcclusion();Check(d.swap->ResizeBuffers(Impl::Frames,width,height,DXGI_FORMAT_R8G8B8A8_UNORM,0),"Swap chain resize failed");d.width=width;d.height=height;d.frame=d.swap->GetCurrentBackBufferIndex();d.Targets();if(recreateDepth&&!d.dlss.Enabled())d.CreateDepth();if(recreateHdrScene&&!d.dlss.Enabled())d.CreateHdrSceneColor();if(recreateAmbientOcclusion&&!d.dlss.Enabled())d.CreateAmbientOcclusion();if(d.dlss.Supported()){d.dlss.Resize(width,height);d.UpdateDlssSceneDescriptors();}d.dlssValidationComplete=false;d.stats.outputWidth=width;d.stats.outputHeight=height;d.stats.canvasViewport=CalculateCanvasViewport({float(d.logicalWidth),float(d.logicalHeight)},{float(width),float(height)});++d.stats.resizeCount;}
bool D3D12Renderer::SupportsDLSS()const{return impl_&&impl_->dlss.Supported();}bool D3D12Renderer::DLSSModuleRequested()const{return impl_&&impl_->dlssRequested;}bool D3D12Renderer::DLSSOn()const{return impl_&&impl_->dlss.Enabled();}void D3D12Renderer::ToggleDLSS(){if(impl_&&impl_->dlss.Supported()){if(impl_->queue)impl_->Flush();impl_->dlss.Toggle();if(impl_->dlss.Enabled()){impl_->ReleaseHdrSceneColor();impl_->ReleaseDepth();}impl_->UpdateDlssSceneDescriptors();impl_->dlssValidationComplete=false;}}RendererStats D3D12Renderer::GetStats()const{if(!impl_)return{};RendererStats result=impl_->stats;result.idleMeshUploadContexts=impl_->availableMeshUploadContexts.size();result.idleMaterialTextureUploadContexts=impl_->availableTextureUploadContexts.size();const StreamlineResourceStats dlss=impl_->dlss.Stats();result.dlssResidentBytes=dlss.residentBytes;result.dlssResourceAllocations=dlss.allocations;result.dlssResourceReleases=dlss.releases;result.dlssResourcesResident=dlss.resourcesResident;result.dlssModuleRequested=impl_->dlssRequested;result.dlssFrameworkLoaded=impl_->dlss.FrameworkLoaded();return result;}unsigned D3D12Renderer::RenderWidth()const{return impl_?(impl_->dlss.Enabled()?impl_->dlss.RenderWidth():impl_->width):0;}unsigned D3D12Renderer::RenderHeight()const{return impl_?(impl_->dlss.Enabled()?impl_->dlss.RenderHeight():impl_->height):0;}
}
