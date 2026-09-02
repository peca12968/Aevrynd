#include <Aevrynd/PhotorealMaterialImport3D.h>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <bcrypt.h>

#include <array>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <set>
#include <string>
#include <vector>

#pragma comment(lib, "bcrypt.lib")

namespace {
bool Expect(bool condition, const char* message) { if (!condition) std::cerr << message << '\n'; return condition; }

std::string Sha256(std::string_view text) {
    BCRYPT_ALG_HANDLE algorithm{}; BCRYPT_HASH_HANDLE hash{}; DWORD objectBytes{}, copied{};
    std::vector<UCHAR> object; std::array<UCHAR, 32> digest{};
    if (BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0) < 0) return {};
    if (BCryptGetProperty(algorithm, BCRYPT_OBJECT_LENGTH, reinterpret_cast<PUCHAR>(&objectBytes), sizeof(objectBytes), &copied, 0) < 0) { BCryptCloseAlgorithmProvider(algorithm, 0); return {}; }
    object.resize(objectBytes);
    const bool ok = BCryptCreateHash(algorithm, &hash, object.data(), objectBytes, nullptr, 0, 0) >= 0 &&
                    BCryptHashData(hash, reinterpret_cast<PUCHAR>(const_cast<char*>(text.data())), static_cast<ULONG>(text.size()), 0) >= 0 &&
                    BCryptFinishHash(hash, digest.data(), static_cast<ULONG>(digest.size()), 0) >= 0;
    if (hash) BCryptDestroyHash(hash); BCryptCloseAlgorithmProvider(algorithm, 0); if (!ok) return {};
    constexpr char hex[] = "0123456789abcdef"; std::string result(64, '0');
    for (std::size_t i = 0; i < digest.size(); ++i) { result[i * 2] = hex[digest[i] >> 4]; result[i * 2 + 1] = hex[digest[i] & 15]; }
    return result;
}

