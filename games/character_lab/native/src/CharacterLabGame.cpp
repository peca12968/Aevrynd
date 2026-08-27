#include "CharacterLabGame.h"
#include "CharacterLabRules.h"
#include <algorithm>
#include <cmath>
#include <format>
#include <iterator>

using namespace Aevrynd;
namespace {
constexpr Color Cyan{.12f,.92f,.82f,1},Gold{1.f,.72f,.2f,1},Ink{.72f,.86f,.88f,1},Coral{1.f,.35f,.25f,1};

void AppendSkinnedPart(SkinnedModel3D& model,const Mesh3D& source,Vec3 position,Vec3 scale,std::uint16_t joint,Color color){
    const Matrix4 transform=CreateTransformMatrix(position,{},scale),normalTransform=CreateNormalMatrix(transform);const std::uint32_t base=std::uint32_t(model.bindMesh.vertices.size());model.bindMesh.vertices.reserve(model.bindMesh.vertices.size()+source.vertices.size());model.influences.reserve(model.influences.size()+source.vertices.size());
    for(const Vertex3D& vertex:source.vertices){const Vec3 point=TransformPoint(transform,{vertex.x,vertex.y,vertex.z});const Vec4 packed=UnpackNormalSnorm8(vertex.normal);const Vec3 normal=TransformNormal(normalTransform,{packed.x,packed.y,packed.z});model.bindMesh.vertices.push_back({point.x,point.y,point.z,PackNormalSnorm8(normal,packed.w),PackColorRgba8(color),vertex.u,vertex.v});SkinInfluences3D influences{};influences.joints[0]=joint;influences.weights[0]=65535;model.influences.push_back(influences);}
    model.bindMesh.indices.reserve(model.bindMesh.indices.size()+source.indices.size());for(std::uint32_t index:source.indices)model.bindMesh.indices.push_back(base+index);
}

SkinnedModel3D CreateCharacterModel(){
    SkinnedModel3D model;model.skeleton.joints={
        {-1,{{0,1.f,0},{},{1,1,1}}},
        {0,{{0,.55f,0},{},{1,1,1}}},
        {1,{{0,.65f,0},{},{1,1,1}}},
        {1,{{-.55f,.25f,0},{},{1,1,1}}},
        {1,{{.55f,.25f,0},{},{1,1,1}}},
        {0,{{-.22f,-.1f,0},{},{1,1,1}}},
        {0,{{.22f,-.1f,0},{},{1,1,1}}}};
    const std::array<Vec3,7> bindPositions={Vec3{0,1,0},Vec3{0,1.55f,0},Vec3{0,2.2f,0},Vec3{-.55f,1.8f,0},Vec3{.55f,1.8f,0},Vec3{-.22f,.9f,0},Vec3{.22f,.9f,0}};model.inverseBindMatrices.reserve(bindPositions.size());for(Vec3 position:bindPositions)model.inverseBindMatrices.push_back(CreateTranslationMatrix(position*-1.f));
    const Mesh3D sphere=Mesh3D::CreateUnitSphere(18,12);AppendSkinnedPart(model,sphere,{0,1.02f,0},{.72f,.58f,.52f},0,{.09f,.28f,.36f,1});AppendSkinnedPart(model,sphere,{0,1.58f,0},{.9f,1.02f,.58f},1,{.08f,.55f,.72f,1});AppendSkinnedPart(model,sphere,{0,2.22f,0},{.56f,.62f,.55f},2,{.72f,.56f,.42f,1});
    AppendSkinnedPart(model,sphere,{-.55f,1.4f,0},{.3f,.88f,.32f},3,{.08f,.46f,.62f,1});AppendSkinnedPart(model,sphere,{-.55f,.94f,0},{.27f,.28f,.3f},3,{.72f,.56f,.42f,1});AppendSkinnedPart(model,sphere,{.55f,1.4f,0},{.3f,.88f,.32f},4,{.08f,.46f,.62f,1});AppendSkinnedPart(model,sphere,{.55f,.94f,0},{.27f,.28f,.3f},4,{.72f,.56f,.42f,1});
    AppendSkinnedPart(model,sphere,{-.22f,.45f,0},{.38f,.92f,.42f},5,{.055f,.16f,.22f,1});AppendSkinnedPart(model,sphere,{-.22f,.02f,.13f},{.42f,.2f,.72f},5,{.035f,.08f,.11f,1});AppendSkinnedPart(model,sphere,{.22f,.45f,0},{.38f,.92f,.42f},6,{.055f,.16f,.22f,1});AppendSkinnedPart(model,sphere,{.22f,.02f,.13f},{.42f,.2f,.72f},6,{.035f,.08f,.11f,1});return model;
}

AnimationClip3D CreateWalkClip(){
    constexpr float swing=.52f;AnimationClip3D clip;clip.duration=1.f;clip.looping=true;
    clip.tracks.push_back({0,{{0,{{0,1.f,0},{},{1,1,1}}},{.5f,{{0,1.05f,0},{},{1,1,1}}},{1.f,{{0,1.f,0},{},{1,1,1}}}}});
    clip.tracks.push_back({3,{{0,{{-.55f,.25f,0},{swing,0,0},{1,1,1}}},{.5f,{{-.55f,.25f,0},{-swing,0,0},{1,1,1}}},{1.f,{{-.55f,.25f,0},{swing,0,0},{1,1,1}}}}});
    clip.tracks.push_back({4,{{0,{{.55f,.25f,0},{-swing,0,0},{1,1,1}}},{.5f,{{.55f,.25f,0},{swing,0,0},{1,1,1}}},{1.f,{{.55f,.25f,0},{-swing,0,0},{1,1,1}}}}});
    clip.tracks.push_back({5,{{0,{{-.22f,-.1f,0},{-swing,0,0},{1,1,1}}},{.5f,{{-.22f,-.1f,0},{swing,0,0},{1,1,1}}},{1.f,{{-.22f,-.1f,0},{-swing,0,0},{1,1,1}}}}});
    clip.tracks.push_back({6,{{0,{{.22f,-.1f,0},{swing,0,0},{1,1,1}}},{.5f,{{.22f,-.1f,0},{-swing,0,0},{1,1,1}}},{1.f,{{.22f,-.1f,0},{swing,0,0},{1,1,1}}}}});return clip;
}
}

