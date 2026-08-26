#include <Aevrynd/Scene3D.h>
#include <cmath>
#include <iostream>

using namespace Aevrynd;
bool Near(float left,float right,float tolerance=.001f){return std::abs(left-right)<tolerance;}

int main(){
    const Matrix4 shadowMatrix=CreateDirectionalShadowMatrix({.5f,-1.f,.25f},.1f);const Vec3 projected=TransformPoint(shadowMatrix,{2,4,6});if(!Near(projected.x,3.95f)||!Near(projected.y,.1f)||!Near(projected.z,6.975f)){std::cerr<<"Directional shadow projection failed\n";return 18;}
    Scene3D scene;
    scene.Box({0,1,6},{2,2,2},{.2f,.8f,.7f,1});
    scene.Line({-2,0,4},{2,0,4},2,{1,1,1,1});
    if(scene.TriangleCount()!=12||scene.LineCount()!=1||scene.MeshCount()!=1){std::cerr<<"Scene box topology failed\n";return 1;}
    DrawList draw;
    Camera3D camera=MakeFirstPersonCamera({0,1,0},0,0);
    Scene3DStats stats=scene.Render(draw,camera,{1280,720});
    if(stats.inputTriangles!=12||stats.visibleTriangles!=12||stats.culledTriangles!=0||stats.visibleLines!=1||draw.vertices.size()!=42){std::cerr<<"Scene projection statistics failed\n";return 2;}

    scene.Clear();draw.Clear();
    const Color nearColor{1,0,0,1},farColor{0,0,1,1};
    scene.Triangle({-1,-1,5},{0,1,5},{1,-1,5},nearColor);
    scene.Triangle({-1,-1,10},{0,1,10},{1,-1,10},farColor);
    stats=scene.Render(draw,camera,{1280,720});
    if(stats.visibleTriangles!=2||draw.vertices.size()!=6||draw.vertices.front().color!=PackColorRgba8(farColor)){std::cerr<<"Stable far-to-near sorting failed\n";return 3;}
    scene.Line({-1,0,4},{1,0,4},1,{1,1,1,1});draw.Clear();stats=scene.Submit(draw,camera,{1280,720});
    if(stats.inputTriangles!=2||stats.visibleLines!=1||draw.worldVertices.size()!=10||draw.worldIndices.size()!=12||draw.vertices.size()!=0){std::cerr<<"GPU scene submission failed\n";return 4;}
    scene.Clear();draw.Clear();scene.Box({0,1,6},{2,2,2},{.2f,.8f,.7f,1});scene.Box({1000,1,6},{2,2,2},{1,.2f,.2f,1});stats=scene.Submit(draw,camera,{1280,720});
    if(stats.meshInstances!=2||stats.visibleMeshInstances!=1||stats.culledMeshInstances!=1||stats.visibleTriangles!=12||stats.culledTriangles!=12||draw.worldVertices.size()!=24||draw.worldIndices.size()!=36){std::cerr<<"Mesh frustum culling failed\n";return 5;}
    draw.Clear();stats=scene.Submit(draw,camera,{1280,720},{false});
    if(stats.visibleMeshInstances!=2||stats.culledMeshInstances!=0||draw.worldVertices.size()!=48||draw.worldIndices.size()!=72){std::cerr<<"Frustum culling override failed\n";return 6;}
    auto boxCpu=std::make_shared<Mesh3D>(Mesh3D::CreateUnitBox());scene.SetBoxMesh({{7},boxCpu->Bounds(),boxCpu->vertices.size(),boxCpu->indices.size(),boxCpu});draw.Clear();stats=scene.Submit(draw,camera,{1280,720});
    if(stats.visibleMeshInstances!=1||draw.meshInstances.size()!=1||draw.meshInstances.front().mesh.value!=7||!draw.worldVertices.empty()||!draw.worldIndices.empty()){std::cerr<<"Resident scene mesh submission failed\n";return 7;}
    draw.Clear();stats=scene.Submit(draw,camera,{1280,720},{true,true,{true,.025f,.25f,100.f,8}});if(stats.shadowCasters!=1||stats.shadowInstances!=1||stats.shadowSkipped!=0||draw.meshInstances.size()!=2||draw.meshInstances.front().layer!=MeshRenderLayer3D::Overlay||draw.meshInstances.back().layer!=MeshRenderLayer3D::Opaque){std::cerr<<"Projected shadow scene submission failed\n";return 19;}
    scene.Clear();scene.SetBoxMesh({});auto pyramidCpu=std::make_shared<Mesh3D>(Mesh3D::CreateUnitPyramid());MeshResource3D pyramid{{9},pyramidCpu->Bounds(),pyramidCpu->vertices.size(),pyramidCpu->indices.size(),pyramidCpu};scene.Mesh(pyramid,{{0,1,6},{0,.4f,0},{2,3,2}},{{.4f,.8f,1,1},{.2f,.8f,.1f,.7f}});draw.Clear();stats=scene.Submit(draw,camera,{1280,720});
    if(stats.meshInstances!=1||stats.visibleMeshInstances!=1||stats.inputTriangles!=6||stats.visibleTriangles!=6||draw.meshInstances.size()!=1||draw.meshInstances.front().mesh.value!=9||draw.meshInstances.front().surface!=PackSurface3D({.2f,.8f,.1f,.7f})){std::cerr<<"Generic resident mesh material submission failed\n";return 8;}
    draw.Clear();stats=scene.Submit(draw,camera,{1280,720},{true,false});if(!draw.meshInstances.empty()||draw.worldVertices.size()!=16||draw.worldIndices.size()!=18){std::cerr<<"Generic mesh CPU fallback failed\n";return 9;}
    Scene3D queryScene;queryScene.Box({0,1,6},{2,2,2},{1,1,1,1});SceneQueryStats3D queryStats{};const Ray3D sceneRay=ScreenPointToRay(camera,{1280,720},{640,360});SceneHit3D hit=queryScene.Raycast(sceneRay,100,&queryStats);if(!hit||hit.objectType!=SceneObjectType3D::Box||hit.objectIndex!=0||!Near(hit.distance,5)||!Near(hit.position.z,5)||!Near(hit.normal.z,-1)||queryStats.boundsTested!=1||queryStats.boundsRejected!=0||queryStats.trianglesTested!=12||queryScene.Raycast(sceneRay,4.9f)){std::cerr<<"Scene box raycast failed\n";return 10;}
    hit=queryScene.Raycast({{20,1,0},{0,0,1}},100,&queryStats);if(hit||queryStats.boundsTested!=1||queryStats.boundsRejected!=1||queryStats.trianglesTested!=0){std::cerr<<"Scene raycast broad phase failed\n";return 11;}
    queryScene.Clear();queryScene.Triangle({-2,-1,8},{0,2,8},{2,-1,8},{1,1,1,1});queryScene.Triangle({-1,-1,4},{0,1,4},{1,-1,4},{1,1,1,1});hit=queryScene.Raycast({{0,0,0},{0,0,4}},100,&queryStats);if(!hit||hit.objectType!=SceneObjectType3D::Triangle||hit.objectIndex!=1||!Near(hit.distance,4)||queryStats.trianglesTested!=2){std::cerr<<"Nearest normalized scene raycast failed\n";return 12;}
    queryScene.Clear();auto queryMeshCpu=std::make_shared<Mesh3D>(Mesh3D::CreateUnitBox());MeshResource3D queryMesh{{10},queryMeshCpu->Bounds(),queryMeshCpu->vertices.size(),queryMeshCpu->indices.size(),queryMeshCpu};queryScene.Mesh(queryMesh,{{0,1,7},{0,.4f,0},{2,3,2}});hit=queryScene.Raycast({{0,1,0},{0,0,1}},100,&queryStats);if(!hit||hit.objectType!=SceneObjectType3D::Mesh||hit.objectIndex!=0||hit.distance<=5||hit.distance>=7||queryStats.boundsRejected!=0){std::cerr<<"Transformed mesh raycast failed\n";return 13;}
    Scene3D lodScene;auto lodNearCpu=std::make_shared<Mesh3D>(Mesh3D::CreateUnitBox()),lodFarCpu=std::make_shared<Mesh3D>(Mesh3D::CreateUnitPyramid());MeshResource3D lodNear{{21},lodNearCpu->Bounds(),lodNearCpu->vertices.size(),lodNearCpu->indices.size(),lodNearCpu},lodFar{{22},lodFarCpu->Bounds(),lodFarCpu->vertices.size(),lodFarCpu->indices.size(),lodFarCpu};lodScene.MeshLod(lodFar,lodNear,10,{{0,1,6},{},{2,2,2}});if(lodScene.MeshCount()!=0){std::cerr<<"LOD must reject a more complex far mesh\n";return 14;}lodScene.MeshLod(lodNear,lodFar,10,{{0,1,6},{},{2,2,2}});draw.Clear();stats=lodScene.Submit(draw,camera,{1280,720});if(stats.lodMeshInstances!=1||stats.nearLodInstances!=1||stats.farLodInstances!=0||stats.lodTrianglesSaved!=0||draw.meshInstances.size()!=1||draw.meshInstances.front().mesh.value!=21){std::cerr<<"Near mesh LOD selection failed\n";return 15;}Camera3D farCamera=MakeFirstPersonCamera({0,1,-20},0,0);draw.Clear();stats=lodScene.Submit(draw,farCamera,{1280,720});if(stats.nearLodInstances!=0||stats.farLodInstances!=1||stats.visibleTriangles!=6||stats.culledTriangles!=6||stats.lodTrianglesSaved!=6||draw.meshInstances.size()!=1||draw.meshInstances.front().mesh.value!=22){std::cerr<<"Far mesh LOD selection failed\n";return 16;}hit=lodScene.Raycast({{0,1,0},{0,0,1}},100,&queryStats);if(!hit||hit.objectIndex!=0||queryStats.trianglesTested!=12){std::cerr<<"LOD raycast must use detailed collision mesh\n";return 17;}
    return 0;
}
