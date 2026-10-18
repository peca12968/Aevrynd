#pragma once
#include <bit>
#include <cstddef>
#include <cstdint>
#include <span>
#include <stdexcept>

namespace Aevrynd {
// 先核剩餘長度才讀little-endian資料；不使用未對齊指標或檔案內未核准的容量配置。
class BoundedBinaryReader3D final {
    std::span<const std::byte> bytes_;std::size_t cursor_{};
public:
    explicit BoundedBinaryReader3D(std::span<const std::byte> bytes):bytes_(bytes){}
    std::span<const std::byte> Take(std::size_t count){
        if(count>bytes_.size()-cursor_)throw std::invalid_argument("BINARY_READER_TRUNCATED");
        const auto result=bytes_.subspan(cursor_,count);cursor_+=count;return result;
    }
    std::uint32_t Word(){const auto value=Take(4);std::uint32_t out{};
        for(unsigned b=0;b<4;++b)out|=std::uint32_t(std::to_integer<unsigned>(value[b]))<<(8*b);return out;}
    std::uint64_t Wide(){const auto low=Word();const auto high=Word();return std::uint64_t(low)|(std::uint64_t(high)<<32);}
    float Float(){return std::bit_cast<float>(Word());}
    double Double(){return std::bit_cast<double>(Wide());}
    std::size_t Remaining()const noexcept{return bytes_.size()-cursor_;}
};
}
