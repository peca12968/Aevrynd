#include <Aevrynd/Scene3D.h>
#include <iostream>

using namespace Aevrynd;

int main(){
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
    scene.SetBoxMesh({7});draw.Clear();stats=scene.Submit(draw,camera,{1280,720});
    if(stats.visibleMeshInstances!=1||draw.meshInstances.size()!=1||draw.meshInstances.front().mesh.value!=7||!draw.worldVertices.empty()||!draw.worldIndices.empty()){std::cerr<<"Resident scene mesh submission failed\n";return 7;}
    return 0;
}
