#include <Aevrynd/CharacterAssetProvenance3D.h>
#include <Aevrynd/SkinnedModelAsset.h>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <winioctl.h>
#include <bcrypt.h>

#include <array>
#include <cstddef>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace {

bool Expect(bool condition, const char* message) {
    if (!condition) std::cerr << message << '\n';
    return condition;
}

std::string Read(const std::filesystem::path& path) {
    std::ifstream stream(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
}

void Write(const std::filesystem::path& path, std::string_view contents) {
    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    stream.write(contents.data(), static_cast<std::streamsize>(contents.size()));
}

bool ReplaceOne(std::string& text, std::string_view before, std::string_view after) {
    const std::size_t offset = text.find(before);
    if (offset == std::string::npos || text.find(before, offset + before.size()) != std::string::npos) return false;
    text.replace(offset, before.size(), after);
    return true;
}

bool SwapOne(std::string& text, std::string_view left, std::string_view right) {
    constexpr std::string_view Marker = "\"role\": \"swap-fixture-marker\"";
    return ReplaceOne(text, left, Marker) && ReplaceOne(text, right, left) &&
           ReplaceOne(text, Marker, right);
}

bool ReplaceArrayWithObject(std::string& text, std::string_view field) {
    const std::string key = "\"" + std::string(field) + "\"";
    const std::size_t keyOffset = text.find(key);
    if (keyOffset == std::string::npos || text.find(key, keyOffset + key.size()) != std::string::npos)
        return false;
    const std::size_t begin = text.find('[', keyOffset + key.size());
    if (begin == std::string::npos) return false;
    std::size_t depth{}; bool quoted{}, escaped{};
    for (std::size_t index = begin; index < text.size(); ++index) {
        const char value = text[index];
        if (quoted) {
            if (escaped) escaped = false;
            else if (value == '\\') escaped = true;
            else if (value == '"') quoted = false;
        } else if (value == '"') quoted = true;
        else if (value == '[') ++depth;
        else if (value == ']' && --depth == 0u) {
            text.replace(begin, index - begin + 1u, "{}");
            return true;
        }
    }
    return false;
}

std::string Sha256(std::string_view contents) {
    BCRYPT_ALG_HANDLE algorithm{}; BCRYPT_HASH_HANDLE hash{};
    DWORD objectBytes{}, copied{}; std::vector<UCHAR> object;
    std::array<UCHAR, 32> digest{};
    bool passed = BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0u) >= 0;
    if (passed) passed = BCryptGetProperty(algorithm, BCRYPT_OBJECT_LENGTH,
        reinterpret_cast<PUCHAR>(&objectBytes), sizeof(objectBytes), &copied, 0u) >= 0;
    if (passed) object.resize(objectBytes);
    if (passed) passed = BCryptCreateHash(algorithm, &hash, object.data(), objectBytes,
                                          nullptr, 0u, 0u) >= 0;
    if (passed) passed = BCryptHashData(
        hash, reinterpret_cast<PUCHAR>(const_cast<char*>(contents.data())),
        static_cast<ULONG>(contents.size()), 0u) >= 0;
    if (passed) passed = BCryptFinishHash(hash, digest.data(),
                                          static_cast<ULONG>(digest.size()), 0u) >= 0;
    if (hash) BCryptDestroyHash(hash);
    if (algorithm) BCryptCloseAlgorithmProvider(algorithm, 0u);
    if (!passed) return {};
    constexpr char Digits[] = "0123456789abcdef";
    std::string result(64u, '0');
    for (std::size_t index = 0; index < digest.size(); ++index) {
        result[index * 2u] = Digits[digest[index] >> 4u];
        result[index * 2u + 1u] = Digits[digest[index] & 15u];
    }
    return result;
}

Aevrynd::CharacterAssetProvenanceTrustAnchor3D AnchorFor(std::string_view manifest) {
    return {manifest.size(), Sha256(manifest)};
}

bool Preserved(const Aevrynd::CharacterAssetProvenanceReceipt3D& value,
               const Aevrynd::CharacterAssetProvenanceReceipt3D& baseline) {
    return value == baseline;
}

