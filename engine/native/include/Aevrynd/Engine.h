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
#include "DevelopmentUltraQuality3D.h"
#include "Game.h"
#include "Input.h"
#include "Memory.h"
#include "MeshProcessing3D.h"
#include "Renderer.h"
#include "Scene2D.h"
#include "SkinnedModel3D.h"

namespace Aevrynd {
struct EngineDevelopmentUltraTestAccess3D;
#if AEVRYND_INTERNAL_RENDERER_TEST_HOOKS
struct EngineRendererTestAccess3D;
#endif
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

enum class EnvironmentLightingGpuOperation3D : std::uint8_t {
    Create,
    Set,
    Clear,
    Destroy,
    Trim
};

struct EnvironmentLightingGpuReceipt3D {
    std::uint64_t sequence{};
    EnvironmentLightingGpuOperation3D operation{};
    EnvironmentLightingHandle3D handle{};
    std::size_t residentSetsBefore{}, residentSetsAfter{};
    std::size_t residentBytesBefore{}, residentBytesAfter{};
    std::size_t descriptorsBefore{}, descriptorsAfter{};
    std::size_t pendingReleaseBytesAfter{};
    bool rendererAccepted{};
    bool activeAfter{};
    explicit operator bool() const noexcept { return sequence != 0; }
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
constexpr std::string_view RenderQualityName3D(RenderQuality3D quality){switch(quality){case RenderQuality3D::Low:return"LOW";case RenderQuality3D::Medium:return"MEDIUM";case RenderQuality3D::High:return"HIGH";case RenderQuality3D::Ultra:return"ULTRA";default:return"UNKNOWN";}}
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
enum class DevelopmentUltraTerminalOutcome3D : std::uint8_t {
    InProgress,
    Committed,
    RolledBack,
    RollbackFailed,
};
enum class DevelopmentUltraTerminalReason3D : std::uint8_t {
    None,
    CommitSucceeded,
    RollbackSucceeded,
    RendererRestoreFailed,
    PendingGpuReleasesAfterRollback,
};
struct DevelopmentUltraTerminalState3D {
    DevelopmentUltraTerminalOutcome3D outcome{DevelopmentUltraTerminalOutcome3D::InProgress};
    DevelopmentUltraTerminalReason3D reason{DevelopmentUltraTerminalReason3D::None};
    std::uint64_t epoch{};
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
    RenderQuality3D RequestedRenderQuality()const{return requestedRenderQuality_;}
    const DevelopmentUltraReceipt3D& DevelopmentUltraReceipt()const{return developmentUltraReceipt_;}
    const DevelopmentUltraGpuTruthSnapshot3D& DevelopmentUltraGpuTruth()const{return developmentUltraReceipt_.gpuTruth;}
    const DevelopmentUltraTerminalState3D& DevelopmentUltraTerminalState()const{return developmentUltraTerminalState_;}
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
    bool CreateEnvironmentLighting3D(const EnvironmentLightingStore3D& store,
        EnvironmentLightingHandle3D handle,EnvironmentLightingGpuReceipt3D* receipt=nullptr);
    bool SetEnvironmentLighting3D(const EnvironmentLightingStore3D& store,
        EnvironmentLightingHandle3D handle,
        EnvironmentLightingGpuReceipt3D* receipt=nullptr);
    bool ClearEnvironmentLighting3D(EnvironmentLightingGpuReceipt3D* receipt=nullptr);
    bool DestroyEnvironmentLighting3D(const EnvironmentLightingStore3D& store,
        EnvironmentLightingHandle3D handle,
        EnvironmentLightingGpuReceipt3D* receipt=nullptr);
    void TrimEnvironmentLighting3D(EnvironmentLightingGpuReceipt3D* receipt=nullptr);
    [[nodiscard]] EnvironmentLightingHandle3D ActiveEnvironmentLighting3D()const{return renderer_->ActiveEnvironmentLighting();}
    size_t TrimUnusedMemory();
    [[nodiscard]] EngineMemoryStats GetMemoryStats()const;
    RendererStats GetRendererStats()const{return renderer_->GetStats();}
    [[nodiscard]] RendererRuntimeEpoch3D GetRendererRuntimeEpoch3D()const noexcept{return renderer_?renderer_->RuntimeEpoch3D():RendererRuntimeEpoch3D{};}
    void SetGpuInstanceCullingSettings3D(GpuInstanceCullingSettings3D settings){renderer_->SetGpuInstanceCullingSettings(settings);}
    void SetHzbOcclusionSettings3D(HzbOcclusionSettings3D settings){renderer_->SetHzbOcclusionSettings3D(settings);}
    GpuUploadReceipt3D SubmitPendingUploads3D(){return renderer_->SubmitPendingUploads3D();}
    bool IsUploadComplete3D(GpuUploadReceipt3D receipt)const{return renderer_->IsUploadComplete(receipt);}
    GpuRetirementReceipt3D CaptureRetirementFence3D(){return renderer_?renderer_->CaptureRetirementFence3D():GpuRetirementReceipt3D{};}
    bool IsRetirementComplete3D(GpuRetirementReceipt3D receipt)const{return renderer_&&renderer_->IsRetirementComplete3D(receipt);}
    void WaitRendererIdle3D(){renderer_->WaitIdle();}
    SkinnedSubmissionResult3D QuerySkinnedSubmission(SkinnedSubmissionReceipt3D receipt)const{return renderer_->QuerySkinnedSubmission(receipt);}
    unsigned RenderWidth()const{return renderer_->RenderWidth();}
    unsigned RenderHeight()const{return renderer_->RenderHeight();}
    bool CaptureNextFrame3D(const std::filesystem::path& path){return renderer_->CaptureNextFrame(path);}
    bool RequestObjectIdReadback3D(ObjectIdCaptureRequestMetadata3D metadata={}){return renderer_&&renderer_->RequestObjectIdReadback3D(metadata);}
    bool CancelObjectIdReadback3D()noexcept{return renderer_&&renderer_->CancelObjectIdReadback3D();}
    std::optional<ObjectIdCapture3D> TakeObjectIdReadback3D(){return renderer_?renderer_->TakeObjectIdReadback3D():std::optional<ObjectIdCapture3D>{};}
    bool RequestDepthReadback3D(DepthCaptureRequestMetadata3D metadata={}){return renderer_&&renderer_->RequestDepthReadback3D(metadata);}
    bool CancelDepthReadback3D()noexcept{return renderer_&&renderer_->CancelDepthReadback3D();}
    std::optional<DepthCapture3D> TakeDepthReadback3D(){return renderer_?renderer_->TakeDepthReadback3D():std::optional<DepthCapture3D>{};}
    bool RequestLinearHdrReadback3D(LinearHdrCaptureRequestMetadata3D metadata={});
    bool CancelLinearHdrReadback3D()noexcept;
    std::optional<LinearHdrReadback3D> TakeLinearHdrReadback3D();
    std::optional<ProductionReadbackBundle3D> TakeProductionReadbackBundle3D();
    std::optional<ReadbackCaptureSequenceToken3D> BeginReadbackCaptureSequence3D(ReadbackCaptureSequenceDescriptor3D descriptor){return renderer_?renderer_->BeginReadbackCaptureSequence3D(descriptor):std::optional<ReadbackCaptureSequenceToken3D>{};}
    std::optional<ReadbackCaptureSequenceToken3D> BeginReadbackCaptureSequence3D(uint32_t expectedPhaseCount){return BeginReadbackCaptureSequence3D(ReadbackCaptureSequenceDescriptor3D{expectedPhaseCount});}
    ReadbackCaptureSequenceResult3D EndReadbackCaptureSequence3D(ReadbackCaptureSequenceToken3D token){return renderer_?renderer_->EndReadbackCaptureSequence3D(token):ReadbackCaptureSequenceResult3D{ReadbackCaptureSequenceStatus3D::DeviceLost,{}};}
    ReadbackCaptureSequenceGpuProvenance3D ReadbackCaptureSequenceGpuProvenance(ReadbackCaptureSequenceToken3D token)const{return renderer_?renderer_->ReadbackCaptureSequenceGpuProvenance(token):ReadbackCaptureSequenceGpuProvenance3D{};}
    bool CancelReadbackCaptureSequence3D(ReadbackCaptureSequenceToken3D token)noexcept{return renderer_&&renderer_->CancelReadbackCaptureSequence3D(token);}
    bool PrepareRebaseReflectionProbeCaptures3D(Vec3 localDelta,uint64_t originGeneration){return renderer_&&renderer_->PrepareRebaseReflectionProbeCaptures3D(localDelta,originGeneration);}
    void CommitPreparedReflectionProbeCaptureRebase3D()noexcept{if(renderer_)renderer_->CommitPreparedReflectionProbeCaptureRebase3D();}
    void AbortPreparedReflectionProbeCaptureRebase3D()noexcept{if(renderer_)renderer_->AbortPreparedReflectionProbeCaptureRebase3D();}
    bool RebaseReflectionProbeCaptures3D(Vec3 localDelta,uint64_t originGeneration){return renderer_&&renderer_->RebaseReflectionProbeCaptures3D(localDelta,originGeneration);}
private:
    friend struct EngineDevelopmentUltraTestAccess3D;
#if AEVRYND_INTERNAL_RENDERER_TEST_HOOKS
    friend struct EngineRendererTestAccess3D;
#endif
    void RefreshDevelopmentUltraGpuTruth();
    bool ExitDevelopmentUltraToLastKnownGood();
    bool RestoreDevelopmentUltraRendererBaseline(bool trim);
    static LRESULT CALLBACK WindowProc(HWND,UINT,WPARAM,LPARAM);
    LRESULT HandleMessage(UINT,WPARAM,LPARAM);
    EngineConfig config_; RenderQuality3D requestedRenderQuality_{RenderQuality3D::High}; DevelopmentUltraReceipt3D developmentUltraReceipt_{}; DevelopmentUltraTerminalState3D developmentUltraTerminalState_{}; DevelopmentUltraQuality3D stagedDevelopmentUltraProfile_{}; DevelopmentUltraGpuTruthSequence3D developmentUltraGpuTruth_{}; DevelopmentUltraRendererBaseline3D lastKnownGoodRendererBaseline_{}; DirectionalShadowCascadeSettings3D lastKnownGoodShadowCascades_{}; std::uint64_t renderQualityEpoch_{}; std::uint8_t developmentUltraTestFailure_{},developmentUltraTestGpuTruthFailure_{}; bool developmentUltraTestFrameBudgetFailure_{},developmentUltraTestRollbackFailure_{},developmentUltraStaged_{},developmentUltraApplied_{}; HWND hwnd_{}; bool running_{true},resizing_{},minimized_{},trimPending_{},automaticTrimmed_{}; float time_{},lastMemorySampleTime_{-1.f};
    Input input_; AudioEngine audio_; DrawList draw_; std::unique_ptr<D3D12Renderer> renderer_; std::unique_ptr<IGame> game_;
    std::unordered_map<uint32_t,std::weak_ptr<const Mesh3D>> meshCpuCache_;
    struct EnvironmentLightingBinding3D {
        std::shared_ptr<const EnvironmentIblValidatedData3D> payload{};
        std::uint32_t references{};
    };
    std::unordered_map<std::uint64_t,EnvironmentLightingBinding3D> environmentLightingBindings_{};
    std::uint64_t environmentLightingReceiptSequence_{};
    ProcessMemoryStats processMemoryStats_{};size_t lastReleasedBytes_{};uint64_t trimCount_{},automaticTrimCount_{};float lastAutomaticTrimTime_{};
    Color clearColor_{.018f,.045f,.055f,1};
};
#if defined(AEVRYND_ENGINE_TEST_ACCESS)
struct EngineDevelopmentUltraTestAccess3D {
    enum class Failure : std::uint8_t {None,RendererApply,PostApplyPending,Admission,PreApplyPending};
    enum class GpuTruthFailure : std::uint8_t {None,AdapterIdentity,Validation,FrameBudget,Serial};
    static void ForceFailure(Engine& engine,Failure failure){engine.developmentUltraTestFailure_=static_cast<std::uint8_t>(failure);}
    static void ForceGpuTruthFailure(Engine& engine,GpuTruthFailure failure){engine.developmentUltraTestGpuTruthFailure_=static_cast<std::uint8_t>(failure);}
    static void ForceNextFrameBudgetFailure(Engine& engine){engine.developmentUltraTestFrameBudgetFailure_=true;}
    static void ForceNextRollbackFailure(Engine& engine){engine.developmentUltraTestRollbackFailure_=true;}
};
#endif
#if AEVRYND_INTERNAL_RENDERER_TEST_HOOKS
struct EngineRendererTestAccess3D {
    static bool Render(Engine& engine, const DrawList& draw) {
        if (!engine.renderer_) return false;
        engine.renderer_->Render(draw, {.02f, .03f, .04f, 1.f});
        engine.renderer_->WaitIdle();
        return true;
    }
    static std::optional<D3D12RendererTestAccess3D::DirectionalShadowCapture3D> CaptureDirectionalShadow(
        Engine& engine, unsigned cascade) {
        engine.WaitRendererIdle3D();
        return engine.renderer_ ? D3D12RendererTestAccess3D::CaptureDirectionalShadow(*engine.renderer_, cascade)
                                : std::nullopt;
    }
};
#endif
}
