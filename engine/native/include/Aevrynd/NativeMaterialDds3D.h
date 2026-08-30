#pragma once

#include <dxgiformat.h>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace Aevrynd {

enum class NativeMaterialDdsRole3D : std::uint8_t {
    BaseColor,
    Surface,
    Normal
};

enum class NativeMaterialDdsError3D : std::uint8_t {
    None,
    InvalidPath,
    IoFailure,
    InvalidHeader,
    UnsupportedFormat,
    UnsupportedResource,
    InvalidDimensions,
    InvalidMipChain,
    InvalidPayload,
    HashMismatch
};

struct NativeMaterialDdsMip3D {
    std::uint32_t width{}, height{};
    std::size_t rowBytes{};
    std::vector<std::uint8_t> pixels;
};

struct NativeMaterialDds3D {
    DXGI_FORMAT format{DXGI_FORMAT_UNKNOWN};
    std::uint32_t width{}, height{}, mipCount{};
    std::size_t payloadBytes{};
    std::string sha256;
    std::vector<NativeMaterialDdsMip3D> mips;
};

// 僅接納材質管線所需的 DX10 DDS；不做解碼或重新壓縮。
[[nodiscard]] bool LoadNativeMaterialDds3D(
    const std::filesystem::path &path, NativeMaterialDdsRole3D role,
    std::uint32_t maximumResolution, bool loadPayload,
    NativeMaterialDds3D &output, NativeMaterialDdsError3D *error = nullptr,
    std::string_view expectedSha256 = {},
    std::uint64_t expectedBytes = 0,
    bool requireExpectedIntegrity = false);

} // namespace Aevrynd
