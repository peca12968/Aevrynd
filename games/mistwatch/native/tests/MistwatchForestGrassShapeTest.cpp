#include "MistwatchForestGrassShape.h"
#include <Aevrynd/MeshLoader.h>
#include <filesystem>
#include <iostream>
#include <limits>

namespace {
using namespace Aevrynd;
using namespace Mistwatch;
void Require(bool value,const char* message) {if(!value)throw std::runtime_error(message);}
bool Same(float a,float b) {return std::bit_cast<std::uint32_t>(a)==std::bit_cast<std::uint32_t>(b);}
template<class Action> void Reject(Action action,const char* message) {
    bool rejected=false;try{action();}catch(const std::invalid_argument&){rejected=true;}Require(rejected,message);
}
std::vector<std::array<double,3>> IndependentNormals(const Mesh3D& mesh) {
    std::vector<std::array<double,3>> normals(mesh.vertices.size());
    for(std::size_t face=0;face<mesh.indices.size();face+=3) {
        const auto a=mesh.indices[face],b=mesh.indices[face+1],c=mesh.indices[face+2];
        const auto& p=mesh.vertices[a];const auto& q=mesh.vertices[b];const auto& r=mesh.vertices[c];
        const std::array<double,3> u{double(q.x)-p.x,double(q.y)-p.y,double(q.z)-p.z};
        const std::array<double,3> v{double(r.x)-p.x,double(r.y)-p.y,double(r.z)-p.z};
        const std::array<double,3> cross{u[1]*v[2]-u[2]*v[1],u[2]*v[0]-u[0]*v[2],u[0]*v[1]-u[1]*v[0]};
        Require(cross[0]*cross[0]+cross[1]*cross[1]+cross[2]*cross[2]>1e-30,"Output triangle lost nonzero area");
        for(const auto vertex:{a,b,c})for(unsigned axis=0;axis<3;++axis)normals[vertex][axis]+=cross[axis];
    }
    return normals;
}
}
int main() {
    try {
        const auto root=std::filesystem::path(MISTWATCH_SOURCE_ROOT)/"engine/assets/mistwatch-grass-medium-02/meshes";
        Require(ForestGrassShapeProfile=="source-uv-curved-v1","Unexpected grass shape revision");
        unsigned totalChanged{},totalRoots{},totalMoving{},negativeCases{};
        for(unsigned variant=0;variant<5;++variant) {
            auto loaded=LoadAevryndMeshFile(root/GrassMeshName(variant));Require(bool(loaded),"Actual five-type grass source load failed");
            NormalizeGrassAsset(loaded.mesh);const auto source=loaded.mesh;
            const auto sourceHash=ForestGrassShapeFingerprint(source);const auto sourceBounds=MeasureForestGrassShapeBounds(source);
            const auto candidate=CreateForestGrassShape(source,variant);const auto repeat=CreateForestGrassShape(source,variant);
            const auto candidateBounds=MeasureForestGrassShapeBounds(candidate);const auto normalSums=IndependentNormals(candidate);
            Require(ForestGrassShapeFingerprint(source)==sourceHash&&ForestGrassShapeFingerprint(loaded.mesh)==sourceHash,"Candidate modified immutable original source");
            Require(candidate.vertices.size()==GrassVertexCounts[variant]&&candidate.indices.size()==std::size_t(GrassTriangleCounts[variant])*3&&
                candidate.indices==source.indices,"Exact source topology changed");
            Require(ForestGrassShapeFingerprint(candidate)==ForestGrassShapeFingerprint(repeat)&&
                ForestGrassShapeFingerprint(candidate)!=sourceHash,"Geometry not deterministic or not changed");
            Require(candidateBounds.rootMaximumRadius<=sourceBounds.rootMaximumRadius&&candidateBounds.maximumHeight<sourceBounds.maximumHeight&&
                candidateBounds.maximumHeight>=sourceBounds.maximumHeight*.6f&&candidateBounds.maximumHeight<=sourceBounds.maximumHeight*.7f,
                "Nonlinear short profile or conservative radial envelope missing");
            unsigned changed{},roots{},moving{},normalChanged{};float minimumHeightRatio=1.f,maximumHeightRatio=0.f;
            bool horizontalCurve=false;
            for(std::size_t index=0;index<source.vertices.size();++index) {
                const auto& original=source.vertices[index];const auto& vertex=candidate.vertices[index];
                Require(std::isfinite(vertex.x)&&std::isfinite(vertex.y)&&std::isfinite(vertex.z)&&std::isfinite(vertex.u)&&std::isfinite(vertex.v),"Nonfinite candidate vertex");
                Require(Same(vertex.u,original.u)&&Same(vertex.v,original.v)&&vertex.color==original.color,"Source UV island or color bits changed");
                Require(vertex.x>=sourceBounds.minimum.x&&vertex.x<=sourceBounds.maximum.x&&vertex.y>=sourceBounds.minimum.y&&
                    vertex.y<=sourceBounds.maximum.y&&vertex.z>=sourceBounds.minimum.z&&vertex.z<=sourceBounds.maximum.z,
                    "Candidate vertex left original full AABB");
                Require(std::hypot(vertex.x,vertex.z)<=sourceBounds.rootMaximumRadius,"Candidate vertex exceeded original rooted radius");
                const auto normal=UnpackNormalSnorm8(vertex.normal);const float length=Length({normal.x,normal.y,normal.z});
                Require(std::abs(length-1.f)<=.02f&&normal.w>=0.f&&normal.w<=1.f,"Recomputed packed normal or wind invalid");
                const auto sum=normalSums[index];const double sumLength=std::sqrt(sum[0]*sum[0]+sum[1]*sum[1]+sum[2]*sum[2]);
                Require(std::isfinite(sumLength)&&sumLength>0&&
                    (double(normal.x)*sum[0]+double(normal.y)*sum[1]+double(normal.z)*sum[2])/(double(length)*sumLength)>.999,
                    "Normal XYZ not recomputed from actual deformed geometry");
                const float expectedWind=1.f-std::clamp(vertex.y/candidateBounds.maximumHeight,0.f,1.f);
                Require(std::abs(normal.w-expectedWind)<.008f,"Candidate inverse-height wind not recomputed");
                if(original.y<=0.f) {
                    ++roots;Require(Same(vertex.x,original.x)&&Same(vertex.y,original.y)&&Same(vertex.z,original.z)&&normal.w==1.f,
                        "Original root-plane geometry moved or root wind unlocked");
                }else {
                    const float ratio=vertex.y/original.y;minimumHeightRatio=(std::min)(minimumHeightRatio,ratio);maximumHeightRatio=(std::max)(maximumHeightRatio,ratio);
                    const float t=original.y/sourceBounds.maximumHeight,inward=1.f-.18f*t*t;
                    horizontalCurve|=std::hypot(vertex.x-original.x*inward,vertex.z-original.z*inward)>.00001f;
                    moving+=normal.w<.95f;
                }
                changed+=!Same(vertex.x,original.x)||!Same(vertex.y,original.y)||!Same(vertex.z,original.z);
                normalChanged+=(vertex.normal&0x00ffffffu)!=(original.normal&0x00ffffffu);
                const auto& again=repeat.vertices[index];Require(Same(vertex.x,again.x)&&Same(vertex.y,again.y)&&Same(vertex.z,again.z)&&
                    Same(vertex.u,again.u)&&Same(vertex.v,again.v)&&vertex.normal==again.normal&&vertex.color==again.color,"Repeat full vertex bits changed");
            }
            Require(changed>source.vertices.size()/2&&normalChanged&&moving&&horizontalCurve&&maximumHeightRatio-minimumHeightRatio>.1f,
                "Profile is merely uniform whole-plant scaling or unchanged normals");
            const auto rejectedMesh=[&](Mesh3D corrupt,const char* message) {
                Reject([&](){(void)CreateForestGrassShape(corrupt,variant);},message);++negativeCases;
            };
            auto corrupt=source;corrupt.vertices[0].x=std::numeric_limits<float>::quiet_NaN();rejectedMesh(std::move(corrupt),"NaN position accepted");
            corrupt=source;corrupt.vertices[0].v=std::numeric_limits<float>::infinity();rejectedMesh(std::move(corrupt),"Infinite UV accepted");
            corrupt=source;corrupt.indices[0]=static_cast<std::uint32_t>(source.vertices.size());rejectedMesh(std::move(corrupt),"Bad index accepted");
            corrupt=source;corrupt.indices[1]=corrupt.indices[0];rejectedMesh(std::move(corrupt),"Zero-area source triangle accepted");
            corrupt=source;for(auto& vertex:corrupt.vertices)vertex.y=0.f;rejectedMesh(std::move(corrupt),"Flat root-only source accepted");
            corrupt=source;corrupt.indices.pop_back();rejectedMesh(std::move(corrupt),"Incomplete triangle topology accepted");
            Reject([&](){(void)CreateForestGrassShape(source,5);},"Out-of-range variant accepted");++negativeCases;
            Reject([&](){(void)CreateForestGrassShape(source,(variant+1)%5);},"Wrong exact source variant accepted");++negativeCases;
            totalChanged+=changed;totalRoots+=roots;totalMoving+=moving;
            std::cout<<"variant="<<variant<<" vertices="<<candidate.vertices.size()<<" triangles="<<candidate.indices.size()/3<<
                " source_height="<<sourceBounds.maximumHeight<<" candidate_height="<<candidateBounds.maximumHeight<<
                " source_radius="<<sourceBounds.rootMaximumRadius<<" candidate_radius="<<candidateBounds.rootMaximumRadius<<
                " changed="<<changed<<" roots="<<roots<<" source_fingerprint="<<sourceHash<<
                " candidate_fingerprint="<<ForestGrassShapeFingerprint(candidate)<<'\n';
        }
        Require(totalRoots&&totalMoving,"Actual source root or tip population not exercised");
        std::cout<<"FOREST_GRASS_SHAPE_CPU_PASS profile="<<ForestGrassShapeProfile<<" variants=5 changed_vertices="<<totalChanged<<
            " fixed_roots="<<totalRoots<<" moving_vertices="<<totalMoving<<" negative_cases="<<negativeCases<<
            " leaf_island_semantics=UNVERIFIED world_collision=UNVERIFIED native_visual=UNVERIFIED\n";return 0;
    }catch(const std::exception& error){std::cerr<<"FIRST_DIVERGENCE: "<<error.what()<<'\n';return 1;}
}
