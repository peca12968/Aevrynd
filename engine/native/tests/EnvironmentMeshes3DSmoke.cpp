#include <Aevrynd/EnvironmentMeshes3D.h>
#include <array>
#include <chrono>
#include <cmath>
#include <iostream>
#include <vector>

using namespace Aevrynd;

namespace {
bool HasSoundTopology(const Mesh3D& mesh){
    if(!mesh.Valid())return false;
    std::vector<bool> referenced(mesh.vertices.size());
    for(uint32_t index:mesh.indices)referenced[index]=true;
    for(size_t index=0;index<mesh.vertices.size();++index){
        const Vertex3D& vertex=mesh.vertices[index];
        const Vec4 normal=UnpackNormalSnorm8(vertex.normal);
        const float length=std::sqrt(normal.x*normal.x+normal.y*normal.y+normal.z*normal.z);
        if(!std::isfinite(vertex.x)||!std::isfinite(vertex.y)||!std::isfinite(vertex.z)||!std::isfinite(length)||(referenced[index]&&(length<.85f||length>1.15f)))return false;
    }
    for(size_t offset=0;offset<mesh.indices.size();offset+=3){
        const Vertex3D& a=mesh.vertices[mesh.indices[offset]],&b=mesh.vertices[mesh.indices[offset+1]],&c=mesh.vertices[mesh.indices[offset+2]];
        const Vec3 edgeA{b.x-a.x,b.y-a.y,b.z-a.z},edgeB{c.x-a.x,c.y-a.y,c.z-a.z};
        if(Length(Cross(edgeA,edgeB))<.000001f)return false;
    }
    return true;
}
bool EqualMesh(const Mesh3D& a,const Mesh3D& b){
    if(a.indices!=b.indices||a.vertices.size()!=b.vertices.size())return false;
    for(size_t index=0;index<a.vertices.size();++index){const Vertex3D& x=a.vertices[index],&y=b.vertices[index];if(x.x!=y.x||x.y!=y.y||x.z!=y.z||x.normal!=y.normal||x.color!=y.color||x.u!=y.u||x.v!=y.v)return false;}
    return true;
}
}

