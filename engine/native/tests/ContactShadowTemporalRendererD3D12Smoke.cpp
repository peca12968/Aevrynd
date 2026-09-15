#include <Aevrynd/Renderer.h>
#include <Aevrynd/Scene3D.h>

#include <Windows.h>

#include <cstring>
#include <cstdlib>
#include <cmath>
#include <iostream>
#include <limits>
#include <optional>

using namespace Aevrynd;

namespace {
bool GpuValidationRequested() {
  char value[8]{};
  return GetEnvironmentVariableA("AEVRYND_GPU_VALIDATION", value,
                                 static_cast<DWORD>(sizeof(value))) > 0 &&
         value[0] == '1';
}

bool ExpectedModeMatches() {
  char value[16]{};
  if (!GetEnvironmentVariableA("AEVRYND_EXPECT_GPU_VALIDATION_MODE", value,
                               static_cast<DWORD>(sizeof(value))))
    return true;
  return (std::strcmp(value, "gbv") == 0) == GpuValidationRequested();
}

#if AEVRYND_INTERNAL_RENDERER_TEST_HOOKS
double MaximumDifference(const D3D12RendererTestAccess3D::LinearHdrCapture3D &a,
                         const D3D12RendererTestAccess3D::LinearHdrCapture3D &b) {
  if (!a.Complete() || !b.Complete() || a.width != b.width || a.height != b.height)
    return std::numeric_limits<double>::infinity();
  double maximum{};
  for (std::size_t i = 0; i < a.pixels.size(); ++i) {
    maximum = (std::max)(maximum, std::abs(double(a.pixels[i].x) - b.pixels[i].x));
    maximum = (std::max)(maximum, std::abs(double(a.pixels[i].y) - b.pixels[i].y));
    maximum = (std::max)(maximum, std::abs(double(a.pixels[i].z) - b.pixels[i].z));
  }
  return maximum;
}

double ResidualRms(const D3D12RendererTestAccess3D::LinearHdrCapture3D &enabledA,
                   const D3D12RendererTestAccess3D::LinearHdrCapture3D &disabledA,
                   const D3D12RendererTestAccess3D::LinearHdrCapture3D &enabledB,
                   const D3D12RendererTestAccess3D::LinearHdrCapture3D &disabledB) {
  if (!enabledA.Complete() || !disabledA.Complete() || !enabledB.Complete() ||
      !disabledB.Complete() || enabledA.width != enabledB.width ||
      enabledA.width != disabledA.width || enabledA.width != disabledB.width ||
      enabledA.height != enabledB.height || enabledA.height != disabledA.height ||
      enabledA.height != disabledB.height)
    return std::numeric_limits<double>::infinity();
  double squared{};
  for (std::size_t i = 0; i < enabledA.pixels.size(); ++i) {
    const std::array<double, 3> residual{
        (enabledA.pixels[i].x - disabledA.pixels[i].x) -
            (enabledB.pixels[i].x - disabledB.pixels[i].x),
        (enabledA.pixels[i].y - disabledA.pixels[i].y) -
            (enabledB.pixels[i].y - disabledB.pixels[i].y),
        (enabledA.pixels[i].z - disabledA.pixels[i].z) -
            (enabledB.pixels[i].z - disabledB.pixels[i].z)};
    for (double value : residual)
      squared += value * value;
  }
  return std::sqrt(squared / double(enabledA.pixels.size() * 3));
}
#endif
} // namespace

