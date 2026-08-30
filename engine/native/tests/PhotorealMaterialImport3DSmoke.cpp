#include <Aevrynd/PhotorealMaterialImport3D.h>

#include <array>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

namespace {
bool Expect(bool condition, const char* message) { if (!condition) std::cerr << message << '\n'; return condition; }
}

int main(int argc, char** argv) {
    using namespace Aevrynd;
    if (argc != 2) return 100;
    const std::filesystem::path root = std::filesystem::absolute(argv[1]);
    PhotorealRuntimeManifest3D runtime;
    PhotorealAssetError3D runtimeError;
    bool passed = Expect(runtime.LoadFile(root / "runtime-asset-manifest.json", &runtimeError) && runtime.Passed(), "production runtime manifest failed");
    const auto sidecarPath = root / "boulder-01/boulder-01.materials.json";
    PhotorealMaterialImportPlan3D plan;
    PhotorealMaterialImportError3D error;
    passed &= Expect(PhotorealMaterialImport3D::BuildPlan(runtime, "boulder_01", sidecarPath, plan, &error), "production boulder sidecar failed");
    if (!error.message.empty()) std::cerr << error.message << '\n';
    passed &= Expect(plan.manifest.entries.size() == 4 && plan.textureBakes.size() == 3 && plan.cookPlan.materials.size() == 1, "material plan topology mismatch");
    if (plan.textureBakes.size() == 3 && plan.cookPlan.materials.size() == 1) {
        passed &= Expect(plan.textureBakes[0].blockFormat == "BC7_UNORM_SRGB" && plan.textureBakes[1].blockFormat == "BC5_UNORM" && plan.textureBakes[2].usage == TextureUsage::PackedOrm, "BC or packed ARM policy mismatch");
        passed &= Expect(plan.textureBakes[1].operation == PhotorealTextureBakeOperation3D::InvertNormalGreenToDirectX && plan.cookPlan.materials[0].textures.surfaceEncoding == MaterialSurfaceMapEncoding3D::PackedOrm, "normal or ORM adapter mismatch");
    }
    passed &= Expect(plan.cookPlan.maximumResolution == 4096 && plan.cookPlan.estimatedResidentBytes > 0 && plan.verifiedSourceBytes > 0, "Ultra cook receipt mismatch");

    const auto treeSidecarPath = root / "island-tree-01/island-tree-01.materials.json";
    PhotorealMaterialImportPlan3D treePlan;
    passed &= Expect(PhotorealMaterialImport3D::BuildPlan(runtime, "island_tree_01", treeSidecarPath, treePlan, &error), "three-slot tree sidecar failed");
    passed &= Expect(treePlan.manifest.entries.size() == 12 && treePlan.textureBakes.size() == 9 && treePlan.cookPlan.materials.size() == 3 && treePlan.cookPlan.uniqueTextureSets == 3, "three-slot tree topology mismatch");

    PhotorealMaterialImportPlan3D groundPlan;
    passed &= Expect(PhotorealMaterialImport3D::BuildPlan(runtime, "Ground108", root / "ground108/ground108.materials.json", groundPlan, &error), "separate AO/roughness ground sidecar failed");
    passed &= Expect(groundPlan.textureBakes.size() == 3 && groundPlan.cookPlan.materials.size() == 1 &&
        groundPlan.textureBakes[2].operation == PhotorealTextureBakeOperation3D::PackAmbientOcclusionRoughnessMetallicScalar &&
        !groundPlan.textureBakes[2].secondarySourcePath.empty() && groundPlan.textureBakes[2].secondarySourceSha256.size() == 64 &&
        groundPlan.textureBakes[2].metallicScalar == 0.f, "separate AO/roughness/metallic scalar cook request mismatch");
    const std::array<std::pair<const char*, const char*>, 9> exactMaterials{{
        {"forest_ground_04", "forest-ground-04/forest-ground-04.materials.json"}, {"Ground108", "ground108/ground108.materials.json"},
        {"coastal_cliff_01", "coastal-cliff-01/coastal-cliff-01.materials.json"}, {"boulder_01", "boulder-01/boulder-01.materials.json"},
        {"island_tree_01", "island-tree-01/island-tree-01.materials.json"}, {"dead_tree_trunk_02", "dead-tree-trunk-02/dead-tree-trunk-02.materials.json"},
        {"RoofingTiles014B", "roofing-tiles014-b/roofing-tiles014-b.materials.json"}, {"Bricks100", "bricks100/bricks100.materials.json"},
        {"stony_dirt_path", "stony-dirt-path/stony-dirt-path.materials.json"}}};
    for (const auto& [id, relative] : exactMaterials) {
        PhotorealMaterialImportPlan3D exactPlan;
        passed &= Expect(PhotorealMaterialImport3D::BuildPlan(runtime, id, root / relative, exactPlan, &error) &&
                         !exactPlan.cookPlan.materials.empty() && exactPlan.textureBakes.size() == exactPlan.cookPlan.materials.size() * 3,
                         "exact material import/PBR cook admission failed");
    }

    PhotorealMaterialImportPlan3D sentinel; sentinel.assetId = "sentinel";
    passed &= Expect(!PhotorealMaterialImport3D::BuildPlan(runtime, "missing", sidecarPath, sentinel, &error) && error.code == PhotorealMaterialImportErrorCode3D::AssetNotFound && sentinel.assetId == "sentinel", "missing runtime asset passed");
    const auto hostileRoot = std::filesystem::temp_directory_path() / "aevrynd-photoreal-material-hostile";
    std::error_code ignored;
    std::filesystem::remove_all(hostileRoot, ignored);
    std::filesystem::copy(sidecarPath.parent_path(), hostileRoot, std::filesystem::copy_options::recursive, ignored);
    if (ignored) passed = false;
    else {
        std::fstream changed(hostileRoot / "textures/boulder_01_diff_1k.jpg", std::ios::in | std::ios::out | std::ios::binary);
        char byte{}; changed.read(&byte, 1); byte ^= 1; changed.seekp(0); changed.write(&byte, 1); changed.close();
        passed &= Expect(!PhotorealMaterialImport3D::BuildPlan(runtime, "boulder_01", hostileRoot / "boulder-01.materials.json", sentinel, &error) && error.code == PhotorealMaterialImportErrorCode3D::FileHashMismatch, "changed disk texture passed runtime hash gate");
        std::ofstream(hostileRoot / "boulder-01.materials.json", std::ios::binary | std::ios::trunc) << "{";
        passed &= Expect(!PhotorealMaterialImport3D::BuildPlan(runtime, "boulder_01", hostileRoot / "boulder-01.materials.json", sentinel, &error) && error.code == PhotorealMaterialImportErrorCode3D::FileHashMismatch, "changed sidecar passed runtime hash gate");
    }
    std::filesystem::remove_all(hostileRoot, ignored);
    if (passed) std::cout << "exactAssets=9 materials=" << plan.cookPlan.materials.size() << " bakes=" << plan.textureBakes.size() << " bytes=" << plan.cookPlan.estimatedResidentBytes << " quality=Ultra\n";
    return passed ? 0 : 1;
}
