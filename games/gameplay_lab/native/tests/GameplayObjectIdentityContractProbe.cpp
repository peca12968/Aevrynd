#include "GameplayLabGame.h"
#include <cstdint>
#include <iostream>

using namespace Aevrynd;
using namespace GameplayLab;

static_assert(GameplayCaptureTerrainIdentity3D.id.high ==
              GameplayEnvironmentIdentityDomain3D);
static_assert(GameplayCaptureTerrainIdentity3D.id.low == 1);
static_assert(GameplayCaptureTerrainIdentity3D.generation == 1);
static_assert((GameplayCaptureTerrainIdentity3D.id.high &
               HlodProxyIdentityDomainBit3D) == 0);
static_assert(HlodProxyIdentityDomainBit3D == (std::uint64_t{1} << 63));
static_assert(HlodProxyRenderObjectId3D(GameplayCaptureTerrainIdentity3D.id).high ==
              (GameplayCaptureTerrainIdentity3D.id.high |
               HlodProxyIdentityDomainBit3D));

int main() {
  const auto proxy = HlodProxyRenderObjectId3D(GameplayCaptureTerrainIdentity3D.id);
  std::cout << "{\"schemaVersion\":1,\"source\":\"GameplayObjectIdentityContractProbe\","
               "\"environmentHigh\":\""
            << GameplayCaptureTerrainIdentity3D.id.high
            << "\",\"terrainLow\":" << GameplayCaptureTerrainIdentity3D.id.low
            << ",\"generation\":" << GameplayCaptureTerrainIdentity3D.generation
            << ",\"hlodProxyBit\":\"" << HlodProxyIdentityDomainBit3D
            << "\",\"proxyHigh\":\"" << proxy.high
            << "\",\"contractPassed\":1}\n";
  return 0;
}
