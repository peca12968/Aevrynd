#include <Aevrynd/Camera.h>
#include <Aevrynd/Renderer.h>

#include <Windows.h>
#include <wincodec.h>
#include <wrl/client.h>

#include <array>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <limits>
#include <optional>
#include <string_view>
#include <vector>

using namespace Aevrynd;
using Microsoft::WRL::ComPtr;

namespace {
constexpr unsigned Width = 256;
constexpr unsigned Height = 256;

struct Sample {
    Vec3 mean{};
    std::size_t pixels{};
    RendererStats stats{};
};

struct Image {
    unsigned width{}, height{};
    std::vector<std::uint8_t> rgba{};
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
    image.rgba.resize(std::size_t(image.width) * image.height * 4);
    return SUCCEEDED(converter->CopyPixels(nullptr, image.width * 4,
                                           UINT(image.rgba.size()),
                                           image.rgba.data()));
}

MeshHandle3D CreateCard(D3D12Renderer &renderer, bool reverse) {
    const auto normal = PackNormalSnorm8(reverse ? Vec3{0, 0, 1} : Vec3{0, 0, -1});
    const auto color = PackColorRgba8({1, 1, 1, 1});
    const std::array<Vertex3D, 4> vertices{{
        {-.8f, -.8f, 0, normal, color, 0, 1},
        {-.8f, .8f, 0, normal, color, 0, 0},
        {.8f, .8f, 0, normal, color, 1, 0},
        {.8f, -.8f, 0, normal, color, 1, 1},
    }};
    const std::array<std::uint32_t, 6> front{{0, 1, 2, 0, 2, 3}};
    const std::array<std::uint32_t, 6> back{{0, 2, 1, 0, 3, 2}};
    return renderer.CreateMesh(vertices, reverse ? back : front);
}

std::optional<Sample> Capture(D3D12Renderer &renderer, MeshHandle3D mesh,
                              MaterialClass3D materialClass, float occlusion,
                              Vec3 authoredLightDirection, float ambient,
                              float intensity, std::uint64_t serial) {
    const Camera3D camera{{0, 0, -4}, {0, 0, 0}, {0, 1, 0}, {}};
    DrawList draw;
    draw.SetViewProjection(CreateViewProjectionMatrix(camera, float(Width) / Height));
    draw.SetBackfaceCulling(false);
    draw.SetTimeOfDayExposure({false});
    Atmosphere3D atmosphere{};
    atmosphere.enabled = false;
    atmosphere.cameraPosition = camera.position;
    draw.SetAtmosphere(atmosphere);
    draw.SetDirectionalLight(
        {authoredLightDirection, ambient, {1, 1, 1, 1}, intensity});
    draw.SetBloom({false});
    const RenderObjectIdentity3D identity{{0x464f4c4941474550ull, serial}, 1};
    if (!draw.MeshInstance(
            mesh, Matrix4::Identity(), {.18f, .72f, .24f, 1},
            {.82f, 0, 0, occlusion}, MeshRenderLayer3D::Opaque, {}, 1.f,
            false, materialClass, (std::numeric_limits<float>::quiet_NaN)(),
            false, 1.f, 0, false, DirectionalShadowAllCascades3D,
            ShadowCascadeSelection3D::Automatic, Matrix4::Identity(), false,
            1.f, MaterialAlphaMode3D::Opaque, .5f, false, identity))
        return {};

    const auto output = std::filesystem::temp_directory_path() /
                        ("aevrynd-foliage-physical-" +
                         std::to_string(GetCurrentProcessId()) + "-" +
                         std::to_string(serial) + ".png");
    if (!renderer.RequestObjectIdReadback3D({serial, serial ^ 0x51u,
                                             serial ^ 0xa2u}) ||
        !renderer.CaptureNextFrame(output))
        return {};
    renderer.Render(draw, {0, 0, 0, 1});
    const auto objectId = renderer.TakeObjectIdReadback3D();
    renderer.WaitIdle();
    Image image{};
    const bool decoded = Decode(output, image);
    std::error_code removeError;
    std::filesystem::remove(output, removeError);
    if (!objectId || !objectId->Complete() || !decoded || image.width != Width ||
        image.height != Height || objectId->pixels.size() * 4 != image.rgba.size())
        return {};

    Sample result{};
    for (std::size_t index = 0; index < objectId->pixels.size(); ++index) {
        const auto *resolved = objectId->Resolve(objectId->pixels[index]);
        if (!resolved || *resolved != identity)
            continue;
        const auto offset = index * 4;
        result.mean = result.mean +
                      Vec3{image.rgba[offset] / 255.f,
                           image.rgba[offset + 1] / 255.f,
                           image.rgba[offset + 2] / 255.f};
        ++result.pixels;
    }
    if (!result.pixels)
        return {};
    result.mean = result.mean * (1.f / float(result.pixels));
    result.stats = renderer.GetStats();
    return result;
}

float Luminance(Vec3 value) {
    return value.x * .2126f + value.y * .7152f + value.z * .0722f;
}

bool ValidationGate(const RendererStats &stats) {
    char expected[16]{};
    const DWORD length = GetEnvironmentVariableA(
        "AEVRYND_EXPECT_GPU_VALIDATION_MODE", expected, DWORD(std::size(expected)));
    const std::string_view mode = length && length < std::size(expected)
                                      ? std::string_view(expected, length)
                                      : std::string_view{};
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
} // namespace

int main() {
    const HRESULT com = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (FAILED(com) && com != RPC_E_CHANGED_MODE)
        return 1;
    HWND window = CreateWindowExW(0, L"STATIC", L"Aevrynd foliage physical shading",
                                  WS_OVERLAPPED, 0, 0, Width, Height, nullptr,
                                  nullptr, GetModuleHandleW(nullptr), nullptr);
    if (!window)
        return 2;

    int result{};
    try {
        D3D12Renderer renderer;
        renderer.Initialize(window, Width, Height, Width, Height, false);
        renderer.SetQualitySettings({false, false, false, false});
        const auto front = CreateCard(renderer, false);
        const auto back = CreateCard(renderer, true);
        if (!front || !back)
            result = 3;

        std::array<std::optional<Sample>, 3> surfaceAo{};
        std::array<std::optional<Sample>, 3> foliageAo{};
        constexpr std::array<float, 3> ao{{0.f, .25f, 1.f}};
        for (unsigned index = 0; !result && index < ao.size(); ++index) {
            surfaceAo[index] = Capture(renderer, front, MaterialClass3D::Surface,
                                       ao[index], {0, 0, 1}, 1.f, 0.f,
                                       0x100u + index);
            foliageAo[index] = Capture(renderer, front, MaterialClass3D::Foliage,
                                       ao[index], {0, 0, 1}, 1.f, 0.f,
                                       0x110u + index);
            if (!surfaceAo[index] || !foliageAo[index] ||
                surfaceAo[index]->pixels < 1000 || foliageAo[index]->pixels < 1000)
                result = 4;
        }

        if (!result) {
            const float surfaceQuarter = Luminance(surfaceAo[1]->mean) /
                                         Luminance(surfaceAo[2]->mean);
            const float foliageQuarter = Luminance(foliageAo[1]->mean) /
                                         Luminance(foliageAo[2]->mean);
            const float surfaceZero = Luminance(surfaceAo[0]->mean);
            const float foliageZero = Luminance(foliageAo[0]->mean);
            std::cout << "ao surface=" << surfaceZero << ',' << surfaceQuarter
                      << ",1 foliage=" << foliageZero << ',' << foliageQuarter
                      << ",1\n";
            if (surfaceZero > .005f || foliageZero > .005f ||
                surfaceQuarter <= .05f || surfaceQuarter >= .95f ||
                foliageQuarter <= surfaceQuarter + .01f ||
                foliageQuarter >= 1.f)
                result = 5;
        }

        const auto surfaceBacklit =
            result ? std::optional<Sample>{}
                   : Capture(renderer, front, MaterialClass3D::Surface, 1.f,
                             {0, 0, -1}, 0.f, 1.f, 0x200u);
        const auto foliageBacklit =
            result ? std::optional<Sample>{}
                   : Capture(renderer, front, MaterialClass3D::Foliage, 1.f,
                             {0, 0, -1}, 0.f, 1.f, 0x201u);
        const auto foliageFront =
            result ? std::optional<Sample>{}
                   : Capture(renderer, front, MaterialClass3D::Foliage, 1.f,
                             {0, 0, 1}, 0.f, 1.f, 0x202u);
        const auto foliageBack =
            result ? std::optional<Sample>{}
                   : Capture(renderer, back, MaterialClass3D::Foliage, 1.f,
                             {0, 0, 1}, 0.f, 1.f, 0x203u);
        if (!result && (!surfaceBacklit || !foliageBacklit || !foliageFront ||
                        !foliageBack))
            result = 6;
        if (!result) {
            const float surfaceBacklitLuma = Luminance(surfaceBacklit->mean);
            const float foliageBacklitLuma = Luminance(foliageBacklit->mean);
            const float frontLuma = Luminance(foliageFront->mean);
            const float backLuma = Luminance(foliageBack->mean);
            const float parity = std::abs(frontLuma - backLuma) /
                                 (std::max)(frontLuma, backLuma);
            std::cout << "transmission surfaceBack=" << surfaceBacklitLuma
                      << " foliageBack=" << foliageBacklitLuma
                      << " twoSided=" << frontLuma << '/' << backLuma
                      << " parity=" << parity << '\n';
            if (surfaceBacklitLuma > .001f || foliageBacklitLuma < .02f ||
                foliageBacklitLuma < surfaceBacklitLuma + .02f ||
                frontLuma < .02f || backLuma < .02f || parity > .03f)
                result = 7;
        }

        if (!result && !ValidationGate(foliageBack->stats))
            result = 8;
        renderer.DestroyMesh(back);
        renderer.DestroyMesh(front);
        renderer.WaitIdle();
    } catch (const std::exception &error) {
        std::cerr << "exception=" << error.what() << '\n';
        result = 9;
    }

    DestroyWindow(window);
    if (SUCCEEDED(com))
        CoUninitialize();
    if (result)
        std::cerr << "Foliage physical shading GPU evidence failed at " << result
                  << '\n';
    return result;
}
