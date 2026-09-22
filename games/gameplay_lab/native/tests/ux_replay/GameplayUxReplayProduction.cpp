#include "GameplayUxReplayProduction.h"
#include "GameplayUxReplayWin32.h"

#include "GameplayCombat3D.h"
#include "ObjectPaletteAdapter3D.h"

#include <Aevrynd/CharacterController3D.h>
#include <Aevrynd/HeightField3D.h>

#include <algorithm>
#include <cmath>
#include <fstream>

namespace GameplayLab::UxReplay {
namespace {
class ProductionAdapter final : public IGameplayUxReplayAdapter3D {
public:
  ProductionAdapter() : palette_(rules_, collisions_, physics_, scene_),
                        combat_(collisions_, physics_) {
    rules_.Reset();
    controller_.Reset({0, 0, 0});
    terrain_.Configure({{-32, 0, -32}, 17, 17, 4});
    palette_.PlaceAt({0, 0, 25}, PaletteCategory::Props);
    combat_.SpawnMonster({5, 0, 20}, 100.f);
  }

  void Apply(const ProductionInput3D &input, const ReplayEdges3D &edges,
             std::span<const PointerEvent3D> pointerEvents,
             float fixedDeltaTime) override {
    if (edges.focusLost) {
      focused_ = false;
      captured_ = false;
      palette_.Cancel();
    }
    if (edges.focusRestored)
      focused_ = true;
    if (edges.closeRequested)
      closed_ = true;
    if (!focused_)
      return;
    if (input.camera)
      rules_.SetCamera(*input.camera);
    if (input.cycleCamera)
      rules_.CycleCamera();
    if (input.category)
      rules_.SetPaletteCategory(*input.category);

    for (const auto &pointer : pointerEvents) {
      if (pointer.phase == PointerPhase3D::RawDelta && !rules_.PaletteOpen()) {
        yaw_ -= pointer.delta.x * .0025f;
        pitch_ = std::clamp(pitch_ - pointer.delta.y * .0025f, -.75f, .9f);
      }
      const Aevrynd::Ray3D ray = Aevrynd::MakeRay3D(
          controller_.Position() + Aevrynd::Vec3{0, 2, -2}, {0, -0.35f, 1});
      if (pointer.phase == PointerPhase3D::Down && rules_.PaletteOpen() &&
          pointer.position.x < 320.f) {
        palette_.Begin(ray, rules_.Category());
        captured_ = palette_.Preview().active;
      } else if (pointer.phase == PointerPhase3D::Up &&
                 palette_.Preview().active) {
        palette_.Commit();
        captured_ = false;
      }
    }
    if (edges.pointerCancelled || input.tick.cancel) {
      palette_.Cancel();
      captured_ = false;
    }
    if (edges.grabPressed && !rules_.PaletteOpen()) {
      if (palette_.Grabbed())
        palette_.EndGrab();
      else
        palette_.BeginGrab(Aevrynd::MakeRay3D(
            controller_.Position() + Aevrynd::Vec3{0, 1, 0}, {0, 0, 1}));
    }

    auto tick = input.tick;
    // UI 開啟或拖放時，正式優先權會攔截攻擊與抓取。
    if (rules_.PaletteOpen() || palette_.Preview().active) {
      tick.attack = false;
      tick.grab = false;
      tick.movement = {};
      tick.run = false;
      tick.jump = false;
    }
    rules_.Tick(tick, fixedDeltaTime);
    const float height = rules_.CharacterHeight();
    controller_.SetHeight(height);
    const Aevrynd::CharacterControl3D control =
        BuildProductionCharacterControl3D(tick, rules_);
    controller_.Step(control, collisions_, terrain_, fixedDeltaTime);
    const Aevrynd::Vec3 forward{std::sin(yaw_), 0, std::cos(yaw_)};
    combat_.Tick({controller_.Position(), forward,
                  (tick.attack || edges.meleePressed) &&
                      !rules_.PaletteOpen() && !palette_.Preview().active},
                 fixedDeltaTime);
    if (edges.escapePressed) {
      if (rules_.PaletteOpen() || palette_.Preview().active) {
        palette_.Cancel();
        TickInput closePalette{};
        closePalette.togglePalette = rules_.PaletteOpen();
        rules_.Tick(closePalette, fixedDeltaTime);
        captured_ = false;
      } else {
        closed_ = true;
      }
    }
    physics_.Step(fixedDeltaTime);
    palette_.SyncDynamicTransforms();
  }

