#include "CharacterLabGame.h"
#include "CharacterLabRules.h"
#include <algorithm>
#include <cmath>
#include <format>
#include <iterator>

using namespace Aevrynd;
namespace {
constexpr Color Cyan{.12f,.92f,.82f,1},Gold{1.f,.72f,.2f,1},Ink{.72f,.86f,.88f,1},Coral{1.f,.35f,.25f,1};
}

Aevrynd::ReusableMemoryStats CharacterLabGame::GetReusableMemoryStats()const{const ReusableMemoryStats collision=collisionWorld_.MemoryStats();return{collision.retainedCapacityBytes,collision.reclaimableCapacityBytes,lastReusableMemoryReleasedBytes_,reusableMemoryTrimCount_};}
std::size_t CharacterLabGame::TrimReusableMemory(){lastReusableMemoryReleasedBytes_=collisionWorld_.TrimUnusedMemory();++reusableMemoryTrimCount_;return lastReusableMemoryReleasedBytes_;}

void CharacterLabGame::OnStart(Engine& engine){
    const BoxCollider3D boxes[]={{{0,-.5f,0},{14,.5f,14}},{{-13.5f,2,0},{.5f,2,14}},{{13.5f,2,0},{.5f,2,14}},{{0,2,-13.5f},{14,2,.5f}},{{0,2,13.5f},{14,2,.5f}},{{0,1,2},{2,1,.5f}},{{-5,1,-2},{1,1,2}},{{5,.75f,4},{1.5f,.75f,1.5f}}};for(std::size_t index=0;index<std::size(boxes);++index)collisionWorld_.CreateBox({boxes[index],1,CollisionLayerAll3D,index,false});controller_.Reset({0,0,-8});boxMesh_=engine.CreateMeshResource3D(Mesh3D::CreateUnitBox());scene_.SetBoxMesh(boxMesh_);jumpSound_=engine.Audio().CreateSound(GenerateTone(520,.08f,.14f));if(engine.DLSSOn())engine.ToggleDLSS();
}

void CharacterLabGame::OnStop(Engine& engine){scene_.SetBoxMesh({});engine.DestroyMesh3D(boxMesh_.handle);engine.Audio().DestroySound(jumpSound_);boxMesh_={};jumpSound_={};}

void CharacterLabGame::OnUpdate(Engine& engine,float deltaTime){
    Input& input=engine.GetInput();if(input.Pressed(VK_ESCAPE))engine.Quit();if(input.Pressed(VK_SPACE))automatic_=!automatic_;if(input.Pressed(VK_F1)){firstPerson_=true;automatic_=false;++modeSwitches_;}if(input.Pressed(VK_F2)){firstPerson_=false;automatic_=false;++modeSwitches_;}
    CharacterControl3D control{};bool requestedJump=false;if(automatic_){const int phase=CharacterLabRules::AutomaticPhase(engine.Time());if(phase!=automaticPhase_){automaticPhase_=phase;firstPerson_=CharacterLabRules::FirstPerson(phase);++modeSwitches_;}control.movement=CharacterLabRules::Movement(phase);yaw_=std::atan2(control.movement.x,control.movement.z);const int jumpCycle=CharacterLabRules::JumpCycle(engine.Time());requestedJump=jumpCycle!=lastJumpCycle_&&controller_.Grounded();if(requestedJump)lastJumpCycle_=jumpCycle;}else{yaw_+=(float(input.Down(VK_RIGHT))-float(input.Down(VK_LEFT))-input.MouseDelta().x*.025f)*deltaTime*2.2f;pitch_=Clamp(pitch_+(float(input.Down(VK_UP))-float(input.Down(VK_DOWN))+input.MouseDelta().y*.02f)*deltaTime*1.8f,-.45f,.8f);const Vec3 forward{std::sin(yaw_),0,std::cos(yaw_)},right{std::cos(yaw_),0,-std::sin(yaw_)};control.movement=forward*(float(input.Down('W'))-float(input.Down('S'))+input.GamepadLeftY())+right*(float(input.Down('D'))-float(input.Down('A'))+input.GamepadLeftX());requestedJump=input.Pressed(VK_RETURN)||input.GamepadPressed(XINPUT_GAMEPAD_A);}control.jump=requestedJump;const uint64_t previousJumps=controller_.Stats().jumps;controller_.Step(control,collisionWorld_,deltaTime);if(controller_.Stats().jumps>previousJumps)engine.Audio().Play(jumpSound_);
    const auto stats=controller_.Stats();const auto physics=collisionWorld_.Stats();engine.SetTitle(std::format(L"Aevrynd Character Lab | {} | POS {:.1f} {:.1f} {:.1f} | GROUND {} | STEP {} COLL {} JUMP {} LAND {} | PHYS Q{} H{} | SHADOW {} | CAM BLOCK {} | HIT {:.1f} Q {}/{}",firstPerson_?L"FIRST PERSON":L"THIRD PERSON",controller_.Position().x,controller_.Position().y,controller_.Position().z,controller_.Grounded()?L"YES":L"NO",stats.steps,stats.collisions,stats.jumps,stats.landings,physics.queries,physics.hits,sceneStats_.shadowInstances,cameraObstructions_,aimHit_?aimHit_.distance:0.f,queryStats_.trianglesTested,queryStats_.boundsTested));
}

