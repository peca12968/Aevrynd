#include <Aevrynd/SceneECS3D.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <limits>
#include <optional>
#include <ranges>
#include <windows.h>

using namespace Aevrynd;

namespace {
using Capture = D3D12RendererTestAccess3D::DirectionalShadowCapture3D;
constexpr HlodClusterId3D ClusterId{0x484c4f445f534844ull, 7};
constexpr std::uint64_t ClusterGeneration = 11;
enum class Mode { Source, Proxy, Transition };
struct ShadowPair { std::array<Capture, 2> cascades{}; };
struct Fixture { MeshResource3D box{}, backface{}; MaterialTextureSetHandle3D masked{}; };

bool Validation(const RendererStats& stats) {
    wchar_t expected[8]{};
    const DWORD count = GetEnvironmentVariableW(L"AEVRYND_EXPECT_GPU_VALIDATION_MODE", expected, DWORD(std::size(expected)));
    if (count == 6 && lstrcmpW(expected, L"normal") == 0)
        return !stats.gpuValidationRequested && !stats.gpuValidationAvailable && !stats.gpuValidationExecuted &&
               !stats.debugLayerEnabled && !stats.gpuValidationPassed && !stats.debugErrorCount && !stats.debugWarningCount;
    if (count == 3 && lstrcmpW(expected, L"gbv") == 0)
        return stats.gpuValidationRequested && stats.gpuValidationAvailable && stats.gpuValidationExecuted &&
               stats.debugLayerEnabled && stats.gpuValidationPassed && !stats.debugErrorCount && !stats.debugWarningCount;
    return false;
}

Mesh3D UvBox() {
    Mesh3D mesh = Mesh3D::CreateUnitBox();
    constexpr std::array<Vec2, 4> uv{{{0, 1}, {1, 1}, {1, 0}, {0, 0}}};
    for (std::size_t index = 0; index < mesh.vertices.size(); ++index) {
        mesh.vertices[index].u = uv[index % uv.size()].x;
        mesh.vertices[index].v = uv[index % uv.size()].y;
    }
    return mesh;
}

Mesh3D BackfaceQuad() {
    Mesh3D mesh;
    const std::uint32_t normal = PackNormalSnorm8({0, -1, 0});
    mesh.vertices = {{-1, 0, -1, normal, 0xffffffffu, 0, 1}, {1, 0, -1, normal, 0xffffffffu, 1, 1},
                     {1, 0, 1, normal, 0xffffffffu, 1, 0}, {-1, 0, 1, normal, 0xffffffffu, 0, 0}};
    mesh.indices = {0, 1, 2, 0, 2, 3};
    return mesh;
}

MeshResource3D Resource(MeshHandle3D handle, const Mesh3D& mesh) {
    return {handle, mesh.Bounds(), mesh.vertices.size(), mesh.indices.size(), {}};
}

Material3D Material(MaterialTextureSetHandle3D textures, MaterialAlphaMode3D alphaMode, bool doubleSided,
                    bool castsShadow = true) {
    Material3D material{};
    material.castsShadow = castsShadow;
    material.textures = textures;
    material.useUv = alphaMode == MaterialAlphaMode3D::Mask;
    material.alphaMode = alphaMode;
    material.alphaCutoff = .5f;
    material.doubleSided = doubleSided;
    return material;
}

SceneEntityDefinition3D Definition(MeshResource3D mesh, Material3D material, Transform3D transform,
                                   RenderObjectIdentity3D identity) {
    SceneMeshRenderable3D renderable{};
    renderable.nearMesh = mesh;
    renderable.nearMaterial = material;
    renderable.identity = identity;
    renderable.resourceOwnerIdentity = 0x534841444f575f31ull;
    renderable.resourceGeneration = 1;
    return {SceneTransformComponent3D{transform}, renderable, {}};
}

bool CreateBatch(SceneECSRegistry3D& registry, MeshResource3D mesh, Material3D material,
                 SceneHlodEntityBatch3D& batch, bool incompatibleProxy = false) {
    const Transform3D transform{{0, 0, 30}, {}, {14, 14, 72}};
    const RenderObjectIdentity3D sourceIdentity{{0x100, 1}, 3};
    const auto source = Definition(mesh, material, transform, sourceIdentity);
    auto proxyMaterial = material;
    if (incompatibleProxy) proxyMaterial.doubleSided = !proxyMaterial.doubleSided;
    const auto proxy = Definition(mesh, proxyMaterial, transform,
                                  {HlodProxyRenderObjectId3D(ClusterId), ClusterGeneration});
    const auto sourceSemantic = HlodShadowSemanticFromRenderable3D(*source.renderable);
    const auto proxySemantic = HlodShadowSemanticFromRenderable3D(*proxy.renderable);
    if (!sourceSemantic || !proxySemantic) return false;
    HlodSourceBuildInput3D sourceBuild{};
    sourceBuild.reference = {sourceIdentity.id};
    sourceBuild.generation = sourceIdentity.generation;
    sourceBuild.meshHandle = mesh.handle.value;
    sourceBuild.materialHandle = material.textures.value;
    sourceBuild.meshGeneration = sourceBuild.materialGeneration = 1;
    sourceBuild.localBoundsCenter = mesh.bounds.center;
    sourceBuild.localBoundsExtents = {1, 1, 1};
    sourceBuild.position = transform.position;
    sourceBuild.rotation = transform.rotation;
    sourceBuild.scale = transform.scale;
    sourceBuild.shadow = *sourceSemantic;
    sourceBuild.meshContentPrimary = mesh.handle.contentPrimary;
    sourceBuild.meshContentSecondary = mesh.handle.contentSecondary;
    sourceBuild.materialSemanticIdentity = material.textures.semanticIdentity;
    sourceBuild.resourceOwnerIdentity = source.renderable->resourceOwnerIdentity;
    const HlodSourceResourceContract3D sourceContract{
        sourceBuild.reference, sourceBuild.generation, sourceBuild.meshHandle, sourceBuild.materialHandle,
        sourceBuild.meshGeneration, sourceBuild.materialGeneration, HlodSourceResourceFingerprint3D(sourceBuild),
        sourceBuild.shadow};
    const HlodShadowContract3D shadowContract{*proxySemantic, HlodShadowSemanticFingerprint3D(*sourceSemantic),
                                              HlodShadowSemanticFingerprint3D(*proxySemantic),
                                              *sourceSemantic == *proxySemantic};
    return registry.CreateHlodBatchAtomic3D(ClusterId, ClusterGeneration,
                                            std::span<const SceneEntityDefinition3D>{&source, 1}, proxy,
                                            std::span<const HlodSourceResourceContract3D>{&sourceContract, 1},
                                            *proxySemantic, shadowContract, batch);
}

HlodSelection3D Selection(Mode mode) {
    HlodSelection3D selection{};
    selection.clusterId = ClusterId;
    selection.sourceCoverage = selection.proxyCoverage = 0.f;
    selection.sourceCoverage8 = selection.proxyCoverage8 = 0;
    selection.proxyInvert = true;
    selection.ditherSeed = 5;
    if (mode == Mode::Source) {
        selection.mode = HlodSelectionMode3D::Source;
        selection.sourceCoverage = 1.f;
        selection.sourceCoverage8 = 255;
    } else if (mode == Mode::Proxy) {
        selection.mode = HlodSelectionMode3D::Proxy;
        selection.proxyCoverage = 1.f;
        selection.proxyCoverage8 = 255;
    } else {
        selection.mode = HlodSelectionMode3D::Transition;
        selection.sourceCoverage8 = 128;
        selection.proxyCoverage8 = 127;
        selection.sourceCoverage = float(selection.sourceCoverage8) / 255.f;
        selection.proxyCoverage = float(selection.proxyCoverage8) / 255.f;
    }
    return selection;
}

std::size_t Foreground(const Capture& capture) {
    return std::ranges::count_if(capture.pixels, [](float depth) { return std::isfinite(depth) && depth < 1.f; });
}

std::optional<ShadowPair> CaptureMode(D3D12Renderer& renderer, SceneECSRegistry3D& registry,
                                      const SceneHlodEntityBatch3D& batch, Mode mode, const Material3D& expectedMaterial,
                                      RendererStats& stats,
                                      bool requireForeground = true) {
    const HlodSelection3D selection = Selection(mode);
    if (!registry.SetHlodSelectionAtomic3D(batch, selection) || !registry.MatchesHlodSelection3D(batch, selection)) {
        std::cerr << "selection bridge rejected mode=" << int(mode) << '\n';
        return {};
    }
    Scene3D scene;
    DrawList draw;
    const auto collected = registry.Collect(scene, draw);
    const Camera3D camera{{0, 8, -14}, {0, 0, 32}, {0, 1, 0},
                          {ProjectionMode::Perspective, .95f, 20.f, .1f, 100.f}};
    Atmosphere3D atmosphere{};
    atmosphere.cameraPosition = camera.position;
    draw.SetAtmosphere(atmosphere);
    draw.SetBackfaceCulling(true);
    draw.SetDirectionalLight({{.45f, -1.f, .25f}, .12f, {1, 1, 1, 1}, 2.f});
    draw.SetDirectionalShadowCascades({2, 512, .1f, 100.f, 0.f, .12f});
    Scene3DSubmissionOptions options{};
    options.frustumCulling = false;
    const auto submitted = scene.Submit(draw, camera, {640, 360}, options);
    const std::size_t expected = mode == Mode::Transition ? 2 : 1;
    if (collected.submittedRenderables != expected || submitted.visibleMeshInstances != expected || draw.meshInstances.size() != expected) {
        std::cerr << "collection bridge mode=" << int(mode) << " collected=" << collected.submittedRenderables
                  << " submitted=" << submitted.visibleMeshInstances << " mutable=" << draw.meshInstances.size() << '\n';
        return {};
    }
    const auto frozen = draw.FreezeMeshInstances3D();
    if (!frozen || frozen.Instances().size() != expected) {
        std::cerr << "freeze bridge mode=" << int(mode) << " instances=" << frozen.Instances().size() << '\n';
        return {};
    }
    if (std::ranges::any_of(frozen.Instances(), [&](const MeshInstance3D& instance) {
            return instance.textures != expectedMaterial.textures || instance.castsShadow != expectedMaterial.castsShadow ||
                   instance.alphaMode != expectedMaterial.alphaMode || instance.alphaCutoff != expectedMaterial.alphaCutoff ||
                   instance.doubleSided != expectedMaterial.doubleSided;
        })) {
        std::cerr << "shadow semantic bridge diverged mode=" << int(mode) << '\n';
        return {};
    }
    draw.SetStaticMeshSubmission3D(frozen);
    if (!draw.meshInstances.empty() || draw.EffectiveMeshInstances3D().size() != expected || !draw.StaticMeshSubmissionRevision3D()) {
        std::cerr << "static bridge mode=" << int(mode) << " mutable=" << draw.meshInstances.size()
                  << " effective=" << draw.EffectiveMeshInstances3D().size() << '\n';
        return {};
    }
    renderer.Render(draw, {.02f, .03f, .04f, 1});
    renderer.WaitIdle();
    ShadowPair result{};
    for (unsigned cascade = 0; cascade < result.cascades.size(); ++cascade) {
        const auto capture = D3D12RendererTestAccess3D::CaptureDirectionalShadow(renderer, cascade);
        if (!capture || !capture->Complete() || (requireForeground && !Foreground(*capture))) {
            std::cerr << "capture bridge mode=" << int(mode) << " cascade=" << cascade
                      << " present=" << bool(capture) << " complete=" << (capture && capture->Complete())
                      << " foreground=" << (capture ? Foreground(*capture) : 0) << '\n';
            return {};
        }
        result.cascades[cascade] = *capture;
    }
    stats = renderer.GetStats();
    return result;
}

bool ExactPair(const ShadowPair& a, const ShadowPair& b, const char* label) {
    for (std::size_t cascade = 0; cascade < a.cascades.size(); ++cascade) {
        const auto& left = a.cascades[cascade];
        const auto& right = b.cascades[cascade];
        if (!Foreground(left) || !Foreground(right) || left.pixels.size() != right.pixels.size()) {
            std::cerr << label << " cascade=" << cascade << " foreground=" << Foreground(left) << '/'
                      << Foreground(right) << " pixels=" << left.pixels.size() << '/' << right.pixels.size() << '\n';
            return false;
        }
        for (std::size_t index = 0; index < left.pixels.size(); ++index) {
            const float difference = std::abs(left.pixels[index] - right.pixels[index]);
            if (difference > 1e-7f) {
                std::cerr << label << " cascade=" << cascade << " pixel=" << index << " difference=" << difference << '\n';
                return false;
            }
        }
    }
    return true;
}

bool EmptyPair(const ShadowPair& pair) {
    return std::ranges::all_of(pair.cascades, [](const Capture& capture) { return Foreground(capture) == 0; });
}
}

