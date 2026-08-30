#pragma once
#include "Geometry3D.h"
#include "Memory.h"
#include "StableId3D.h"
#include <cstdint>
#include <limits>
#include <unordered_map>
#include <vector>

namespace Aevrynd {
inline constexpr uint32_t CollisionLayerAll3D=0xffffffffu;
struct BoxCollider3D {Vec3 center{},extents{.5f,.5f,.5f};};
struct ColliderHandle3D {uint32_t value{};explicit constexpr operator bool()const{return value!=0;}constexpr bool operator==(const ColliderHandle3D&)const=default;};
struct StaticBoxCollider3D {BoxCollider3D bounds{};uint32_t layer{1},collisionMask{CollisionLayerAll3D};uint64_t userData{};bool trigger{};};
struct CollisionQueryFilter3D {uint32_t layerMask{CollisionLayerAll3D},queryLayer{1};bool includeTriggers{};};
struct CollisionRayHit3D {ColliderHandle3D collider{};float distance{std::numeric_limits<float>::infinity()};Vec3 position{},normal{};uint64_t userData{};explicit operator bool()const{return bool(collider);}};
// 球體掃掠結果；fraction 位於 [0,1]，position 是球面與碰撞盒接觸點，center 是命中時球心。
struct CollisionSphereSweepHit3D {
    ColliderHandle3D collider{};
    float fraction{std::numeric_limits<float>::infinity()},distance{std::numeric_limits<float>::infinity()};
    Vec3 center{},position{},normal{};
    uint64_t userData{};
    bool startedOverlapping{};
    explicit operator bool()const{return bool(collider);}
};
struct CollisionWorldStats3D {std::size_t colliders{},solidColliders{},triggers{};uint64_t queries{},boundsTested{},hits{};};
enum class CollisionWorldIdentityMode3D:uint8_t {LegacyLocal,AuthoritativeStable};
struct CollisionWorldStableIdentitySettings3D {
    uint64_t epoch{},nextCounter{};
    bool exhausted{};
    constexpr bool operator==(const CollisionWorldStableIdentitySettings3D&)const=default;
};
struct CollisionObjectRef3D {
    StableObjectId3D id{};
    uint64_t generation{};
    explicit constexpr operator bool()const{return(id.high||id.low)&&generation!=0;}
    constexpr auto operator<=>(const CollisionObjectRef3D&)const=default;
};
struct CollisionStableCreateResult3D {
    CollisionObjectRef3D object{};
    ColliderHandle3D local{};
    explicit constexpr operator bool()const{return bool(object)&&bool(local);}
};

class CollisionWorld3D {
public:
    // LegacyLocal 才開放此組 handle CRUD；AuthoritativeStable 必須使用下方 stable APIs。
    ColliderHandle3D CreateBox(StaticBoxCollider3D collider);
    bool UpdateBox(ColliderHandle3D handle,StaticBoxCollider3D collider);
    bool Destroy(ColliderHandle3D handle);
    // 自動配置 {epoch,counter} 與 generation 1；false 時 output 與全部 allocator/state 維持不變。
    [[nodiscard]] bool CreateStableBox(StaticBoxCollider3D collider,CollisionStableCreateResult3D& output)noexcept;
    // Authoritative 建立是 CAS：新 ID 只接受 nextCounter/expected 0；復活只接受 tombstone 的當前世代。
    [[nodiscard]] bool CreateStableBox(StableObjectId3D id,uint64_t expectedGeneration,StaticBoxCollider3D collider,CollisionStableCreateResult3D& output)noexcept;
    [[nodiscard]] bool UpdateStableBox(CollisionObjectRef3D object,StaticBoxCollider3D collider)noexcept;
    // tick 必須非零且不小於上次 destroy tick；generation MAX 會永久 retire 此 ID。
    [[nodiscard]] bool DestroyStable(CollisionObjectRef3D object,uint64_t tick)noexcept;
    [[nodiscard]] bool Resolve(CollisionObjectRef3D object,ColliderHandle3D& local)const noexcept;
    [[nodiscard]] bool Resolve(ColliderHandle3D local,CollisionObjectRef3D& object)const noexcept;
    // LegacyLocal 清除 live colliders；AuthoritativeStable 為 no-op，不能繞過 tombstone/retirement。
    void Clear();
    std::size_t TrimUnusedMemory();
    ReusableMemoryStats MemoryStats()const;
    const StaticBoxCollider3D* Get(ColliderHandle3D handle)const;
    const StaticBoxCollider3D* Get(CollisionObjectRef3D object)const noexcept;
    CollisionRayHit3D Raycast(Ray3D ray,float maximumDistance=std::numeric_limits<float>::infinity(),CollisionQueryFilter3D filter={})const;
    // 精確求 moving sphere 對 AABB 的 rounded face/edge/corner TOI；radius >= 0，無效輸入 fail-closed 且不配置記憶體。
    CollisionSphereSweepHit3D SweepSphere(Vec3 center,float radius,Vec3 displacement,CollisionQueryFilter3D filter={})const;
    bool OverlapsBox(Vec3 center,Vec3 extents,CollisionQueryFilter3D filter={})const;
    std::vector<ColliderHandle3D> QueryBox(Vec3 center,Vec3 extents,CollisionQueryFilter3D filter={})const;
    void CollectBoxes(std::vector<BoxCollider3D>& output,CollisionQueryFilter3D filter={})const;
    CollisionWorldStats3D Stats()const;
    // 僅雜湊可重播的語意狀態；不納入 bucket、容量、統計與位址。
    [[nodiscard]] uint64_t StateHash()const noexcept;
    // 預備 live/local-ref/tombstone 三表固定容量；不切換模式，也不改 epoch/counter。
    [[nodiscard]] bool PrepareStable(std::size_t maximumStableColliders)noexcept;
    // 僅空 world 可切換。AuthoritativeStable 必須先 PrepareStable，且 epoch/counter 不得為零。
    [[nodiscard]] bool SetIdentityMode(CollisionWorldIdentityMode3D mode,CollisionWorldStableIdentitySettings3D settings={})noexcept;
    [[nodiscard]] CollisionWorldIdentityMode3D IdentityMode()const noexcept{return identityMode_;}
    [[nodiscard]] CollisionWorldStableIdentitySettings3D StableIdentitySettings()const noexcept{return stableIdentitySettings_;}
    [[nodiscard]] std::size_t StablePreparedCapacity()const noexcept{return stablePreparedCapacity_;}
    bool CanRebaseOrigin(Vec3 delta)const noexcept;
    void RebaseOrigin(Vec3 delta)noexcept;
private:
    friend struct CollisionWorld3DTestAccess;
    bool Matches(const StaticBoxCollider3D& collider,CollisionQueryFilter3D filter)const;
    bool CreateStableBoxInternal(StableObjectId3D id,uint64_t expectedGeneration,StaticBoxCollider3D collider,CollisionStableCreateResult3D& output)noexcept;
    struct StableEntry {CollisionObjectRef3D object{};ColliderHandle3D local{};StaticBoxCollider3D collider{};};
    struct StableLocalRef {ColliderHandle3D local{};CollisionObjectRef3D object{};};
    struct StableTombstone {StableObjectId3D id{};uint64_t generation{};uint64_t destroyTick{};bool retired{};};
    std::unordered_map<uint32_t,StaticBoxCollider3D> colliders_;
    // 查詢一律依 handle 升序走訪；unordered_map 只負責 O(1) 定位。
    std::vector<uint32_t> canonicalIds_;
    // 三個 flat tables 都由 PrepareStable 預配；live 依 stable ID、local refs 依 local handle、tombstones 依 stable ID 排序。
    std::vector<StableEntry> stableEntries_;
    std::vector<StableLocalRef> stableLocalRefs_;
    std::vector<StableTombstone> stableTombstones_;
    mutable CollisionWorldStats3D stats_{};
    std::size_t lastReleasedBytes_{};
    uint64_t trimCount_{};
    uint32_t nextHandle_{1};
    bool handlesExhausted_{};
    CollisionWorldIdentityMode3D identityMode_{CollisionWorldIdentityMode3D::LegacyLocal};
    CollisionWorldStableIdentitySettings3D stableIdentitySettings_{};
    std::size_t stablePreparedCapacity_{};
    uint64_t stableLastDestroyTick_{};
};
}