  ReplayState3D Snapshot() const override {
    const auto ruleStats = rules_.GetStats();
    const auto paletteStats = palette_.Stats();
    ReplayState3D state{};
    state.playerPosition = controller_.Position();
    state.camera = rules_.Camera();
    state.yaw = yaw_;
    state.pitch = pitch_;
    state.cameraDistance = rules_.Camera() == CameraMode::FirstPerson ? .25f : 3.4f;
    state.characterHeight = rules_.CharacterHeight();
    state.posture = rules_.Posture();
    state.focused = focused_;
    state.cursorCaptured = captured_;
    state.cursorVisible = !focused_ || rules_.PaletteOpen() || !captured_;
    state.paletteOpen = rules_.PaletteOpen();
    state.dragging = palette_.Preview().active;
    state.grabbed = palette_.Grabbed().has_value();
    state.crouched = rules_.Crouched();
    state.prone = rules_.Prone();
    state.closed = closed_;
    state.cameraToggles = ruleStats.cameraSwitches;
    state.paletteToggles = paletteToggles_;
    state.crouchToggles = ruleStats.crouches;
    state.proneToggles = ruleStats.prones;
    state.jumps = ruleStats.jumps;
    state.grabs = paletteStats.grabs;
    state.melee = combat_.Stats().sweeps +
                  (combat_.CurrentAttack() != Attack3D::None ? 1u : 0u);
    state.commits = paletteStats.commits;
    state.cancels = paletteStats.cancels;
    return state;
  }

  void SetFocus(bool focused) override {
    focused_ = focused;
    if (!focused) {
      captured_ = false;
      palette_.Cancel();
    }
  }

private:
  Rules3D rules_{};
  Aevrynd::CharacterController3D controller_{};
  Aevrynd::CollisionWorld3D collisions_{};
  Aevrynd::PhysicsWorld3D physics_{};
  Aevrynd::SceneECSRegistry3D scene_{};
  Aevrynd::HeightField3D terrain_{};
  ObjectPaletteAdapter3D palette_;
  GameplayCombat3D combat_;
  float yaw_{}, pitch_{.2f};
  std::uint64_t paletteToggles_{};
  bool focused_{true}, captured_{}, closed_{};
};

bool Edge(GameplayUxReplayWin32Harness3D &harness, unsigned key) {
  return harness.Key(key, true) && harness.Tick() &&
         harness.Key(key, false) && harness.Tick();
}

std::uint64_t RunRate(unsigned rate) {
  ProductionAdapter adapter;
  GameplayUxReplayWin32Harness3D harness(adapter);
  if (!harness.Initialize() || !harness.Key('W', true))
    return 0;
  constexpr unsigned FixedRate = 120;
  for (unsigned frame = 0; frame < rate; ++frame)
    for (unsigned step = frame * FixedRate / rate;
         step < (frame + 1) * FixedRate / rate; ++step)
      if (!harness.Tick())
        return 0;
  if (!harness.Key('W', false) || !harness.Tick())
    return 0;
  return HashReplayState3D(adapter.Snapshot());
}
} // namespace
} // namespace GameplayLab::UxReplay

