#include <Aevrynd/Camera.h>
#include <Aevrynd/Engine.h>
#include <Aevrynd/Mesh3D.h>

#include <Windows.h>
#include <array>
#include <cmath>
#include <iostream>
#include <memory>
#include <string_view>

using namespace Aevrynd;

namespace {
int Result{};

class RuntimeGame final : public IGame {
public:
 explicit RuntimeGame(bool expectFailure,unsigned expectedShadowTransactions):expectedShadowTransactions_(expectedShadowTransactions),expectFailure_(expectFailure){}
 void OnStart(Engine& engine)override{
  const auto receipt=engine.DevelopmentUltraReceipt();
  if(engine.RequestedRenderQuality()!=RenderQuality3D::Ultra||engine.RenderQuality()!=RenderQuality3D::High||!receipt.requestActive||receipt.rendererApplied||receipt.transitionPolicyApplied){Result=1;engine.Quit();return;}
  if(expectFailure_){const auto stats=engine.GetRendererStats();if(receipt.shadowResourcesApplied||receipt.volumetricResolutionApplied||receipt.cloudSamplesApplied||receipt.cloudShadowSamplesApplied||receipt.lightShaftSamplesApplied||receipt.vegetationDensityApplied||receipt.textureBudgetApplied||receipt.meshBudgetApplied||stats.directionalShadowResolution||stats.directionalShadowCascadeCapacity||stats.directionalShadowMapBytes||stats.directionalShadowMapAllocations!=expectedShadowTransactions_||stats.directionalShadowMapReleases!=expectedShadowTransactions_||stats.directionalShadowDescriptorSwitches!=expectedShadowTransactions_*2||stats.directionalShadowResourceGeneration!=expectedShadowTransactions_*2||stats.volumetricAtmosphereWidth||stats.volumetricAtmosphereHeight||stats.volumetricAtmosphereResidentBytes||stats.volumetricAtmosphereResourceCreates||stats.volumetricAtmosphereResourceReleases||stats.pendingGpuReleaseBytes){Result=7;engine.Quit();}else failureStateValidated_=true;return;}
  const Mesh3D box=Mesh3D::CreateUnitBox();mesh_=engine.CreateMesh3D(box);if(!mesh_){Result=2;engine.Quit();}
 }
 void OnUpdate(Engine&,float)override{}
 void OnRender(Engine& engine,float)override{
  if(Result)return;
  if(expectFailure_){++frames_;const auto receipt=engine.DevelopmentUltraReceipt();const auto stats=engine.GetRendererStats();const bool validationValid=!stats.gpuValidationRequested||(stats.gpuValidationAvailable&&stats.gpuValidationExecuted&&stats.gpuValidationPassed);if(frames_>=4){if(!failureStateValidated_||engine.RenderQuality()!=RenderQuality3D::High||receipt.actual!=DevelopmentUltraQualityMode3D::High||receipt.rendererApplied||receipt.transitionPolicyApplied||receipt.shadowResourcesApplied||receipt.volumetricResolutionApplied||stats.directionalShadowResolution||stats.directionalShadowCascadeCapacity||stats.directionalShadowMapBytes||stats.directionalShadowMapAllocations!=expectedShadowTransactions_||stats.directionalShadowMapReleases!=expectedShadowTransactions_||stats.directionalShadowDescriptorSwitches!=expectedShadowTransactions_*2||stats.directionalShadowResourceGeneration!=expectedShadowTransactions_*2||stats.volumetricAtmosphereWidth||stats.volumetricAtmosphereHeight||stats.volumetricAtmosphereResidentBytes||stats.volumetricAtmosphereResourceCreates||stats.volumetricAtmosphereResourceReleases||stats.pendingGpuReleaseBytes||stats.debugErrorCount||stats.debugWarningCount||!validationValid){Result=7;}else{passed_=true;std::cout<<"rollback exact shadow=0 volumetric=0 shadowTransactions="<<expectedShadowTransactions_<<" pending=0 submitted="<<stats.submittedFrames<<" gbv="<<stats.gpuValidationExecuted<<'\n';}engine.Quit();}return;}
  auto& draw=engine.Draw();const Camera3D camera{{0,90,-240},{0,80,420},{0,1,0},{}};draw.SetViewProjection(CreateViewProjectionMatrix(camera,16.f/9.f));draw.SetBackfaceCulling(false);draw.SetAtmosphere({{.025f,.04f,.075f,1},camera.position,180.f,420.f,true,.008f,0,.02f,.85f});draw.SetDirectionalLight({{-.35f,-.82f,.44f},.12f,{1,.9f,.72f,1},2.2f});draw.SetDirectionalShadowCascades({4,4096,.1f,220.f,.70f,.10f});VolumetricCloudSettings3D clouds{true,180,420,.05f,4.f,0.f,28,28,true,.7f};clouds.maximumDistance=520.f;draw.SetVolumetricClouds(clouds);draw.SetDirectionalLightShafts({true,2.f,.72f,520,10});draw.MeshInstance(mesh_,CreateTransformMatrix({0,-18,650},{},{1500,20,1800}),{.2f,.31f,.14f,1},{.86f,0,0,1},MeshRenderLayer3D::Opaque,{},.25f,false,MaterialClass3D::Terrain,1,true);draw.MeshInstance(mesh_,CreateTransformMatrix({0,85,430},{},{90,105,90}),{.4f,.32f,.22f,1},{.62f,0,0,1},MeshRenderLayer3D::Opaque,{},.25f,false,MaterialClass3D::Surface,1,true);
  ++frames_;const auto receipt=engine.DevelopmentUltraReceipt();if(engine.RenderQuality()==RenderQuality3D::Ultra){const auto stats=engine.GetRendererStats();const bool validationValid=!stats.gpuValidationRequested||(stats.gpuValidationAvailable&&stats.gpuValidationExecuted&&stats.gpuValidationPassed);if(receipt.actual!=DevelopmentUltraQualityMode3D::DevelopmentUltra||!receipt.rendererApplied||!receipt.transitionPolicyApplied||!receipt.shadowResourcesApplied||!receipt.volumetricResolutionApplied||receipt.cloudSamplesApplied||receipt.cloudShadowSamplesApplied||receipt.lightShaftSamplesApplied||receipt.vegetationDensityApplied||receipt.textureBudgetApplied||receipt.meshBudgetApplied||stats.directionalShadowResolution!=4096||stats.directionalShadowCascadeCapacity!=4||stats.volumetricAtmosphereWidth!=1440||stats.volumetricAtmosphereHeight!=810||std::abs(stats.volumetricAtmosphereResolutionScale-.75f)>.0001f||stats.completedGpuFrameSerial<3||stats.gpuTimingSamples<3||stats.pendingGpuReleaseBytes||stats.debugErrorCount||stats.debugWarningCount||!validationValid){Result=3;}else{passed_=true;std::cout<<"actual=ULTRA submitted="<<stats.submittedFrames<<" completed="<<stats.completedGpuFrameSerial<<" timingSamples="<<stats.gpuTimingSamples<<" gpuNs="<<stats.lastGpuFrameNanoseconds<<" pending="<<stats.pendingGpuReleaseBytes<<'\n';}engine.Quit();return;}if(frames_>=90){Result=4;std::cerr<<"Ultra GPU truth did not commit after 90 runtime frames\n";engine.Quit();}
 }
 void OnStop(Engine& engine)override{if(mesh_){engine.DestroyMesh3D(mesh_);engine.WaitRendererIdle3D();mesh_={};}if(!Result&&!passed_)Result=5;}
private:
 MeshHandle3D mesh_{};unsigned frames_{},expectedShadowTransactions_{};bool expectFailure_{},failureStateValidated_{},passed_{};
};
}

int main(){
 try{EngineConfig config{};config.title=L"Development Ultra Engine Runtime";config.width=1920;config.height=1080;config.resizable=false;config.enableDLSS=false;config.enableAutomaticMemoryTrim=false;config.renderQuality=RenderQuality3D::Ultra;Engine engine(config);std::array<wchar_t,32> failure{};const DWORD length=GetEnvironmentVariableW(L"AEVRYND_ULTRA_FAILURE",failure.data(),DWORD(failure.size()));const std::wstring_view value(failure.data(),length<failure.size()?length:0);const bool expectFailure=!value.empty();unsigned expectedShadowTransactions{};if(value==L"apply")EngineDevelopmentUltraTestAccess3D::ForceFailure(engine,EngineDevelopmentUltraTestAccess3D::Failure::RendererApply);else if(value==L"pending"){EngineDevelopmentUltraTestAccess3D::ForceFailure(engine,EngineDevelopmentUltraTestAccess3D::Failure::PostApplyPending);expectedShadowTransactions=1;}else if(expectFailure)return 8;engine.Run(std::make_unique<RuntimeGame>(expectFailure,expectedShadowTransactions),GetModuleHandleW(nullptr),SW_HIDE);}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 6;}return Result;
}