std::string ReadText(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary); return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}
bool WriteText(const std::filesystem::path& path, std::string_view text) {
    std::ofstream output(path, std::ios::binary | std::ios::trunc); output.write(text.data(), static_cast<std::streamsize>(text.size())); return output.good();
}
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
    if (!error.message.empty()) std::cerr << "tree import: " << error.message << '\n';
    passed &= Expect(treePlan.manifest.entries.size() == 12 && treePlan.textureBakes.size() == 9 && treePlan.cookPlan.materials.size() == 3 && treePlan.cookPlan.uniqueTextureSets == 3, "three-slot tree topology mismatch");
    if (treePlan.manifest.entries.size() == 12) {
        std::set<std::string> materialIds;
        for (const auto& entry : treePlan.manifest.entries) if (entry.kind == AssetKind::Material) materialIds.insert(entry.id);
        passed &= Expect(materialIds == std::set<std::string>{"island_tree_01-branches-material", "island_tree_01-leaves-material", "island_tree_01-trunk-material"},
                         "three-slot tree exact material mismatch");
    }

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

    const auto schemaRoot = std::filesystem::temp_directory_path() / ("aevrynd-photoreal-material-schema-" + std::to_string(GetCurrentProcessId()) + "-" + std::to_string(GetTickCount64()));
    std::error_code schemaIo; const bool schemaCreated = std::filesystem::create_directories(schemaRoot / "textures", schemaIo);
    if (schemaIo || !schemaCreated) return 101;
    for (const char* texture : {"boulder_01_diff_1k.jpg", "boulder_01_nor_gl_1k.jpg", "boulder_01_arm_1k.jpg"})
        if (!std::filesystem::copy_file(root / "boulder-01/textures" / texture, schemaRoot / "textures" / texture, std::filesystem::copy_options::none, schemaIo) || schemaIo) return 102;
    const std::string runtimeTemplate = ReadText(root / "runtime-asset-manifest.json");
    const std::string originalBoulder = ReadText(sidecarPath);
    const std::string originalBoulderHash = Sha256(originalBoulder);
    if (runtimeTemplate.empty() || originalBoulder.empty() || originalBoulderHash.empty()) return 103;
    auto rawSchemaCase = [&](const std::string& sidecar, bool expected, const char* expectedId, const char* message) {
        std::string receipt = runtimeTemplate; const std::string variantHash = Sha256(sidecar); const auto at = receipt.find(originalBoulderHash);
        if (at == std::string::npos || variantHash.empty()) return Expect(false, "schema fixture receipt construction failed");
        receipt.replace(at, originalBoulderHash.size(), variantHash);
        if (!WriteText(schemaRoot / "fixture.materials.json", sidecar) || !WriteText(schemaRoot / "runtime.json", receipt)) return Expect(false, "schema fixture write failed");
        PhotorealRuntimeManifest3D fixtureRuntime; PhotorealAssetError3D fixtureRuntimeError;
        PhotorealMaterialImportPlan3D fixturePlan; PhotorealMaterialImportError3D fixtureError;
        const bool admitted = fixtureRuntime.LoadFile(schemaRoot / "runtime.json", &fixtureRuntimeError) && fixtureRuntime.Passed() &&
                              PhotorealMaterialImport3D::BuildPlan(fixtureRuntime, "boulder_01", schemaRoot / "fixture.materials.json", fixturePlan, &fixtureError);
        bool result = admitted == expected;
        if (result && admitted && expectedId) result = std::any_of(fixturePlan.manifest.entries.begin(), fixturePlan.manifest.entries.end(), [&](const auto& entry) { return entry.kind == AssetKind::Material && entry.id == expectedId; });
        return Expect(result, message);
    };
    auto schemaCase = [&](std::string_view header, std::string_view materialPrefix, std::string_view materialFields, bool expected, const char* expectedId, const char* message) {
        const std::string effectiveHeader = header.empty() ? "\"schemaVersion\":1,\"binding\":\"runtime-sidecar\",\"outputResolution\":1024,\"expectedMipCount\":11" : std::string(header);
        const std::string sidecar = "{" + effectiveHeader + ",\"materials\":[{" + std::string(materialPrefix) + "\"baseColor\":\"textures/boulder_01_diff_1k.jpg\",\"normalGl\":\"textures/boulder_01_nor_gl_1k.jpg\",\"packedArm\":\"textures/boulder_01_arm_1k.jpg\"" + std::string(materialFields) + "}]}";
        return rawSchemaCase(sidecar, expected, expectedId, message);
    };
    auto check = [&](std::string_view fields, bool expected, const char* message) { return schemaCase("", "\"slot\":0,", fields, expected, nullptr, message); };
    passed &= check("", true, "legacy opaque material rejected");
    passed &= schemaCase("\"schemaVersion\":1,\"outputResolution\":1024,\"expectedMipCount\":11", "\"slot\":0,", "", true, nullptr, "optional binding rejected");
    passed &= schemaCase("\"schemaVersion\":1,\"binding\":\"runtime-sidecar\",\"outputResolution\":1024,\"expectedMipCount\":11", "\"slot\":0,", "", true, nullptr, "runtime-sidecar binding rejected");
    passed &= schemaCase("\"schemaVersion\":1,\"binding\":null,\"outputResolution\":1024,\"expectedMipCount\":11", "\"slot\":0,", "", false, nullptr, "null binding passed");
    passed &= check(",\"alphaMode\":\"mask\",\"alphaSource\":\"embedded\",\"alphaCutoff\":0.5,\"alphaDilationPixels\":4,\"doubleSided\":true,\"materialClass\":\"foliage\"", true, "embedded MASK material rejected");
    passed &= check(",\"alphaMode\":\"mask\",\"alphaSource\":\"embedded\",\"alphaBlackKeyThreshold\":0.05,\"alphaCutoff\":0.5,\"alphaDilationPixels\":4", false, "embedded threshold passed");
    passed &= check(",\"alphaMode\":\"mask\",\"alphaSource\":\"black-key\",\"alphaBlackKeyThreshold\":0,\"alphaCutoff\":0.5,\"alphaDilationPixels\":4", false, "zero black-key threshold passed");
    passed &= check(",\"alphaMode\":\"mask\",\"alphaSource\":\"black-key\",\"alphaBlackKeyThreshold\":0.251,\"alphaCutoff\":0.5,\"alphaDilationPixels\":4", false, "oversized black-key threshold passed");
    passed &= check(",\"alphaMode\":\"mask\",\"alphaSource\":\"embedded\",\"alphaCutoff\":0.4,\"alphaDilationPixels\":4", false, "noncanonical alpha cutoff passed");
    passed &= check(",\"alphaMode\":\"mask\",\"alphaSource\":\"embedded\",\"alphaCutoff\":0.5,\"alphaDilationPixels\":33", false, "oversized alpha dilation passed");
    passed &= check(",\"alphaMode\":\"mask\",\"alphaSource\":\"embedded\",\"alphaCutoff\":0.5,\"alphaDilationPixels\":1.5", false, "fractional alpha dilation passed");
    passed &= check(",\"materialClass\":\"foliage\",\"doubleSided\":true", false, "opaque foliage passed");
    passed &= check(",\"alphaMode\":\"mask\",\"alphaSource\":\"embedded\",\"alphaCutoff\":0.5,\"alphaDilationPixels\":4,\"materialClass\":\"foliage\",\"doubleSided\":false", false, "single-sided foliage passed");
    passed &= check(",\"alphaMode\":\"mask\",\"alphaSource\":\"black-key\",\"alphaBlackKeyThreshold\":1e9999,\"alphaCutoff\":0.5,\"alphaDilationPixels\":4", false, "non-finite threshold passed");
    passed &= check(",\"alphaMode\":\"mask\",\"alphaSource\":\"black-key\",\"alphaBlackKeyThreshold\":\"0.05\",\"alphaCutoff\":0.5,\"alphaDilationPixels\":4", false, "wrong-type threshold passed");
    passed &= check(",\"unknown\":0", false, "unknown material field passed");
    passed &= schemaCase("", "", "", true, "boulder_01-slot-0-material", "missing slot fallback rejected");
    passed &= schemaCase("", "\"slot\":18446744073709551615,", "", true, "boulder_01-slot-18446744073709551615-material", "UINT64_MAX slot rejected");
    for (const char* slot : {"01", "-01", "1.5", "1e0", "9007199254740993.0", "18446744073709551616"})
        passed &= schemaCase("", std::string{"\"slot\":"} + slot + ",", "", false, nullptr, "invalid slot passed");
    passed &= schemaCase("\"schemaVersion\":1,\"outputResolution\":1024,\"expectedMipCount\":10", "\"slot\":0,", "", false, nullptr, "wrong mip count passed");
    passed &= schemaCase("\"schemaVersion\":1,\"expectedMipCount\":11", "\"slot\":0,", "", false, nullptr, "missing output resolution passed");
    passed &= schemaCase("\"outputResolution\":1024,\"expectedMipCount\":11", "\"slot\":0,", "", false, nullptr, "missing schema version passed");
    passed &= schemaCase("\"schemaVersion\":1,\"outputResolution\":0,\"expectedMipCount\":1", "\"slot\":0,", "", false, nullptr, "invalid output resolution passed");
    passed &= schemaCase("", "\"slot\":0,", ",\"ambientOcclusion\":\"textures/boulder_01_arm_1k.jpg\",\"roughness\":\"textures/boulder_01_arm_1k.jpg\",\"metallicScalar\":-0.1", false, nullptr, "negative metallic scalar passed");
    const std::string fixtureHeader = "{\"schemaVersion\":1,\"outputResolution\":1024,\"expectedMipCount\":11,\"materials\":[";
    const std::string fixtureChannels = "\"baseColor\":\"textures/boulder_01_diff_1k.jpg\",\"normalGl\":\"textures/boulder_01_nor_gl_1k.jpg\",\"packedArm\":\"textures/boulder_01_arm_1k.jpg\"";
    passed &= rawSchemaCase(fixtureHeader + "{\"name\":\"Named\",\"slot\":false," + fixtureChannels + "}]}", false, nullptr, "named boolean slot passed");
    passed &= rawSchemaCase(fixtureHeader + "{\"name\":\"Foo\"," + fixtureChannels + "},{\"name\":\"foo\"," + fixtureChannels + "}]}", false, nullptr, "case-insensitive duplicate name passed");
    passed &= rawSchemaCase(fixtureHeader + "{\"slot\":0," + fixtureChannels + ",\"ambientOcclusion\":\"textures/boulder_01_arm_1k.jpg\"}]}", false, nullptr, "partial separate channels with packed ARM passed");
    passed &= rawSchemaCase(fixtureHeader + "{\"slot\":0,\"baseColor\":\"textures/boulder_01_diff_1k.jpg\",\"normalGl\":false,\"normal\":\"textures/boulder_01_nor_gl_1k.jpg\",\"packedArm\":\"textures/boulder_01_arm_1k.jpg\"}]}", false, nullptr, "wrong-type normalGl alias fallback passed");
    passed &= rawSchemaCase(fixtureHeader + "{\"slot\":0,\"baseColor\":\"textures/boulder_01_diff_1k.jpg\",\"normalGl\":\"textures/boulder_01_nor_gl_1k.jpg\",\"packedArm\":false,\"arm\":\"textures/boulder_01_arm_1k.jpg\"}]}", false, nullptr, "wrong-type packedArm alias fallback passed");
    const std::string separateChannels = "\"ambientOcclusion\":\"textures/boulder_01_arm_1k.jpg\",\"roughness\":\"textures/boulder_01_arm_1k.jpg\",\"metallicScalar\":0";
    passed &= rawSchemaCase(fixtureHeader + "{\"slot\":0,\"baseColor\":\"textures/boulder_01_diff_1k.jpg\",\"normalGl\":\"textures/boulder_01_nor_gl_1k.jpg\",\"packedArm\":false," + separateChannels + "}]}", false, nullptr, "wrong-type packedArm with complete separate channels passed");
    passed &= rawSchemaCase(fixtureHeader + "{\"slot\":0,\"baseColor\":\"textures/boulder_01_diff_1k.jpg\",\"normalGl\":\"textures/boulder_01_nor_gl_1k.jpg\",\"arm\":\"\"," + separateChannels + "}]}", false, nullptr, "empty ARM alias with complete separate channels passed");
    passed &= check(",\"alphaMode\":\"mask\",\"alphaSource\":\"embedded\",\"alphaCutoff\":0.5,\"alphaDilationPixels\":4.0", false, "decimal alpha dilation passed");
    passed &= check(",\"alphaMode\":\"mask\",\"alphaSource\":\"embedded\",\"alphaCutoff\":0.5,\"alphaDilationPixels\":4e0", false, "exponent alpha dilation passed");
    passed &= schemaCase("\"schemaVersion\":1.0,\"outputResolution\":1024,\"expectedMipCount\":11", "\"slot\":0,", "", false, nullptr, "decimal schema version passed");
    passed &= schemaCase("\"schemaVersion\":true,\"outputResolution\":1024,\"expectedMipCount\":11", "\"slot\":0,", "", false, nullptr, "boolean schema version passed");
    std::filesystem::remove_all(schemaRoot, schemaIo);

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
