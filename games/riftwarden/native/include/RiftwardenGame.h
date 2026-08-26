#pragma once
#include <Aevrynd/Engine.h>
#include <Aevrynd/Effects2D.h>
#include <atomic>
#include <mutex>
#include <optional>
#include <thread>
#include <vector>
#include "LocalQwen.h"
#include "RiftwardenRules.h"

class RiftwardenGame final:public Aevrynd::IGame {
public:
    void OnStart(Aevrynd::Engine&)override;
    void OnUpdate(Aevrynd::Engine&,float)override;
    void OnRender(Aevrynd::Engine&,float)override;
    void OnStop(Aevrynd::Engine&)override;
private:
    using TowerKind=RiftwardenRules::TowerKind;
    using EnemyKind=RiftwardenRules::EnemyKind;
    using UpgradeKind=RiftwardenRules::UpgradeKind;
    enum class Language{None,TraditionalChinese,English};
    struct Enemy{size_t id{};Aevrynd::Vec2 pos{};float progress{},hp{},maxHp{},speed{},slow{};int reward{};EnemyKind kind{EnemyKind::Grunt};bool dead{};};
    struct Tower{Aevrynd::Vec2 pos{};TowerKind kind{};int level{1};float cooldown{};};
    struct Shot{Aevrynd::Vec2 pos{};size_t target{};float speed{},damage{},splash{};TowerKind kind{};bool dead{};};
    struct Modifiers{float enemySpeed{1},damage{1},gold{1},range{1};};
    void Reset();void BeginWave();void SpawnEnemy(EnemyKind kind);void Fire(Tower&,size_t);void Hit(Shot&);void DefeatEnemy(Enemy&,Aevrynd::Color);void Burst(Aevrynd::Vec2,Aevrynd::Color,int);void UsePulse();void UseTimeLock();void TryBuildAt(Aevrynd::Vec2);void ChooseUpgrade(UpgradeKind);void AddCameraShake(float,float);void PlayCue(Aevrynd::SoundHandle,float volume=1,float pitch=1);
    void SaveGame()const;void LoadGame();void ConfirmLanguage();
    void RequestEvent();void ApplyPendingEvent();Aevrynd::Vec2 PathPosition(float)const;float TowerRange(TowerKind)const;float TowerCost(TowerKind)const;
    const char* Tr(const char* traditionalChinese,const char* english)const{return language_==Language::TraditionalChinese?traditionalChinese:english;}
    std::vector<Enemy> enemies_;std::vector<Tower> towers_;std::vector<Shot> shots_;Aevrynd::ParticleSystem2D particles_{2048};Aevrynd::ScreenShake2D screenShake_{};
    int wave_{},lives_{20},gold_{300},kills_{},spawnLeft_{},tutorialStep_{},combo_{},bestCombo_{},shatters_{},upgradeSelection_{};size_t nextEnemyId_{1},gamepadSpot_{};uint32_t effectSeed_{1};float spawnClock_{},betweenWaves_{8},eventClock_{},pulseCooldown_{},lockCooldown_{},messageClock_{},comboClock_{};bool waveActive_{},paused_{},gameOver_{},helpVisible_{true},upgradePending_{},performanceVisible_{};
    Language language_{Language::None},pendingLanguage_{Language::English};bool progressLoaded_{};TowerKind selected_{TowerKind::Bolt};Modifiers mods_{};RiftwardenRules::UpgradeLevels upgrades_{};
    Aevrynd::AudioEngine* audio_{};Aevrynd::SoundHandle buildSound_{},waveSound_{},abilitySound_{},damageSound_{};
    std::jthread qwenThread_;std::atomic_bool qwenBusy_{false};std::mutex eventMutex_;std::optional<GeneratedEvent> pendingEvent_;std::string eventName_="SYSTEM READY",statusMessage_="WELCOME, WARDEN";
};
