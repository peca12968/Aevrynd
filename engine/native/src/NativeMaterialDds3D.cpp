#include <Aevrynd/NativeMaterialDds3D.h>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <bcrypt.h>

#include <algorithm>
#include <array>
#include <cstring>
#include <fstream>
#include <limits>
#include <span>

namespace Aevrynd {
namespace {

#pragma pack(push, 1)
struct DdsPixelFormat3D {
    std::uint32_t size{}, flags{}, fourCc{}, rgbBitCount{};
    std::uint32_t redMask{}, greenMask{}, blueMask{}, alphaMask{};
};
struct DdsHeader3D {
    std::uint32_t size{}, flags{}, height{}, width{}, pitchOrLinearSize{};
    std::uint32_t depth{}, mipCount{};
    std::uint32_t reserved1[11]{};
    DdsPixelFormat3D pixelFormat{};
    std::uint32_t caps{}, caps2{}, caps3{}, caps4{}, reserved2{};
};
struct DdsHeaderDx10_3D {
    std::uint32_t format{}, resourceDimension{}, miscFlag{}, arraySize{};
    std::uint32_t miscFlags2{};
};
#pragma pack(pop)

static_assert(sizeof(DdsPixelFormat3D) == 32);
static_assert(sizeof(DdsHeader3D) == 124);
static_assert(sizeof(DdsHeaderDx10_3D) == 20);

constexpr std::uint32_t DdsMagic3D = 0x20534444u;
constexpr std::uint32_t DdsFourCcFlag3D = 0x4u;
constexpr std::uint32_t DdsDx10FourCc3D = 0x30315844u;
constexpr std::uint32_t DdsResourceTexture2D3D = 3u;
constexpr std::uint32_t DdsResourceMiscTextureCube3D = 0x4u;
constexpr std::uint32_t DdsCaps2CubeMask3D = 0xfe00u;
constexpr std::uint32_t DdsCaps2Volume3D = 0x200000u;
constexpr std::uint32_t DdsHeaderDepthFlag3D = 0x800000u;
constexpr std::size_t DdsPayloadOffset3D =
    sizeof(std::uint32_t) + sizeof(DdsHeader3D) + sizeof(DdsHeaderDx10_3D);
constexpr std::uint64_t MaximumNativeMaterialDdsFileBytes3D =
    32ull * 1024ull * 1024ull;

void SetError(NativeMaterialDdsError3D *error,
              NativeMaterialDdsError3D value) {
    if (error)
        *error = value;
}

bool IsHexSha256(std::string_view value) {
    return value.size() == 64 &&
           std::ranges::all_of(value, [](char item) {
               return (item >= '0' && item <= '9') ||
                      (item >= 'a' && item <= 'f');
           });
}

bool SafeRegularFile(const std::filesystem::path &path) {
    if (path.empty())
        return false;
    std::error_code ec;
    const auto absolute = std::filesystem::absolute(path, ec).lexically_normal();
    if (ec || absolute.empty())
        return false;
    std::filesystem::path current;
    for (const auto &component : absolute) {
        current /= component;
        const DWORD attributes = GetFileAttributesW(current.c_str());
        if (attributes == INVALID_FILE_ATTRIBUTES ||
            (attributes & FILE_ATTRIBUTE_REPARSE_POINT))
            return false;
    }
    const DWORD attributes = GetFileAttributesW(absolute.c_str());
    return attributes != INVALID_FILE_ATTRIBUTES &&
           !(attributes & FILE_ATTRIBUTE_DIRECTORY);
}

bool Sha256(std::span<const std::uint8_t> bytes, std::string &output) {
    BCRYPT_ALG_HANDLE algorithm{};
    BCRYPT_HASH_HANDLE hash{};
    DWORD objectBytes{}, copied{};
    std::vector<UCHAR> object;
    std::array<UCHAR, 32> digest{};
    bool passed = BCryptOpenAlgorithmProvider(
                      &algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0) >= 0;
    if (passed)
        passed = BCryptGetProperty(
                     algorithm, BCRYPT_OBJECT_LENGTH,
                     reinterpret_cast<PUCHAR>(&objectBytes), sizeof(objectBytes),
                     &copied, 0) >= 0;
    if (passed)
        object.resize(objectBytes);
    if (passed)
        passed = BCryptCreateHash(algorithm, &hash, object.data(), objectBytes,
                                  nullptr, 0, 0) >= 0;
    if (passed && !bytes.empty())
        passed = bytes.size() <= ULONG_MAX &&
                 BCryptHashData(hash, const_cast<PUCHAR>(bytes.data()),
                                static_cast<ULONG>(bytes.size()), 0) >= 0;
    if (passed)
        passed = BCryptFinishHash(hash, digest.data(),
                                  static_cast<ULONG>(digest.size()), 0) >= 0;
    if (hash)
        BCryptDestroyHash(hash);
    if (algorithm)
        BCryptCloseAlgorithmProvider(algorithm, 0);
    if (!passed)
        return false;
    constexpr char Hex[] = "0123456789abcdef";
    output.resize(64);
    for (std::size_t index = 0; index < digest.size(); ++index) {
        output[index * 2] = Hex[digest[index] >> 4];
        output[index * 2 + 1] = Hex[digest[index] & 15];
    }
    return true;
}

bool FormatAllowed(DXGI_FORMAT format, NativeMaterialDdsRole3D role) {
    switch (role) {
    case NativeMaterialDdsRole3D::BaseColor:
        return format == DXGI_FORMAT_BC7_UNORM_SRGB ||
               format == DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    case NativeMaterialDdsRole3D::Surface:
        return format == DXGI_FORMAT_BC7_UNORM ||
               format == DXGI_FORMAT_BC3_UNORM;
    case NativeMaterialDdsRole3D::Normal:
        return format == DXGI_FORMAT_BC5_UNORM;
    }
    return false;
}

} // namespace

bool LoadNativeMaterialDds3D(const std::filesystem::path &path,
                             NativeMaterialDdsRole3D role,
                             std::uint32_t maximumResolution, bool loadPayload,
                             NativeMaterialDds3D &output,
                             NativeMaterialDdsError3D *error,
                             std::string_view expectedSha256,
                             std::uint64_t expectedBytes,
                             bool requireExpectedIntegrity) {
    output = {};
    SetError(error, NativeMaterialDdsError3D::None);
    if (!SafeRegularFile(path)) {
        SetError(error, NativeMaterialDdsError3D::InvalidPath);
        return false;
    }
    if ((requireExpectedIntegrity &&
         (!expectedBytes || !IsHexSha256(expectedSha256))) ||
        (!expectedSha256.empty() && !IsHexSha256(expectedSha256))) {
        SetError(error, NativeMaterialDdsError3D::HashMismatch);
        return false;
    }
    // 寫入與刪除共享均關閉，尺寸、內容與 SHA 來自同一個固定檔案快照。
    const HANDLE file = CreateFileW(
        path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OPEN_REPARSE_POINT, nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        SetError(error, NativeMaterialDdsError3D::IoFailure);
        return false;
    }
    struct CloseFile3D {
        HANDLE value;
        ~CloseFile3D() { CloseHandle(value); }
    } close{file};
    BY_HANDLE_FILE_INFORMATION information{};
    LARGE_INTEGER size{};
    if (!GetFileInformationByHandle(file, &information) ||
        information.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT ||
        !GetFileSizeEx(file, &size) || size.QuadPart < 0 ||
        static_cast<std::uint64_t>(size.QuadPart) < DdsPayloadOffset3D ||
        static_cast<std::uint64_t>(size.QuadPart) >
            MaximumNativeMaterialDdsFileBytes3D ||
        static_cast<std::uint64_t>(size.QuadPart) >
            (std::numeric_limits<std::size_t>::max)() ||
        (expectedBytes &&
         static_cast<std::uint64_t>(size.QuadPart) != expectedBytes)) {
        SetError(error, expectedBytes ? NativeMaterialDdsError3D::HashMismatch
                                      : NativeMaterialDdsError3D::IoFailure);
        return false;
    }
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(size.QuadPart));
    std::size_t offset{};
    while (offset < bytes.size()) {
        const DWORD request = static_cast<DWORD>((std::min)(
            bytes.size() - offset, std::size_t{4u * 1024u * 1024u}));
        DWORD read{};
        if (!ReadFile(file, bytes.data() + offset, request, &read, nullptr) ||
            read != request) {
            SetError(error, NativeMaterialDdsError3D::IoFailure);
            return false;
        }
        offset += read;
    }
    std::uint8_t trailing{};
    DWORD trailingRead{};
    if (!ReadFile(file, &trailing, 1, &trailingRead, nullptr) || trailingRead) {
        SetError(error, NativeMaterialDdsError3D::IoFailure);
        return false;
    }
    std::uint32_t magic{};
    DdsHeader3D header{};
    DdsHeaderDx10_3D dx10{};
    std::memcpy(&magic, bytes.data(), sizeof(magic));
    std::memcpy(&header, bytes.data() + sizeof(magic), sizeof(header));
    std::memcpy(&dx10, bytes.data() + sizeof(magic) + sizeof(header),
                sizeof(dx10));
    if (magic != DdsMagic3D || header.size != sizeof(DdsHeader3D) ||
        header.pixelFormat.size != sizeof(DdsPixelFormat3D) ||
        header.pixelFormat.flags != DdsFourCcFlag3D ||
        header.pixelFormat.fourCc != DdsDx10FourCc3D) {
        SetError(error, NativeMaterialDdsError3D::InvalidHeader);
        return false;
    }
    const DXGI_FORMAT format = static_cast<DXGI_FORMAT>(dx10.format);
    if (!FormatAllowed(format, role)) {
        SetError(error, NativeMaterialDdsError3D::UnsupportedFormat);
        return false;
    }
    if (dx10.resourceDimension != DdsResourceTexture2D3D ||
        dx10.arraySize != 1 || dx10.miscFlag & DdsResourceMiscTextureCube3D ||
        header.caps2 & (DdsCaps2CubeMask3D | DdsCaps2Volume3D) ||
        header.flags & DdsHeaderDepthFlag3D || header.depth > 1) {
        SetError(error, NativeMaterialDdsError3D::UnsupportedResource);
        return false;
    }
    maximumResolution = (std::min)(4096u, (std::max)(1u, maximumResolution));
    if (!header.width || !header.height || header.width > maximumResolution ||
        header.height > maximumResolution) {
        SetError(error, NativeMaterialDdsError3D::InvalidDimensions);
        return false;
    }
    std::uint32_t expectedMipCount = 1;
    for (std::uint32_t width = header.width, height = header.height;
         width > 1 || height > 1;
         width = (std::max)(1u, width / 2),
                       height = (std::max)(1u, height / 2))
        ++expectedMipCount;
    if (header.mipCount != expectedMipCount) {
        SetError(error, NativeMaterialDdsError3D::InvalidMipChain);
        return false;
    }
    // 眉睫等精細遮罩可使用未壓縮 RGBA；仍要求明確 sRGB 與完整 mip。
    const bool rgba8 = format == DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    if (rgba8 && (!(header.flags & 0x8u) ||
                  header.pitchOrLinearSize != header.width * 4u)) {
        SetError(error, NativeMaterialDdsError3D::InvalidHeader);
        return false;
    }
    std::size_t payloadBytes{};
    std::uint32_t width = header.width, height = header.height;
    for (std::uint32_t mip = 0; mip < expectedMipCount; ++mip) {
        const std::size_t unitsWide = rgba8 ? std::size_t(width)
                                          : (std::size_t(width) + 3) / 4;
        const std::size_t unitsHigh = rgba8 ? std::size_t(height)
                                          : (std::size_t(height) + 3) / 4;
        const std::size_t unitBytes = rgba8 ? 4u : 16u;
        if (unitsWide > (std::numeric_limits<std::size_t>::max)() / unitBytes ||
            unitsHigh > (std::numeric_limits<std::size_t>::max)() /
                            (unitsWide * unitBytes)) {
            SetError(error, NativeMaterialDdsError3D::InvalidPayload);
            return false;
        }
        const std::size_t mipBytes = unitsWide * unitsHigh * unitBytes;
        if (payloadBytes > (std::numeric_limits<std::size_t>::max)() - mipBytes) {
            SetError(error, NativeMaterialDdsError3D::InvalidPayload);
            return false;
        }
        payloadBytes += mipBytes;
        width = (std::max)(1u, width / 2);
        height = (std::max)(1u, height / 2);
    }
    if (payloadBytes != bytes.size() - DdsPayloadOffset3D) {
        SetError(error, NativeMaterialDdsError3D::InvalidPayload);
        return false;
    }
    std::string sha256;
    if (!Sha256(bytes, sha256)) {
        SetError(error, NativeMaterialDdsError3D::IoFailure);
        return false;
    }
    if (!expectedSha256.empty() || !sha256.empty()) {
        if (!expectedSha256.empty() && sha256 != expectedSha256) {
            SetError(error, NativeMaterialDdsError3D::HashMismatch);
            return false;
        }
    }
    output.format = format;
    output.width = header.width;
    output.height = header.height;
    output.mipCount = expectedMipCount;
    output.payloadBytes = payloadBytes;
    output.sha256 = std::move(sha256);
    if (!loadPayload)
        return true;
    output.mips.reserve(expectedMipCount);
    std::size_t payloadOffset = DdsPayloadOffset3D;
    width = header.width;
    height = header.height;
    for (std::uint32_t mip = 0; mip < expectedMipCount; ++mip) {
        const std::size_t rowBytes = rgba8 ? std::size_t(width) * 4u
                                         : ((std::size_t(width) + 3) / 4) * 16;
        const std::size_t rows = rgba8 ? std::size_t(height)
                                     : (std::size_t(height) + 3) / 4;
        const std::size_t mipBytes = rowBytes * rows;
        NativeMaterialDdsMip3D item{width, height, rowBytes};
        item.pixels.assign(bytes.begin() + payloadOffset,
                           bytes.begin() + payloadOffset + mipBytes);
        output.mips.push_back(std::move(item));
        payloadOffset += mipBytes;
        width = (std::max)(1u, width / 2);
        height = (std::max)(1u, height / 2);
    }
    return true;
}

} // namespace Aevrynd
