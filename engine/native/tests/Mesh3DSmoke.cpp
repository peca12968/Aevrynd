#include <Aevrynd/Mesh3D.h>
#include <Aevrynd/MeshLoader.h>
#include <cmath>
#include <iostream>

using namespace Aevrynd;

bool Near(float left,float right,float tolerance=.001f){return std::abs(left-right)<tolerance;}

int main(){
    constexpr std::string_view smoothObj="v -1 -1 0 255 64 32 1\nv 1 -1 0 255 64 32 1\nv 1 1 0 255 64 32 1\nv -1 1 0 255 64 32 1\ns 1\nf -4 -3 -2 -1\n";MeshLoadResult3D loaded=LoadObjMesh(smoothObj);if(!loaded||loaded.mesh.vertices.size()!=4||loaded.mesh.indices.size()!=6||loaded.mesh.vertices[0].color!=PackColorRgba8({1.f,64.f/255.f,32.f/255.f,1})){std::cerr<<"OBJ quad triangulation or vertex color failed\n";return 14;}for(const Vertex3D& vertex:loaded.mesh.vertices){const Vec4 normal=UnpackNormalSnorm8(vertex.normal);if(!Near(normal.z,-1)){std::cerr<<"OBJ generated normal or handedness conversion failed\n";return 15;}}
    constexpr std::string_view normalObj="v 0 0 0\nv 1 0 0\nv 0 1 0\nv 0 0 1\nvn 0 0 1\ns off\nf 1//1 2//1 3//1\nf 1 4 2\n";loaded=LoadObjMesh(normalObj);if(!loaded||loaded.mesh.indices.size()!=6||loaded.mesh.vertices.size()!=6){std::cerr<<"OBJ explicit and flat normal parsing failed\n";return 16;}if(LoadObjMesh("v 0 0 0\nf 1 2 3\n")){std::cerr<<"Malformed OBJ was accepted\n";return 17;}
    const Vec4 packed=UnpackNormalSnorm8(PackNormalSnorm8({0,-1,.5f},0));
    if(!Near(packed.x,0)||!Near(packed.y,-1)||!Near(packed.z,.5f,.005f)||!Near(packed.w,0)){std::cerr<<"Packed normal format failed\n";return 10;}
    Mesh3D box=Mesh3D::CreateUnitBox();
    if(!box.Valid()||box.vertices.size()!=24||box.indices.size()!=36){std::cerr<<"Indexed box topology failed\n";return 1;}
    for(std::size_t index=0;index<box.indices.size();index+=3){const Vertex3D& source=box.vertices[box.indices[index]];const Vec4 unpacked=UnpackNormalSnorm8(source.normal);const Vec3 normal{unpacked.x,unpacked.y,unpacked.z},position{source.x,source.y,source.z};if(!Near(Length(normal),1,.01f)||Dot(normal,position)<=0){std::cerr<<"Box normals must point outward\n";return 11;}const auto point=[&](std::size_t offset){const Vertex3D& value=box.vertices[box.indices[index+offset]];return Vec3{value.x,value.y,value.z};};if(Dot(Normalize(Cross(point(1)-point(0),point(2)-point(0))),normal)<.99f){std::cerr<<"Box winding must match normals\n";return 12;}}
    Mesh3D invalid=box;invalid.indices.back()=999;
    if(invalid.Valid()){std::cerr<<"Invalid mesh index was accepted\n";return 2;}
    DrawList draw;Transform3D transform{{3,4,5},{0,.5f,0},{2,4,6}};
    if(!SubmitMesh(draw,box,transform,{{.4f,.8f,1.f,1.f}})||draw.worldVertices.size()!=24||draw.worldIndices.size()!=36){std::cerr<<"Mesh submission failed\n";return 3;}
    Vec3 center{};for(const auto& vertex:draw.worldVertices)center=center+Vec3{vertex.x,vertex.y,vertex.z};center=center*(1.f/float(draw.worldVertices.size()));
    if(!Near(center.x,3)||!Near(center.y,4)||!Near(center.z,5)){std::cerr<<"Mesh transform failed\n";return 4;}
    const Vec4 sourceNormal=UnpackNormalSnorm8(box.vertices[0].normal),submittedNormal=UnpackNormalSnorm8(draw.worldVertices[0].normal);const Vec3 expectedNormal=TransformNormal(CreateNormalMatrix(transform.Matrix()),{sourceNormal.x,sourceNormal.y,sourceNormal.z});if(Dot(expectedNormal,Normalize({submittedNormal.x,submittedNormal.y,submittedNormal.z}))<.999f){std::cerr<<"CPU mesh normal transform failed\n";return 13;}
    draw.MeshInstance({42},transform.Matrix(),{.4f,.8f,1,1},{.18f,.82f,.35f,.7f});if(draw.meshInstances.size()!=1||draw.meshInstances.front().mesh.value!=42||draw.meshInstances.front().tint!=PackColorRgba8({.4f,.8f,1,1})||draw.meshInstances.front().surface!=PackSurface3D({.18f,.82f,.35f,.7f})){std::cerr<<"Mesh instance material submission failed\n";return 6;}
    draw.Clear();if(!draw.meshInstances.empty()){std::cerr<<"Mesh instances were not cleared\n";return 7;}
    const std::vector<uint32_t> clipped={0,1,2,3,4,99};
    if(D3D12Renderer::DrawableWorldIndexCount(clipped,5)!=3){std::cerr<<"Index clipping failed\n";return 5;}
    Mesh3D pyramid=Mesh3D::CreateUnitPyramid();const MeshBounds3D bounds=pyramid.Bounds();if(!pyramid.Valid()||pyramid.vertices.size()!=16||pyramid.indices.size()!=18||!Near(bounds.center.x,0)||!Near(bounds.center.y,0)||!Near(bounds.center.z,0)||!Near(bounds.radius,std::sqrt(.75f))){std::cerr<<"Pyramid topology or bounds failed\n";return 8;}
    Mesh3D sphere=Mesh3D::CreateUnitSphere();const MeshBounds3D sphereBounds=sphere.Bounds();if(!sphere.Valid()||sphere.vertices.size()!=117||sphere.indices.size()!=504||!Near(sphereBounds.center.x,0)||!Near(sphereBounds.center.y,0)||!Near(sphereBounds.center.z,0)||!Near(sphereBounds.radius,.5f,.002f)){std::cerr<<"Procedural sphere topology or bounds failed\n";return 19;}for(const Vertex3D& vertex:sphere.vertices){const Vec4 normal=UnpackNormalSnorm8(vertex.normal);const Vec3 position{vertex.x,vertex.y,vertex.z};if(Dot(Normalize(position),Normalize({normal.x,normal.y,normal.z}))<.999f){std::cerr<<"Sphere normals must point outward\n";return 20;}}
    const MeshBounds3D transformed=TransformBounds(bounds,{{3,4,5},{.2f,.7f,0},{2,4,3}});if(!Near(transformed.center.x,3)||!Near(transformed.center.y,4)||!Near(transformed.center.z,5)||!Near(transformed.radius,bounds.radius*4)){std::cerr<<"Transformed mesh bounds failed\n";return 9;}const MeshBounds3D axisAligned=TransformBounds(bounds,{{3,4,5},{},{2,4,6}});if(!Near(axisAligned.extents.x,1)||!Near(axisAligned.extents.y,2)||!Near(axisAligned.extents.z,3)){std::cerr<<"Axis-aligned mesh extents failed\n";return 18;}
    return 0;
}
