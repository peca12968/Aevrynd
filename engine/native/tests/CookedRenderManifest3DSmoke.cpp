#include <Aevrynd/CookedRenderManifest3D.h>

#include <Windows.h>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

using namespace Aevrynd;

namespace {
constexpr auto FixtureSha =
    "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad";
constexpr auto ProductionManifestSha =
    "3aabe78ae0b6b003148ed7568f40abe11970f3052a04653e57b0f244a0c42bad";

bool WriteFixture(const std::filesystem::path &root, std::string_view path) {
    const auto absolute = root / path;
    std::filesystem::create_directories(absolute.parent_path());
    std::ofstream output(absolute, std::ios::binary | std::ios::trunc);
    output.write("abc", 3);
    return bool(output);
}

bool WriteText(const std::filesystem::path &path, std::string_view text) {
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    output.write(text.data(), static_cast<std::streamsize>(text.size()));
    return bool(output);
}

std::string FixtureJson() {
    return std::string{"{\"schemaVersion\":1,\"futureMetadata\":{\"accepted\":true},\"files\":["}
        + "{\"path\":\"materials/base.dds\",\"bytes\":3,\"sha256\":\"" + FixtureSha + "\",\"format\":\"future\"},"
        + "{\"path\":\"materials/surface.dds\",\"bytes\":3,\"sha256\":\"" + FixtureSha + "\"},"
        + "{\"path\":\"materials/normal.dds\",\"bytes\":3,\"sha256\":\"" + FixtureSha + "\"},"
        + "{\"path\":\"meshes/lod0.aemesh\",\"bytes\":3,\"sha256\":\"" + FixtureSha + "\"},"
        + "{\"path\":\"meshes/lod1.aemesh\",\"bytes\":3,\"sha256\":\"" + FixtureSha + "\"},"
        + "{\"path\":\"meshes/lod2.aemesh\",\"bytes\":3,\"sha256\":\"" + FixtureSha + "\"},"
        + "{\"path\":\"ibl/specular.dds\",\"bytes\":3,\"sha256\":\"" + FixtureSha + "\"},"
        + "{\"path\":\"ibl/irradiance.dds\",\"bytes\":3,\"sha256\":\"" + FixtureSha + "\"},"
        + "{\"path\":\"ibl/brdf.dds\",\"bytes\":3,\"sha256\":\"" + FixtureSha + "\"}],"
        + "\"materials\":[{\"id\":\"fixture:slot-0\",\"assetId\":\"fixture\",\"slot\":\"slot-0\",\"baseColorPath\":\"materials/base.dds\",\"surfacePath\":\"materials/surface.dds\",\"normalPath\":\"materials/normal.dds\",\"surfaceEncoding\":\"packed-orm\",\"alphaMode\":\"mask\",\"alphaCutoff\":0.37,\"doubleSided\":true,\"materialClass\":\"foliage\"}],"
        + "\"meshParts\":[{\"id\":\"fixture:part-0\",\"assetId\":\"fixture\",\"materialId\":\"fixture:slot-0\",\"lodPaths\":[\"meshes/lod0.aemesh\",\"meshes/lod1.aemesh\",\"meshes/lod2.aemesh\"]}],"
        + "\"ibl\":{\"specularPath\":\"ibl/specular.dds\",\"irradiancePath\":\"ibl/irradiance.dds\",\"brdfLutPath\":\"ibl/brdf.dds\",\"futureRotation\":0}}";
}

bool ReplaceOnce(std::string &text, std::string_view from,
                 std::string_view to) {
    const auto position = text.find(from);
    if (position == std::string::npos)
        return false;
    text.replace(position, from.size(), to);
    return true;
}

bool Expect(bool condition, const char *message) {
    if (!condition)
        std::cerr << message << '\n';
    return condition;
}

bool MatchesFixtureReceipt(const CookedRenderManifest3D &manifest) {
    const auto &stats = manifest.Stats();
    return stats.files == 9 && stats.materials == 1 && stats.meshParts == 1 &&
           stats.referencedFiles == 9 && stats.validatedFiles == 9 &&
           stats.validatedMaterials == 1 && stats.validatedMeshParts == 1 &&
           stats.validatedReferencedFiles == 9 &&
           manifest.FindFile("materials/base.dds") &&
           manifest.FindMaterial("fixture:slot-0") &&
           manifest.FindMeshPart("fixture:part-0");
}

bool CreateDirectoryJunction(const std::filesystem::path &link,
                             const std::filesystem::path &target) {
    std::wstring command = L"cmd.exe /d /c mklink /J \"" + link.wstring() +
                           L"\" \"" + target.wstring() + L"\" >nul";
    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION process{};
    if (!CreateProcessW(nullptr, command.data(), nullptr, nullptr, FALSE,
                        CREATE_NO_WINDOW, nullptr, nullptr, &startup, &process))
        return false;
    WaitForSingleObject(process.hProcess, INFINITE);
    DWORD exitCode{};
    const bool passed = GetExitCodeProcess(process.hProcess, &exitCode) &&
                        exitCode == 0;
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    return passed;
}
} // namespace

