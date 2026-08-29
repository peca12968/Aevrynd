#pragma once
#include <windows.h>
#include <filesystem>
#include <memory>
#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include "Audio.h"
#include "Game.h"
#include "Input.h"
#include "Memory.h"
#include "MeshProcessing3D.h"
#include "Renderer.h"
#include "Scene2D.h"
#include "SkinnedModel3D.h"

namespace Aevrynd {
#ifndef AEVRYND_ENGINE_VERSION
#define AEVRYND_ENGINE_VERSION "development"
#endif
inline constexpr std::string_view EngineVersion{AEVRYND_ENGINE_VERSION};
[[nodiscard]] bool CommandLineHasOption(std::wstring_view commandLine,std::wstring_view option);
[[nodiscard]] bool IsDLSSStartupRequested(const wchar_t* commandLine);
struct MeshLodResourceChain3D {
    std::array<MeshResource3D,MaximumGeneratedMeshLodLevels3D+1> levels{};
    std::uint32_t levelCount{};
    MeshLodChainStats3D processing{};
    MeshLodResourceChain3D()=default;
    MeshLodResourceChain3D(const MeshLodResourceChain3D&)=delete;
    MeshLodResourceChain3D& operator=(const MeshLodResourceChain3D&)=delete;
    MeshLodResourceChain3D(MeshLodResourceChain3D&& other)noexcept:levels(std::move(other.levels)),levelCount(other.levelCount),processing(other.processing){other.levels={};other.levelCount=0;other.processing={};}
    MeshLodResourceChain3D& operator=(MeshLodResourceChain3D&&)=delete;
    explicit operator bool()const{return levelCount>=2&&bool(levels[0]);}
};
struct SkinnedMeshResource3D {
    SkinnedMeshHandle3D handle{};
    MeshBounds3D bounds{};
    std::size_t vertexCount{},indexCount{},jointCount{};
    explicit operator bool()const{return bool(handle)&&vertexCount&&indexCount&&jointCount;}
};
enum class MeshCpuRetention3D:std::uint8_t {Discard,RetainForQueries};
enum class MeshLodCpuRetention3D:std::uint8_t {None,Level0Only,AllLevels};
struct MeshCpuMemoryStats3D {std::size_t residentResources{},residentBytes{},cacheEntries{},expiredCacheEntries{};};
enum class RenderQuality3D:std::uint8_t {Low,Medium,High,Ultra};
constexpr std::string_view RenderQualityName3D(RenderQuality3D quality){switch(quality){case RenderQuality3D::Low:return"LOW";case RenderQuality3D::Medium:return"MEDIUM";case RenderQuality3D::High:return"HIGH";default:return"ULTRA";}}
constexpr RenderQuality3D NextRenderQuality3D(RenderQuality3D quality){return quality==RenderQuality3D::Low?RenderQuality3D::Medium:quality==RenderQuality3D::Medium?RenderQuality3D::High:quality==RenderQuality3D::High?RenderQuality3D::Ultra:RenderQuality3D::Low;}
struct EngineConfig { std::wstring title=L"Aevrynd Engine"; unsigned width=1280,height=720; float fixedStep=1.f/60.f; bool resizable=true,enableDLSS=false,enableAutomaticMemoryTrim=true; unsigned memoryPressureThresholdPercent=85; float memoryTrimCooldownSeconds=10.f; size_t minimumAutomaticMemoryReclaimBytes=512*1024; RenderQuality3D renderQuality{RenderQuality3D::Ultra}; };
struct EngineMemoryStats {
    ProcessMemoryStats process{};
    TransientMemoryStats draw{};
    TransientMemoryStats renderer{};
    ReusableMemoryStats audio{};
    ReusableMemoryStats game{};
    MeshCpuMemoryStats3D meshCpu{};
    size_t lastReleasedBytes{};
    uint64_t trimCount{};
    uint64_t automaticTrimCount{};
};
class Engine {
public:
    explicit Engine(EngineConfig config={}); ~Engine();
    int Run(std::unique_ptr<IGame> game,HINSTANCE instance,int showCommand);
    DrawList& Draw(){return draw_;}
    Input& GetInput(){return input_;}
    AudioEngine& Audio(){return audio_;}
    const AudioEngine& Audio()const{return audio_;}
    unsigned Width()const{return config_.width;}
    unsigned Height()const{return config_.height;}
    RenderQuality3D RenderQuality()const{return config_.renderQuality;}
    void SetRenderQuality(RenderQuality3D quality);
    float Time()const{return time_;}
    void Quit(){running_=false;}
    void SetTitle(const std::wstring& title);
    void SetClearColor(Color color){clearColor_=color;}
    Color ClearColor()const{return clearColor_;}
    std::filesystem::path ExecutableDirectory()const;
    HWND Window()const{return hwnd_;}
    bool SupportsDLSS()const{return renderer_->SupportsDLSS();}
    bool DLSSModuleRequested()const{return renderer_->DLSSModuleRequested();}
    bool DLSSOn()const{return renderer_->DLSSOn();}
    void ToggleDLSS(){renderer_->ToggleDLSS();}
    MeshHandle3D CreateMesh3D(const Mesh3D& mesh);
    MeshResource3D CreateMeshResource3D(const Mesh3D& mesh,MeshCpuRetention3D retention=MeshCpuRetention3D::RetainForQueries);
    SkinnedMeshResource3D CreateSkinnedMeshResource3D(const SkinnedModel3D& model);
    bool CreateMeshLodResourceChain3D(const Mesh3D& mesh,MeshLodResourceChain3D& output,MeshLodChainSettings3D settings={},MeshProcessingError3D* error=nullptr,MeshLodCpuRetention3D retention=MeshLodCpuRetention3D::Level0Only);
    void DestroyMeshLodResourceChain3D(MeshLodResourceChain3D& chain);
    void DestroyMesh3D(MeshHandle3D mesh){renderer_->DestroyMesh(mesh);}
    void DestroySkinnedMesh3D(SkinnedMeshHandle3D mesh){renderer_->DestroySkinnedMesh(mesh);}
    void SetMeshMemoryBudget(size_t bytes){renderer_->SetMeshMemoryBudget(bytes);}
    MaterialTextureSetHandle3D CreateMaterialTextureSet3D(const MaterialTextureSetDefinition3D& definition){return renderer_->CreateMaterialTextureSet(definition);}
    [[nodiscard]] std::optional<size_t> EstimateMaterialTextureSetAllocationBytes3D(const MaterialTextureSetDefinition3D& definition)const{return renderer_->EstimateMaterialTextureSetAllocationBytes3D(definition);}
    void DestroyMaterialTextureSet3D(MaterialTextureSetHandle3D textures){renderer_->DestroyMaterialTextureSet(textures);}
    void SetMaterialTextureMemoryBudget(size_t bytes){renderer_->SetMaterialTextureMemoryBudget(bytes);}
    size_t TrimUnusedMemory();
    [[nodiscard]] EngineMemoryStats GetMemoryStats()const;
    RendererStats GetRendererStats()const{return renderer_->GetStats();}
    GpuUploadReceipt3D SubmitPendingUploads3D(){return renderer_->SubmitPendingUploads3D();}
    bool IsUploadComplete3D(GpuUploadReceipt3D receipt)const{return renderer_->IsUploadComplete(receipt);}
    void WaitRendererIdle3D(){renderer_->WaitIdle();}
    SkinnedSubmissionResult3D QuerySkinnedSubmission(SkinnedSubmissionReceipt3D receipt)const{return renderer_->QuerySkinnedSubmission(receipt);}
    unsigned RenderWidth()const{return renderer_->RenderWidth();}
    unsigned RenderHeight()const{return renderer_->RenderHeight();}
    bool CaptureNextFrame3D(const std::filesystem::path& path){return renderer_->CaptureNextFrame(path);}
    bool RebaseReflectionProbeCaptures3D(Vec3 localDelta,uint64_t originGeneration){return renderer_&&renderer_->RebaseReflectionProbeCaptures3D(localDelta,originGeneration);}
private:
    static LRESULT CALLBACK WindowProc(HWND,UINT,WPARAM,LPARAM);
    LRESULT HandleMessage(UINT,WPARAM,LPARAM);
    EngineConfig config_; HWND hwnd_{}; bool running_{true},resizing_{},minimized_{},trimPending_{},automaticTrimmed_{}; float time_{},lastMemorySampleTime_{-1.f};
    Input input_; AudioEngine audio_; DrawList draw_; std::unique_ptr<D3D12Renderer> renderer_; std::unique_ptr<IGame> game_;
    std::unordered_map<uint32_t,std::weak_ptr<const Mesh3D>> meshCpuCache_;
    ProcessMemoryStats processMemoryStats_{};size_t lastReleasedBytes_{};uint64_t trimCount_{},automaticTrimCount_{};float lastAutomaticTrimTime_{};
    Color clearColor_{.018f,.045f,.055f,1};
};
}
