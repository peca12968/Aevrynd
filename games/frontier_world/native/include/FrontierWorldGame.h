#pragma once
#include "FrontierWorldRules.h"
#include <Aevrynd/Animation3D.h>
#include <Aevrynd/CharacterController3D.h>
#include <Aevrynd/CollisionWorld3D.h>
#include <Aevrynd/Engine.h>
#include <Aevrynd/EnvironmentMeshes3D.h>
#include <Aevrynd/HeightField3D.h>
#include <Aevrynd/MaterialLibrary3D.h>
#include <Aevrynd/Navigation3D.h>
#include <Aevrynd/Scene3D.h>
#include <Aevrynd/WorldStreamer3D.h>
#include <vector>

class FrontierWorldGame final:public Aevrynd::IGame {
public:
    void OnStart(Aevrynd::Engine&)override;
    void OnUpdate(Aevrynd::Engine&,float)override;
    void OnRender(Aevrynd::Engine&,float)override;
    void OnStop(Aevrynd::Engine&)override;
    Aevrynd::ReusableMemoryStats GetReusableMemoryStats()const override;
    std::size_t TrimReusableMemory()override;
private:
    enum class ViewMode {FirstPerson,ThirdPerson,GodView};
    struct Building {FrontierWorldRules::BuildingKind kind{};Aevrynd::Vec3 position{};float yaw{};Aevrynd::ColliderHandle3D collider{};};
    enum class MonsterKind {Wolf,Golem};
    struct Monster {Aevrynd::Vec3 position{};float health{},maximumHealth{},speed{},attackCooldown{};MonsterKind kind{};bool alive{true};std::vector<Aevrynd::Vec3> path;std::size_t pathIndex{};float repathClock{};};
    void PopulateWorld();
    void UpdateMonsters(float deltaTime);
    void RebuildNavigation();
    void Attack();
    bool Build(FrontierWorldRules::BuildingKind kind,Aevrynd::Vec3 position,float yaw);
    void AddEnvironment();
    void AddHumanoid(Aevrynd::Vec3 position,float yaw,bool armed);
    void AddMonster(const Monster& monster);
    Aevrynd::CharacterController3D controller_{};
    Aevrynd::CollisionWorld3D collisionWorld_;
    Aevrynd::Scene3D scene_;
    Aevrynd::Scene3DStats sceneStats_{};
    Aevrynd::MeshResource3D boxMesh_{},pyramidMesh_{},cylinderMesh_{},coneHighMesh_{},coneLowMesh_{},sphereHighMesh_{},sphereLowMesh_{},terrainMesh_{},grassHighMesh_{},grassLowMesh_{},crownHighMesh_{},crownLowMesh_{},riverMesh_{},riverFoamLeftMesh_{},riverFoamRightMesh_{};
    Aevrynd::MaterialLibrary3D materialLibrary_{};
    Aevrynd::MaterialLibraryStats3D materialLibraryStats_{};
    Aevrynd::Material3D groundMaterial_{},woodMaterial_{},barkMaterial_{},rockMaterial_{};
    Aevrynd::HeightField3D terrain_{};
    Aevrynd::SoundHandle swingSound_{},buildSound_{};
    Aevrynd::Skeleton3D humanoidSkeleton_{},creatureSkeleton_{};
    Aevrynd::AnimationClip3D idleClip_{},walkClip_{},attackClip_{},creatureWalkClip_{};
    Aevrynd::AnimationPlayer3D playerAnimation_{};
    Aevrynd::AnimationPose3D playerPose_{},creaturePose_{};
    Aevrynd::WorldStreamer3D worldStreamer_{};
    Aevrynd::WorldStreamingStats3D worldStreamingStats_{};
    Aevrynd::NavigationGrid3D navigationGrid_{};
    Aevrynd::NavigationQueryWorkspace3D navigationWorkspace_{};
    std::vector<Building> buildings_;
    std::vector<Monster> monsters_;
    Aevrynd::Camera3D camera_{};
    ViewMode viewMode_{ViewMode::ThirdPerson};
    FrontierWorldRules::BuildingKind selectedBuilding_{FrontierWorldRules::BuildingKind::Foundation};
    float yaw_{},pitch_{.15f},attackCooldown_{},attackAnimationClock_{},automaticClock_{};
    int health_{100},wood_{120},stone_{80},experience_{},level_{1},kills_{};
    uint64_t attacks_{},hits_{},buildAttempts_{};
    uint64_t navigationSearches_{},navigationVisited_{};
    std::size_t lastReusableMemoryReleasedBytes_{};uint64_t reusableMemoryTrimCount_{};
    std::size_t navigationBlockedCells_{},navigationPathPoints_{};
    bool automatic_{true},moving_{},navigationDirty_{true};
};
