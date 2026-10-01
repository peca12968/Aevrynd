#include "MistwatchRiver.h"
#include "MistwatchWorld.h"
#include <Aevrynd/Scene3D.h>
#include <iostream>
int main() {
    using namespace Aevrynd;
    const auto terrain=Mistwatch::CreateTerrain();
    float lowestShore=100.f,highestShore=-100.f,previousShore=0.f;
    for(int x=-50;x<=50;++x) {
        const float shore=Mistwatch::RiverShoreZ(terrain,float(x),false);
        lowestShore=(std::min)(lowestShore,shore);highestShore=(std::max)(highestShore,shore);
        if(x>-50 && std::abs(shore-previousShore)>.65f)return 11;
        previousShore=shore;
    }
    if(highestShore-lowestShore<4.f) {
        std::cerr<<"FIRST_DIVERGENCE: river shoreline is still straight; span="<<highestShore-lowestShore<<'\n';
        return 10;
    }
    const auto repeat=Mistwatch::CreateTerrain();
    if(terrain.SampleCount()!=48441 || terrain.Settings().cellSize!=.5f)return 12;
    for(unsigned z=0;z<241;++z)for(unsigned x=0;x<201;++x) {
        if(terrain.Height(x,z)!=repeat.Height(x,z))return 13;
        const auto normal=terrain.SampleNormal(float(x)*.5f-50.f,float(z)*.5f-20.f);
        if(!std::isfinite(normal.x) || !std::isfinite(normal.y) || !std::isfinite(normal.z) || normal.y<=0)return 14;
    }
    for(float x:{-40.f,-20.f,0.f,20.f,40.f}) {
        const float center=Mistwatch::RiverCenterZ(x);
        unsigned transitionSamples=0;
        for(int offset=-32;offset<=32;++offset) {
            const float z=center+float(offset)*.25f;
            const float height=terrain.SampleHeight(x,z);
            if(height> -7.f && height< -1.f)++transitionSamples;
        }
        if(transitionSamples<8)return 15;
    }
    for(const auto p:Mistwatch::GroundCoverPositions(terrain)) {
        if(p.y<=Mistwatch::RiverHeight || Mistwatch::RiverDistance(p.x,p.z)<Mistwatch::RiverOuterWidth(p.x)+1.5f)return 16;
    }
    for(const auto p:Mistwatch::TreePositions()) {
        if(p.y<=Mistwatch::RiverHeight || Mistwatch::RiverDistance(p.x,p.z)<Mistwatch::RiverOuterWidth(p.x)+2.f)return 17;
    }
    const auto dryColors=terrain.CreateMesh({{.65f,.75f,.55f,1},{.75f,.72f,.62f,1},{.45f,.43f,.39f,1}});
    const auto bankColors=terrain.CreateMesh(Mistwatch::GroundMeshSettings());
    unsigned bankVertices=0;
    for(std::size_t index=0;index<dryColors.vertices.size();++index) {
        const auto& a=dryColors.vertices[index];const auto& b=bankColors.vertices[index];
        if(a.x!=b.x || a.y!=b.y || a.z!=b.z || a.normal!=b.normal)return 18;
        if(a.color!=b.color) {
            if(std::abs(a.y-Mistwatch::RiverHeight)>=3.f)return 19;
            ++bankVertices;
        }
    }
    if(bankVertices<100)return 20;
    const auto mesh=Mistwatch::CreateRiverMesh(terrain);
    if(!mesh.Valid() || mesh.indices.size()<4800 || mesh.indices.size()>30000)return 1;
    for(std::size_t index=0;index<mesh.vertices.size();++index) {
        const auto& vertex=mesh.vertices[index];
        if(vertex.y!=Mistwatch::RiverHeight || terrain.SampleHeight(vertex.x,vertex.z)>vertex.y+.001f)return 2;
    }
    float maximumBankIntersection=0.f;
    for(std::size_t offset=0;offset<mesh.indices.size();offset+=3) {
        const auto p=[&](unsigned index){const auto& v=mesh.vertices[index];return Vec3{v.x,v.y,v.z};};
        const auto a=p(mesh.indices[offset]),b=p(mesh.indices[offset+1]),c=p(mesh.indices[offset+2]);
        if(Cross(b-a,c-a).y<=0)return 4;
        for(unsigned u=0;u<=8;++u)for(unsigned v=0;v<=8-u;++v) {
            const auto point=a+(b-a)*(float(u)/8.f)+(c-a)*(float(v)/8.f);
            maximumBankIntersection=(std::max)(maximumBankIntersection,terrain.SampleHeight(point.x,point.z)-point.y);
        }
    }
    if(maximumBankIntersection>.02f) {
        std::cerr<<"FIRST_DIVERGENCE: water crosses dry terrain; maximum="<<maximumBankIntersection<<'\n';return 21;
    }
    for(int x=-50;x<=50;++x)for(bool farBank:{false,true}) {
        const float shore=Mistwatch::RiverShoreZ(terrain,float(x),farBank);
        bool matched=false;
        for(const auto& vertex:mesh.vertices)matched|=std::abs(vertex.x-float(x))<.0001f && std::abs(vertex.z-shore)<.001f;
        if(!matched)return 3;
    }
    const auto material=Mistwatch::RiverMaterial();
    if(material.materialClass!=MaterialClass3D::Water || material.alphaMode!=MaterialAlphaMode3D::Opaque ||
       material.castsShadow || !material.doubleSided || material.tint.a<=0 || material.tint.a>=1)return 5;
    DrawList draw;
    Scene3D scene;
    const MeshResource3D resource{{1},mesh.Bounds(),mesh.vertices.size(),mesh.indices.size()};
    scene.Mesh(resource,{},material);
    Camera3D camera;camera.position={0,8,28};camera.target={0,-4,36};
    Scene3DSubmissionOptions options;options.frustumCulling=false;
    scene.Submit(draw,camera,{1280,720},options);
    if(draw.meshInstances.size()!=1 || draw.meshInstances.front().layer!=MeshRenderLayer3D::Transparent ||
       draw.meshInstances.front().materialClass!=MaterialClass3D::Water || draw.meshInstances.front().castsShadow) {
        std::cerr<<"FIRST_DIVERGENCE: river rejected before transparent submission\n";return 9;
    }
    const auto settings=Mistwatch::RiverSurface();
    draw.SetWaterSurface(settings);
    const auto receipt=draw.WaterSurfaceReceipt();
    if(!receipt.normalized || !receipt.singleCompositeRequested || !receipt.fingerprint ||
       receipt.fingerprint!=WaterSurfaceFingerprint3D(settings) || settings.broadWaveStrength<=0 || settings.detailWaveStrength<=0)return 6;
    const auto solids=Mistwatch::WorldSolids(false);
    for(const auto& b:solids) {
        if(std::abs(b.center.z-36)<1.f && b.center.y< -1.f && b.extents.x>20.f)return 7;
    }
    HeightField3D dry({{-50,0,-20},51,61,2.f});
    if(Mistwatch::CreateRiverMesh(dry).Valid())return 8;
    std::cout<<"River terrain shore intersections, upward winding and transparent water material PASS; vertices="<<mesh.vertices.size()<<" triangles="<<mesh.indices.size()/3
        <<" shore span="<<highestShore-lowestShore<<" maximum bank intersection="<<maximumBankIntersection<<" bank vertices="<<bankVertices<<'\n';
}
