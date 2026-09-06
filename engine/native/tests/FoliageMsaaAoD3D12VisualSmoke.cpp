#include <Aevrynd/CookedRenderManifest3D.h>
#include <Aevrynd/Renderer.h>

#include <Windows.h>
#include <wincodec.h>
#include <wrl/client.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

using namespace Aevrynd;
using Microsoft::WRL::ComPtr;

namespace {
constexpr unsigned Width = 256;
constexpr unsigned Height = 256;
constexpr float FoliageExtent = .72f;

struct Image {
    UINT width{}, height{};
    std::vector<std::uint8_t> rgba;
};

struct PixelOracle {
    std::size_t green{}, holes{}, samples{};
};

struct CaptureResult {
    Image image{};
    PixelOracle pixels{};
    RendererStats stats{};
    bool valid{};
};

bool Decode(const std::filesystem::path &path, Image &image) {
    ComPtr<IWICImagingFactory> factory;
    ComPtr<IWICBitmapDecoder> decoder;
    ComPtr<IWICBitmapFrameDecode> frame;
    ComPtr<IWICFormatConverter> converter;
    if (FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr,
                                CLSCTX_INPROC_SERVER,
                                IID_PPV_ARGS(&factory))) ||
        FAILED(factory->CreateDecoderFromFilename(
            path.c_str(), nullptr, GENERIC_READ, WICDecodeMetadataCacheOnLoad,
            &decoder)) ||
        FAILED(decoder->GetFrame(0, &frame)) ||
        FAILED(frame->GetSize(&image.width, &image.height)) ||
        FAILED(factory->CreateFormatConverter(&converter)) ||
        FAILED(converter->Initialize(
            frame.Get(), GUID_WICPixelFormat32bppRGBA,
            WICBitmapDitherTypeNone, nullptr, 0,
            WICBitmapPaletteTypeCustom)))
        return false;
    image.rgba.resize(std::size_t(image.width) * image.height * 4);
    return SUCCEEDED(converter->CopyPixels(
        nullptr, image.width * 4, UINT(image.rgba.size()), image.rgba.data()));
}

PixelOracle Measure(const Image &image) {
    PixelOracle result{};
    // 避開幾何邊緣，只比較葉片圖集本身的實色與透明孔洞。
    constexpr float OracleExtent = FoliageExtent - .05f;
    for (unsigned y = 0; y < image.height; ++y) {
        for (unsigned x = 0; x < image.width; ++x) {
            const float ndcX = 2.f * (float(x) + .5f) / float(image.width) - 1.f;
            const float ndcY = 1.f - 2.f * (float(y) + .5f) / float(image.height);
            if (std::abs(ndcX) > OracleExtent ||
                std::abs(ndcY) > OracleExtent)
                continue;
            const std::size_t offset =
                (std::size_t(y) * image.width + x) * 4;
            const int red = image.rgba[offset];
            const int green = image.rgba[offset + 1];
            const int blue = image.rgba[offset + 2];
            ++result.samples;
            if (green >= 24 && green >= red + 8 && green >= blue + 8)
                ++result.green;
            if (std::min({red, green, blue}) >= 96 &&
                std::max({red, green, blue}) -
                        std::min({red, green, blue}) <=
                    28)
                ++result.holes;
        }
    }
    return result;
}

MeshHandle3D CreateQuad(D3D12Renderer &renderer, float extent, float depth,
                        bool textured) {
    const std::uint32_t normal = PackNormalSnorm8({0.f, 0.f, -1.f}, 0.f);
    const float maximumUv = textured ? 1.f : 0.f;
    const std::array<Vertex3D, 4> vertices{{
        {-extent, -extent, depth, normal, 0xffffffffu, 0.f, maximumUv},
        {-extent, extent, depth, normal, 0xffffffffu, 0.f, 0.f},
        {extent, extent, depth, normal, 0xffffffffu, maximumUv, 0.f},
        {extent, -extent, depth, normal, 0xffffffffu, maximumUv, maximumUv},
    }};
    constexpr std::array<std::uint32_t, 6> indices{{0, 1, 2, 0, 2, 3}};
    return renderer.CreateMesh(vertices, indices);
}

