#include "EngineLabGame.h"
#include <Aevrynd/MeshLoader.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <format>

using namespace Aevrynd;
namespace {
constexpr Color Cyan{.08f,.92f,.82f,1},Gold{1.f,.72f,.22f,1},Ink{.72f,.86f,.84f,1};
constexpr std::array<const char*,4> CameraNames={"FIRST PERSON","THIRD PERSON","GOD VIEW","ISOMETRIC 2.5D"};
uint64_t AverageWaitMicroseconds(const RendererStats& stats){return stats.resourceWaits?stats.totalResourceWaitNanoseconds/stats.resourceWaits/1000:0;}
uint64_t AverageGpuMicroseconds(const RendererStats& stats){return stats.gpuTimingSamples?stats.totalGpuFrameNanoseconds/stats.gpuTimingSamples/1000:0;}
}

void EngineLabGame::OnStart(Engine& engine){
    cameraMode_=0;automatic_=true;cullingEnabled_=true;instancingEnabled_=true;backfaceCulling_=true;lightingEnabled_=true;cameraRig_.Reset(CameraViewMode3D::FirstPerson,{0,3.f,-12});camera_=cameraRig_.Camera();
    boxMesh_=engine.CreateMeshResource3D(Mesh3D::CreateUnitBox());pyramidMesh_=engine.CreateMeshResource3D(Mesh3D::CreateUnitPyramid());const MeshLoadResult3D probe=LoadObjMeshFile(engine.ExecutableDirectory()/"assets"/"engine_probe.obj");if(probe)probeMesh_=engine.CreateMeshResource3D(probe.mesh);scene_.SetBoxMesh(boxMesh_);
    if(engine.DLSSOn())engine.ToggleDLSS();
}

void EngineLabGame::OnStop(Engine& engine){scene_.SetBoxMesh({});engine.DestroyMesh3D(boxMesh_.handle);engine.DestroyMesh3D(pyramidMesh_.handle);engine.DestroyMesh3D(probeMesh_.handle);boxMesh_={};pyramidMesh_={};probeMesh_={};}

void EngineLabGame::OnUpdate(Engine& engine,float deltaTime){
    auto& input=engine.GetInput();if(input.Pressed(VK_ESCAPE))engine.Quit();if(input.Pressed(VK_SPACE)){automatic_=!automatic_;if(!automatic_){const Vec3 anchor=cameraMode_==0?Vec3{0,3.f,-12}:Vec3{0,1.5f,25};cameraRig_.Reset(CameraViewMode3D(cameraMode_),anchor);camera_=cameraRig_.Camera();}}if(input.Pressed(VK_F5))cullingEnabled_=!cullingEnabled_;if(input.Pressed(VK_F6))instancingEnabled_=!instancingEnabled_;if(input.Pressed(VK_F7))backfaceCulling_=!backfaceCulling_;if(input.Pressed(VK_F8))lightingEnabled_=!lightingEnabled_;
    for(int index=0;index<4;++index)if(input.Pressed(VK_F1+index)){cameraMode_=index;automatic_=false;const Vec3 anchor=index==0?Vec3{0,3.f,-12}:Vec3{0,1.5f,25};cameraRig_.Reset(CameraViewMode3D(index),anchor);camera_=cameraRig_.Camera();}if(automatic_)cameraMode_=int(engine.Time()/4.f)%4;
    if(!automatic_){const Vec2 mouseLook=input.Down(VK_SHIFT)?input.MouseDelta()*-.035f:Vec2{};CameraControl3D control;control.movement={float(input.Down('D'))-float(input.Down('A'))+input.GamepadLeftX(),float(input.Down('E'))-float(input.Down('Q')),float(input.Down('W'))-float(input.Down('S'))+input.GamepadLeftY()};control.look={float(input.Down(VK_RIGHT))-float(input.Down(VK_LEFT))+input.GamepadRightX()-mouseLook.x,float(input.Down(VK_UP))-float(input.Down(VK_DOWN))+input.GamepadRightY()+mouseLook.y};control.zoom=float(input.Down(VK_OEM_PLUS)||input.Down(VK_ADD))-float(input.Down(VK_OEM_MINUS)||input.Down(VK_SUBTRACT))+input.MouseWheel()*8.f;camera_=cameraRig_.Update(control,deltaTime);}
    const RendererStats renderer=engine.GetRendererStats();
    engine.SetTitle(std::format(L"Aevrynd Engine Lab | {} | OBJ {}/{} | HIT {:.1f} Q {}/{} | MESH {}/{} CULL {} | INST {} V {} I {} | GPU {}/{}US | W {}/{}US DROP {}",cameraMode_+1,probeMesh_.vertexCount,probeMesh_.TriangleCount(),sceneHit_?sceneHit_.distance:0.f,queryStats_.trianglesTested,queryStats_.boundsTested,sceneStats_.visibleMeshInstances,sceneStats_.meshInstances,sceneStats_.culledMeshInstances,renderer.lastSubmittedMeshInstances,renderer.lastSubmittedWorldVertices,renderer.lastSubmittedWorldIndices,renderer.lastGpuFrameNanoseconds/1000,AverageGpuMicroseconds(renderer),renderer.resourceWaits,AverageWaitMicroseconds(renderer),renderer.totalDroppedVertices+renderer.totalDroppedWorldVertices+renderer.totalDroppedWorldIndices+renderer.totalDroppedMeshInstances));
}

