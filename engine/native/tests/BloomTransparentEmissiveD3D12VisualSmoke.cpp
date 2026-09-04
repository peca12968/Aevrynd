#include <Aevrynd/Renderer.h>
#include <wincodec.h>
#include <wrl/client.h>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <iostream>

using namespace Aevrynd;
using Microsoft::WRL::ComPtr;

namespace {
constexpr unsigned Width = 640;
constexpr unsigned Height = 360;

struct Image {
    UINT width{};
    UINT height{};
    std::vector<uint8_t> rgba;
};

bool Decode(const std::filesystem::path& path, Image& image) {
    ComPtr<IWICImagingFactory> factory;
    ComPtr<IWICBitmapDecoder> decoder;
    ComPtr<IWICBitmapFrameDecode> frame;
    ComPtr<IWICFormatConverter> converter;
    if (FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory))) ||
        FAILED(factory->CreateDecoderFromFilename(path.c_str(), nullptr, GENERIC_READ, WICDecodeMetadataCacheOnLoad, &decoder)) ||
        FAILED(decoder->GetFrame(0, &frame)) || FAILED(frame->GetSize(&image.width, &image.height)) ||
        FAILED(factory->CreateFormatConverter(&converter)) ||
        FAILED(converter->Initialize(frame.Get(), GUID_WICPixelFormat32bppRGBA, WICBitmapDitherTypeNone, nullptr, 0,
                                     WICBitmapPaletteTypeCustom))) {
        return false;
    }
    image.rgba.resize(size_t(image.width) * image.height * 4);
    return SUCCEEDED(converter->CopyPixels(nullptr, image.width * 4, UINT(image.rgba.size()), image.rgba.data()));
}

double Luminance(const Image& image, unsigned x, unsigned y) {
    const size_t offset = (size_t(y) * image.width + x) * 4;
    return .2126 * image.rgba[offset] + .7152 * image.rgba[offset + 1] + .0722 * image.rgba[offset + 2];
}

double HaloEnergy(const Image& bloom, const Image& control, unsigned left, unsigned right) {
    double energy{};
    for (unsigned y = 72; y < Height - 72; ++y) {
        for (unsigned x = left; x < right; ++x) {
            energy += (std::max)(0.0, Luminance(bloom, x, y) - Luminance(control, x, y));
        }
    }
    return energy;
}

bool DepthNear(const D3D12RendererTestAccess3D::SemanticDepthCapture3D& depth, unsigned x, unsigned y,
               float expected, float tolerance) {
    return depth.Complete() && x < depth.width && y < depth.height &&
           std::abs(depth.pixels[size_t(y) * depth.width + x] - expected) <= tolerance;
}

bool ValidationGate(const RendererStats& stats, const std::string& mode) {
    const bool clean = !stats.debugErrorCount && !stats.debugWarningCount;
    if (mode == "normal") {
        return clean && !stats.gpuValidationRequested && !stats.gpuValidationAvailable &&
               !stats.gpuValidationExecuted && !stats.gpuValidationPassed && !stats.debugLayerEnabled;
    }
    if (mode == "gbv") {
        return clean && stats.gpuValidationRequested && stats.gpuValidationAvailable &&
               stats.gpuValidationExecuted && stats.gpuValidationPassed && stats.debugLayerEnabled;
    }
    return false;
}
}