int RunGameplayUxReplayProduction(const std::filesystem::path &receiptPath) {
  using namespace GameplayLab::UxReplay;
  ProductionAdapter adapter;
  GameplayUxReplayWin32Harness3D harness(adapter);
  if (!harness.Initialize())
    return 81;
  if (!harness.Focus(false) || !harness.Key('W', true) || !harness.Tick() ||
      Aevrynd::Length(adapter.Snapshot().playerPosition) != 0.f ||
      !harness.Focus(true) || !harness.Tick())
    return 82;
  if (!Edge(harness, '1') || !Edge(harness, '2') || !Edge(harness, '3') ||
      !Edge(harness, 'V'))
    return 83;
  const auto togglesBeforeRepeat=adapter.Snapshot().cameraToggles;
  if(!harness.Key('V',true)||!harness.Tick()||!harness.Key('V',true)||
     !harness.Tick()||!harness.Key('V',false)||!harness.Tick()||
     adapter.Snapshot().cameraToggles!=togglesBeforeRepeat+1)
    return 83;
  const auto walkStart=adapter.Snapshot().playerPosition;
  if(!harness.Key('W',true))return 90;
  for(unsigned tick=0;tick<120;++tick)if(!harness.Tick())return 90;
  if(!harness.Key('W',false)||!harness.Tick())return 90;
  const auto walkEnd=adapter.Snapshot().playerPosition;
  const auto runStart=walkEnd;
  if(!harness.Key(VK_SHIFT,true)||!harness.Key('W',true))return 90;
  for(unsigned tick=0;tick<120;++tick)if(!harness.Tick())return 90;
  if(!harness.Key('W',false)||!harness.Key(VK_SHIFT,false)||!harness.Tick())return 90;
  const auto runEnd=adapter.Snapshot().playerPosition;
  const float walkDistance=Aevrynd::Length(walkEnd-walkStart);
  const float runDistance=Aevrynd::Length(runEnd-runStart);
  if(std::abs(walkDistance-4.f)>.05f||std::abs(runDistance-7.f)>.05f)
    {std::ofstream diagnostic(receiptPath,std::ios::trunc);diagnostic<<"{\"status\":\"run-ratio-fail\",\"walk\":"<<walkDistance<<",\"run\":"<<runDistance<<"}";return 90;}
  const auto lookBefore=adapter.Snapshot();
  if (!harness.RawMouse(32, -16) || !harness.Tick() ||
      (adapter.Snapshot().yaw==lookBefore.yaw&&adapter.Snapshot().pitch==lookBefore.pitch) ||
      !Edge(harness, 'C'))
    return 91;
  const auto crouchStart=adapter.Snapshot().playerPosition;
  if(!harness.Key('W',true))return 92;
  for(unsigned tick=0;tick<120;++tick)if(!harness.Tick())return 92;
  if(!harness.Key('W',false)||!harness.Tick())return 92;
  const float crouchDistance=Aevrynd::Length(adapter.Snapshot().playerPosition-crouchStart);
  const auto beforeCrouchedJump=adapter.Snapshot();
  const auto writePostureFailure=[&](const char* stage){const auto snapshot=adapter.Snapshot();std::ofstream diagnostic(receiptPath,std::ios::trunc);diagnostic<<"{\"status\":\"posture-fail\",\"stage\":\""<<stage<<"\",\"height\":"<<snapshot.characterHeight<<",\"posture\":"<<unsigned(snapshot.posture)<<",\"y\":"<<snapshot.playerPosition.y<<",\"jumps\":"<<snapshot.jumps<<",\"crouchDistance\":"<<crouchDistance<<"}";};
  if(std::abs(crouchDistance-2.f)>.05f||!Edge(harness,VK_SPACE)){writePostureFailure("crouch-jump-edge");return 92;}
  const auto afterCrouchedJump=adapter.Snapshot();
  if(std::abs(afterCrouchedJump.playerPosition.y-beforeCrouchedJump.playerPosition.y)>.0001f||
     afterCrouchedJump.jumps!=beforeCrouchedJump.jumps){writePostureFailure("crouch-jump-gate");return 92;}
  if(!Edge(harness,'Z')||!adapter.Snapshot().prone||
     std::abs(adapter.Snapshot().characterHeight-.6f)>.001f){writePostureFailure("prone-enter");return 92;}
  const auto beforeProneJump=adapter.Snapshot();
  if(!Edge(harness,VK_SPACE)){writePostureFailure("prone-jump-edge");return 92;}
  const auto afterProneJump=adapter.Snapshot();
  if(std::abs(afterProneJump.playerPosition.y-beforeProneJump.playerPosition.y)>.0001f||
     afterProneJump.jumps!=beforeProneJump.jumps){writePostureFailure("prone-jump-gate");return 92;}
  const auto crawlStart=adapter.Snapshot().playerPosition;
  if(!harness.Key(VK_SHIFT,true)||!harness.Key('W',true)){writePostureFailure("crawl-key-down");return 92;}
  for(unsigned tick=0;tick<120;++tick)if(!harness.Tick()){writePostureFailure("crawl-tick");return 92;}
  if(!harness.Key('W',false)||!harness.Key(VK_SHIFT,false)||!harness.Tick()){writePostureFailure("crawl-key-up");return 92;}
  const float crawlDistance=Aevrynd::Length(adapter.Snapshot().playerPosition-crawlStart);
  if(std::abs(crawlDistance-1.5f)>.05f){std::ofstream diagnostic(receiptPath,std::ios::trunc);diagnostic<<"{\"status\":\"posture-fail\",\"stage\":\"crawl-distance\",\"crawlDistance\":"<<crawlDistance<<"}";return 92;}
  if(!Edge(harness,'Z')||adapter.Snapshot().posture!=GameplayLab::PostureState::Standing){writePostureFailure("prone-exit");return 92;}
  if (!Edge(harness, VK_TAB) || !harness.MouseMove(100, 100) ||
      !harness.MouseButton(true) || !harness.Tick() ||
      !adapter.Snapshot().dragging || !adapter.Snapshot().cursorCaptured ||
      !harness.MouseButton(true, true) || !harness.MouseButton(false, true) ||
      !harness.Tick() || adapter.Snapshot().dragging ||
      adapter.Snapshot().cursorCaptured)
    return 93;
  const auto panelBefore=adapter.Snapshot();
  if(!harness.Key('W',true)||!harness.RawMouse(40,20)||!Edge(harness,'F')||
     !harness.Key('W',false)||!harness.Tick())return 94;
  const auto panelAfter=adapter.Snapshot();
  if(std::abs(panelAfter.playerPosition.x-panelBefore.playerPosition.x)>1e-6f||
     std::abs(panelAfter.playerPosition.z-panelBefore.playerPosition.z)>1e-6f||
     panelAfter.yaw!=panelBefore.yaw||panelAfter.pitch!=panelBefore.pitch||
     panelAfter.melee!=panelBefore.melee){std::ofstream diagnostic(receiptPath,std::ios::trunc);diagnostic<<"{\"status\":\"panel-ownership-fail\",\"dx\":"<<panelAfter.playerPosition.x-panelBefore.playerPosition.x<<",\"dz\":"<<panelAfter.playerPosition.z-panelBefore.playerPosition.z<<",\"dyaw\":"<<panelAfter.yaw-panelBefore.yaw<<",\"dpitch\":"<<panelAfter.pitch-panelBefore.pitch<<",\"meleeBefore\":"<<panelBefore.melee<<",\"meleeAfter\":"<<panelAfter.melee<<",\"paletteBefore\":"<<panelBefore.paletteOpen<<",\"paletteAfter\":"<<panelAfter.paletteOpen<<"}";return 94;}
  if (!Edge(harness, 'B') || !Edge(harness, 'G') || !Edge(harness, 'G') ||
      !harness.MouseButton(true) || !harness.MouseButton(false) ||
      !harness.Tick() || !Edge(harness, 'F'))
    return 86;
  if(!Edge(harness,'B')||!Edge(harness,VK_ESCAPE)||adapter.Snapshot().closed||
     adapter.Snapshot().paletteOpen||!Edge(harness,VK_ESCAPE)||
     !adapter.Snapshot().closed||!harness.Close())return 86;
  const auto state = adapter.Snapshot();
  if (!state.closed || state.cameraDistance < .25f ||
      state.cameraDistance > 3.4f || harness.Receipts().empty())
    return 87;
  const std::array rateHashes{RunRate(30),RunRate(60),RunRate(144)};
  if(!rateHashes[0]||rateHashes[0]!=rateHashes[1]||rateHashes[0]!=rateHashes[2])
    return 89;
  std::filesystem::create_directories(receiptPath.parent_path());
  std::ofstream out(receiptPath, std::ios::trunc);
  out << "{\"source\":\"GameplayLab--ux-replay\",\"status\":\"pass\","
         "\"receipts\":" << harness.Receipts().size()
      << ",\"stateHash\":" << harness.Receipts().back().stateHash
      << ",\"cameraToggles\":" << state.cameraToggles
      << ",\"walkDistance\":" << walkDistance
      << ",\"runDistance\":" << runDistance
      << ",\"runRatio\":" << runDistance/walkDistance
      << ",\"crouchDistance\":" << crouchDistance
      << ",\"crawlDistance\":" << crawlDistance
      << ",\"proneHeight\":" << afterProneJump.characterHeight
      << ",\"proneToggles\":" << state.proneToggles
      << ",\"lowPostureJumpBlocked\":1"
      << ",\"rateHash\":" << rateHashes[0]
      << ",\"crouched\":" << (state.crouched ? 1 : 0)
      << ",\"cursorCaptured\":" << (state.cursorCaptured ? 1 : 0)
      << ",\"uiPriority\":1,\"panelOwnership\":1,\"focusRelease\":1,"
         "\"rawMouse\":1,\"repeatedKeydown\":1,\"escapeCancel\":1,"
         "\"dragCancel\":1,\"grab\":1,\"melee\":1,"
         "\"rates\":\"30,60,144\"}";
  return out ? 0 : 88;
}
