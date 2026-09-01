#include <Aevrynd/Camera.h>
#include <Aevrynd/Mesh3D.h>
#include <Aevrynd/Renderer.h>
#include <Windows.h>
#include <iostream>

using namespace Aevrynd;

int main() {
#if AEVRYND_INTERNAL_RENDERER_TEST_HOOKS
  std::cerr << "Production contract must compile without internal hooks\n";
  return 1;
#else
  const HINSTANCE instance = GetModuleHandleW(nullptr);
  const wchar_t *className = L"AevryndVolumetricAtmosphereProductionContract";
  WNDCLASSW windowClass{};
  windowClass.lpfnWndProc = DefWindowProcW;
  windowClass.hInstance = instance;
  windowClass.lpszClassName = className;
  RegisterClassW(&windowClass);
  const HWND window = CreateWindowW(className, L"production contract",
                                    WS_OVERLAPPED, 0, 0, 320, 180, nullptr,
                                    nullptr, instance, nullptr);
  if (!window)
    return 2;

  int result{};
  {
    D3D12Renderer renderer;
    renderer.Initialize(window, 320, 180, 320, 180, false);
    const auto initialized = renderer.GetStats();
    if (initialized.volumetricAtmosphereDiagnosticsAvailable ||
        initialized.volumetricAtmosphereMutationAvailable)
      result = 3;

    // 舊環境變數在 production build 不可啟用 mutation。
    SetEnvironmentVariableW(L"AEVRYND_VOLUMETRIC_LEGACY_DEPTH_TEST", L"1");
    SetEnvironmentVariableW(L"AEVRYND_VOLUMETRIC_NEAREST_TEST", L"1");
    SetEnvironmentVariableW(L"AEVRYND_VOLUMETRIC_LEGACY_PLANAR_NOISE_TEST", L"1");
    SetEnvironmentVariableW(L"AEVRYND_VOLUMETRIC_NO_PATH_CAP_TEST", L"1");
    SetEnvironmentVariableW(L"AEVRYND_VOLUMETRIC_ADDITIVE_SHAFT_TEST", L"1");
    SetEnvironmentVariableW(L"AEVRYND_VOLUMETRIC_WHOLE_SEGMENT_TEST", L"1");
    DrawList draw{DrawListCapacityProfile::Minimal2D};
    Camera3D camera{{0, 90, -220}, {0, 130, 420}, {0, 1, 0}, {}};
    camera.projection.farPlane = 2400.f;
    draw.SetViewProjection(CreateViewProjectionMatrix(camera, 16.f / 9.f));
    draw.SetAtmosphere({{.025f, .04f, .075f, 1}, camera.position, 210.f,
                        360.f, true, .008f, 0, .02f, .85f});
    draw.SetVolumetricClouds(
        {true, 210, 360, .05f, 4.f, 0.f, 24, 14, false, 0.f});
    draw.SetDirectionalLightShafts({true, 2.f, .72f, 520, 4});
    const Mesh3D box = Mesh3D::CreateUnitBox();
    const MeshHandle3D mesh = renderer.CreateMesh(box.vertices, box.indices);
    if (!mesh)
      result = 4;
    draw.MeshInstance(mesh, CreateTransformMatrix({0, -10, 500}, {},
                                                   {900, 20, 1200}),
                      {.2f, .31f, .14f, 1}, {.86f, 0, 0, 1},
                      MeshRenderLayer3D::Opaque);
    for (unsigned frame = 0; frame < 8; ++frame)
      renderer.Render(draw, {.015f, .025f, .05f, 1});
    renderer.WaitIdle();
    SetEnvironmentVariableW(L"AEVRYND_VOLUMETRIC_LEGACY_DEPTH_TEST", nullptr);
    SetEnvironmentVariableW(L"AEVRYND_VOLUMETRIC_NEAREST_TEST", nullptr);
    SetEnvironmentVariableW(L"AEVRYND_VOLUMETRIC_LEGACY_PLANAR_NOISE_TEST", nullptr);
    SetEnvironmentVariableW(L"AEVRYND_VOLUMETRIC_NO_PATH_CAP_TEST", nullptr);
    SetEnvironmentVariableW(L"AEVRYND_VOLUMETRIC_ADDITIVE_SHAFT_TEST", nullptr);
    SetEnvironmentVariableW(L"AEVRYND_VOLUMETRIC_WHOLE_SEGMENT_TEST", nullptr);

    const auto stats = renderer.GetStats();
    if (!stats.volumetricAtmosphereDispatched ||
        stats.volumetricAtmosphereDiagnosticsAvailable ||
        stats.volumetricAtmosphereDiagnosticsEnabled ||
        stats.volumetricAtmosphereMutationAvailable ||
        stats.volumetricAtmosphereTemporalAcceptedPixels ||
        stats.volumetricAtmosphereTemporalRejectedPixels ||
        stats.volumetricAtmosphereTemporalInvalidPixels ||
        stats.volumetricAtmosphereTemporalDiagnosticFrames ||
        stats.volumetricAtmosphereResourceCreates != 5 ||
        !stats.volumetricAtmosphereAppliedFingerprint ||
        stats.volumetricAtmosphereAppliedCloudSamples != 14 ||
        stats.volumetricAtmosphereAppliedShaftSamples != 4 ||
        stats.volumetricAtmosphereBlueNoiseBytes != 4096 ||
        stats.debugErrorCount || stats.debugWarningCount)
      result = 4;
    std::cout << "production diagnosticsAvailable="
              << stats.volumetricAtmosphereDiagnosticsAvailable
              << " mutationAvailable="
              << stats.volumetricAtmosphereMutationAvailable
              << " counters="
              << stats.volumetricAtmosphereTemporalAcceptedPixels << '/'
              << stats.volumetricAtmosphereTemporalRejectedPixels << '/'
              << stats.volumetricAtmosphereTemporalInvalidPixels
              << " diagnosticFrames="
              << stats.volumetricAtmosphereTemporalDiagnosticFrames
              << " resources=" << stats.volumetricAtmosphereResourceCreates
              << '\n';
    renderer.DestroyMesh(mesh);
    renderer.WaitIdle();
  }
  DestroyWindow(window);
  return result;
#endif
}
