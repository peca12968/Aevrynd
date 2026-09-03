#include "Aevrynd/Renderer.h"
#include <cstdint>
#include <iostream>
#include <type_traits>

using namespace Aevrynd;

static_assert(sizeof(GpuRetirementReceipt3D)==sizeof(std::uint64_t)*2);
static_assert(std::is_trivially_copyable_v<GpuRetirementReceipt3D>);
static_assert(!std::is_aggregate_v<GpuRetirementReceipt3D>);
static_assert(!std::is_constructible_v<GpuRetirementReceipt3D,std::uint64_t,std::uint64_t>);

int main(){
 constexpr GpuRetirementReceipt3D empty{};
 static_assert(!empty&&empty.DeviceEpoch()==0&&empty.FenceValue()==0);
 D3D12Renderer renderer;
 if(renderer.CaptureRetirementFence3D()||renderer.IsRetirementComplete3D(empty)){std::cerr<<"Uninitialized renderer accepted a retirement receipt\n";return 1;}
 return 0;
}
