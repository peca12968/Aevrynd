#pragma once
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <span>
#include <array>

namespace Aevrynd {

inline constexpr unsigned MaximumFrameCaptureDimension3D=16384;
inline constexpr std::size_t MaximumFrameAnalysisSamples3D=262144;

struct FrameCaptureAnalysis3D {
    bool valid{};
    std::size_t sampledPixels{};
    double averageLuminance{},luminanceVariance{};
    double darkPixelFraction{},brightPixelFraction{};
    double linearAverageLuminance{};
    double linearLuminanceP05{},linearLuminanceP50{},linearLuminanceP95{},linearLuminanceP99{};
};

// 可選 CPU 牆鐘量測；WIC 壓縮與暫存檔寫入合併，沒有 GPU 時間。
struct FrameCaptureCost3D {
    std::uint64_t setupNanoseconds{},encodeWriteNanoseconds{},publishNanoseconds{},verifyHashNanoseconds{},totalNanoseconds{};
    bool complete{};
};

struct PublishedFrameCapture3D {
    std::array<std::byte,32> finalFileSha256{};
    std::uint64_t finalFileBytes{};
    [[nodiscard]] bool Complete() const noexcept {
        return finalFileBytes > 0;
    }
};

// 將 R8G8B8A8 GPU readback 的 row-pitch 影像以原子替換方式寫成 PNG。
bool EncodeFrameCapturePng3D(const std::filesystem::path& path,unsigned width,unsigned height,std::size_t rowPitch,std::span<const std::uint8_t> pixels);
// 原子替換後釘選最終路徑的檔案控制代碼，並在雜湊前後重驗身分與長度。
bool EncodeFrameCapturePng3D(const std::filesystem::path& path,unsigned width,unsigned height,std::size_t rowPitch,std::span<const std::uint8_t> pixels,PublishedFrameCapture3D& publication,FrameCaptureCost3D* cost=nullptr);
// 重新釘選最終檔案並核對長度與 SHA-256，供收據驗證拒絕竄改或替換。
bool ValidatePublishedFrameCapture3D(const std::filesystem::path& path,const PublishedFrameCapture3D& publication);
// 以固定樣本上限分析畫面曝光與動態範圍，避免自主渲染驗證本身造成無界 CPU 成本。
FrameCaptureAnalysis3D AnalyzeFrameCaptureRgba8(unsigned width,unsigned height,std::size_t rowPitch,std::span<const std::uint8_t> pixels);

}