Aevrynd::ReusableMemoryStats CharacterLabGame::GetReusableMemoryStats()const{const ReusableMemoryStats collision=collisionWorld_.MemoryStats(),scene=scene_.MemoryStats(),skinning=skinningWorkspace_.MemoryStats();return{collision.retainedCapacityBytes+scene.retainedCapacityBytes+skinning.retainedCapacityBytes,collision.reclaimableCapacityBytes+scene.reclaimableCapacityBytes+skinning.reclaimableCapacityBytes,lastReusableMemoryReleasedBytes_,reusableMemoryTrimCount_};}
std::size_t CharacterLabGame::TrimReusableMemory(){const std::size_t before=GetReusableMemoryStats().retainedCapacityBytes;collisionWorld_.TrimUnusedMemory();scene_.TrimUnusedMemory();skinningWorkspace_.TrimUnusedMemory();const std::size_t after=GetReusableMemoryStats().retainedCapacityBytes;lastReusableMemoryReleasedBytes_=before>after?before-after:0;++reusableMemoryTrimCount_;return lastReusableMemoryReleasedBytes_;}

void CharacterLabGame::OnStart(Engine& engine){
    const BoxCollider3D boxes[]={{{0,-.5f,0},{14,.5f,14}},{{-13.5f,2,0},{.5f,2,14}},{{13.5f,2,0},{.5f,2,14}},{{0,2,-13.5f},{14,2,.5f}},{{0,2,13.5f},{14,2,.5f}},{{0,1,2},{2,1,.5f}},{{-5,1,-2},{1,1,2}},{{5,.75f,4},{1.5f,.75f,1.5f}}};for(std::size_t index=0;index<std::size(boxes);++index)collisionWorld_.CreateBox({boxes[index],1,CollisionLayerAll3D,index,false});controller_.Reset({0,0,-8});boxMesh_=engine.CreateMeshResource3D(Mesh3D::CreateUnitBox());scene_.SetBoxMesh(boxMesh_);scene_.Reserve({0,32,8,4,0});characterModel_=CreateCharacterModel();walkClip_=CreateWalkClip();animationPlayer_.Play(walkClip_,0,true);jumpSound_=engine.Audio().CreateSound(GenerateTone(520,.08f,.14f));if(!captureDirectory_.empty()){std::filesystem::create_directories(captureDirectory_);std::error_code error;std::filesystem::remove(captureDirectory_/L"third-person-skinned.png",error);automatic_=false;firstPerson_=false;}if(engine.DLSSOn())engine.ToggleDLSS();
}

