#pragma once
#include "FrontierWorldRules.h"
#include <Aevrynd/Animation3D.h>
#include <Aevrynd/CharacterController3D.h>
#include <Aevrynd/CollisionWorld3D.h>
#include <Aevrynd/Engine.h>
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
private:
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
    Aevrynd::MeshResource3D boxMesh_{},pyramidMesh_{},sphereHighMesh_{},sphereLowMesh_{};
    Aevrynd::SoundHandle swingSound_{},buildSound_{};
    Aevrynd::Skeleton3D humanoidSkeleton_{},creatureSkeleton_{};
    Aevrynd::AnimationClip3D idleClip_{},walkClip_{},attackClip_{},creatureWalkClip_{};
    Aevrynd::AnimationPlayer3D playerAnimation_{};
    Aevrynd::WorldStreamer3D worldStreamer_{};
    Aevrynd::WorldStreamingStats3D worldStreamingStats_{};
    Aevrynd::NavigationGrid3D navigationGrid_{};
    std::vector<Building> buildings_;
    std::vector<Monster> monsters_;
    Aevrynd::Camera3D camera_{};
    FrontierWorldRules::BuildingKind selectedBuilding_{FrontierWorldRules::BuildingKind::Foundation};
    float yaw_{},pitch_{.15f},attackCooldown_{},attackAnimationClock_{},automaticClock_{};
    int health_{100},wood_{120},stone_{80},experience_{},level_{1},kills_{};
    uint64_t attacks_{},hits_{},buildAttempts_{};
    uint64_t navigationSearches_{},navigationVisited_{};
    std::size_t navigationBlockedCells_{},navigationPathPoints_{};
    bool firstPerson_{},automatic_{true},moving_{},navigationDirty_{true};
};
