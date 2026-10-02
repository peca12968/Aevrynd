#include "Aevrynd/Engine.h"
#include "Aevrynd/Mesh3D.h"
#include <windowsx.h>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cwctype>
#include <fstream>
#include <new>
#include <limits>
#include <stdexcept>

namespace Aevrynd {
bool CommandLineHasOption(std::wstring_view commandLine,std::wstring_view option){std::size_t position{};while(position<commandLine.size()){while(position<commandLine.size()&&std::iswspace(commandLine[position]))++position;if(position==commandLine.size())break;const bool quoted=commandLine[position]==L'"';if(quoted)++position;const std::size_t start=position;while(position<commandLine.size()&&(quoted?commandLine[position]!=L'"':!std::iswspace(commandLine[position])))++position;if(commandLine.substr(start,position-start)==option)return true;if(quoted&&position<commandLine.size())++position;}return false;}
bool IsDLSSStartupRequested(const wchar_t* commandLine){return(commandLine&&CommandLineHasOption(commandLine,L"--dlss"))||GetEnvironmentVariableW(L"AEVRYND_ENABLE_DLSS",nullptr,0)>0;}
Engine::Engine(EngineConfig config):config_(std::move(config)),renderer_(std::make_unique<D3D12Renderer>()){
    const RenderQuality3D requested=config_.renderQuality;if(requested!=RenderQuality3D::Ultra){SetRenderQuality(requested);return;}requestedRenderQuality_=requested;renderQualityEpoch_=NextDevelopmentUltraEpoch3D(renderQualityEpoch_);const auto tier=config_.height>=1440?DevelopmentUltraResolutionTier3D::QuadHd:DevelopmentUltraResolutionTier3D::FullHd;const auto profile=MakeDevelopmentUltraQuality3D(tier);DevelopmentUltraAdmission3D staged{};staged.admitted=true;staged.reason=DevelopmentUltraAdmissionReason3D::Admitted;developmentUltraReceipt_=StageDevelopmentUltraReceipt3D(profile,staged,renderQualityEpoch_);developmentUltraTerminalState_.epoch=renderQualityEpoch_;config_.renderQuality=RenderQuality3D::High;
}
Engine::~Engine(){if(game_)game_->OnStop(*this);if(renderer_)renderer_->WaitIdle();}
bool Engine::RequestLinearHdrReadback3D(LinearHdrCaptureRequestMetadata3D metadata){return renderer_&&renderer_->RequestLinearHdrReadback3D(metadata);}
bool Engine::CancelLinearHdrReadback3D()noexcept{return renderer_&&renderer_->CancelLinearHdrReadback3D();}
std::optional<LinearHdrReadback3D> Engine::TakeLinearHdrReadback3D(){return renderer_?renderer_->TakeLinearHdrReadback3D():std::optional<LinearHdrReadback3D>{};}
std::optional<ProductionReadbackBundle3D> Engine::TakeProductionReadbackBundle3D(){return renderer_?renderer_->TakeProductionReadbackBundle3D():std::optional<ProductionReadbackBundle3D>{};}
bool Engine::RestoreDevelopmentUltraRendererBaseline(bool trim){
    const DevelopmentUltraReceipt3D priorReceipt=developmentUltraReceipt_;
    const bool forceRollbackFailure=developmentUltraTestRollbackFailure_;
    developmentUltraTestRollbackFailure_=false;
    bool restored=false,pendingClean=false;
    try{
        renderer_->WaitIdle();
        if(!forceRollbackFailure){
            restored=renderer_->RestoreDevelopmentUltraResourceBaseline(lastKnownGoodRendererBaseline_);
            if(restored)draw_.SetDirectionalShadowCascades(lastKnownGoodShadowCascades_);
            renderer_->WaitIdle();
            if(trim&&!lastKnownGoodRendererBaseline_.shadowResident&&!lastKnownGoodRendererBaseline_.volumetricAtmosphereResident){renderer_->TrimTransientMemory();renderer_->WaitIdle();}
        }
        pendingClean=renderer_->GetStats().pendingGpuReleaseBytes==0;
    }catch(...){restored=false;pendingClean=false;}
    const bool rollbackSucceeded=restored&&pendingClean&&!forceRollbackFailure;
    developmentUltraTestFrameBudgetFailure_=false;
    developmentUltraTestGpuTruthFailure_=0;
    if(!rollbackSucceeded&&!hwnd_){
        developmentUltraStaged_=developmentUltraApplied_=false;
        config_.renderQuality=RenderQuality3D::High;
        developmentUltraReceipt_=priorReceipt;
        developmentUltraReceipt_.rollbackAttempted=true;
        developmentUltraReceipt_.rollbackSucceeded=false;
        developmentUltraTerminalState_={DevelopmentUltraTerminalOutcome3D::RollbackFailed,DevelopmentUltraTerminalReason3D::RendererRestoreFailed,renderQualityEpoch_};
        return false;
    }
    if(!rollbackSucceeded){
        // 回復結果不可信時保留較高資源狀態，避免誤報 High baseline 已恢復。
        developmentUltraStaged_=false;
        developmentUltraApplied_=true;
        config_.renderQuality=RenderQuality3D::Ultra;
        developmentUltraReceipt_=priorReceipt;
        developmentUltraReceipt_.phase=DevelopmentUltraTransitionPhase3D::Committed;
        developmentUltraReceipt_.actual=DevelopmentUltraQualityMode3D::DevelopmentUltra;
        developmentUltraReceipt_.actualFingerprint=developmentUltraReceipt_.normalizedFingerprint;
        developmentUltraReceipt_.noOp=false;
        developmentUltraReceipt_.rendererApplied=true;
        developmentUltraReceipt_.transitionPolicyApplied=true;
        developmentUltraReceipt_.rollbackAttempted=true;
        developmentUltraReceipt_.rollbackSucceeded=false;
        developmentUltraTerminalState_={DevelopmentUltraTerminalOutcome3D::RollbackFailed,(!restored||forceRollbackFailure)?DevelopmentUltraTerminalReason3D::RendererRestoreFailed:DevelopmentUltraTerminalReason3D::PendingGpuReleasesAfterRollback,renderQualityEpoch_};
        return false;
    }
    developmentUltraStaged_=developmentUltraApplied_=false;
    developmentUltraGpuTruth_={};
    config_.renderQuality=RenderQuality3D::High;
    const auto tier=config_.height>=1440?DevelopmentUltraResolutionTier3D::QuadHd:DevelopmentUltraResolutionTier3D::FullHd;
    DevelopmentUltraAdmission3D staged{};staged.admitted=true;staged.reason=DevelopmentUltraAdmissionReason3D::Admitted;
    developmentUltraReceipt_=StageDevelopmentUltraReceipt3D(MakeDevelopmentUltraQuality3D(tier),staged,renderQualityEpoch_);
    developmentUltraReceipt_.phase=priorReceipt.phase;
    developmentUltraReceipt_.firstFalseReason=priorReceipt.firstFalseReason;
    developmentUltraReceipt_.applyAttempted=priorReceipt.applyAttempted;
    developmentUltraReceipt_.applySucceeded=priorReceipt.applySucceeded;
    developmentUltraReceipt_.postApplyPassed=priorReceipt.postApplyPassed;
    developmentUltraReceipt_.rollbackAttempted=true;
    developmentUltraReceipt_.rollbackSucceeded=true;
    developmentUltraTerminalState_={DevelopmentUltraTerminalOutcome3D::RolledBack,DevelopmentUltraTerminalReason3D::RollbackSucceeded,renderQualityEpoch_};
    return true;
}
bool Engine::ExitDevelopmentUltraToLastKnownGood(){
    if(!developmentUltraStaged_&&!developmentUltraApplied_)return true;return RestoreDevelopmentUltraRendererBaseline(true);
}
void Engine::RefreshDevelopmentUltraGpuTruth(){
    if(!developmentUltraStaged_&&!developmentUltraApplied_)return;
    if(!developmentUltraApplied_)developmentUltraReceipt_.phase=DevelopmentUltraTransitionPhase3D::GpuTruth;const auto stats=renderer_->GetStats();const auto& resolution=stagedDevelopmentUltraProfile_.resolution;
    const unsigned expectedWidth=(std::max)(1u,unsigned(std::ceil(float(renderer_->RenderWidth())*resolution.volumetricResolutionScale))),expectedHeight=(std::max)(1u,unsigned(std::ceil(float(renderer_->RenderHeight())*resolution.volumetricResolutionScale)));
    const bool resourcesTrue=stats.directionalShadowResolution==stagedDevelopmentUltraProfile_.shadowResolution&&stats.directionalShadowCascadeCapacity==stagedDevelopmentUltraProfile_.shadowCascadeCount&&stats.directionalShadowMapBytes==size_t(stagedDevelopmentUltraProfile_.shadowResolution)*stagedDevelopmentUltraProfile_.shadowResolution*sizeof(float)*stagedDevelopmentUltraProfile_.shadowCascadeCount&&stats.volumetricAtmosphereResolutionScale==resolution.volumetricResolutionScale&&stats.volumetricAtmosphereWidth==expectedWidth&&stats.volumetricAtmosphereHeight==expectedHeight&&stats.volumetricAtmosphereDispatched&&stats.pendingGpuReleaseBytes==0;
    const std::uint8_t forcedGpuTruth=developmentUltraTestGpuTruthFailure_;developmentUltraTestGpuTruthFailure_=0;const auto telemetry=QueryPrimaryDxgiVideoMemoryTelemetry3D(renderQualityEpoch_);const std::size_t hardVram=DevelopmentUltraHardVramBudgetBytes(stagedDevelopmentUltraProfile_,telemetry.dedicatedVideoMemoryBytes);const bool adapterIdentityTrue=forcedGpuTruth!=1&&DevelopmentUltraAdapterIdentityMatches3D(stats.adapterLuidValid,stats.adapterLuidLow,stats.adapterLuidHigh,telemetry);const bool telemetryTrue=adapterIdentityTrue&&telemetry.hardwareAdapter&&telemetry.local.querySucceeded&&telemetry.sampledAtSteadyNanoseconds&&telemetry.ageNanoseconds<=2'000'000'000ull&&hardVram&&telemetry.local.currentUsageBytes<=hardVram;const bool validationTrue=forcedGpuTruth!=2&&!stats.debugErrorCount&&!stats.debugWarningCount&&(!stats.gpuValidationRequested||(stats.gpuValidationAvailable&&stats.gpuValidationExecuted&&stats.gpuValidationPassed));const auto timingGate=EvaluateDevelopmentUltraFrameTiming3D(stats.lastGpuFrameNanoseconds,stats.gpuTimingSamples,stats.completedGpuFrameSerial,stats.lastCompletedGpuTimingReceipt.serial,stats.lastCompletedGpuTimingReceipt.nanoseconds,stats.gpuValidationRequested,stats.gpuValidationAvailable,stats.gpuValidationExecuted,stats.gpuValidationPassed,stats.debugErrorCount,stats.debugWarningCount);const bool forceFrameBudgetFailure=developmentUltraTestFrameBudgetFailure_||forcedGpuTruth==3;developmentUltraTestFrameBudgetFailure_=false;const bool frameBudgetTrue=!forceFrameBudgetFailure&&timingGate.accepted;const bool framePassed=resourcesTrue&&telemetryTrue&&validationTrue&&frameBudgetTrue;
    DevelopmentUltraGpuTruthSnapshot3D snapshot{};snapshot.epoch=renderQualityEpoch_;const auto priorObservation=developmentUltraReceipt_.gpuTruth.epoch==renderQualityEpoch_?developmentUltraReceipt_.gpuTruth.observationIndex:0;snapshot.observationIndex=priorObservation==(std::numeric_limits<std::uint64_t>::max)()?priorObservation:priorObservation+1;snapshot.submittedFrameSerial=stats.submittedFrames;snapshot.completedFrameSerial=stats.completedGpuFrameSerial;snapshot.timingSampleSerial=stats.gpuTimingSamples;snapshot.lastGpuFrameNanoseconds=stats.lastGpuFrameNanoseconds;snapshot.telemetryAgeNanoseconds=telemetry.ageNanoseconds;snapshot.dedicatedVideoMemoryBytes=telemetry.dedicatedVideoMemoryBytes;snapshot.currentVideoMemoryUsageBytes=telemetry.local.currentUsageBytes;snapshot.rendererAdapterLuidLow=stats.adapterLuidLow;snapshot.rendererAdapterLuidHigh=stats.adapterLuidHigh;snapshot.telemetryAdapterLuidLow=telemetry.adapterLuidLow;snapshot.telemetryAdapterLuidHigh=telemetry.adapterLuidHigh;snapshot.hardVramBudgetBytes=hardVram;snapshot.pendingGpuReleaseBytes=stats.pendingGpuReleaseBytes;snapshot.debugErrorCount=stats.debugErrorCount;snapshot.debugWarningCount=stats.debugWarningCount;snapshot.requiredStableFrames=stagedDevelopmentUltraProfile_.transition.stableFramesBeforeCommit;snapshot.timingProvenance=timingGate.provenance;snapshot.observed=true;snapshot.resourcesTrue=resourcesTrue;snapshot.telemetryTrue=telemetryTrue;snapshot.rendererAdapterIdentityValid=stats.adapterLuidValid;snapshot.adapterIdentityTrue=adapterIdentityTrue;snapshot.validationTrue=validationTrue;snapshot.timingRecorded=timingGate.timingRecorded;snapshot.timingSerialAligned=timingGate.serialAligned;snapshot.productionFrameBudgetApplied=timingGate.productionBudgetApplied;snapshot.frameBudgetTrue=frameBudgetTrue;snapshot.framePassed=framePassed;bool fresh=false;const bool forceSerialFailure=forcedGpuTruth==4;const bool sequenceCommitted=AdvanceDevelopmentUltraGpuTruthSequence3D(developmentUltraGpuTruth_,forceSerialFailure?developmentUltraGpuTruth_.lastSubmittedFrameSerial:stats.submittedFrames,forceSerialFailure?developmentUltraGpuTruth_.lastCompletedFrameSerial:stats.completedGpuFrameSerial,forceSerialFailure?developmentUltraGpuTruth_.lastTimingSampleSerial:stats.gpuTimingSamples,framePassed,snapshot.requiredStableFrames,&fresh);snapshot.fresh=fresh;snapshot.stableFrames=developmentUltraGpuTruth_.stableFrames;snapshot.committed=developmentUltraApplied_||sequenceCommitted;developmentUltraReceipt_.gpuTruth=snapshot;if(forceSerialFailure)RecordDevelopmentUltraFirstFalse3D(developmentUltraReceipt_,DevelopmentUltraTransitionPhase3D::GpuTruth,DevelopmentUltraFirstFalseReason3D::GpuSerialNotFresh);else if(!resourcesTrue)RecordDevelopmentUltraFirstFalse3D(developmentUltraReceipt_,DevelopmentUltraTransitionPhase3D::GpuTruth,DevelopmentUltraFirstFalseReason3D::ResourceValidationFailed);else if(!adapterIdentityTrue)RecordDevelopmentUltraFirstFalse3D(developmentUltraReceipt_,DevelopmentUltraTransitionPhase3D::GpuTruth,DevelopmentUltraFirstFalseReason3D::AdapterIdentityFailed);else if(!telemetryTrue)RecordDevelopmentUltraFirstFalse3D(developmentUltraReceipt_,DevelopmentUltraTransitionPhase3D::GpuTruth,DevelopmentUltraFirstFalseReason3D::TelemetryValidationFailed);else if(!validationTrue)RecordDevelopmentUltraFirstFalse3D(developmentUltraReceipt_,DevelopmentUltraTransitionPhase3D::GpuTruth,DevelopmentUltraFirstFalseReason3D::GpuValidationFailed);else if(!frameBudgetTrue)RecordDevelopmentUltraFirstFalse3D(developmentUltraReceipt_,DevelopmentUltraTransitionPhase3D::GpuTruth,DevelopmentUltraFirstFalseReason3D::FrameBudgetFailed);else if(!fresh)RecordDevelopmentUltraFirstFalse3D(developmentUltraReceipt_,DevelopmentUltraTransitionPhase3D::GpuTruth,DevelopmentUltraFirstFalseReason3D::GpuSerialNotFresh);
    if(developmentUltraApplied_||!sequenceCommitted)return;developmentUltraApplied_=true;developmentUltraStaged_=false;config_.renderQuality=RenderQuality3D::Ultra;developmentUltraReceipt_.phase=DevelopmentUltraTransitionPhase3D::Committed;developmentUltraReceipt_.actual=DevelopmentUltraQualityMode3D::DevelopmentUltra;developmentUltraReceipt_.actualFingerprint=developmentUltraReceipt_.normalizedFingerprint;developmentUltraReceipt_.reason=DevelopmentUltraAdmissionReason3D::Admitted;developmentUltraReceipt_.noOp=false;developmentUltraReceipt_.rendererApplied=true;developmentUltraReceipt_.transitionPolicyApplied=true;developmentUltraReceipt_.shadowResourcesApplied=true;developmentUltraReceipt_.volumetricResolutionApplied=true;developmentUltraTerminalState_={DevelopmentUltraTerminalOutcome3D::Committed,DevelopmentUltraTerminalReason3D::CommitSucceeded,renderQualityEpoch_};
}
void Engine::SetRenderQuality(RenderQuality3D quality){
    if((quality!=RenderQuality3D::Ultra||developmentUltraStaged_||developmentUltraApplied_)&&!ExitDevelopmentUltraToLastKnownGood())return;requestedRenderQuality_=quality;renderQualityEpoch_=NextDevelopmentUltraEpoch3D(renderQualityEpoch_);developmentUltraTestFrameBudgetFailure_=false;developmentUltraTestGpuTruthFailure_=0;developmentUltraTestRollbackFailure_=false;developmentUltraReceipt_={};developmentUltraReceipt_.reason=DevelopmentUltraAdmissionReason3D::InvalidProfile;developmentUltraReceipt_.epoch=renderQualityEpoch_;developmentUltraReceipt_.gpuTruth.epoch=renderQualityEpoch_;developmentUltraTerminalState_={DevelopmentUltraTerminalOutcome3D::InProgress,DevelopmentUltraTerminalReason3D::None,renderQualityEpoch_};RendererQualitySettings3D settings{};
    switch(quality){
    case RenderQuality3D::Low:config_.renderQuality=RenderQuality3D::Low;settings={false,false,false,false};break;
    case RenderQuality3D::Medium:config_.renderQuality=RenderQuality3D::Medium;settings={true,true,false,false};break;
    case RenderQuality3D::High:config_.renderQuality=RenderQuality3D::High;settings={true,true,true,true};break;
    case RenderQuality3D::Ultra:{
        const auto tier=config_.height>=1440?DevelopmentUltraResolutionTier3D::QuadHd:DevelopmentUltraResolutionTier3D::FullHd;
        const auto profile=MakeDevelopmentUltraQuality3D(tier);const auto telemetry=QueryPrimaryDxgiVideoMemoryTelemetry3D(renderQualityEpoch_);const auto stats=renderer_->GetStats();
        std::size_t resident=stats.residentMeshBytes;if(stats.residentTextureAllocationBytes<=(std::numeric_limits<std::size_t>::max)()-resident)resident+=stats.residentTextureAllocationBytes;else resident=(std::numeric_limits<std::size_t>::max)();
        std::size_t additional=profile.textureBudgetBytes;if(profile.meshBudgetBytes<=(std::numeric_limits<std::size_t>::max)()-additional)additional+=profile.meshBudgetBytes;else additional=(std::numeric_limits<std::size_t>::max)();
        const bool forceAdmissionFailure=developmentUltraTestFailure_==3,forcePreApplyPending=developmentUltraTestFailure_==4;auto admission=EvaluateDevelopmentUltraAdmission3D(profile,telemetry,{resident,additional,forcePreApplyPending?1:stats.pendingGpuReleaseBytes});if(forceAdmissionFailure){admission.admitted=false;admission.reason=DevelopmentUltraAdmissionReason3D::TelemetryUnavailable;}if(forceAdmissionFailure||forcePreApplyPending)developmentUltraTestFailure_=0;developmentUltraReceipt_=StageDevelopmentUltraReceipt3D(profile,admission,renderQualityEpoch_);config_.renderQuality=RenderQuality3D::High;settings={true,true,true,true};if(!admission.admitted)break;
        stagedDevelopmentUltraProfile_=profile;developmentUltraReceipt_.phase=DevelopmentUltraTransitionPhase3D::RendererApply;if(!developmentUltraStaged_&&!developmentUltraApplied_){lastKnownGoodShadowCascades_=draw_.DirectionalShadowCascades();lastKnownGoodRendererBaseline_=renderer_->CaptureDevelopmentUltraResourceBaseline();}renderer_->WaitIdle();if(renderer_->GetStats().pendingGpuReleaseBytes){RecordDevelopmentUltraFirstFalse3D(developmentUltraReceipt_,DevelopmentUltraTransitionPhase3D::RendererApply,DevelopmentUltraFirstFalseReason3D::PendingGpuReleasesBeforeApply);break;}bool transactionActive=true;struct TransactionGuard{Engine* engine{};bool* active{};~TransactionGuard(){if(engine&&active&&*active)(void)engine->RestoreDevelopmentUltraRendererBaseline(true);}}transaction{this,&transactionActive};const bool forceApplyFailure=developmentUltraTestFailure_==1,forcePendingFailure=developmentUltraTestFailure_==2;developmentUltraTestFailure_=0;developmentUltraReceipt_.applyAttempted=true;const bool rendererApplied=!forceApplyFailure&&renderer_->ApplyDevelopmentUltraResourceProfile(profile.shadowResolution,profile.shadowCascadeCount,profile.resolution.volumetricResolutionScale);developmentUltraReceipt_.applySucceeded=rendererApplied;if(!rendererApplied)RecordDevelopmentUltraFirstFalse3D(developmentUltraReceipt_,DevelopmentUltraTransitionPhase3D::RendererApply,DevelopmentUltraFirstFalseReason3D::RendererApplyFailed);if(rendererApplied)draw_.SetDirectionalShadowCascades({profile.shadowCascadeCount,profile.shadowResolution,.1f,220.f,.70f,.10f});renderer_->WaitIdle();if(!rendererApplied)break;developmentUltraReceipt_.phase=DevelopmentUltraTransitionPhase3D::PostApply;const size_t pendingForGate=forcePendingFailure?1:renderer_->GetStats().pendingGpuReleaseBytes;developmentUltraReceipt_.postApplyPassed=DevelopmentUltraPostApplyGate3D(rendererApplied,pendingForGate);if(!developmentUltraReceipt_.postApplyPassed){RecordDevelopmentUltraFirstFalse3D(developmentUltraReceipt_,DevelopmentUltraTransitionPhase3D::PostApply,DevelopmentUltraFirstFalseReason3D::PostApplyPendingGpuReleases);break;}developmentUltraReceipt_.phase=DevelopmentUltraTransitionPhase3D::GpuTruth;developmentUltraStaged_=true;developmentUltraApplied_=false;const auto truthBaseline=renderer_->GetStats();developmentUltraGpuTruth_={truthBaseline.submittedFrames,truthBaseline.submittedFrames,truthBaseline.gpuTimingSamples,0};transactionActive=false;break;
    }
    default:{
        config_.renderQuality=RenderQuality3D::High;settings={true,true,true,true};RecordDevelopmentUltraFirstFalse3D(developmentUltraReceipt_,DevelopmentUltraTransitionPhase3D::Admission,DevelopmentUltraFirstFalseReason3D::AdmissionDenied);break;
    }}
    renderer_->SetQualitySettings(settings);
}
void Engine::SetTitle(const std::wstring& title){if(hwnd_)SetWindowTextW(hwnd_,title.c_str());}
std::filesystem::path Engine::ExecutableDirectory()const{std::wstring path(32768,L'\0');const DWORD length=GetModuleFileNameW(nullptr,path.data(),DWORD(path.size()));if(!length||length>=path.size())return{};path.resize(length);return std::filesystem::path(path).parent_path();}
MeshHandle3D Engine::CreateMesh3D(const Mesh3D& mesh){return mesh.Valid()?renderer_->CreateMesh(mesh.vertices,mesh.indices):MeshHandle3D{};}
MeshResource3D Engine::CreateMeshResource3D(const Mesh3D& mesh,MeshCpuRetention3D retention){if(!mesh.Valid())return{};const MeshBounds3D bounds=mesh.Bounds();const MeshHandle3D handle=renderer_->CreateMesh(mesh.vertices,mesh.indices);if(!handle)return{};if(retention==MeshCpuRetention3D::Discard)return{handle,bounds,mesh.vertices.size(),mesh.indices.size(),{}};try{if(meshCpuCache_.size()>=256)for(auto iterator=meshCpuCache_.begin();iterator!=meshCpuCache_.end();)if(iterator->second.expired())iterator=meshCpuCache_.erase(iterator);else++iterator;std::shared_ptr<const Mesh3D> cpuMesh;if(const auto cached=meshCpuCache_.find(handle.value);cached!=meshCpuCache_.end())cpuMesh=cached->second.lock();if(!cpuMesh){cpuMesh=std::make_shared<Mesh3D>(mesh);meshCpuCache_.insert_or_assign(handle.value,cpuMesh);}return{handle,bounds,cpuMesh->vertices.size(),cpuMesh->indices.size(),std::move(cpuMesh)};}catch(const std::bad_alloc&){renderer_->DestroyMesh(handle);return{};}}
SkinnedMeshResource3D Engine::CreateSkinnedMeshResource3D(const SkinnedModel3D& model){
    std::vector<SkinnedVertex3D> vertices;
    if(!BuildGpuSkinnedVertices(model,vertices))return{};
    const SkeletonCompatibilityId3D compatibility=model.skeletonCompatibilityId?model.skeletonCompatibilityId:ComputeStructuralSkeletonCompatibilityId3D(model.skeleton,model.inverseBindMatrices);
    const SkinnedMeshHandle3D handle=renderer_->CreateSkinnedMesh(vertices,model.bindMesh.indices,static_cast<std::uint32_t>(model.skeleton.joints.size()),compatibility);
    return handle?SkinnedMeshResource3D{handle,model.bindMesh.Bounds(),vertices.size(),model.bindMesh.indices.size(),model.skeleton.joints.size()}:SkinnedMeshResource3D{};
}
bool Engine::CreateMeshLodResourceChain3D(const Mesh3D& mesh,MeshLodResourceChain3D& output,MeshLodChainSettings3D settings,MeshProcessingError3D* error,MeshLodCpuRetention3D retention){
    std::vector<Mesh3D> generated;MeshLodChainStats3D processing;MeshLodResourceChain3D temporary{};const auto clear=[&](MeshLodResourceChain3D& chain){chain.levels={};chain.levelCount=0;chain.processing={};};const auto rollback=[&](){for(std::uint32_t level=temporary.levelCount;level>0;--level)if(temporary.levels[level-1])DestroyMesh3D(temporary.levels[level-1].handle);clear(temporary);};const auto retainLevel=[&](std::size_t level){return retention==MeshLodCpuRetention3D::AllLevels||(retention==MeshLodCpuRetention3D::Level0Only&&level==0)?MeshCpuRetention3D::RetainForQueries:MeshCpuRetention3D::Discard;};try{if(!MeshProcessing3D::GenerateLodChain(mesh,generated,settings,&processing,error))return false;temporary.processing=processing;temporary.levelCount=static_cast<std::uint32_t>(generated.size()+1);temporary.levels[0]=CreateMeshResource3D(mesh,retainLevel(0));if(!temporary.levels[0])throw std::bad_alloc{};for(std::size_t level=0;level<generated.size();++level){temporary.levels[level+1]=CreateMeshResource3D(generated[level],retainLevel(level+1));if(!temporary.levels[level+1])throw std::bad_alloc{};}}catch(const std::bad_alloc&){rollback();if(error)*error={MeshProcessingErrorCode3D::LimitExceeded,0};return false;}DestroyMeshLodResourceChain3D(output);output.levels=std::move(temporary.levels);output.levelCount=temporary.levelCount;output.processing=temporary.processing;clear(temporary);if(error)*error={};return true;
}
void Engine::DestroyMeshLodResourceChain3D(MeshLodResourceChain3D& chain){for(std::uint32_t level=(std::min)(chain.levelCount,static_cast<std::uint32_t>(chain.levels.size()));level>0;--level)if(chain.levels[level-1])DestroyMesh3D(chain.levels[level-1].handle);chain.levels={};chain.levelCount=0;chain.processing={};}
namespace {
EnvironmentLightingGpuReceipt3D EnvironmentReceipt(
    std::uint64_t sequence,EnvironmentLightingGpuOperation3D operation,
    EnvironmentLightingHandle3D handle,const RendererStats& before,
    const RendererStats& after,bool accepted){
    return{sequence,operation,handle,before.residentEnvironmentLightingSets,
        after.residentEnvironmentLightingSets,before.residentEnvironmentLightingBytes,
        after.residentEnvironmentLightingBytes,before.residentEnvironmentLightingDescriptors,
        after.residentEnvironmentLightingDescriptors,
        after.pendingEnvironmentLightingReleaseBytes,accepted,
        after.environmentLightingActive};
}
}
bool Engine::CreateEnvironmentLighting3D(const EnvironmentLightingStore3D& store,
    EnvironmentLightingHandle3D handle,EnvironmentLightingGpuReceipt3D* receipt){
    const auto before=renderer_->GetStats();const auto lease=store.Resolve(handle);bool accepted=false;
    if(lease&&lease.payload){
        const auto found=environmentLightingBindings_.find(handle.value);
        if(found==environmentLightingBindings_.end()){
            if(renderer_->CreateEnvironmentLighting(store,handle))try{
                environmentLightingBindings_.emplace(handle.value,EnvironmentLightingBinding3D{lease.payload,1});accepted=true;
            }catch(...){renderer_->DestroyEnvironmentLighting(handle);}
        }else if(found->second.payload.get()==lease.payload.get()&&found->second.references<UINT32_MAX&&renderer_->CreateEnvironmentLighting(store,handle)){
            ++found->second.references;accepted=true;
        }
    }
    const auto after=renderer_->GetStats();
    if(receipt)*receipt=EnvironmentReceipt(++environmentLightingReceiptSequence_,EnvironmentLightingGpuOperation3D::Create,handle,before,after,accepted);else ++environmentLightingReceiptSequence_;
    return accepted;
}
bool Engine::SetEnvironmentLighting3D(const EnvironmentLightingStore3D& store,EnvironmentLightingHandle3D handle,
    EnvironmentLightingGpuReceipt3D* receipt){
    const auto before=renderer_->GetStats();const auto lease=store.Resolve(handle);const auto found=environmentLightingBindings_.find(handle.value);
    const bool accepted=lease&&found!=environmentLightingBindings_.end()&&found->second.payload.get()==lease.payload.get()&&renderer_->SetEnvironmentLighting(handle);const auto after=renderer_->GetStats();
    if(receipt)*receipt=EnvironmentReceipt(++environmentLightingReceiptSequence_,EnvironmentLightingGpuOperation3D::Set,handle,before,after,accepted);else ++environmentLightingReceiptSequence_;
    return accepted;
}
bool Engine::ClearEnvironmentLighting3D(EnvironmentLightingGpuReceipt3D* receipt){
    const auto before=renderer_->GetStats();const bool accepted=renderer_->SetEnvironmentLighting({});const auto after=renderer_->GetStats();
    if(receipt)*receipt=EnvironmentReceipt(++environmentLightingReceiptSequence_,EnvironmentLightingGpuOperation3D::Clear,{},before,after,accepted);else ++environmentLightingReceiptSequence_;
    return accepted;
}
bool Engine::DestroyEnvironmentLighting3D(const EnvironmentLightingStore3D& store,EnvironmentLightingHandle3D handle,
    EnvironmentLightingGpuReceipt3D* receipt){
    const auto before=renderer_->GetStats();const auto lease=store.Resolve(handle);const auto found=environmentLightingBindings_.find(handle.value);
    const bool accepted=lease&&found!=environmentLightingBindings_.end()&&found->second.payload.get()==lease.payload.get();
    if(accepted){renderer_->DestroyEnvironmentLighting(handle);if(--found->second.references==0)environmentLightingBindings_.erase(found);}
    const auto after=renderer_->GetStats();
    if(receipt)*receipt=EnvironmentReceipt(++environmentLightingReceiptSequence_,EnvironmentLightingGpuOperation3D::Destroy,handle,before,after,accepted);else ++environmentLightingReceiptSequence_;
    return accepted;
}
void Engine::TrimEnvironmentLighting3D(EnvironmentLightingGpuReceipt3D* receipt){
    const auto before=renderer_->GetStats();renderer_->TrimEnvironmentLighting();const auto after=renderer_->GetStats();
    if(receipt)*receipt=EnvironmentReceipt(++environmentLightingReceiptSequence_,EnvironmentLightingGpuOperation3D::Trim,{},before,after,true);else ++environmentLightingReceiptSequence_;
}
EngineMemoryStats Engine::GetMemoryStats()const{MeshCpuMemoryStats3D meshCpu{};meshCpu.cacheEntries=meshCpuCache_.size();for(const auto& [handle,cached]:meshCpuCache_)if(const auto mesh=cached.lock()){++meshCpu.residentResources;meshCpu.residentBytes+=mesh->vertices.size()*sizeof(Vertex3D)+mesh->indices.size()*sizeof(std::uint32_t);}else ++meshCpu.expiredCacheEntries;return{processMemoryStats_,draw_.MemoryStats(),renderer_->MemoryStats(),audio_.MemoryStats(),game_?game_->GetReusableMemoryStats():ReusableMemoryStats{},meshCpu,lastReleasedBytes_,trimCount_,automaticTrimCount_};}
size_t Engine::TrimUnusedMemory(){for(auto iterator=meshCpuCache_.begin();iterator!=meshCpuCache_.end();)if(iterator->second.expired())iterator=meshCpuCache_.erase(iterator);else++iterator;if(meshCpuCache_.empty())meshCpuCache_.rehash(0);const size_t audioReleased=audio_.TrimUnusedMemory(),gameReleased=game_?game_->TrimReusableMemory():0;draw_.TrimTransientMemory();renderer_->TrimTransientMemory();lastReleasedBytes_=audioReleased+gameReleased+draw_.MemoryStats().lastReleasedBytes+renderer_->MemoryStats().lastReleasedBytes;++trimCount_;processMemoryStats_=QueryProcessMemoryStats();return lastReleasedBytes_;}

int Engine::Run(std::unique_ptr<IGame> game,HINSTANCE instance,int showCommand){
    std::filesystem::path diagnosticPath;std::array<wchar_t,32768> diagnosticBuffer{};const DWORD diagnosticLength=GetEnvironmentVariableW(L"AEVRYND_FATAL_DIAGNOSTIC_PATH",diagnosticBuffer.data(),DWORD(diagnosticBuffer.size()));if(diagnosticLength&&diagnosticLength<diagnosticBuffer.size())diagnosticPath=std::wstring_view(diagnosticBuffer.data(),diagnosticLength);
    const auto phase=[&](const char* value){if(diagnosticPath.empty())return;std::ofstream output(diagnosticPath,std::ios::app);output<<value<<'\n';output.flush();};
    bool gameStarted=false,gameStopped=false,audioStarted=false,firstUpdate=true,firstRender=true;
    game_=std::move(game);SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    try{
    WNDCLASSEXW wc{sizeof(wc)};wc.style=CS_HREDRAW|CS_VREDRAW;wc.lpfnWndProc=WindowProc;wc.hInstance=instance;wc.hCursor=LoadCursor(nullptr,IDC_ARROW);wc.hbrBackground=nullptr;wc.lpszClassName=L"AevryndEngineWindow";
    if(!RegisterClassExW(&wc)&&GetLastError()!=ERROR_CLASS_ALREADY_EXISTS)throw std::runtime_error("RegisterClass failed");
    RECT workArea{};SystemParametersInfoW(SPI_GETWORKAREA,0,&workArea,0);const LONG workWidth=workArea.right-workArea.left,workHeight=workArea.bottom-workArea.top;
    const float fit=(std::min)({1.f,float((std::max)(320L,workWidth-48))/config_.width,float((std::max)(180L,workHeight-72))/config_.height});
    const LONG desiredWidth=LONG(std::lround(config_.width*fit)),desiredHeight=LONG(std::lround(config_.height*fit));
    RECT rect{0,0,desiredWidth,desiredHeight};DWORD style=WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU|WS_MINIMIZEBOX|(config_.resizable?WS_THICKFRAME|WS_MAXIMIZEBOX:0);AdjustWindowRectEx(&rect,style,FALSE,0);
    const LONG windowWidth=rect.right-rect.left,windowHeight=rect.bottom-rect.top,x=workArea.left+(workWidth-windowWidth)/2,y=workArea.top+(workHeight-windowHeight)/2;
    hwnd_=CreateWindowExW(0,wc.lpszClassName,config_.title.c_str(),style,x,y,windowWidth,windowHeight,nullptr,nullptr,instance,this);
    if(!hwnd_)throw std::runtime_error("CreateWindow failed");ShowWindow(hwnd_,showCommand);UpdateWindow(hwnd_);RECT client{};GetClientRect(hwnd_,&client);renderer_->Initialize(hwnd_,unsigned((std::max)(1L,client.right-client.left)),unsigned((std::max)(1L,client.bottom-client.top)),config_.width,config_.height,config_.enableDLSS);SetRenderQuality(requestedRenderQuality_);audio_.Initialize();audioStarted=true;processMemoryStats_=QueryProcessMemoryStats();phase("OnStartBefore");game_->OnStart(*this);gameStarted=true;phase("OnStartAfter");
    using Clock=std::chrono::steady_clock;auto last=Clock::now();double accumulator=0.0;MSG msg{};int exitCode{};
    while(running_){
        input_.BeginFrame();
        input_.PollGamepad();
        while(PeekMessageW(&msg,nullptr,0,0,PM_REMOVE)){if(msg.message==WM_QUIT){exitCode=int(msg.wParam);running_=false;break;}TranslateMessage(&msg);DispatchMessageW(&msg);}if(!running_)break;
        // 最小化時停止提交畫面，並只在進入背景時修剪一次暫存容量。
        if(minimized_){if(trimPending_){TrimUnusedMemory();trimPending_=false;}audio_.Update();input_.ConsumePresses(true);accumulator=0;Sleep(50);last=Clock::now();continue;}
        auto now=Clock::now();double frame=std::chrono::duration<double>(now-last).count();last=now;if(frame>.25)frame=.25;accumulator+=frame;int updates=0;
        while(accumulator>=config_.fixedStep&&updates<5){if(firstUpdate)phase("FirstOnUpdateBefore");game_->OnUpdate(*this,config_.fixedStep);if(firstUpdate){phase("FirstOnUpdateAfter");firstUpdate=false;}input_.ConsumePresses();time_+=config_.fixedStep;accumulator-=config_.fixedStep;updates++;}
        if(updates==5)accumulator=0;audio_.Update();draw_.Clear();draw_.SetClipRect({0,0},{float(config_.width),float(config_.height)});if(firstRender)phase("FirstOnRenderBefore");game_->OnRender(*this,float(accumulator/config_.fixedStep));if(firstRender)phase("FirstOnRenderAfter");if(firstRender)phase("FirstRendererRenderBefore");renderer_->Render(draw_,clearColor_);RefreshDevelopmentUltraGpuTruth();if(firstRender){phase("FirstRendererRenderAfter");if(!diagnosticPath.empty())phase("CaptureCallback");firstRender=false;}if(lastMemorySampleTime_<0||time_-lastMemorySampleTime_>=.5f){processMemoryStats_=QueryProcessMemoryStats();lastMemorySampleTime_=time_;const TransientMemoryStats drawMemory=draw_.MemoryStats(),rendererMemory=renderer_->MemoryStats();const ReusableMemoryStats audioMemory=audio_.MemoryStats(),gameMemory=game_->GetReusableMemoryStats();const RendererStats rendererStats=renderer_->GetStats();const size_t reclaimable=drawMemory.reclaimableCapacityBytes+rendererMemory.reclaimableCapacityBytes+audioMemory.reclaimableCapacityBytes+gameMemory.reclaimableCapacityBytes;const bool disposableGpuResources=rendererStats.idleMeshUploadContexts||rendererStats.idleMaterialTextureUploadContexts;const MemoryPressurePolicy policy{config_.memoryPressureThresholdPercent,config_.minimumAutomaticMemoryReclaimBytes,config_.memoryTrimCooldownSeconds};const float secondsSinceAutomaticTrim=automaticTrimmed_?time_-lastAutomaticTrimTime_:config_.memoryTrimCooldownSeconds;if(config_.enableAutomaticMemoryTrim&&ShouldTrimMemory(processMemoryStats_,reclaimable,secondsSinceAutomaticTrim,policy,disposableGpuResources)){TrimUnusedMemory();lastAutomaticTrimTime_=time_;automaticTrimmed_=true;++automaticTrimCount_;}}
    }
    game_->OnStop(*this);gameStopped=true;game_.reset();audio_.Shutdown();audioStarted=false;return exitCode;
    }catch(...){phase("EngineCatch");if(game_&&gameStarted&&!gameStopped){try{game_->OnStop(*this);gameStopped=true;phase("OnStopAfterCatch");}catch(...){phase("OnStopCatch");}}game_.reset();if(audioStarted){audio_.Shutdown();audioStarted=false;}throw;}
}

LRESULT CALLBACK Engine::WindowProc(HWND hwnd,UINT message,WPARAM wParam,LPARAM lParam){Engine* self=reinterpret_cast<Engine*>(GetWindowLongPtrW(hwnd,GWLP_USERDATA));if(message==WM_NCCREATE){auto cs=reinterpret_cast<CREATESTRUCTW*>(lParam);self=static_cast<Engine*>(cs->lpCreateParams);SetWindowLongPtrW(hwnd,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(self));self->hwnd_=hwnd;}
    if(self&&(message==WM_SETFOCUS||message==WM_KILLFOCUS)){
        self->input_.SetFocused(message==WM_SETFOCUS);
        if(message==WM_KILLFOCUS&&GetCapture()==hwnd)ReleaseCapture();
    }
    if(self&&message==WM_RBUTTONDOWN){self->input_.SetRightMouseButton(true);return 0;}if(self&&message==WM_RBUTTONUP){self->input_.SetRightMouseButton(false);return 0;}return self?self->HandleMessage(message,wParam,lParam):DefWindowProcW(hwnd,message,wParam,lParam);}
LRESULT Engine::HandleMessage(UINT message,WPARAM wParam,LPARAM lParam){switch(message){case WM_CLOSE:running_=false;DestroyWindow(hwnd_);return 0;case WM_DESTROY:PostQuitMessage(0);return 0;case WM_SIZE:if(wParam==SIZE_MINIMIZED){minimized_=true;trimPending_=true;}else{minimized_=false;if(!resizing_)renderer_->Resize(unsigned(LOWORD(lParam)),unsigned(HIWORD(lParam)));}return 0;case WM_ENTERSIZEMOVE:resizing_=true;return 0;case WM_EXITSIZEMOVE:{resizing_=false;RECT client{};GetClientRect(hwnd_,&client);renderer_->Resize(unsigned((std::max)(1L,client.right-client.left)),unsigned((std::max)(1L,client.bottom-client.top)));return 0;}case WM_DPICHANGED:{const RECT* suggested=reinterpret_cast<const RECT*>(lParam);SetWindowPos(hwnd_,nullptr,suggested->left,suggested->top,suggested->right-suggested->left,suggested->bottom-suggested->top,SWP_NOACTIVATE|SWP_NOZORDER);return 0;}case WM_GETMINMAXINFO:{auto info=reinterpret_cast<MINMAXINFO*>(lParam);info->ptMinTrackSize={400,260};return 0;}case WM_KEYDOWN:case WM_SYSKEYDOWN:input_.SetKey(unsigned(wParam),true);return 0;case WM_KEYUP:case WM_SYSKEYUP:input_.SetKey(unsigned(wParam),false);return 0;case WM_MOUSEMOVE:{RECT client{};GetClientRect(hwnd_,&client);const Vec2 output{float((std::max)(1L,client.right-client.left)),float((std::max)(1L,client.bottom-client.top))},point{float(GET_X_LPARAM(lParam)),float(GET_Y_LPARAM(lParam))};const CanvasViewport viewport=CalculateCanvasViewport({float(config_.width),float(config_.height)},output);input_.SetMouse(MapClientToCanvas(point,viewport),viewport.Contains(point));return 0;}case WM_MOUSEWHEEL:input_.AddMouseWheel(float(GET_WHEEL_DELTA_WPARAM(wParam))/float(WHEEL_DELTA));return 0;case WM_LBUTTONDOWN:if(input_.MouseInsideCanvas()){SetCapture(hwnd_);input_.SetMouseButton(true);}return 0;case WM_LBUTTONUP:ReleaseCapture();input_.SetMouseButton(false);return 0;}return DefWindowProcW(hwnd_,message,wParam,lParam);}
}
