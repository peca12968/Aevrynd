#include "Aevrynd/Camera.h"
#include "Aevrynd/Mesh3D.h"
#include "Aevrynd/Renderer.h"

#include <Windows.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <optional>
#include <string>
#include <utility>
#include <vector>

using namespace Aevrynd;

namespace {
constexpr unsigned Width = 320, Height = 180;
// 128/255 解碼後的兩個切線分量各為 1/255；單位白光、灰色漫射底色、
// roughness=1 限制量化造成的輻射差異。0.002 為預先固定的逐通道 HDR 容差。
constexpr float FlatHdrTolerance = .002f;
constexpr float DetailMinimumMeanDelta = .002f;
constexpr std::size_t MinimumObjectPixels = 1000;
const RenderObjectIdentity3D Identity{{0x545249504c414e41ull, 0x524e4f524d414c31ull}, 1};

void Put32(std::vector<std::byte>& bytes, std::size_t offset, std::uint32_t value) {
    for (unsigned index = 0; index < 4; ++index)
        bytes[offset + index] = std::byte(value >> (index * 8));
}

bool WriteBmp(const std::filesystem::path& path, std::array<std::uint8_t, 4> first,
              std::array<std::uint8_t, 4> second, bool patterned = false) {
    constexpr unsigned Size = 16;
    std::vector<std::byte> bytes(54 + Size * Size * 4);
    bytes[0] = std::byte{'B'};
    bytes[1] = std::byte{'M'};
    Put32(bytes, 2, std::uint32_t(bytes.size()));
    Put32(bytes, 10, 54);
    Put32(bytes, 14, 40);
    Put32(bytes, 18, Size);
    Put32(bytes, 22, Size);
    bytes[26] = std::byte{1};
    bytes[28] = std::byte{32};
    Put32(bytes, 34, Size * Size * 4);
    for (unsigned y = 0; y < Size; ++y) for (unsigned x = 0; x < Size; ++x) {
        const auto& rgba = patterned && (((x / 2) + (y / 2)) & 1u) ? second : first;
        const std::size_t offset = 54 + (std::size_t(y) * Size + x) * 4;
        bytes[offset] = std::byte{rgba[2]};
        bytes[offset + 1] = std::byte{rgba[1]};
        bytes[offset + 2] = std::byte{rgba[0]};
        bytes[offset + 3] = std::byte{rgba[3]};
    }
    std::ofstream output(path, std::ios::binary);
    output.write(reinterpret_cast<const char*>(bytes.data()), std::streamsize(bytes.size()));
    return bool(output);
}

Mesh3D SlopeCard(Vec3 normal) {
    normal = Normalize(normal);
    const Vec3 reference = std::abs(normal.y) > .95f ? Vec3{1, 0, 0} : Vec3{0, 1, 0};
    const Vec3 tangent = Normalize(Cross(reference, normal));
    const Vec3 bitangent = Cross(normal, tangent);
    const auto packedNormal = PackNormalSnorm8(normal);
    const auto white = PackColorRgba8({1, 1, 1, 1});
    Mesh3D mesh;
    for (const Vec2 corner : std::array<Vec2, 4>{{{-2, -1}, {-2, 1}, {2, 1}, {2, -1}}}) {
        const Vec3 position = tangent * corner.x + bitangent * corner.y;
        mesh.vertices.push_back({position.x, position.y, position.z, packedNormal, white, 0, 0});
    }
    mesh.indices = {0, 1, 2, 0, 2, 3};
    return mesh;
}

struct Capture {
    D3D12RendererTestAccess3D::LinearHdrCapture3D hdr;
    ObjectIdCapture3D oid;
    DepthCapture3D depth;
};

std::optional<Capture> CaptureSlope(D3D12Renderer& renderer, MeshHandle3D mesh,
                                  MaterialTextureSetHandle3D textures, Vec3 normal,
                                  float normalScale, std::uint64_t serial) {
    normal = Normalize(normal);
    const Vec3 reference = std::abs(normal.y) > .95f ? Vec3{1, 0, 0} : Vec3{0, 1, 0};
    const Vec3 up = Cross(normal, Normalize(Cross(reference, normal)));
    Camera3D camera{normal * 4.f, {0, 0, 0}, up, {}};
    DrawList draw;
    draw.SetTimeOfDayExposure({false});
    draw.SetViewProjection(CreateViewProjectionMatrix(camera, float(Width) / float(Height)));
    draw.SetBackfaceCulling(false);
    Atmosphere3D atmosphere{};
    atmosphere.enabled = false;
    atmosphere.cameraPosition = camera.position;
    draw.SetAtmosphere(atmosphere);
    // 沿較小的非零法線分量入射，讓錯誤的四次方軸偏轉有可觀測的光照差。
    const Vec3 incoming = std::abs(normal.x) > .01f
        ? Vec3{normal.x < 0.f ? -1.f : 1.f, 0, 0}
        : Vec3{0, normal.y < 0.f ? -1.f : 1.f, 0};
    draw.SetDirectionalLight({incoming * -1.f, 0.f, {1, 1, 1, 1}, 1.f});
    EnvironmentLightingBlendSettings3D blend{};
    blend.diffuseIntensity = blend.specularIntensity = 0.f;
    if (!draw.SetEnvironmentLightingBlend(blend)) return {};
    if (!draw.MeshInstance(mesh, Matrix4::Identity(), {1, 1, 1, 1}, {1, 0, 0, 1},
            MeshRenderLayer3D::Opaque, textures, 1.f, false, MaterialClass3D::Surface,
            std::numeric_limits<float>::quiet_NaN(), false, 1.f, 0, false,
            DirectionalShadowAllCascades3D, ShadowCascadeSelection3D::Automatic,
            Matrix4::Identity(), false, normalScale, MaterialAlphaMode3D::Opaque,
            .5f, false, Identity)) return {};
    const ObjectIdCaptureRequestMetadata3D metadata{serial, serial ^ 0x21u, serial ^ 0x43u};
    if (!renderer.RequestObjectIdReadback3D(metadata) || !renderer.RequestDepthReadback3D(metadata)) return {};
    renderer.Render(draw, {0, 0, 0, 1});
    auto oid = renderer.TakeObjectIdReadback3D();
    auto depth = renderer.TakeDepthReadback3D();
    auto hdr = D3D12RendererTestAccess3D::CaptureLinearHdr(renderer, 0.f);
    renderer.WaitIdle();
    if (!oid || !depth || !hdr || !oid->Complete() || !depth->Complete() || !hdr->Complete()
        || oid->requestMetadata != metadata || depth->requestMetadata != metadata
        || hdr->width != oid->width || hdr->height != oid->height
        || depth->width != oid->width || depth->height != oid->height) return {};
    return Capture{std::move(*hdr), std::move(*oid), std::move(*depth)};
}

struct Delta { float mean{}, maximum{}; std::size_t count{}; bool finite{true}, geometry{true}; };

Delta Compare(const Capture& left, const Capture& right) {
    Delta result;
    result.geometry = left.oid.pixels == right.oid.pixels
        && left.oid.tokenMap == right.oid.tokenMap
        && left.depth.pixelSha256 == right.depth.pixelSha256;
    if (!result.geometry || left.hdr.pixels.size() != right.hdr.pixels.size()) return result;
    double sum{};
    for (std::size_t index = 0; index < left.oid.pixels.size(); ++index) {
        const auto* resolved = left.oid.Resolve(left.oid.pixels[index]);
        if (!resolved || *resolved != Identity) continue;
        const auto a = left.hdr.pixels[index], b = right.hdr.pixels[index];
        const float delta = (std::max)({std::abs(a.x - b.x), std::abs(a.y - b.y), std::abs(a.z - b.z)});
        result.finite = result.finite && std::isfinite(a.x) && std::isfinite(a.y) && std::isfinite(a.z)
            && std::isfinite(b.x) && std::isfinite(b.y) && std::isfinite(b.z);
        result.maximum = (std::max)(result.maximum, delta);
        sum += delta;
        ++result.count;
    }
    if (result.count) result.mean = float(sum / double(result.count));
    return result;
}

bool Report(const char* name, const Capture& baseline, const Capture& candidate, bool detailExpected) {
    const auto delta = Compare(baseline, candidate);
    const bool passed = delta.geometry && delta.finite && delta.count >= MinimumObjectPixels
        && (detailExpected ? delta.mean >= DetailMinimumMeanDelta : delta.maximum <= FlatHdrTolerance);
    std::cout << name << " passed=" << passed << " pixels=" << delta.count
              << " geometry=" << delta.geometry << " finite=" << delta.finite
              << " meanMaxChannelDelta=" << delta.mean << " maximumChannelDelta=" << delta.maximum << '\n';
    return passed;
}
}

