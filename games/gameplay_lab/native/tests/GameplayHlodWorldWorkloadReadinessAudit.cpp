#include "GameplayShowcaseEnvironment3D.h"

#include <Aevrynd/WorldDeltaPersistence3D.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <numeric>
#include <ranges>
#include <span>
#include <string>
#include <string_view>
#include <vector>

using namespace GameplayLab;

namespace {
struct SourceFile {
  std::string relativePath;
  std::string contents;
};

std::string ReadText(const std::filesystem::path &path) {
  std::ifstream input(path, std::ios::binary);
  if (!input)
    return {};
  return {std::istreambuf_iterator<char>(input),
          std::istreambuf_iterator<char>()};
}

std::size_t Count(std::string_view haystack, std::string_view needle) {
  std::size_t result{};
  for (std::size_t offset{};
       (offset = haystack.find(needle, offset)) != std::string_view::npos;
       offset += needle.size())
    ++result;
  return result;
}

std::string Hex(std::span<const std::byte> bytes) {
  constexpr char Digits[] = "0123456789abcdef";
  std::string result;
  result.reserve(bytes.size() * 2);
  for (const auto item : bytes) {
    const auto value = std::to_integer<unsigned>(item);
    result.push_back(Digits[value >> 4]);
    result.push_back(Digits[value & 15]);
  }
  return result;
}

bool ProductionSource(const std::filesystem::path &root,
                      const std::filesystem::path &path) {
  const auto relative = path.lexically_relative(root).generic_string();
  const auto extension = path.extension().string();
  return (relative.starts_with("src/") || relative.starts_with("include/")) &&
         (extension == ".cpp" || extension == ".h" || extension == ".hpp");
}
} // namespace