int main() {
    bool passed = true;
    const auto root = std::filesystem::temp_directory_path() /
                      ("aevrynd-render-manifest-" +
                       std::to_string(GetCurrentProcessId()));
    std::error_code filesystemError;
    std::filesystem::remove_all(root, filesystemError);
    const char *paths[] = {"materials/base.dds", "materials/surface.dds",
                           "materials/normal.dds", "meshes/lod0.aemesh",
                           "meshes/lod1.aemesh", "meshes/lod2.aemesh",
                           "ibl/specular.dds", "ibl/irradiance.dds",
                           "ibl/brdf.dds"};
    for (const char *path : paths)
        passed &= Expect(WriteFixture(root, path), "fixture write failed");

    CookedRenderManifest3D manifest;
    CookedRenderManifestError3D error;
    const std::string valid = FixtureJson();
    passed &= Expect(manifest.LoadJson(valid, root, &error),
                     "bounded synthetic manifest was rejected");
    passed &= Expect(manifest.Stats().files == 9 &&
                         manifest.Stats().materials == 1 &&
                         manifest.Stats().meshParts == 1 &&
                         manifest.Stats().referencedFiles == 9 &&
                         manifest.Stats().aggregateBytes == 27 &&
                         manifest.Stats().fingerprint != 0,
                     "synthetic manifest stats are incorrect");
    passed &= Expect(manifest.FindMaterial("fixture:slot-0") &&
                         manifest.FindMeshPart("fixture:part-0") &&
                         manifest.FindFile("ibl/brdf.dds"),
                     "typed manifest lookup failed");
    const auto *fixtureMaterial = manifest.FindMaterial("fixture:slot-0");
    const auto fixtureDefinition = fixtureMaterial
                                       ? fixtureMaterial->Definition()
                                       : MaterialAssetDefinition3D{};
    passed &= Expect(
        fixtureMaterial &&
            fixtureMaterial->alphaMode == MaterialAlphaMode3D::Mask &&
            fixtureMaterial->alphaCutoff == .37f &&
            fixtureMaterial->doubleSided &&
            fixtureMaterial->materialClass == MaterialClass3D::Foliage &&
            fixtureDefinition.textures.alphaMode == MaterialAlphaMode3D::Mask &&
            fixtureDefinition.textures.alphaCutoff == .37f &&
            fixtureDefinition.material.alphaMode == MaterialAlphaMode3D::Mask &&
            fixtureDefinition.material.alphaCutoff == .37f &&
            fixtureDefinition.material.doubleSided &&
            fixtureDefinition.material.materialClass == MaterialClass3D::Foliage,
        "material rendering semantics were not propagated");
    auto legacy = valid;
    ReplaceOnce(legacy,
                ",\"alphaMode\":\"mask\",\"alphaCutoff\":0.37,\"doubleSided\":true,\"materialClass\":\"foliage\"",
                "");
    CookedRenderManifest3D legacyManifest;
    passed &= Expect(legacyManifest.LoadJson(legacy, root, &error),
                     "legacy material defaults were rejected");
    const auto *legacyMaterial =
        legacyManifest.FindMaterial("fixture:slot-0");
    passed &= Expect(
        legacyMaterial &&
            legacyMaterial->alphaMode == MaterialAlphaMode3D::Opaque &&
            legacyMaterial->alphaCutoff == .5f &&
            !legacyMaterial->doubleSided &&
            legacyMaterial->materialClass == MaterialClass3D::Surface,
        "legacy material defaults changed");
    legacyManifest.Clear();
    const auto acceptedFingerprint = manifest.Stats().fingerprint;

    CookedRenderManifest3D receiptMutation;
    auto receiptHostile = valid;
    ReplaceOnce(receiptHostile, "fixture:slot-0", "fixture:slot-mutated");
    passed &= Expect(!receiptMutation.LoadJson(receiptHostile, root, &error) ||
                         !MatchesFixtureReceipt(receiptMutation),
                     "material identity mutation passed the exact receipt gate");
    receiptHostile = valid;
    ReplaceOnce(receiptHostile, "fixture:part-0", "fixture:part-mutated");
    passed &= Expect(receiptMutation.LoadJson(receiptHostile, root, &error) &&
                         !MatchesFixtureReceipt(receiptMutation),
                     "mesh-part identity mutation passed the exact receipt gate");
    receiptMutation.Clear();

    const auto syntheticManifestPath = root / "runtime-render-manifest.json";
    passed &= Expect(WriteText(syntheticManifestPath, valid) &&
                         manifest.LoadFile(syntheticManifestPath, &error) &&
                         manifest.Stats().manifestSha256.size() == 64 &&
                         manifest.Stats().validatedFiles == 9 &&
                         manifest.Stats().validatedMaterials == 1 &&
                         manifest.Stats().validatedMeshParts == 1 &&
                         manifest.Stats().validatedReferencedFiles == 9,
                     "file-backed manifest SHA/count receipt failed");
    const auto firstManifestSha256 = manifest.Stats().manifestSha256;
    const auto firstFileFingerprint = manifest.Stats().fingerprint;
    const std::string whitespaceVariant = valid + "\n";
    passed &= Expect(WriteText(syntheticManifestPath, whitespaceVariant) &&
                         manifest.LoadFile(syntheticManifestPath, &error) &&
                         manifest.Stats().manifestSha256 != firstManifestSha256 &&
                         manifest.Stats().fingerprint == firstFileFingerprint,
                     "manifest byte change did not change SHA-256 evidence");
    const auto acceptedFileSha256 = manifest.Stats().manifestSha256;
    HANDLE manifestWriter = CreateFileW(
        syntheticManifestPath.c_str(), GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
        OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    passed &= Expect(manifestWriter != INVALID_HANDLE_VALUE &&
                         !manifest.LoadFile(syntheticManifestPath, &error) &&
                         manifest.Stats().manifestSha256 == acceptedFileSha256,
                     "writable manifest handle bypassed pinned read/hash/parse");
    if (manifestWriter != INVALID_HANDLE_VALUE)
        CloseHandle(manifestWriter);
    passed &= Expect(WriteText(syntheticManifestPath, "{invalid") &&
                         !manifest.LoadFile(syntheticManifestPath, &error) &&
                         manifest.Stats().manifestSha256 == acceptedFileSha256 &&
                         manifest.Stats().validatedFiles == 9,
                     "invalid file-backed manifest replaced validated evidence");

    auto hostile = valid;
    ReplaceOnce(hostile, "\"alphaCutoff\":0.37", "\"alphaCutoff\":1.01");
    passed &= Expect(
        !manifest.LoadJson(hostile, root, &error) &&
            error.code == CookedRenderManifestErrorCode3D::InvalidSchema,
        "out-of-range alpha cutoff was accepted");
    hostile = valid;
    ReplaceOnce(hostile, "\"alphaCutoff\":0.37", "\"alphaCutoff\":1e999");
    passed &= Expect(!manifest.LoadJson(hostile, root, &error),
                     "non-finite alpha cutoff was accepted");
    hostile = valid;
    ReplaceOnce(hostile, "\"alphaCutoff\":0.37", "\"alphaCutoff\":-0.01");
    passed &= Expect(
        !manifest.LoadJson(hostile, root, &error) &&
            error.code == CookedRenderManifestErrorCode3D::InvalidSchema,
        "negative alpha cutoff was accepted");
    hostile = valid;
    ReplaceOnce(hostile, "\"alphaCutoff\":0.37",
                "\"alphaCutoff\":\"0.37\"");
    passed &= Expect(
        !manifest.LoadJson(hostile, root, &error) &&
            error.code == CookedRenderManifestErrorCode3D::InvalidSchema,
        "non-numeric alpha cutoff was accepted");
    hostile = valid;
    ReplaceOnce(hostile, "\"doubleSided\":true",
                "\"doubleSided\":\"true\"");
    passed &= Expect(
        !manifest.LoadJson(hostile, root, &error) &&
            error.code == CookedRenderManifestErrorCode3D::InvalidSchema,
        "non-boolean double-sided flag was accepted");
    hostile = valid;
    ReplaceOnce(hostile, "\"materialClass\":\"foliage\"",
                "\"materialClass\":\"terrain\"");
    passed &= Expect(
        !manifest.LoadJson(hostile, root, &error) &&
            error.code == CookedRenderManifestErrorCode3D::InvalidSchema,
        "unsupported material class was accepted");
    hostile = valid;
    ReplaceOnce(hostile, "\"alphaMode\":\"mask\"",
                "\"alphaMode\":\"opaque\"");
    passed &= Expect(
        !manifest.LoadJson(hostile, root, &error) &&
            error.code == CookedRenderManifestErrorCode3D::InvalidSchema,
        "opaque foliage material was accepted");
    hostile = valid;
    ReplaceOnce(hostile, "\"doubleSided\":true",
                "\"doubleSided\":false");
    passed &= Expect(
        !manifest.LoadJson(hostile, root, &error) &&
            error.code == CookedRenderManifestErrorCode3D::InvalidSchema,
        "single-sided foliage material was accepted");
    hostile = valid;
    ReplaceOnce(hostile, "\"schemaVersion\":1", "\"schemaVersion\":2");
    passed &= Expect(!manifest.LoadJson(hostile, root, &error) &&
                         error.code == CookedRenderManifestErrorCode3D::InvalidSchema &&
                         manifest.Stats().fingerprint == acceptedFingerprint,
                     "schema rejection was not transactional");
    hostile = valid;
    ReplaceOnce(hostile, "materials/base.dds", "../base.dds");
    passed &= Expect(!manifest.LoadJson(hostile, root, &error) &&
                         manifest.Stats().fingerprint == acceptedFingerprint,
                     "path traversal was accepted");
    hostile = valid;
    ReplaceOnce(hostile, FixtureSha,
                "za7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
    passed &= Expect(!manifest.LoadJson(hostile, root, &error) &&
                         error.code == CookedRenderManifestErrorCode3D::InvalidSha256,
                     "invalid SHA-256 was accepted");
    hostile = valid;
    ReplaceOnce(hostile, "\"bytes\":3", "\"bytes\":4");
    passed &= Expect(!manifest.LoadJson(hostile, root, &error) &&
                         error.code == CookedRenderManifestErrorCode3D::FileSizeMismatch,
                     "disk size mismatch was accepted");
    hostile = valid;
    ReplaceOnce(hostile, FixtureSha,
                "aa7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
    passed &= Expect(!manifest.LoadJson(hostile, root, &error) &&
                         error.code == CookedRenderManifestErrorCode3D::FileHashMismatch,
                     "disk SHA-256 mismatch was accepted");
    CookedRenderManifestLimits3D aggregateLimits{};
    aggregateLimits.maximumAggregateBytes = 26;
    passed &= Expect(!manifest.LoadJson(valid, root, &error, aggregateLimits) &&
                         error.code == CookedRenderManifestErrorCode3D::InputLimitExceeded,
                     "aggregate byte overflow was accepted");
    hostile = valid;
    const std::string orphan = std::string{"{\"path\":\"orphan.dds\",\"bytes\":3,\"sha256\":\""} +
                               FixtureSha + "\"},";
    ReplaceOnce(hostile, "\"files\":[", "\"files\":[" + orphan);
    passed &= Expect(WriteFixture(root, "orphan.dds") &&
                         !manifest.LoadJson(hostile, root, &error) &&
                         error.code == CookedRenderManifestErrorCode3D::UnreferencedFile,
                     "unreferenced payload was accepted");
    std::filesystem::remove(root / "orphan.dds", filesystemError);
    manifest.Clear();
    std::filesystem::remove(root / "ibl/brdf.dds", filesystemError);
    passed &= Expect(!manifest.LoadJson(valid, root, &error) &&
                         error.code == CookedRenderManifestErrorCode3D::FileMissing,
                     "missing payload was accepted");
    passed &= Expect(WriteFixture(root, "ibl/brdf.dds"),
                     "missing fixture restore failed");
    hostile = valid;
    ReplaceOnce(hostile,
                "\"normalPath\":\"materials/normal.dds\"",
                "\"normalPath\":\"materials/base.dds\"");
    passed &= Expect(!manifest.LoadJson(hostile, root, &error) &&
                         error.code == CookedRenderManifestErrorCode3D::DuplicateEntry,
                     "multiply referenced payload was accepted");
    {
        std::fstream changed(root / "materials/base.dds",
                             std::ios::binary | std::ios::in | std::ios::out);
        changed.write("z", 1);
    }
    passed &= Expect(!manifest.LoadJson(valid, root, &error) &&
                         error.code ==
                             CookedRenderManifestErrorCode3D::FileHashMismatch,
                     "same-size disk hash mismatch was accepted");
    passed &= Expect(WriteFixture(root, "materials/base.dds"),
                     "fixture hash restore failed");
    const auto link = root / "linked-materials";
    const bool linkCreated =
        CreateDirectoryJunction(link, root / "materials");
    passed &= Expect(linkCreated, "junction fixture could not be created");
    hostile = valid;
    ReplaceOnce(hostile, "materials/base.dds", "linked-materials/base.dds");
    ReplaceOnce(hostile, "\"baseColorPath\":\"materials/base.dds\"",
                "\"baseColorPath\":\"linked-materials/base.dds\"");
    const bool reparseRejected = !manifest.LoadJson(hostile, root, &error);
    if (!reparseRejected || error.code != CookedRenderManifestErrorCode3D::InvalidPath)
        std::cerr << "reparse code=" << static_cast<int>(error.code)
                  << " message=" << error.message << '\n';
    passed &= Expect(!linkCreated || (reparseRejected &&
                         error.code == CookedRenderManifestErrorCode3D::InvalidPath),
                     "reparse-point payload path was accepted");
    std::filesystem::remove(link, filesystemError);
    passed &= Expect(manifest.LoadJson(valid, root, &error),
                     "payload lease fixture reload failed");
    const auto pinnedPath = root / "materials/base.dds";
    HANDLE writer = CreateFileW(pinnedPath.c_str(), GENERIC_WRITE,
                                FILE_SHARE_READ | FILE_SHARE_WRITE |
                                    FILE_SHARE_DELETE,
                                nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL,
                                nullptr);
    passed &= Expect(writer == INVALID_HANDLE_VALUE &&
                         GetLastError() == ERROR_SHARING_VIOLATION,
                     "validated payload was mutable before manifest release");
    if (writer != INVALID_HANDLE_VALUE)
        CloseHandle(writer);
    manifest.Clear();
    writer = CreateFileW(pinnedPath.c_str(), GENERIC_WRITE,
                         FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                         nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    passed &= Expect(writer != INVALID_HANDLE_VALUE,
                     "manifest Clear did not release payload file leases");
    if (writer != INVALID_HANDLE_VALUE)
        CloseHandle(writer);

    CookedRenderManifest3D production;
    const std::filesystem::path productionPath =
        std::filesystem::path{AEVRYND_SOURCE_DIR} /
        "engine/assets/realistic-cc0-first-batch/cooked/runtime-render-manifest.json";
    passed &= Expect(production.LoadFile(productionPath, &error),
                     error.message.empty() ? "production manifest was rejected"
                                           : error.message.c_str());
    passed &= Expect(production.Stats().files == 54 &&
                         production.Stats().materials == 11 &&
                         production.Stats().meshParts == 6 &&
                         production.Stats().referencedFiles == 54 &&
                         production.Stats().manifestSha256 ==
                             ProductionManifestSha &&
                         production.Stats().validatedFiles == 54 &&
                         production.Stats().validatedMaterials == 11 &&
                         production.Stats().validatedMeshParts == 6 &&
                         production.Stats().validatedReferencedFiles == 54,
                     "production exact-set counts changed");
    const auto *leaves = production.FindMaterial("island_tree_01:leaves");
    const auto leavesDefinition =
        leaves ? leaves->Definition() : MaterialAssetDefinition3D{};
    passed &= Expect(leaves &&
                         leavesDefinition.textures.alphaMode ==
                             MaterialAlphaMode3D::Mask &&
                         leavesDefinition.textures.alphaCutoff > 0.f &&
                         leavesDefinition.textures.alphaCutoff <= 1.f &&
                         leavesDefinition.material.alphaMode ==
                             MaterialAlphaMode3D::Mask &&
                         leavesDefinition.material.alphaCutoff ==
                             leavesDefinition.textures.alphaCutoff &&
                         leavesDefinition.material.doubleSided &&
                         leavesDefinition.material.materialClass ==
                             MaterialClass3D::Foliage &&
                         leavesDefinition.textures.integrity[0].expectedBytes > 0 &&
                         leavesDefinition.textures.integrity[0]
                                 .expectedSha256.size() == 64,
                     "production masked foliage semantics were lost");

    std::filesystem::remove_all(root, filesystemError);
    if (passed)
        std::cout << "synthetic=9 production=54 materials=11 meshParts=6 exactSet=passed\n";
    return passed ? 0 : 1;
}
