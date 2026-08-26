#include "Aevrynd/Memory.h"
#include <windows.h>
#include <psapi.h>

namespace Aevrynd {
ProcessMemoryStats QueryProcessMemoryStats() {
    PROCESS_MEMORY_COUNTERS_EX counters{};
    counters.cb = sizeof(counters);
    if (!GetProcessMemoryInfo(GetCurrentProcess(), reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&counters), sizeof(counters))) return {};
    return {counters.WorkingSetSize, counters.PeakWorkingSetSize, counters.PrivateUsage, counters.PagefileUsage, true};
}
}
