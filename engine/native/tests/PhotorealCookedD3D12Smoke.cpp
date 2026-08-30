#include <Aevrynd/CookedRenderManifest3D.h>
#include <Aevrynd/EnvironmentIbl3D.h>
#include <Aevrynd/MaterialLibrary3D.h>
#include <Aevrynd/NativeMaterialDds3D.h>
#include <Aevrynd/StaticMeshAssetLibrary3D.h>

#include <Windows.h>

#include <array>
#include <iostream>
#include <memory>
#include <vector>

using namespace Aevrynd;

namespace {
int Result{};

class PhotorealCookedGame final : public IGame {
public:
    void OnStart(Engine &engine) override {
        baseline_ = engine.GetRendererStats();
        engine.SetMeshMemoryBudget(512ull * 1024ull * 1024ull);
        engine.SetMaterialTextureMemoryBudget(512ull * 1024ull * 1024ull);
        const auto path = std::filesystem::path{AEVRYND_SOURCE_DIR} /
                          "engine/assets/realistic-cc0-first-batch/cooked/runtime-render-manifest.json";
        CookedRenderManifestError3D manifestError;
        if (!manifest_.LoadFile(path, &manifestError))
            return Fail(engine, 1, manifestError.message);
        for (const auto &material : manifest_.Materials()) {
            constexpr std::uint64_t DdsHeaderBytes = 148;
            if (material.baseColorBytes <= DdsHeaderBytes ||
                material.surfaceBytes <= DdsHeaderBytes ||
                material.normalBytes <= DdsHeaderBytes)
                return Fail(engine, 1, "native DDS payload receipt is invalid");
            expectedNativePayloadBytes_ +=
                material.baseColorBytes + material.surfaceBytes +
                material.normalBytes - DdsHeaderBytes * 3;
        }

        if (!materials_.Initialize(engine, manifest_.AssetRoot()))
            return Fail(engine, 2, "material library initialization failed");
        std::vector<MaterialAssetDefinition3D> definitions;
        definitions.reserve(manifest_.Materials().size());
        for (const auto &material : manifest_.Materials())
            definitions.push_back(material.Definition());
        for (std::size_t index = 0; index < definitions.size(); ++index) {
            auto resolvedTextures = definitions[index].textures;
            resolvedTextures.baseColorPath = manifest_.AssetRoot() / resolvedTextures.baseColorPath;
            resolvedTextures.roughnessPath = manifest_.AssetRoot() / resolvedTextures.roughnessPath;
            resolvedTextures.normalPath = manifest_.AssetRoot() / resolvedTextures.normalPath;
            if (!engine.EstimateMaterialTextureSetAllocationBytes3D(
                    resolvedTextures)) {
                const auto &source = manifest_.Materials()[index];
                const std::array<std::filesystem::path, 3> paths{
                    source.baseColorPath, source.surfacePath, source.normalPath};
                const std::array<NativeMaterialDdsRole3D, 3> roles{
                    NativeMaterialDdsRole3D::BaseColor,
                    NativeMaterialDdsRole3D::Surface,
                    NativeMaterialDdsRole3D::Normal};
                const std::array<std::string, 3> hashes{
                    source.baseColorSha256, source.surfaceSha256,
                    source.normalSha256};
                const std::array<std::uint64_t, 3> bytes{
                    source.baseColorBytes, source.surfaceBytes,
                    source.normalBytes};
                for (std::size_t texture = 0; texture < paths.size(); ++texture) {
                    NativeMaterialDds3D native;
                    NativeMaterialDdsError3D nativeError{};
                    if (!LoadNativeMaterialDds3D(
                            manifest_.AssetRoot() / paths[texture], roles[texture],
                            4096, false, native, &nativeError, hashes[texture],
                            bytes[texture]))
                        return Fail(engine, 3,
                                    "native DDS preflight failed: " +
                                        definitions[index].name + " texture=" +
                                        std::to_string(texture) + " error=" +
                                        std::to_string(
                                            static_cast<unsigned>(nativeError)));
                }
                return Fail(engine, 3,
                            "renderer DDS estimate rejected validated payloads: " +
                                definitions[index].name);
            }
        }
        if (!materials_.LoadAll(definitions))
            return Fail(engine, 3,
                        "11 cooked material sets failed atomic GPU load");

        if (!meshes_.Initialize(engine, registry_, manifest_.AssetRoot()))
            return Fail(engine, 4, "static mesh library initialization failed");
        std::vector<StaticMeshAssetDefinition3D> meshDefinitions;
        meshDefinitions.reserve(manifest_.MeshParts().size());
        for (const auto &part : manifest_.MeshParts()) {
            const auto *material = manifest_.FindMaterial(part.materialId);
            if (!material)
                return Fail(engine, 5, "mesh material reference was not resolved");
            StaticMeshAssetDefinition3D definition{};
            definition.name = part.id;
            definition.meshPath = part.lodPaths[0];
            definition.material = material->Definition();
            definition.generateLods = false;
            definition.importedLodCount = 2;
            definition.importedLods[0].meshPath = part.lodPaths[1];
            definition.importedLods[1].meshPath = part.lodPaths[2];
            definition.lodSwitchDistances = {18.f, 48.f, 96.f};
            definition.lodTransitionWidths = {3.f, 6.f, 10.f};
            definition.cpuRetention = MeshLodCpuRetention3D::None;
            meshDefinitions.push_back(std::move(definition));
        }
        if (!meshes_.LoadAll(meshDefinitions))
            return Fail(engine, 6, "six cooked 3-LOD chains failed to load");

        leases_.reserve(meshDefinitions.size());
        entities_.reserve(meshDefinitions.size());
        for (std::size_t index = 0; index < meshDefinitions.size(); ++index) {
            auto lease = meshes_.ResolveRenderable(meshDefinitions[index].name);
            if (!lease || lease->Renderable().lodLevelCount != 3)
                return Fail(engine, 7, "typed renderable lease did not expose a 3-LOD chain");
            const auto entity = registry_.Create();
            Transform3D transform{};
            transform.position = {float(index % 3) * 4.f - 4.f, 0.f,
                                  9.f + float(index / 3) * 5.f};
            if (!entity || !registry_.SetTransform(entity, {transform}) ||
                !registry_.SetRenderable(entity, lease->Renderable()))
                return Fail(engine, 8, "typed lease could not enter render submission");
            entities_.push_back(entity);
            leases_.push_back(std::move(*lease));
        }

        const auto &ibl = manifest_.Ibl();
        environmentHandle_ = environment_.Create(
            {manifest_.AssetRoot() / ibl.specularPath,
             manifest_.AssetRoot() / ibl.irradiancePath,
             manifest_.AssetRoot() / ibl.brdfLutPath, 0.f});
        if (!environmentHandle_ ||
            !engine.CreateEnvironmentLighting3D(environment_, environmentHandle_,
                                                &createReceipt_) ||
            !engine.SetEnvironmentLighting3D(environment_, environmentHandle_, &setReceipt_))
            return Fail(engine, 9, "cooked IBL GPU create/set failed");
        const auto upload = engine.SubmitPendingUploads3D();
        if (!upload)
            return Fail(engine, 10, "GPU upload receipt was not produced");
        engine.WaitRendererIdle3D();
        if (!engine.IsUploadComplete3D(upload))
            return Fail(engine, 11, "GPU upload fence did not complete");
        const auto resident = engine.GetRendererStats();
        if (resident.residentMeshes != baseline_.residentMeshes + 18 ||
            resident.residentMeshReferences !=
                baseline_.residentMeshReferences + 18 ||
            resident.residentMaterialTextureSets !=
                baseline_.residentMaterialTextureSets + 11 ||
            resident.residentMaterialTextureReferences !=
                baseline_.residentMaterialTextureReferences + 17 ||
            resident.residentEnvironmentLightingSets !=
                baseline_.residentEnvironmentLightingSets + 1 ||
            resident.totalMaterialTextureDecodedBytes !=
                baseline_.totalMaterialTextureDecodedBytes ||
            resident.materialDerivedDataCacheHits !=
                baseline_.materialDerivedDataCacheHits ||
            resident.materialDerivedDataCacheMisses !=
                baseline_.materialDerivedDataCacheMisses ||
            resident.materialDerivedDataCacheWrites !=
                baseline_.materialDerivedDataCacheWrites ||
            resident.totalMaterialTextureCompressedBytes !=
                baseline_.totalMaterialTextureCompressedBytes +
                    expectedNativePayloadBytes_ ||
            !resident.environmentLightingActive ||
            !createReceipt_.rendererAccepted || !setReceipt_.rendererAccepted ||
            createReceipt_.sequence == 0 ||
            setReceipt_.sequence != createReceipt_.sequence + 1)
            return Fail(engine, 12, "resident exact-set or IBL receipts changed");
        ready_ = true;
    }

