#pragma once

#include "Scene3D.h"
#include <cstdint>
#include <limits>
#include <optional>
#include <vector>

namespace Aevrynd {
struct SceneEntity3D {
    std::uint32_t index{(std::numeric_limits<std::uint32_t>::max)()};
    std::uint32_t generation{};
    explicit constexpr operator bool() const { return index != (std::numeric_limits<std::uint32_t>::max)() && generation != 0; }
    constexpr bool operator==(const SceneEntity3D&) const = default;
};

struct SceneTransformComponent3D { Transform3D value{}; };
struct SceneMeshRenderable3D {
    MeshResource3D nearMesh{};
    MeshResource3D farMesh{};
    float lodSwitchDistance{40.f};
    float lodTransitionWidth{};
    MeshLodDistanceMetric3D lodDistanceMetric{MeshLodDistanceMetric3D::Center};
    Material3D nearMaterial{};
    Material3D farMaterial{};
    std::uint32_t layer{1};
    bool visible{true};
    std::array<MeshLodLevel3D,MaximumMeshLodLevels3D> lodLevels{};
    std::uint32_t lodLevelCount{};
};
struct ScenePointLightComponent3D {
    PointLight3D light{};
    std::uint32_t layer{1};
    bool visible{true};
    bool inheritTransform{true};
};

struct SceneECSCollectionStats3D {
    std::size_t aliveEntities{}, transformComponents{}, renderableComponents{}, pointLightComponents{};
    std::size_t renderableCandidates{}, submittedRenderables{}, submittedLodRenderables{}, submittedPointLights{};
    std::size_t missingTransform{}, hidden{}, layerCulled{}, invalidComponents{};
    std::uint64_t collectionPasses{};
};

class SceneECSRegistry3D {
public:
    SceneEntity3D Create();
    bool Destroy(SceneEntity3D entity);
    bool Alive(SceneEntity3D entity) const;
    void Clear();

    bool SetTransform(SceneEntity3D entity, SceneTransformComponent3D component);
    bool SetRenderable(SceneEntity3D entity, SceneMeshRenderable3D component);
    bool SetPointLight(SceneEntity3D entity, ScenePointLightComponent3D component);
    bool RemoveTransform(SceneEntity3D entity);
    bool RemoveRenderable(SceneEntity3D entity);
    bool RemovePointLight(SceneEntity3D entity);

    SceneTransformComponent3D* Transform(SceneEntity3D entity);
    SceneMeshRenderable3D* Renderable(SceneEntity3D entity);
    ScenePointLightComponent3D* PointLight(SceneEntity3D entity);
    const SceneTransformComponent3D* Transform(SceneEntity3D entity) const;
    const SceneMeshRenderable3D* Renderable(SceneEntity3D entity) const;
    const ScenePointLightComponent3D* PointLight(SceneEntity3D entity) const;

    // 收集只追加到既有 Scene3D 與 DrawList，呼叫端可自由混用傳統 Scene3D API。
    SceneECSCollectionStats3D Collect(Scene3D& scene, DrawList& draw, std::uint32_t visibilityMask = 0xffffffffu) const;
    // 回傳最近一次 Collect 的結果，不代表尚未重新收集的即時世界狀態。
    SceneECSCollectionStats3D Stats() const;
    ReusableMemoryStats MemoryStats() const;
    std::size_t TrimUnusedMemory();

private:
    struct Slot {
        std::uint32_t generation{};
        bool alive{};
        std::optional<SceneTransformComponent3D> transform;
        std::optional<SceneMeshRenderable3D> renderable;
        std::optional<ScenePointLightComponent3D> pointLight;
    };
    Slot* Find(SceneEntity3D entity);
    const Slot* Find(SceneEntity3D entity) const;
    std::vector<Slot> slots_;
    std::vector<std::uint32_t> freeSlots_;
    mutable std::vector<std::uint32_t> renderableBatch_;
    mutable std::vector<std::uint32_t> pointLightBatch_;
    mutable SceneECSCollectionStats3D lastStats_{};
    std::size_t lastReleasedBytes_{};
    std::uint64_t trimCount_{};
};
}
