#include <Aevrynd/Camera.h>
#include <Aevrynd/CookedRenderManifest3D.h>
#include <Aevrynd/MeshLoader.h>
#include <Aevrynd/NativeMaterialDds3D.h>
#include <Aevrynd/Renderer.h>

#include <Windows.h>

#include <array>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <limits>

using namespace Aevrynd;

namespace {
constexpr unsigned Width = 256;
constexpr unsigned Height = 256;
constexpr float NearExtent = .8f;
constexpr float FarExtent = .2f;
constexpr unsigned MaximumTextureResolution = 1024;
constexpr double MinimumAtlasCoverage = .15;
constexpr double MaximumAtlasCoverage = .40;
constexpr double MinimumFullCoverage = .15;
constexpr double MaximumFullCoverage = .40;
constexpr double MaximumMipCoverageDelta = .06;

struct ProbeResult {
    bool valid{};
    std::size_t foliagePixels{};
    std::size_t quadPixels{};
    std::size_t holes{};
    std::size_t semanticOutsideQuad{};
    double coverage{};
    RendererStats stats{};
};

struct ActualMeshProbeResult {
    bool valid{};
    std::size_t foliagePixels{};
    std::size_t backgroundPixels{};
    double coverage{};
    std::uint64_t viewProjectionHash{};
    RendererStats stats{};
};

MeshHandle3D CreateQuad(D3D12Renderer &renderer, float extent, float minimumU,
                        float minimumV, float maximumU, float maximumV) {
    // Alpha oracle 固定風權重為零，避免頂點風擺改變像素母體與 mip 導數。
    const std::uint32_t normal = PackNormalSnorm8({0.f, 0.f, -1.f}, 1.f);
    const std::array<Vertex3D, 4> vertices{{
        {-extent, -extent, .3f, normal, 0xffffffffu, minimumU, maximumV},
        {-extent, extent, .3f, normal, 0xffffffffu, minimumU, minimumV},
        {extent, extent, .3f, normal, 0xffffffffu, maximumU, minimumV},
        {extent, -extent, .3f, normal, 0xffffffffu, maximumU, maximumV},
    }};
    constexpr std::array<std::uint32_t, 6> indices{{0, 1, 2, 0, 2, 3}};
    return renderer.CreateMesh(vertices, indices);
}

ProbeResult CaptureProbe(D3D12Renderer &renderer, MeshHandle3D mesh,
                         MaterialTextureSetHandle3D textures,
                         const Material3D &material,
                         RenderObjectIdentity3D identity, float extent,
                         ObjectIdCaptureRequestMetadata3D metadata) {
    ProbeResult result{};
    DrawList draw;
    draw.SetViewProjection(Matrix4::Identity());
    draw.SetBackfaceCulling(true);
    const bool accepted = draw.MeshInstance(
        mesh, Matrix4::Identity(), material.tint, material.surface,
        MeshRenderLayer3D::Opaque, textures, material.textureScale,
        material.useUv, material.materialClass,
        (std::numeric_limits<float>::quiet_NaN)(), material.castsShadow, 1.f, 0,
        false, DirectionalShadowAllCascades3D,
        ShadowCascadeSelection3D::Automatic, Matrix4::Identity(), false,
        material.normalScale, material.alphaMode, material.alphaCutoff,
        material.doubleSided, identity);
    if (!accepted || !renderer.RequestObjectIdReadback3D(metadata))
        return result;

    renderer.Render(draw, {0.f, 0.f, 0.f, 1.f});
    const auto capture = renderer.TakeObjectIdReadback3D();
    renderer.WaitIdle();
    result.stats = renderer.GetStats();
    if (!capture)
        return result;

    constexpr float RasterEdgeTolerance = 1.f / float(Width);
    std::size_t foreignSemanticPixels{};
    for (unsigned y = 0; y < Height; ++y) {
        for (unsigned x = 0; x < Width; ++x) {
            const std::uint32_t token =
                capture->pixels[std::size_t(y) * Width + x];
            const RenderObjectIdentity3D *resolved = capture->Resolve(token);
            const float ndcX = 2.f * (float(x) + .5f) / float(Width) - 1.f;
            const float ndcY = 1.f - 2.f * (float(y) + .5f) / float(Height);
            const bool inside =
                std::abs(ndcX) <= extent + RasterEdgeTolerance &&
                std::abs(ndcY) <= extent + RasterEdgeTolerance;
            if (inside) {
                ++result.quadPixels;
                if (!token)
                    ++result.holes;
                else if (resolved && *resolved == identity)
                    ++result.foliagePixels;
                else
                    ++foreignSemanticPixels;
            } else if (token) {
                ++result.semanticOutsideQuad;
            }
        }
    }
    result.coverage = result.quadPixels
                          ? double(result.foliagePixels) /
                                double(result.quadPixels)
                          : 0.0;
    result.valid =
        capture->Complete() && capture->maskedStaticComplete &&
        !capture->rejectedMaskedStatic && capture->tokenMap.size() == 1 &&
        capture->semanticPixelCount == result.foliagePixels &&
        !foreignSemanticPixels && !result.semanticOutsideQuad && result.holes &&
        result.stats.lastSubmittedMeshInstances == 1 &&
        result.stats.lastSubmittedTexturedMeshInstances == 1 &&
        result.stats.lastSubmittedMaskedMeshInstances == 1 &&
        result.stats.lastSubmittedDoubleSidedMeshInstances == 1 &&
        result.stats.lastSubmittedFoliageInstances == 1;
    return result;
}

ActualMeshProbeResult CaptureActualMeshProbe(
    D3D12Renderer &renderer, MeshHandle3D mesh,
    MaterialTextureSetHandle3D textures, const Material3D &material,
    RenderObjectIdentity3D identity, const Camera3D &camera,
    ObjectIdCaptureRequestMetadata3D metadata) {
    ActualMeshProbeResult result{};
    DrawList draw;
    draw.SetViewProjection(CreateViewProjectionMatrix(
        camera, float(Width) / float(Height)));
    draw.SetBackfaceCulling(true);
    const bool accepted = draw.MeshInstance(
        mesh, Matrix4::Identity(), material.tint, material.surface,
        MeshRenderLayer3D::Opaque, textures, material.textureScale,
        material.useUv, material.materialClass,
        (std::numeric_limits<float>::quiet_NaN)(), material.castsShadow, 1.f, 0,
        false, DirectionalShadowAllCascades3D,
        ShadowCascadeSelection3D::Automatic, Matrix4::Identity(), false,
        material.normalScale, material.alphaMode, material.alphaCutoff,
        material.doubleSided, identity);
    if (!accepted || !renderer.RequestObjectIdReadback3D(metadata))
        return result;
    renderer.Render(draw, {0.f, 0.f, 0.f, 1.f});
    const auto capture = renderer.TakeObjectIdReadback3D();
    renderer.WaitIdle();
    result.stats = renderer.GetStats();
    if (!capture)
        return result;

    std::size_t foreignSemanticPixels{};
    for (const std::uint32_t token : capture->pixels) {
        if (!token) {
            ++result.backgroundPixels;
            continue;
        }
        const RenderObjectIdentity3D *resolved = capture->Resolve(token);
        if (resolved && *resolved == identity)
            ++result.foliagePixels;
        else
            ++foreignSemanticPixels;
    }
    result.coverage = double(result.foliagePixels) / double(capture->pixels.size());
    result.viewProjectionHash = capture->rasterProvenance.viewProjectionHash;
    result.valid =
        capture->Complete() && capture->maskedStaticComplete &&
        !capture->rejectedMaskedStatic && capture->tokenMap.size() == 1 &&
        capture->semanticPixelCount == result.foliagePixels &&
        !foreignSemanticPixels && result.foliagePixels >= 64 &&
        result.backgroundPixels && result.coverage < .55 &&
        result.stats.lastSubmittedMeshInstances == 1 &&
        result.stats.lastSubmittedTexturedMeshInstances == 1 &&
        result.stats.lastSubmittedMaskedMeshInstances == 1 &&
        result.stats.lastSubmittedDoubleSidedMeshInstances == 1 &&
        result.stats.lastSubmittedFoliageInstances == 1;
    return result;
}
} // 命名空間