bool CreateJunction(const std::filesystem::path& link, const std::filesystem::path& target) {
    struct MountPointReparseData3D {
        DWORD tag;
        WORD dataLength;
        WORD reserved;
        WORD substituteNameOffset;
        WORD substituteNameLength;
        WORD printNameOffset;
        WORD printNameLength;
        wchar_t pathBuffer[1];
    };
    constexpr std::size_t ReparseHeaderBytes = sizeof(DWORD) + sizeof(WORD) + sizeof(WORD);
    std::error_code ignored;
    if (!std::filesystem::create_directory(link, ignored) || ignored) return false;
    const std::wstring printName = std::filesystem::absolute(target).lexically_normal().wstring();
    const std::wstring substituteName = L"\\??\\" + printName;
    const std::size_t pathBytes = (substituteName.size() + 1u + printName.size() + 1u) * sizeof(wchar_t);
    std::vector<unsigned char> storage(offsetof(MountPointReparseData3D, pathBuffer) + pathBytes);
    auto* data = reinterpret_cast<MountPointReparseData3D*>(storage.data());
    data->tag = IO_REPARSE_TAG_MOUNT_POINT;
    data->dataLength = static_cast<WORD>(storage.size() - ReparseHeaderBytes);
    data->reserved = 0u;
    data->substituteNameOffset = 0u;
    data->substituteNameLength = static_cast<WORD>(substituteName.size() * sizeof(wchar_t));
    data->printNameOffset = static_cast<WORD>((substituteName.size() + 1u) * sizeof(wchar_t));
    data->printNameLength = static_cast<WORD>(printName.size() * sizeof(wchar_t));
    std::memcpy(data->pathBuffer, substituteName.c_str(),
                (substituteName.size() + 1u) * sizeof(wchar_t));
    std::memcpy(reinterpret_cast<unsigned char*>(data->pathBuffer) + data->printNameOffset,
                printName.c_str(), (printName.size() + 1u) * sizeof(wchar_t));
    const HANDLE directory = CreateFileW(link.c_str(), GENERIC_WRITE, 0u, nullptr, OPEN_EXISTING,
                                          FILE_FLAG_OPEN_REPARSE_POINT | FILE_FLAG_BACKUP_SEMANTICS,
                                          nullptr);
    if (directory == INVALID_HANDLE_VALUE) return false;
    DWORD returned{};
    const BOOL created = DeviceIoControl(directory, FSCTL_SET_REPARSE_POINT, data,
                                         static_cast<DWORD>(storage.size()), nullptr, 0u,
                                         &returned, nullptr);
    CloseHandle(directory);
    return created != FALSE;
}

struct BarrierProbe3D {
    std::filesystem::path runtimeRoot;
    std::filesystem::path rebound;
    bool runtimeRootRenameRejected{};
    bool modelPacksRenameRejected{};
    bool assetPackRenameRejected{};
    bool cookedWriterRejected{};
};

void ProbeBarrier(Aevrynd::CharacterAssetProvenanceBarrier3D barrier, void* context) noexcept {
    auto& probe = *static_cast<BarrierProbe3D*>(context);
    auto renameRejected = [&](const std::filesystem::path& path, std::wstring_view suffix) {
        const auto rebound = probe.rebound / suffix;
        const BOOL moved = MoveFileExW(path.c_str(), rebound.c_str(), MOVEFILE_REPLACE_EXISTING);
        if (moved) MoveFileExW(rebound.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING);
        const DWORD reason = moved ? ERROR_SUCCESS : GetLastError();
        return moved == FALSE &&
               (reason == ERROR_SHARING_VIOLATION || reason == ERROR_ACCESS_DENIED);
    };
    if (barrier == Aevrynd::CharacterAssetProvenanceBarrier3D::RuntimeRootPinned)
        probe.runtimeRootRenameRejected = renameRejected(probe.runtimeRoot, L"runtime-root");
    else if (barrier == Aevrynd::CharacterAssetProvenanceBarrier3D::ModelPacksPinned)
        probe.modelPacksRenameRejected =
            renameRejected(probe.runtimeRoot / "assets/model-packs", L"model-packs");
    else if (barrier == Aevrynd::CharacterAssetProvenanceBarrier3D::AssetPackPinned)
        probe.assetPackRenameRejected = renameRejected(
            probe.runtimeRoot / "assets/model-packs/realistic-female", L"realistic-female");
    else if (barrier == Aevrynd::CharacterAssetProvenanceBarrier3D::CookedAssetPinned) {
        const HANDLE writer = CreateFileW(
            (probe.runtimeRoot / "assets/model-packs/realistic-female/realistic-female.aeskin").c_str(),
            GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL, nullptr);
        const DWORD reason = writer == INVALID_HANDLE_VALUE ? GetLastError() : ERROR_SUCCESS;
        probe.cookedWriterRejected = writer == INVALID_HANDLE_VALUE &&
                                     reason == ERROR_SHARING_VIOLATION;
        if (writer != INVALID_HANDLE_VALUE) CloseHandle(writer);
    }
}

} // namespace

