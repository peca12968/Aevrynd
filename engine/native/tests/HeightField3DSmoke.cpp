#include <Aevrynd/HeightField3D.h>
#include <chrono>
#include <cmath>
#include <iostream>

using namespace Aevrynd;
bool Near(float left,float right,float tolerance=.01f){return std::abs(left-right)<=tolerance;}
int main(){
    HeightField3D field({{0,0,0},3,3,1});for(unsigned z=0;z<3;++z)for(unsigned x=0;x<3;++x)field.SetHeight(x,z,float(x+z));if(!Near(field.SampleHeight(.5f,.5f),1.f)||field.Contains(-1,0)||!field.Contains(2,2)){std::cerr<<"Height interpolation or bounds failed\n";return 1;}const Vec3 normal=field.SampleNormal(1,1);if(normal.y<.55f||normal.x>-.2f||normal.z>-.2f){std::cerr<<"Height normal failed\n";return 2;}const Mesh3D mesh=field.CreateMesh();if(!mesh.Valid()||mesh.vertices.size()!=9||mesh.indices.size()!=24||UnpackNormalSnorm8(mesh.vertices[4].normal).y<.5f){std::cerr<<"Height mesh generation failed\n";return 3;}
    HeightField3D first({{-32,0,-32},65,65,1}),second({{-32,0,-32},65,65,1}),different({{-32,0,-32},65,65,1});first.Generate(42,8,.03f,4);second.Generate(42,8,.03f,4);different.Generate(43,8,.03f,4);if(!Near(first.Height(17,29),second.Height(17,29),.00001f)||Near(first.Height(17,29),different.Height(17,29),.00001f)||first.MaximumHeight()-first.MinimumHeight()<2.f){std::cerr<<"Deterministic generation failed\n";return 4;}first.FlattenCircle({0,0,0},4,1.5f,4);if(!Near(first.SampleHeight(0,0),1.5f)||Near(first.SampleHeight(20,20),1.5f)){std::cerr<<"Terrain flattening failed\n";return 5;}
    constexpr int Samples=100000;float total{};const auto start=std::chrono::steady_clock::now();for(int index=0;index<Samples;++index)total+=first.SampleHeight(float(index%63)-31.5f,float((index*7)%63)-31.5f);const uint64_t averageNs=uint64_t(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now()-start).count())/Samples;if(averageNs>500||!std::isfinite(total)){std::cerr<<"Height sampling throughput failed\n";return 6;}std::cout<<"samples="<<first.SampleCount()<<" range="<<first.MaximumHeight()-first.MinimumHeight()<<" vertices="<<mesh.vertices.size()<<" averageNs="<<averageNs<<'\n';return 0;
}
