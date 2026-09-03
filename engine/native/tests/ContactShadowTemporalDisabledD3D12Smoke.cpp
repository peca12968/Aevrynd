#include <Aevrynd/Renderer.h>
#include <Aevrynd/Camera.h>
#include <Aevrynd/Mesh3D.h>
#include <Windows.h>
#include <iostream>

using namespace Aevrynd;

int main() {
  HWND window = CreateWindowExW(0, L"STATIC", L"ContactTemporalDisabled",
                                WS_OVERLAPPED, 0, 0, 320, 180, nullptr,
                                nullptr, GetModuleHandleW(nullptr), nullptr);
  if (!window)
    return 1;
  int result{};
  try {
    D3D12Renderer renderer;
    renderer.Initialize(window, 320, 180, 320, 180, false);
    renderer.SetQualitySettings({true, true, false, false});
    const Mesh3D box = Mesh3D::CreateUnitBox();
    const auto mesh = renderer.CreateMesh(box.vertices, box.indices);
    const Camera3D camera{{0, 3, -6}, {0, 0, 4}, {0, 1, 0}, {}};
    DrawList draw;
    draw.SetViewProjection(CreateViewProjectionMatrix(camera, 16.f / 9.f));
    draw.SetContactShadows({false, 12, 1.5f, .04f, .8f});
    draw.MeshInstance(mesh, CreateTransformMatrix({0, 0, 4}, {}, {3, 1, 3}));
    draw.meshInstances[0].castsShadow = true;
    renderer.Render(draw, {0, 0, 0, 1});
    renderer.WaitIdle();
    renderer.TrimTransientMemory();
    const auto stats = renderer.GetStats();
    if (stats.contactShadowTemporalFiltering ||
        stats.contactShadowTemporalDispatches ||
        stats.contactShadowTemporalResidentBytes ||
        stats.contactShadowTemporalAllocations) {
      std::cerr << "disabled contact shadow allocated or dispatched temporal resources\n";
      result = 2;
    }
    renderer.DestroyMesh(mesh);
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    result = 3;
  }
  DestroyWindow(window);
  return result;
}