int main() {
    CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    const wchar_t* name = L"AevryndBloomTransparentEmissive";
    WNDCLASSW wc{};
    wc.lpfnWndProc = DefWindowProcW;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpszClassName = name;
    RegisterClassW(&wc);
    HWND window = CreateWindowW(name, name, WS_OVERLAPPED, 0, 0, Width, Height, nullptr, nullptr, wc.hInstance, nullptr);
    if (!window) {
        return 1;
    }

    int result{};
    {
        D3D12Renderer renderer;
        renderer.Initialize(window, Width, Height, Width, Height, false);
        renderer.SetQualitySettings({false, false, false, false});
        const uint32_t normal = PackNormalSnorm8({0, 0, -1});
        const std::array<uint32_t, 6> indices{0, 1, 2, 0, 2, 3};
        const std::array<Vertex3D, 4> farVertices{{
            {-.75f, -.7f, .55f, normal, ~0u, 0, 1}, {-.75f, .7f, .55f, normal, ~0u, 0, 0},
            {.75f, .7f, .55f, normal, ~0u, 1, 0}, {.75f, -.7f, .55f, normal, ~0u, 1, 1}}};
        const std::array<Vertex3D, 4> nearVertices{{
            {-.25f, -.45f, .25f, normal, ~0u, 0, 1}, {-.25f, .45f, .25f, normal, ~0u, 0, 0},
            {.5f, .45f, .25f, normal, ~0u, 1, 0}, {.5f, -.45f, .25f, normal, ~0u, 1, 1}}};
        const auto farMesh = renderer.CreateMesh(farVertices, indices);
        const auto nearMesh = renderer.CreateMesh(nearVertices, indices);
        if (!farMesh || !nearMesh) {
            result = 2;
        }

        DrawList draw;
        draw.SetViewProjection(Matrix4::Identity());
        draw.SetBackfaceCulling(false);
        // 測試使用低亮夜空，避免天空本身飽和而遮蔽透明放光所產生的 Bloom 差異。
        const bool profileAccepted = draw.SetPhaseRadianceProfile({
            {.002f, .003f, .005f}, .2f,
            {.003f, .004f, .006f}, .5f,
            {.002f, .002f, .002f}, .00935f,
            {.01f, .01f, .01f}, 0.f,
            {1, 1, 1}, 0.f,
            0x42544c4f4f4dull});
        draw.WorldTriangle({-1, -1, .8f}, {-1, 1, .8f}, {0, 1, .8f}, {.006f, .006f, .006f, 1});
        draw.WorldTriangle({-1, -1, .8f}, {0, 1, .8f}, {0, -1, .8f}, {.006f, .006f, .006f, 1});
        DrawList control = draw;

        Atmosphere3D atmosphere{};
        atmosphere.enabled = true;
        atmosphere.cameraPosition = {0, 0, -2};
        atmosphere.aerialPerspective = {true, .008f, .003f, 0, 0, 0, .001f, 80.f, .05f,
                                         {.01f, .015f, .025f, 1}, .01f};
        const bool farAccepted = draw.MeshInstance(farMesh, Matrix4::Identity(), {1, .06f, .015f, .55f},
                                                   {.2f, 0, 1, 1}, MeshRenderLayer3D::Transparent, {}, .25f,
                                                   false, MaterialClass3D::Surface, .55f, false);
        const bool nearAccepted = draw.MeshInstance(nearMesh, Matrix4::Identity(), {.015f, .08f, 1, .68f},
                                                    {.2f, 0, 1, 1}, MeshRenderLayer3D::Transparent, {}, .25f,
                                                    false, MaterialClass3D::Surface, .25f, false);

        const std::filesystem::path output = "test-artifacts/bloom-transparent-emissive";
        std::filesystem::create_directories(output);
        std::array<Image, 3> images{};
        std::array<Image, 3> controls{};
        RendererStats awareStats{};
        RendererStats atmosphereStats{};
        std::optional<D3D12RendererTestAccess3D::SemanticDepthCapture3D> semanticDepth;
        for (int mode = 0; mode < 3; ++mode) {
            const BloomSettings3D bloom{mode != 0, .08f, .35f, .28f, 8.f, mode == 2, .015f, .015f};
            draw.SetBloom(bloom);
            control.SetBloom(bloom);
            const auto path = output / ("transparent-" + std::to_string(mode) + ".png");
            if (!renderer.CaptureNextFrame(path)) {
                result = 3;
                continue;
            }
            renderer.Render(draw, {.01f, .015f, .025f, 1});
            renderer.WaitIdle();
            if (!Decode(path, images[size_t(mode)]) || images[size_t(mode)].width != Width ||
                images[size_t(mode)].height != Height) {
                result = 3;
            }
            if (mode == 2) {
                awareStats = renderer.GetStats();
                semanticDepth = D3D12RendererTestAccess3D::CaptureSemanticDepth(renderer);
            }
            const auto controlPath = output / ("control-" + std::to_string(mode) + ".png");
            if (!renderer.CaptureNextFrame(controlPath)) {
                result = 3;
                continue;
            }
            renderer.Render(control, {.01f, .015f, .025f, 1});
            renderer.WaitIdle();
            if (!Decode(controlPath, controls[size_t(mode)]) || controls[size_t(mode)].width != Width ||
                controls[size_t(mode)].height != Height) {
                result = 3;
            }
        }

        draw.SetAtmosphere(atmosphere);
        draw.SetBloom({true, .08f, .35f, .28f, 8.f, true, .015f, .015f});
        renderer.Render(draw, {.01f, .015f, .025f, 1});
        renderer.WaitIdle();
        atmosphereStats = renderer.GetStats();

        const double skyLegacy = HaloEnergy(images[1], controls[1], 560, 574);
        const double skyAware = HaloEnergy(images[2], controls[2], 560, 574);
        const double opaqueLegacy = HaloEnergy(images[1], controls[1], 66, 80);
        const double opaqueAware = HaloEnergy(images[2], controls[2], 66, 80);
        const auto channel = [&](const Image& image, unsigned x, unsigned y, unsigned component) {
            return image.rgba[(size_t(y) * image.width + x) * 4 + component];
        };
        const bool sortedColor = channel(images[2], 500, 180, 0) > channel(images[2], 500, 180, 2) &&
                                 channel(images[2], 320, 180, 2) > channel(images[2], 500, 180, 2);
        const bool depthOracle = semanticDepth && DepthNear(*semanticDepth, 40, 180, .8f, .001f) &&
                                 DepthNear(*semanticDepth, 120, 180, .55f, .001f) &&
                                 DepthNear(*semanticDepth, 320, 180, .25f, .001f) &&
                                 DepthNear(*semanticDepth, 600, 180, 1.f, 0.f);
        const auto& receipt = awareStats.bloomSemanticReceipt;
        const bool receiptGate = receipt.actual == BloomSemanticSource3D::HdrAlphaCarrier && receipt.available &&
                                 !receipt.failOpen && receipt.transparentOrderingPreserved &&
                                 receipt.transparentLayerCount == 2 && receipt.transparentOrderHash != 0 &&
                                 receipt.transparentCarrierPipeline;
        const bool atmosphereReceipt = atmosphereStats.bloomSemanticReceipt.atmosphereCarrierPipeline &&
                                       atmosphereStats.volumetricAtmosphereCompositePasses > 0;
        char modeBuffer[16]{};
        const DWORD modeLength = GetEnvironmentVariableA("AEVRYND_EXPECT_GPU_VALIDATION_MODE", modeBuffer,
                                                         DWORD(std::size(modeBuffer)));
        const std::string validationMode = modeLength && modeLength < std::size(modeBuffer) ? modeBuffer : "";
        const bool validation = ValidationGate(atmosphereStats, validationMode);
        const bool haloGate = skyLegacy > 0 && opaqueLegacy > 0 && skyAware / skyLegacy <= .2 &&
                              opaqueAware / opaqueLegacy <= .2;
        std::cout << "transparent halo=" << skyAware << '/' << skyLegacy << ',' << opaqueAware << '/'
                  << opaqueLegacy << " profile=" << profileAccepted << " accepted=" << farAccepted << '/'
                  << nearAccepted << " depth=" << depthOracle << " color=" << sortedColor << " receipt="
                  << receiptGate << '/' << atmosphereReceipt << " layers=" << receipt.transparentLayerCount
                  << " order=" << receipt.transparentOrderHash << " pipelines="
                  << receipt.transparentCarrierPipeline << '/'
                  << atmosphereStats.bloomSemanticReceipt.atmosphereCarrierPipeline << " validation="
                  << validation << " gbv=" << atmosphereStats.gpuValidationRequested << '/'
                  << atmosphereStats.gpuValidationAvailable << '/' << atmosphereStats.gpuValidationExecuted << '/'
                  << atmosphereStats.gpuValidationPassed << '/' << atmosphereStats.debugLayerEnabled << " debug="
                  << atmosphereStats.debugErrorCount << '/' << atmosphereStats.debugWarningCount << '\n';
        for (size_t index = 0; index < atmosphereStats.debugErrorIdCount; ++index) {
            std::cout << "error id=" << atmosphereStats.debugErrorIds[index] << " description="
                      << atmosphereStats.debugErrorDescriptions[index] << '\n';
        }
        for (size_t index = 0; index < atmosphereStats.debugWarningIdCount; ++index) {
            std::cout << "warning id=" << atmosphereStats.debugWarningIds[index] << " occurrences="
                      << atmosphereStats.debugWarningOccurrences[index] << " description="
                      << atmosphereStats.debugWarningDescriptions[index] << '\n';
        }
        if (!profileAccepted || !farAccepted || !nearAccepted || !haloGate || !depthOracle || !sortedColor ||
            !receiptGate || !atmosphereReceipt || !validation) {
            result = 4;
        }
        renderer.DestroyMesh(nearMesh);
        renderer.DestroyMesh(farMesh);
        renderer.WaitIdle();
    }
    DestroyWindow(window);
    CoUninitialize();
    return result;
}
