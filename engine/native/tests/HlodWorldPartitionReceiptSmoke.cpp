#include <Aevrynd/WorldPartitionTerrainVegetationAdapter3D.h>
#include <iostream>
#include <unordered_set>

using namespace Aevrynd;

class FakeReceiptSource final : public IGpuRetirementReceiptSource3D {
public:
  GpuRetirementReceipt3D CaptureGraphicsRetirementReceipt3D() override {
    if (failCapture)
      return {};
    return MakeReceipt(epoch, nextFence++);
  }
  bool IsGraphicsRetirementComplete3D(
      GpuRetirementReceipt3D receipt) const override {
    return receipt.DeviceEpoch() == epoch && complete.contains(receipt.FenceValue());
  }
  std::uint64_t epoch{11}, nextFence{1};
  bool failCapture{};
  std::unordered_set<std::uint64_t> complete{};
};

int main() {
  FakeReceiptSource source;
  HlodRetirementGate3D first, second;
  if (!first.Begin(source) || !second.Begin(source) ||
      first.Receipt().FenceValue() == second.Receipt().FenceValue())
    return 1;
  bool firstReleased{}, secondReleased{};
  source.complete.insert(second.Receipt().FenceValue());
  if (first.Complete(source) || !second.Complete(source))
    return 2;
  secondReleased = second.Complete(source);
  if (firstReleased || !secondReleased)
    return 3;
  ++source.epoch;
  source.complete.insert(first.Receipt().FenceValue());
  if (first.Complete(source))
    return 4; // device epoch mismatch 必須 fail-closed。
  source.failCapture = true;
  HlodRetirementGate3D failed;
  if (failed.Begin(source) || failed.Receipt())
    return 5;
  source.failCapture = false;
  if (!failed.Begin(source) || !failed.Receipt())
    return 6;
  std::cout << "HLOD World Partition receipt gate passed\n";
}