int main() {
    HWND window = CreateWindowExW(0, L"STATIC", L"AevryndFoliageAlphaGpuOracle",
                                  WS_OVERLAPPED, 0, 0, Width, Height, nullptr,
                                  nullptr, GetModuleHandleW(nullptr), nullptr);
    if (!window)
        return 1;

    int result{};
    try {
        const std::filesystem::path manifestPath =
            std::filesystem::path{AEVRYND_SOURCE_DIR} /
            "engine/assets/realistic-cc0-first-batch/cooked/runtime-render-manifest.json";
        CookedRenderManifest3D manifest;
        CookedRenderManifestError3D manifestError;
        if (!manifest.LoadFile(manifestPath, &manifestError)) {
            std::cerr << "manifest=" << manifestError.message << '\n';
            result = 2;
        }

        const CookedRenderMaterial3D *leaves =
            manifest.FindMaterial("island_tree_01:leaves");
        MaterialAssetDefinition3D material =
            leaves ? leaves->Definition(MaximumTextureResolution)
                   : MaterialAssetDefinition3D{};
        if (!leaves || material.textures.alphaMode != MaterialAlphaMode3D::Mask ||
            material.material.alphaMode != MaterialAlphaMode3D::Mask ||
            material.textures.alphaCutoff != .5f ||
            material.material.alphaCutoff != .5f ||
            !material.material.doubleSided ||
            material.material.materialClass != MaterialClass3D::Foliage) {
            std::cerr << "cooked foliage semantics changed\n";
            result = 3;
        }

        material.textures.baseColorPath =
            manifest.AssetRoot() / material.textures.baseColorPath;
        material.textures.roughnessPath =
            manifest.AssetRoot() / material.textures.roughnessPath;
        material.textures.normalPath =
            manifest.AssetRoot() / material.textures.normalPath;
        NativeMaterialDds3D baseDds;
        NativeMaterialDdsError3D ddsError{};
        if (!leaves || !LoadNativeMaterialDds3D(
                           material.textures.baseColorPath,
                           NativeMaterialDdsRole3D::BaseColor,
                           MaximumTextureResolution, false, baseDds, &ddsError,
                           leaves->baseColorSha256, leaves->baseColorBytes,
                           true) ||
            baseDds.width != MaximumTextureResolution ||
            baseDds.height != MaximumTextureResolution || baseDds.mipCount < 6) {
            std::cerr << "cooked foliage DDS mip chain failed error="
                      << unsigned(ddsError) << " width=" << baseDds.width
                      << " height=" << baseDds.height
                      << " mips=" << baseDds.mipCount << '\n';
            result = 4;
        }

        D3D12Renderer renderer;
        renderer.Initialize(window, Width, Height, Width, Height, false);
        renderer.SetQualitySettings({false, false, false, false});
        const RendererStats baseline = renderer.GetStats();
        const MaterialTextureSetHandle3D textures =
            renderer.CreateMaterialTextureSet(material.textures);
        const MeshHandle3D atlasMesh =
            CreateQuad(renderer, NearExtent, 0.f, .25f, .25f, .5f);
        const MeshHandle3D nearMesh =
            CreateQuad(renderer, NearExtent, 0.f, 0.f, 1.f, 1.f);
        const MeshHandle3D farMesh =
            CreateQuad(renderer, FarExtent, 0.f, 0.f, 1.f, 1.f);
        const CookedRenderMeshPart3D *leavesPart =
            manifest.FindMeshPart("island-tree-01:material-1");
        const MeshLoadResult3D actualLoaded =
            leavesPart ? LoadAevryndMeshFile(manifest.AssetRoot() /
                                             leavesPart->lodPaths[2])
                       : MeshLoadResult3D{};
        const MeshHandle3D actualMesh =
            actualLoaded ? renderer.CreateMesh(actualLoaded.mesh.vertices,
                                               actualLoaded.mesh.indices)
                         : MeshHandle3D{};
        if (!textures || !atlasMesh || !nearMesh || !farMesh || !leavesPart ||
            leavesPart->materialId != "island_tree_01:leaves" ||
            !actualLoaded || !actualMesh) {
            std::cerr << "foliage GPU resource setup failed\n";
            if (!actualLoaded.error.empty())
                std::cerr << "actualMesh=" << actualLoaded.error << '\n';
            result = 4;
        }

        const ProbeResult atlas = CaptureProbe(
            renderer, atlasMesh, textures, material.material,
            {{0x464f4c4941474541ull, 0x41544c41534f5241ull}, 1}, NearExtent,
            {0xfa01u, 0xfa02u, 0xfa03u});
        const ProbeResult nearFull = CaptureProbe(
            renderer, nearMesh, textures, material.material,
            {{0x464f4c4941474541ull, 0x4e45415246554c4cull}, 2}, NearExtent,
            {0xfb01u, 0xfb02u, 0xfb03u});
        const ProbeResult farFull = CaptureProbe(
            renderer, farMesh, textures, material.material,
            {{0x464f4c4941474541ull, 0x46415246554c4c00ull}, 3}, FarExtent,
            {0xfc01u, 0xfc02u, 0xfc03u});

        const double nearPixelSpan = double(Width) * double(NearExtent);
        const double farPixelSpan = double(Width) * double(FarExtent);
        const double nearTexelsPerPixel = double(baseDds.width) / nearPixelSpan;
        const double farTexelsPerPixel = double(baseDds.width) / farPixelSpan;
        const double nearLod = std::log2(nearTexelsPerPixel);
        const double farLod = std::log2(farTexelsPerPixel);
        const double mipCoverageDelta =
            std::abs(nearFull.coverage - farFull.coverage);
        std::cout << "atlas=" << atlas.coverage << " nearFull="
                  << nearFull.coverage << " farFull=" << farFull.coverage
                  << " nearLod=" << nearLod << " farLod=" << farLod
                  << " lodDelta=" << farLod - nearLod
                  << " coverageDelta=" << mipCoverageDelta
                  << " nearHoles=" << nearFull.holes
                  << " farHoles=" << farFull.holes << '\n';

        if (!atlas.valid || atlas.coverage < MinimumAtlasCoverage ||
            atlas.coverage > MaximumAtlasCoverage || !nearFull.valid ||
            !farFull.valid || nearFull.coverage < MinimumFullCoverage ||
            nearFull.coverage > MaximumFullCoverage ||
            farFull.coverage < MinimumFullCoverage ||
            farFull.coverage > MaximumFullCoverage ||
            farLod - nearLod < 1.9 ||
            farLod >= double(baseDds.mipCount - 1) ||
            mipCoverageDelta > MaximumMipCoverageDelta ||
            farFull.foliagePixels >= nearFull.foliagePixels ||
            farFull.quadPixels >= nearFull.quadPixels ||
            farFull.stats.rejectedStaticAlphaBindings !=
                baseline.rejectedStaticAlphaBindings) {
            std::cerr << "cooked foliage alpha or mip GPU oracle failed\n";
            result = 5;
        }

        const MeshBounds3D actualBounds = actualLoaded.mesh.Bounds();
        const float radius = (std::max)(actualBounds.radius, .01f);
        const float cameraDistance = radius * 3.f + 1.f;
        CameraProjection orthographic{};
        orthographic.mode = ProjectionMode::Orthographic;
        orthographic.orthographicHeight = radius * 2.5f;
        orthographic.nearPlane = .01f;
        orthographic.farPlane = cameraDistance * 3.f + radius;
        const Camera3D godFront{
            actualBounds.center + Vec3{0.f, cameraDistance, 0.f},
            actualBounds.center, {0.f, 0.f, 1.f}, orthographic};
        const Vec3 obliqueDirection = Normalize(Vec3{.65f, 1.f, -.55f});
        const Camera3D godOblique{
            actualBounds.center + obliqueDirection * cameraDistance,
            actualBounds.center, {0.f, 0.f, 1.f}, orthographic};
        const Camera3D godBack{
            actualBounds.center - Vec3{0.f, cameraDistance, 0.f},
            actualBounds.center, {0.f, 0.f, 1.f}, orthographic};
        const ActualMeshProbeResult actualFront = CaptureActualMeshProbe(
            renderer, actualMesh, textures, material.material,
            {{0x41435455414c4c45ull, 0x46524f4e54000000ull}, 4}, godFront,
            {0xfd01u, 0xfd02u, 0xfd03u});
        const ActualMeshProbeResult actualOblique = CaptureActualMeshProbe(
            renderer, actualMesh, textures, material.material,
            {{0x41435455414c4c45ull, 0x4f424c4951554500ull}, 5}, godOblique,
            {0xfe01u, 0xfe02u, 0xfe03u});
        const ActualMeshProbeResult actualBack = CaptureActualMeshProbe(
            renderer, actualMesh, textures, material.material,
            {{0x41435455414c4c45ull, 0x4241434b00000000ull}, 6}, godBack,
            {0xff01u, 0xff02u, 0xff03u});
        const double frontBackRatio =
            actualFront.coverage > 0.0
                ? actualBack.coverage / actualFront.coverage
                : 0.0;
        std::cout << "actualVertices=" << actualLoaded.mesh.vertices.size()
                  << " actualTriangles=" << actualLoaded.mesh.indices.size() / 3
                  << " radius=" << actualBounds.radius
                  << " front=" << actualFront.coverage
                  << " oblique=" << actualOblique.coverage
                  << " back=" << actualBack.coverage
                  << " frontBackRatio=" << frontBackRatio
                  << " actualPixels=" << actualFront.foliagePixels << ','
                  << actualOblique.foliagePixels << ','
                  << actualBack.foliagePixels
                  << " backgrounds=" << actualFront.backgroundPixels << ','
                  << actualOblique.backgroundPixels << ','
                  << actualBack.backgroundPixels << '\n';
        if (!actualFront.valid || !actualOblique.valid || !actualBack.valid ||
            actualFront.viewProjectionHash == actualOblique.viewProjectionHash ||
            actualFront.viewProjectionHash == actualBack.viewProjectionHash ||
            actualOblique.viewProjectionHash == actualBack.viewProjectionHash ||
            frontBackRatio < .35 || frontBackRatio > 2.85) {
            std::cerr << "actual cooked foliage mesh GPU oracle failed\n";
            result = 8;
        }

        renderer.DestroyMaterialTextureSet(textures);
        renderer.DestroyMesh(actualMesh);
        renderer.DestroyMesh(farMesh);
        renderer.DestroyMesh(nearMesh);
        renderer.DestroyMesh(atlasMesh);
        renderer.WaitIdle();
        renderer.TrimTransientMemory();
        const RendererStats released = renderer.GetStats();
        if (released.residentMeshes != baseline.residentMeshes ||
            released.residentMeshReferences != baseline.residentMeshReferences ||
            released.residentMeshBytes != baseline.residentMeshBytes ||
            released.residentMaterialTextureSets !=
                baseline.residentMaterialTextureSets ||
            released.residentMaterialTextureReferences !=
                baseline.residentMaterialTextureReferences ||
            released.residentTextureBytes != baseline.residentTextureBytes ||
            released.residentTextureAllocationBytes !=
                baseline.residentTextureAllocationBytes ||
            released.pendingGpuReleaseBytes) {
            std::cerr << "foliage resources were retained after trim\n";
            result = 6;
        }
        if (released.gpuValidationRequested &&
            (!released.gpuValidationAvailable ||
             !released.gpuValidationExecuted || !released.debugLayerEnabled ||
             !released.gpuValidationPassed || released.debugErrorCount ||
             released.debugWarningCount)) {
            std::cerr << "gbv available=" << released.gpuValidationAvailable
                      << " executed=" << released.gpuValidationExecuted
                      << " passed=" << released.gpuValidationPassed
                      << " errors=" << released.debugErrorCount
                      << " warnings=" << released.debugWarningCount << '\n';
            result = 7;
        }
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        result = 9;
    }
    DestroyWindow(window);
    return result;
}