int main() {
    using namespace Aevrynd;
    const std::filesystem::path source = AEVRYND_SOURCE_DIR;
    const auto expectedRuntimeRoot = source / "games/character_lab";
    const auto assetRoot = expectedRuntimeRoot / "assets/model-packs/realistic-female";
    const auto manifest = assetRoot / "realistic-female.provenance.json";
    CharacterAssetProvenanceReceipt3D receipt;
    CharacterAssetProvenanceError3D error;
    bool passed = true;
    const std::string valid = Read(manifest);
    const CharacterAssetProvenanceTrustAnchor3D canonicalAnchor{
        2353u, "0bc845ee76105cf25e6438670de94795ccff669b3e1f86bc9775ca43c8c9184e"};

    const bool canonicalValidated = ValidateCharacterAssetProvenance3D(
        expectedRuntimeRoot, manifest, canonicalAnchor, receipt, &error);
    if (!canonicalValidated) {
        std::cerr << "canonical error=" << static_cast<unsigned>(error.code)
                  << " offset=" << error.offset << " message=" << error.message << '\n';
    }
    passed &= Expect(canonicalValidated, "canonical provenance manifest did not validate");
    passed &= Expect(receipt.passed && receipt.assetId == "realistic-female" &&
                         receipt.canonicalRuntimePath ==
                             "assets/model-packs/realistic-female/realistic-female.aeskin" &&
                         receipt.generatorId == "tools/blender/create-cc0-f2-m2-characters.py" &&
                         receipt.converterId == "tools/convert-gltf-to-aeskin-v2.ps1" &&
                         receipt.skeletonIdentity == "F2FemaleRig" && receipt.jointCount == 39u &&
                         receipt.materials.size() == 9u && receipt.lods.size() == 1u &&
                         receipt.authoredAnimations.size() == 2u &&
                         receipt.authoredAnimations.front() == "IdleBreathing" &&
                         receipt.authoredAnimations.back() == "GripPrototype" &&
                         receipt.runtimeFallbacks.size() == 1u &&
                         receipt.runtimeFallbacks.front() == "procedural-locomotion" &&
                         receipt.artifacts.size() == 3u && receipt.manifestBytes > 0u &&
                         receipt.manifestBytes == 2353u &&
                         receipt.manifestSha256 ==
                             "0bc845ee76105cf25e6438670de94795ccff669b3e1f86bc9775ca43c8c9184e" &&
                         receipt.immutableCookedAeskin.size() == 1480232u,
                     "canonical provenance receipt is incomplete or dishonest");
    passed &= Expect(valid.size() == 2353u && !valid.empty() && valid.back() == '\n' &&
                         valid.find('\r') == std::string::npos,
                     "canonical manifest is not the exact LF-only 2353-byte document");
    const CharacterAssetProvenanceReceipt3D baseline = receipt;

    const auto temporary = std::filesystem::temp_directory_path() /
                           "aevrynd-character-asset-provenance-smoke";
    const auto outside = std::filesystem::temp_directory_path() /
                         "aevrynd-character-asset-provenance-outside";
    std::error_code ignored;
    std::filesystem::remove_all(temporary, ignored);
    std::filesystem::remove_all(outside, ignored);
    const auto temporaryAssetRoot =
        temporary / "assets/model-packs/realistic-female";
    std::filesystem::create_directories(temporaryAssetRoot / "source");
    for (const auto& relative : {std::filesystem::path("source/realistic-female.gltf"),
                                 std::filesystem::path("source/realistic-female.bin"),
                                 std::filesystem::path("realistic-female.aeskin")}) {
        std::filesystem::copy_file(assetRoot / relative, temporaryAssetRoot / relative,
                                   std::filesystem::copy_options::overwrite_existing);
    }
    const auto testManifest = temporaryAssetRoot / "realistic-female.provenance.json";
    auto Reject = [&](std::string document, CharacterAssetProvenanceErrorCode3D expected,
                      const char* message) {
        Write(testManifest, document);
        error = {};
        const auto fixtureAnchor = AnchorFor(document);
        const bool rejected = !ValidateCharacterAssetProvenance3D(
            temporary, testManifest, fixtureAnchor, receipt, &error);
        passed &= Expect(rejected && error.code == expected && Preserved(receipt, baseline), message);
    };

    std::string hostile = valid;
    passed &= Expect(ReplaceOne(hostile, "\"assetId\": \"realistic-female\"",
                                "\"assetIdentifier\": \"realistic-female\""),
                     "missing-field fixture creation failed");
    Reject(hostile, CharacterAssetProvenanceErrorCode3D::InvalidSchema,
           "missing required field was accepted or erased last-good receipt");

    hostile = valid;
    passed &= Expect(ReplaceOne(hostile, "\"path\": \"source/realistic-female.gltf\"",
                                "\"path\": \"../source/realistic-female.gltf\""),
                     "path-traversal fixture creation failed");
    Reject(hostile, CharacterAssetProvenanceErrorCode3D::InvalidSchema,
           "path traversal was accepted or erased last-good receipt");

    hostile = valid;
    passed &= Expect(ReplaceOne(hostile, "\"schema\": \"aevrynd.character-asset-provenance\",",
                                "\"schema\": \"aevrynd.character-asset-provenance\",\n  \"schema\": \"duplicate\","),
                     "duplicate-key fixture creation failed");
    Reject(hostile, CharacterAssetProvenanceErrorCode3D::InvalidJson,
           "duplicate JSON key was accepted or erased last-good receipt");

    hostile = valid;
    passed &= Expect(ReplaceOne(hostile, "\"identity\": \"realistic-female-source-bin\"",
                                "\"identity\": \"realistic-female-source-gltf\""),
                     "duplicate-identity fixture creation failed");
    Reject(hostile, CharacterAssetProvenanceErrorCode3D::InvalidSchema,
           "duplicate identity was accepted or erased last-good receipt");

    hostile = valid;
    passed &= Expect(ReplaceOne(hostile, "\"bytes\": 106478", "\"bytes\": 106479"),
                     "size fixture creation failed");
    Reject(hostile, CharacterAssetProvenanceErrorCode3D::IntegrityMismatch,
           "artifact size mismatch was accepted or erased last-good receipt");

    hostile = valid;
    passed &= Expect(ReplaceOne(hostile,
        "c7fb36382f6460e38613e501015250473b15b960022c64a335178c1a565c95ed",
        "07fb36382f6460e38613e501015250473b15b960022c64a335178c1a565c95ed"),
        "hash fixture creation failed");
    Reject(hostile, CharacterAssetProvenanceErrorCode3D::IntegrityMismatch,
           "artifact SHA-256 mismatch was accepted or erased last-good receipt");

    hostile = valid;
    passed &= Expect(ReplaceOne(hostile,
                                "assets/model-packs/realistic-female/realistic-female.aeskin",
                                "spoof/assets/model-packs/realistic-female/realistic-female.aeskin"),
                     "canonical suffix-spoof fixture creation failed");
    Reject(hostile, CharacterAssetProvenanceErrorCode3D::InvalidSchema,
           "canonical path suffix spoof was accepted or erased last-good receipt");

    hostile = valid;
    passed &= Expect(SwapOne(hostile, "\"role\": \"source-gltf\"",
                              "\"role\": \"source-bin\""),
                     "artifact role-swap fixture creation failed");
    Reject(hostile, CharacterAssetProvenanceErrorCode3D::InvalidJson,
           "artifact role/path mismatch was accepted or erased last-good receipt");

    hostile = valid;
    passed &= Expect(ReplaceOne(hostile, "\"identity\": \"IdleBreathing\"",
                                "\"identity\": \"MissingClip\"") &&
                         ReplaceOne(hostile, "\"clip\": \"IdleBreathing\"",
                                    "\"clip\": \"MissingClip\""),
                     "authored-clip mismatch fixture creation failed");
    Reject(hostile, CharacterAssetProvenanceErrorCode3D::IntegrityMismatch,
           "manifest clip absent from sealed GLTF/AESKIN was accepted or erased last-good receipt");

    hostile = valid;
    passed &= Expect(ReplaceOne(hostile, "\"jointCount\": 39", "\"jointCount\": 40"),
                     "skeleton joint-count mismatch fixture creation failed");
    Reject(hostile, CharacterAssetProvenanceErrorCode3D::IntegrityMismatch,
           "manifest skeleton count absent from sealed AESKIN was accepted or erased last-good receipt");

    hostile = valid;
    passed &= Expect(ReplaceOne(hostile, "\"SkinWarmPBR\"", "\"SkinColdPBR\""),
                     "material mismatch fixture creation failed");
    Reject(hostile, CharacterAssetProvenanceErrorCode3D::IntegrityMismatch,
           "manifest material absent from sealed GLTF/AESKIN was accepted or erased last-good receipt");

    for (std::string_view collection : {"materials", "lods", "animations", "runtimeFallbacks"}) {
        hostile = valid;
        const std::string field = "\"" + std::string(collection) + "\"";
        const std::string missing = "\"missing-" + std::string(collection) + "\"";
        passed &= Expect(ReplaceOne(hostile, field, missing),
                         "missing collection fixture creation failed");
        Reject(hostile, CharacterAssetProvenanceErrorCode3D::InvalidSchema,
               "missing collection returned no precise schema error or erased last-good receipt");
        hostile = valid;
        passed &= Expect(ReplaceArrayWithObject(hostile, collection),
                         "wrong-kind collection fixture creation failed");
        Reject(hostile, CharacterAssetProvenanceErrorCode3D::InvalidSchema,
               "wrong-kind collection returned no precise schema error or erased last-good receipt");
    }

    hostile = valid;
    passed &= Expect(ReplaceOne(hostile, "\"level\": 0", "\"level\": 1"),
                     "LOD gap fixture creation failed");
    Reject(hostile, CharacterAssetProvenanceErrorCode3D::InvalidSchema,
           "non-zero-starting LOD was accepted or erased last-good receipt");

    hostile = valid;
    constexpr std::string_view LodsBefore =
        "    {\n      \"identity\": \"lod0\",\n      \"level\": 0,\n      \"source\": \"cooked-aeskin\"\n    }";
    constexpr std::string_view LodsAfter =
        "    {\n      \"identity\": \"lod0\",\n      \"level\": 0,\n      \"source\": \"cooked-aeskin\"\n    },\n"
        "    {\n      \"identity\": \"lod1\",\n      \"level\": 0,\n      \"source\": \"cooked-aeskin\"\n    }";
    passed &= Expect(ReplaceOne(hostile, LodsBefore, LodsAfter),
                     "duplicate LOD level fixture creation failed");
    Reject(hostile, CharacterAssetProvenanceErrorCode3D::DuplicateIdentity,
           "duplicate LOD level was accepted or erased last-good receipt");

    hostile = valid;
    constexpr std::string_view ExtraLod =
        "    {\n      \"identity\": \"lod0\",\n      \"level\": 0,\n      \"source\": \"cooked-aeskin\"\n    },\n"
        "    {\n      \"identity\": \"lod1\",\n      \"level\": 1,\n      \"source\": \"cooked-aeskin\"\n    }";
    passed &= Expect(ReplaceOne(hostile, LodsBefore, ExtraLod),
                     "unproved extra LOD fixture creation failed");
    Reject(hostile, CharacterAssetProvenanceErrorCode3D::IntegrityMismatch,
           "manifest extra LOD absent from sealed AESKIN was accepted or erased last-good receipt");

    Write(testManifest, valid);
    std::filesystem::create_hard_link(temporaryAssetRoot / "source/realistic-female.gltf",
                                      temporaryAssetRoot / "source/duplicate-gltf");
    hostile = valid;
    passed &= Expect(ReplaceOne(hostile, "\"path\": \"source/realistic-female.bin\"",
                                "\"path\": \"source/duplicate-gltf\"") &&
                         ReplaceOne(hostile, "\"bytes\": 1944160", "\"bytes\": 106478") &&
                         ReplaceOne(hostile,
                                    "dc4defdacdd1904c4117cbc615fe48c6a357974f2c7507f17aa87700fb64a970",
                                    "c7fb36382f6460e38613e501015250473b15b960022c64a335178c1a565c95ed"),
                     "physical duplicate fixture creation failed");
    Reject(hostile, CharacterAssetProvenanceErrorCode3D::DuplicateIdentity,
           "two artifact paths to one physical file were accepted or erased last-good receipt");
    std::filesystem::remove(temporaryAssetRoot / "source/duplicate-gltf");

    Write(testManifest, valid);
    error = {};
    passed &= Expect(!ValidateCharacterAssetProvenance3D(expectedRuntimeRoot, testManifest,
                                                          canonicalAnchor, receipt, &error) &&
                         error.code == CharacterAssetProvenanceErrorCode3D::InvalidPath &&
                         Preserved(receipt, baseline),
                     "self-consistent temporary pack escaped expected runtime root binding");

    CharacterAssetProvenanceLimits3D excessiveLimits;
    excessiveLimits.maximumArtifactBytes = 256ull * 1024ull * 1024ull + 1ull;
    error = {};
    passed &= Expect(!ValidateCharacterAssetProvenance3D(temporary, testManifest,
                                                          canonicalAnchor, receipt,
                                                          &error, excessiveLimits) &&
                         error.code == CharacterAssetProvenanceErrorCode3D::InputLimitExceeded &&
                         Preserved(receipt, baseline),
                     "artifact maximum exceeded the hard validation ceiling");

    std::filesystem::create_directories(outside);
    std::filesystem::copy_file(assetRoot / "source/realistic-female.gltf",
                               outside / "realistic-female.gltf",
                               std::filesystem::copy_options::overwrite_existing);
    passed &= Expect(CreateJunction(temporaryAssetRoot / "linked-source", outside),
                     "parent-junction fixture creation failed");
    hostile = valid;
    passed &= Expect(ReplaceOne(hostile, "\"path\": \"source/realistic-female.gltf\"",
                                "\"path\": \"linked-source/realistic-female.gltf\""),
                     "parent-junction manifest fixture creation failed");
    Reject(hostile, CharacterAssetProvenanceErrorCode3D::InvalidPath,
           "parent junction escaped pinned root or erased last-good receipt");

    Write(testManifest, valid);
    const std::string originalSourceBin = Read(temporaryAssetRoot / "source/realistic-female.bin");
    std::string tamperedSourceBin = originalSourceBin;
    passed &= Expect(!tamperedSourceBin.empty(), "source BIN tamper fixture is empty");
    if (!tamperedSourceBin.empty()) tamperedSourceBin.front() ^= 1;
    Write(temporaryAssetRoot / "source/realistic-female.bin", tamperedSourceBin);
    error = {};
    passed &= Expect(!ValidateCharacterAssetProvenance3D(
                         temporary, testManifest, canonicalAnchor, receipt, &error) &&
                         error.code == CharacterAssetProvenanceErrorCode3D::IntegrityMismatch &&
                         Preserved(receipt, baseline),
                     "source BIN byte tamper passed the anchored artifact hash chain");

    hostile = valid;
    const std::string tamperedBinSha = Sha256(tamperedSourceBin);
    passed &= Expect(!tamperedBinSha.empty() && ReplaceOne(
                         hostile,
                         "dc4defdacdd1904c4117cbc615fe48c6a357974f2c7507f17aa87700fb64a970",
                         tamperedBinSha),
                     "coherent artifact replacement fixture creation failed");
    Write(testManifest, hostile);
    error = {};
    passed &= Expect(!ValidateCharacterAssetProvenance3D(
                         temporary, testManifest, canonicalAnchor, receipt, &error) &&
                         error.code == CharacterAssetProvenanceErrorCode3D::IntegrityMismatch &&
                         Preserved(receipt, baseline),
                     "coherent manifest plus semantic BIN replacement bypassed runtime trust anchor");
    Write(temporaryAssetRoot / "source/realistic-female.bin", originalSourceBin);
    Write(testManifest, valid);

    {
        std::ofstream replaced(temporaryAssetRoot / "realistic-female.aeskin", std::ios::binary | std::ios::app);
        replaced.put('\0');
    }
    error = {};
    passed &= Expect(!ValidateCharacterAssetProvenance3D(temporary, testManifest, canonicalAnchor,
                                                          receipt, &error) &&
                         error.code == CharacterAssetProvenanceErrorCode3D::IntegrityMismatch &&
                         Preserved(receipt, baseline),
                     "replaced cooked asset was accepted or erased last-good receipt");
    std::filesystem::copy_file(assetRoot / "realistic-female.aeskin",
                               temporaryAssetRoot / "realistic-female.aeskin",
                               std::filesystem::copy_options::overwrite_existing);

    std::filesystem::remove(temporaryAssetRoot / "source/realistic-female.bin");
    error = {};
    passed &= Expect(!ValidateCharacterAssetProvenance3D(temporary, testManifest, canonicalAnchor,
                                                          receipt, &error) &&
                         (error.code == CharacterAssetProvenanceErrorCode3D::InvalidPath ||
                          error.code == CharacterAssetProvenanceErrorCode3D::IoFailure) &&
                         Preserved(receipt, baseline),
                     "missing source artifact was accepted or erased last-good receipt");
    std::filesystem::copy_file(assetRoot / "source/realistic-female.bin",
                               temporaryAssetRoot / "source/realistic-female.bin");

    const HANDLE writer = CreateFileW(testManifest.c_str(), GENERIC_WRITE,
                                      FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                                      OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    passed &= Expect(writer != INVALID_HANDLE_VALUE, "manifest race fixture could not acquire writer");
    if (writer != INVALID_HANDLE_VALUE) {
        error = {};
        passed &= Expect(!ValidateCharacterAssetProvenance3D(temporary, testManifest,
                                                              canonicalAnchor, receipt, &error) &&
                             error.code == CharacterAssetProvenanceErrorCode3D::ConcurrentModification &&
                             Preserved(receipt, baseline),
                         "writable manifest race was accepted or erased last-good receipt");
        CloseHandle(writer);
    }

    const HANDLE artifactWriter = CreateFileW((temporaryAssetRoot / "source/realistic-female.gltf").c_str(),
                                              GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
                                              nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    passed &= Expect(artifactWriter != INVALID_HANDLE_VALUE,
                     "artifact race fixture could not acquire writer");
    if (artifactWriter != INVALID_HANDLE_VALUE) {
        error = {};
        passed &= Expect(!ValidateCharacterAssetProvenance3D(temporary, testManifest,
                                                              canonicalAnchor, receipt, &error) &&
                             error.code == CharacterAssetProvenanceErrorCode3D::ConcurrentModification &&
                             Preserved(receipt, baseline),
                         "writable artifact race was accepted or erased last-good receipt");
        CloseHandle(artifactWriter);
    }

    const auto rebound = std::filesystem::temp_directory_path() /
                         "aevrynd-character-asset-provenance-rebound";
    std::filesystem::remove_all(rebound, ignored);
    std::filesystem::create_directories(rebound);
    BarrierProbe3D probe{temporary, rebound};
    CharacterAssetProvenanceLimits3D barrierLimits;
    barrierLimits.barrierCallback = ProbeBarrier;
    barrierLimits.barrierContext = &probe;
    error = {};
    passed &= Expect(ValidateCharacterAssetProvenance3D(
                         temporary, testManifest, canonicalAnchor, receipt, &error,
                         barrierLimits) && receipt.passed &&
                         probe.runtimeRootRenameRejected && probe.modelPacksRenameRejected &&
                         probe.assetPackRenameRejected && probe.cookedWriterRejected,
                     "pinned root/parent/cooked deterministic barriers did not reject rebinding");

    const auto immutableLoad = LoadSkinnedModelAssetDetailed(receipt.immutableCookedAeskin);
    {
        std::ofstream replaced(temporaryAssetRoot / "realistic-female.aeskin",
                               std::ios::binary | std::ios::app);
        replaced.put('\0');
    }
    const auto replacedLoad = LoadSkinnedModelAssetFileDetailed(
        temporaryAssetRoot / "realistic-female.aeskin");
    passed &= Expect(immutableLoad && !replacedLoad &&
                         immutableLoad.asset.jointNames.size() == receipt.jointCount,
                     "validate-to-use replacement consumed unvalidated B instead of immutable A");
    std::filesystem::copy_file(assetRoot / "realistic-female.aeskin",
                               temporaryAssetRoot / "realistic-female.aeskin",
                               std::filesystem::copy_options::overwrite_existing);
    std::filesystem::remove_all(temporary, ignored);
    std::filesystem::remove_all(outside, ignored);
    std::filesystem::remove_all(rebound, ignored);
    if (!passed) return 1;
    std::cout << "CHARACTER_ASSET_PROVENANCE=PASS artifacts=" << receipt.artifacts.size()
              << " materials=" << receipt.materials.size()
              << " authoredClips=" << receipt.authoredAnimations.size()
              << " runtimeFallbacks=" << receipt.runtimeFallbacks.size() << '\n';
    return 0;
}
