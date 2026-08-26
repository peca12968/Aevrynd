#pragma once
#include <cstddef>

namespace Aevrynd {
struct ProcessMemoryStats {
    std::size_t workingSetBytes{};
    std::size_t peakWorkingSetBytes{};
    std::size_t privateBytes{};
    std::size_t commitBytes{};
    bool available{};
};

[[nodiscard]] ProcessMemoryStats QueryProcessMemoryStats();
}
