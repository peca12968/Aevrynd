#include <Aevrynd/StaticMeshAssetLibrary3D.h>
#include <algorithm>
#include <atomic>
#include <charconv>
#include <cmath>
#include <cwctype>
#include <limits>
#include <new>
#include <sstream>
#include <utility>
#include <vector>

namespace Aevrynd {
namespace {

bool SameColor(Color left, Color right) {
  return left.r == right.r && left.g == right.g && left.b == right.b &&
         left.a == right.a;
}
bool SameSurface(Surface3D left, Surface3D right) {
  return left.roughness == right.roughness && left.metallic == right.metallic &&
         left.emissive == right.emissive && left.occlusion == right.occlusion;
}
bool SameObjOptions(const ObjLoadOptions3D &left,
                    const ObjLoadOptions3D &right) {
  return SameColor(left.defaultColor, right.defaultColor) &&
         left.convertRightHandedToLeftHanded ==
             right.convertRightHandedToLeftHanded;
}
bool SameLayer(const MaterialTextureLayerDefinition3D &left,
               const MaterialTextureLayerDefinition3D &right) {
  return left.baseColorPath == right.baseColorPath &&
         left.roughnessPath == right.roughnessPath &&
         left.normalPath == right.normalPath &&
         left.surfaceEncoding == right.surfaceEncoding;
}
bool SameMaterial(const MaterialAssetDefinition3D &left,
                  const MaterialAssetDefinition3D &right) {
  return left.name == right.name &&
         left.textures.baseColorPath == right.textures.baseColorPath &&
         left.textures.roughnessPath == right.textures.roughnessPath &&
         left.textures.normalPath == right.textures.normalPath &&
         left.textures.maximumResolution == right.textures.maximumResolution &&
         left.textures.alphaMode == right.textures.alphaMode &&
         left.textures.alphaCutoff == right.textures.alphaCutoff &&
         left.textures.surfaceEncoding == right.textures.surfaceEncoding &&
         SameLayer(left.textures.additionalLayers[0],
                   right.textures.additionalLayers[0]) &&
         SameLayer(left.textures.additionalLayers[1],
                   right.textures.additionalLayers[1]) &&
         SameColor(left.material.tint, right.material.tint) &&
         SameSurface(left.material.surface, right.material.surface) &&
         left.material.castsShadow == right.material.castsShadow &&
         left.material.textureScale == right.material.textureScale &&
         left.material.baseTextureScale == right.material.baseTextureScale &&
         left.material.detailTextureScale ==
             right.material.detailTextureScale &&
         left.material.normalScale == right.material.normalScale &&
         left.material.useUv == right.material.useUv &&
         left.material.materialClass == right.material.materialClass &&
         left.material.shadingModel == right.material.shadingModel &&
         left.material.alphaMode == right.material.alphaMode &&
         left.material.alphaCutoff == right.material.alphaCutoff &&
         left.material.doubleSided == right.material.doubleSided;
}
bool SameDefinition(const StaticMeshAssetDefinition3D &left,
                    const StaticMeshAssetDefinition3D &right) {
  if (left.name != right.name || left.meshPath != right.meshPath ||
      !SameObjOptions(left.objOptions, right.objOptions) ||
      !SameMaterial(left.material, right.material) ||
      left.lodSettings.targetTriangleRatios !=
          right.lodSettings.targetTriangleRatios ||
      left.lodSettings.generatedLevelCount !=
          right.lodSettings.generatedLevelCount ||
      left.lodSettings.maximumGridResolution !=
          right.lodSettings.maximumGridResolution ||
      left.lodSettings.reorderForVertexCache !=
          right.lodSettings.reorderForVertexCache ||
      left.lodSettings.vertexCacheSize != right.lodSettings.vertexCacheSize ||
      left.lodSettings.maximumTotalOutputTriangles !=
          right.lodSettings.maximumTotalOutputTriangles ||
      left.lodSwitchDistances != right.lodSwitchDistances ||
      left.lodTransitionWidths != right.lodTransitionWidths ||
      left.lodDistanceMetric != right.lodDistanceMetric ||
      left.cpuRetention != right.cpuRetention ||
      left.generateLods != right.generateLods ||
      left.importedLodCount != right.importedLodCount)
    return false;
  for (std::uint32_t index = 0; index < left.importedLodCount; ++index)
    if (left.importedLods[index].meshPath !=
            right.importedLods[index].meshPath ||
        !SameObjOptions(left.importedLods[index].objOptions,
                        right.importedLods[index].objOptions))
      return false;
  return true;
}
bool ValidLodPresentation(const StaticMeshAssetDefinition3D &definition) {
  if (definition.importedLodCount > MaximumGeneratedMeshLodLevels3D ||
      definition.generateLods && definition.importedLodCount)
    return false;
  for (std::uint32_t index = 0; index < MaximumGeneratedMeshLodLevels3D;
       ++index)
    if ((index < definition.importedLodCount) ==
        definition.importedLods[index].meshPath.empty())
      return false;
  const std::uint32_t transitions =
      definition.generateLods ? definition.lodSettings.generatedLevelCount
                              : definition.importedLodCount;
  if (definition.generateLods &&
      (!transitions || transitions > MaximumGeneratedMeshLodLevels3D))
    return false;
  float previous{}, previousEnd = -std::numeric_limits<float>::infinity();
  for (std::uint32_t index = 0; index < transitions; ++index) {
    const float distance = definition.lodSwitchDistances[index],
                width = definition.lodTransitionWidths[index];
    if (!std::isfinite(distance) || distance <= previous ||
        !std::isfinite(width) || width < 0.f)
      return false;
    const float start = distance - width * .5f, end = distance + width * .5f;
    if (!std::isfinite(start) || !std::isfinite(end) || start < previousEnd)
      return false;
    previous = distance;
    previousEnd = end;
  }
  return definition.lodDistanceMetric == MeshLodDistanceMetric3D::Center ||
         definition.lodDistanceMetric == MeshLodDistanceMetric3D::BoundsSurface;
}
std::filesystem::path ResolvePath(const std::filesystem::path &root,
                                  const std::filesystem::path &value) {
  if (value.empty())
    return {};
  std::error_code error;
  std::filesystem::path resolved = value.is_relative() ? root / value : value;
  const std::filesystem::path canonical =
      std::filesystem::weakly_canonical(resolved, error);
  return error ? resolved.lexically_normal() : canonical;
}
bool IsWithin(const std::filesystem::path &root,
              const std::filesystem::path &candidate) {
  auto rootIt = root.begin(), candidateIt = candidate.begin();
  for (; rootIt != root.end(); ++rootIt, ++candidateIt)
    if (candidateIt == candidate.end() || *rootIt != *candidateIt)
      return false;
  return true;
}
bool Finite(Vec3 value) {
  return std::isfinite(value.x) && std::isfinite(value.y) &&
         std::isfinite(value.z);
}
bool ValidTransform(Transform3D value) {
  return Finite(value.position) && Finite(value.rotation) &&
         Finite(value.scale);
}
MeshLoadResult3D LoadMesh(const std::filesystem::path &path,
                          const ObjLoadOptions3D &options) {
  std::wstring extension = path.extension().wstring();
  std::ranges::transform(extension, extension.begin(), [](wchar_t value) {
    return wchar_t(std::towlower(value));
  });
  return extension == L".obj" ? LoadObjMeshFile(path, options)
         : extension == L".aemesh"
             ? LoadAevryndMeshFile(path)
             : MeshLoadResult3D{{}, "Unsupported static mesh extension"};
}
bool CompatibleBounds(const MeshBounds3D &source, const MeshBounds3D &candidate,
                      const StaticMeshAssetLibraryLimits3D &limits) {
  if (!Finite(source.center) || !Finite(source.extents) ||
      !std::isfinite(source.radius) || source.radius <= 0.f ||
      !Finite(candidate.center) || !Finite(candidate.extents) ||
      !std::isfinite(candidate.radius) || candidate.radius <= 0.f)
    return false;
  if (Length(candidate.center - source.center) >
      (std::max)(.001f, source.radius * limits.maximumBoundsCenterDriftRatio))
    return false;
  const auto compatible = [&](float original, float value) {
    if (original <= .00001f)
      return std::abs(value - original) <= .001f;
    const float ratio = value / original;
    return std::isfinite(ratio) && ratio >= limits.minimumBoundsExtentRatio &&
           ratio <= limits.maximumBoundsExtentRatio;
  };
  return compatible(source.extents.x, candidate.extents.x) &&
         compatible(source.extents.y, candidate.extents.y) &&
         compatible(source.extents.z, candidate.extents.z);
}
void TransferMeshChain(MeshLodResourceChain3D &source,
                       MeshLodResourceChain3D &destination) noexcept {
  destination.levelCount = source.levelCount;
  destination.processing = source.processing;
  for (std::uint32_t index = 0; index < source.levelCount; ++index)
    destination.levels[index] = std::move(source.levels[index]);
  source.levels = {};
  source.levelCount = 0;
  source.processing = {};
}
std::uint64_t NextAssetLeaseIdentity() noexcept {
  static std::atomic<std::uint64_t> next{1};
  std::uint64_t value = next.fetch_add(1, std::memory_order_relaxed);
  if (!value)
    value = next.fetch_add(1, std::memory_order_relaxed);
  return value;
}
std::uint64_t NextAssetLibraryOwnerIdentity() noexcept {
  static std::atomic<std::uint64_t> next{1};
  std::uint64_t value = next.fetch_add(1, std::memory_order_relaxed);
  if (!value)
    value = next.fetch_add(1, std::memory_order_relaxed);
  return value;
}
void HashBytes(std::uint64_t &hash, const void *data, std::size_t bytes) {
  const auto *values = static_cast<const std::uint8_t *>(data);
  for (std::size_t index = 0; index < bytes; ++index) {
    hash ^= values[index];
    hash *= 1099511628211ull;
  }
}
std::uint64_t MeshContentIdentity(std::span<const Mesh3D> meshes) {
  std::uint64_t hash = 1469598103934665603ull;
  for (const auto &mesh : meshes) {
    HashBytes(hash, mesh.vertices.data(),
              mesh.vertices.size() * sizeof(Vertex3D));
    HashBytes(hash, mesh.indices.data(),
              mesh.indices.size() * sizeof(std::uint32_t));
  }
  return hash ? hash : 1;
}
std::string CanonicalKey(const StaticMeshAssetDefinition3D &value,
                         std::uint64_t content) {
  std::ostringstream out;
  out << value.meshPath.generic_string() << '|'
      << value.objOptions.convertRightHandedToLeftHanded << '|'
      << value.objOptions.defaultColor.r << ','
      << value.objOptions.defaultColor.g << ','
      << value.objOptions.defaultColor.b << ','
      << value.objOptions.defaultColor.a << '|' << value.generateLods << '|'
      << value.importedLodCount << '|' << int(value.cpuRetention) << '|'
      << int(value.lodDistanceMetric) << '|'
      << value.lodSettings.generatedLevelCount << '|'
      << value.lodSettings.maximumGridResolution << '|'
      << value.lodSettings.reorderForVertexCache << '|'
      << value.lodSettings.vertexCacheSize << '|'
      << value.lodSettings.maximumTotalOutputTriangles;
  for (float item : value.lodSettings.targetTriangleRatios)
    out << '|' << item;
  for (float item : value.lodSwitchDistances)
    out << '|' << item;
  for (float item : value.lodTransitionWidths)
    out << '|' << item;
  for (std::uint32_t index = 0; index < value.importedLodCount; ++index)
    out << '|' << value.importedLods[index].meshPath.generic_string() << '|'
        << value.importedLods[index].objOptions.convertRightHandedToLeftHanded;
  const auto &textures = value.material.textures;
  out << '|' << textures.baseColorPath.generic_string() << '|'
      << textures.roughnessPath.generic_string() << '|'
      << textures.normalPath.generic_string() << '|'
      << textures.maximumResolution << '|' << int(textures.alphaMode) << '|'
      << textures.alphaCutoff << '|' << int(textures.surfaceEncoding);
  for (const auto &layer : textures.additionalLayers)
    out << '|' << layer.baseColorPath.generic_string() << '|'
        << layer.roughnessPath.generic_string() << '|'
        << layer.normalPath.generic_string() << '|'
        << int(layer.surfaceEncoding);
  const auto &material = value.material.material;
  out << '|' << material.tint.r << ',' << material.tint.g << ','
      << material.tint.b << ',' << material.tint.a << '|'
      << material.surface.roughness << ',' << material.surface.metallic << ','
      << material.surface.emissive << ',' << material.surface.occlusion << '|'
      << material.castsShadow << '|' << material.textureScale << '|'
      << material.baseTextureScale << '|' << material.detailTextureScale << '|'
      << material.normalScale << '|' << material.useUv << '|'
      << int(material.materialClass) << '|' << int(material.shadingModel) << '|'
      << int(material.alphaMode) << '|' << material.alphaCutoff << '|'
      << material.doubleSided << '|' << content;
  return out.str();
}

bool CheckedAdd(std::size_t left, std::size_t right, std::size_t &output) {
  if (right > (std::numeric_limits<std::size_t>::max)() - left)
    return false;
  output = left + right;
  return true;
}
bool CheckedMultiply(std::size_t left, std::size_t right,
                     std::size_t &output) {
  if (left && right > (std::numeric_limits<std::size_t>::max)() / left)
    return false;
  output = left * right;
  return true;
}
void AppendHex64(std::string &output, std::uint64_t value) {
  char digits[16]{};
  auto converted = std::to_chars(std::begin(digits), std::end(digits), value, 16);
  const std::size_t count = std::size_t(converted.ptr - digits);
  output.append(16 - count, '0');
  output.append(digits, count);
}
template <class Integer> void AppendDecimal(std::string &output, Integer value) {
  char digits[32]{};
  const auto converted = std::to_chars(std::begin(digits), std::end(digits), value);
  output.append(digits, std::size_t(converted.ptr - digits));
}
std::optional<std::string> HlodProxyLogicalName(
    StableObjectId3D worldId, GlobalChunkCoordinate3D chunk,
    HlodClusterId3D clusterId, std::uint64_t generation) {
  if (!worldId || !clusterId || !generation)
    return std::nullopt;
  try {
    std::string result;
    result.reserve(MaximumHlodProxyLogicalNameBytes3D);
    result = "hlod/";
    AppendHex64(result, worldId.high);
    result.push_back('-');
    AppendHex64(result, worldId.low);
    result.push_back('/');
    AppendDecimal(result, chunk.x);
    result.push_back('-');
    AppendDecimal(result, chunk.y);
    result.push_back('-');
    AppendDecimal(result, chunk.z);
    result.push_back('/');
    AppendHex64(result, clusterId.high);
    result.push_back('-');
    AppendHex64(result, clusterId.low);
    result += "/g";
    AppendDecimal(result, generation);
    if (result.size() > MaximumHlodProxyLogicalNameBytes3D)
      return std::nullopt;
    return result;
  } catch (const std::bad_alloc &) {
    return std::nullopt;
  }
}
std::uint64_t HlodPreparedIdentity(StableObjectId3D worldId,GlobalChunkCoordinate3D chunk,HlodClusterId3D clusterId,std::uint64_t generation,std::string_view name){
  std::uint64_t hash=1469598103934665603ull;const auto scalar=[&](std::uint64_t value){for(unsigned byte=0;byte<8;++byte){hash^=(value>>(byte*8))&255u;hash*=1099511628211ull;}};scalar(worldId.high);scalar(worldId.low);scalar(static_cast<std::uint64_t>(chunk.x));scalar(static_cast<std::uint64_t>(chunk.y));scalar(static_cast<std::uint64_t>(chunk.z));scalar(clusterId.high);scalar(clusterId.low);scalar(generation);for(unsigned char value:name){hash^=value;hash*=1099511628211ull;}return hash?hash:1;
}
bool ValidHlodProxyLogicalName(std::string_view value,
                               HlodClusterId3D clusterId,
                               std::uint64_t generation) {
  if (!clusterId || !generation || value.empty() ||
      value.size() > MaximumHlodProxyLogicalNameBytes3D ||
      !value.starts_with("hlod/"))
    return false;
  const auto lastSlash = value.rfind('/');
  if (lastSlash == std::string_view::npos || lastSlash + 2 >= value.size() ||
      value[lastSlash + 1] != 'g')
    return false;
  std::uint64_t parsedGeneration{};
  const auto generationText = value.substr(lastSlash + 2);
  const auto generationResult = std::from_chars(
      generationText.data(), generationText.data() + generationText.size(),
      parsedGeneration);
  if (generationResult.ec != std::errc{} ||
      generationResult.ptr != generationText.data() + generationText.size() ||
      parsedGeneration != generation ||
      (generationText.size() > 1 && generationText.front() == '0'))
    return false;
  std::string expectedCluster;
  try {
    expectedCluster.reserve(33);
    AppendHex64(expectedCluster, clusterId.high);
    expectedCluster.push_back('-');
    AppendHex64(expectedCluster, clusterId.low);
  } catch (const std::bad_alloc &) {
    return false;
  }
  const auto clusterSlash = value.rfind('/', lastSlash - 1);
  if (clusterSlash == std::string_view::npos ||
      value.substr(clusterSlash + 1, lastSlash - clusterSlash - 1) !=
          expectedCluster)
    return false;
  const auto chunkSlash = value.rfind('/', clusterSlash - 1);
  if (chunkSlash == std::string_view::npos || chunkSlash <= 5)
    return false;
  const auto world = value.substr(5, chunkSlash - 5);
  if (world.size() != 33 || world[16] != '-')
    return false;
  for (std::size_t index = 0; index < world.size(); ++index)
    if (index != 16 && !((world[index] >= '0' && world[index] <= '9') ||
                         (world[index] >= 'a' && world[index] <= 'f')))
      return false;
  const auto chunk = value.substr(chunkSlash + 1, clusterSlash - chunkSlash - 1);
  std::size_t begin{};
  for (unsigned component = 0; component < 3; ++component) {
    std::size_t end = component == 2 ? chunk.size() : chunk.find('-', begin + (begin < chunk.size() && chunk[begin] == '-' ? 1 : 0));
    if (end == std::string_view::npos || end <= begin)
      return false;
    const auto text = chunk.substr(begin, end - begin);
    std::int64_t parsed{};
    const auto converted = std::from_chars(text.data(), text.data() + text.size(), parsed);
    if (converted.ec != std::errc{} || converted.ptr != text.data() + text.size() ||
        (text.size() > 1 && text.front() == '0') ||
        (text.size() > 2 && text[0] == '-' && text[1] == '0'))
      return false;
    begin = end + 1;
  }
  return begin == chunk.size() + 1;
}

} // namespace

StaticMeshAssetLibrary3D::OperationGuard::OperationGuard(
    const StaticMeshAssetLibrary3D &owner, bool shutdown) noexcept
    : owner_(&owner), shutdown_(shutdown),
      active_(owner.BeginOperation(shutdown)) {}

StaticMeshAssetLibrary3D::OperationGuard::~OperationGuard() {
  if (active_)
    owner_->EndOperation(shutdown_);
}

bool StaticMeshAssetLibrary3D::BeginOperation(bool shutdown) const noexcept {
  try {
    std::unique_lock lock{operationMutex_};
    const auto caller = std::this_thread::get_id();
    if (operationActive_ && operationOwner_ == caller) {
      ++operationDepth_;
      return true;
    }
    if (shutdown) {
      if (shutdownRequests_ == (std::numeric_limits<std::size_t>::max)())
        return false;
      ++shutdownRequests_;
    }
    operationChanged_.wait(lock, [&] {
      return !operationActive_ && (shutdown || !shutdownRequests_);
    });
    operationActive_ = true;
    operationOwner_ = caller;
    operationDepth_ = 1;
    if (operationTicket_ != (std::numeric_limits<std::uint64_t>::max)())
      ++operationTicket_;
    return true;
  } catch (...) {
    if (shutdown) {
      try {
        std::lock_guard lock{operationMutex_};
        if (shutdownRequests_)
          --shutdownRequests_;
        operationChanged_.notify_all();
      } catch (...) {
      }
    }
    return false;
  }
}

void StaticMeshAssetLibrary3D::EndOperation(bool shutdown) const noexcept {
  std::lock_guard lock{operationMutex_};
  if (!operationActive_ || operationOwner_ != std::this_thread::get_id() ||
      !operationDepth_)
    std::terminate();
  if (--operationDepth_)
    return;
  operationActive_ = false;
  operationOwner_ = {};
  if (stateGeneration_ != (std::numeric_limits<std::uint64_t>::max)())
    ++stateGeneration_;
  if (shutdown && shutdownRequests_)
    --shutdownRequests_;
  operationChanged_.notify_all();
}

bool StaticMeshAssetLibrary3D::Initialize(
    Engine &engine, SceneECSRegistry3D &registry,
    std::filesystem::path assetRoot, StaticMeshAssetLibraryLimits3D limits) {
  OperationGuard operation{*this};
  if (!operation)
    return false;
  assetRoot = ResolvePath({}, assetRoot);
  if (!limits.maximumAssets || !limits.maximumInstances ||
      !limits.maximumSourceFileBytes || !limits.maximumAggregateSourceBytes ||
      !std::isfinite(limits.maximumBoundsCenterDriftRatio) ||
      limits.maximumBoundsCenterDriftRatio < 0.f ||
      !std::isfinite(limits.minimumBoundsExtentRatio) ||
      limits.minimumBoundsExtentRatio <= 0.f ||
      !std::isfinite(limits.maximumBoundsExtentRatio) ||
      limits.maximumBoundsExtentRatio < limits.minimumBoundsExtentRatio)
    return false;
  if (engine_ && (engine_ != &engine || registry_ != &registry))
    return false;
  if (!entries_.empty())
    return engine_ == &engine && registry_ == &registry &&
           assetRoot_ == assetRoot;
  if (!materials_.Initialize(engine, assetRoot))
    return false;
  try {
    if (!ownerState_ || !ownerState_->active.load(std::memory_order_acquire)) {
      ownerState_ = std::make_shared<StaticMeshAssetLibraryLifetime3D>();
      ownerState_->ownerIdentity = NextAssetLibraryOwnerIdentity();
    }
  } catch (...) {
    materials_.Shutdown();
    return false;
  }
  engine_ = &engine;
  registry_ = &registry;
  assetRoot_ = std::move(assetRoot);
  limits_ = limits;
  return true;
}

StaticMeshAssetDefinition3D StaticMeshAssetLibrary3D::Resolve(
    StaticMeshAssetDefinition3D definition) const {
  definition.meshPath = ResolvePath(assetRoot_, definition.meshPath);
  for (auto &imported : definition.importedLods)
    imported.meshPath = ResolvePath(assetRoot_, imported.meshPath);
  auto resolve = [this](std::filesystem::path &path) {
    path = ResolvePath(assetRoot_, path);
  };
  resolve(definition.material.textures.baseColorPath);
  resolve(definition.material.textures.roughnessPath);
  resolve(definition.material.textures.normalPath);
  for (auto &layer : definition.material.textures.additionalLayers) {
    resolve(layer.baseColorPath);
    resolve(layer.roughnessPath);
    resolve(layer.normalPath);
  }
  if (definition.material.name.empty())
    definition.material.name = definition.name + ".material";
  return definition;
}

void StaticMeshAssetLibrary3D::DestroyPhysical(PhysicalAssetRecord &asset) {
  if (engine_)
    engine_->DestroyMeshLodResourceChain3D(asset.meshes);
}

std::optional<PreparedStaticMeshAsset3D> StaticMeshAssetLibrary3D::PrepareCpu(
    StaticMeshAssetDefinition3D definition) const {
  OperationGuard operation{*this};
  if (!operation)
    return std::nullopt;
  if (!engine_ || !registry_ || definition.name.empty() ||
      definition.meshPath.empty())
    return std::nullopt;
  definition = Resolve(std::move(definition));
  const auto pathInside = [this](const std::filesystem::path &path) {
    return path.empty() || IsWithin(assetRoot_, path);
  };
  bool pathsValid = pathInside(definition.meshPath) &&
                    pathInside(definition.material.textures.baseColorPath) &&
                    pathInside(definition.material.textures.roughnessPath) &&
                    pathInside(definition.material.textures.normalPath);
  for (const auto &layer : definition.material.textures.additionalLayers)
    pathsValid &= pathInside(layer.baseColorPath) &&
                  pathInside(layer.roughnessPath) &&
                  pathInside(layer.normalPath);
  for (std::uint32_t index = 0; index < definition.importedLodCount; ++index)
    pathsValid &= pathInside(definition.importedLods[index].meshPath);
  std::array<std::filesystem::path, 1 + MaximumGeneratedMeshLodLevels3D>
      sourcePaths{};
  sourcePaths[0] = definition.meshPath;
  for (std::uint32_t index = 0; index < definition.importedLodCount; ++index)
    sourcePaths[index + 1] = definition.importedLods[index].meshPath;
  std::size_t aggregateSourceBytes{};
  bool sourceSizesValid = true;
  for (std::uint32_t index = 0; index <= definition.importedLodCount; ++index) {
    std::error_code filesystemError;
    const std::uintmax_t bytes =
        std::filesystem::file_size(sourcePaths[index], filesystemError);
    if (filesystemError || bytes > limits_.maximumSourceFileBytes ||
        bytes > limits_.maximumAggregateSourceBytes - aggregateSourceBytes) {
      sourceSizesValid = false;
      break;
    }
    aggregateSourceBytes += std::size_t(bytes);
  }
  if (!ValidLodPresentation(definition) || !pathsValid || !sourceSizesValid)
    return std::nullopt;
  PreparedStaticMeshAsset3D result{};
  result.definition = definition;
  MeshLoadResult3D loaded =
      LoadMesh(definition.meshPath, definition.objOptions);
  if (!loaded)
    return std::nullopt;
  result.cpuMeshes[0] = std::move(loaded.mesh);
  MeshProcessingError3D error{};
  if (!MeshProcessing3D::Validate(result.cpuMeshes[0], &error))
    return std::nullopt;
  result.levelCount = 1;
  if (definition.generateLods) {
    std::vector<Mesh3D> generated;
    if (!MeshProcessing3D::GenerateLodChain(result.cpuMeshes[0], generated,
                                            definition.lodSettings,
                                            &result.processing, &error))
      return std::nullopt;
    for (auto &mesh : generated)
      result.cpuMeshes[result.levelCount++] = std::move(mesh);
  } else if (definition.importedLodCount) {
    const MeshBounds3D bounds = result.cpuMeshes[0].Bounds();
    std::size_t previous = result.cpuMeshes[0].indices.size() / 3;
    result.processing.sourceTriangles = previous;
    for (std::uint32_t index = 0; index < definition.importedLodCount;
         ++index) {
      auto imported = LoadMesh(definition.importedLods[index].meshPath,
                               definition.importedLods[index].objOptions);
      if (!imported)
        return std::nullopt;
      const size_t triangles = imported.mesh.indices.size() / 3;
      if (!MeshProcessing3D::Validate(imported.mesh, &error) ||
          triangles >= previous ||
          !CompatibleBounds(bounds, imported.mesh.Bounds(), limits_))
        return std::nullopt;
      previous = triangles;
      result.processing.totalOutputTriangles += triangles;
      result.cpuMeshes[result.levelCount++] = std::move(imported.mesh);
    }
    result.processing.finalTriangles = previous;
  }
  result.assetKey.contentIdentity = MeshContentIdentity(
      std::span<const Mesh3D>{result.cpuMeshes.data(), result.levelCount});
  result.assetKey.canonical =
      CanonicalKey(result.definition, result.assetKey.contentIdentity);
  return result;
}

std::size_t StaticMeshAssetLibrary3D::EstimatePreparedPhysicalAllocationBytes3D(
    const PreparedStaticMeshAsset3D &prepared) const {
  OperationGuard operation{*this};
  if (!operation || !engine_ || !prepared ||
      prepared.levelCount > prepared.cpuMeshes.size())
    return SIZE_MAX;
  std::size_t total{};
  for (std::uint32_t level = 0; level < prepared.levelCount; ++level) {
    const auto &mesh = prepared.cpuMeshes[level];
    std::size_t vertexBytes{}, indexBytes{};
    if (!mesh.Valid() ||
        !CheckedMultiply(mesh.vertices.size(), sizeof(Vertex3D), vertexBytes) ||
        !CheckedMultiply(mesh.indices.size(), sizeof(std::uint32_t), indexBytes) ||
        vertexBytes > (std::numeric_limits<std::size_t>::max)() -
                          (alignof(std::uint32_t) - 1))
      return SIZE_MAX;
    const std::size_t indexOffset =
        (vertexBytes + alignof(std::uint32_t) - 1) &
        ~(alignof(std::uint32_t) - 1);
    std::size_t meshBytes{};
    if (!CheckedAdd(indexOffset, indexBytes, meshBytes) ||
        !CheckedAdd(total, meshBytes, total))
      return SIZE_MAX;
  }
  const auto textureBytes = engine_->EstimateMaterialTextureSetAllocationBytes3D(
      prepared.definition.material.textures);
  return textureBytes && CheckedAdd(total, *textureBytes, total) ? total
                                                                 : SIZE_MAX;
}

std::optional<PreparedHlodProxyAsset3D>
StaticMeshAssetLibrary3D::PrepareHlodProxyCpu3D(
    StableObjectId3D worldId, GlobalChunkCoordinate3D chunk,
    HlodClusterId3D clusterId, std::uint64_t clusterGeneration,
    StaticMeshAssetDefinition3D definition) const {
  OperationGuard operation{*this};
  if (!operation)
    return std::nullopt;
  auto logicalName =
      HlodProxyLogicalName(worldId, chunk, clusterId, clusterGeneration);
  if (!logicalName)
    return std::nullopt;
  definition.name = *logicalName;
  if (definition.material.name.empty())
    definition.material.name = "__hlod-proxy-material";
  auto prepared = PrepareCpu(std::move(definition));
  if (!prepared || prepared->definition.name != *logicalName)
    return std::nullopt;
  const std::size_t estimate = EstimatePreparedPhysicalAllocationBytes3D(*prepared);
  if (estimate == SIZE_MAX)
    return std::nullopt;
  const std::size_t reserved = ContainsPreparedPhysical(*prepared) ? 0 : estimate;
  PreparedHlodProxyAsset3D result{};result.worldId=worldId;result.chunk=chunk;result.clusterId=clusterId;result.clusterGeneration=clusterGeneration;result.logicalName=std::move(*logicalName);result.prepared=std::move(*prepared);result.reservedPhysicalBytes=reserved;result.canonicalIdentity_=HlodPreparedIdentity(result.worldId,result.chunk,result.clusterId,result.clusterGeneration,result.logicalName);return result;
}

bool ValidatePreparedHlodProxyDescriptor3D(
    const PreparedHlodProxyAsset3D &value) noexcept {
  try {
    if (!value || value.prepared.definition.name != value.logicalName ||
        value.prepared.levelCount == 0 ||
        value.prepared.levelCount > value.prepared.cpuMeshes.size())
      return false;
    const auto expectedName = HlodProxyLogicalName(
        value.worldId, value.chunk, value.clusterId, value.clusterGeneration);
    if (!expectedName || *expectedName != value.logicalName ||
        value.canonicalIdentity_ !=
            HlodPreparedIdentity(value.worldId, value.chunk, value.clusterId,
                                 value.clusterGeneration, value.logicalName))
      return false;
    const auto meshes = std::span<const Mesh3D>{value.prepared.cpuMeshes.data(),
                                                value.prepared.levelCount};
    const auto contentIdentity = MeshContentIdentity(meshes);
    return contentIdentity == value.prepared.assetKey.contentIdentity &&
           CanonicalKey(value.prepared.definition, contentIdentity) ==
               value.prepared.assetKey.canonical;
  } catch (...) {
    return false;
  }
}

bool StaticMeshAssetLibrary3D::AdvanceHlodProxyFailureCheckpoint() noexcept {
  if (hlodProxyFailureCheckpoint_ < 0)
    return false;
  if (!hlodProxyFailureCheckpoint_) {
    hlodProxyFailureCheckpoint_ = -1;
    return true;
  }
  --hlodProxyFailureCheckpoint_;
  return false;
}

bool StaticMeshAssetLibrary3D::AcquireHlodProxyAtomic3D(
    PreparedHlodProxyAsset3D prepared, HlodProxyLease3D &output) {
  OperationGuard operation{*this};
  if (!operation || !engine_ || !registry_ || output.worldId || output.clusterId ||
      output.clusterGeneration || !output.logicalName.empty() ||
      output.renderable.Valid() || output.actualPhysicalBytes || output.owner_ ||
      !prepared ||
      !ValidHlodProxyLogicalName(prepared.logicalName, prepared.clusterId,
                                 prepared.clusterGeneration) ||
      prepared.prepared.definition.name != prepared.logicalName ||
      prepared.prepared.assetKey.contentIdentity == 0)
    return false;
  const auto expectedName=HlodProxyLogicalName(prepared.worldId,prepared.chunk,prepared.clusterId,prepared.clusterGeneration);
  if(!expectedName||*expectedName!=prepared.logicalName||prepared.canonicalIdentity_!=HlodPreparedIdentity(prepared.worldId,prepared.chunk,prepared.clusterId,prepared.clusterGeneration,prepared.logicalName))return false;
  if(!prepared.prepared.levelCount||prepared.prepared.levelCount>prepared.prepared.cpuMeshes.size())return false;
  const auto preparedMeshes=std::span<const Mesh3D>{prepared.prepared.cpuMeshes.data(),prepared.prepared.levelCount};const std::uint64_t contentIdentity=MeshContentIdentity(preparedMeshes);
  if(contentIdentity!=prepared.prepared.assetKey.contentIdentity||CanonicalKey(prepared.prepared.definition,contentIdentity)!=prepared.prepared.assetKey.canonical)return false;
  const std::size_t estimate =
      EstimatePreparedPhysicalAllocationBytes3D(prepared.prepared);
  if (estimate == SIZE_MAX ||
      (prepared.reservedPhysicalBytes != 0 &&
       prepared.reservedPhysicalBytes != estimate))
    return false;
  const bool physicalExisted = ContainsPreparedPhysical(prepared.prepared);
  if (!physicalExisted && prepared.reservedPhysicalBytes != estimate)
    return false;
  if (const auto existing = entries_.find(prepared.logicalName);
      existing != entries_.end()) {
    if (existing->second.assetKey != prepared.prepared.assetKey.canonical)
      ++stats_.nameConflicts;
    return false;
  }
  try {
    if (AdvanceHlodProxyFailureCheckpoint())
      throw std::bad_alloc{};
    entries_.reserve(entries_.size() + 1);
    physicalAssets_.reserve(physicalAssets_.size() + (physicalExisted ? 0 : 1));
    if (AdvanceHlodProxyFailureCheckpoint())
      throw std::bad_alloc{};
  } catch (...) {
    return false;
  }
  const RendererStats before = engine_->GetRendererStats();
  bool aliasCreated{};
  HlodProxyLease3D result{};
  try {
    if (!AcquirePrepared(std::move(prepared.prepared)))
      return false;
    aliasCreated = true;
    if (AdvanceHlodProxyFailureCheckpoint())
      throw std::bad_alloc{};
    auto renderable = ResolveRenderable(prepared.logicalName, 1, false);
    if (!renderable || !renderable->Valid() ||
        renderable->OwnerIdentity() != ownerState_->ownerIdentity ||
        renderable->Generation() !=
            ownerState_->generation.load(std::memory_order_acquire))
      throw std::runtime_error("HLOD proxy renderable lease validation failed");
    if (AdvanceHlodProxyFailureCheckpoint())
      throw std::bad_alloc{};
    const RendererStats after = engine_->GetRendererStats();
    if (after.residentMeshBytes < before.residentMeshBytes ||
        after.residentTextureAllocationBytes <
            before.residentTextureAllocationBytes)
      throw std::runtime_error("HLOD proxy resident byte counters regressed");
    const std::size_t meshDelta =
        after.residentMeshBytes - before.residentMeshBytes;
    const std::size_t textureDelta = after.residentTextureAllocationBytes -
                                     before.residentTextureAllocationBytes;
    std::size_t actual{};
    if (!CheckedAdd(meshDelta, textureDelta, actual) ||
        actual > prepared.reservedPhysicalBytes)
      throw std::overflow_error("HLOD proxy physical delta exceeded reservation");
    result.worldId = prepared.worldId;
    result.chunk = prepared.chunk;
    result.clusterId = prepared.clusterId;
    result.clusterGeneration = prepared.clusterGeneration;
    result.logicalName = prepared.logicalName;
    result.renderable = std::move(*renderable);
    result.actualPhysicalBytes = actual;
    result.owner_ = ownerState_;
    result.ownerIdentity_ = ownerState_->ownerIdentity;
    result.ownerGeneration_ =
        ownerState_->generation.load(std::memory_order_acquire);
    result.canonicalIdentity_=HlodPreparedIdentity(result.worldId,result.chunk,result.clusterId,result.clusterGeneration,result.logicalName);
    if (AdvanceHlodProxyFailureCheckpoint())
      throw std::bad_alloc{};
    output = std::move(result);
    return true;
  } catch (...) {
    result.renderable.Reset();
    if (aliasCreated)
      (void)Unload(prepared.logicalName);
    return false;
  }
}

bool StaticMeshAssetLibrary3D::ReleaseHlodProxyAlias3D(
    HlodProxyLease3D &lease) {
  OperationGuard operation{*this};
  if (!operation || !lease || lease.owner_.get() != ownerState_.get() ||
      lease.ownerIdentity_ != ownerState_->ownerIdentity ||
      lease.ownerGeneration_ !=
          ownerState_->generation.load(std::memory_order_acquire) ||
      !ownerState_->active.load(std::memory_order_acquire) ||
      !ValidHlodProxyLogicalName(lease.logicalName, lease.clusterId,
                                 lease.clusterGeneration))
    return false;
  const auto expectedName=HlodProxyLogicalName(lease.worldId,lease.chunk,lease.clusterId,lease.clusterGeneration);
  if(!expectedName||*expectedName!=lease.logicalName||lease.canonicalIdentity_!=HlodPreparedIdentity(lease.worldId,lease.chunk,lease.clusterId,lease.clusterGeneration,lease.logicalName))return false;
  lease.renderable.Reset();
  if (!Unload(lease.logicalName))
    return false;
  lease.Reset();
  return true;
}

bool StaticMeshAssetLibrary3D::AcquirePrepared(
    PreparedStaticMeshAsset3D prepared) {
  OperationGuard operation{*this};
  if (!operation)
    return false;
  ++stats_.loadRequests;
  if (!engine_ || !registry_ || !prepared ||
      entries_.size() >= limits_.maximumAssets) {
    ++stats_.loadFailures;
    return false;
  }
  if (const auto found = entries_.find(prepared.definition.name);
      found != entries_.end()) {
    if (found->second.assetKey == prepared.assetKey.canonical) {
      ++stats_.cacheHits;
      return true;
    }
    ++stats_.loadFailures;
    ++stats_.nameConflicts;
    return false;
  }
  if (const auto record = physicalAssets_.find(prepared.assetKey.canonical);
      record != physicalAssets_.end()) {
    try {
      const auto [entry, inserted] = entries_.emplace(
          prepared.definition.name,
          Entry{prepared.definition, prepared.assetKey.canonical});
      if (!inserted) {
        ++stats_.loadFailures;
        return false;
      }
      ++record->second->aliasCount;
    } catch (...) {
      ++stats_.loadFailures;
      return false;
    }
    ++stats_.cacheHits;
    if (prepared.definition.generateLods)
      ++stats_.generatedAssets;
    else if (prepared.definition.importedLodCount) {
      ++stats_.importedAssets;
      stats_.importedLevels += prepared.definition.importedLodCount;
    } else
      ++stats_.singleLevelAssets;
    return true;
  }
  auto physical = std::make_unique<PhysicalAssetRecord>();
  physical->materialName =
      "__static-" + std::to_string(prepared.assetKey.contentIdentity) + "-" +
      std::to_string(std::hash<std::string>{}(prepared.assetKey.canonical));
  auto materialDefinition = prepared.definition.material;
  materialDefinition.name = physical->materialName;
  const bool materialExisted =
      materials_.Find(physical->materialName) != nullptr;
  if (!materials_.Load(materialDefinition)) {
    ++stats_.loadFailures;
    return false;
  }
  physical->material = *materials_.Find(physical->materialName);
  physical->meshes.levelCount = prepared.levelCount;
  physical->meshes.processing = prepared.processing;
  for (std::uint32_t level = 0; level < prepared.levelCount; ++level) {
    const bool retain =
        prepared.definition.cpuRetention == MeshLodCpuRetention3D::AllLevels ||
        prepared.definition.cpuRetention == MeshLodCpuRetention3D::Level0Only &&
            level == 0;
    physical->meshes.levels[level] = engine_->CreateMeshResource3D(
        prepared.cpuMeshes[level], retain ? MeshCpuRetention3D::RetainForQueries
                                          : MeshCpuRetention3D::Discard);
    if (!physical->meshes.levels[level]) {
      DestroyPhysical(*physical);
      if (!materialExisted)
        materials_.Unload(physical->materialName);
      ++stats_.loadFailures;
      return false;
    }
  }
  physical->assetLease = std::make_shared<AssetLeaseState3D>(
      AssetLeaseState3D{NextAssetLeaseIdentity()});
  physical->aliasCount = 1;
  const std::string key = prepared.assetKey.canonical;
  const std::string materialName = physical->materialName;
  try {
    auto [record, inserted] = physicalAssets_.emplace(key, std::move(physical));
    if (!inserted)
      throw std::bad_alloc{};
    try {
      if (!entries_
               .emplace(prepared.definition.name,
                        Entry{prepared.definition, key})
               .second)
        throw std::bad_alloc{};
      materialReferences_.emplace(materialName, 1);
    } catch (...) {
      physical = std::move(record->second);
      physicalAssets_.erase(record);
      throw;
    }
  } catch (...) {
    if (physical)
      DestroyPhysical(*physical);
    if (!materialExisted)
      materials_.Unload(materialName);
    ++stats_.loadFailures;
    return false;
  }
  if (prepared.definition.generateLods)
    ++stats_.generatedAssets;
  else if (prepared.definition.importedLodCount) {
    ++stats_.importedAssets;
    stats_.importedLevels += prepared.definition.importedLodCount;
  } else
    ++stats_.singleLevelAssets;
  return true;
}

bool StaticMeshAssetLibrary3D::Load(StaticMeshAssetDefinition3D definition) {
  OperationGuard operation{*this};
  if (!operation)
    return false;
  auto prepared = PrepareCpu(std::move(definition));
  if (!prepared) {
    ++stats_.loadRequests;
    ++stats_.loadFailures;
    return false;
  }
  return AcquirePrepared(std::move(*prepared));
}

bool StaticMeshAssetLibrary3D::LoadAll(
    std::span<const StaticMeshAssetDefinition3D> definitions) {
  OperationGuard operation{*this};
  if (!operation)
    return false;
  std::vector<std::string> loaded;
  try {
    loaded.reserve(definitions.size());
  } catch (const std::bad_alloc &) {
    ++stats_.loadFailures;
    return false;
  }
  for (const auto &definition : definitions) {
    const bool existed = Contains(definition.name);
    if (Load(definition)) {
      if (!existed)
        loaded.push_back(definition.name);
      continue;
    }
    for (auto iterator = loaded.rbegin(); iterator != loaded.rend(); ++iterator)
      Unload(*iterator);
    ++stats_.batchRollbacks;
    return false;
  }
  return true;
}

bool StaticMeshAssetLibrary3D::Unload(std::string_view name) {
  OperationGuard operation{*this};
  if (!operation)
    return false;
  CollectDeadInstances();
  const std::string logicalName{name};
  const auto found = entries_.find(logicalName);
  if (found == entries_.end())
    return false;
  if (std::ranges::any_of(instances_, [&](const Instance &instance) {
        return instance.assetName == logicalName;
      })) {
    ++stats_.unloadRejections;
    return false;
  }
  const auto record = physicalAssets_.find(found->second.assetKey);
  if (record == physicalAssets_.end() || !record->second ||
      !record->second->aliasCount) {
    ++stats_.unloadRejections;
    return false;
  }
  PhysicalAssetRecord &physical = *record->second;
  if (physical.aliasCount > 1) {
    entries_.erase(found);
    --physical.aliasCount;
    ++stats_.unloaded;
    return true;
  }
  if (physical.assetLease.use_count() != 1) {
    ++stats_.unloadRejections;
    ++stats_.leaseUnloadRejections;
    return false;
  }
  entries_.erase(found);
  physical.aliasCount = 0;
  DestroyPhysical(physical);
  materials_.Unload(physical.materialName);
  materialReferences_.erase(physical.materialName);
  physicalAssets_.erase(record);
  ++stats_.unloaded;
  return true;
}

bool StaticMeshAssetLibrary3D::Shutdown() {
  OperationGuard operation{*this, true};
  if (!operation)
    return false;
  ClearInstances();
  if (std::ranges::any_of(physicalAssets_, [](const auto &value) {
        return value.second->assetLease.use_count() > 1;
      })) {
    ++stats_.shutdownRejections;
    return false;
  }
  if (engine_)
    for (auto &[key, asset] : physicalAssets_) {
      (void)key;
      DestroyPhysical(*asset);
    }
  std::unordered_map<std::string, Entry>{}.swap(entries_);
  std::unordered_map<std::string, std::unique_ptr<PhysicalAssetRecord>>{}.swap(
      physicalAssets_);
  std::unordered_map<std::string, std::size_t>{}.swap(materialReferences_);
  std::vector<Instance>{}.swap(instances_);
  materials_.Shutdown();
  engine_ = nullptr;
  registry_ = nullptr;
  assetRoot_.clear();
  if (ownerState_) {
    ownerState_->active.store(false, std::memory_order_release);
    ownerState_->generation.fetch_add(1, std::memory_order_acq_rel);
    ownerState_.reset();
  }
  return true;
}

bool StaticMeshAssetLibrary3D::Contains(std::string_view name) const {
  OperationGuard operation{*this};
  if (!operation)
    return false;
  return entries_.contains(std::string(name));
}

bool StaticMeshAssetLibrary3D::ContainsPreparedPhysical(
    const PreparedStaticMeshAsset3D &prepared) const {
  OperationGuard operation{*this};
  return operation && prepared &&
         physicalAssets_.contains(prepared.assetKey.canonical);
}

bool StaticMeshAssetLibrary3D::Initialized() const {
  OperationGuard operation{*this};
  return operation && engine_ != nullptr;
}

std::filesystem::path StaticMeshAssetLibrary3D::AssetRoot() const {
  OperationGuard operation{*this};
  return operation ? assetRoot_ : std::filesystem::path{};
}

std::optional<StaticMeshRenderableLease3D>
StaticMeshAssetLibrary3D::ResolveRenderable(std::string_view name,
                                            std::uint32_t layer,
                                            bool visible) const {
  OperationGuard operation{*this};
  if (!operation)
    return std::nullopt;
  if (!ownerState_ || !ownerState_->active.load(std::memory_order_acquire))
    return std::nullopt;
  auto renderable = CreateRenderable(name, layer, visible);
  if (!renderable)
    return std::nullopt;
  StaticMeshRenderableLease3D result{};
  result.renderable_ = std::move(*renderable);
  result.owner_ = ownerState_;
  result.ownerIdentity_ = ownerState_->ownerIdentity;
  result.generation_ = ownerState_->generation.load(std::memory_order_acquire);
  result.renderable_.resourceOwnerIdentity = result.ownerIdentity_;
  result.renderable_.resourceGeneration = result.generation_;
  return result.Valid()
             ? std::optional<StaticMeshRenderableLease3D>{std::move(result)}
             : std::nullopt;
}

std::optional<SceneMeshRenderable3D> StaticMeshAssetLibrary3D::CreateRenderable(
    std::string_view name, std::uint32_t layer, bool visible) const {
  const auto found = entries_.find(std::string(name));
  if (found == entries_.end())
    return std::nullopt;
  const Entry &entry = found->second;
  const auto record = physicalAssets_.find(entry.assetKey);
  if (record == physicalAssets_.end() || !record->second)
    return std::nullopt;
  const PhysicalAssetRecord &physical = *record->second;
  SceneMeshRenderable3D result{};
  result.assetLease = physical.assetLease;
  result.layer = layer;
  result.visible = visible;
  result.lodDistanceMetric = entry.definition.lodDistanceMetric;
  if (physical.meshes.levelCount == 1) {
    result.nearMesh = physical.meshes.levels[0];
    result.nearMaterial = physical.material;
    return result;
  }
  result.lodLevelCount = physical.meshes.levelCount;
  for (std::uint32_t level = 0; level < physical.meshes.levelCount; ++level) {
    result.lodLevels[level].mesh = physical.meshes.levels[level];
    result.lodLevels[level].material = physical.material;
    if (level + 1 < physical.meshes.levelCount) {
      result.lodLevels[level].switchDistance =
          entry.definition.lodSwitchDistances[level];
      result.lodLevels[level].transitionWidth =
          entry.definition.lodTransitionWidths[level];
    }
  }
  return result;
}

void StaticMeshAssetLibrary3D::CollectDeadInstances() {
  if (!registry_)
    return;
  for (auto iterator = instances_.begin(); iterator != instances_.end();) {
    const auto entry = entries_.find(iterator->assetName);
    const auto record = entry == entries_.end()
                            ? physicalAssets_.end()
                            : physicalAssets_.find(entry->second.assetKey);
    const SceneMeshRenderable3D *renderable =
        registry_->Renderable(iterator->entity);
    if (!registry_->Alive(iterator->entity) || !renderable ||
        record == physicalAssets_.end() ||
        renderable->assetLease.get() != record->second->assetLease.get()) {
      iterator = instances_.erase(iterator);
      ++stats_.externallyDestroyedInstances;
    } else
      ++iterator;
  }
}

SceneEntity3D StaticMeshAssetLibrary3D::Instantiate(std::string_view name,
                                                    Transform3D transform,
                                                    std::uint32_t layer,
                                                    bool visible) {
  OperationGuard operation{*this};
  if (!operation)
    return {};
  CollectDeadInstances();
  if (!registry_ || instances_.size() >= limits_.maximumInstances ||
      !ValidTransform(transform)) {
    ++stats_.instanceFailures;
    return {};
  }
  const auto renderable = CreateRenderable(name, layer, visible);
  if (!renderable) {
    ++stats_.instanceFailures;
    return {};
  }
  try {
    instances_.reserve(instances_.size() + 1);
  } catch (const std::bad_alloc &) {
    ++stats_.instanceFailures;
    return {};
  }
  const SceneEntity3D entity = registry_->Create();
  if (!entity || !registry_->SetTransform(entity, {transform}) ||
      !registry_->SetRenderable(entity, *renderable)) {
    if (entity)
      registry_->Destroy(entity);
    ++stats_.instanceFailures;
    return {};
  }
  instances_.push_back({entity, std::string(name)});
  ++stats_.instancesCreated;
  return entity;
}

bool StaticMeshAssetLibrary3D::DestroyInstance(SceneEntity3D entity) {
  OperationGuard operation{*this};
  if (!operation)
    return false;
  CollectDeadInstances();
  const auto found = std::ranges::find(instances_, entity, &Instance::entity);
  if (found == instances_.end() || !registry_->Destroy(entity))
    return false;
  instances_.erase(found);
  ++stats_.instancesDestroyed;
  return true;
}

std::size_t StaticMeshAssetLibrary3D::ClearInstances() {
  OperationGuard operation{*this};
  if (!operation)
    return 0;
  CollectDeadInstances();
  const std::size_t count = instances_.size();
  if (registry_)
    for (const Instance &instance : instances_)
      if (registry_->Destroy(instance.entity))
        ++stats_.instancesDestroyed;
  instances_.clear();
  return count;
}

std::uint32_t
StaticMeshAssetLibrary3D::LevelCount(std::string_view name) const {
  OperationGuard operation{*this};
  if (!operation)
    return 0;
  const auto found = entries_.find(std::string(name));
  if (found == entries_.end())
    return 0;
  const auto record = physicalAssets_.find(found->second.assetKey);
  return record == physicalAssets_.end() ? 0u
                                         : record->second->meshes.levelCount;
}

StaticMeshAssetLibraryStats3D StaticMeshAssetLibrary3D::Stats() const {
  OperationGuard operation{*this};
  if (!operation)
    return {};
  const_cast<StaticMeshAssetLibrary3D *>(this)->CollectDeadInstances();
  StaticMeshAssetLibraryStats3D result = stats_;
  result.assets = entries_.size();
  result.liveInstances = instances_.size();
  for (const auto &[name, references] : materialReferences_) {
    (void)name;
    result.materialReferences += references;
  }
  for (const auto &[key, physical] : physicalAssets_) {
    (void)key;
    const long references = physical->assetLease.use_count();
    if (references > 1)
      result.assetBorrowers += std::size_t(references - 1);
    result.meshLevels += physical->meshes.levelCount;
    if (physical->meshes.levelCount) {
      result.sourceTriangles += physical->meshes.levels[0].TriangleCount();
      for (std::uint32_t level = 0; level < physical->meshes.levelCount;
           ++level)
        result.residentTriangles +=
            physical->meshes.levels[level].TriangleCount();
    }
  }
  return result;
}

} // namespace Aevrynd
