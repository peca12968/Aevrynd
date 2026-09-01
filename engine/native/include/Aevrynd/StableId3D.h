#pragma once
#include <compare>
#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace Aevrynd {
// 跨存檔、網路與重播的 128-bit 識別；排序固定為 high 再 low 的字典序。
struct StableObjectId3D {
    std::uint64_t high{},low{};
    explicit constexpr operator bool()const noexcept{return high!=0||low!=0;}
    constexpr auto operator<=>(const StableObjectId3D&)const=default;
};

// 固定以 little-endian scalar bytes 雜湊；不讀 padding、位址或容器狀態。
[[nodiscard]] constexpr std::uint64_t StableObjectIdScalarHash3D(StableObjectId3D id)noexcept{
    std::uint64_t hash=1469598103934665603ull;
    for(unsigned scalarIndex=0;scalarIndex<2;++scalarIndex){const std::uint64_t scalar=scalarIndex?id.low:id.high;for(unsigned byte=0;byte<8;++byte){hash^=(scalar>>(byte*8))&255u;hash*=1099511628211ull;}}
    return hash;
}
struct StableObjectIdHash3D {
    [[nodiscard]] constexpr std::size_t operator()(StableObjectId3D id)const noexcept{return static_cast<std::size_t>(StableObjectIdScalarHash3D(id));}
};
static_assert(sizeof(StableObjectId3D)==16);
static_assert(std::is_standard_layout_v<StableObjectId3D>);
static_assert(std::is_trivially_copyable_v<StableObjectId3D>);
}