bool ValidationGate(const RendererStats &stats, std::string &mode) {
    char expected[16]{};
    const DWORD length = GetEnvironmentVariableA(
        "AEVRYND_EXPECT_GPU_VALIDATION_MODE", expected, DWORD(std::size(expected)));
    mode = length && length < std::size(expected) ? expected : "";
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

DrawList BuildScene(MeshHandle3D background, MeshHandle3D foliage,
                    MaterialTextureSetHandle3D foliageTextures,
                    const Material3D &foliageMaterial, bool cull,
                    bool &accepted) {
    DrawList draw;
    draw.SetViewProjection(Matrix4::Identity());
    draw.SetBackfaceCulling(cull);
    draw.SetAtmosphere({});
    draw.SetDirectionalLight({{0.f, 0.f, -1.f}, 1.f, {1.f, 1.f, 1.f, 1.f},
                              0.f});
    draw.SetBloom({false});
    const bool backgroundAccepted = draw.MeshInstance(
        background, Matrix4::Identity(), {1.f, 1.f, 1.f, 1.f},
        {1.f, 0.f, 1.f, 1.f}, MeshRenderLayer3D::Opaque, {}, 1.f, false,
        MaterialClass3D::Surface, (std::numeric_limits<float>::quiet_NaN)(),
        false);
    const bool foliageAccepted = draw.MeshInstance(
        foliage, Matrix4::Identity(), foliageMaterial.tint,
        foliageMaterial.surface, MeshRenderLayer3D::Opaque, foliageTextures,
        foliageMaterial.textureScale, foliageMaterial.useUv,
        foliageMaterial.materialClass,
        (std::numeric_limits<float>::quiet_NaN)(), false, 1.f, 0, false,
        DirectionalShadowAllCascades3D, ShadowCascadeSelection3D::Automatic,
        Matrix4::Identity(), false, foliageMaterial.normalScale,
        foliageMaterial.alphaMode, foliageMaterial.alphaCutoff, false);
    accepted = backgroundAccepted && foliageAccepted;
    return draw;
}

CaptureResult Capture(D3D12Renderer &renderer, DrawList &draw,
                      RendererQualitySettings3D quality,
                      const std::filesystem::path &path) {
    CaptureResult result{};
    // 每個控制組先釋放歷史，確保 cull 與採樣模式不共享前一組 TAA 狀態。
    renderer.SetQualitySettings({false, false, false, false});
    renderer.SetQualitySettings(quality);
    for (unsigned frame = 0; frame < 5; ++frame)
        renderer.Render(draw, {.72f, .72f, .72f, 1.f});
    if (!renderer.CaptureNextFrame(path))
        return result;
    renderer.Render(draw, {.72f, .72f, .72f, 1.f});
    renderer.WaitIdle();
    result.stats = renderer.GetStats();
    result.valid = Decode(path, result.image) && result.image.width == Width &&
                   result.image.height == Height;
    if (result.valid)
        result.pixels = Measure(result.image);
    return result;
}
} // 命名空間