void CharacterLabGame::OnStop(Engine& engine){scene_.SetBoxMesh({});engine.DestroyMesh3D(boxMesh_.handle);engine.Audio().DestroySound(jumpSound_);boxMesh_={};jumpSound_={};}

void CharacterLabGame::OnUpdate(Engine& engine,float deltaTime){
    Input& input=engine.GetInput();if(input.Pressed(VK_ESCAPE))engine.Quit();if(input.Pressed(VK_SPACE))automatic_=!automatic_;if(input.Pressed(VK_F1)){firstPerson_=true;automatic_=false;++modeSwitches_;}if(input.Pressed(VK_F2)){firstPerson_=false;automatic_=false;++modeSwitches_;}
    CharacterControl3D control{};bool requestedJump=false;if(automatic_){const int phase=CharacterLabRules::AutomaticPhase(engine.Time());if(phase!=automaticPhase_){automaticPhase_=phase;firstPerson_=CharacterLabRules::FirstPerson(phase);++modeSwitches_;}control.movement=CharacterLabRules::Movement(phase);yaw_=std::atan2(control.movement.x,control.movement.z);const int jumpCycle=CharacterLabRules::JumpCycle(engine.Time());requestedJump=jumpCycle!=lastJumpCycle_&&controller_.Grounded();if(requestedJump)lastJumpCycle_=jumpCycle;}else{yaw_+=(float(input.Down(VK_RIGHT))-float(input.Down(VK_LEFT))-input.MouseDelta().x*.025f)*deltaTime*2.2f;pitch_=Clamp(pitch_+(float(input.Down(VK_UP))-float(input.Down(VK_DOWN))+input.MouseDelta().y*.02f)*deltaTime*1.8f,-.45f,.8f);const Vec3 forward{std::sin(yaw_),0,std::cos(yaw_)},right{std::cos(yaw_),0,-std::sin(yaw_)};control.movement=forward*(float(input.Down('W'))-float(input.Down('S'))+input.GamepadLeftY())+right*(float(input.Down('D'))-float(input.Down('A'))+input.GamepadLeftX());requestedJump=input.Pressed(VK_RETURN)||input.GamepadPressed(XINPUT_GAMEPAD_A);}control.jump=requestedJump;const uint64_t previousJumps=controller_.Stats().jumps;controller_.Step(control,collisionWorld_,deltaTime);if(controller_.Stats().jumps>previousJumps)engine.Audio().Play(jumpSound_);
    animationPlayer_.Update(deltaTime);if(!captureDirectory_.empty()){const RendererStats renderer=engine.GetRendererStats();if(capturePending_&&(renderer.successfulFrameCaptures>captureSuccessfulBaseline_||renderer.rejectedFrameCaptures>captureRejectedBaseline_)){if(renderer.successfulFrameCaptures>captureSuccessfulBaseline_)engine.Quit();capturePending_=false;}if(!capturePending_&&engine.Time()>=nextCaptureTime_){captureSuccessfulBaseline_=renderer.successfulFrameCaptures;captureRejectedBaseline_=renderer.rejectedFrameCaptures;capturePending_=engine.CaptureNextFrame3D(captureDirectory_/L"third-person-skinned.png");nextCaptureTime_=engine.Time()+.4;}}const auto stats=controller_.Stats();engine.SetTitle(std::format(L"Aevrynd Character Lab | {} | POS {:.1f} {:.1f} {:.1f} | GROUND {} | STEP {} COLL {} JUMP {} LAND {} | SKIN {} | SHADOW {} | CAM BLOCK {}",firstPerson_?L"FIRST PERSON":L"THIRD PERSON",controller_.Position().x,controller_.Position().y,controller_.Position().z,controller_.Grounded()?L"YES":L"NO",stats.steps,stats.collisions,stats.jumps,stats.landings,skinningStats_.vertices,sceneStats_.shadowInstances,cameraObstructions_));
}

