#include "MistwatchGeneratorDetail.h"
#include "MistwatchWorld.h"
#include <Aevrynd/Scene3D.h>
#include <bit>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>

namespace {
std::size_t checks{};
void Require(bool value,const char* message){++checks;if(!value)throw std::runtime_error(message);}
bool Same(float a,float b){return std::bit_cast<std::uint32_t>(a)==std::bit_cast<std::uint32_t>(b);}
bool Same(Aevrynd::Vec3 a,Aevrynd::Vec3 b){return Same(a.x,b.x)&&Same(a.y,b.y)&&Same(a.z,b.z);}
double DoubleDot(Aevrynd::Vec3 a,Aevrynd::Vec3 b){return double(a.x)*b.x+double(a.y)*b.y+double(a.z)*b.z;}
std::uint64_t Mix(std::uint64_t value,std::uint32_t word){return (value^word)*1099511628211ull;}
std::uint64_t Fingerprint(const Mistwatch::GeneratorDetailGeometry& geometry) {
    std::uint64_t value=1469598103934665603ull;
    for(const auto& mesh:geometry.meshes){
        value=Mix(value,static_cast<std::uint32_t>(mesh.vertices.size()));
        value=Mix(value,static_cast<std::uint32_t>(mesh.indices.size()));
        for(const auto& v:mesh.vertices){for(float f:{v.x,v.y,v.z,v.u,v.v})value=Mix(value,std::bit_cast<std::uint32_t>(f));
            value=Mix(Mix(value,v.normal),v.color);}
        for(const auto index:mesh.indices)value=Mix(value,index);
    }return value;
}
void WorldEqual(const std::vector<Aevrynd::BoxCollider3D>& a,const std::vector<Aevrynd::BoxCollider3D>& b){
    Require(a.size()==b.size(),"Actual world collider count changed");
    for(std::size_t i=0;i<a.size();++i)Require(Same(a[i].center,b[i].center)&&Same(a[i].extents,b[i].extents),"Actual world collider bytes changed");
}
Mistwatch::Session GeneratorSession(unsigned circuit){
    Mistwatch::Session session;session.collected=7;session.repairMaterials=7;session.bridgeRepaired=true;
    session.guardianDefeated=true;session.fuseClueRead=true;session.fuseDials=Mistwatch::Session::solvedFuseDials;
    session.fuseCollected=true;session.circuit=circuit;session.feet=Mistwatch::Session::generator+Aevrynd::Vec3{-1.6f,0,0};return session;
}
}
int main(){try {
    using namespace Aevrynd;
    const auto oldCollider=Mistwatch::GeneratorSolid();
    const auto legacyBefore=Mistwatch::WorldSolids(false);
    const auto woodBefore=Mistwatch::WorldSolids(false,Mistwatch::FirLayoutSelector::ForestWood);
    const auto repairBefore=Mistwatch::WorldSolids(true,Mistwatch::FirLayoutSelector::ForestWood);
    Require(Same(oldCollider.center,Vec3{7,.6f,65})&&Same(oldCollider.extents,Vec3{.85f,.6f,.6f}),"Baseline generator collider changed");
    const auto geometry=Mistwatch::CreateGeneratorDetail();const auto repeat=Mistwatch::CreateGeneratorDetail();
    Require(geometry.meshes.size()==3&&geometry.parts.size()>=60&&geometry.parts.size()<=96,"Material or authored part budget");
    Require(Fingerprint(geometry)==Fingerprint(repeat),"Deterministic real vertex/index bytes");
    std::size_t triangles{},vertices{};Vec3 minimum{100,100,100},maximum{-100,-100,-100};
    double minimumArea=100,minimumNormalAgreement=1,minimumOutward=100;
    std::array<std::size_t,3> coveredIndices{};
    for(std::size_t group=0;group<geometry.meshes.size();++group){
        const auto& mesh=geometry.meshes[group];const auto& again=repeat.meshes[group];
        Require(mesh.Valid(),"Actual Mesh3D rejects mesh");
        Require(mesh.vertices.size()==again.vertices.size()&&mesh.indices==again.indices,"Repeat membership");
        triangles+=mesh.indices.size()/3u;vertices+=mesh.vertices.size();
        for(std::size_t i=0;i<mesh.vertices.size();++i){const auto& v=mesh.vertices[i];const auto& w=again.vertices[i];
            Require(Same(v.x,w.x)&&Same(v.y,w.y)&&Same(v.z,w.z)&&Same(v.u,w.u)&&Same(v.v,w.v)&&v.normal==w.normal&&v.color==w.color,"Exact vertex determinism");
            Require(std::isfinite(v.x)&&std::isfinite(v.y)&&std::isfinite(v.z)&&std::isfinite(v.u)&&std::isfinite(v.v),"Nonfinite authored vertex/UV");
            Require(v.x>=-.85f&&v.x<=.85f&&v.y>=0&&v.y<=1.2f&&v.z>=-.6f&&v.z<=.6f,"Authored geometry escaped original collision envelope");
            Require(v.u>=0&&v.u<=1&&v.v>=0&&v.v<=1&&v.color==0xffffffffu,"UV/color contract");
            minimum.x=(std::min)(minimum.x,v.x);minimum.y=(std::min)(minimum.y,v.y);minimum.z=(std::min)(minimum.z,v.z);
            maximum.x=(std::max)(maximum.x,v.x);maximum.y=(std::max)(maximum.y,v.y);maximum.z=(std::max)(maximum.z,v.z);
        }
    }
    for(const auto& part:geometry.parts){
        Require(part.group<3,"Part material group");const auto& mesh=geometry.meshes[part.group];
        Require(part.firstIndex==coveredIndices[part.group]&&part.indexCount>0&&part.indexCount%3u==0&&part.firstIndex+part.indexCount<=mesh.indices.size(),"Independent part membership without gaps");
        coveredIndices[part.group]+=part.indexCount;
        for(std::size_t i=part.firstIndex;i<part.firstIndex+part.indexCount;i+=3){
            const auto& va=mesh.vertices.at(mesh.indices.at(i));const auto& vb=mesh.vertices.at(mesh.indices.at(i+1));const auto& vc=mesh.vertices.at(mesh.indices.at(i+2));
            const Vec3 a{va.x,va.y,va.z},b{vb.x,vb.y,vb.z},c{vc.x,vc.y,vc.z};
            // 獨立 double 叉積，不使用作者的 Triangle/法線函式判斷自身正確。
            const double ux=double(b.x)-a.x,uy=double(b.y)-a.y,uz=double(b.z)-a.z;
            const double vx=double(c.x)-a.x,vy=double(c.y)-a.y,vz=double(c.z)-a.z;
            const double nx=uy*vz-uz*vy,ny=uz*vx-ux*vz,nz=ux*vy-uy*vx;
            const double norm=std::sqrt(nx*nx+ny*ny+nz*nz),area=.5*norm;
            Require(std::isfinite(area)&&area>1.e-8,"Independent triangle area");minimumArea=(std::min)(minimumArea,area);
            const Vec3 centroid=(a+b+c)*(1.f/3.f)-part.centre;
            const double outward=(nx*centroid.x+ny*centroid.y+nz*centroid.z)/norm;
            Require(outward>1.e-6,"Winding points into convex authored component");minimumOutward=(std::min)(minimumOutward,outward);
            for(const auto* v:{&va,&vb,&vc}){const auto packed=UnpackNormalSnorm8(v->normal);const Vec3 n{packed.x,packed.y,packed.z};
                const double length=std::sqrt(DoubleDot(n,n)),agreement=(nx*n.x+ny*n.y+nz*n.z)/(norm*length);
                Require(std::isfinite(length)&&length>.98&&length<1.02&&agreement>.999,"Packed normal/triangle winding differs");
                Require(packed.w==1.f,"Packed lighting weight");minimumNormalAgreement=(std::min)(minimumNormalAgreement,agreement);
            }
        }
    }
    for(std::size_t group=0;group<3;++group)Require(coveredIndices[group]==geometry.meshes[group].indices.size(),"Unaccounted geometry");
    Require(minimum.y==0&&maximum.y>1.1f&&maximum.y<=1.2f,"Ground contact/top envelope");
    Require(triangles<=1800&&vertices<=5400,"Independent poly/vertex budget");
    const auto materials=Mistwatch::GeneratorDetailMaterials();
    Require(materials[0].surface.metallic<.3f&&materials[2].surface.metallic>.75f,"Coated and bare metal are distinct");
    for(const auto& material:materials)Require(material.surface.roughness>=.30f&&material.surface.roughness<=.8f&&material.surface.metallic>=0&&material.surface.metallic<=1&&material.surface.emissive==0&&material.surface.occlusion==1&&material.castsShadow&&!material.doubleSided&&material.alphaMode==MaterialAlphaMode3D::Opaque&&material.tint.a==1&&!material.textures,"Ordinary opaque bounded PBR materials");
    Scene3D scene;
    // 只建立 CPU 描述符，不呼叫 Engine/Renderer 或實際上傳 GPU。
    for(std::size_t group=0;group<3;++group){const auto& mesh=geometry.meshes[group];
        const MeshResource3D descriptor{MeshHandle3D{static_cast<std::uint32_t>(group+1u)},mesh.Bounds(),mesh.vertices.size(),mesh.indices.size(),std::make_shared<const Mesh3D>(mesh)};
        Require(scene.Mesh(descriptor,{Mistwatch::Session::generator,{}, {1,1,1}},materials[group])!=0,"Actual Scene3D CPU descriptor insertion");
    }
    Require(scene.MeshCount()==3&&scene.TriangleCount()==triangles,"Three shared material submissions");scene.Clear();
    Require(scene.MeshCount()==0&&scene.TriangleCount()==0,"Scene clear does not accumulate");
    DrawList draw;for(std::size_t group=0;group<3;++group)Require(SubmitMesh(draw,geometry.meshes[group],{Mistwatch::Session::generator,{}, {1,1,1}},materials[group]),"Actual CPU world mesh submission");
    Require(draw.worldVertices.size()==vertices&&draw.worldIndices.size()==triangles*3u,"True submitted triangle population");
    for(const auto& v:draw.worldVertices)Require(v.x>=6.15f&&v.x<=7.85f&&v.y>=0&&v.y<=1.2f&&v.z>=64.4f&&v.z<=65.6f,"World transformed device inside actual generator collider");
    WorldEqual(legacyBefore,Mistwatch::WorldSolids(false));WorldEqual(woodBefore,Mistwatch::WorldSolids(false,Mistwatch::FirLayoutSelector::ForestWood));
    WorldEqual(repairBefore,Mistwatch::WorldSolids(true,Mistwatch::FirLayoutSelector::ForestWood));
    Require(Same(oldCollider.center,Mistwatch::GeneratorSolid().center)&&Same(oldCollider.extents,Mistwatch::GeneratorSolid().extents),"Exact generator collider invariant");
    constexpr unsigned masks[]{3,6,4};
    for(unsigned circuit=0;circuit<8;++circuit){
        for(unsigned lever=0;lever<3;++lever){auto session=GeneratorSession(circuit);const auto saved=session.Serialize();
            Require(session.Valid()&&!saved.empty(),"Valid real circuit input");
            const auto unused=Mistwatch::CreateGeneratorDetail();(void)unused;
            Require(session.Serialize()==saved,"Static geometry must not mutate Session");
            Require(session.ToggleCircuit(lever)&&session.circuit==(circuit^masks[lever]),"Real existing circuit XOR semantics");
            Require(session.RepairGenerator()==(session.circuit==7),"Real power activation rule unchanged");
        }
        auto session=GeneratorSession(circuit);const auto original=session.Serialize();
        Require(!session.ToggleCircuit(3)&&session.Serialize()==original,"Invalid lever remains atomic");
        session.feet={0,0,0};Require(!session.ToggleCircuit(0)&&!session.RepairGenerator(),"Existing proximity guard retained");
    }
    auto solved=GeneratorSession(0);Require(solved.ToggleCircuit(0)&&solved.ToggleCircuit(2)&&solved.circuit==7&&solved.RepairGenerator()&&solved.CurrentStage()==Mistwatch::Stage::Relays,"Original generator solution and next stage unchanged");
    std::cout<<"GENERATOR_DETAIL_CPU_PASS checks="<<checks<<" groups=3 parts="<<geometry.parts.size()<<" triangles="<<triangles<<" vertices="<<vertices<<" fingerprint="<<Fingerprint(geometry)
        <<" min="<<minimum.x<<','<<minimum.y<<','<<minimum.z<<" max="<<maximum.x<<','<<maximum.y<<','<<maximum.z<<" min_area="<<minimumArea<<" min_normal_agreement="<<minimumNormalAgreement<<" min_outward="<<minimumOutward<<"\n";
    return 0;
}catch(const std::exception& error){std::cerr<<"FIRST_DIVERGENCE checks="<<checks<<" error="<<error.what()<<'\n';return 1;}}
