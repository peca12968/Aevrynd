#include <Aevrynd/CookedRenderManifest3D.h>
#include <Aevrynd/Engine.h>

#include <Windows.h>

#include <iostream>
#include <memory>

using namespace Aevrynd;

namespace {
int Result{};
class TestGame final : public IGame {
public:
    void OnStart(Engine &engine) override {
        const auto baseline = engine.GetRendererStats();
        CookedRenderManifest3D manifest;
        CookedRenderManifestError3D error;
        if (!manifest.LoadFile(
                std::filesystem::path{AEVRYND_SOURCE_DIR} /
                    "engine/assets/realistic-cc0-first-batch/cooked/runtime-render-manifest.json",
                &error))
            return Fail(engine, 1, error.message);
        const auto &ibl = manifest.Ibl();
        const auto handle = store_.Create(
            {manifest.AssetRoot() / ibl.specularPath,
             manifest.AssetRoot() / ibl.irradiancePath,
             manifest.AssetRoot() / ibl.brdfLutPath, .25f});
        const auto foreignHandle = foreignStore_.Create(
            {manifest.AssetRoot() / ibl.specularPath,
             manifest.AssetRoot() / ibl.irradiancePath,
             manifest.AssetRoot() / ibl.brdfLutPath, .25f});
        EnvironmentLightingGpuReceipt3D create, set, clear, destroy, trim;
        if (!handle || !engine.CreateEnvironmentLighting3D(store_, handle, &create) ||
            !engine.SetEnvironmentLighting3D(store_, handle, &set) ||
            engine.ActiveEnvironmentLighting3D() != handle)
            return Fail(engine, 2, "Engine IBL create/set wrapper failed");
        EnvironmentLightingGpuReceipt3D foreignSet, foreignDestroy;
        if (!foreignHandle || foreignHandle.value != handle.value ||
            engine.SetEnvironmentLighting3D(foreignStore_, foreignHandle,
                                            &foreignSet) ||
            engine.DestroyEnvironmentLighting3D(foreignStore_, foreignHandle,
                                                &foreignDestroy) ||
            foreignSet.rendererAccepted || foreignDestroy.rendererAccepted ||
            engine.ActiveEnvironmentLighting3D() != handle)
            return Fail(engine, 2, "foreign-store handle passed Engine ownership");
        foreignStore_.Destroy(foreignHandle);
        const auto upload = engine.SubmitPendingUploads3D();
        engine.WaitRendererIdle3D();
        const auto resident = engine.GetRendererStats();
        // IBL 建立目前同步等待自身 upload；空 receipt 仍依契約代表已完成。
        if (!engine.IsUploadComplete3D(upload) ||
            resident.residentEnvironmentLightingSets !=
                baseline.residentEnvironmentLightingSets + 1 ||
            !resident.environmentLightingActive || !create.rendererAccepted ||
            !set.rendererAccepted || set.sequence != create.sequence + 1 ||
            !set.activeAfter)
            return Fail(engine, 3, "Engine IBL upload receipt failed");
        if (!engine.ClearEnvironmentLighting3D(&clear))
            return Fail(engine, 4, "Engine IBL clear wrapper failed");
        if (!engine.DestroyEnvironmentLighting3D(store_, handle, &destroy))
            return Fail(engine, 5, "Engine IBL destroy ownership failed");
        store_.Destroy(handle);
        store_.Trim();
        engine.WaitRendererIdle3D();
        engine.TrimEnvironmentLighting3D(&trim);
        engine.TrimUnusedMemory();
        engine.WaitRendererIdle3D();
        const auto released = engine.GetRendererStats();
        if (released.residentEnvironmentLightingSets !=
                baseline.residentEnvironmentLightingSets ||
            released.residentEnvironmentLightingBytes !=
                baseline.residentEnvironmentLightingBytes ||
            released.residentEnvironmentLightingDescriptors !=
                baseline.residentEnvironmentLightingDescriptors ||
            released.pendingEnvironmentLightingReleaseBytes ||
            released.environmentLightingActive || !clear.rendererAccepted ||
            clear.activeAfter || !destroy.rendererAccepted ||
            destroy.sequence != clear.sequence + 1 ||
            trim.sequence != destroy.sequence + 1)
            return Fail(engine, 5, "Engine IBL reverse release receipt failed");
        passed_ = true;
        engine.Quit();
    }
    void OnUpdate(Engine &, float) override {}
    void OnRender(Engine &, float) override {}
    void OnStop(Engine &) override {
        store_.Shutdown();
        foreignStore_.Shutdown();
        if (passed_)
            std::cout << "ibl=3 wrappers=create,set,clear,destroy,trim release=baseline\n";
    }

private:
    static void Fail(Engine &engine, int code, const std::string &message) {
        Result = code;
        std::cerr << message << '\n';
        engine.Quit();
    }
    EnvironmentLightingStore3D store_{};
    EnvironmentLightingStore3D foreignStore_{};
    bool passed_{};
};
} // namespace

int main() {
    try {
        Engine engine({L"Aevrynd Engine IBL Wrapper Test", 320, 180,
                       1.f / 60.f, false, false, false});
        engine.Run(std::make_unique<TestGame>(), GetModuleHandleW(nullptr),
                   SW_HIDE);
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 6;
    }
    return Result;
}
