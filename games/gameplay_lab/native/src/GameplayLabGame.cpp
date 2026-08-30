#include "GameplayLabGame.h"
#include <array>
#include <cctype>
#include <format>
#include <fstream>
#include <objbase.h>
#include <ranges>
#include <unordered_set>

using namespace Aevrynd;using namespace GameplayLab;
namespace {constexpr std::array<const wchar_t*,11> Names={L"first-day.png",L"third-day.png",L"god-day.png",L"first-dusk.png",L"third-dusk.png",L"god-dusk.png",L"first-night.png",L"third-night.png",L"god-night.png",L"palette-building.png",L"melee-combat.png"};constexpr Color Cyan{.08f,.9f,.8f,1},Gold{1,.68f,.18f,1};std::string RunId(){GUID g{};if(FAILED(CoCreateGuid(&g)))throw std::runtime_error("Gameplay capture GUID unavailable");return std::format("{:08x}-{:04x}-{:04x}-{:02x}{:02x}-{:02x}{:02x}{:02x}{:02x}{:02x}{:02x}",g.Data1,g.Data2,g.Data3,g.Data4[0],g.Data4[1],g.Data4[2],g.Data4[3],g.Data4[4],g.Data4[5],g.Data4[6],g.Data4[7]);}}
std::string CreateGameplayLabRunId(){return RunId();}
namespace {std::uint64_t HashPose(const AnimationPose3D& pose){std::uint64_t hash=14695981039346656037ull;for(const auto& matrix:pose.globalMatrices)for(const auto& row:matrix.values)for(float value:row){const auto* bytes=reinterpret_cast<const unsigned char*>(&value);for(std::size_t i=0;i<sizeof(value);++i){hash^=bytes[i];hash*=1099511628211ull;}}return hash;}}
namespace {Matrix4 InverseAffineViewmodelSocket(const Matrix4& source){const double a=source[0][0],b=source[0][1],c=source[0][2],d=source[1][0],e=source[1][1],f=source[1][2],g=source[2][0],h=source[2][1],i=source[2][2],det=a*(e*i-f*h)-b*(d*i-f*g)+c*(d*h-e*g);if(!std::isfinite(det)||std::abs(det)<1e-12)throw std::runtime_error("Viewmodel grip socket is singular");Matrix4 inverse=Matrix4::Identity();inverse[0][0]=float((e*i-f*h)/det);inverse[0][1]=float((c*h-b*i)/det);inverse[0][2]=float((b*f-c*e)/det);inverse[1][0]=float((f*g-d*i)/det);inverse[1][1]=float((a*i-c*g)/det);inverse[1][2]=float((c*d-a*f)/det);inverse[2][0]=float((d*h-e*g)/det);inverse[2][1]=float((b*g-a*h)/det);inverse[2][2]=float((a*e-b*d)/det);const float tx=source[0][3],ty=source[1][3],tz=source[2][3];inverse[0][3]=-(inverse[0][0]*tx+inverse[0][1]*ty+inverse[0][2]*tz);inverse[1][3]=-(inverse[1][0]*tx+inverse[1][1]*ty+inverse[1][2]*tz);inverse[2][3]=-(inverse[2][0]*tx+inverse[2][1]*ty+inverse[2][2]*tz);return inverse;}}
namespace {void PostCombatFinishInput(Engine& engine,std::uint64_t tick){if(tick==132||tick==166||(tick>=200&&tick<=440&&tick%40==0)){PostMessageW(engine.Window(),WM_LBUTTONDOWN,0,0);PostMessageW(engine.Window(),WM_LBUTTONUP,0,0);}}}
namespace {
void PostReplayInput(Engine &engine, std::uint64_t tick) {
  const auto tap = [&](WPARAM key) {
    PostMessageW(engine.Window(), WM_KEYDOWN, key, 0);
    PostMessageW(engine.Window(), WM_KEYUP, key, 0);
  };
  switch (tick) {
  case 2:
    tap('1');
    break;
  case 4:
    tap('2');
    break;
  case 6:
    tap('3');
    break;
  case 8:
    tap('V');
    break;
  case 10:
    PostMessageW(engine.Window(), WM_KEYDOWN, 'W', 0);
    break;
  case 12:
    PostMessageW(engine.Window(), WM_KEYDOWN, VK_SHIFT, 0);
    break;
  case 14:
    tap(VK_SPACE);
    break;
  case 16:
    tap('C');
    break;
  case 18:
    PostMessageW(engine.Window(), WM_KEYUP, VK_SHIFT, 0);
    tap('C');
    break;
  case 20:
    PostMessageW(engine.Window(), WM_KEYUP, 'W', 0);
    tap('3');
    break;
  case 21:
    tap('C');
    break;
  case 22:
    tap(VK_TAB);
    break;
  case 24:
    PostMessageW(engine.Window(), WM_MOUSEMOVE, 0, MAKELPARAM(640, 360));
    PostMessageW(engine.Window(), WM_LBUTTONDOWN, 0, 0);
    break;
  case 25:
    PostMessageW(engine.Window(), WM_MOUSEMOVE, 0, MAKELPARAM(700, 390));
    break;
  case 26:
    tap('R');
    PostMessageW(engine.Window(), WM_LBUTTONUP, 0, 0);
    break;
  case 28:
    PostMessageW(engine.Window(), WM_RBUTTONDOWN, 0, 0);
    PostMessageW(engine.Window(), WM_RBUTTONUP, 0, 0);
    break;
  case 30:
    tap(VK_DELETE);
    break;
  case 32:
    PostMessageW(engine.Window(), WM_KEYDOWN, VK_CONTROL, 0);
    tap('Z');
    PostMessageW(engine.Window(), WM_KEYUP, VK_CONTROL, 0);
    break;
  case 34:
    PostMessageW(engine.Window(), WM_KEYDOWN, VK_CONTROL, 0);
    tap('Y');
    PostMessageW(engine.Window(), WM_KEYUP, VK_CONTROL, 0);
    break;
  case 36:
    tap(VK_F2);
    break;
  case 38:
    tap(VK_F3);
    break;
  case 40:
    tap(VK_F5);
    break;
  case 42:
    tap(VK_F6);
    break;
  case 44:
    tap(VK_F7);
    break;
  case 46:
    tap('T');
    break;
  case 48:
    tap('4');
    break;
  case 50:
    tap('5');
    break;
  case 52:
    tap('6');
    break;
  case 54:
    tap('7');
    break;
  case 56:
    tap('8');
    break;
  case 58:
    tap('9');
    break;
  case 59:
    tap(VK_TAB);
    break;
  case 60:
  case 78:
  case 96:
  case 114:
    PostMessageW(engine.Window(), WM_LBUTTONDOWN, 0, 0);
    PostMessageW(engine.Window(), WM_LBUTTONUP, 0, 0);
    break;
  case 260:
    PostMessageW(engine.Window(), WM_KEYDOWN, 'G', 0);
    break;
  case 268:
    PostMessageW(engine.Window(), WM_KEYUP, 'G', 0);
    break;
  default:
    break;
  }
}
} // namespace
ReusableMemoryStats GameplayLabGame::GetReusableMemoryStats()const{const auto c=collisions_.MemoryStats(),p=physics_.MemoryStats(),s=scene_.MemoryStats(),k=skinning_.MemoryStats(),fa=femaleAnimation_.MemoryStats(),ma=maleAnimation_.MemoryStats(),mo=monsterAnimation_.MemoryStats(),ff=femaleFootPlacement_.MemoryStats(),mf=maleFootPlacement_.MemoryStats(),ph=playerHistory_.MemoryStats(),mh=monsterHistory_.MemoryStats();return{c.retainedCapacityBytes+p.retainedCapacityBytes+s.retainedCapacityBytes+k.retainedCapacityBytes+fa.retainedCapacityBytes+ma.retainedCapacityBytes+mo.retainedCapacityBytes+ff.retainedCapacityBytes+mf.retainedCapacityBytes+ph.retainedCapacityBytes+mh.retainedCapacityBytes,c.reclaimableCapacityBytes+p.reclaimableCapacityBytes+s.reclaimableCapacityBytes+k.reclaimableCapacityBytes+fa.reclaimableCapacityBytes+ma.reclaimableCapacityBytes+mo.reclaimableCapacityBytes+ff.reclaimableCapacityBytes+mf.reclaimableCapacityBytes+ph.reclaimableCapacityBytes+mh.reclaimableCapacityBytes,released_,trims_};}
std::size_t GameplayLabGame::TrimReusableMemory(){const auto before=GetReusableMemoryStats().retainedCapacityBytes;collisions_.TrimUnusedMemory();physics_.TrimUnusedMemory();scene_.TrimUnusedMemory();skinning_.TrimUnusedMemory();femaleAnimation_.TrimUnusedMemory();maleAnimation_.TrimUnusedMemory();monsterAnimation_.TrimUnusedMemory();femaleFootPlacement_.TrimUnusedMemory();maleFootPlacement_.TrimUnusedMemory();playerHistory_.TrimUnusedMemory();monsterHistory_.TrimUnusedMemory();const auto after=GetReusableMemoryStats().retainedCapacityBytes;released_=before>after?before-after:0;++trims_;return released_;}
void GameplayLabGame::OnStart(Engine& engine){
  std::array<char,128> expectedFingerprint{};
  const DWORD expectedLength=GetEnvironmentVariableA("AEVRYND_EXPECTED_CHARACTER_FINGERPRINT",expectedFingerprint.data(),DWORD(expectedFingerprint.size()));
  if(expectedLength&&expectedLength<expectedFingerprint.size())expectedSourceFingerprint_.assign(expectedFingerprint.data(),expectedLength);
  sourceFingerprintPassed_=expectedSourceFingerprint_.empty()||expectedSourceFingerprint_==actualSourceFingerprint_;
  if(!sourceFingerprintPassed_)throw std::runtime_error("Gameplay character build fingerprint mismatch");
  std::array<char,32768> phasePath{};const DWORD phaseLength=GetEnvironmentVariableA("AEVRYND_FATAL_DIAGNOSTIC_PATH",phasePath.data(),DWORD(phasePath.size()));if(phaseLength&&phaseLength<phasePath.size()){std::ofstream phase(std::filesystem::path(std::string(phasePath.data(),phaseLength)),std::ios::app);phase<<"BuildFingerprint:"<<actualSourceFingerprint_<<'\n';phase.flush();}
  rules_.Reset();controller_.Reset({0,0,5});
  boxMesh_=engine.CreateMeshResource3D(Mesh3D::CreateUnitBox());
  sphereMesh_=engine.CreateMeshResource3D(Mesh3D::CreateUnitSphere(18,12));
  const auto assetRoot=engine.ExecutableDirectory()/"assets";
  const auto modelRoot=assetRoot/"model-packs";
  const auto materialRoot=assetRoot/"material-packs"/"original";
  auto female=LoadSkinnedModelAssetFileDetailed(modelRoot/"realistic-female"/"realistic-female.aeskin");
  auto male=LoadSkinnedModelAssetFileDetailed(modelRoot/"realistic-male"/"realistic-male.aeskin");
  if(!female||!male)throw std::runtime_error("Gameplay Lab requires the existing F2/M2 character packs");
  std::string error;
  if(!LoadCharacterMaterialBindings3D(engine,femaleMaterialLibrary_,female.asset,materialRoot,materialRoot/"female.materials.json",femaleMaterials_,&error)||
     !LoadCharacterMaterialBindings3D(engine,maleMaterialLibrary_,male.asset,materialRoot,materialRoot/"male.materials.json",maleMaterials_,&error))
    throw std::runtime_error(error);
  if(femaleMaterials_.size()!=9||maleMaterials_.size()!=9)throw std::runtime_error("Gameplay character material count mismatch");
  if(!femaleAnimation_.Initialize(female.asset,&error)||!maleAnimation_.Initialize(male.asset,&error)||!monsterAnimation_.Initialize(male.asset,&error))
    throw std::runtime_error(error);
  CharacterEquipmentAsset3D femaleWeapon{},maleWeapon{};
  if(!CreateCharacterEquipmentAsset3D(female.asset,femaleAnimation_.Rig(),CharacterEquipmentShape3D::Sword,femaleWeapon,&error)||
     !CreateCharacterEquipmentAsset3D(male.asset,maleAnimation_.Rig(),CharacterEquipmentShape3D::Sword,maleWeapon,&error))
    throw std::runtime_error(error);
  femaleWeaponSubmeshes_=femaleWeapon.submeshes;maleWeaponSubmeshes_=maleWeapon.submeshes;
  femaleWeaponMaterials_={femaleMaterials_[8],femaleMaterials_[7]};
  maleWeaponMaterials_={maleMaterials_[8],maleMaterials_[7]};
  femaleSubmeshes_=female.asset.submeshes;maleSubmeshes_=male.asset.submeshes;
  femaleModel_=std::move(female.asset.model);maleModel_=std::move(male.asset.model);
  femaleMesh_=engine.CreateSkinnedMeshResource3D(femaleModel_);
  maleMesh_=engine.CreateSkinnedMeshResource3D(maleModel_);
  femaleWeaponMesh_=engine.CreateSkinnedMeshResource3D(femaleWeapon.model);
  maleWeaponMesh_=engine.CreateSkinnedMeshResource3D(maleWeapon.model);
  if(!femaleMesh_||!maleMesh_||!femaleWeaponMesh_||!maleWeaponMesh_)throw std::runtime_error("Gameplay character GPU resource creation failed");
  if(!femaleFootPlacement_.Prepare(femaleModel_.skeleton,femaleAnimation_.Rig(),femaleModel_.skeletonCompatibilityId)||
     !maleFootPlacement_.Prepare(maleModel_.skeleton,maleAnimation_.Rig(),maleModel_.skeletonCompatibilityId))
    throw std::runtime_error("Gameplay foot placement preparation failed");
  for(unsigned state=0;state<11;++state){
    femaleAnimation_.SetState(AnimationState3D(state),true);femaleAnimation_.Tick(.173f);
    maleAnimation_.SetState(AnimationState3D(state),true);maleAnimation_.Tick(.173f);
    if(!femaleAnimation_.EvaluateInto(femalePose_)||!maleAnimation_.EvaluateInto(malePose_))throw std::runtime_error("Fallback animation evaluation failed");
    femalePoseHashes_[state]=HashPose(femalePose_);malePoseHashes_[state]=HashPose(malePose_);
  }
  femaleAnimation_.SetState(AnimationState3D::Idle,true);maleAnimation_.SetState(AnimationState3D::Idle,true);monsterAnimation_.SetState(AnimationState3D::Idle,true);
  scene_.SetBoxMesh(boxMesh_);
  collisions_.CreateBox({{{0,-.5f,0},{30,.5f,30}},1,0xffffffffu});
  collisions_.CreateBox({{{0,1.5f,-12},{4,1.5f,.6f}},1,0xffffffffu});
  physics_.CreateBody({RigidBodyType3D::Static,{{0,-.5f,0},{},{30,1,30}},{30,.5f,30}});
  for(int i=0;i<18;++i){const Vec3 p{-8.f+float(i%6)*1.6f,.65f+float(i/6)*1.25f,-3.f};if(!palette_->PlaceAt(p,PaletteCategory::Physics))throw std::runtime_error("Initial gameplay palette transaction failed");}
  if(!captureDirectory_.empty()){
    std::filesystem::create_directories(captureDirectory_);
    std::unordered_set<std::uint64_t> femaleUnique(femalePoseHashes_.begin(),femalePoseHashes_.end()),maleUnique(malePoseHashes_.begin(),malePoseHashes_.end());
    if(femaleUnique.size()!=11||maleUnique.size()!=11||std::ranges::any_of(femalePoseHashes_,[](auto value){return value==0;})||std::ranges::any_of(malePoseHashes_,[](auto value){return value==0;}))throw std::runtime_error("Semantic gameplay pose hashes are not unique");
    std::ofstream poseOut(captureDirectory_/"gameplay-pose-receipt.json",std::ios::trunc);
    poseOut<<"{\"runId\":\""<<runId_<<"\",\"source\":\"GameplayLabSemanticSkinning\",\"female\":[";
    for(std::size_t i=0;i<11;++i){if(i)poseOut<<",";poseOut<<femalePoseHashes_[i];}
    poseOut<<"],\"male\":[";
    for(std::size_t i=0;i<11;++i){if(i)poseOut<<",";poseOut<<malePoseHashes_[i];}
    poseOut<<"]}";
    std::ofstream jointOut(captureDirectory_/"weapon-joint.txt",std::ios::trunc);
    jointOut<<maleAnimation_.Rig().Joint(HumanoidJoint3D::RightHand)<<" RightHand";
    for(auto name:Names){std::error_code ec;std::filesystem::remove(captureDirectory_/name,ec);}
  }
}
void GameplayLabGame::OnStop(Engine &engine) {
  palette_->EndGrab();
  if (!captureDirectory_.empty()) {
    const auto grab = palette_->Stats();
    std::ofstream out(captureDirectory_ / "gameplay-grab-receipt.json",
                      std::ios::trunc);
    out << std::format(
        "{{\"runId\":\"{}\",\"source\":\"GameplayLabGrab\",\"pickAttempts\":{},"
        "\"pickRejected\":{},\"grabBegins\":{},\"grabHoldSteps\":{},"
        "\"releaseDrops\":{},\"activeAtStop\":{}}}",
        runId_, grab.pickAttempts, grab.pickRejected, grab.grabs,
        grab.dragSteps, grab.drops, palette_->Grabbed() ? 1 : 0);
  }
  palette_->Clear();
  physics_.Clear();
  collisions_.Clear();
  scene_.SetBoxMesh({});
  playerHistory_.DiscardSubmitted();
  monsterHistory_.DiscardSubmitted();
  playerHistory_.Invalidate();
  monsterHistory_.Invalidate();
  femaleMaterialLibrary_.Shutdown();
  maleMaterialLibrary_.Shutdown();
  engine.DestroySkinnedMesh3D(femaleMesh_.handle);
  engine.DestroySkinnedMesh3D(maleMesh_.handle);
  engine.DestroySkinnedMesh3D(femaleWeaponMesh_.handle);
  engine.DestroySkinnedMesh3D(maleWeaponMesh_.handle);
  engine.DestroyMesh3D(boxMesh_.handle);
  engine.DestroyMesh3D(sphereMesh_.handle);
  femaleMesh_ = {};
  maleMesh_ = {};
  femaleWeaponMesh_ = {};
  maleWeaponMesh_ = {};
  femaleModel_ = {};
  maleModel_ = {};
  femaleSubmeshes_.clear();maleSubmeshes_.clear();femaleWeaponSubmeshes_.clear();maleWeaponSubmeshes_.clear();femaleMaterials_.clear();maleMaterials_.clear();
  boxMesh_ = {};
  sphereMesh_ = {};
  skinning_.TrimUnusedMemory();
  TrimReusableMemory();
  if(!captureDirectory_.empty()){
    engine.TrimUnusedMemory();
    const auto renderer=engine.GetRendererStats();
    const auto memory=engine.GetMemoryStats();
    std::ofstream releaseOut(captureDirectory_/"gameplay-character-release-receipt.json",std::ios::trunc);
    releaseOut<<std::format("{{\"runId\":\"{}\",\"source\":\"GameplayLabCharacterRelease\",\"sourceFingerprint\":\"{}\",\"residentSkinnedMeshes\":{},\"residentSkinnedMeshBytes\":{},\"residentMaterialTextureSets\":{},\"residentMaterialTextureReferences\":{},\"skinnedMeshReleases\":{},\"engineTrimCount\":{},\"gameTrimCount\":{},\"gameRetainedBytes\":{},\"gameReclaimableBytes\":{}}}",runId_,actualSourceFingerprint_,renderer.residentSkinnedMeshes,renderer.residentSkinnedMeshBytes,renderer.residentMaterialTextureSets,renderer.residentMaterialTextureReferences,renderer.skinnedMeshReleases,memory.trimCount,memory.game.trimCount,memory.game.retainedCapacityBytes,memory.game.reclaimableCapacityBytes);
  }
}
void GameplayLabGame::UpdateCamera() {
  const Vec3 feet = controller_.Position(), focus = feet + Vec3{0, 1.1f, 0};
  switch (rules_.Camera()) {
  case CameraMode::FirstPerson:
    camera_ = MakeFirstPersonCamera(
        feet + Vec3{0, rules_.CharacterHeight() - .12f, 0}, yaw_, pitch_);
    break;
  case CameraMode::ThirdPerson:
    camera_ = ConstrainThirdPersonCamera(
        MakeThirdPersonCamera(focus, yaw_, .24f, 3.4f), focus, collisions_,
        .25f);
    break;
  default:
    camera_ = MakeOverheadCamera(focus, 22, true);
    break;
  }
}
void GameplayLabGame::OnUpdate(Engine &engine, float dt) {
  simulationDelta_=GameplayPresentationDelta3D(dt);
  Input &in = engine.GetInput();
  if (in.Pressed(VK_ESCAPE))
    engine.Quit();
  if (!combatSpawned_) {
    combat_->SpawnMonster({4, 0, 3});
    combatSpawned_ = true;
  }
  const ProductionInput3D mapped = ReadProductionInput3D(in, yaw_);
  if (mapped.camera)
    rules_.SetCamera(*mapped.camera);
  if (mapped.cycleCamera)
    rules_.CycleCamera();
  if (mapped.lighting)
    rules_.SetLighting(*mapped.lighting);
  if (mapped.cycleLighting)
    rules_.CycleLighting();
  if (mapped.category)
    rules_.SetPaletteCategory(*mapped.category);
  if (mapped.female)
    rules_.SetCharacterFemale(true);
  if (mapped.male)
    rules_.SetCharacterFemale(false);
  yaw_ -= mapped.mouseDelta.x * .0025f;
  pitch_ = Clamp(pitch_ - mapped.mouseDelta.y * .0025f, -.75f, .9f);
  const auto beforeCrouchPosition = controller_.Position();
  if (!captureDirectory_.empty() && rules_.GetStats().ticks == 16 &&
      !captureCeiling_)
    captureCeiling_ = collisions_.CreateBox(
        {{{beforeCrouchPosition.x, beforeCrouchPosition.y + 1.45f,
           beforeCrouchPosition.z},
          {1, .15f, 1}},
         8,
         0xffffffffu,
         0xc001,
         false});
  const Vec3 standingCenter = controller_.Position() + Vec3{0, .9f, 0};
  rules_.SetHeadroom(
      !collisions_.OverlapsBox(standingCenter, {.44f, .89f, .44f}));
  TickInput rulesInput = mapped.tick;
  rulesInput.attack = false;
  rulesInput.movement = {};
  rulesInput.jump = false;
  rules_.Tick(rulesInput, dt);
  controller_.SetHeight(rules_.Crouched() ? 1.15f : 1.8f);
  minimumControllerHeight_ =
      (std::min)(minimumControllerHeight_, controller_.Extents().y * 2.f);
  crouchFeetInvariant_ =
      crouchFeetInvariant_ &&
      Length(controller_.Position() - beforeCrouchPosition) < 1e-6f;
  if (captureCeiling_ && rules_.GetStats().ticks >= 20) {
    collisions_.Destroy(captureCeiling_);
    captureCeiling_ = {};
  }
  CharacterControl3D control{mapped.tick.movement, mapped.tick.jump};
  controller_.Step(control, collisions_, dt);
  const auto gameplayTick = rules_.GetStats().ticks;
  if (!captureDirectory_.empty() && gameplayTick >= 258 &&
      gameplayTick <= 268)
    rules_.SetCamera(CameraMode::FirstPerson);
  UpdateCamera();
  Ray3D cursorRay = ScreenPointToRay(camera_, {1280, 720}, mapped.mouse);
  if (!captureDirectory_.empty() && gameplayTick == 258 &&
      !captureGrabSeeded_) {
    const float directionLength = Length(cursorRay.direction);
    if (!std::isfinite(directionLength) || directionLength < .0001f)
      throw std::runtime_error("Capture grab ray was invalid");
    const Vec3 seedPosition =
        cursorRay.origin + cursorRay.direction * (3.f / directionLength);
    if (!palette_->PlaceAt(seedPosition, PaletteCategory::Physics))
      throw std::runtime_error("Capture grab seed transaction failed");
    rules_.SetPaletteCategory(PaletteCategory::Lights);
    captureGrabSeeded_ = true;
  }
  if (in.Pressed('G') && !rules_.PaletteOpen())
    palette_->BeginGrab(cursorRay);
  if (palette_->Grabbed()) {
    if (in.Down('G'))
      palette_->UpdateGrab(cursorRay, dt);
    else
      palette_->EndGrab();
  }
  const Vec3 forward{std::sin(yaw_), 0, std::cos(yaw_)};
  combat_->Tick({controller_.Position(), forward,
                 mapped.tick.attack && !rules_.PaletteOpen()},
                dt);
  if (combat_->CurrentAttack() == Attack3D::Attack3 && combat_->InHitWindow())
    weaponHitWindowEvidence_ = true;
  AnimationState3D animation = AnimationState3D::Idle;
  if (combat_->CurrentAttack() != Attack3D::None)
    animation = AnimationState3D(unsigned(AnimationState3D::Attack1) +
                                 unsigned(combat_->CurrentAttack()) - 1);
  else if (!controller_.Grounded())
    animation = controller_.Velocity().y >= 0 ? AnimationState3D::Jump
                                              : AnimationState3D::Fall;
  else if (rules_.Crouched())
    animation = AnimationState3D::Crouch;
  else if (Length(mapped.tick.movement) > .1f)
    animation =
        mapped.tick.run ? AnimationState3D::Run : AnimationState3D::Walk;
  if (!captureDirectory_.empty() && captureIndex_ == 4)
    animation = AnimationState3D::Crouch;
  femaleAnimation_.SetState(animation);
  maleAnimation_.SetState(animation);
  femaleAnimation_.Tick(dt);
  maleAnimation_.Tick(dt);
  if(combat_->CurrentAttack()!=Attack3D::None){
    femaleAnimation_.SynchronizeNormalizedPhase(combat_->NormalizedAttackPhase());
    maleAnimation_.SynchronizeNormalizedPhase(combat_->NormalizedAttackPhase());
  }
  monsterAnimation_.SetState(combat_->Monster().alive
                                 ? AnimationState3D::Walk
                                 : AnimationState3D::Death);
  monsterAnimation_.Tick(dt);
  if(combat_->CurrentAttack()!=Attack3D::None&&
     (captureDirectory_.empty()||captureIndex_!=4)){
    const float error=std::abs(femaleAnimation_.NormalizedPhase()-combat_->NormalizedAttackPhase());
    maximumWeaponPhaseError_=(std::max)(maximumWeaponPhaseError_,error);
    weaponAnimationSyncPassed_=weaponAnimationSyncPassed_&&error<=.035f;
  }
  physics_.Step(dt, 2);
  if (!captureDirectory_.empty() && rules_.GetStats().ticks == 240)
    captureAuthoritativePhysicsSteps_ = physics_.Stats().steps;
  if (!palette_->SyncDynamicTransforms())
    engine.Quit();
  if (rules_.PaletteOpen() && mapped.tick.attack && !palette_->Preview().active)
    palette_->Begin(cursorRay, rules_.Category());
  if (palette_->Preview().active) {
    palette_->Update(cursorRay, mapped.rotate);
    if (!in.MouseDown())
      palette_->Commit();
  }
  if (mapped.tick.cancel)
    palette_->Cancel();
  if (mapped.tick.erase && !rules_.Entities().empty())
    palette_->Delete(rules_.Entities().back().id);
  if (mapped.tick.undo)
    palette_->Undo();
  if (mapped.tick.redo)
    palette_->Redo();
  UpdateCapture(engine);
}
void GameplayLabGame::BuildWorld(Engine &engine) {
  scene_.Clear();
  scene_.SetBoxMesh(boxMesh_);
  scene_.MaterialBox({0, -.5f, 0}, {60, 1, 60},
                     {{.12f, .18f, .13f, 1}, {.78f, 0, 0, .9f}, false});
  scene_.MaterialBox({0, 1.5f, -12}, {8, 3, 1.2f},
                     {{.24f, .13f, .08f, 1}, {.48f, 0, 0, 1}, false});
  for (int i = -28; i <= 28; i += 2) {
    scene_.Line({float(i), .011f, -28}, {float(i), .011f, 28}, 1,
                {.12f, .2f, .17f, .5f});
    scene_.Line({-28, .011f, float(i)}, {28, .011f, float(i)}, 1,
                {.12f, .2f, .17f, .5f});
  }
  for (auto handle : bodies_)
    if (const auto *b = physics_.GetBody(handle))
      scene_.MaterialBox(b->settings.transform.position,
                         b->settings.colliderExtents * 2,
                         {{.18f, .28f, .38f, 1}, {.38f, .12f, 0, 1}, false});
  for (const auto &placed : rules_.Entities())
    if (const auto *body = physics_.GetBody(palette_->Body(placed.id)))
      scene_.MaterialBox(
          body->settings.transform.position, body->settings.colliderExtents * 2,
          {{placed.id <= 18 ? .18f : .8f, placed.id <= 18 ? .28f : .42f,
            placed.id <= 18 ? .38f : .12f, 1},
           {.32f, .08f, 0, 1},
           false});
  const auto preview = palette_->Preview();
  if (preview.active)
    scene_.MaterialBox(
        preview.position, {1.04f, 1.04f, 1.04f},
        {{preview.valid ? .15f : .9f, preview.valid ? .85f : .12f, .08f, .55f},
         {.7f, 0, 0, .5f},
         false});
  if (captureIndex_ >= 9) {
    if (const auto valid = captureValidGhost_)
      scene_.MaterialBox(*valid, {.9f, .9f, .9f},
                         {{.02f, 1.f, .05f, 1.f}, {.1f, 0, 0, 1.f}, false});
    if (const auto invalid = captureInvalidGhost_)
      scene_.MaterialBox(*invalid, {1.25f, 1.25f, 1.25f},
                         {{1.f, .01f, .01f, .9f}, {.1f, 0, 0, 1.f}, false});
  }
  const LightingState light = rules_.EvaluateLighting();
  engine.SetClearColor(light.clearColor);
  DrawList &draw = engine.Draw();
  draw.SetDirectionalLight(
      {light.sunDirection, .65f, light.sunColor, light.intensity});
  draw.SetAtmosphere({light.fogColor, camera_.position, 18, 120, true,
                      light.fogDensity, .12f, 0, .9f});
  draw.SetExposureCompensation(std::log2(light.exposure));
  draw.SetVolumetricClouds(
      {true, 1200, 1800, .48f, 1, .22f, 12, 12, true, .6f});
  const auto grabbed = palette_->Grabbed();
  const auto grabbedCategory = palette_->GrabbedCategory();
  draw.Quad({1025, 575}, {490, 122}, {.008f, .018f, .022f, .9f});
  draw.Text({790, 528}, "[G HOLD] GRAB / RELEASE", 1.1f, Cyan);
  draw.Text({790, 554},
            std::format("ACTIVE {}  ID {}  CLASS {}", grabbed ? 1 : 0,
                        grabbed.value_or(0),
                        unsigned(grabbedCategory.value_or(rules_.Category()))),
            1.05f, grabbed ? Gold : Cyan);
  draw.Text(
      {790, 580},
      std::format(
          "HOLD {:.2f}  ERROR {:.3f}",
          grabbed ? Length(palette_->GrabTarget() - camera_.position) : 0.f,
          palette_->GrabError()),
      1.05f, Gold);
  scene_.Submit(draw, camera_, {1280, 720},
                {true, true, {true, .02f, .2f, 80, 256}, {150, .1f}});
  scene_.Clear();
}
void GameplayLabGame::UpdateCapture(Engine &engine) {
  if (captureDirectory_.empty())
    return;
  const auto replayTick = rules_.GetStats().ticks;
  PostReplayInput(engine, replayTick);
  PostCombatFinishInput(engine, replayTick);
  const auto renderer = engine.GetRendererStats();
  if (capturePending_ && (renderer.successfulFrameCaptures > captureSuccess_ ||
                          renderer.rejectedFrameCaptures > captureReject_)) {
    if (renderer.successfulFrameCaptures > captureSuccess_) {
      ++captureIndex_;
      // 第三人稱黃昏固定展示 C 蹲姿，下一張之前恢復站姿。
      if (captureIndex_ == 4 || captureIndex_ == 5) {
        PostMessageW(engine.Window(), WM_KEYDOWN, 'C', 0);
        PostMessageW(engine.Window(), WM_KEYUP, 'C', 0);
      }
    }
    capturePending_ = false;
    nextCapture_ = engine.Time() + .2;
  }
  if (captureIndex_ == Names.size()) {
    std::ofstream out(captureDirectory_ / "gameplay-receipt.json",
                      std::ios::trunc);
    const auto s = rules_.GetStats();
    const auto p = physics_.Stats();
    const auto r = engine.GetRendererStats();
    const auto c = combat_->Stats();
    const auto palette = palette_->Stats();
    const auto femaleMaterialStats = femaleMaterialLibrary_.Stats();
    const auto maleMaterialStats = maleMaterialLibrary_.Stats();
    if (captureAuthoritativePhysicsSteps_ != 240)
      throw std::runtime_error(
          "Gameplay capture missed its authoritative physics tick");
    out << std::format(
        "{{\"runId\":\"{}\",\"source\":\"GameplayLabFramebuffer\",\"sourceFingerprint\":\"{}\",\"sourceFingerprintPassed\":{},\"entry\":\"-"
        "-capture-demo\",\"captures\":11,\"ticks\":{},"
        "\"authoritativePhysicsTick\":240,\"authoritativePhysicsSteps\":{},"
        "\"entities\":{},\"physicsBodies\":{},"
        "\"physicsSteps\":{},\"cameraSwitches\":{},\"jumps\":{},\"crouches\":{}"
        ",\"palettePreviews\":{},\"paletteValidPreviews\":{},"
        "\"paletteInvalidPreviews\":{},\"selectedCategory\":{},"
        "\"paletteCommits\":{},\"combatSweeps\":{},\"combatHits\":{},"
        "\"combatKills\":{},\"monsterHealth\":{},\"animationState\":{},"
        "\"monsterAnimationState\":{},\"corpseVisible\":{},\"femaleGpuDraws\":{"
        "},\"maleGpuDraws\":{},\"femaleModelIdentity\":{},"
        "\"maleModelIdentity\":{},\"femalePoseHash\":{},\"malePoseHash\":{},"
        "\"minimumControllerHeight\":{},\"headroomRejected\":{},"
        "\"crouchFeetInvariant\":{},\"weaponAttachmentPassed\":{},"
        "\"weaponHitWindowEvidence\":{},\"weaponPbrDraws\":{},"
        "\"weaponAnimationSyncPassed\":{},\"maximumWeaponPhaseError\":{},"
        "\"firstPersonBodyHidden\":{},\"weaponTrajectoryHash\":{},"
        "\"attackTrajectoryHashes\":[{},{},{}],\"characterMaterialCount\":{},"
        "\"characterMaterialLoadFailures\":{},\"skinnedGroups\":{},"
        "\"skinnedInstances\":{},\"rejectedSkinnedGroups\":{},"
        "\"residentSkinnedMeshBytes\":{},\"gpuValidationRequested\":{},"
        "\"gpuValidationAvailable\":{},\"gpuValidationExecuted\":{},"
        "\"debugLayerEnabled\":{},\"gpuValidationPassed\":{},\"gpuErrors\":{},"
        "\"gpuWarnings\":{},\"lighting\":[{{\"phase\":\"day\",\"sun\":[0.38,-1."
        "0,0.24],\"intensity\":1.8,\"exposure\":1.0,\"fogDensity\":0.006,"
        "\"shadow\":1}},{{\"phase\":\"dusk\",\"sun\":[-0.65,-0.34,0.28],"
        "\"intensity\":1.2,\"exposure\":0.9,\"fogDensity\":0.014,\"shadow\":1}}"
        ",{{\"phase\":\"night\",\"sun\":[0.25,-0.72,-0.34],\"intensity\":0.48,"
        "\"exposure\":0.72,\"fogDensity\":0.022,\"shadow\":1}}]}}",
        runId_, actualSourceFingerprint_,sourceFingerprintPassed_?1:0,s.ticks, captureAuthoritativePhysicsSteps_, s.entities,
        p.bodies, p.steps, s.cameraSwitches,
        controller_.Stats().jumps, s.crouches, palette.previews,
        palette.validPreviews, palette.collisionRejected,
        unsigned(rules_.Category()), palette.commits, c.sweeps, c.hits, c.kills,
        combat_->Monster().health, unsigned((rules_.Female()?femaleAnimation_:maleAnimation_).State()),
        unsigned(monsterAnimation_.State()),
        combatSpawned_ && !combat_->Monster().alive ? 1 : 0, femaleGpuDraws_,
        maleGpuDraws_, femaleMesh_.handle.resourceIdentity,
        maleMesh_.handle.resourceIdentity, HashPose(femalePose_),
        HashPose(malePose_), minimumControllerHeight_, s.headroomRejected,
        crouchFeetInvariant_ ? 1 : 0, weaponAttachmentPassed_ ? 1 : 0,
        weaponHitWindowEvidence_ ? 1 : 0, weaponPbrDraws_,
        weaponAnimationSyncPassed_ ? 1 : 0, maximumWeaponPhaseError_,
        firstPersonBodyHidden_ ? 1 : 0, weaponTrajectoryHash_,
        attackTrajectoryHashes_[0], attackTrajectoryHashes_[1], attackTrajectoryHashes_[2],
        femaleMaterialStats.materials + maleMaterialStats.materials,
        femaleMaterialStats.loadFailures + maleMaterialStats.loadFailures,
        r.lastSubmittedSkinnedGroups, r.lastSubmittedSkinnedInstances,
        r.rejectedSkinnedGroups, r.residentSkinnedMeshBytes,
        r.gpuValidationRequested ? 1 : 0,
        r.gpuValidationAvailable ? 1 : 0, r.gpuValidationExecuted ? 1 : 0,
        r.debugLayerEnabled ? 1 : 0, r.gpuValidationPassed ? 1 : 0,
        r.debugErrorCount, r.debugWarningCount);
    std::ofstream captureOut(captureDirectory_ / "gameplay-character-capture-receipt.json", std::ios::trunc);
    captureOut << "{\"runId\":\"" << runId_ << "\",\"source\":\"GameplayLabCharacterPresentation\",\"sourceFingerprint\":\""<<actualSourceFingerprint_<<"\",\"captures\":[";
    for(std::size_t index=0;index<Names.size();++index){
      if(index)captureOut<<",";
      const bool female=index<5;
      const char* camera=index<9?(index%3==0?"first":index%3==1?"third":"god"):"third";
      captureOut<<"{\"name\":\""<<std::filesystem::path(Names[index]).string()<<"\",\"character\":\""<<(female?"female-f2":"male-m2")<<"\",\"camera\":\""<<camera<<"\",\"crouch\":"<<(index==4?1:0)<<",\"attack\":"<<(index==10?1:0)<<",\"bodyMaterialSlots\":"<<captureBodyMaterialSlots_[index]<<",\"weaponMaterialRanges\":"<<captureWeaponMaterialRanges_[index]<<",\"weaponAnchorError\":"<<captureWeaponAnchorErrors_[index]<<",\"weaponScreenCoverage\":"<<captureWeaponScreenCoverage_[index]<<",\"weaponMinimumDepth\":"<<captureWeaponMinimumDepth_[index]<<",\"weaponCenterExcluded\":"<<unsigned(captureWeaponCenterExcluded_[index])<<",\"weaponNearClipClear\":"<<unsigned(captureWeaponNearClipClear_[index])<<",\"weaponHudOverlap\":"<<unsigned(captureWeaponHudOverlap_[index])<<",\"weaponGripX\":"<<captureWeaponGripX_[index]<<",\"weaponGripY\":"<<captureWeaponGripY_[index]<<",\"weaponBladeY\":"<<captureWeaponBladeY_[index]<<",\"gripEdgeAnchored\":"<<unsigned(captureWeaponGripEdgeAnchored_[index])<<",\"bladeAboveGrip\":"<<unsigned(captureWeaponBladeAboveGrip_[index])<<"}";
    }
    captureOut<<"]}";
    engine.Quit();
    return;
  }
  if (!capturePending_ && engine.Time() >= nextCapture_) {
    if (captureIndex_ == 10) {
      const auto grab = palette_->Stats();
      if (combat_->Monster().alive ||
          combat_->CurrentAttack() != Attack3D::Attack3 ||
          !combat_->InHitWindow() || !grab.grabs || !grab.dragSteps ||
          !grab.drops)
        return;
    }
    if (captureIndex_ == 9 && !captureEvidenceCollider_) {
      const Vec3 base = controller_.Position() + Vec3{0, 0, 2};
      captureEvidenceCollider_ =
          collisions_.CreateBox({{base + Vec3{1, .5f, 0}, {.5f, .5f, .5f}},
                                 4,
                                 0xffffffffu,
                                 0xfeed,
                                 false});
      if (!captureEvidenceCollider_ ||
          !palette_->PreviewAt(base + Vec3{-1, .5f, 0},
                               PaletteCategory::Building))
        throw std::runtime_error("Expected valid capture ghost");
      captureValidGhost_ = palette_->Preview().position;
      palette_->Cancel();
      if (palette_->PreviewAt(base + Vec3{1, .5f, 0},
                              PaletteCategory::Building))
        throw std::runtime_error("Expected invalid capture ghost");
      captureInvalidGhost_ = palette_->Preview().position;
      palette_->Cancel();
      rules_.SetPaletteCategory(PaletteCategory::Lights);
    }
    if (captureIndex_ < 9) {
      rules_.SetCharacterFemale(captureIndex_ < 5);
      rules_.SetLighting(LightingPhase(captureIndex_ / 3));
      rules_.SetCamera(CameraMode(captureIndex_ % 3));
    } else {
      rules_.SetCamera(CameraMode::ThirdPerson);
      rules_.SetLighting(captureIndex_ == 9 ? LightingPhase::Day
                                            : LightingPhase::Dusk);
    }
    UpdateCamera();
    captureSuccess_ = renderer.successfulFrameCaptures;
    captureReject_ = renderer.rejectedFrameCaptures;
    capturePending_ =
        engine.CaptureNextFrame3D(captureDirectory_ / Names[captureIndex_]);
    nextCapture_ = engine.Time() + .25;
  }
}
void GameplayLabGame::OnRender(Engine& engine,float){
  if(playerHistory_.HasPendingSubmission())playerHistory_.ResolveSubmitted(engine);
  if(monsterHistory_.HasPendingSubmission())monsterHistory_.ResolveSubmitted(engine);
  BuildWorld(engine);
  if(!femaleAnimation_.EvaluateInto(femalePose_)||!maleAnimation_.EvaluateInto(malePose_)||!monsterAnimation_.EvaluateInto(monsterPose_))
    throw std::runtime_error("Gameplay fallback animation evaluation failed");
  const Matrix4 playerWorld=CreateTransformMatrix(controller_.Position(),{0,yaw_,0},{1,1,1});
  const Matrix4 playerWorldInverse=CreateRotationMatrix({0,-yaw_,0})*CreateTranslationMatrix(controller_.Position()*-1.f);
  FootPlacementFrame3D footFrame{};
  footFrame.collisionWorld=&collisions_;
  footFrame.modelToWorld=playerWorld;
  footFrame.worldToModel=playerWorldInverse;
  // Render 插值可能恰為零；足部放置必須使用最近一次固定模擬步長。
  footFrame.deltaTime=simulationDelta_;
  footFrame.cameraDistance=Length(camera_.position-controller_.Position());
  footFrame.frameIndex=rules_.GetStats().ticks;
  AnimationPose3D& activePose=rules_.Female()?femalePose_:malePose_;
  FootPlacement3D& activePlacement=rules_.Female()?femaleFootPlacement_:maleFootPlacement_;
  const SkinnedModel3D& activeModel=rules_.Female()?femaleModel_:maleModel_;
  const HumanoidSkeletonMap3D& activeRig=rules_.Female()?femaleAnimation_.Rig():maleAnimation_.Rig();
  FootPlacementResult3D footResult{};
  if(!activePlacement.Evaluate(activeModel.skeleton,activeRig,footFrame,activePose,&footResult))
    throw std::runtime_error("Gameplay foot placement evaluation failed");
  DrawList& draw=engine.Draw();
  const auto bodyMesh=rules_.Female()?femaleMesh_.handle:maleMesh_.handle;
  const auto weaponMesh=rules_.Female()?femaleWeaponMesh_.handle:maleWeaponMesh_.handle;
  const auto& bodySubmeshes=rules_.Female()?femaleSubmeshes_:maleSubmeshes_;
  const auto& bodyMaterials=rules_.Female()?femaleMaterials_:maleMaterials_;
  const auto& weaponSubmeshes=rules_.Female()?femaleWeaponSubmeshes_:maleWeaponSubmeshes_;
  const auto& weaponMaterials=rules_.Female()?femaleWeaponMaterials_:maleWeaponMaterials_;
  const auto palette=AddSkinPaletteFromPose(draw,activeModel,activePose,skinning_);
  Matrix4 weaponWorld=playerWorld;
  const bool firstPerson=rules_.Camera()==CameraMode::FirstPerson;
  if(rules_.Camera()==CameraMode::FirstPerson){
    Vec3 forward=Normalize(camera_.target-camera_.position),right=Normalize(Cross(camera_.up,forward)),up=Normalize(Cross(forward,right));
    if(Length(forward)<.001f||Length(right)<.001f||Length(up)<.001f)throw std::runtime_error("First-person weapon camera basis failed");
    Matrix4 cameraWorld=Matrix4::Identity();
    cameraWorld[0][0]=right.x;cameraWorld[1][0]=right.y;cameraWorld[2][0]=right.z;
    cameraWorld[0][1]=up.x;cameraWorld[1][1]=up.y;cameraWorld[2][1]=up.z;
    cameraWorld[0][2]=forward.x;cameraWorld[1][2]=forward.y;cameraWorld[2][2]=forward.z;
    cameraWorld[0][3]=camera_.position.x;cameraWorld[1][3]=camera_.position.y;cameraWorld[2][3]=camera_.position.z;
    // Viewmodel 使用獨立 camera-space 比例與方向，不依賴世界角色手部 socket。
    Matrix4 modelHandSocket{};if(!CharacterSocketTransform3D(activeModel.skeleton,activeRig,HumanoidJoint3D::RightHand,activePose,Matrix4::Identity(),modelHandSocket))throw std::runtime_error("Viewmodel semantic grip socket failed");
    const Matrix4 modelGripSocket=modelHandSocket*CreateTranslationMatrix({0,.105f,.02f});
    const Matrix4 desiredGrip=CreateTransformMatrix({.82f,-.07f,.90f},{-.12f,.08f,-.18f},{.43f,.43f,.43f});
    weaponWorld=cameraWorld*desiredGrip*InverseAffineViewmodelSocket(modelGripSocket);
  }
  std::array<SkinnedRenderablePart3D,2> playerParts{};
  std::size_t playerPartCount=0;
  if(rules_.Camera()!=CameraMode::FirstPerson)
    playerParts[playerPartCount++]={bodyMesh,playerWorld,Matrix4::Identity(),std::uint32_t(activeModel.bindMesh.indices.size()),bodySubmeshes,bodyMaterials,{1}};
  playerParts[playerPartCount++]={weaponMesh,weaponWorld,Matrix4::Identity(),std::uint32_t(rules_.Female()?femaleWeaponMesh_.indexCount:maleWeaponMesh_.indexCount),weaponSubmeshes,weaponMaterials,{2}};
  firstPersonBodyHidden_=firstPersonBodyHidden_&&(rules_.Camera()!=CameraMode::FirstPerson||playerPartCount==1);
  if(palette&&playerHistory_.Submit(draw,palette,std::span<const SkinnedRenderablePart3D>{playerParts.data(),playerPartCount},false,&skinnedGroupStats_)){
    ++weaponPbrDraws_;
    if(rules_.Camera()!=CameraMode::FirstPerson){if(rules_.Female())++femaleGpuDraws_;else ++maleGpuDraws_;}
  }
  Matrix4 socketWorld{},bodySocketWorld{};
  if(!CharacterSocketTransform3D(activeModel.skeleton,activeRig,HumanoidJoint3D::RightHand,activePose,weaponWorld,socketWorld)||
     (!firstPerson&&!CharacterSocketTransform3D(activeModel.skeleton,activeRig,HumanoidJoint3D::RightHand,activePose,playerWorld,bodySocketWorld)))
    throw std::runtime_error("Gameplay semantic weapon socket failed");
  const Vec3 handAnchor=TransformPoint(socketWorld,{});
  const Vec3 weaponTip=TransformPoint(socketWorld,{0,-.72f,0});
  const float thirdPersonAnchorError=firstPerson?0.f:Length(handAnchor-TransformPoint(bodySocketWorld,{}));
  weaponAttachmentPassed_=std::isfinite(handAnchor.x)&&std::isfinite(handAnchor.y)&&std::isfinite(handAnchor.z)&&thirdPersonAnchorError<=.001f;
  if(captureIndex_<Names.size()){
    captureBodyMaterialSlots_[captureIndex_]=rules_.Camera()==CameraMode::FirstPerson?0u:std::uint32_t(bodyMaterials.size());
    captureWeaponMaterialRanges_[captureIndex_]=std::uint32_t(weaponSubmeshes.size());
    captureWeaponAnchorErrors_[captureIndex_]=thirdPersonAnchorError;
    if(firstPerson){
      float minimumX=1280,minimumY=720,maximumX=0,maximumY=0,minimumDepth=1000;
      constexpr std::array<Vec3,8> envelope={Vec3{-.14f,-.15f,-.06f},Vec3{.14f,-.15f,-.06f},Vec3{-.14f,.68f,-.06f},Vec3{.14f,.68f,-.06f},Vec3{-.14f,-.15f,.06f},Vec3{.14f,-.15f,.06f},Vec3{-.14f,.68f,.06f},Vec3{.14f,.68f,.06f}};
      for(Vec3 local:envelope){const Vec3 world=TransformPoint(socketWorld,local);const auto projected=ProjectToViewport(world,camera_,{1280,720});minimumX=(std::min)(minimumX,projected.position.x);minimumY=(std::min)(minimumY,projected.position.y);maximumX=(std::max)(maximumX,projected.position.x);maximumY=(std::max)(maximumY,projected.position.y);minimumDepth=(std::min)(minimumDepth,TransformPoint(CreateViewMatrix(camera_),world).z);}
      const float width=(std::max)(0.f,maximumX-minimumX),height=(std::max)(0.f,maximumY-minimumY);
      captureWeaponScreenCoverage_[captureIndex_]=width*height/(1280.f*720.f);
      captureWeaponMinimumDepth_[captureIndex_]=minimumDepth;
      captureWeaponCenterExcluded_[captureIndex_]=!(minimumX<=640&&maximumX>=640&&minimumY<=360&&maximumY>=360);
      captureWeaponNearClipClear_[captureIndex_]=minimumDepth>camera_.projection.nearPlane+.015f;
      const bool topHud=minimumY<66&&maximumY>0, paletteHud=minimumX<372&&maximumX>0&&minimumY<720&&maximumY>470,rightHud=minimumX<1272&&maximumX>778&&minimumY<638&&maximumY>512;
      captureWeaponHudOverlap_[captureIndex_]=topHud||paletteHud||rightHud;
      const auto grip=ProjectToViewport(TransformPoint(socketWorld,{0,.105f,.02f}),camera_,{1280,720});
      const auto blade=ProjectToViewport(TransformPoint(socketWorld,{0,.65f,0}),camera_,{1280,720});captureWeaponGripX_[captureIndex_]=grip.position.x;captureWeaponGripY_[captureIndex_]=grip.position.y;captureWeaponBladeY_[captureIndex_]=blade.position.y;
      captureWeaponGripEdgeAnchored_[captureIndex_]=grip.position.x>=1040&&grip.position.x<=1278&&grip.position.y>=80&&grip.position.y<=510;
      captureWeaponBladeAboveGrip_[captureIndex_]=blade.position.y+80<grip.position.y;
    }
  }
  const auto hashPoint=[&](std::uint64_t& hash,Vec3 point){for(float value:{point.x,point.y,point.z}){const auto* bytes=reinterpret_cast<const unsigned char*>(&value);for(std::size_t index=0;index<sizeof(value);++index){hash^=bytes[index];hash*=1099511628211ull;}}};
  hashPoint(weaponTrajectoryHash_,weaponTip);
  if(combat_->CurrentAttack()!=Attack3D::None)hashPoint(attackTrajectoryHashes_[std::size_t(combat_->CurrentAttack())-1],weaponTip);
  if(combatSpawned_){
    const auto monsterPalette=AddSkinPaletteFromPose(draw,maleModel_,monsterPose_,skinning_);
    const Matrix4 monsterWorld=CreateTransformMatrix(combat_->Monster().position,{0,-.8f,0},{1.08f,1.08f,1.08f});
    const SkinnedRenderablePart3D monsterPart{maleMesh_.handle,monsterWorld,Matrix4::Identity(),std::uint32_t(maleModel_.bindMesh.indices.size()),maleSubmeshes_,maleMaterials_,{11}};
    monsterHistory_.Submit(draw,monsterPalette,std::span<const SkinnedRenderablePart3D>{&monsterPart,1},false,&skinnedGroupStats_);
  }
  draw.Quad({640,32},{1280,64},{.008f,.018f,.022f,.92f});
  draw.Text({18,14},"GAMEPLAY LAB  [1/2/3,V] CAMERA  [WASD/SHIFT/SPACE/C] MOVE  [LMB] COMBO  [TAB/B] PALETTE",1.25f,Cyan);
  draw.Quad({190,585},{360,230},{.008f,.018f,.022f,.9f});
  draw.Text({22,486},"OBJECT PALETTE",1.7f,Gold);
  draw.Text({22,520},"[4] PRIMITIVES  [5] PHYSICS  [6] PROPS",1.2f,Cyan);
  draw.Text({22,550},"[7] BUILDING  [8] CHARACTERS  [9] LIGHTS",1.2f,Cyan);
  draw.Text({22,582},std::format("SELECTED {}  GHOST VALID {} INVALID {}",unsigned(rules_.Category()),palette_->Stats().validPreviews,palette_->Stats().collisionRejected),1.15f,Gold);
  draw.Text({22,614},"DRAG/LMB PLACE  RMB CANCEL  R ROTATE",1.05f,Cyan);
  const auto stats=rules_.GetStats();
  draw.Text({22,638},std::format("ENT {}/{}  UNDO {} REDO {}",stats.entities,Rules3D::MaximumEntities,stats.undoDepth,stats.redoDepth),1.f,Gold);
  draw.Text({790,684},std::format("MONSTER HP {}  COMBO HITS {} KILLS {}",combat_->Monster().health,combat_->Stats().hits,combat_->Stats().kills),1.1f,combat_->Monster().alive?Cyan:Gold);
}
