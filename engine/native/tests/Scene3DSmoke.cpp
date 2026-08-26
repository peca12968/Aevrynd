#include <Aevrynd/Scene3D.h>
#include <iostream>

using namespace Aevrynd;

int main(){
    Scene3D scene;
    scene.Box({0,1,6},{2,2,2},{.2f,.8f,.7f,1});
    scene.Line({-2,0,4},{2,0,4},2,{1,1,1,1});
    if(scene.TriangleCount()!=12||scene.LineCount()!=1){std::cerr<<"Scene box topology failed\n";return 1;}
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
    return 0;
}