int main() {
  const std::filesystem::path sourceRoot = AEVRYND_SOURCE_DIR;
  const auto gameplayRoot = sourceRoot / "games/gameplay_lab/native";
  const auto captureHeader = gameplayRoot / "include/GameplayLabGame.h";
  if (!std::filesystem::is_directory(gameplayRoot) ||
      !std::filesystem::is_regular_file(captureHeader)) {
    std::cerr << "Gameplay production source inventory unavailable\n";
    return 1;
  }

  GameplayShowcaseEnvironment3D environment;
  if (!environment.Initialize(0x4a17u)) {
    std::cerr << "Production showcase workload initialization failed\n";
    return 2;
  }
  const auto receipt = environment.Receipt();
  const auto targets = GameplayShowcaseEnvironment3D::Targets();
  const std::size_t expectedDescriptors =
      targets.terrainPatches + targets.waterSegments + targets.treeInstances +
      targets.grassInstances + targets.rockInstances +
      targets.buildingComponents;
  if (environment.Descriptors().size() != expectedDescriptors ||
      receipt.integratedDescriptors != expectedDescriptors ||
      expectedDescriptors != 464) {
    std::cerr << "Production showcase workload cardinality drifted\n";
    return 3;
  }

  const auto captureSource = ReadText(captureHeader);
  constexpr std::string_view CaptureEntry =
      "MakeGameplayCaptureExpectation3D(L\"";
  const auto captureSlots = Count(captureSource, CaptureEntry);
  if (captureSlots != 11) {
    std::cerr << "Production capture route inventory drifted slots="
              << captureSlots << '\n';
    return 4;
  }

  std::vector<SourceFile> productionSources;
  for (const auto &entry : std::filesystem::recursive_directory_iterator(
           gameplayRoot, std::filesystem::directory_options::skip_permission_denied)) {
    if (!entry.is_regular_file() || !ProductionSource(gameplayRoot, entry.path()))
      continue;
    auto contents = ReadText(entry.path());
    if (contents.empty()) {
      std::cerr << "Unreadable or empty production source: "
                << entry.path().filename().string() << '\n';
      return 5;
    }
    productionSources.push_back(
        {entry.path().lexically_relative(gameplayRoot).generic_string(),
         std::move(contents)});
  }
  std::ranges::sort(productionSources, {}, &SourceFile::relativePath);
  if (productionSources.empty())
    return 6;

  // 只稽核正式 src/include；tests 中的合成 raster fixture 不得被算成 gameplay 接線。
  constexpr std::array RequiredProductionSeams{
      "HlodData3D", "HlodReferenceSelector3D",
      "TerrainVegetationWorldChunkActivationTarget3D", "WorldPartition3D",
      "CreateHlodBatchAtomic3D", "SelectHlodFrame3D", "FreezeHlodFrame3D",
      "CollectFrozenHlodFrame3D"};
  std::array<std::size_t, RequiredProductionSeams.size()> seamCounts{};
  std::vector<std::byte> canonicalSource;
  for (const auto &source : productionSources) {
    canonicalSource.insert(canonicalSource.end(),
                           reinterpret_cast<const std::byte *>(source.relativePath.data()),
                           reinterpret_cast<const std::byte *>(source.relativePath.data() +
                                                                source.relativePath.size()));
    canonicalSource.push_back(std::byte{0});
    canonicalSource.insert(canonicalSource.end(),
                           reinterpret_cast<const std::byte *>(source.contents.data()),
                           reinterpret_cast<const std::byte *>(source.contents.data() +
                                                                source.contents.size()));
    canonicalSource.push_back(std::byte{'\n'});
    for (std::size_t seam{}; seam < RequiredProductionSeams.size(); ++seam)
      seamCounts[seam] += Count(source.contents, RequiredProductionSeams[seam]);
  }
  const auto totalSeams =
      std::accumulate(seamCounts.begin(), seamCounts.end(), std::size_t{});
  // seam 存在只代表已具備接線基礎；OnRender、全類型資產與效能閘門仍須另外驗證。
  const auto seamReference = [&](std::string_view name) {
    const std::size_t index = static_cast<std::size_t>(
        std::ranges::find(RequiredProductionSeams, name) -
        RequiredProductionSeams.begin());
    return index < seamCounts.size() ? seamCounts[index] : 0u;
  };
  const bool seamReady = seamReference("HlodData3D") > 0 &&
                         seamReference("WorldPartition3D") > 0 &&
                         seamReference("SelectHlodFrame3D") > 0 &&
                         seamReference("FreezeHlodFrame3D") > 0 &&
                         seamReference("CollectFrozenHlodFrame3D") > 0;
  if (!seamReady) {
    std::cerr << "Required gameplay HLOD seam predicate is not satisfied\n";
    return 7;
  }

  std::array<std::byte, 32> sourceDigest{};
  if (!Aevrynd::WorldDeltaSha2563D(canonicalSource, sourceDigest)) {
    std::cerr << "Production source inventory SHA-256 failed\n";
    return 8;
  }

  std::ofstream output(AEVRYND_HLOD_WORKLOAD_READINESS_RECEIPT,
                       std::ios::binary | std::ios::trunc);
  if (!output)
    return 9;
  output << "{\"source\":\"GameplayHlodWorldWorkloadReadinessAudit\","
            "\"status\":\"NO-GO\",\"seamReady\":"
         << (seamReady ? 1 : 0) << ","
            "\"syntheticSubstitution\":0,\"performanceMeasured\":0,"
            "\"productionHlodSceneChain\":0,\"fullDescriptorCoverage\":0,"
         << "\"productionSourceFiles\":" << productionSources.size()
         << ",\"productionSourceSha256\":\"" << Hex(sourceDigest) << "\","
         << "\"worldHash\":" << receipt.worldHash
         << ",\"productionDescriptors\":" << expectedDescriptors
         << ",\"terrain\":" << receipt.terrain << ",\"water\":"
         << receipt.water << ",\"trees\":" << receipt.trees
         << ",\"grass\":" << receipt.grass << ",\"rocks\":"
         << receipt.rocks << ",\"buildingComponents\":"
         << receipt.buildings << ",\"productionCaptureSlots\":"
         << captureSlots << ",\"requiredSeams\":[";
  for (std::size_t index{}; index < RequiredProductionSeams.size(); ++index) {
    if (index)
      output << ',';
    output << "{\"name\":\"" << RequiredProductionSeams[index]
           << "\",\"references\":" << seamCounts[index] << '}';
  }
  output << "],\"blocker\":\"Production gameplay HLOD seam is ready for "
            "rocks, but OnRender ownership, all descriptor kinds, aggregate "
            "proxy and performance measurement remain incomplete.\"}";
  output.flush();
  if (!output)
    return 10;

  std::cout << "Gameplay HLOD real-world workload NO-GO descriptors="
            << expectedDescriptors << " captureSlots=" << captureSlots
            << " productionSources=" << productionSources.size()
            << " productionHlodSeams=" << totalSeams
            << " seamReady=" << (seamReady ? 1 : 0)
            << " performanceMeasured=0 syntheticSubstitution=0\n";
  return 0;
}