void EngineLabGame::OnRender(Engine& engine,float){
    auto& draw=engine.Draw();const float time=engine.Time();draw.SetBackfaceCulling(backfaceCulling_);draw.SetDirectionalLight(lightingEnabled_?DirectionalLight3D{}:DirectionalLight3D{{.4f,-1.f,.3f},1.f,{1,1,1,1},0.f});scene_.Clear();
    scene_.Box({0,-.1f,30},{124,.1f,124},{.025f,.07f,.075f,1});
    for(int index=-40;index<=40;index+=2){const Color color=index==0?Color{.12f,.62f,.58f,.9f}:Color{.08f,.22f,.25f,.62f};scene_.Line({float(index),0,-30},{float(index),0,90},index==0?2.2f:1.f,color);scene_.Line({-40,0,float(index+30)},{40,0,float(index+30)},index==-30?2.2f:1.f,color);}
    for(int z=-10;z<=30;++z)for(int x=-20;x<=20;++x){const float height=.8f+float((x*x+z*z)%5)*.55f;const float hue=float((x+20)%9)/9.f;Color color{.08f+.10f*hue,.32f+.30f*(1.f-hue),.48f+.28f*float((z+10)%7)/7.f,1};if(probeMesh_&&(x*7+z*3)%17==0)scene_.Mesh(probeMesh_,{{float(x)*3.f,.75f+height*.25f,float(z)*3.f},{0,float(x+z)*.13f,0},{1.8f,1.2f+height*.6f,1.8f}},{{color.r+.25f,color.g,color.b,1}});else if((x*3+z*5)%11==0)scene_.Mesh(pyramidMesh_,{{float(x)*3.f,height*.5f,float(z)*3.f},{0,float(x-z)*.17f,0},{2.1f,height*1.4f,2.1f}},{{color.r*.9f,color.g,color.b,1}});else scene_.Box({float(x)*3.f,height*.5f,float(z)*3.f},{1.7f,height,1.7f},color);}
    scene_.Box({0,5.f,18},{4,10,4},{.78f,.28f,.22f,1});Camera3D camera=camera_;const Vec3 focus{0,1.5f,25};
    if(automatic_)switch(cameraMode_){case 0:camera=MakeFirstPersonCamera({0,3.f,-12},std::sin(time*.35f)*.28f,-.06f);break;case 1:camera=MakeThirdPersonCamera(focus,std::sin(time*.28f)*.65f,.35f,38);break;case 2:camera=MakeOverheadCamera(focus,42,true);break;default:camera=MakeIsometricCamera(focus,48,32);break;}
    sceneHit_=scene_.Raycast(ScreenPointToRay(camera,{1280,720},{640,360}),camera.projection.farPlane,&queryStats_);if(sceneHit_){const float marker=.55f;scene_.Line(sceneHit_.position-Vec3{marker,0,0},sceneHit_.position+Vec3{marker,0,0},4,Gold);scene_.Line(sceneHit_.position-Vec3{0,marker,0},sceneHit_.position+Vec3{0,marker,0},4,Gold);scene_.Line(sceneHit_.position-Vec3{0,0,marker},sceneHit_.position+Vec3{0,0,marker},4,Gold);scene_.Line(sceneHit_.position,sceneHit_.position+sceneHit_.normal*1.8f,3,Cyan);}
    sceneStats_=scene_.Submit(draw,camera,{1280,720},{cullingEnabled_,instancingEnabled_});const RendererStats renderer=engine.GetRendererStats();
    draw.Quad({640,39},{1280,78},{.01f,.025f,.04f,.96f});draw.Text({28,18},"AEVRYND ENGINE LAB",3.8f,Cyan);draw.Text({420,18},CameraNames[cameraMode_],3.1f,Gold);draw.Text({900,18},instancingEnabled_?"MULTI-MESH GPU":"MULTI-MESH CPU",2.1f,instancingEnabled_?Cyan:Gold);
    draw.Quad({640,674},{1280,92},{.01f,.025f,.04f,.96f});draw.Text({28,642},std::format("MESH {} / {} CULL {}  INST {}  DRAW {}  GPU {} / {} US",sceneStats_.visibleMeshInstances,sceneStats_.meshInstances,sceneStats_.culledMeshInstances,renderer.lastSubmittedMeshInstances,renderer.lastDrawCalls,renderer.lastGpuFrameNanoseconds/1000,AverageGpuMicroseconds(renderer)),2.1f,Ink);draw.Text({28,675},std::format("WORLD V {} I {}  PICK {:.1f} Q {} / {}  DROP {}",renderer.lastSubmittedWorldVertices,renderer.lastSubmittedWorldIndices,sceneHit_?sceneHit_.distance:0.f,queryStats_.trianglesTested,queryStats_.boundsTested,renderer.totalDroppedVertices+renderer.totalDroppedWorldVertices+renderer.totalDroppedWorldIndices+renderer.totalDroppedMeshInstances),1.8f,Gold);draw.Text({780,675},automatic_?"SPACE MANUAL":"WASD QE  ARROWS  +/-",1.7f,Cyan);
}
