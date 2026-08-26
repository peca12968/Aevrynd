#pragma once
#include "Engine.h"
#include "PhysicsWorld3D.h"
#include "Scene3D.h"
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>

namespace Aevrynd {
enum class PrimitiveShape3D : uint8_t {Box,Pyramid,Sphere,Cylinder,Cone};
enum class MaterialPreset3D : uint8_t {Matte,Wood,Stone,Metal,Glass,Emissive,Foliage,Water};
struct WorldObjectHandle3D {uint32_t value{};explicit constexpr operator bool()const{return value!=0;}constexpr bool operator==(const WorldObjectHandle3D&)const=default;};
struct WorldObjectDefinition3D {std::string name;PrimitiveShape3D primitive{PrimitiveShape3D::Box};Transform3D transform{};Material3D material{};RigidBodyType3D bodyType{RigidBodyType3D::Static};PhysicsMaterial3D physicsMaterial{};uint32_t collisionLayer{1},collisionMask{0xffffffffu};bool visible{true},collisionEnabled{true};};
struct WorldObject3D {WorldObjectHandle3D handle{};WorldObjectDefinition3D definition{};RigidBodyHandle3D rigidBody{};};
struct WorldBuilderStats3D {std::size_t objects{},visibleObjects{},colliders{},dynamicObjects{},residentPrimitiveMeshes{};uint64_t created{},destroyed{},submitted{};};
Material3D CreateMaterialPreset3D(MaterialPreset3D preset,Color tint={1,1,1,1},bool castsShadow=true);

class WorldBuilder3D {
public:
    void Initialize(Engine& engine);
    void Shutdown(Engine& engine,PhysicsWorld3D* physics=nullptr);
    WorldObjectHandle3D Create(WorldObjectDefinition3D definition,PhysicsWorld3D* physics=nullptr);
    WorldObjectHandle3D Create(std::string name,PrimitiveShape3D primitive,Transform3D transform,MaterialPreset3D material,Color tint,RigidBodyType3D bodyType=RigidBodyType3D::Static,PhysicsWorld3D* physics=nullptr,MaterialTextureSetHandle3D textures={},float textureScale=.25f);
    bool Destroy(WorldObjectHandle3D handle,PhysicsWorld3D* physics=nullptr);
    bool SetTransform(WorldObjectHandle3D handle,Transform3D transform,PhysicsWorld3D* physics=nullptr);
    bool SetMaterial(WorldObjectHandle3D handle,Material3D material);
    WorldObject3D* Get(WorldObjectHandle3D handle);
    const WorldObject3D* Get(WorldObjectHandle3D handle)const;
    WorldObjectHandle3D Find(std::string_view name)const;
    void Synchronize(const PhysicsWorld3D& physics);
    void Submit(Scene3D& scene)const;
    WorldBuilderStats3D Stats()const;
private:
    const MeshResource3D& MeshFor(PrimitiveShape3D primitive)const;
    std::unordered_map<uint32_t,WorldObject3D> objects_;
    MeshResource3D box_{},pyramid_{},sphere_{},cylinder_{},cone_{};
    uint32_t nextHandle_{1};mutable WorldBuilderStats3D stats_{};
};
}
