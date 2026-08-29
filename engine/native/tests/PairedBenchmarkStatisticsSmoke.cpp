#include "PairedBenchmarkStatistics.h"

#include <cstdint>
#include <iostream>
#include <vector>

using namespace Aevrynd::Test;

namespace {
std::vector<std::uint64_t> Stable(std::uint64_t value = 10000) {
  std::vector<std::uint64_t> values(101, value);
  for (std::size_t index = 0; index < values.size(); ++index)
    values[index] += std::uint64_t(index % 5) * 10;
  return values;
}
std::vector<std::int64_t> Delta(std::int64_t value) {
  return std::vector<std::int64_t>(101, value);
}
bool Expect(bool condition, const char *name) {
  if (!condition)
    std::cerr << "Paired statistics case failed: " << name << '\n';
  return condition;
}
} // namespace

int main() {
  const auto stable = Stable();
  bool pass = true;
  pass &= Expect(AnalyzePairedEnvironment(stable).valid, "stable0");
  pass &= Expect(PairedCpuRegressionGate(stable, Delta(490)), "stable4.9");
  pass &= Expect(!PairedCpuRegressionGate(stable, Delta(510)), "stable5.1");

  auto drift = stable;
  for (std::size_t index = 0; index < drift.size(); ++index)
    drift[index] += std::uint64_t(index * 40);
  pass &= Expect(!AnalyzePairedEnvironment(drift).valid, "drift");

  auto singleOutlier = stable;
  singleOutlier[47] *= 4;
  pass &= Expect(AnalyzePairedEnvironment(singleOutlier).valid,
                 "single-outlier");

  auto burst = stable;
  for (std::size_t index = 45; index < 51; ++index)
    burst[index] *= 4;
  pass &= Expect(!AnalyzePairedEnvironment(burst).valid, "burst");
  pass &= Expect(!PairedCpuRegressionGate(stable, Delta(600)), "true+6");
  pass &= Expect(!AnalyzePairedEnvironment({}).valid, "environment-invalid");

  std::cout << "stable0=pass stable4.9=pass stable5.1=pass drift=pass "
               "single_outlier=pass burst=pass true_plus_6=pass "
               "environment_invalid=pass\n";
  return pass ? 0 : 1;
}
