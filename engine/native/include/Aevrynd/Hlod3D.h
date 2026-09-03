#pragma once
#include "Camera.h"
#include "StableId3D.h"
#include "WorldOrigin3D.h"
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <thread>
#include <vector>
namespace Aevrynd {
using HlodClusterId3D=StableObjectId3D;
struct HlodSourceReference3D {StableObjectId3D globalObjectId{};constexpr bool operator==(const HlodSourceReference3D&)const=default;};
static_assert(sizeof(HlodSourceReference3D)==16);
struct HlodSourceBuildInput3D {HlodSourceReference3D reference{};std::uint64_t generation{},meshHandle{},materialHandle{};std::uint32_t meshGeneration{},materialGeneration{};Vec3 localBoundsCenter{},localBoundsExtents{1,1,1};Vec3 position{},rotation{},scale{1,1,1};};
enum class HlodCompatibility3D:std::uint8_t {OpaqueCompatible,MaskedMismatch,Transparent,Skinned,Dynamic,Destructible,Water,MaterialMismatch};
struct HlodClusterAsset3D {using Compatibility=HlodCompatibility3D;HlodClusterId3D id{};std::uint64_t generation{};Vec3 localBoundsCenter{};float localBoundsRadius{},geometricError{};float transitionStart{.8f},transitionEnd{1.2f};std::uint32_t sourceInstanceCount{};HlodCompatibility3D compatibility{HlodCompatibility3D::OpaqueCompatible};};
static_assert(sizeof(HlodClusterAsset3D)<=64);
struct HlodClusterResource3D {std::uint64_t proxyMeshHandle{},proxyMaterialHandle{},proxyResidentBytes{};std::uint32_t proxyMeshGeneration{},proxyMaterialGeneration{},sourceTriangleCount{},proxyTriangleCount{};};
struct HlodInstance3D {HlodClusterId3D clusterId{};GlobalChunkCoordinate3D globalChunk{};Vec3 chunkLocalPosition{},scale{1,1,1};Vec3 rotation{};};
struct HlodClusterBuildInput3D {HlodClusterAsset3D asset{};HlodClusterResource3D resource{};HlodInstance3D instance{};std::span<const HlodSourceBuildInput3D> sources{};};
// 所有 span 都借用 HlodData3D 的 immutable 儲存，資料本體必須保持存活。
class HlodData3D final {public:HlodData3D(const HlodData3D&)=delete;HlodData3D& operator=(const HlodData3D&)=delete;std::span<const HlodClusterAsset3D> Clusters()const noexcept{return clusters_;}std::span<const HlodClusterResource3D> Resources()const noexcept{return resources_;}std::span<const HlodInstance3D> Instances()const noexcept{return instances_;}std::span<const HlodSourceReference3D> Sources(std::size_t clusterIndex)const noexcept;std::uint64_t ContentHash()const noexcept{return contentHash_;}private:friend std::shared_ptr<const HlodData3D> BuildHlodData3D(std::span<const HlodClusterBuildInput3D>);HlodData3D()=default;std::vector<HlodClusterAsset3D> clusters_{};std::vector<HlodClusterResource3D> resources_{};std::vector<HlodInstance3D> instances_{};std::vector<HlodSourceReference3D> sources_{};std::vector<std::uint32_t> sourceOffsets_{};std::uint64_t contentHash_{};};
std::shared_ptr<const HlodData3D> BuildHlodData3D(std::span<const HlodClusterBuildInput3D> input);
struct HlodQualitySettings3D {bool enabled{true};std::uint32_t minimumSourceInstances{2};float transitionStart{.8f},transitionEnd{1.2f},maximumScreenError{2.f},hysteresis{.08f};std::uint64_t maximumResidentBytes{256ull*1024ull*1024ull};};
enum class HlodSelectionMode3D:std::uint8_t {Source,Transition,Proxy};
struct HlodSelection3D {HlodClusterId3D clusterId{};HlodSelectionMode3D mode{HlodSelectionMode3D::Source};float sourceCoverage{1},proxyCoverage{};std::uint8_t sourceCoverage8{255},proxyCoverage8{},ditherSeed{};bool sourceInvert{},proxyInvert{true};Vec3 renderLocalPosition{};std::uint64_t renderOriginGeneration{};};
struct HlodSelectionView3D {Camera3D camera{};Vec2 viewport{1920,1080};WorldOrigin3D origin{};float chunkSize{32};bool cameraCut{},historyInvalid{};};
// Selector 限建立執行緒使用；Select 回傳的 span 會在下一次 Select 時失效。
class HlodReferenceSelector3D final {public:HlodReferenceSelector3D()noexcept:owner_(std::this_thread::get_id()){};HlodReferenceSelector3D(const HlodReferenceSelector3D&)=delete;HlodReferenceSelector3D& operator=(const HlodReferenceSelector3D&)=delete;bool Reserve(std::size_t clusters);std::span<const HlodSelection3D> Select(const HlodData3D&,const HlodSelectionView3D&,const HlodQualitySettings3D&)noexcept;std::size_t RetainedCapacityBytes()const noexcept;private:std::vector<HlodSelection3D> selections_{};std::vector<std::uint8_t> stableModes_{};std::vector<std::uint64_t> stableGenerations_{};std::vector<HlodClusterId3D> stableIds_{};std::vector<Vec3> cachedCenters_{};std::uint64_t stableDataHash_{},cachedDataHash_{},cachedOriginGeneration_{};float cachedChunkSize_{};GlobalPosition3D cachedOrigin_{};std::thread::id owner_{};};
// Core Slice 1 不持有資產 lease、frozen submission 或 GPU fence retirement。
inline constexpr bool HlodCoreSliceOwnsAssetLeases3D=false;
}