int main() {
    const HRESULT comResult = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (FAILED(comResult) && comResult != RPC_E_CHANGED_MODE)
        return 1;
    HWND window = CreateWindowExW(0, L"STATIC", L"AevryndFoliageMsaaAo",
                                  WS_OVERLAPPED, 0, 0, Width, Height, nullptr,
                                  nullptr, GetModuleHandleW(nullptr), nullptr);
    if (!window) {
        if (SUCCEEDED(comResult))
            CoUninitialize();
        return 2;
    }

    int result{};
    std::filesystem::path output;
    try {
        const std::filesystem::path manifestPath =
            std::filesystem::path{AEVRYND_SOURCE_DIR} /
            "engine/assets/realistic-cc0-first-batch/cooked/runtime-render-manifest.json";
        CookedRenderManifest3D manifest;
        CookedRenderManifestError3D manifestError{};
        if (!manifest.LoadFile(manifestPath, &manifestError)) {
            std::cerr << "manifest=" << manifestError.message << '\n';
            result = 3;
        }

        const CookedRenderMaterial3D *leaves =
            result ? nullptr : manifest.FindMaterial("island_tree_01:leaves");
        MaterialAssetDefinition3D material =
            leaves ? leaves->Definition(1024) : MaterialAssetDefinition3D{};
        if (!leaves || material.textures.alphaMode != MaterialAlphaMode3D::Mask ||
            material.material.alphaMode != MaterialAlphaMode3D::Mask) {
            std::cerr << "foliage MASK material unavailable\n";
            result = 4;
        }

        if (!result) {
            material.textures.baseColorPath =
                manifest.AssetRoot() / material.textures.baseColorPath;
            material.textures.roughnessPath =
                manifest.AssetRoot() / material.textures.roughnessPath;
            material.textures.normalPath =
                manifest.AssetRoot() / material.textures.normalPath;
            material.material.tint = {.12f, 1.f, .12f, 1.f};
            material.material.surface = {1.f, 0.f, 1.f, 1.f};
            material.material.castsShadow = false;
            material.material.doubleSided = false;
        }

        if (!result) {
            D3D12Renderer renderer;
            renderer.Initialize(window, Width, Height, Width, Height, false);
            const MeshHandle3D background = CreateQuad(renderer, .9f, .78f, false);
            const MeshHandle3D foliage =
                CreateQuad(renderer, FoliageExtent, .28f, true);
            const MaterialTextureSetHandle3D textures =
                renderer.CreateMaterialTextureSet(material.textures);
            if (!background || !foliage || !textures) {
                std::cerr << "fixture resources unavailable\n";
                result = 5;
            }

            output = std::filesystem::temp_directory_path() /
                     ("aevrynd-foliage-msaa-ao-" +
                      std::to_string(GetCurrentProcessId()));
            std::filesystem::create_directories(output);
            const RendererQualitySettings3D referenceQuality{false, true, false,
                                                               true};
            const RendererQualitySettings3D productionQuality{false, true, true,
                                                                true};
            std::array<CaptureResult, 2> references{};
            std::array<CaptureResult, 2> productions{};
            for (unsigned cull = 0; !result && cull < 2; ++cull) {
                bool sceneAccepted{};
                DrawList draw = BuildScene(background, foliage, textures,
                                           material.material, cull != 0,
                                           sceneAccepted);
                if (!sceneAccepted) {
                    std::cerr << "scene submission failed cull=" << cull
                              << '\n';
                    result = 6;
                    break;
                }
                references[cull] = Capture(
                    renderer, draw, referenceQuality,
                    output / (cull ? "reference-cull-on.png"
                                   : "reference-cull-off.png"));
                productions[cull] = Capture(
                    renderer, draw, productionQuality,
                    output / (cull ? "production-cull-on.png"
                                   : "production-cull-off.png"));
            }

            std::string validationMode;
            const bool validation =
                !result && ValidationGate(productions.back().stats,
                                           validationMode);
            for (unsigned cull = 0; !result && cull < 2; ++cull) {
                const auto &reference = references[cull];
                const auto &production = productions[cull];
                const auto &stats = production.stats;
                const double retention = reference.pixels.green
                                             ? double(production.pixels.green) /
                                                   double(reference.pixels.green)
                                             : 0.0;
                const bool referenceGate =
                    reference.valid && reference.pixels.green >= 256 &&
                    reference.pixels.holes >= 256 &&
                    reference.stats.effectiveQuality.ambientOcclusion &&
                    reference.stats.ambientOcclusionEnabled &&
                    !reference.stats.effectiveQuality.multisampling &&
                    !reference.stats.multisampleEnabled &&
                    reference.stats.effectiveQuality.postProcessAntiAliasing &&
                    reference.stats.temporalAntiAliasingEnabled &&
                    reference.stats.temporalHistoryUsed &&
                    reference.stats.ambientOcclusionDepthPrepasses > 0 &&
                    reference.stats.lastSubmittedMaskedMeshInstances == 1;
                const bool productionGate =
                    production.valid && production.pixels.green >= 256 &&
                    production.pixels.holes >= 256 && retention >= .5;
                const bool routeGate =
                    stats.effectiveQuality.ambientOcclusion &&
                    stats.ambientOcclusionEnabled &&
                    stats.effectiveQuality.multisampling &&
                    stats.multisampleEnabled && stats.multisampleCount == 4 &&
                    stats.effectiveQuality.postProcessAntiAliasing &&
                    stats.temporalAntiAliasingEnabled &&
                    stats.temporalHistoryUsed &&
                    stats.ambientOcclusionDepthPrepasses > 0 &&
                    stats.lastSubmittedMaskedMeshInstances == 1;
                std::cout << "cull=" << cull << " reference="
                          << reference.pixels.green << '/'
                          << reference.pixels.holes << " production="
                          << production.pixels.green << '/'
                          << production.pixels.holes << " retention="
                          << retention << " route=" << routeGate << '\n';
                if (!referenceGate || !productionGate || !routeGate)
                    result = 7;
            }
            if (!result) {
                const auto minimumGreen =
                    std::min(productions[0].pixels.green,
                             productions[1].pixels.green);
                const auto maximumGreen =
                    std::max(productions[0].pixels.green,
                             productions[1].pixels.green);
                const bool cullParity =
                    maximumGreen &&
                    double(minimumGreen) / double(maximumGreen) >= .8;
                const auto &stats = productions.back().stats;
                std::cout << "validation=" << validationMode << '/'
                          << validation << " debug=" << stats.debugErrorCount
                          << '/' << stats.debugWarningCount
                          << " cullParity=" << cullParity << '\n';
                if (!cullParity || !validation)
                    result = 8;
            }

            renderer.DestroyMaterialTextureSet(textures);
            renderer.DestroyMesh(foliage);
            renderer.DestroyMesh(background);
            renderer.WaitIdle();
        }
    } catch (const std::exception &error) {
        std::cerr << "exception=" << error.what() << '\n';
        result = 9;
    }

    if (!output.empty())
        std::cout << "evidence=" << output.string() << '\n';
    DestroyWindow(window);
    if (SUCCEEDED(comResult))
        CoUninitialize();
    return result;
}