int main(){
    const Mesh3D grassHigh=CreateGrassClumpMesh3D(11,3),grassLow=CreateGrassClumpMesh3D(4,2),crownHigh=CreateBroadleafCrownMesh3D(10,6),crownLow=CreateBroadleafCrownMesh3D(6,4),treeNear=CreateBroadleafTreeMesh3D(EnvironmentMeshDetail3D::Near,17),treeFar=CreateBroadleafTreeMesh3D(EnvironmentMeshDetail3D::Far,17),coniferNear=CreateConiferTreeMesh3D(EnvironmentMeshDetail3D::Near,23),coniferFar=CreateConiferTreeMesh3D(EnvironmentMeshDetail3D::Far,23),treeLow=CreateTreeLodMesh3D(),skyDome=CreateSkyDomeMesh3D(),mountainNear=CreateMountainMesh3D(24,6,17),mountainFar=CreateMountainMesh3D(10,3,17),rangeNear=CreateMountainRangeMesh3D(EnvironmentMeshDetail3D::Near,31),rangeFar=CreateMountainRangeMesh3D(EnvironmentMeshDetail3D::Far,31),rockNear=CreateRockMesh3D(EnvironmentMeshDetail3D::Near,17),rockFar=CreateRockMesh3D(EnvironmentMeshDetail3D::Far,17);
    if(!grassHigh.Valid()||!grassLow.Valid()||grassLow.indices.size()>=grassHigh.indices.size()){std::cerr<<"Grass LOD generation failed\n";return 1;}
    bool anchoredBlade=false,windTip=false;for(const Vertex3D& vertex:grassHigh.vertices){const float weight=UnpackNormalSnorm8(vertex.normal).w;anchoredBlade|=weight>.95f;windTip|=weight<.05f;}if(!anchoredBlade||!windTip){std::cerr<<"Grass wind weights failed\n";return 7;}
    if(!HasSoundTopology(grassHigh)||grassHigh.vertices.size()>100||grassHigh.indices.size()/3>140){std::cerr<<"Grass topology or budget failed\n";return 14;}
    if(!crownHigh.Valid()||!crownLow.Valid()||crownLow.indices.size()>=crownHigh.indices.size()){std::cerr<<"Crown LOD generation failed\n";return 2;}
    if(!treeLow.Valid()||treeLow.Bounds().extents.y<.6f){std::cerr<<"Tree LOD generation failed\n";return 5;}
    if(!HasSoundTopology(treeNear)||!HasSoundTopology(treeFar)){std::cerr<<"Broadleaf tree topology or normals failed\n";return 9;}
    if(!EqualMesh(treeNear,CreateBroadleafTreeMesh3D(EnvironmentMeshDetail3D::Near,17))){std::cerr<<"Broadleaf tree seed is not deterministic\n";return 13;}
    if(!EqualMesh(treeFar,CreateBroadleafTreeMesh3D(EnvironmentMeshDetail3D::Far,17))||!EqualMesh(crownLow,CreateBroadleafCrownMesh3D(6,4))){std::cerr<<"Far crown generation is not deterministic\n";return 16;}
    if(treeNear.vertices.size()>1300||treeNear.indices.size()/3>950||treeFar.vertices.size()>280||treeFar.indices.size()/3>240||treeFar.indices.size()>=treeNear.indices.size()/3){std::cerr<<"Broadleaf tree LOD budget failed\n";return 10;}
    if(!HasSoundTopology(coniferNear)||!HasSoundTopology(coniferFar)){std::cerr<<"Conifer topology or normals failed\n";return 21;}
    if(!EqualMesh(coniferNear,CreateConiferTreeMesh3D(EnvironmentMeshDetail3D::Near,23))||!EqualMesh(coniferFar,CreateConiferTreeMesh3D(EnvironmentMeshDetail3D::Far,23))){std::cerr<<"Conifer seed is not deterministic\n";return 22;}
    if(EqualMesh(coniferNear,CreateConiferTreeMesh3D(EnvironmentMeshDetail3D::Near,29))){std::cerr<<"Conifer seed variation failed\n";return 23;}
    const MeshBounds3D coniferNearBounds=coniferNear.Bounds(),coniferFarBounds=coniferFar.Bounds();if(coniferNearBounds.extents.x<.55f||coniferNearBounds.extents.y<1.25f||coniferNearBounds.extents.z<.55f||coniferFarBounds.extents.x<.5f||coniferFarBounds.extents.y<1.2f||coniferFarBounds.extents.z<.5f){std::cerr<<"Conifer bounds failed\n";return 24;}
    if(coniferNear.vertices.size()>400||coniferNear.indices.size()/3>420||coniferFar.vertices.size()>100||coniferFar.indices.size()/3>120||coniferFar.indices.size()>=coniferNear.indices.size()/3){std::cerr<<"Conifer LOD budget failed\n";return 25;}
    const MeshBounds3D nearBounds=treeNear.Bounds(),farBounds=treeFar.Bounds();
    if(nearBounds.extents.y<1.f||nearBounds.extents.x<.45f||nearBounds.extents.z<.35f||farBounds.extents.y<.8f){std::cerr<<"Broadleaf tree silhouette bounds failed\n";return 11;}
    const Mesh3D treeVariation=CreateBroadleafTreeMesh3D(EnvironmentMeshDetail3D::Near,19);bool varied=false;for(size_t index=0;index<treeNear.vertices.size()&&index<treeVariation.vertices.size();++index){const Vertex3D& a=treeNear.vertices[index],&b=treeVariation.vertices[index];if(std::abs(a.x-b.x)+std::abs(a.y-b.y)+std::abs(a.z-b.z)>.001f){varied=true;break;}}if(!varied){std::cerr<<"Broadleaf tree variation failed\n";return 12;}
    const Vec4 skyNormal=UnpackNormalSnorm8(skyDome.vertices.front().normal);if(!skyDome.Valid()||skyNormal.w>-.99f){std::cerr<<"Sky dome generation failed\n";return 6;}
    if(!HasSoundTopology(mountainNear)||!HasSoundTopology(mountainFar)||mountainFar.indices.size()>=mountainNear.indices.size()/2||mountainNear.Bounds().extents.y<.45f){std::cerr<<"Mountain LOD generation failed\n";return 13;}
    if(!EqualMesh(mountainNear,CreateMountainMesh3D(24,6,17))||mountainNear.vertices.size()>160||mountainNear.indices.size()/3>270||mountainFar.vertices.size()>40){std::cerr<<"Mountain determinism or budget failed\n";return 15;}
    if(!HasSoundTopology(rangeNear)||!HasSoundTopology(rangeFar)){std::cerr<<"Mountain range topology or normals failed\n";return 27;}
    for(const Vertex3D& vertex:rangeNear.vertices)if(UnpackNormalSnorm8(vertex.normal).y<=0.f){std::cerr<<"Mountain range normals must face upward\n";return 32;}
    if(!EqualMesh(rangeNear,CreateMountainRangeMesh3D(EnvironmentMeshDetail3D::Near,31))||!EqualMesh(rangeFar,CreateMountainRangeMesh3D(EnvironmentMeshDetail3D::Far,31))){std::cerr<<"Mountain range seed is not deterministic\n";return 28;}
    if(EqualMesh(rangeNear,CreateMountainRangeMesh3D(EnvironmentMeshDetail3D::Near,37))){std::cerr<<"Mountain range seed variation failed\n";return 29;}
    const MeshBounds3D rangeNearBounds=rangeNear.Bounds(),rangeFarBounds=rangeFar.Bounds();if(rangeNearBounds.extents.x<1.45f||rangeNearBounds.extents.y<.2f||rangeNearBounds.extents.z<.7f||rangeFarBounds.extents.x<1.45f||rangeFarBounds.extents.y<.18f||rangeFarBounds.extents.z<.7f){std::cerr<<"Mountain range bounds failed\n";return 30;}
    if(rangeNear.vertices.size()>600||rangeNear.indices.size()/3>1100||rangeFar.vertices.size()>100||rangeFar.indices.size()/3>160||rangeFar.indices.size()>=rangeNear.indices.size()/5){std::cerr<<"Mountain range LOD budget failed\n";return 31;}
    if(!HasSoundTopology(rockNear)||!HasSoundTopology(rockFar)){std::cerr<<"Rock topology or normals failed\n";return 17;}
    if(!EqualMesh(rockNear,CreateRockMesh3D(EnvironmentMeshDetail3D::Near,17))||!EqualMesh(rockFar,CreateRockMesh3D(EnvironmentMeshDetail3D::Far,17))){std::cerr<<"Rock seed is not deterministic\n";return 18;}
    const MeshBounds3D rockNearBounds=rockNear.Bounds(),rockFarBounds=rockFar.Bounds();if(rockNearBounds.extents.x<.35f||rockNearBounds.extents.y<.4f||rockNearBounds.extents.z<.35f||rockFarBounds.extents.x<.3f||rockFarBounds.extents.y<.38f||rockFarBounds.extents.z<.3f){std::cerr<<"Rock bounds failed\n";return 19;}
    if(rockNear.vertices.size()>64||rockNear.indices.size()/3>125||rockFar.vertices.size()>20||rockFar.indices.size()/3>=rockNear.indices.size()/3/3){std::cerr<<"Rock LOD budget failed\n";return 20;}
    const std::array<Vec3,3> centers={Vec3{-2,0,0},Vec3{0,.1f,2},Vec3{3,0,4}};const std::array<float,3> widths={1.f,1.5f,2.f};const Mesh3D ribbon=CreateCurvedRibbonMesh3D(centers,widths,.05f);
    if(!ribbon.Valid()||ribbon.vertices.size()!=6||ribbon.indices.size()!=12){std::cerr<<"Curved ribbon generation failed\n";return 3;}
    const Vertex3D& ribbonA=ribbon.vertices[ribbon.indices[0]],&ribbonB=ribbon.vertices[ribbon.indices[1]],&ribbonC=ribbon.vertices[ribbon.indices[2]];if(Cross(Vec3{ribbonB.x-ribbonA.x,ribbonB.y-ribbonA.y,ribbonB.z-ribbonA.z},Vec3{ribbonC.x-ribbonA.x,ribbonC.y-ribbonA.y,ribbonC.z-ribbonA.z}).y<=0){std::cerr<<"Ribbon front face must point upward\n";return 8;}
    if(CreateCurvedRibbonMesh3D(std::span<const Vec3>{centers}.first(2),widths).Valid()){std::cerr<<"Mismatched ribbon inputs must fail\n";return 4;}
    std::size_t generatedTriangles{};const auto generationStart=std::chrono::steady_clock::now();for(unsigned seed=0;seed<128;++seed){const Mesh3D generated=CreateConiferTreeMesh3D(seed&1u?EnvironmentMeshDetail3D::Near:EnvironmentMeshDetail3D::Far,seed);generatedTriangles+=generated.indices.size()/3;}const auto generationUs=std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now()-generationStart).count();const auto rangeStart=std::chrono::steady_clock::now();for(unsigned seed=0;seed<64;++seed){const Mesh3D generated=CreateMountainRangeMesh3D(seed&1u?EnvironmentMeshDetail3D::Near:EnvironmentMeshDetail3D::Far,seed);generatedTriangles+=generated.indices.size()/3;}const auto rangeGenerationUs=std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now()-rangeStart).count();if(!generatedTriangles){return 26;}
    std::cout<<"grassHighTriangles="<<grassHigh.indices.size()/3<<" grassLowTriangles="<<grassLow.indices.size()/3<<" mountainNearTriangles="<<mountainNear.indices.size()/3<<" mountainFarTriangles="<<mountainFar.indices.size()/3<<" mountainNearVertices="<<mountainNear.vertices.size()<<" mountainFarVertices="<<mountainFar.vertices.size()<<" rangeNearTriangles="<<rangeNear.indices.size()/3<<" rangeFarTriangles="<<rangeFar.indices.size()/3<<" rangeNearVertices="<<rangeNear.vertices.size()<<" rangeFarVertices="<<rangeFar.vertices.size()<<" range64GenerationUs="<<rangeGenerationUs<<" rockNearTriangles="<<rockNear.indices.size()/3<<" rockFarTriangles="<<rockFar.indices.size()/3<<" coniferNearTriangles="<<coniferNear.indices.size()/3<<" coniferFarTriangles="<<coniferFar.indices.size()/3<<" coniferNearVertices="<<coniferNear.vertices.size()<<" coniferFarVertices="<<coniferFar.vertices.size()<<" conifer128GenerationUs="<<generationUs<<" crownHighTriangles="<<crownHigh.indices.size()/3<<" crownLowTriangles="<<crownLow.indices.size()/3<<" treeNearTriangles="<<treeNear.indices.size()/3<<" treeFarTriangles="<<treeFar.indices.size()/3<<" treeNearVertices="<<treeNear.vertices.size()<<" treeFarVertices="<<treeFar.vertices.size()<<'\n';
}
