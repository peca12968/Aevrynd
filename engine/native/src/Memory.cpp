#include "Aevrynd/Memory.h"
#include <windows.h>
#include <psapi.h>

namespace Aevrynd {
ProcessMemoryStats QueryProcessMemoryStats() {
    PROCESS_MEMORY_COUNTERS_EX counters{};
    counters.cb = sizeof(counters);
    MEMORYSTATUSEX system{};
    system.dwLength = sizeof(system);
    if (!GetProcessMemoryInfo(GetCurrentProcess(), reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&counters), sizeof(counters)) || !GlobalMemoryStatusEx(&system)) return {};
    return {counters.WorkingSetSize,counters.PeakWorkingSetSize,counters.PrivateUsage,counters.PagefileUsage,std::size_t(system.ullAvailPhys),std::size_t(system.ullTotalPhys),system.dwMemoryLoad,true};
}

bool ShouldTrimMemory(const ProcessMemoryStats& memory,std::size_t reclaimableBytes,float secondsSinceLastTrim,MemoryPressurePolicy policy){
    if(!memory.available||!policy.thresholdPercent||policy.thresholdPercent>100||policy.cooldownSeconds<0.f)return false;
    return memory.physicalMemoryLoadPercent>=policy.thresholdPercent&&reclaimableBytes>=policy.minimumReclaimableBytes&&secondsSinceLastTrim>=policy.cooldownSeconds;
}
}
