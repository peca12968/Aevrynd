#include "GameplayShowcaseEnvironment3D.h"

#include <fstream>
#include <iostream>

using namespace GameplayLab;

int main() {
  GameplayShowcaseEnvironment3D environment;
  if (!environment.Initialize(0x4a17u)) {
    std::cerr << "Production showcase workload initialization failed\n";
    return 1;
  }

  const auto receipt = environment.Receipt();
  const auto targets = GameplayShowcaseEnvironment3D::Targets();
  const std::size_t expectedDescriptors =
      targets.terrainPatches + targets.waterSegments + targets.treeInstances +
      targets.grassInstances + targets.rockInstances +
      targets.buildingComponents;
  if (environment.Descriptors().size() != expectedDescriptors ||
      receipt.integratedDescriptors != expectedDescriptors ||
      expectedDescriptors != 520) {
    std::cerr << "Production showcase workload cardinality drifted\n";
    return 2;
  }

  std::ofstream output(AEVRYND_HLOD_WORKLOAD_READINESS_RECEIPT,
                       std::ios::binary | std::ios::trunc);
  if (!output)
    return 3;

  // 此 target 沒有連結 GameplayHlodWorldRuntime3D，也沒有可靠的同輪 runtime
  // receipt 輸入。只回報實際建立的 workload；未量測欄位保持 null，不能以
  // source 字串或人工組裝的 receipt 冒充 runtime 通過。
  output << "{\"source\":\"GameplayHlodWorldWorkloadReadinessAudit\","
            "\"status\":\"NO-GO\",\"runtimeEvidenceConsumed\":false,"
            "\"runtimeReadiness\":null,\"runtimeTerminalReason\":null,"
            "\"worldPartitionResident\":null,\"sceneEcsActivated\":null,"
            "\"selectFreezeCollect\":null,\"registryOwner\":null,"
            "\"transactionalStagingWip\":null,\"negativeCases\":null,"
            "\"performanceMeasured\":false,\"syntheticSubstitution\":false,"
            "\"productionWorkload\":{\"worldHash\":"
         << receipt.worldHash << ",\"productionDescriptors\":"
         << expectedDescriptors << ",\"terrain\":" << receipt.terrain
         << ",\"water\":" << receipt.water << ",\"trees\":"
         << receipt.trees << ",\"grass\":" << receipt.grass
         << ",\"rocks\":" << receipt.rocks
         << ",\"buildingComponents\":" << receipt.buildings
         << "},\"blocker\":\"This audit target cannot execute or consume "
            "same-run GameplayHlodWorldRuntime3D evidence within its owned "
            "build topology; runtime readiness claims are intentionally "
            "withheld.\"}";
  output.flush();
  if (!output)
    return 4;

  std::cout << "Gameplay HLOD workload-only NO-GO descriptors="
            << expectedDescriptors
            << " runtimeEvidenceConsumed=0 performanceMeasured=0\n";
  return 0;
}