    void OnUpdate(Engine &engine, float) override {
        if (Result || !ready_ || completed_) {
            engine.Quit();
            return;
        }
        if (!rendered_)
            return;
        const auto submitted = engine.GetRendererStats();
        if (submitted.lastSubmittedMeshInstances < 6 ||
            submitted.lastSubmittedTexturedMeshInstances < 6) {
            Fail(engine, 13, "cooked GPU handles did not reach a textured draw");
            return;
        }
        engine.Draw().Clear();
        scene_.Clear();
        for (auto iterator = entities_.rbegin(); iterator != entities_.rend(); ++iterator)
            if (!registry_.Destroy(*iterator))
                return Fail(engine, 14, "reverse entity release failed");
        entities_.clear();
        for (auto iterator = leases_.rbegin(); iterator != leases_.rend(); ++iterator)
            iterator->Reset();
        leases_.clear();
        for (auto iterator = manifest_.MeshParts().rbegin();
             iterator != manifest_.MeshParts().rend(); ++iterator)
            if (!meshes_.Unload(iterator->id))
                return Fail(engine, 15,
                            "reverse mesh release failed: " + iterator->id +
                                " contains=" +
                                std::to_string(meshes_.Contains(iterator->id)) +
                                " levels=" +
                                std::to_string(meshes_.LevelCount(iterator->id)) +
                                " borrowers=" +
                                std::to_string(meshes_.Stats().assetBorrowers));
        if (!meshes_.Shutdown())
            return Fail(engine, 16, "mesh library shutdown failed");
        for (auto iterator = manifest_.Materials().rbegin();
             iterator != manifest_.Materials().rend(); ++iterator)
            if (!materials_.Unload(iterator->id))
                return Fail(engine, 17, "reverse material release failed");
        materials_.Shutdown();

        if (!engine.ClearEnvironmentLighting3D(&clearReceipt_))
            return Fail(engine, 18, "IBL deactivate failed");
        if (!engine.DestroyEnvironmentLighting3D(environment_, environmentHandle_, &destroyReceipt_))
            return Fail(engine, 18, "IBL destroy ownership failed");
        environment_.Destroy(environmentHandle_);
        environmentHandle_ = {};
        environment_.Trim();
        engine.WaitRendererIdle3D();
        engine.TrimEnvironmentLighting3D(&trimReceipt_);
        engine.TrimUnusedMemory();
        engine.WaitRendererIdle3D();
        const auto released = engine.GetRendererStats();
        if (released.residentMeshes != baseline_.residentMeshes ||
            released.residentMeshReferences != baseline_.residentMeshReferences ||
            released.residentMeshBytes != baseline_.residentMeshBytes ||
            released.residentMaterialTextureSets !=
                baseline_.residentMaterialTextureSets ||
            released.residentMaterialTextureReferences !=
                baseline_.residentMaterialTextureReferences ||
            released.residentTextureBytes != baseline_.residentTextureBytes ||
            released.residentTextureAllocationBytes !=
                baseline_.residentTextureAllocationBytes ||
            released.residentEnvironmentLightingSets !=
                baseline_.residentEnvironmentLightingSets ||
            released.residentEnvironmentLightingBytes !=
                baseline_.residentEnvironmentLightingBytes ||
            released.residentEnvironmentLightingDescriptors !=
                baseline_.residentEnvironmentLightingDescriptors ||
            released.pendingGpuReleaseBytes ||
            released.pendingEnvironmentLightingReleaseBytes ||
            released.environmentLightingActive ||
            !clearReceipt_.rendererAccepted ||
            destroyReceipt_.sequence != clearReceipt_.sequence + 1 ||
            trimReceipt_.sequence != destroyReceipt_.sequence + 1)
            return Fail(engine, 19, "reverse GPU release did not return to baseline");
        completed_ = true;
        ready_ = false;
        engine.Quit();
    }

