#pragma once
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <span>

namespace Aevrynd {

inline constexpr unsigned MaximumFrameCaptureDimension3D=16384;

// 將 R8G8B8A8 GPU readback 的 row-pitch 影像以原子替換方式寫成 PNG。
bool EncodeFrameCapturePng3D(const std::filesystem::path& path,unsigned width,unsigned height,std::size_t rowPitch,std::span<const std::uint8_t> pixels);

}