void CharacterLabGame::BuildEnvironment(){
    scene_.Clear();scene_.MaterialBox({0,-.5f,0},{28,1,28},{{.035f,.11f,.12f,1},{.94f,0,0,.9f},false});scene_.Box({-13.5f,2,0},{1,4,28},{.08f,.24f,.28f,1});scene_.Box({13.5f,2,0},{1,4,28},{.08f,.24f,.28f,1});scene_.Box({0,2,-13.5f},{28,4,1},{.08f,.24f,.28f,1});scene_.Box({0,2,13.5f},{28,4,1},{.08f,.24f,.28f,1});scene_.Box({0,1,2},{4,2,1},Coral);scene_.Box({-5,1,-2},{2,2,4},{.25f,.42f,.75f,1});scene_.Box({5,.75f,4},{3,1.5f,3},Gold);for(int index=-12;index<=12;index+=2){scene_.Line({float(index),.01f,-12},{float(index),.01f,12},1,{.08f,.32f,.32f,1});scene_.Line({-12,.01f,float(index)},{12,.01f,float(index)},1,{.08f,.32f,.32f,1});}}

void CharacterLabGame::OnRender(Engine& engine,float){
    BuildEnvironment();const Vec3 feet=controller_.Position(),focus=feet+Vec3{0,1.25f,0};if(firstPerson_)camera_=MakeFirstPersonCamera(feet+Vec3{0,1.65f,0},yaw_,pitch_);else{Camera3D desired=MakeThirdPersonCamera(focus,yaw_,.28f,7.5f);camera_=ConstrainThirdPersonCamera(desired,focus,collisionWorld_,.25f);if(Length(camera_.position-focus)+.01f<Length(desired.position-focus))++cameraObstructions_;}
    aimHit_=scene_.Raycast(ScreenPointToRay(camera_,{1280,720},{640,360}),40,&queryStats_);if(aimHit_){scene_.Line(aimHit_.position-Vec3{.25f,0,0},aimHit_.position+Vec3{.25f,0,0},5,Gold);scene_.Line(aimHit_.position-Vec3{0,.25f,0},aimHit_.position+Vec3{0,.25f,0},5,Gold);scene_.Line(aimHit_.position,aimHit_.position+aimHit_.normal,3,Cyan);}
    if(!firstPerson_){scene_.Mesh(boxMesh_,{feet+Vec3{0,1.15f,0},{0,yaw_,0},{.72f,1.15f,.42f}},{{.15f,.72f,.82f,1}});scene_.Mesh(boxMesh_,{feet+Vec3{0,1.85f,0},{0,yaw_,0},{.48f,.48f,.48f}},{{.82f,.72f,.55f,1}});scene_.Mesh(boxMesh_,{feet+Vec3{-.2f,.35f,0},{0,yaw_,0},{.24f,.7f,.28f}},{{.12f,.28f,.34f,1}});scene_.Mesh(boxMesh_,{feet+Vec3{.2f,.35f,0},{0,yaw_,0},{.24f,.7f,.28f}},{{.12f,.28f,.34f,1}});}
    DrawList& draw=engine.Draw();engine.SetClearColor({.04f,.13f,.16f,1});draw.SetDirectionalLight({{.45f,-1,.25f},.25f,{1,.94f,.82f,1},.9f});draw.SetAtmosphere({{.04f,.13f,.16f,.85f},camera_.position,22,80,true});draw.AddPointLight({feet+Vec3{0,2.2f,0},8,{.15f,.72f,1,1},1.4f});draw.AddPointLight({{-5,3,-2},11,{.32f,.48f,1,1},1.7f});draw.AddPointLight({{5,3,4},11,{1,.48f,.18f,1},1.7f});sceneStats_=scene_.Submit(draw,camera_,{1280,720},{true,true,{true,.025f,.24f,45.f,128},{80.f,.25f}});draw.Quad({640,32},{1280,64},{.01f,.025f,.035f,.92f});draw.Text({28,16},firstPerson_?"CHARACTER LAB  FIRST PERSON":"CHARACTER LAB  THIRD PERSON",2.8f,firstPerson_?Gold:Cyan);draw.Text({840,17},automatic_?"AUTO VALIDATION":"MANUAL CONTROL",2.2f,Ink);draw.Line({628,360},{652,360},2,Cyan);draw.Line({640,348},{640,372},2,Cyan);const auto stats=controller_.Stats();draw.Quad({640,688},{1280,64},{.01f,.025f,.035f,.92f});draw.Text({24,672},std::format("GROUND {} COLL {} JUMP {} LAND {} MODE {} SHADOW {} LIGHT 3 HIT {:.1f}",controller_.Grounded()?"YES":"NO",stats.collisions,stats.jumps,stats.landings,modeSwitches_,sceneStats_.shadowInstances,aimHit_?aimHit_.distance:0.f),1.75f,Cyan);draw.Text({1000,672},automatic_?"SPACE MANUAL":"F1 F2 WASD",1.6f,Gold);
}
