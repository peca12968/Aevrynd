#include "MistwatchRepairShelterBoardUv.h"
// 原Test全文未修改；同一TU保留其全部守門，原正式CTest仍獨立存在。
#define main OriginalShelterTestMain
#include "MistwatchRepairShelterVisualTest.cpp"
#undef main
#include <cstring>
#include <limits>

namespace V882Smoke {
using namespace Aevrynd;
unsigned checks{},negatives{};
void Check(bool value,const char* label){++checks;if(!value)throw std::runtime_error(label);}
bool FloatSame(float a,float b){return std::bit_cast<std::uint32_t>(a)==std::bit_cast<std::uint32_t>(b);}
bool GeometrySame(const Vertex3D& a,const Vertex3D& b){return FloatSame(a.x,b.x)&&FloatSame(a.y,b.y)&&FloatSame(a.z,b.z)&&a.normal==b.normal&&a.color==b.color;}
bool VertexSame(const Vertex3D& a,const Vertex3D& b){return GeometrySame(a,b)&&FloatSame(a.u,b.u)&&FloatSame(a.v,b.v);}
bool Validate(const Mesh3D& mesh,const Mesh3D& original){
    if(mesh.vertices.size()!=7392||mesh.indices.size()!=10164||mesh.indices!=original.indices)return false;
    std::size_t begin{};
    for(unsigned part=0;part<5;++part){
        for(unsigned board=0;board<Mistwatch::ShelterBoardCounts[part];++board){
            const auto first=begin+std::size_t(board)*96u;
            Vec3 lo{std::numeric_limits<float>::infinity(),std::numeric_limits<float>::infinity(),std::numeric_limits<float>::infinity()},hi=lo*-1.f;
            for(std::size_t i=first;i<first+96u;++i){const auto& p=original.vertices[i];
                lo.x=(std::min)(lo.x,p.x);hi.x=(std::max)(hi.x,p.x);lo.y=(std::min)(lo.y,p.y);hi.y=(std::max)(hi.y,p.y);lo.z=(std::min)(lo.z,p.z);hi.z=(std::max)(hi.z,p.z);}
            const float phase=.1f+.025f*float((board+part*3u)%5u);
            for(std::size_t i=first;i<first+96u;++i){const auto& p=mesh.vertices[i];
                if(!GeometrySame(p,original.vertices[i])||!std::isfinite(p.u)||!std::isfinite(p.v))return false;
                if(part==0||part==4){if(!VertexSame(p,original.vertices[i]))return false;continue;}
                const auto n=UnpackNormalSnorm8(p.normal);const float ax=std::abs(n.x),ay=std::abs(n.y),az=std::abs(n.z);
                float transverse{},longitudinal{};
                if(ay>=ax&&ay>=az){
                    transverse=part==1?(p.z-lo.z)/(hi.z-lo.z):(p.x-lo.x)/(hi.x-lo.x);
                    const float depth=part==1?(p.x-lo.x)/(hi.x-lo.x):(p.z-lo.z)/(hi.z-lo.z);
                    longitudinal=phase+(p.y>=lo.y+(hi.y-lo.y)*.5f?(hi.y-lo.y)*.5f:0.f)+depth*.025f;
                }else{
                    transverse=ax>=az?(p.z-lo.z)/(hi.z-lo.z):(p.x-lo.x)/(hi.x-lo.x);
                    longitudinal=phase+(p.y-lo.y)*.5f;
                }
                const float scale=2048.f*(16.f/32767.f);
                const float expectedU=(32.f/1024.f+(48.f/1024.f)*transverse)/scale;
                if(!FloatSame(p.u,expectedU)||!FloatSame(p.v,longitudinal/scale))return false;
                // 直接檢查實際頂點域；不以GPU觀測回填golden。
                if(p.u<(32.f/1024.f)/scale||p.u>(80.f/1024.f)/scale||p.v<.1f/scale||p.v>=1.f/scale)return false;
            }
        }
        begin+=std::size_t(Mistwatch::ShelterBoardCounts[part])*96u;
    }
    if(begin!=mesh.vertices.size())return false;
    for(std::size_t i=0;i<mesh.indices.size();i+=3){
        const auto& a=mesh.vertices[mesh.indices[i]];const auto& b=mesh.vertices[mesh.indices[i+1]];const auto& c=mesh.vertices[mesh.indices[i+2]];
        Vec3 normal{};if(!TryComputeTriangleNormal({a.x,a.y,a.z},{b.x,b.y,b.z},{c.x,c.y,c.z},normal))return false;
        const auto packed=UnpackNormalSnorm8(a.normal);
        if(Dot(normal,{packed.x,packed.y,packed.z})<=.99f)return false;
        const double jac=(double(b.u)-a.u)*(double(c.v)-a.v)-(double(c.u)-a.u)*(double(b.v)-a.v);
        if(!std::isfinite(jac)||jac==0)return false;
    }
    return true;
}
template<class Change>void Reject(const Mesh3D& candidate,const Mesh3D& original,Change change,const char* name){
    auto broken=candidate;change(broken);++negatives;Check(!Validate(broken,original),name);
}
}
int main(){try{
    using namespace V882Smoke;
    Check(OriginalShelterTestMain()==0,"complete unchanged original ShelterTest");
    const auto original=Mistwatch::CreatePlankedRepairShelter();
    const auto a=Mistwatch::ShelterBoardUv::CreatePlankedRepairShelterBoardUv();
    const auto b=Mistwatch::ShelterBoardUv::CreatePlankedRepairShelterBoardUv();
    Check(Validate(a,original),"full geometry/raw ownership, all board UV and all triangle Jacobians");
    Check(a.indices==b.indices,"repeat exact indices");
    unsigned changed{},protectedVertices{},wallVertices{};
    for(std::size_t i=0;i<a.vertices.size();++i){Check(VertexSame(a.vertices[i],b.vertices[i]),"repeat exact all fields");
        Check(GeometrySame(a.vertices[i],original.vertices[i]),"whole geometry raw unchanged");
        changed+=!FloatSame(a.vertices[i].u,original.vertices[i].u)||!FloatSame(a.vertices[i].v,original.vertices[i].v);}
    std::size_t begin{};for(unsigned part=0;part<5;++part){const auto end=begin+Mistwatch::ShelterBoardCounts[part]*96u;
        for(std::size_t i=begin;i<end;++i){if(part==0||part==4){++protectedVertices;Check(VertexSame(a.vertices[i],original.vertices[i]),"roof/equipment whole raw unchanged");}else ++wallVertices;}begin=end;}
    Check(protectedVertices==2016&&wallVertices==5376&&changed>0,"owned56 walls and protected21 boards");
    constexpr std::size_t wall=20u*96u;
    Reject(a,original,[](Mesh3D& m){for(std::size_t i=wall;i<wall+96u;++i)std::swap(m.vertices[i].u,m.vertices[i].v);},"wrong90deg grain");
    Reject(a,original,[](Mesh3D& m){for(std::size_t i=wall;i<wall+96u;++i)m.vertices[i].u=m.vertices[i].v=0;},"zero UV");
    Reject(a,original,[](Mesh3D& m){m.vertices[wall].u=0;},"outside source strip");
    Reject(a,original,[](Mesh3D& m){m.vertices[wall].v+=.025f;},"wrong source phase");
    Reject(a,original,[](Mesh3D& m){m.vertices[wall].u*=Mistwatch::ShelterBoardUv::StaticDecodedScale;},"missing static decoder compensation");
    Reject(a,original,[](Mesh3D& m){m.vertices[wall].v=std::numeric_limits<float>::quiet_NaN();},"nonfinite UV");
    Reject(a,original,[](Mesh3D& m){m.vertices[wall].x=std::nextafter(m.vertices[wall].x,std::numeric_limits<float>::infinity());},"position mutation");
    Reject(a,original,[](Mesh3D& m){m.vertices[wall].normal^=1u;},"packed normal mutation");
    Reject(a,original,[](Mesh3D& m){m.vertices[wall].color^=1u;},"packed color mutation");
    Reject(a,original,[](Mesh3D& m){std::swap(m.indices[20u*132u],m.indices[20u*132u+1u]);},"index/winding mutation");
    Reject(a,original,[](Mesh3D& m){m.vertices[0].u+=.125f;},"roof UV mutation");
    Reject(a,original,[](Mesh3D& m){m.vertices[76u*96u].v+=.125f;},"equipment UV mutation");
    Reject(a,original,[](Mesh3D& m){m.vertices.pop_back();},"membership mutation");
    Check(!Validate(original,original),"old transverse grain must not satisfy new wall UV contract");
    std::cout<<"SHELTER_BOARD_UV_CPU_PASS checks="<<checks<<" negatives="<<negatives
             <<" walls=56 wall_vertices="<<wallVertices<<" protected_vertices="<<protectedVertices<<" changed_uv_vertices="<<changed
             <<" vertices=7392 indices=10164 triangles=3388 source_u_px=32,80 source_v_along_y_cycles_per_m=.5 static_scale="
             <<Mistwatch::ShelterBoardUv::StaticDecodedScale<<" geometry_changed=false native=false art=false\n";
    return 0;
}catch(const std::exception& error){std::cerr<<"FIRST_DIVERGENCE "<<error.what()<<'\n';return 1;}}
