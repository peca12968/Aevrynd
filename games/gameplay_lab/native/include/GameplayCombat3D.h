#pragma once
#include <Aevrynd/CollisionWorld3D.h>
#include <Aevrynd/PhysicsWorld3D.h>
#include <cstdint>
#include <unordered_set>

namespace GameplayLab {
// 近戰有效距離由武器長度、雙方碰撞半徑與動作接觸餘量共同決定；AI停止距離
// 稍小於有效距離，確保畫面模型、collider與AI座標完全一致且仍可命中。
inline constexpr float GameplayMeleeWeaponLength3D=.72f;
inline constexpr float GameplayPlayerCapsuleRadius3D=.38f;
inline constexpr float GameplayMonsterCapsuleRadius3D=.45f;
inline constexpr float GameplayMeleeMotionAllowance3D=.40f;
inline constexpr float GameplayMeleeContactReach3D=GameplayMeleeWeaponLength3D+GameplayPlayerCapsuleRadius3D+GameplayMonsterCapsuleRadius3D+GameplayMeleeMotionAllowance3D;
inline constexpr float GameplayMonsterStopDistance3D=GameplayMeleeContactReach3D-.02f;
inline constexpr float GameplayPlayerMaximumHealth3D=100.f;
inline constexpr float GameplayMonsterAttackDamage3D=20.f;
inline constexpr float GameplayMonsterAttackCooldown3D=.55f;
inline constexpr float GameplayMonsterAttackHitTime3D=.20f;
inline constexpr float GameplayMonsterAttackDuration3D=.60f;
enum class Attack3D : std::uint8_t {None,Attack1,Attack2,Attack3};
struct CombatInput3D {Aevrynd::Vec3 playerPosition{},playerForward{0,0,1};bool attackPressed{};bool playerDamageEnabled{true};};
struct CombatStats3D {std::uint64_t sweeps{},hits{},duplicateHits{},kills{},monsterAttacks{},playerHits{},playerDeaths{};};
struct MonsterState3D {Aevrynd::Vec3 position{};float health{100};bool alive{true};};
struct PlayerCombatState3D {float health{GameplayPlayerMaximumHealth3D};bool alive{true};};

class GameplayCombat3D {
public:
    GameplayCombat3D(Aevrynd::CollisionWorld3D& collision,Aevrynd::PhysicsWorld3D& physics):collision_(collision),physics_(physics){}
    void SpawnMonster(Aevrynd::Vec3 position,float health=100);
    void Tick(const CombatInput3D& input,float deltaTime);
    [[nodiscard]] const MonsterState3D& Monster()const{return monster_;}
    [[nodiscard]] const PlayerCombatState3D& Player()const{return player_;}
    [[nodiscard]] Attack3D CurrentAttack()const{return attack_;}
    [[nodiscard]] bool MonsterAttackActive()const{return monsterAttackActive_;}
    [[nodiscard]] bool MonsterInHitWindow()const{return monsterAttackActive_&&monsterAttackClock_>=GameplayMonsterAttackHitTime3D&&monsterAttackClock_<=GameplayMonsterAttackHitTime3D+.12f;}
    [[nodiscard]] bool InHitWindow()const{return attack_!=Attack3D::None&&attackClock_>=.12f&&attackClock_<=.24f;}
    [[nodiscard]] float AttackClock()const{return attackClock_;}
    [[nodiscard]] float AttackDuration()const{return .55f;}
    [[nodiscard]] float NormalizedAttackPhase()const{return attack_==Attack3D::None?0.f:Aevrynd::Clamp(attackClock_/AttackDuration(),0.f,1.f);}
    [[nodiscard]] const CombatStats3D& Stats()const{return stats_;}
private:
    void BeginAttack();void Sweep(const CombatInput3D& input);
    Aevrynd::CollisionWorld3D& collision_;Aevrynd::PhysicsWorld3D& physics_;
    MonsterState3D monster_{};PlayerCombatState3D player_{};Aevrynd::ColliderHandle3D monsterCollider_{};Aevrynd::RigidBodyHandle3D monsterBody_{};
    Attack3D attack_{Attack3D::None},nextAttack_{Attack3D::Attack1};float attackClock_{},aiClock_{};bool windowSwept_{};std::unordered_set<std::uint32_t> hitThisAttack_;CombatStats3D stats_{};
    float monsterAttackCooldown_{},monsterAttackClock_{};bool monsterAttackActive_{},monsterAttackApplied_{};
};
}