int main() {
    const auto root = std::filesystem::temp_directory_path()
        / ("aevrynd-triplanar-normal-" + std::to_string(GetCurrentProcessId()) + "-" + std::to_string(GetTickCount64()));
    std::error_code error;
    if (!std::filesystem::create_directory(root, error) || error) return 1;
    const auto base = root / L"base.bmp", orm = root / L"orm.bmp", flat = root / L"flat.bmp",
        pattern = root / L"pattern.bmp", reverse = root / L"reverse.bmp";
    if (!WriteBmp(base, {128, 128, 128, 255}, {}) || !WriteBmp(orm, {255, 255, 0, 255}, {})
        || !WriteBmp(flat, {128, 128, 255, 255}, {})
        || !WriteBmp(pattern, {56, 128, 220, 255}, {200, 128, 220, 255}, true)
        || !WriteBmp(reverse, {255, 0, 128, 255}, {})) {
        std::filesystem::remove_all(root, error);
        return 2;
    }
    HWND window = CreateWindowExW(0, L"STATIC", L"AevryndTriplanarNormal", WS_OVERLAPPED,
        0, 0, Width, Height, nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
    if (!window) { std::filesystem::remove_all(root, error); return 3; }
    int result{};
    try {
        D3D12Renderer renderer;
        renderer.Initialize(window, Width, Height, Width, Height, false);
        renderer.SetQualitySettings({false, false, false, false});
        const auto createTextures = [&](const std::filesystem::path& normal) {
            MaterialTextureSetDefinition3D definition{base, orm, 16, normal};
            definition.surfaceEncoding = MaterialSurfaceMapEncoding3D::PackedOrm;
            return renderer.CreateMaterialTextureSet(definition);
        };
        const auto flatTextures = createTextures(flat), patternTextures = createTextures(pattern),
            reverseTextures = createTextures(reverse);
        if (!flatTextures || !patternTextures || !reverseTextures) result = 4;
        const std::array<Vec3, 5> normals{{{.5f, .8660254f, 0}, {-.5f, .8660254f, 0},
            {.5f, -.8660254f, 0}, {.35f, .6f, -.72f}, {0, .6f, -.8f}}};
        std::uint64_t serial = 0x545249500000ull;
        for (std::size_t index = 0; !result && index < normals.size(); ++index) {
            const auto source = SlopeCard(normals[index]);
            const auto mesh = renderer.CreateMesh(source.vertices, source.indices);
            if (!mesh) { result = 4; break; }
            const auto zero = CaptureSlope(renderer, mesh, flatTextures, normals[index], 0.f, ++serial);
            const auto partial = CaptureSlope(renderer, mesh, flatTextures, normals[index], .65f, ++serial);
            const auto full = CaptureSlope(renderer, mesh, flatTextures, normals[index], 1.f, ++serial);
            if (!zero || !partial || !full) result = 5;
            else {
                std::cout << "slope=" << index << " normal=" << normals[index].x << ',' << normals[index].y << ',' << normals[index].z << '\n';
                if (!Report("flat-scale-.65", *zero, *partial, false)
                    || !Report("flat-scale-1", *zero, *full, false)) result = 6;
                if (!result && index == 0) {
                    const auto detail = CaptureSlope(renderer, mesh, patternTextures, normals[index], 1.f, ++serial);
                    const auto detailZero = CaptureSlope(renderer, mesh, patternTextures, normals[index], 0.f, ++serial);
                    if (!detail || !detailZero) result = 5;
                    else if (!Report("pattern-detail-retained", *zero, *detail, true)
                        || !Report("pattern-scale-zero", *zero, *detailZero, false)) result = 7;
                }
                if (!result && index + 1 == normals.size()) {
                    // 此 RG 極值在舊軸混合法線下與幾何法線的點積為負，
                    // scale=0 必須仍與平坦貼圖的幾何法線基準一致。
                    const auto adversarial = CaptureSlope(renderer, mesh, reverseTextures, normals[index], 0.f, ++serial);
                    if (!adversarial) result = 5;
                    else if (!Report("opposed-detail-scale-zero", *zero, *adversarial, false)) result = 8;
                }
            }
            renderer.DestroyMesh(mesh);
        }
        const auto stats = renderer.GetStats();
        wchar_t validationFlag[3]{};
        const bool validationExpected = GetEnvironmentVariableW(L"AEVRYND_GPU_VALIDATION", validationFlag, 3) == 1
            && validationFlag[0] == L'1';
        std::cout << "gpuValidation expected=" << validationExpected << " requested=" << stats.gpuValidationRequested
                  << " available=" << stats.gpuValidationAvailable << " executed=" << stats.gpuValidationExecuted
                  << " passed=" << stats.gpuValidationPassed << " debugLayer=" << stats.debugLayerEnabled
                  << " errors=" << stats.debugErrorCount << " warnings=" << stats.debugWarningCount << '\n';
        if (!result && (stats.debugErrorCount || stats.debugWarningCount
            || (validationExpected && (!stats.gpuValidationRequested || !stats.debugLayerEnabled)))) result = 9;
        if (!result && stats.gpuValidationRequested && (!stats.gpuValidationAvailable
            || !stats.gpuValidationExecuted || !stats.gpuValidationPassed
            || stats.debugErrorCount || stats.debugWarningCount)) result = 9;
        for (auto textures : {flatTextures, patternTextures, reverseTextures})
            if (textures) renderer.DestroyMaterialTextureSet(textures);
        renderer.WaitIdle();
        const auto released = renderer.GetStats();
        std::cout << "cleanup meshes=" << released.residentMeshes << " textures=" << released.residentMaterialTextureSets << '\n';
        if (!result && (released.residentMeshes || released.residentMaterialTextureSets)) result = 11;
    } catch (const std::exception& exception) {
        std::cerr << "Triplanar normal GPU exception: " << exception.what() << '\n';
        result = 10;
    }
    DestroyWindow(window);
    std::filesystem::remove_all(root, error);
    if (result) std::cerr << "Triplanar normal GPU failed at " << result << '\n';
    return result;
}