    void OnRender(Engine &engine, float) override {
        if (!ready_ || completed_)
            return;
        scene_.Clear();
        const auto collected = registry_.Collect(scene_, engine.Draw());
        Camera3D camera{};
        camera.position = {0.f, 5.f, -10.f};
        camera.target = {0.f, 1.f, 12.f};
        scene_.Submit(engine.Draw(), camera,
                      {float(engine.Width()), float(engine.Height())});
        if (collected.submittedLodRenderables == 6)
            ++rendered_;
    }

    void OnStop(Engine &) override {
        for (auto &lease : leases_)
            lease.Reset();
        registry_.Clear();
        meshes_.Shutdown();
        materials_.Shutdown();
        if (environmentHandle_)
            environment_.Destroy(environmentHandle_);
        environment_.Shutdown();
        if (completed_)
            std::cout << "payloads=54 meshes=18 materials=11 ibl=3 draw=6 release=baseline\n";
    }

private:
    static void Fail(Engine &engine, int code, const std::string &message) {
        if (!Result) {
            Result = code;
            std::cerr << message << '\n';
        }
        engine.Quit();
    }

    RendererStats baseline_{};
    CookedRenderManifest3D manifest_{};
    MaterialLibrary3D materials_{};
    StaticMeshAssetLibrary3D meshes_{};
    SceneECSRegistry3D registry_{};
    EnvironmentLightingStore3D environment_{};
    EnvironmentLightingHandle3D environmentHandle_{};
    EnvironmentLightingGpuReceipt3D createReceipt_{}, setReceipt_{},
        clearReceipt_{}, destroyReceipt_{}, trimReceipt_{};
    std::vector<StaticMeshRenderableLease3D> leases_{};
    std::vector<SceneEntity3D> entities_{};
    Scene3D scene_{};
    std::size_t rendered_{};
    std::uint64_t expectedNativePayloadBytes_{};
    bool ready_{}, completed_{};
};
} // namespace

int main() {
    try {
        Engine engine({L"Aevrynd Photoreal Cooked D3D12 Test", 640, 360,
                       1.f / 60.f, false, false, false});
        engine.Run(std::make_unique<PhotorealCookedGame>(),
                   GetModuleHandleW(nullptr), SW_HIDE);
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 20;
    }
    return Result;
}
