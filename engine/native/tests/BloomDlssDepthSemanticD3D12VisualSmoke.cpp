#include <Aevrynd/Renderer.h>
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <wincodec.h>
#include <wrl/client.h>

using namespace Aevrynd;
using Microsoft::WRL::ComPtr;
namespace {
struct Image {
  UINT width{}, height{};
  std::vector<uint8_t> rgba;
};
struct Capture {
  Image image{};
  RendererStats stats{};
  bool valid{};
};
bool Decode(const std::filesystem::path &path, Image &image) {
  ComPtr<IWICImagingFactory> factory;
  ComPtr<IWICBitmapDecoder> decoder;
  ComPtr<IWICBitmapFrameDecode> frame;
  ComPtr<IWICFormatConverter> converter;
  if (FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr,
                              CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory))) ||
      FAILED(factory->CreateDecoderFromFilename(
          path.c_str(), nullptr, GENERIC_READ, WICDecodeMetadataCacheOnLoad,
          &decoder)) ||
      FAILED(decoder->GetFrame(0, &frame)) ||
      FAILED(frame->GetSize(&image.width, &image.height)) ||
      FAILED(factory->CreateFormatConverter(&converter)) ||
      FAILED(converter->Initialize(frame.Get(), GUID_WICPixelFormat32bppRGBA,
                                   WICBitmapDitherTypeNone, nullptr, 0,
                                   WICBitmapPaletteTypeCustom)))
    return false;
  image.rgba.resize(size_t(image.width) * image.height * 4);
  return SUCCEEDED(converter->CopyPixels(
      nullptr, image.width * 4, UINT(image.rgba.size()), image.rgba.data()));
}
void Rectangle(DrawList &draw, float left, float right, float depth,
               Color color) {
  draw.WorldTriangle({left, -1, depth}, {left, 1, depth}, {right, 1, depth},
                     color);
  draw.WorldTriangle({left, -1, depth}, {right, 1, depth}, {right, -1, depth},
                     color);
}
double Luminance(const Image &image, unsigned x, unsigned y) {
  const size_t offset = (size_t(y) * image.width + x) * 4;
  return .2126 * image.rgba[offset] + .7152 * image.rgba[offset + 1] +
         .0722 * image.rgba[offset + 2];
}
double RegionMean(const Image &image, unsigned left, unsigned right) {
  double sum{};
  size_t count{};
  for (unsigned y = 24; y + 24 < image.height; ++y)
    for (unsigned x = left; x < right; ++x) {
      sum += Luminance(image, x, y);
      ++count;
    }
  return count ? sum / count : 0;
}
double PositiveDelta(const Image &value, const Image &baseline, unsigned left,
                     unsigned right) {
  double sum{};
  for (unsigned y = 24; y + 24 < value.height; ++y)
    for (unsigned x = left; x < right; ++x)
      sum +=
          (std::max)(0.0, Luminance(value, x, y) - Luminance(baseline, x, y));
  return sum;
}
bool Validation(const RendererStats &stats, const std::string &mode) {
  const bool clean = !stats.debugErrorCount && !stats.debugWarningCount;
  if (mode == "normal")
    return clean && !stats.gpuValidationRequested &&
           !stats.gpuValidationAvailable && !stats.gpuValidationExecuted &&
           !stats.gpuValidationPassed && !stats.debugLayerEnabled;
  if (mode == "gbv")
    return clean && stats.gpuValidationRequested &&
           stats.gpuValidationAvailable && stats.gpuValidationExecuted &&
           stats.gpuValidationPassed && stats.debugLayerEnabled;
  return false;
}
bool Receipt(const RendererStats &stats, unsigned width, unsigned height) {
  const auto &value = stats.bloomSemanticReceipt;
  return value.requestedDepthAware && value.available && !value.failOpen &&
         value.requested == BloomSemanticSource3D::DlssDepthReplay &&
         value.actual == BloomSemanticSource3D::DlssDepthReplay &&
         value.semanticRenderWidth && value.semanticRenderHeight &&
         value.semanticRenderWidth <= width &&
         value.semanticRenderHeight <= height &&
         value.semanticOutputWidth == width &&
         value.semanticOutputHeight == height &&
         value.semanticResourceGeneration && value.semanticResizeGeneration &&
         value.dlssReplayCurrentFrame && value.dlssReplaySampleable &&
         value.dlssReplayTransitioned;
}
} // namespace
int main() {
  CoInitializeEx(nullptr, COINIT_MULTITHREADED);
  constexpr unsigned Width = 640, Height = 360;
  const wchar_t *name = L"AevryndBloomDlssDepthSemantic";
  WNDCLASSW wc{};
  wc.lpfnWndProc = DefWindowProcW;
  wc.hInstance = GetModuleHandleW(nullptr);
  wc.lpszClassName = name;
  RegisterClassW(&wc);
  HWND window = CreateWindowW(name, name, WS_OVERLAPPED, 0, 0, Width, Height,
                              nullptr, nullptr, wc.hInstance, nullptr);
  if (!window)
    return 1;
  int result{};
  bool dlssAvailable{};
  {
    D3D12Renderer probe;
    probe.Initialize(window, Width, Height, Width, Height, true);
    if (probe.SupportsDLSS()) {
      probe.SetQualitySettings({false, false, false, false});
      probe.ToggleDLSS();
      dlssAvailable = probe.DLSSOn();
    }
  }
  if (!dlssAvailable) {
      std::cout << "NO-GO: DLSS runtime or adapter unavailable\n";
      result = 77;
  } else {
        const std::filesystem::path output =
            "test-artifacts/bloom-dlss-depth-semantic";
        std::filesystem::create_directories(output);
        auto captureWith = [&](D3D12Renderer &active, DrawList &draw, int mode,
                               const std::string &label) {
          draw.SetBloom(
              {mode != 0, .08f, .35f, .22f, 8.f, mode == 2, .015f, .015f});
          Capture value{};
          const auto path =
              output / (label + "-" + std::to_string(mode) + ".png");
          if (!active.CaptureNextFrame(path))
            return value;
          active.Render(draw, {.02f, .03f, .05f, 1});
          active.WaitIdle();
          value.valid = Decode(path, value.image);
          value.stats = active.GetStats();
          return value;
        };
        auto captureIndependent = [&](DrawList &draw, int mode,
                                      const std::string &label) {
          const HWND isolatedWindow =
              CreateWindowW(name, name, WS_OVERLAPPED, 0, 0, Width, Height,
                            nullptr, nullptr, wc.hInstance, nullptr);
          if (!isolatedWindow)
            return Capture{};
          Capture value{};
          {
            D3D12Renderer isolated;
            isolated.Initialize(isolatedWindow, Width, Height, Width, Height,
                                true);
            isolated.SetQualitySettings({false, false, false, false});
            if (isolated.SupportsDLSS()) {
              isolated.ToggleDLSS();
              if (isolated.DLSSOn())
                // 每個比較模式具有相同 renderer、viewport 與 DLSS frame-zero 起點。
                value = captureWith(isolated, draw, mode, label);
            }
          }
          DestroyWindow(isolatedWindow);
          return value;
        };
        DrawList boundary;
        boundary.SetViewProjection(Matrix4::Identity());
        boundary.SetBackfaceCulling(false);
        Rectangle(boundary, -1, 0, .2f, {1, 1, 1, 1});
        Rectangle(boundary, 0, 1, .8f, {.002f, .002f, .002f, 1});
        DrawList same;
        same.SetViewProjection(Matrix4::Identity());
        same.SetBackfaceCulling(false);
        Rectangle(same, -1, 1, .4f, {1, 1, 1, 1});
        auto disableTimeOfDay = [](DrawList &draw) {
          auto settings = draw.TimeOfDayExposure();
          settings.enabled = false;
          draw.SetTimeOfDayExposure(settings);
        };
        // 此 smoke 只量 bloom；深度感知模式會載入 scene semantic，必須關閉共享的時段曝光。
        disableTimeOfDay(boundary);
        disableTimeOfDay(same);
        const std::array boundarySet{
            captureIndependent(boundary, 0, "different-depth"),
            captureIndependent(boundary, 1, "different-depth"),
            captureIndependent(boundary, 2, "different-depth")};
        const std::array sameSet{captureIndependent(same, 0, "same-depth"),
                                 captureIndependent(same, 1, "same-depth"),
                                 captureIndependent(same, 2, "same-depth")};
        const unsigned edge = Width / 2;
        const double legacyLeak =
                         PositiveDelta(boundarySet[1].image,
                                       boundarySet[0].image, edge, edge + 12),
                     awareLeak =
                         PositiveDelta(boundarySet[2].image,
                                       boundarySet[0].image, edge, edge + 12);
        const double legacyGain =
                         RegionMean(sameSet[1].image, edge - 80, edge + 80) -
                         RegionMean(sameSet[0].image, edge - 80, edge + 80),
                     awareGain =
                         RegionMean(sameSet[2].image, edge - 80, edge + 80) -
                         RegionMean(sameSet[0].image, edge - 80, edge + 80);
        const double suppression =
                         legacyLeak > 0 ? awareLeak / legacyLeak : INFINITY,
                     retention =
                         legacyGain > 0 ? awareGain / legacyGain : -INFINITY;
        Capture beforeResizeCapture{}, resized{};
        {
          D3D12Renderer resizeRenderer;
          resizeRenderer.Initialize(window, Width, Height, Width, Height, true);
          resizeRenderer.SetQualitySettings({false, false, false, false});
          if (resizeRenderer.SupportsDLSS()) {
            resizeRenderer.ToggleDLSS();
            if (resizeRenderer.DLSSOn()) {
              beforeResizeCapture = captureWith(
                  resizeRenderer, boundary, 2, "before-resize");
              resizeRenderer.Resize(480, 270);
              resized = captureWith(resizeRenderer, boundary, 2, "resized");
            }
          }
        }
        const uint64_t beforeResize = beforeResizeCapture.stats
                                          .bloomSemanticReceipt
                                          .semanticResizeGeneration;
        char buffer[16]{};
        const DWORD count =
            GetEnvironmentVariableA("AEVRYND_EXPECT_GPU_VALIDATION_MODE",
                                    buffer, DWORD(std::size(buffer)));
        const std::string mode =
            count && count < std::size(buffer) ? buffer : "";
        const bool captures =
            std::ranges::all_of(
                boundarySet,
                [](const Capture &value) { return value.valid; }) &&
            std::ranges::all_of(
                sameSet, [](const Capture &value) { return value.valid; }) &&
            beforeResizeCapture.valid && resized.valid;
        const bool receipts =
            Receipt(boundarySet[2].stats, Width, Height) &&
            Receipt(sameSet[2].stats, Width, Height) &&
            Receipt(resized.stats, 480, 270) &&
            resized.stats.bloomSemanticReceipt.semanticResizeGeneration >
                beforeResize;
        const bool validation = Validation(resized.stats, mode);
        std::cout << "suppression=" << suppression << " retention=" << retention
                  << " receipts=" << receipts << " resize=" << beforeResize
                  << "->"
                  << resized.stats.bloomSemanticReceipt.semanticResizeGeneration
                  << " validation=" << validation
                  << " debug=" << resized.stats.debugErrorCount << '/'
                  << resized.stats.debugWarningCount << '\n';
        for (size_t index = 0; index < resized.stats.debugErrorIdCount; ++index)
          std::cout << "debug-error[" << resized.stats.debugErrorIds[index]
                    << "]=" << resized.stats.debugErrorDescriptions[index] << '\n';
        for (size_t index = 0; index < resized.stats.debugWarningIdCount; ++index)
          std::cout << "debug-warning[" << resized.stats.debugWarningIds[index]
                    << "]=" << resized.stats.debugWarningDescriptions[index]
                    << " occurrences="
                    << resized.stats.debugWarningOccurrences[index] << '\n';
        if (!captures || !receipts || !std::isfinite(suppression) ||
            suppression > .2 || !std::isfinite(retention) || retention < .9 ||
            !validation)
          result = 2;
  }
  DestroyWindow(window);
  CoUninitialize();
  return result;
}
