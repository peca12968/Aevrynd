#pragma once
#include <Aevrynd/CollisionWorld3D.h>
#include <Aevrynd/PhysicsWorld3D.h>
#include <cstdint>
#include <unordered_set>

namespace GameplayLab {
enum class Attack3D : std::uint8_t {None,Attack1,Attack2,Attack3};
struct CombatInput3D {Aevrynd::Vec3 playerPosition{},playerForward{0,0,1};bool attackPressed{};};
struct CombatStats3D {std::uint64_t sweeps{},hits{},duplicateHits{},kills{};};
struct MonsterState3D {Aevrynd::Vec3 position{};float health{100};bool alive{true};};

class GameplayCombat3D {
public:
    GameplayCombat3D(Aevrynd::CollisionWorld3D& collision,Aevrynd::PhysicsWorld3D& physics):collision_(collision),physics_(physics){}
    void SpawnMonster(Aevrynd::Vec3 position,float health=100);
    void Tick(const CombatInput3D& input,float deltaTime);
    [[nodiscard]] const MonsterState3D& Monster()const{return monster_;}
    [[nodiscard]] Attack3D CurrentAttack()const{return attack_;}
    [[nodiscard]] bool InHitWindow()const{return attack_!=Attack3D::None&&attackClock_>=.12f&&attackClock_<=.24f;}
    [[nodiscard]] float AttackClock()const{return attackClock_;}
    [[nodiscard]] float AttackDuration()const{return .55f;}
    [[nodiscard]] float NormalizedAttackPhase()const{return attack_==Attack3D::None?0.f:Aevrynd::Clamp(attackClock_/AttackDuration(),0.f,1.f);}
    [[nodiscard]] const CombatStats3D& Stats()const{return stats_;}
private:
    void BeginAttack();void Sweep(const CombatInput3D& input);
    Aevrynd::CollisionWorld3D& collision_;Aevrynd::PhysicsWorld3D& physics_;
    MonsterState3D monster_{};Aevrynd::ColliderHandle3D monsterCollider_{};Aevrynd::RigidBodyHandle3D monsterBody_{};
    Attack3D attack_{Attack3D::None},nextAttack_{Attack3D::Attack1};float attackClock_{},aiClock_{};bool windowSwept_{};std::unordered_set<std::uint32_t> hitThisAttack_;CombatStats3D stats_{};
};
}
