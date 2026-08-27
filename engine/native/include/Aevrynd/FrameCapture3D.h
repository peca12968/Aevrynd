#pragma once
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <span>

namespace Aevrynd {

inline constexpr unsigned MaximumFrameCaptureDimension3D=16384;
inline constexpr std::size_t MaximumFrameAnalysisSamples3D=262144;

struct FrameCaptureAnalysis3D {
    bool valid{};
    std::size_t sampledPixels{};
    double averageLuminance{},luminanceVariance{};
    double darkPixelFraction{},brightPixelFraction{};
};

// 將 R8G8B8A8 GPU readback 的 row-pitch 影像以原子替換方式寫成 PNG。
bool EncodeFrameCapturePng3D(const std::filesystem::path& path,unsigned width,unsigned height,std::size_t rowPitch,std::span<const std::uint8_t> pixels);
// 以固定樣本上限分析畫面曝光與動態範圍，避免自主渲染驗證本身造成無界 CPU 成本。
FrameCaptureAnalysis3D AnalyzeFrameCaptureRgba8(unsigned width,unsigned height,std::size_t rowPitch,std::span<const std::uint8_t> pixels);

}