void CharacterLabGame::BuildEnvironment(){
    scene_.Clear();scene_.MaterialBox({0,-.5f,0},{28,1,28},{{.035f,.11f,.12f,1},{.94f,0,0,.9f},false});scene_.Box({-13.5f,2,0},{1,4,28},{.08f,.24f,.28f,1});scene_.Box({13.5f,2,0},{1,4,28},{.08f,.24f,.28f,1});scene_.Box({0,2,-13.5f},{28,4,1},{.08f,.24f,.28f,1});scene_.Box({0,2,13.5f},{28,4,1},{.08f,.24f,.28f,1});scene_.Box({0,1,2},{4,2,1},Coral);scene_.Box({-5,1,-2},{2,2,4},{.25f,.42f,.75f,1});scene_.Box({5,.75f,4},{3,1.5f,3},Gold);for(int index=-12;index<=12;index+=2){scene_.Line({float(index),.01f,-12},{float(index),.01f,12},1,{.08f,.32f,.32f,1});scene_.Line({-12,.01f,float(index)},{12,.01f,float(index)},1,{.08f,.32f,.32f,1});}}

void CharacterLabGame::OnRender(Engine& engine,float){
    BuildEnvironment();const Vec3 feet=controller_.Position(),focus=feet+Vec3{0,1.25f,0};if(firstPerson_)camera_=MakeFirstPersonCamera(feet+Vec3{0,1.65f,0},yaw_,pitch_);else{Camera3D desired=MakeThirdPersonCamera(focus,yaw_,.28f,7.5f);camera_=ConstrainThirdPersonCamera(desired,focus,collisionWorld_,.25f);if(Length(camera_.position-focus)+.01f<Length(desired.position-focus))++cameraObstructions_;}
    aimHit_=scene_.Raycast(ScreenPointToRay(camera_,{1280,720},{640,360}),40,&queryStats_);if(aimHit_){scene_.Line(aimHit_.position-Vec3{.25f,0,0},aimHit_.position+Vec3{.25f,0,0},5,Gold);scene_.Line(aimHit_.position-Vec3{0,.25f,0},aimHit_.position+Vec3{0,.25f,0},5,Gold);scene_.Line(aimHit_.position,aimHit_.position+aimHit_.normal,3,Cyan);}
    DrawList& draw=engine.Draw();engine.SetClearColor({.04f,.13f,.16f,1});draw.SetDirectionalLight({{.45f,-1,.25f},.25f,{1,.94f,.82f,1},.9f});draw.SetAtmosphere({{.04f,.13f,.16f,.85f},camera_.position,22,80,true});draw.AddPointLight({feet+Vec3{0,2.2f,0},8,{.15f,.72f,1,1},1.4f});draw.AddPointLight({{-5,3,-2},11,{.32f,.48f,1,1},1.7f});draw.AddPointLight({{5,3,4},11,{1,.48f,.18f,1},1.7f});sceneStats_=scene_.Submit(draw,camera_,{1280,720},{true,true,{true,.025f,.24f,45.f,128},{80.f,.25f}});scene_.Clear();if(!firstPerson_){animationPlayer_.EvaluateInto(characterModel_.skeleton,characterPose_);if(SkinModelInto(characterModel_,characterPose_,skinningWorkspace_,skinnedCharacter_,&skinningStats_))SubmitMesh(draw,skinnedCharacter_,{feet,{0,yaw_,0},{1,1,1}});}draw.Quad({640,32},{1280,64},{.01f,.025f,.035f,.92f});draw.Text({28,16},firstPerson_?"CHARACTER LAB  FIRST PERSON":"CHARACTER LAB  THIRD PERSON",2.8f,firstPerson_?Gold:Cyan);draw.Text({840,17},automatic_?"AUTO VALIDATION":"MANUAL CONTROL",2.2f,Ink);draw.Line({628,360},{652,360},2,Cyan);draw.Line({640,348},{640,372},2,Cyan);const auto stats=controller_.Stats();draw.Quad({640,688},{1280,64},{.01f,.025f,.035f,.92f});draw.Text({24,672},std::format("GROUND {} COLL {} JUMP {} LAND {} MODE {} SKIN {} SHADOW {}",controller_.Grounded()?"YES":"NO",stats.collisions,stats.jumps,stats.landings,modeSwitches_,skinningStats_.vertices,sceneStats_.shadowInstances),1.75f,Cyan);draw.Text({1000,672},automatic_?"SPACE MANUAL":"F1 F2 WASD",1.6f,Gold);
}
