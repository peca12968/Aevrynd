#include <Aevrynd/Camera.h>
#include <Aevrynd/MeshLoader.h>
#include <Aevrynd/Renderer.h>
#include "PairedBenchmarkStatistics.h"
#include <algorithm>
#include <chrono>
#include <iostream>
#include <vector>
#include <windows.h>
using namespace Aevrynd;
namespace {
uint64_t P(std::vector<uint64_t> values, size_t p) {
  return Test::LowerPercentile(std::move(values), p);
}
int64_t SignedP(std::vector<int64_t> values, size_t p) {
  std::ranges::sort(values);
  return values[(values.size() - 1) * p / 100];
}
uint64_t Mad(const std::vector<uint64_t> &values) {
  const uint64_t median = P(values, 50);
  std::vector<uint64_t> deviations;
  deviations.reserve(values.size());
  for (uint64_t value : values)
    deviations.push_back(value > median ? value - median : median - value);
  return P(std::move(deviations), 50);
}
DrawList Scene(MeshHandle3D wall, MeshHandle3D mesh, bool hidden) {
  DrawList d;
  Camera3D camera{{0, 0, -8}, {0, 0, 0}, {0, 1, 0}, {}};
  d.SetViewProjection(CreateViewProjectionMatrix(camera, 1920.f / 1080.f));
  d.MeshInstance(wall, CreateScaleMatrix({12, 8, .2f}), {1, 1, 1, 1}, {},
                 MeshRenderLayer3D::Opaque, {}, .25f, false,
                 MaterialClass3D::Surface,
                 std::numeric_limits<float>::quiet_NaN(), false);
  for (size_t i = 0; i < 4096; ++i) {
    const float x = float(int(i % 64) - 32) * .12f,
                y = float(int(i / 64) - 32) * .12f;
    d.MeshInstance(mesh,
                   CreateTranslationMatrix({x, y, hidden ? 2.f : -2.f}) *
                       CreateScaleMatrix({.08f, .08f, .08f}),
                   {1, 1, 1, 1}, {}, MeshRenderLayer3D::Opaque, {}, .25f, false,
                   MaterialClass3D::Surface,
                   std::numeric_limits<float>::quiet_NaN(), false);
  }
  d.SetStaticMeshSubmission3D(d.FreezeMeshInstances3D());
  return d;
}
uint64_t Sample(D3D12Renderer &r, const DrawList &d) {
  const auto b = std::chrono::steady_clock::now();
  r.Render(d, {.04f, .06f, .09f, 1});
  r.WaitIdle();
  return uint64_t(std::chrono::duration_cast<std::chrono::nanoseconds>(
                      std::chrono::steady_clock::now() - b)
                      .count());
}
struct IsolatedDistribution {
  std::vector<uint64_t> total, prepass, build, cull, color;
};
IsolatedDistribution Isolated(D3D12Renderer &renderer, const DrawList &draw,
                              const char *name) {
  for (size_t index = 0; index < 16; ++index)
    Sample(renderer, draw);
  IsolatedDistribution result;
  for (size_t index = 0; index < 1000; ++index) {
    Sample(renderer, draw);
    const RendererStats stats = renderer.GetStats();
    result.total.push_back(stats.lastGpuFrameNanoseconds);
    result.prepass.push_back(stats.lastHzbPrepassGpuNanoseconds);
    result.build.push_back(stats.lastHzbBuildGpuNanoseconds);
    result.cull.push_back(stats.lastHzbCullGpuNanoseconds);
    result.color.push_back(stats.lastHzbColorGpuNanoseconds);
  }
  std::cout << name << "_isolated=" << P(result.total, 50) << '/'
            << P(result.total, 95) << '/' << P(result.total, 99) << '/'
            << P(result.total, 100) << " pre=" << P(result.prepass, 50) << '/'
            << P(result.prepass, 95) << '/' << P(result.prepass, 99) << '/'
            << P(result.prepass, 100) << " build=" << P(result.build, 50) << '/'
            << P(result.build, 95) << '/' << P(result.build, 99) << '/'
            << P(result.build, 100) << " cull=" << P(result.cull, 50) << '/'
            << P(result.cull, 95) << '/' << P(result.cull, 99) << '/'
            << P(result.cull, 100) << " color=" << P(result.color, 50) << '/'
            << P(result.color, 95) << '/' << P(result.color, 99) << '/'
            << P(result.color, 100) << '\n';
  return result;
}
bool Pair(D3D12Renderer &off, D3D12Renderer &on, const DrawList &a,
          const DrawList &b, const char *name, bool hidden) {
  const bool injectCpuRegression =
      GetEnvironmentVariableA("AEVRYND_TEST_HZB_CPU_REGRESSION", nullptr, 0) >
      0;
  for (size_t i = 0; i < 8; ++i) {
    Sample(off, a);
    Sample(on, b);
  }
  std::vector<uint64_t> ag, bg, ac, bc, aprep, bprep, selection, prepass, build,
      cull, prepassGpu, buildGpu, cullGpu, colorGpu, phaseSumGpu,
      phaseRemainderGpu;
  std::vector<int64_t> pairedGpuDelta, pairedCpuDelta;
  std::vector<uint64_t> pairedCpuOff, offControls;
  size_t wins{};
  bool warm = true;
  for (size_t i = 0; i < 101; ++i) {
    uint64_t aw1{}, aw2{}, bw1{}, bw2{};
    RendererStats as1{}, as2{}, bs1{}, bs2{};
    auto takeOff = [&] {
      auto w = Sample(off, a);
      auto s = off.GetStats();
      return std::pair{w, s};
    };
    auto takeOn = [&] {
      auto w = Sample(on, b);
      auto s = on.GetStats();
      return std::pair{w, s};
    };
    if (i & 1) {
      auto n1 = takeOn(), o1 = takeOff(), o2 = takeOff(), n2 = takeOn();
      bw1 = n1.first;
      bs1 = n1.second;
      aw1 = o1.first;
      as1 = o1.second;
      aw2 = o2.first;
      as2 = o2.second;
      bw2 = n2.first;
      bs2 = n2.second;
    } else {
      auto o1 = takeOff(), n1 = takeOn(), n2 = takeOn(), o2 = takeOff();
      aw1 = o1.first;
      as1 = o1.second;
      bw1 = n1.first;
      bs1 = n1.second;
      bw2 = n2.first;
      bs2 = n2.second;
      aw2 = o2.first;
      as2 = o2.second;
    }
    const uint64_t aw = (aw1 + aw2) / 2, bw = (bw1 + bw2) / 2;
    ac.push_back(aw);
    bc.push_back(bw);
    const auto as = as2, bs = bs2;
    ag.push_back(as.lastGpuFrameNanoseconds);
    bg.push_back(bs.lastGpuFrameNanoseconds);
    const uint64_t acpu1 = as1.lastRenderPreparationCpuNanoseconds +
                               as1.lastRenderCommandRecordingCpuNanoseconds,
                   acpu2 = as2.lastRenderPreparationCpuNanoseconds +
                               as2.lastRenderCommandRecordingCpuNanoseconds,
                   acpuPair = (acpu1 + acpu2) / 2;
    uint64_t bcpuPair = (bs1.lastRenderPreparationCpuNanoseconds +
                               bs1.lastRenderCommandRecordingCpuNanoseconds +
                               bs2.lastRenderPreparationCpuNanoseconds +
                               bs2.lastRenderCommandRecordingCpuNanoseconds) /
                              2;
    if (injectCpuRegression)
      bcpuPair = (std::max)(bcpuPair, (acpuPair * 106 + 99) / 100);
    aprep.push_back(acpuPair);
    bprep.push_back(bcpuPair);
    pairedCpuOff.push_back(acpuPair);
    pairedCpuDelta.push_back(int64_t(bcpuPair) - int64_t(acpuPair));
    // 每個 quartet 內兩次 off 的正規化差值是 stationarity control；這會
    // 消除長時間 benchmark 中的 CPU 位準 phase shift，但保留前後漂移。
    const int64_t offDifference = int64_t(acpu2) - int64_t(acpu1);
    const int64_t normalizedControl =
        1000000 + offDifference * 1000000 / int64_t((std::max)(acpuPair, 1ull));
    offControls.push_back(uint64_t((std::max)(normalizedControl, int64_t{1})));
    warm &= !bs1.lastStaticMeshSubmissionPreparationIterations &&
            !bs1.lastStaticMeshSubmissionCopiedBytes &&
            !bs2.lastStaticMeshSubmissionPreparationIterations &&
            !bs2.lastStaticMeshSubmissionCopiedBytes;
    selection.push_back(bs.lastHzbSelectionCpuNanoseconds);
    prepass.push_back(bs.lastHzbPrepassRecordingCpuNanoseconds);
    build.push_back(bs.lastHzbBuildRecordingCpuNanoseconds);
    cull.push_back(bs.lastHzbCullRecordingCpuNanoseconds);
    prepassGpu.push_back(bs.lastHzbPrepassGpuNanoseconds);
    buildGpu.push_back(bs.lastHzbBuildGpuNanoseconds);
    cullGpu.push_back(bs.lastHzbCullGpuNanoseconds);
    colorGpu.push_back(bs.lastHzbColorGpuNanoseconds);
    const uint64_t phaseSum =
        bs.lastHzbPrepassGpuNanoseconds + bs.lastHzbBuildGpuNanoseconds +
        bs.lastHzbCullGpuNanoseconds + bs.lastHzbColorGpuNanoseconds;
    phaseSumGpu.push_back(phaseSum);
    phaseRemainderGpu.push_back(bg.back() > phaseSum ? bg.back() - phaseSum
                                                     : 0);
    pairedGpuDelta.push_back(int64_t(bg.back()) - int64_t(ag.back()));
    wins += bg.back() < ag.back();
  }
  const auto am = P(ag, 50), bm = P(bg, 50), ap = P(ag, 95), bp = P(bg, 95),
             acpu = P(aprep, 50), bcpu = P(bprep, 50);
  const double gain = 1. - double(bm) / am, p95Gain = 1. - double(bp) / ap,
               winRate = double(wins) / 101.,
               cpuRegression = double(bcpu) / acpu - 1.;
  const auto s = on.GetStats();
  std::cout << name << "_gpu=" << am << "->" << bm << " " << name
            << "_p95=" << ap << "->" << bp << " gpu_mad=" << Mad(bg)
            << " gain=" << gain << " p95_gain=" << p95Gain
            << " wins=" << winRate << " wall=" << P(ac, 50) << "->" << P(bc, 50)
            << " prep_record=" << acpu << "->" << bcpu
            << " prep_p95=" << P(aprep, 95) << "->" << P(bprep, 95)
            << " cpu_regression=" << cpuRegression
            << " selection=" << P(selection, 50) << '/' << P(selection, 95)
            << '/' << Mad(selection) << " prepass=" << P(prepass, 50) << '/'
            << P(prepass, 95) << '/' << Mad(prepass)
            << " build=" << P(build, 50) << '/' << P(build, 95) << '/'
            << Mad(build) << " cull=" << P(cull, 50) << '/' << P(cull, 95)
            << '/' << Mad(cull) << " occluded=" << s.lastHzbOccluded
            << " prepasses=" << s.hzbDepthPrepasses
            << " builds=" << s.hzbBuildDispatches
            << " bytes=" << s.hzbOcclusionResidentBytes
            << " pre_gpu=" << P(prepassGpu, 50) << '/' << P(prepassGpu, 95)
            << '/' << P(prepassGpu, 99) << '/' << P(prepassGpu, 100)
            << " build_gpu=" << P(buildGpu, 50) << '/' << P(buildGpu, 95) << '/'
            << P(buildGpu, 99) << '/' << P(buildGpu, 100)
            << " cull_gpu=" << P(cullGpu, 50) << '/' << P(cullGpu, 95) << '/'
            << P(cullGpu, 99) << '/' << P(cullGpu, 100)
            << " color_gpu=" << P(colorGpu, 50) << '/' << P(colorGpu, 95) << '/'
            << P(colorGpu, 99) << '/' << P(colorGpu, 100)
            << " phase_sum=" << P(phaseSumGpu, 50) << '/' << P(phaseSumGpu, 95)
            << '/' << P(phaseSumGpu, 99) << '/' << P(phaseSumGpu, 100)
            << " remainder=" << P(phaseRemainderGpu, 50) << '/'
            << P(phaseRemainderGpu, 95) << '/' << P(phaseRemainderGpu, 99)
            << '/' << P(phaseRemainderGpu, 100)
            << " paired_delta=" << SignedP(pairedGpuDelta, 50) << '/'
            << SignedP(pairedGpuDelta, 95) << '/' << SignedP(pairedGpuDelta, 99)
            << '/' << SignedP(pairedGpuDelta, 100) << '\n';
  const bool gpuGate = hidden ? gain >= .20 && winRate >= .70 : gain >= -.05;
  const auto environment = Test::AnalyzePairedEnvironment(offControls);
  const bool environmentGate = environment.valid;
  const int64_t pairedCpu = SignedP(pairedCpuDelta, 50);
  const bool cpuGate = hidden ? pairedCpu <= 10000
                              : Test::PairedCpuRegressionGate(
                                    pairedCpuOff, pairedCpuDelta);
  std::cout << " paired_cpu_delta=" << pairedCpu
            << " off_block_drift=" << environment.blockDrift
            << " off_outlier_fraction=" << environment.outlierFraction
            << " off_longest_outlier_burst="
            << environment.longestOutlierBurst << " gpu_gate=" << gpuGate
            << " cpu_gate=" << cpuGate
            << " environment_gate=" << environmentGate << " warm_gate=" << warm
            << '\n';
  return gpuGate && cpuGate && environmentGate && warm;
}
} // namespace
int main() {
  DWORD_PTR process{}, system{};
  GetProcessAffinityMask(GetCurrentProcess(), &process, &system);
  DWORD_PTR candidates = process & ~DWORD_PTR{1};
  DWORD_PTR selected = candidates ? (candidates & (~candidates + 1))
                                  : (process & (~process + 1));
  const DWORD_PTR previous =
      SetThreadAffinityMask(GetCurrentThread(), selected);
  HINSTANCE h = GetModuleHandleW(nullptr);
  WNDCLASSW wc{};
  wc.lpfnWndProc = DefWindowProcW;
  wc.hInstance = h;
  wc.lpszClassName = L"AevryndHzbBenchmark";
  RegisterClassW(&wc);
  HWND a = CreateWindowW(wc.lpszClassName, L"off", WS_OVERLAPPED, 0, 0, 1920,
                         1080, nullptr, nullptr, h, nullptr),
       b = CreateWindowW(wc.lpszClassName, L"on", WS_OVERLAPPED, 0, 0, 1920,
                         1080, nullptr, nullptr, h, nullptr);
  if (!a || !b)
    return 1;
  D3D12Renderer off, on;
  off.Initialize(a, 1920, 1080, 1920, 1080, false);
  on.Initialize(b, 1920, 1080, 1920, 1080, false);
  for (auto *r : {&off, &on}) {
    r->SetQualitySettings({false, false, false, false});
    r->SetGpuInstanceCullingSettings(
        {true, 500.f, GpuInstanceSubmissionMode3D::MultiGroup});
  }
  off.SetHzbOcclusionSettings3D({HzbOcclusionQuality3D::Off});
  on.SetHzbOcclusionSettings3D(
      {HzbOcclusionQuality3D::Conservative, 2048, 16, 200000, 64.f, .0005f});
  const Mesh3D box = Mesh3D::CreateUnitBox(),
               sphere = Mesh3D::CreateUnitSphere(16, 12);
  const auto aw = off.CreateMesh(box.vertices, box.indices),
             bw = on.CreateMesh(box.vertices, box.indices),
             am = off.CreateMesh(sphere.vertices, sphere.indices),
             bm = on.CreateMesh(sphere.vertices, sphere.indices);
  if (!aw || !bw || !am || !bm)
    return 2;
  const auto ah = Scene(aw, am, true), bh = Scene(bw, bm, true),
             av = Scene(aw, am, false), bv = Scene(bw, bm, false);
  const IsolatedDistribution isolatedOff = Isolated(off, ah, "hidden_off"),
                             isolatedOn = Isolated(on, bh, "hidden_on");
  const double isolatedP95Gain =
      1. - double(P(isolatedOn.total, 95)) / P(isolatedOff.total, 95);
  std::cout << "isolated_p95_gain=" << isolatedP95Gain << '\n';
  const bool isolated = isolatedP95Gain >= .15,
             hidden = Pair(off, on, ah, bh, "hidden", true),
             ordinary = Pair(off, on, av, bv, "ordinary", false);
  const auto stats = on.GetStats();
  const bool warm = stats.lastStaticMeshSubmissionPreparationIterations == 0 &&
                    !stats.lastStaticMeshSubmissionCopiedBytes;
  off.DestroyMesh(aw);
  off.DestroyMesh(am);
  on.DestroyMesh(bw);
  on.DestroyMesh(bm);
  off.WaitIdle();
  on.WaitIdle();
  DestroyWindow(a);
  DestroyWindow(b);
  if (previous)
    SetThreadAffinityMask(GetCurrentThread(), previous);
  const bool pass = isolated && hidden && ordinary && warm;
  std::cout << " isolated_gate=" << isolated << " hidden_gate=" << hidden
            << " ordinary_gate=" << ordinary << " final_warm_gate=" << warm
            << '\n';
  return pass ? 0 : 3;
}