int main() {
  if (!ExpectedModeMatches())
    return 1;
  HWND window = CreateWindowExW(0, L"STATIC", L"ContactShadowTemporalTest",
                                WS_OVERLAPPED, 0, 0, 640, 360, nullptr,
                                nullptr, GetModuleHandleW(nullptr), nullptr);
  if (!window)
    return 2;
  int result{};
  try {
    D3D12Renderer renderer;
    renderer.Initialize(window, 640, 360, 640, 360, false);
    renderer.SetQualitySettings({true, true, false, false});
    const Mesh3D box = Mesh3D::CreateUnitBox();
    const auto mesh = renderer.CreateMesh(box.vertices, box.indices);
    const Camera3D camera{{0.f, 0.f, -8.f}, {0.f, 0.f, 5.f}, {0.f, 1.f, 0.f},
                          {ProjectionMode::Perspective, .82f, 20.f, .1f, 100.f}};
    DrawList draw;
    draw.SetViewProjection(CreateViewProjectionMatrix(camera, 16.f / 9.f));
    draw.SetDirectionalLight({{1.f, 0.f, 1.f}, .08f, {1.f, .92f, .78f, 1.f}, 2.2f});
    draw.SetContactShadows({true, 12, 1.5f, .08f, .8f});
    draw.MeshInstance(mesh, CreateTransformMatrix({0, 0, 6}, {}, {8, 8, .5f}),
                      {.48f, .52f, .44f, 1}, {.84f, 0, 0, 1});
    draw.MeshInstance(mesh, CreateTransformMatrix({0, 0, 5}, {}, {2.2f, 2.2f, .25f}),
                      {.55f, .22f, .08f, 1}, {.42f, 0, 0, 1});
    draw.MeshInstance(mesh, CreateTransformMatrix({2.5f, 0, 5}, {}, {1.5f, 1.5f, .25f}),
                      {.18f, .34f, .5f, 1}, {.35f, .1f, 0, 1});
    draw.meshInstances[0].castsShadow = true;
    draw.meshInstances[1].castsShadow = false;
    draw.meshInstances[2].castsShadow = false;

    draw.SetCameraCut3D(true);
    renderer.Render(draw, {.01f, .02f, .03f, 1.f});
    draw.SetCameraCut3D(false);
    const auto reset = renderer.GetStats();
#if AEVRYND_INTERNAL_RENDERER_TEST_HOOKS
    const auto cutCurrent = D3D12RendererTestAccess3D::CaptureContactShadow(
        renderer, D3D12RendererTestAccess3D::ContactShadowCaptureStage3D::Current);
    const auto cutHistory = D3D12RendererTestAccess3D::CaptureContactShadow(
        renderer, D3D12RendererTestAccess3D::ContactShadowCaptureStage3D::History);
    const bool cutSeedExact = cutCurrent && cutHistory && cutCurrent->Complete() &&
                              cutHistory->Complete() &&
                              cutCurrent->width == cutHistory->width &&
                              cutCurrent->height == cutHistory->height &&
                              std::memcmp(cutCurrent->pixels.data(),
                                          cutHistory->pixels.data(),
                                          cutCurrent->pixels.size() * sizeof(Vec2)) == 0;
    if (!cutSeedExact || reset.contactShadowTemporalDispatches != 0) {
      std::cerr << "camera-cut current-to-history seed was not an exact GPU copy\n";
      result = 15;
    }
#endif
    for (unsigned frame = 0; frame < 4; ++frame)
      renderer.Render(draw, {.01f, .02f, .03f, 1.f});
    renderer.WaitIdle();
    const auto stable = renderer.GetStats();
    if (!mesh || !stable.contactShadowsEnabled ||
        !stable.contactShadowTemporalFiltering ||
        stable.contactShadowTemporalDispatches < reset.contactShadowTemporalDispatches + 4 ||
        !stable.contactShadowTemporalResidentBytes ||
        !stable.contactShadowTemporalAllocations ||
        stable.contactShadowTemporalHistoryEligibleFrames <= reset.contactShadowTemporalHistoryEligibleFrames ||
        !reset.contactShadowTemporalHistoryIneligibleFrames ||
        !reset.contactShadowTemporalHistoryResets ||
        stable.contactShadowTemporalHistoryResets != reset.contactShadowTemporalHistoryResets) {
      std::cerr << "integrated stable/reset temporal contract failed\n";
      result = 3;
    }
    if (!result && (stable.velocityReadbackRequests ||
                    stable.velocityReadbackCompletions ||
                    stable.velocityReadbackRejections)) {
      std::cerr << "production render unexpectedly requested velocity readback\n";
      result = 14;
    }

#if AEVRYND_INTERNAL_RENDERER_TEST_HOOKS
    const auto currentContact = D3D12RendererTestAccess3D::CaptureContactShadow(
        renderer, D3D12RendererTestAccess3D::ContactShadowCaptureStage3D::Current);
    const auto historyContact = D3D12RendererTestAccess3D::CaptureContactShadow(
        renderer, D3D12RendererTestAccess3D::ContactShadowCaptureStage3D::History);
    const auto bilateralContact = D3D12RendererTestAccess3D::CaptureContactShadow(
        renderer, D3D12RendererTestAccess3D::ContactShadowCaptureStage3D::Bilateral);
    const auto rawContact = D3D12RendererTestAccess3D::CaptureContactShadow(
        renderer, D3D12RendererTestAccess3D::ContactShadowCaptureStage3D::Raw);
    const auto minimumVisibility = [](const auto &capture) {
      float minimum = 1.f;
      if (capture)
        for (const auto pixel : capture->pixels)
          minimum = (std::min)(minimum, pixel.x);
      return minimum;
    };
    std::cout << " currentMin=" << minimumVisibility(currentContact)
              << " historyMin=" << minimumVisibility(historyContact)
              << " bilateralMin=" << minimumVisibility(bilateralContact);
    float bilateralContactMinimum=1.f;if(bilateralContact)for(const auto pixel:bilateralContact->pixels)bilateralContactMinimum=(std::min)(bilateralContactMinimum,pixel.y);float rawContactMinimum=1.f;if(rawContact)for(const auto pixel:rawContact->pixels)rawContactMinimum=(std::min)(rawContactMinimum,pixel.y);std::cout<<" rawContactMin="<<rawContactMinimum<<" bilateralContactMin="<<bilateralContactMinimum;
    const auto capture = [&]()
        -> std::optional<D3D12RendererTestAccess3D::LinearHdrCapture3D> {
      renderer.WaitIdle();
      return D3D12RendererTestAccess3D::CaptureLinearHdr(renderer, 0.f);
    };
    const auto jitteredCamera = [&](float offset) {
      Camera3D value = camera;
      draw.SetViewProjection(CreateViewProjectionMatrix(value, 16.f / 9.f));
      auto settings=draw.ContactShadows();
      settings.strength=.8f+offset*10.f;
      draw.SetContactShadows(settings);
    };
    jitteredCamera(-.004f);
    renderer.Render(draw, {.01f, .02f, .03f, 1.f});
    const auto stableA = capture();
    const auto stableContactA = D3D12RendererTestAccess3D::CaptureContactShadow(
        renderer, D3D12RendererTestAccess3D::ContactShadowCaptureStage3D::History);
    jitteredCamera(.004f);
    renderer.Render(draw, {.01f, .02f, .03f, 1.f});
    const auto stableB = capture();
    const auto stableContactB = D3D12RendererTestAccess3D::CaptureContactShadow(
        renderer, D3D12RendererTestAccess3D::ContactShadowCaptureStage3D::History);
    draw.SetCameraCut3D(true);
    jitteredCamera(-.004f);
    renderer.Render(draw, {.01f, .02f, .03f, 1.f});
    const auto rawA = capture();
    const auto rawContactA = D3D12RendererTestAccess3D::CaptureContactShadow(
        renderer, D3D12RendererTestAccess3D::ContactShadowCaptureStage3D::Current);
    jitteredCamera(.004f);
    renderer.Render(draw, {.01f, .02f, .03f, 1.f});
    const auto rawB = capture();
    const auto rawContactB = D3D12RendererTestAccess3D::CaptureContactShadow(
        renderer, D3D12RendererTestAccess3D::ContactShadowCaptureStage3D::Current);
    draw.SetCameraCut3D(false);
    auto contactDisabled = draw.ContactShadows();
    contactDisabled.enabled = false;
    draw.SetContactShadows(contactDisabled);
    jitteredCamera(-.004f);
    renderer.Render(draw, {.01f, .02f, .03f, 1.f});
    const auto disabledA = capture();
    jitteredCamera(.004f);
    renderer.Render(draw, {.01f, .02f, .03f, 1.f});
    const auto disabledB = capture();
    contactDisabled.enabled = true;
    draw.SetContactShadows(contactDisabled);
    const auto contactRms=[](const auto& a,const auto& b){if(!a||!b||a->pixels.size()!=b->pixels.size())return double(INFINITY);double sum{};for(size_t index=0;index<a->pixels.size();++index){const double delta=double(a->pixels[index].x)-b->pixels[index].x;sum+=delta*delta;}return std::sqrt(sum/a->pixels.size());};
    const auto bilateralContactRms=[](const auto& a,const auto& b){if(a.pixels.size()!=b.pixels.size())return double(INFINITY);double sum{};for(size_t index=0;index<a.pixels.size();++index){const double delta=double(a.pixels[index].y)-b.pixels[index].y;sum+=delta*delta;}return std::sqrt(sum/a.pixels.size());};
    const double stableVariance=contactRms(stableContactA,stableContactB);
    const double rawVariance=contactRms(rawContactA,rawContactB);
    const double contactSignal = rawA && disabledA
                                     ? MaximumDifference(*rawA, *disabledA)
                                     : INFINITY;
    std::cout << " controlledNoiseStableRms=" << stableVariance
              << " controlledNoiseRawRms=" << rawVariance
              << " contactSignalMax=" << contactSignal;
    if (!result && (!std::isfinite(stableVariance) || !std::isfinite(rawVariance) ||
                    !std::isfinite(contactSignal) || contactSignal <= 1e-7 ||
                    rawVariance <= 1e-7 || stableVariance > rawVariance*.35))
      result = 8;

    jitteredCamera(0.f);
    const auto beforeMoveHistory = D3D12RendererTestAccess3D::CaptureContactShadow(
        renderer, D3D12RendererTestAccess3D::ContactShadowCaptureStage3D::History);
    draw.meshInstances[1].transform =
        CreateTransformMatrix({.7f, 0.f, 5.f}, {}, {2.2f, 2.2f, .25f});
    draw.meshInstances[1].normalTransform =
        CreateNormalMatrix(draw.meshInstances[1].transform);
    renderer.Render(draw, {.01f, .02f, .03f, 1.f});
    const auto movedCurrent = D3D12RendererTestAccess3D::CaptureContactShadow(
        renderer, D3D12RendererTestAccess3D::ContactShadowCaptureStage3D::Current);
    const auto movedHistory = D3D12RendererTestAccess3D::CaptureContactShadow(
        renderer, D3D12RendererTestAccess3D::ContactShadowCaptureStage3D::History);
    double disocclusionMaximum{};size_t disocclusionPixels{};
    double depthDeltaMaximum{};size_t depthOver0001{},depthOver0005{};if(beforeMoveHistory&&movedCurrent&&movedHistory){for(size_t index=0;index<movedCurrent->pixels.size();++index){const double delta=std::abs(double(movedCurrent->pixels[index].y)-beforeMoveHistory->pixels[index].y);depthDeltaMaximum=(std::max)(depthDeltaMaximum,delta);depthOver0001+=delta>.0001;depthOver0005+=delta>.0005;}for(size_t index=0;index<movedCurrent->pixels.size();++index)if(std::abs(movedCurrent->pixels[index].y-beforeMoveHistory->pixels[index].y)>1e-5f){disocclusionMaximum=(std::max)(disocclusionMaximum,double(std::abs(movedHistory->pixels[index].x-movedCurrent->pixels[index].x)));++disocclusionPixels;}}else disocclusionMaximum=INFINITY;
    std::cout << " disocclusionFreshMax=" << disocclusionMaximum<<" disocclusionPixels="<<disocclusionPixels<<" depthDeltaMax="<<depthDeltaMaximum<<" depthOver0001="<<depthOver0001<<" depthOver0005="<<depthOver0005;
    if (!result && (!disocclusionPixels||!std::isfinite(disocclusionMaximum) || disocclusionMaximum > 1.0 / 1024.0))
      result = 9;
    auto temporalDisabled=draw.ContactShadows();
    temporalDisabled.enabled=false;
    draw.SetContactShadows(temporalDisabled);
    renderer.Render(draw,{.01f,.02f,.03f,1.f});
    const auto disabledRaw=D3D12RendererTestAccess3D::CaptureContactShadow(
        renderer,D3D12RendererTestAccess3D::ContactShadowCaptureStage3D::Raw);
    const auto disabledBlur=D3D12RendererTestAccess3D::CaptureContactShadow(
        renderer,D3D12RendererTestAccess3D::ContactShadowCaptureStage3D::Bilateral);
    if(!result&&(!disabledRaw||!disabledBlur||disabledRaw->pixels.size()!=disabledBlur->pixels.size()))
      result=11;
    if(!result)for(size_t index=0;index<disabledRaw->pixels.size();++index)
      if(std::abs(disabledRaw->pixels[index].y-disabledBlur->pixels[index].y)>1.f/255.f||
         std::abs(disabledRaw->pixels[index].y-1.f)>1.f/255.f){result=11;break;}
    temporalDisabled.enabled=true;
    draw.SetContactShadows(temporalDisabled);
    renderer.SetQualitySettings({true,true,false,true});
    const auto paaToggle=renderer.GetStats();
    if(!result&&(paaToggle.contactShadowTemporalResidentBytes||
                 paaToggle.contactShadowTemporalFiltering))
      result=13;
    jitteredCamera(0.f);
    using ContactCapture=D3D12RendererTestAccess3D::ContactShadowCapture3D;
    std::vector<ContactCapture> taaBaseline,taaTemporal;
    for(unsigned frame=0;frame<9;++frame){
      D3D12RendererTestAccess3D::InvalidateContactShadowTemporalHistory(renderer);
      renderer.Render(draw,{.01f,.02f,.03f,1.f});
      const auto sampleCapture=D3D12RendererTestAccess3D::CaptureContactShadow(
          renderer,D3D12RendererTestAccess3D::ContactShadowCaptureStage3D::Bilateral);
      if(sampleCapture)taaBaseline.push_back(*sampleCapture);
    }
    D3D12RendererTestAccess3D::InvalidateContactShadowTemporalHistory(renderer);
    for(unsigned frame=0;frame<13;++frame){
      renderer.Render(draw,{.01f,.02f,.03f,1.f});
      if(frame>=4){const auto sampleCapture=D3D12RendererTestAccess3D::CaptureContactShadow(
          renderer,D3D12RendererTestAccess3D::ContactShadowCaptureStage3D::Bilateral);
        if(sampleCapture)taaTemporal.push_back(*sampleCapture);}
    }
    const auto sequenceRms=[&](const std::vector<ContactCapture>& captures){double sum{};if(captures.size()<2)return double(INFINITY);for(size_t index=1;index<captures.size();++index)sum+=bilateralContactRms(captures[index-1],captures[index]);return sum/double(captures.size()-1);};
    const double taaBaselineDifference=sequenceRms(taaBaseline);
    const double taaFilteredDifference=sequenceRms(taaTemporal);
    const auto beforeRapidFrames=renderer.GetStats();
    for(unsigned frame=0;frame<12;++frame)renderer.Render(draw,{.01f,.02f,.03f,1.f});
    const auto afterRapidFrames=renderer.GetStats();
    // 中文：mutation oracle 固定關閉 presentation jitter，確保 OFF 前後使用相同的 ray phase 與輸入。
    renderer.SetQualitySettings({true,true,false,false});
    const auto median=[](std::vector<uint64_t> values){if(values.empty())return uint64_t{};std::sort(values.begin(),values.end());return values[values.size()/2];};
    std::vector<uint64_t> microOffTimes,microOnTimes;
    D3D12RendererTestAccess3D::SetContactShadowTemporalForcedOff(renderer,true);
    for(unsigned frame=0;frame<40;++frame){renderer.Render(draw,{.01f,.02f,.03f,1.f});if(frame>=8)microOffTimes.push_back(renderer.GetStats().lastGpuFrameNanoseconds);}
    D3D12RendererTestAccess3D::SetContactShadowTemporalForcedOff(renderer,false);
    for(unsigned frame=0;frame<40;++frame){renderer.Render(draw,{.01f,.02f,.03f,1.f});if(frame>=8)microOnTimes.push_back(renderer.GetStats().lastGpuFrameNanoseconds);}
    const uint64_t microOffMedian=median(microOffTimes),microOnMedian=median(microOnTimes);
    // 中文：正式預算以 1080p production path 衡量；OFF/ON 使用完全相同的場景與後處理。
    renderer.Resize(1920,1080);
    DrawList timingDraw=draw;
    Atmosphere3D timingAtmosphere{};
    timingAtmosphere.enabled=true;
    timingDraw.SetAtmosphere(timingAtmosphere);
    auto timingContactSettings=timingDraw.ContactShadows();
    timingContactSettings.maximumSteps=64;
    timingDraw.SetContactShadows(timingContactSettings);
    std::vector<MeshHandle3D> timingAssets;
    timingAssets.reserve(24);
    for(unsigned index=0;index<24;++index){
      const auto timingAsset=renderer.CreateMesh(box.vertices,box.indices);
      timingAssets.push_back(timingAsset);
      const float x=float(int(index%8)-4)*.7f;
      const float y=float(int(index/8)-1)*.7f;
      timingDraw.MeshInstance(timingAsset,CreateTransformMatrix({x,y,4.6f+.08f*float(index%3)}, {}, {.24f,.24f,.2f}),
                              {.25f+.02f*float(index%12),.3f,.38f,1.f},{.35f,0.f,0.f,1.f});
    }
    std::vector<uint64_t> baselineGpuTimes,temporalGpuTimes;
    D3D12RendererTestAccess3D::SetContactShadowTemporalForcedOff(renderer,true);
    renderer.Render(timingDraw,{.01f,.02f,.03f,1.f});
    const auto forcedOffBegin=renderer.GetStats();
    for(unsigned frame=0;frame<130;++frame){renderer.Render(timingDraw,{.01f,.02f,.03f,1.f});const auto value=renderer.GetStats().lastGpuFrameNanoseconds;if(frame>=10&&value)baselineGpuTimes.push_back(value);}
    const auto forcedOffEnd=renderer.GetStats();
    const auto forcedOffOutput=D3D12RendererTestAccess3D::CaptureContactShadow(
        renderer,D3D12RendererTestAccess3D::ContactShadowCaptureStage3D::Bilateral);
    const auto forcedOffRaw=D3D12RendererTestAccess3D::CaptureContactShadow(
        renderer,D3D12RendererTestAccess3D::ContactShadowCaptureStage3D::Raw);
    D3D12RendererTestAccess3D::SetContactShadowTemporalForcedOff(renderer,false);
    for(unsigned frame=0;frame<130;++frame){renderer.Render(timingDraw,{.01f,.02f,.03f,1.f});const auto value=renderer.GetStats().lastGpuFrameNanoseconds;if(frame>=10&&value)temporalGpuTimes.push_back(value);}
    const auto forcedOnEnd=renderer.GetStats();
    const auto forcedOnOutput=D3D12RendererTestAccess3D::CaptureContactShadow(
        renderer,D3D12RendererTestAccess3D::ContactShadowCaptureStage3D::Bilateral);
    // 中文：先讓 ON 填入有效 history，再以相同固定 phase 強制 OFF；輸出必須與先前 spatial-only 完全一致。
    D3D12RendererTestAccess3D::SetContactShadowTemporalForcedOff(renderer,true);
    renderer.Render(timingDraw,{.01f,.02f,.03f,1.f});
    const auto forcedOffAfterHistoryStats=renderer.GetStats();
    const auto forcedOffAfterHistory=D3D12RendererTestAccess3D::CaptureContactShadow(
        renderer,D3D12RendererTestAccess3D::ContactShadowCaptureStage3D::Bilateral);
    D3D12RendererTestAccess3D::SetContactShadowTemporalForcedOff(renderer,false);
    const uint64_t baselineMedian=median(baselineGpuTimes),temporalMedian=median(temporalGpuTimes);
    std::vector<uint64_t> baselineDeviations,temporalDeviations;baselineDeviations.reserve(baselineGpuTimes.size());temporalDeviations.reserve(temporalGpuTimes.size());for(const uint64_t value:baselineGpuTimes)baselineDeviations.push_back(value>baselineMedian?value-baselineMedian:baselineMedian-value);for(const uint64_t value:temporalGpuTimes)temporalDeviations.push_back(value>temporalMedian?value-temporalMedian:temporalMedian-value);const uint64_t baselineMad=median(baselineDeviations),temporalMad=median(temporalDeviations);const uint64_t temporalIncrement=temporalMedian>baselineMedian?temporalMedian-baselineMedian:0;constexpr uint64_t targetFrameNanoseconds=16666667;const double temporalBudgetPercent=100.0*double(temporalIncrement)/double(targetFrameNanoseconds);const double relativeTimingPercent=baselineMedian?100.0*double(temporalIncrement)/double(baselineMedian):INFINITY;const double forcedOutputDifference=forcedOffOutput&&forcedOnOutput?bilateralContactRms(*forcedOffOutput,*forcedOnOutput):INFINITY;const double forcedOffSpatialRepeatDifference=forcedOffOutput&&forcedOffAfterHistory?bilateralContactRms(*forcedOffOutput,*forcedOffAfterHistory):INFINITY;
    std::cout << " taaBaselineDifference=" << taaBaselineDifference
              << " taaFilteredDifference=" << taaFilteredDifference
              << " microOffMedian=" << microOffMedian
              << " microOnMedian=" << microOnMedian
              << " baselineGpuMedian=" << baselineMedian
              << " baselineGpuMad=" << baselineMad
              << " temporalGpuMedian=" << temporalMedian
              << " temporalGpuIncrement=" << temporalIncrement
              << " temporalFrameBudgetPercent=" << temporalBudgetPercent
              << " temporalRelativePercent=" << relativeTimingPercent
              << " forcedOffOnOutputRms=" << forcedOutputDifference
              << " forcedOffSpatialRepeatRms=" << forcedOffSpatialRepeatDifference
              << " temporalGpuMad=" << temporalMad;
    if(!result&&(!std::isfinite(taaBaselineDifference)||!std::isfinite(taaFilteredDifference)||
                 taaBaselineDifference<=1e-7||taaFilteredDifference>taaBaselineDifference*.5||
                 afterRapidFrames.contactShadowTemporalAllocations!=beforeRapidFrames.contactShadowTemporalAllocations||
                 forcedOffEnd.contactShadowTemporalDispatches!=forcedOffBegin.contactShadowTemporalDispatches||
                 forcedOnEnd.contactShadowTemporalDispatches<=forcedOffEnd.contactShadowTemporalDispatches||
                 forcedOffAfterHistoryStats.contactShadowTemporalDispatches!=forcedOnEnd.contactShadowTemporalDispatches||
                 forcedOffEnd.contactShadowTemporalAllocations!=forcedOffBegin.contactShadowTemporalAllocations||
                 forcedOnEnd.contactShadowTemporalAllocations!=forcedOffEnd.contactShadowTemporalAllocations||
                 !forcedOffOutput||!forcedOffOutput->Complete()||!forcedOffRaw||!forcedOffRaw->Complete()||
                 !forcedOnOutput||!forcedOnOutput->Complete()||!std::isfinite(forcedOutputDifference)||forcedOutputDifference<=1e-7||
                 !forcedOffAfterHistory||!forcedOffAfterHistory->Complete()||!std::isfinite(forcedOffSpatialRepeatDifference)||forcedOffSpatialRepeatDifference>1e-7||
                 !baselineMedian||!temporalMedian||temporalIncrement>200000||temporalIncrement>targetFrameNanoseconds/20))
      result=12;
    for(const auto timingAsset:timingAssets)renderer.DestroyMesh(timingAsset);
    renderer.SetQualitySettings({true,true,false,false});
#endif

    renderer.Resize(1279, 719);
    renderer.Render(draw, {.01f, .02f, .03f, 1.f});
    const auto resized = renderer.GetStats();
    renderer.Render(draw, {.01f, .02f, .03f, 1.f});
    const auto recovered = renderer.GetStats();
    if (!result &&
        (resized.contactShadowTemporalHistoryResets <= stable.contactShadowTemporalHistoryResets ||
         resized.contactShadowTemporalHistoryIneligibleFrames <= stable.contactShadowTemporalHistoryIneligibleFrames ||
         recovered.contactShadowTemporalHistoryEligibleFrames <= resized.contactShadowTemporalHistoryEligibleFrames ||
         resized.ambientOcclusionWidth != 640 || resized.ambientOcclusionHeight != 360))
      result = 4;

    DrawList empty;
    renderer.SetQualitySettings({false, true, false, false});
    renderer.Render(empty, {0.f, 0.f, 0.f, 1.f});
    renderer.WaitIdle();
    const auto contactReleased = renderer.GetStats();
    if (!result && (contactReleased.contactShadowTemporalFiltering ||
                    contactReleased.contactShadowTemporalResidentBytes ||
                    contactReleased.contactShadowTemporalReleases <= stable.contactShadowTemporalReleases))
      result = 10;
    renderer.SetQualitySettings({false, false, false, false});
    renderer.Render(empty, {0.f, 0.f, 0.f, 1.f});
    renderer.WaitIdle();
    renderer.TrimTransientMemory();
    const auto released = renderer.GetStats();
    if (!result && (released.contactShadowTemporalFiltering ||
                    released.contactShadowTemporalResidentBytes ||
                    released.contactShadowTemporalReleases <= stable.contactShadowTemporalReleases))
      result = 5;
    if (!result && GpuValidationRequested() &&
        (!released.gpuValidationAvailable || !released.gpuValidationExecuted ||
         !released.gpuValidationPassed || released.debugErrorCount ||
         released.debugWarningCount)) {
      for (size_t index = 0; index < released.debugWarningIdCount; ++index)
        std::cerr << "warning[" << released.debugWarningIds[index] << "] "
                  << released.debugWarningDescriptions[index] << '\n';
      for (size_t index = 0; index < released.debugErrorIdCount; ++index)
        std::cerr << "error[" << released.debugErrorIds[index] << "] "
                  << released.debugErrorDescriptions[index] << '\n';
      result = 6;
    }
    renderer.DestroyMesh(mesh);
    std::cout << "ContactShadowTemporalRendererD3D12 mode="
              << (GpuValidationRequested() ? "gbv" : "normal")
              << " dispatches=" << stable.contactShadowTemporalDispatches
              << " eligibleFrames=" << stable.contactShadowTemporalHistoryEligibleFrames
              << " ineligibleFrames=" << resized.contactShadowTemporalHistoryIneligibleFrames
              << " residentBytes=" << stable.contactShadowTemporalResidentBytes
              << " gpuValidationAvailable=" << released.gpuValidationAvailable
              << " gpuValidationExecuted=" << released.gpuValidationExecuted
              << " gpuValidationPassed=" << released.gpuValidationPassed
              << " debugErrors=" << released.debugErrorCount
              << " debugWarnings=" << released.debugWarningCount
              << '\n';
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    result = 7;
  }
  DestroyWindow(window);
  return result;
}
