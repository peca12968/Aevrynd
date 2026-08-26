#pragma once
#include <Rift/Engine.h>
#include <atomic>
#include <mutex>
#include <optional>
#include <random>
#include <thread>
#include <vector>
#include "LocalQwen.h"

class RiftwardenGame final:public Rift::IGame {
public:
    void OnStart(Rift::Engine&)override;
    void OnUpdate(Rift::Engine&,float)override;
    void OnRender(Rift::Engine&,float)override;
    void OnStop(Rift::Engine&)override;
private:
    enum class TowerKind{Bolt,Frost,Nova};
    enum class EnemyKind{Grunt,Swift,Tank,Boss};
    enum class Language{None,TraditionalChinese,English};
    struct Enemy{size_t id{};Rift::Vec2 pos{};float progress{},hp{},maxHp{},speed{},slow{};int reward{};EnemyKind kind{EnemyKind::Grunt};bool dead{};};
    struct Tower{Rift::Vec2 pos{};TowerKind kind{};int level{1};float cooldown{};};
    struct Shot{Rift::Vec2 pos{};size_t target{};float speed{},damage{},splash{};TowerKind kind{};bool dead{};};
    struct Particle{Rift::Vec2 pos{},velocity{};Rift::Color color{};float life{},size{};};
    struct Modifiers{float enemySpeed{1},damage{1},gold{1},range{1};};
    void Reset();void BeginWave();void SpawnEnemy(EnemyKind kind);void Fire(Tower&,size_t);void Hit(Shot&);void Burst(Rift::Vec2,Rift::Color,int);void UsePulse();void UseTimeLock();void TryBuildAt(Rift::Vec2);
    void SaveGame()const;void LoadGame();void ConfirmLanguage();
    void RequestEvent();void ApplyPendingEvent();Rift::Vec2 PathPosition(float)const;float TowerRange(TowerKind)const;float TowerCost(TowerKind)const;
    const char* Tr(const char* traditionalChinese,const char* english)const{return language_==Language::TraditionalChinese?traditionalChinese:english;}
    std::vector<Enemy> enemies_;std::vector<Tower> towers_;std::vector<Shot> shots_;std::vector<Particle> particles_;
    int wave_{},lives_{20},gold_{300},kills_{},spawnLeft_{},tutorialStep_{};size_t nextEnemyId_{1},gamepadSpot_{};float spawnClock_{},betweenWaves_{8},eventClock_{},pulseCooldown_{},lockCooldown_{},messageClock_{};bool waveActive_{},paused_{},gameOver_{},helpVisible_{true};
    Language language_{Language::None},pendingLanguage_{Language::English};bool progressLoaded_{};TowerKind selected_{TowerKind::Bolt};Modifiers mods_{};std::mt19937 rng_{std::random_device{}()};
    std::jthread qwenThread_;std::atomic_bool qwenBusy_{false};std::mutex eventMutex_;std::optional<GeneratedEvent> pendingEvent_;std::string eventName_="SYSTEM READY",statusMessage_="WELCOME, WARDEN";
};