int main() {
    HWND window = CreateWindowExW(0, L"STATIC", L"HLOD shadow parity", WS_OVERLAPPED, 0, 0, 640, 360,
                                  nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
    if (!window) return 1;
    int result{};
    try {
        D3D12Renderer renderer;
        renderer.Initialize(window, 640, 360, 640, 360, false);
        renderer.SetQualitySettings({true, false, false, false});
        const Mesh3D boxCpu = UvBox();
        const Mesh3D backfaceCpu = BackfaceQuad();
        Fixture fixture{};
        fixture.box = Resource(renderer.CreateMesh(boxCpu.vertices, boxCpu.indices), boxCpu);
        fixture.backface = Resource(renderer.CreateMesh(backfaceCpu.vertices, backfaceCpu.indices), backfaceCpu);
        const std::filesystem::path hairRoot = std::filesystem::path{AEVRYND_SOURCE_DIR} / "games" / "character_lab" /
                                                   "assets" / "material-packs" / "original" / "hair";
        MaterialTextureSetDefinition3D maskDefinition{hairRoot / "base-color.png", hairRoot / "roughness.png", 256,
                                                      hairRoot / "normal-dx.png"};
        maskDefinition.alphaMode = MaterialAlphaMode3D::Mask;
        maskDefinition.alphaCutoff = .5f;
        fixture.masked = renderer.CreateMaterialTextureSet(maskDefinition);
        if (!fixture.box || !fixture.backface || !fixture.masked) result = 2;
        struct SemanticCase { const char* name; MeshResource3D mesh; Material3D material; };
        const std::array cases{
            SemanticCase{"opaque", fixture.box, Material(fixture.masked, MaterialAlphaMode3D::Opaque, false)},
            SemanticCase{"masked", fixture.box, Material(fixture.masked, MaterialAlphaMode3D::Mask, false)},
            SemanticCase{"double-sided", fixture.backface, Material(fixture.masked, MaterialAlphaMode3D::Opaque, true)}};
        RendererStats stats{};
        std::array<std::array<std::size_t, 2>, 3> foreground{};
        for (std::size_t caseIndex = 0; !result && caseIndex < cases.size(); ++caseIndex) {
            const auto& semantic = cases[caseIndex];
            SceneECSRegistry3D registry;
            SceneHlodEntityBatch3D batch;
            if (!CreateBatch(registry, semantic.mesh, semantic.material, batch)) {
                std::cerr << "batch creation rejected semantic=" << semantic.name << '\n';
                result = 3;
                break;
            }
            const auto source = CaptureMode(renderer, registry, batch, Mode::Source, semantic.material, stats);
            const auto proxy = CaptureMode(renderer, registry, batch, Mode::Proxy, semantic.material, stats);
            const auto transition = CaptureMode(renderer, registry, batch, Mode::Transition, semantic.material, stats);
            if (!source || !proxy || !transition || !ExactPair(*source, *proxy, semantic.name) ||
                !ExactPair(*source, *transition, semantic.name)) { result = 4; break; }
            for (std::size_t cascade = 0; cascade < 2; ++cascade) foreground[caseIndex][cascade] = Foreground(source->cascades[cascade]);
        }
        if (!result) {
            SceneECSRegistry3D noCasterRegistry;
            SceneHlodEntityBatch3D noCasterBatch;
            const auto noCasterMaterial = Material(fixture.masked, MaterialAlphaMode3D::Opaque, true, false);
            const auto noCaster = CreateBatch(noCasterRegistry, fixture.box, noCasterMaterial, noCasterBatch)
                                      ? CaptureMode(renderer, noCasterRegistry, noCasterBatch, Mode::Source,
                                                    noCasterMaterial, stats, false)
                                      : std::nullopt;
            if (!noCaster || !EmptyPair(*noCaster)) result = 5;
        }
        if (!result) {
            SceneECSRegistry3D rejectedRegistry;
            SceneHlodEntityBatch3D rejectedBatch;
            if (CreateBatch(rejectedRegistry, fixture.box, Material(fixture.masked, MaterialAlphaMode3D::Opaque, false), rejectedBatch, true) || rejectedBatch) {
                std::cerr << "incompatible shadow semantic batch was accepted\n";
                result = 6;
            }
            Scene3D rejectedScene;
            DrawList rejectedDraw;
            if (rejectedRegistry.Collect(rejectedScene, rejectedDraw).aliveEntities || rejectedScene.MeshCount()) result = 6;
        }
        if (!result && (D3D12RendererTestAccess3D::CaptureDirectionalShadow(renderer, 2) || !Validation(stats))) result = 7;
        if (!result)
            std::cout << "hlodShadowRealChain=passed semantics=3 modes=3 cascades=2 foreground="
                      << foreground[0][0] << '/' << foreground[0][1] << ',' << foreground[1][0] << '/'
                      << foreground[1][1] << ',' << foreground[2][0] << '/' << foreground[2][1]
                      << " frozenSubmission=passed noCaster=passed negative=passed validation=passed\n";
        renderer.DestroyMaterialTextureSet(fixture.masked);
        renderer.DestroyMesh(fixture.backface.handle);
        renderer.DestroyMesh(fixture.box.handle);
        renderer.WaitIdle();
    } catch (const std::exception& error) {
        std::cerr << "hlod_shadow_parity_exception=" << error.what() << '\n';
        result = 8;
    }
    DestroyWindow(window);
    return result;
}
