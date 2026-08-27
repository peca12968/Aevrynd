#pragma once
#include <cstddef>
#include <cstdint>

namespace Aevrynd {
struct ProcessMemoryStats {
    std::size_t workingSetBytes{};
    std::size_t peakWorkingSetBytes{};
    std::size_t privateBytes{};
    std::size_t commitBytes{};
    std::size_t availablePhysicalBytes{};
    std::size_t totalPhysicalBytes{};
    std::uint32_t physicalMemoryLoadPercent{};
    bool available{};
};

struct MemoryPressurePolicy {
    std::uint32_t thresholdPercent{85};
    std::size_t minimumReclaimableBytes{512 * 1024};
    float cooldownSeconds{10.f};
};

struct ReusableMemoryStats {
    std::size_t retainedCapacityBytes{};
    std::size_t reclaimableCapacityBytes{};
    std::size_t lastReleasedBytes{};
    std::uint64_t trimCount{};
};

[[nodiscard]] ProcessMemoryStats QueryProcessMemoryStats();
[[nodiscard]] bool ShouldTrimMemory(const ProcessMemoryStats& memory,std::size_t reclaimableBytes,float secondsSinceLastTrim,MemoryPressurePolicy policy={},bool hasDisposableResources=false);
}
