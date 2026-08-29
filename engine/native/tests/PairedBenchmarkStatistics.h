#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <vector>

namespace Aevrynd::Test {

struct PairedEnvironmentStatistics {
  double blockDrift{};
  double outlierFraction{};
  std::size_t longestOutlierBurst{};
  bool valid{};
};

template <class Value> Value LowerPercentile(std::vector<Value> values,
                                              std::size_t percentile) {
  if (values.empty())
    return {};
  std::ranges::sort(values);
  return values[(values.size() - 1) * percentile / 100];
}

inline std::uint64_t MedianAbsoluteDeviation(
    const std::vector<std::uint64_t> &values) {
  if (values.empty())
    return {};
  const auto median = LowerPercentile(values, 50);
  std::vector<std::uint64_t> deviations;
  deviations.reserve(values.size());
  for (const auto value : values)
    deviations.push_back(value > median ? value - median : median - value);
  return LowerPercentile(std::move(deviations), 50);
}

inline PairedEnvironmentStatistics AnalyzePairedEnvironment(
    const std::vector<std::uint64_t> &controls, std::size_t blockSize = 5) {
  PairedEnvironmentStatistics result{};
  if (blockSize < 3 || controls.size() < blockSize * 3 ||
      std::ranges::any_of(controls, [](auto value) { return value == 0; }))
    return result;

  const auto median = LowerPercentile(controls, 50);
  if (!median)
    return result;
  std::vector<std::uint64_t> blocks;
  for (std::size_t begin = 0; begin + blockSize <= controls.size();
       begin += blockSize) {
    blocks.push_back(LowerPercentile(
        std::vector<std::uint64_t>(controls.begin() + begin,
                                   controls.begin() + begin + blockSize),
        50));
  }
  const auto half = blocks.size() / 2;
  const auto early = LowerPercentile(
      std::vector<std::uint64_t>(blocks.begin(), blocks.begin() + half), 50);
  const auto late = LowerPercentile(
      std::vector<std::uint64_t>(blocks.end() - half, blocks.end()), 50);
  const double rawBlockDrift =
      std::abs(double(late) - double(early)) / double(early);
  std::size_t rising{}, falling{};
  for (std::size_t index = 1; index < blocks.size(); ++index) {
    rising += blocks[index] > blocks[index - 1];
    falling += blocks[index] < blocks[index - 1];
  }
  const double directionalFraction =
      double((std::max)(rising, falling)) / double(blocks.size() - 1);
  // block 位準跳動屬排程 phase；只有具一致方向的 block 漂移才視為環境趨勢。
  result.blockDrift = directionalFraction >= .70 ? rawBlockDrift : 0.;

  const auto mad = MedianAbsoluteDeviation(controls);
  const double outlierLimit =
      (std::max)(double(median) * .05, double(mad) * 6.0);
  std::size_t outliers{}, burst{}, longest{};
  for (std::size_t index = 0; index < controls.size(); ++index) {
    if (std::abs(double(controls[index]) - double(median)) > outlierLimit) {
      ++outliers;
      longest = (std::max)(longest, ++burst);
    } else {
      burst = 0;
    }
  }
  result.outlierFraction = double(outliers) / double(controls.size());
  result.longestOutlierBurst = longest;
  result.valid = result.blockDrift <= .03 && result.outlierFraction <= .05 &&
                 result.longestOutlierBurst <= 2;
  return result;
}

inline bool PairedCpuRegressionGate(
    const std::vector<std::uint64_t> &off,
    const std::vector<std::int64_t> &delta, double maximumRegression = .05) {
  if (off.empty() || off.size() != delta.size())
    return false;
  const auto baseline = LowerPercentile(off, 50);
  return baseline && LowerPercentile(delta, 50) <=
                         std::int64_t(double(baseline) * maximumRegression);
}

} // namespace Aevrynd::Test
