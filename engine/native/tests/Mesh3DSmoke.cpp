#include <Aevrynd/Mesh3D.h>
#include <cmath>
#include <iostream>

using namespace Aevrynd;

bool Near(float left,float right){return std::abs(left-right)<.001f;}

int main(){
    Mesh3D box=Mesh3D::CreateUnitBox();
    if(!box.Valid()||box.vertices.size()!=24||box.indices.size()!=36){std::cerr<<"Indexed box topology failed\n";return 1;}
    Mesh3D invalid=box;invalid.indices.back()=999;
    if(invalid.Valid()){std::cerr<<"Invalid mesh index was accepted\n";return 2;}
    DrawList draw;Transform3D transform{{3,4,5},{0,.5f,0},{2,4,6}};
    if(!SubmitMesh(draw,box,transform,{{.4f,.8f,1.f,1.f}})||draw.worldVertices.size()!=24||draw.worldIndices.size()!=36){std::cerr<<"Mesh submission failed\n";return 3;}
    Vec3 center{};for(const auto& vertex:draw.worldVertices)center=center+Vec3{vertex.x,vertex.y,vertex.z};center=center*(1.f/float(draw.worldVertices.size()));
    if(!Near(center.x,3)||!Near(center.y,4)||!Near(center.z,5)){std::cerr<<"Mesh transform failed\n";return 4;}
    const std::vector<uint32_t> clipped={0,1,2,3,4,99};
    if(D3D12Renderer::DrawableWorldIndexCount(clipped,5)!=3){std::cerr<<"Index clipping failed\n";return 5;}
    return 0;
}
