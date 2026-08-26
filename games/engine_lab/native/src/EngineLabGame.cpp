#include "EngineLabGame.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <format>

using namespace Aevrynd;
namespace {
constexpr Color Cyan{.08f,.92f,.82f,1},Gold{1.f,.72f,.22f,1},Ink{.72f,.86f,.84f,1};
constexpr std::array<const char*,4> CameraNames={"FIRST PERSON","THIRD PERSON","GOD VIEW","ISOMETRIC 2.5D"};
}

void EngineLabGame::OnStart(Engine&){cameraMode_=0;automatic_=true;}
void EngineLabGame::OnStop(Engine&){}

void EngineLabGame::OnUpdate(Engine& engine,float){auto& input=engine.GetInput();if(input.Pressed(VK_ESCAPE))engine.Quit();if(input.Pressed(VK_SPACE))automatic_=!automatic_;for(int index=0;index<4;++index)if(input.Pressed(VK_F1+index)){cameraMode_=index;automatic_=false;}if(automatic_)cameraMode_=int(engine.Time()/4.f)%4;const RendererStats renderer=engine.GetRendererStats();engine.SetTitle(std::format(L"Aevrynd Engine Lab | {} | TRI {}/{} | FRAME {} WAIT {} DROP {}",cameraMode_+1,sceneStats_.visibleTriangles,sceneStats_.inputTriangles,renderer.submittedFrames,renderer.resourceWaits,renderer.totalDroppedVertices));}

void EngineLabGame::OnRender(Engine& engine,float){auto& draw=engine.Draw();const float time=engine.Time();draw.Quad({640,360},{1280,720},{.018f,.035f,.055f,1});scene_.Clear();for(int index=-10;index<=10;++index){const Color color=index==0?Color{.12f,.62f,.58f,.9f}:Color{.08f,.22f,.25f,.62f};scene_.Line({float(index),0,-5},{float(index),0,15},index==0?2.2f:1.f,color);scene_.Line({-10,0,float(index+5)},{10,0,float(index+5)},index==-5?2.2f:1.f,color);}for(int z=0;z<4;++z)for(int x=-2;x<=2;++x){const float height=1.f+float((x*x+z)%3);Color color{.10f+.05f*z,.42f+.06f*(x+2),.58f+.05f*z,1};scene_.Box({float(x)*3.f,height*.5f,float(z)*3.f},{1.7f,height,1.7f},color);}scene_.Box({0,3.5f,12},{3,7,3},{.78f,.28f,.22f,1});Camera3D camera;const Vec3 focus{0,1.5f,5};switch(cameraMode_){case 0:camera=MakeFirstPersonCamera({0,2.8f,-10},std::sin(time*.35f)*.16f,-.08f);break;case 1:camera=MakeThirdPersonCamera(focus,std::sin(time*.28f)*.65f,.35f,18);break;case 2:camera=MakeOverheadCamera(focus,24,true);break;default:camera=MakeIsometricCamera(focus,25,21);break;}sceneStats_=scene_.Render(draw,camera,{1280,720});const RendererStats renderer=engine.GetRendererStats();draw.Quad({640,39},{1280,78},{.01f,.025f,.04f,.96f});draw.Text({28,18},"AEVRYND ENGINE LAB",3.8f,Cyan);draw.Text({420,18},CameraNames[cameraMode_],3.1f,Gold);draw.Text({925,18},automatic_?"AUTO 4 SEC":"MANUAL",2.5f,Ink);draw.Quad({640,674},{1280,92},{.01f,.025f,.04f,.96f});draw.Text({28,642},std::format("SCENE  TRI {} / {}  CULL {}  LINE {} / {}",sceneStats_.visibleTriangles,sceneStats_.inputTriangles,sceneStats_.culledTriangles,sceneStats_.visibleLines,sceneStats_.inputLines),2.2f,Ink);draw.Text({28,675},std::format("RENDER  VTX {}  UP {} KB  FRAME {}  WAIT {}  DROP {}",renderer.lastSubmittedVertices,(renderer.lastUploadBytes+1023)/1024,renderer.submittedFrames,renderer.resourceWaits,renderer.totalDroppedVertices),2.2f,Gold);draw.Text({925,675},"F1-F4 VIEW  SPACE AUTO",1.8f,Cyan);}
