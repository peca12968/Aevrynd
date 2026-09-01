#include <Aevrynd/NativeMaterialDds3D.h>

#include <windows.h>

#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <vector>

#ifndef AEVRYND_SOURCE_DIR
#error AEVRYND_SOURCE_DIR is required
#endif

using namespace Aevrynd;

namespace {
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
    const auto root = std::filesystem::path{AEVRYND_SOURCE_DIR} / "engine" /
                      "assets" / "realistic-cc0-first-batch" / "cooked" /
                      "materials" / "bricks100";
    struct Fixture {
        const wchar_t *name;
        NativeMaterialDdsRole3D role;
        DXGI_FORMAT format;
    };
    const Fixture fixtures[] = {
        {L"slot-0-base.dds", NativeMaterialDdsRole3D::BaseColor,
         DXGI_FORMAT_BC7_UNORM_SRGB},
        {L"slot-0-arm.dds", NativeMaterialDdsRole3D::Surface,
         DXGI_FORMAT_BC7_UNORM},
        {L"slot-0-normal.dds", NativeMaterialDdsRole3D::Normal,
         DXGI_FORMAT_BC5_UNORM}};
    for (const auto &fixture : fixtures) {
        NativeMaterialDds3D image{};
        NativeMaterialDdsError3D error{};
        if (!LoadNativeMaterialDds3D(root / fixture.name, fixture.role, 4096,
                                     true, image, &error) ||
            image.width != 1024 || image.height != 512 || image.mipCount != 11 ||
            image.format != fixture.format || image.payloadBytes != 699088 ||
            image.mips.size() != 11 || image.sha256.size() != 64) {
            std::cerr << "Native DDS fixture failed error=" << unsigned(error)
                      << " width=" << image.width << " height=" << image.height
                      << " mips=" << image.mipCount
                      << " payload=" << image.payloadBytes << '\n';
            return 1;
        }
        NativeMaterialDds3D rejected{};
        if (LoadNativeMaterialDds3D(root / fixture.name, fixture.role, 4096,
                                    false, rejected, &error,
                                    std::string(64, '0'), 699236) ||
            error != NativeMaterialDdsError3D::HashMismatch)
            return 2;
        if (LoadNativeMaterialDds3D(root / fixture.name, fixture.role, 4096,
                                    false, rejected, &error, image.sha256,
                                    699235) ||
            error != NativeMaterialDdsError3D::HashMismatch)
            return 3;
        if (!LoadNativeMaterialDds3D(root / fixture.name, fixture.role, 4096,
                                     false, rejected, &error, image.sha256,
                                     699236))
            return 4;
    }
    const auto terrainRoot = std::filesystem::path{AEVRYND_SOURCE_DIR} /
                             "engine/assets/realistic-cc0-first-batch/cooked/materials/forest-ground-04";
    struct TerrainFixture {
        const wchar_t *name;
        NativeMaterialDdsRole3D role;
        DXGI_FORMAT format;
        const char *sha256;
    };
    const TerrainFixture terrainFixtures[] = {
        {L"slot-0-base.dds", NativeMaterialDdsRole3D::BaseColor,
         DXGI_FORMAT_BC7_UNORM_SRGB,
         "00850c7c033be2bcb2818c0ab3db91a62c8353eba746daede3c0de373c3e32b4"},
        {L"slot-0-arm.dds", NativeMaterialDdsRole3D::Surface,
         DXGI_FORMAT_BC7_UNORM,
         "3427bf0a83e0c33326a207f700305620402fd0fb6c532240f204bcebefeabcc4"},
        {L"slot-0-normal.dds", NativeMaterialDdsRole3D::Normal,
         DXGI_FORMAT_BC5_UNORM,
         "56e687b2a208f2eaaf9b0e3670530f0a36e5e88d6e74b26090633daaa4e9db93"}};
    for (const auto &fixture : terrainFixtures) {
        NativeMaterialDds3D image{};
        NativeMaterialDdsError3D error{};
        if (!LoadNativeMaterialDds3D(terrainRoot / fixture.name, fixture.role,
                                     4096, false, image, &error,
                                     fixture.sha256, 5592580) ||
            image.width != 2048 || image.height != 2048 ||
            image.mipCount != 12 || image.format != fixture.format ||
            image.payloadBytes != 5592432 || image.sha256 != fixture.sha256) {
            std::cerr << "Forest ground DDS contract failed error="
                      << unsigned(error) << " format=" << unsigned(image.format)
                      << " bytes=" << image.payloadBytes << '\n';
            return 18;
        }
    }
    const auto temporary = std::filesystem::temp_directory_path() /
                           ("aevrynd-native-dds-hostile-" +
                            std::to_string(GetCurrentProcessId()));
    struct Cleanup {
        std::filesystem::path path;
        ~Cleanup() {
            std::error_code ignored;
            std::filesystem::remove_all(path, ignored);
        }
    } cleanup{temporary};
    std::filesystem::create_directories(temporary);
    const auto sourcePath = root / L"slot-0-base.dds";
    std::ifstream source(sourcePath, std::ios::binary);
    std::vector<std::uint8_t> sourceBytes(
        std::istreambuf_iterator<char>{source}, std::istreambuf_iterator<char>{});
    auto rejected = [&](std::string_view name, std::vector<std::uint8_t> bytes,
                        NativeMaterialDdsError3D expected) {
        const auto path = temporary / std::string{name};
        std::ofstream(path, std::ios::binary)
            .write(reinterpret_cast<const char *>(bytes.data()),
                   static_cast<std::streamsize>(bytes.size()));
        NativeMaterialDds3D image{};
        NativeMaterialDdsError3D error{};
        return !LoadNativeMaterialDds3D(path,
                                        NativeMaterialDdsRole3D::BaseColor,
                                        4096, true, image, &error) &&
               error == expected;
    };
    auto write32 = [](std::vector<std::uint8_t> &bytes, std::size_t offset,
                      std::uint32_t value) {
        std::memcpy(bytes.data() + offset, &value, sizeof(value));
    };
    auto mutated = sourceBytes;
    write32(mutated, 84, 0);
    if (!rejected("legacy.dds", std::move(mutated),
                  NativeMaterialDdsError3D::InvalidHeader))
        return 5;
    mutated = sourceBytes;
    write32(mutated, 128, DXGI_FORMAT_R8G8B8A8_UNORM);
    if (!rejected("format.dds", std::move(mutated),
                  NativeMaterialDdsError3D::UnsupportedFormat))
        return 6;
    mutated = sourceBytes;
    write32(mutated, 132, 4);
    if (!rejected("dimension.dds", std::move(mutated),
                  NativeMaterialDdsError3D::UnsupportedResource))
        return 7;
    mutated = sourceBytes;
    write32(mutated, 136, 4);
    if (!rejected("cube.dds", std::move(mutated),
                  NativeMaterialDdsError3D::UnsupportedResource))
        return 8;
    mutated = sourceBytes;
    write32(mutated, 140, 2);
    if (!rejected("array.dds", std::move(mutated),
                  NativeMaterialDdsError3D::UnsupportedResource))
        return 9;
    mutated = sourceBytes;
    write32(mutated, 8, 0x800000u);
    if (!rejected("depth-flag.dds", std::move(mutated),
                  NativeMaterialDdsError3D::UnsupportedResource))
        return 10;
    mutated = sourceBytes;
    write32(mutated, 24, 2);
    if (!rejected("depth.dds", std::move(mutated),
                  NativeMaterialDdsError3D::UnsupportedResource))
        return 11;
    mutated = sourceBytes;
    write32(mutated, 112, 0x200000u);
    if (!rejected("volume.dds", std::move(mutated),
                  NativeMaterialDdsError3D::UnsupportedResource))
        return 12;
    mutated = sourceBytes;
    write32(mutated, 28, 10);
    if (!rejected("mips.dds", std::move(mutated),
                  NativeMaterialDdsError3D::InvalidMipChain))
        return 13;
    mutated = sourceBytes;
    write32(mutated, 16, 0xffffffffu);
    if (!rejected("overflow.dds", std::move(mutated),
                  NativeMaterialDdsError3D::InvalidDimensions))
        return 14;
    mutated = sourceBytes;
    mutated.pop_back();
    if (!rejected("short.dds", std::move(mutated),
                  NativeMaterialDdsError3D::InvalidPayload))
        return 15;
    mutated = sourceBytes;
    mutated.push_back(0);
    if (!rejected("trailing.dds", std::move(mutated),
                  NativeMaterialDdsError3D::InvalidPayload))
        return 16;
    NativeMaterialDds3D wrongRole{};
    NativeMaterialDdsError3D wrongRoleError{};
    if (LoadNativeMaterialDds3D(sourcePath, NativeMaterialDdsRole3D::Normal,
                                4096, false, wrongRole, &wrongRoleError) ||
        wrongRoleError != NativeMaterialDdsError3D::UnsupportedFormat)
        return 17;
    NativeMaterialDds3D missingIntegrity{};
    NativeMaterialDdsError3D missingIntegrityError{};
    if (LoadNativeMaterialDds3D(sourcePath,
                                NativeMaterialDdsRole3D::BaseColor, 4096,
                                false, missingIntegrity,
                                &missingIntegrityError, {}, 0, true) ||
        missingIntegrityError != NativeMaterialDdsError3D::HashMismatch)
        return 18;
    NativeMaterialDds3D sourceIdentity{};
    if (!LoadNativeMaterialDds3D(sourcePath,
                                 NativeMaterialDdsRole3D::BaseColor, 4096,
                                 false, sourceIdentity))
        return 19;
    mutated = sourceBytes;
    mutated.back() ^= 0x5a;
    const auto sameSizePath = temporary / "same-size-mutation.dds";
    std::ofstream(sameSizePath, std::ios::binary)
        .write(reinterpret_cast<const char *>(mutated.data()),
               static_cast<std::streamsize>(mutated.size()));
    NativeMaterialDds3D sameSize{};
    NativeMaterialDdsError3D sameSizeError{};
    if (LoadNativeMaterialDds3D(
            sameSizePath, NativeMaterialDdsRole3D::BaseColor, 4096, false,
            sameSize, &sameSizeError, sourceIdentity.sha256,
            sourceBytes.size(), true) ||
        sameSizeError != NativeMaterialDdsError3D::HashMismatch)
        return 20;
    const auto junction = temporary / "linked-source";
    if (!CreateDirectoryJunction(junction, root))
        return 21;
    NativeMaterialDds3D linked{};
    NativeMaterialDdsError3D linkedError{};
    const bool linkedAccepted = LoadNativeMaterialDds3D(
        junction / L"slot-0-base.dds", NativeMaterialDdsRole3D::BaseColor,
        4096, false, linked, &linkedError, sourceIdentity.sha256,
        sourceBytes.size(), true);
    std::error_code ignored;
    std::filesystem::remove(junction, ignored);
    if (linkedAccepted || linkedError != NativeMaterialDdsError3D::InvalidPath)
        return 22;
    std::cout << "nativeDds=3 mips=33 bytes=2097708 hostile=15 strictIntegrity=passed reparse=passed\n";
}
