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
namespace {
std::size_t SemanticJoint(std::span<const std::string> names,std::initializer_list<std::string_view> candidates,std::size_t fallback){const auto lower=[](std::string_view value){std::string result(value);std::ranges::transform(result,result.begin(),[](unsigned char c){return char(std::tolower(c));});return result;};for(const auto candidate:candidates){const auto wanted=lower(candidate);for(std::size_t i=0;i<names.size();++i){const auto name=lower(names[i]);if(name==wanted||name.ends_with(wanted))return i;}}return names.empty()?0:(std::min)(fallback,names.size()-1);}
void RotateJoint(JointTransform3D& joint,Vec3 delta){if(joint.usesQuaternion){const Quaternion3D a=joint.quaternion,b=QuaternionFromEuler3D(delta);joint.quaternion=NormalizeQuaternion3D({a.w*b.x+a.x*b.w+a.y*b.z-a.z*b.y,a.w*b.y-a.x*b.z+a.y*b.w+a.z*b.x,a.w*b.z+a.x*b.y-a.y*b.x+a.z*b.w,a.w*b.w-a.x*b.x-a.y*b.y-a.z*b.z});}else joint.rotation=joint.rotation+delta;}
void ApplyGameplayPose(const SkinnedModel3D& model,std::span<const std::string> names,AnimationPose3D& destination,const GameplayAnimation3D& source){destination.localTransforms.resize(model.skeleton.joints.size());for(std::size_t i=0;i<model.skeleton.joints.size();++i)destination.localTransforms[i]=model.skeleton.joints[i].bindTransform;const auto root=SemanticJoint(names,{"Root","Hips"},0),spine=SemanticJoint(names,{"Spine","Spine1"},1),head=SemanticJoint(names,{"Head"},2),leftArm=SemanticJoint(names,{"LeftUpperArm","LeftShoulder"},3),rightArm=SemanticJoint(names,{"RightUpperArm","RightShoulder"},4),leftLeg=SemanticJoint(names,{"LeftUpperLeg","LeftLeg"},5),rightLeg=SemanticJoint(names,{"RightUpperLeg","RightLeg"},6);const float wave=source.Pose().localTransforms.size()>2?source.Pose().localTransforms[2].rotation.x:0;switch(source.State()){case AnimationState3D::Walk:case AnimationState3D::Run:RotateJoint(destination.localTransforms[leftArm],{wave,0,0});RotateJoint(destination.localTransforms[rightArm],{-wave,0,0});RotateJoint(destination.localTransforms[leftLeg],{-wave,0,0});RotateJoint(destination.localTransforms[rightLeg],{wave,0,0});break;case AnimationState3D::Crouch:destination.localTransforms[root].translation.y-=.35f;RotateJoint(destination.localTransforms[spine],{.28f,0,0});break;case AnimationState3D::Jump:destination.localTransforms[root].translation.y+=.22f;RotateJoint(destination.localTransforms[leftLeg],{-.55f,0,0});RotateJoint(destination.localTransforms[rightLeg],{-.55f,0,0});break;case AnimationState3D::Fall:RotateJoint(destination.localTransforms[leftArm],{0,0,-.8f});RotateJoint(destination.localTransforms[rightArm],{0,0,.8f});break;case AnimationState3D::Attack1:RotateJoint(destination.localTransforms[rightArm],{-1.1f,0,-.25f});break;case AnimationState3D::Attack2:RotateJoint(destination.localTransforms[leftArm],{-1.1f,0,.25f});break;case AnimationState3D::Attack3:RotateJoint(destination.localTransforms[spine],{0,1.05f,0});RotateJoint(destination.localTransforms[rightArm],{-1.65f,.35f,-.55f});break;case AnimationState3D::Hit:RotateJoint(destination.localTransforms[spine],{-.5f,0,0});break;case AnimationState3D::Death:RotateJoint(destination.localTransforms[root],{0,0,1.45f});break;default:RotateJoint(destination.localTransforms[head],{0,.04f,0});break;}BuildAnimationPoseGlobalMatrices3D(model.skeleton,destination);}
}
namespace {Vec3 SkinSocketWorld(const SkinnedModel3D& model,const AnimationPose3D& pose,std::size_t joint,const Matrix4& world){Vec3 total{};float count=0;const auto size=(std::min)(model.bindMesh.vertices.size(),model.influences.size());for(std::size_t i=0;i<size;++i){const auto& influence=model.influences[i];bool attached=false;for(std::size_t k=0;k<4;++k)attached=attached||(influence.joints[k]==joint&&influence.weights[k]>=16384);if(!attached)continue;const auto& vertex=model.bindMesh.vertices[i];const Vec3 bind{vertex.x,vertex.y,vertex.z};Vec3 skinned{};float weightSum=0;for(std::size_t k=0;k<4;++k){const auto bone=influence.joints[k];const float weight=float(influence.weights[k])/65535.f;if(!weight||bone>=pose.globalMatrices.size()||bone>=model.inverseBindMatrices.size())continue;skinned=skinned+TransformPoint(pose.globalMatrices[bone]*model.inverseBindMatrices[bone],bind)*weight;weightSum+=weight;}if(weightSum>.0001f){total=total+TransformPoint(world,skinned*(1.f/weightSum));count+=1;}}return count>0?total*(1.f/count):TransformPoint(world*pose.globalMatrices[joint],{});}}
namespace {std::uint64_t HashPose(const AnimationPose3D& pose){std::uint64_t hash=14695981039346656037ull;for(const auto& matrix:pose.globalMatrices)for(const auto& row:matrix.values)for(float value:row){const auto* bytes=reinterpret_cast<const unsigned char*>(&value);for(std::size_t i=0;i<sizeof(value);++i){hash^=bytes[i];hash*=1099511628211ull;}}return hash;}}
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
ReusableMemoryStats GameplayLabGame::GetReusableMemoryStats()const{const auto c=collisions_.MemoryStats(),p=physics_.MemoryStats(),s=scene_.MemoryStats();return{c.retainedCapacityBytes+p.retainedCapacityBytes+s.retainedCapacityBytes,c.reclaimableCapacityBytes+p.reclaimableCapacityBytes+s.reclaimableCapacityBytes,released_,trims_};}
std::size_t GameplayLabGame::TrimReusableMemory(){const auto before=GetReusableMemoryStats().retainedCapacityBytes;collisions_.TrimUnusedMemory();physics_.TrimUnusedMemory();scene_.TrimUnusedMemory();const auto after=GetReusableMemoryStats().retainedCapacityBytes;released_=before>after?before-after:0;++trims_;return released_;}
void GameplayLabGame::OnStart(Engine& engine){rules_.Reset();controller_.Reset({0,0,5});boxMesh_=engine.CreateMeshResource3D(Mesh3D::CreateUnitBox());sphereMesh_=engine.CreateMeshResource3D(Mesh3D::CreateUnitSphere(18,12));const auto root=engine.ExecutableDirectory()/"assets"/"model-packs";auto female=LoadSkinnedModelAssetFileDetailed(root/"realistic-female"/"realistic-female.aeskin"),male=LoadSkinnedModelAssetFileDetailed(root/"realistic-male"/"realistic-male.aeskin");if(!female||!male)throw std::runtime_error("Gameplay Lab requires the existing F2/M2 character packs");femaleJointNames_=female.asset.jointNames;maleJointNames_=male.asset.jointNames;femaleModel_=std::move(female.asset.model);maleModel_=std::move(male.asset.model);femaleMesh_=engine.CreateSkinnedMeshResource3D(femaleModel_);maleMesh_=engine.CreateSkinnedMeshResource3D(maleModel_);AnimationClip3D bind{};bind.duration=1;SampleAnimationClipInto(femaleModel_.skeleton,bind,0,femalePose_);SampleAnimationClipInto(maleModel_.skeleton,bind,0,malePose_);SampleAnimationClipInto(maleModel_.skeleton,bind,0,monsterPose_);for(unsigned state=0;state<11;++state){gameplayAnimation_.SetState(AnimationState3D(state),true);gameplayAnimation_.Tick(.173f);ApplyGameplayPose(femaleModel_,femaleJointNames_,femalePose_,gameplayAnimation_);ApplyGameplayPose(maleModel_,maleJointNames_,malePose_,gameplayAnimation_);femalePoseHashes_[state]=HashPose(femalePose_);malePoseHashes_[state]=HashPose(malePose_);}gameplayAnimation_.SetState(AnimationState3D::Idle,true);scene_.SetBoxMesh(boxMesh_);collisions_.CreateBox({{{0,-.5f,0},{30,.5f,30}},1,0xffffffffu});collisions_.CreateBox({{{0,1.5f,-12},{4,1.5f,.6f}},1,0xffffffffu});physics_.CreateBody({RigidBodyType3D::Static,{{0,-.5f,0},{},{30,1,30}},{30,.5f,30}});for(int i=0;i<18;++i){const Vec3 p{-8.f+float(i%6)*1.6f,.65f+float(i/6)*1.25f,-3.f};if(!palette_->PlaceAt(p,PaletteCategory::Physics))throw std::runtime_error("Initial gameplay palette transaction failed");}if(!captureDirectory_.empty()){std::filesystem::create_directories(captureDirectory_);{std::unordered_set<std::uint64_t> femaleUnique(femalePoseHashes_.begin(),femalePoseHashes_.end()),maleUnique(malePoseHashes_.begin(),malePoseHashes_.end());if(femaleUnique.size()!=11||maleUnique.size()!=11||std::ranges::any_of(femalePoseHashes_,[](auto value){return value==0;})||std::ranges::any_of(malePoseHashes_,[](auto value){return value==0;}))throw std::runtime_error("Semantic gameplay pose hashes are not unique");std::ofstream poseOut(captureDirectory_/"gameplay-pose-receipt.json",std::ios::trunc);poseOut<<"{\"runId\":\""<<runId_<<"\",\"source\":\"GameplayLabSemanticSkinning\",\"female\":[";for(std::size_t i=0;i<11;++i){if(i)poseOut<<",";poseOut<<femalePoseHashes_[i];}poseOut<<"],\"male\":[";for(std::size_t i=0;i<11;++i){if(i)poseOut<<",";poseOut<<malePoseHashes_[i];}poseOut<<"]}";}const auto weaponJoint=SemanticJoint(maleJointNames_,{"RightHand","RightWrist","RightUpperArm","RightShoulder"},4);std::ofstream jointOut(captureDirectory_/"weapon-joint.txt",std::ios::trunc);jointOut<<weaponJoint<<" "<<(weaponJoint<maleJointNames_.size()?maleJointNames_[weaponJoint]:"missing");for(auto name:Names){std::error_code ec;std::filesystem::remove(captureDirectory_/name,ec);}}}
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
  engine.DestroySkinnedMesh3D(femaleMesh_.handle);
  engine.DestroySkinnedMesh3D(maleMesh_.handle);
  engine.DestroyMesh3D(boxMesh_.handle);
  engine.DestroyMesh3D(sphereMesh_.handle);
  femaleMesh_ = {};
  maleMesh_ = {};
  femaleModel_ = {};
  maleModel_ = {};
  boxMesh_ = {};
  sphereMesh_ = {};
  skinning_.TrimUnusedMemory();
  TrimReusableMemory();
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
        MakeThirdPersonCamera(focus, yaw_, .28f, 6.5f), focus, collisions_,
        .25f);
    break;
  default:
    camera_ = MakeOverheadCamera(focus, 22, true);
    break;
  }
}
void GameplayLabGame::OnUpdate(Engine &engine, float dt) {
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
  if (!combat_->Monster().alive)
    animation = AnimationState3D::Attack3;
  gameplayAnimation_.SetState(animation);
  gameplayAnimation_.Tick(dt);
  monsterAnimation_.SetState(combat_->Monster().alive
                                 ? AnimationState3D::Walk
                                 : AnimationState3D::Death);
  monsterAnimation_.Tick(dt);
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
    if (renderer.successfulFrameCaptures > captureSuccess_)
      ++captureIndex_;
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
    if (captureAuthoritativePhysicsSteps_ != 240)
      throw std::runtime_error(
          "Gameplay capture missed its authoritative physics tick");
    out << std::format(
        "{{\"runId\":\"{}\",\"source\":\"GameplayLabFramebuffer\",\"entry\":\"-"
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
        "\"weaponHitWindowEvidence\":{},\"gpuValidationRequested\":{},"
        "\"gpuValidationAvailable\":{},\"gpuValidationExecuted\":{},"
        "\"debugLayerEnabled\":{},\"gpuValidationPassed\":{},\"gpuErrors\":{},"
        "\"gpuWarnings\":{},\"lighting\":[{{\"phase\":\"day\",\"sun\":[0.38,-1."
        "0,0.24],\"intensity\":1.8,\"exposure\":1.0,\"fogDensity\":0.006,"
        "\"shadow\":1}},{{\"phase\":\"dusk\",\"sun\":[-0.65,-0.34,0.28],"
        "\"intensity\":1.2,\"exposure\":0.9,\"fogDensity\":0.014,\"shadow\":1}}"
        ",{{\"phase\":\"night\",\"sun\":[0.25,-0.72,-0.34],\"intensity\":0.48,"
        "\"exposure\":0.72,\"fogDensity\":0.022,\"shadow\":1}}]}}",
        runId_, s.ticks, captureAuthoritativePhysicsSteps_, s.entities,
        p.bodies, p.steps, s.cameraSwitches,
        controller_.Stats().jumps, s.crouches, palette.previews,
        palette.validPreviews, palette.collisionRejected,
        unsigned(rules_.Category()), palette.commits, c.sweeps, c.hits, c.kills,
        combat_->Monster().health, unsigned(gameplayAnimation_.State()),
        unsigned(monsterAnimation_.State()),
        combatSpawned_ && !combat_->Monster().alive ? 1 : 0, femaleGpuDraws_,
        maleGpuDraws_, femaleMesh_.handle.resourceIdentity,
        maleMesh_.handle.resourceIdentity, HashPose(femalePose_),
        HashPose(malePose_), minimumControllerHeight_, s.headroomRejected,
        crouchFeetInvariant_ ? 1 : 0, weaponAttachmentPassed_ ? 1 : 0,
        weaponHitWindowEvidence_ ? 1 : 0, r.gpuValidationRequested ? 1 : 0,
        r.gpuValidationAvailable ? 1 : 0, r.gpuValidationExecuted ? 1 : 0,
        r.debugLayerEnabled ? 1 : 0, r.gpuValidationPassed ? 1 : 0,
        r.debugErrorCount, r.debugWarningCount);
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
void GameplayLabGame::OnRender(Engine& engine,float){BuildWorld(engine);ApplyGameplayPose(femaleModel_,femaleJointNames_,femalePose_,gameplayAnimation_);ApplyGameplayPose(maleModel_,maleJointNames_,malePose_,gameplayAnimation_);ApplyGameplayPose(maleModel_,maleJointNames_,monsterPose_,monsterAnimation_);DrawList& draw=engine.Draw();if(rules_.Camera()!=CameraMode::FirstPerson){const auto& playerModel=rules_.Female()?femaleModel_:maleModel_;const auto& playerPose=rules_.Female()?femalePose_:malePose_;const auto playerMesh=rules_.Female()?femaleMesh_.handle:maleMesh_.handle;const auto palette=AddSkinPaletteFromPose(draw,playerModel,playerPose,skinning_);if(draw.SkinnedMeshInstance(playerMesh,palette,CreateTransformMatrix(controller_.Position(),{0,yaw_,0},{1,1,1}),rules_.Female()?Color{1,.82f,.74f,1}:Color{.55f,.72f,1,1},{.42f,0,0,1})){if(rules_.Female())++femaleGpuDraws_;else ++maleGpuDraws_;}}if(combatSpawned_){const auto palette=AddSkinPaletteFromPose(draw,maleModel_,monsterPose_,skinning_);draw.SkinnedMeshInstance(maleMesh_.handle,palette,CreateTransformMatrix(combat_->Monster().position,{0,-.8f,0},{1.08f,1.08f,1.08f}),{.38f,.62f,.28f,1},{.58f,.04f,0,1});}const auto& weaponPose=rules_.Female()?femalePose_:malePose_;const auto& weaponNames=rules_.Female()?femaleJointNames_:maleJointNames_;const auto hand=SemanticJoint(weaponNames,{"RightHand","RightWrist","RightUpperArm","RightShoulder"},4);const Matrix4 playerWorld=CreateTransformMatrix(controller_.Position(),{0,yaw_,0},{1,1,1});const Matrix4 jointWorld=hand<weaponPose.globalMatrices.size()?playerWorld*weaponPose.globalMatrices[hand]:playerWorld;const Vec3 handAnchor=hand<weaponPose.globalMatrices.size()?SkinSocketWorld(rules_.Female()?femaleModel_:maleModel_,weaponPose,hand,playerWorld):TransformPoint(playerWorld,{});const Matrix4 handWorld=CreateTranslationMatrix(handAnchor-TransformPoint(jointWorld,{}))*jointWorld;const Vec3 basisX=TransformPoint(handWorld,{1,0,0})-handAnchor,basisY=TransformPoint(handWorld,{0,1,0})-handAnchor,basisZ=TransformPoint(handWorld,{0,0,1})-handAnchor;const auto finite=[](Vec3 value){return std::isfinite(value.x)&&std::isfinite(value.y)&&std::isfinite(value.z);};const bool finiteBasis=finite(handAnchor)&&finite(basisX)&&finite(basisY)&&finite(basisZ);const auto unit=[](Vec3 value){const float length=Length(value);return std::isfinite(length)&&length>.0001f;};const Matrix4 gripWorld=handWorld;const Vec3 gripAnchor=TransformPoint(gripWorld,{});weaponAttachmentPassed_=finiteBasis&&unit(basisX)&&unit(basisY)&&unit(basisZ)&&Length(gripAnchor-handAnchor)<.001f;draw.MeshInstance(sphereMesh_.handle,gripWorld*CreateScaleMatrix({.12f,.12f,.12f}),Gold,{.3f,.8f,.15f,1.f});const Matrix4 blade=handWorld*CreateRotationMatrix({1.5707963f,0,0})*CreateTranslationMatrix({0,0,.58f})*CreateScaleMatrix({.08f,.08f,1.15f});draw.MeshInstance(boxMesh_.handle,blade,Gold,{.25f,.85f,.1f,1.f});draw.Quad({640,32},{1280,64},{.008f,.018f,.022f,.92f});draw.Text({18,14},"GAMEPLAY LAB  [1/2/3,V] CAMERA  [WASD/SHIFT/SPACE/C] MOVE  [LMB] COMBO  [TAB/B] PALETTE",1.25f,Cyan);draw.Quad({190,585},{360,230},{.008f,.018f,.022f,.9f});draw.Text({22,486},"OBJECT PALETTE",1.7f,Gold);draw.Text({22,520},"[4] PRIMITIVES  [5] PHYSICS  [6] PROPS",1.2f,Cyan);draw.Text({22,550},"[7] BUILDING  [8] CHARACTERS  [9] LIGHTS",1.2f,Cyan);draw.Text({22,582},std::format("SELECTED {}  GHOST VALID {} INVALID {}",unsigned(rules_.Category()),palette_->Stats().validPreviews,palette_->Stats().collisionRejected),1.15f,Gold);draw.Text({22,614},"DRAG/LMB PLACE  RMB CANCEL  R ROTATE",1.05f,Cyan);const auto s=rules_.GetStats();draw.Text({22,638},std::format("ENT {}/{}  UNDO {} REDO {}",s.entities,Rules3D::MaximumEntities,s.undoDepth,s.redoDepth),1.f,Gold);draw.Text({790,684},std::format("MONSTER HP {}  COMBO HITS {} KILLS {}",combat_->Monster().health,combat_->Stats().hits,combat_->Stats().kills),1.1f,combat_->Monster().alive?Cyan:Gold);}
