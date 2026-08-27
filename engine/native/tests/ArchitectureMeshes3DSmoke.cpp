#include <Aevrynd/ArchitectureMeshes3D.h>
#include <chrono>
#include <cmath>
#include <cstring>
#include <iostream>

using namespace Aevrynd;

namespace {
bool SoundMesh(const Mesh3D& mesh){if(!mesh.Valid())return false;for(const Vertex3D& vertex:mesh.vertices){const Vec4 normal=UnpackNormalSnorm8(vertex.normal);const float length=std::sqrt(normal.x*normal.x+normal.y*normal.y+normal.z*normal.z);if(!std::isfinite(vertex.x)||!std::isfinite(vertex.y)||!std::isfinite(vertex.z)||length<.85f||length>1.15f)return false;}for(size_t offset=0;offset<mesh.indices.size();offset+=3){const Vertex3D& a=mesh.vertices[mesh.indices[offset]],&b=mesh.vertices[mesh.indices[offset+1]],&c=mesh.vertices[mesh.indices[offset+2]];if(Length(Cross(Vec3{b.x-a.x,b.y-a.y,b.z-a.z},Vec3{c.x-a.x,c.y-a.y,c.z-a.z}))<.000001f)return false;}return true;}
bool EqualMesh(const Mesh3D& a,const Mesh3D& b){return a.vertices.size()==b.vertices.size()&&a.indices==b.indices&&std::memcmp(a.vertices.data(),b.vertices.data(),a.vertices.size()*sizeof(Vertex3D))==0;}
}

int main(){
    CottageMeshSettings3D settings;settings.width=7.f;settings.depth=5.f;settings.wallHeight=3.2f;settings.roofHeight=1.7f;settings.seed=41;
    const Mesh3D nearMesh=CreateCottageMesh3D(settings),repeatMesh=CreateCottageMesh3D(settings),variationMesh=[](){CottageMeshSettings3D varied;varied.width=7.f;varied.depth=5.f;varied.wallHeight=3.2f;varied.roofHeight=1.7f;varied.seed=47;return CreateCottageMesh3D(varied);}();settings.detail=ArchitectureMeshDetail3D::Far;const Mesh3D farMesh=CreateCottageMesh3D(settings),repeatFarMesh=CreateCottageMesh3D(settings);
    if(!SoundMesh(nearMesh)||!SoundMesh(farMesh)){std::cerr<<"Architecture topology or normals failed\n";return 1;}
    if(!EqualMesh(nearMesh,repeatMesh)){std::cerr<<"Architecture generation is not deterministic\n";return 2;}
    if(!EqualMesh(farMesh,repeatFarMesh)||EqualMesh(nearMesh,variationMesh)){std::cerr<<"Architecture seed behavior failed\n";return 7;}
    if(nearMesh.vertices.size()>1450||nearMesh.indices.size()/3>720||farMesh.vertices.size()>80||farMesh.indices.size()/3>45||farMesh.indices.size()>=nearMesh.indices.size()/5){std::cerr<<"Architecture LOD budget failed\n";return 3;}
    const MeshBounds3D bounds=nearMesh.Bounds();if(bounds.extents.x<3.7f||bounds.extents.y<2.4f||bounds.extents.z<2.7f||bounds.center.y<2.f){std::cerr<<"Architecture bounds failed\n";return 4;}
    if(nearMesh.indices.size()/3<farMesh.indices.size()/3+300){std::cerr<<"Near architecture detail is missing\n";return 6;}
    float minimumY=nearMesh.vertices.front().y;for(const Vertex3D& vertex:nearMesh.vertices)minimumY=(std::min)(minimumY,vertex.y);if(minimumY<-.001f){std::cerr<<"Architecture detail floats below its foundation: "<<minimumY<<'\n';return 8;}
    settings.width=1.f;if(CreateCottageMesh3D(settings).Valid()){std::cerr<<"Invalid architecture dimensions accepted\n";return 5;}
    std::size_t generatedTriangles{};const auto start=std::chrono::steady_clock::now();for(std::uint32_t seed=0;seed<256;++seed){CottageMeshSettings3D generatedSettings;generatedSettings.seed=seed;generatedSettings.detail=seed&1u?ArchitectureMeshDetail3D::Near:ArchitectureMeshDetail3D::Far;generatedTriangles+=CreateCottageMesh3D(generatedSettings).indices.size()/3;}const auto generationUs=std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now()-start).count();if(!generatedTriangles||generationUs>250000){std::cerr<<"Architecture generation time budget failed\n";return 9;}
    std::cout<<"nearTriangles="<<nearMesh.indices.size()/3<<" farTriangles="<<farMesh.indices.size()/3<<" nearVertices="<<nearMesh.vertices.size()<<" farVertices="<<farMesh.vertices.size()<<" cottage256GenerationUs="<<generationUs<<'\n';
}
