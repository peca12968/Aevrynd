#pragma once
#include <cstddef>

namespace Aevrynd {
struct DepthEntry {
    float depth{};
    std::size_t index{};
    std::size_t group{};
};

// 由遠到近穩定排序，同深度物件維持遊戲提交的先後關係。
template<class Entries>
constexpr void SortDepthEntries(Entries& entries) {
    for (std::size_t index = 1; index < entries.size(); ++index) {
        DepthEntry current = entries[index];
        std::size_t destination = index;
        while (destination > 0 && current.depth < entries[destination - 1].depth) {
            entries[destination] = entries[destination - 1];
            --destination;
        }
        entries[destination] = current;
    }
}
}
