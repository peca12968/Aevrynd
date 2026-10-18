#include "MistwatchWorkshopFrame.h"
#include <iostream>
#include <sstream>
#include <iomanip>

using namespace Aevrynd;
using namespace Mistwatch;
namespace W=::Mistwatch::WorkshopFrame;
namespace {
constexpr std::array<BoxCollider3D,4> Source{{
    {{-7,2.7f,59},{2.5f,.15f,2}},{{-9.4f,1.3f,59},{.1f,1.3f,2}},
    {{-7,1.3f,57.1f},{2.5f,1.3f,.1f}},{{-7,1.3f,60.9f},{2.5f,1.3f,.1f}}}};
bool Bits(float a,float b){return std::bit_cast<std::uint32_t>(a)==std::bit_cast<std::uint32_t>(b);}
bool Same(Vec3 a,Vec3 b){return Bits(a.x,b.x)&&Bits(a.y,b.y)&&Bits(a.z,b.z);}
bool SameVertex(const Vertex3D& a,const Vertex3D& b){return Same({a.x,a.y,a.z},{b.x,b.y,b.z})&&Bits(a.u,b.u)&&Bits(a.v,b.v)&&a.normal==b.normal&&a.color==b.color;}
void Need(bool pass,const char* why){if(!pass)throw std::runtime_error(why);}
struct Bounds{Vec3 low{1000,1000,1000},high{-1000,-1000,-1000};};
Bounds Measure(const Mesh3D& mesh,std::size_t first,std::size_t count){Bounds b;
    for(std::size_t i=first;i<first+count;++i){const auto& v=mesh.vertices[i];
        b.low={(std::min)(b.low.x,v.x),(std::min)(b.low.y,v.y),(std::min)(b.low.z,v.z)};
        b.high={(std::max)(b.high.x,v.x),(std::max)(b.high.y,v.y),(std::max)(b.high.z,v.z)};}
    return b;
}
bool Intersects(Bounds a,Bounds b){return a.low.x<b.high.x&&a.high.x>b.low.x&&a.low.y<b.high.y&&a.high.y>b.low.y&&a.low.z<b.high.z&&a.high.z>b.low.z;}
// 驗證使用獨立四盒常數與固定輸出分組，不依賴候選回報bounds或法線函式。
bool Validate(const Mesh3D& mesh,std::string* reason=nullptr){
    const auto fail=[&](const char* gate,unsigned part,std::size_t index){if(reason)*reason=std::string(gate)+" part="+std::to_string(part)+" index="+std::to_string(index);return false;};
    if(!mesh.Valid()||mesh.vertices.size()!=12480||mesh.indices.size()!=17160)return fail("membership",0,0);
    std::size_t first{},indexFirst{};std::array<Bounds,54> frames{};
    for(unsigned group=0;group<58;++group){const bool frame=group>=4;
        const unsigned ordinal=frame?group-4:0,part=frame?1+ordinal/18:group;
        const unsigned boards=frame?1:std::array<unsigned,4>{20,16,20,20}[part];
        const std::size_t count=boards*96u,indexCount=boards*132u;
        const auto box=Source[part];const auto low=box.center-box.extents,high=box.center+box.extents;
        const auto bounds=Measure(mesh,first,count);if(frame)frames[ordinal]=bounds;
        for(std::size_t i=first;i<first+count;++i){const auto& v=mesh.vertices[i];
            if(!std::isfinite(v.x)||!std::isfinite(v.y)||!std::isfinite(v.z)||!std::isfinite(v.u)||!std::isfinite(v.v))return fail("finite",part,i);
            if(v.x<low.x||v.x>high.x||v.y<low.y||v.y>high.y||v.z<low.z||v.z>high.z)return fail("strict_bounds",part,i);
            const auto n=UnpackNormalSnorm8(v.normal);const auto length=Length({n.x,n.y,n.z});
            if(!std::isfinite(length)||length<.98f||length>1.02f)return fail("normal_length",part,i);
        }
        unsigned frontTriangles{};
        for(std::size_t i=indexFirst;i<indexFirst+indexCount;i+=3){
            for(unsigned k=0;k<3;++k)if(mesh.indices[i+k]<first||mesh.indices[i+k]>=first+count)return fail("index_owner",part,i+k);
            const auto& a=mesh.vertices[mesh.indices[i]];const auto& b=mesh.vertices[mesh.indices[i+1]];const auto& c=mesh.vertices[mesh.indices[i+2]];
            const double ux=double(b.x)-a.x,uy=double(b.y)-a.y,uz=double(b.z)-a.z;
            const double vx=double(c.x)-a.x,vy=double(c.y)-a.y,vz=double(c.z)-a.z;
            const double nx=uy*vz-uz*vy,ny=uz*vx-ux*vz,nz=ux*vy-uy*vx,area=std::sqrt(nx*nx+ny*ny+nz*nz);
            if(!std::isfinite(area)||area<1.e-10)return fail("triangle_area",part,i/3);
            for(const auto* v:{&a,&b,&c}){const auto n=UnpackNormalSnorm8(v->normal);
                if((nx*n.x+ny*n.y+nz*n.z)/area<.98)return fail("winding_normal",part,i/3);}
            const double du1=double(b.u)-a.u,dv1=double(b.v)-a.v,du2=double(c.u)-a.u,dv2=double(c.v)-a.v;
            const double jac=du1*dv2-du2*dv1;if(!std::isfinite(jac)||std::abs(jac)<1.e-15)return fail("uv_area",part,i/3);
            if(frame){const unsigned member=ordinal%9;const Vec3 along=part==1?Vec3{0,0,1}:Vec3{1,0,0};
                const double outward=part==1?std::abs(nx/area):std::abs(nz/area);
                if(outward>.99){++frontTriangles;
                    Vec3 grain=member<3?Vec3{0,1,0}:along;
                    if(member>=7){const float postCentre=part==1?1.7f:2.38f,sign=member==7?-1.f:1.f;
                        grain=Normalize(along*(sign*(postCentre-.37f))+Vec3{0,1.88f,0});}
                    const double bx=(vx*du1-ux*du2)/jac,by=(vy*du1-uy*du2)/jac,bz=(vz*du1-uz*du2)/jac;
                    const double length=std::sqrt(bx*bx+by*by+bz*bz);
                    if(!std::isfinite(length)||length<1.e-8||(bx*grain.x+by*grain.y+bz*grain.z)/length<.99)return fail("long_axis_wood_grain",part,i/3);
                }
            }
        }
        if(frame&&frontTriangles!=4)return fail("complete_front_back_faces",part,group);
        first+=count;indexFirst+=indexCount;
    }
    // 所有牆面的框架都不得交疊，包括西牆與南北牆的轉角；木板則需有實際退縮深度。
    for(unsigned i=0;i<54;++i){for(unsigned j=i+1;j<54;++j)if(Intersects(frames[i],frames[j]))return fail("intersecting_frame_members",1+i/18,j);
        const unsigned part=1+i/18;const auto b=frames[i];const auto centre=Source[part].center;
        const float closest=part==1?(std::min)(std::abs(b.low.x-centre.x),std::abs(b.high.x-centre.x)):
            (std::min)(std::abs(b.low.z-centre.z),std::abs(b.high.z-centre.z));
        if(closest<=.0801f)return fail("frame_not_above_inset_boards",part,i);
    }
    return first==12480&&indexFirst==17160;
}
}
int main()try{
    const auto solids=WorkshopSolids();Need(solids.size()==4,"Four original colliders");
    for(unsigned i=0;i<4;++i)Need(Same(solids[i].center,Source[i].center)&&Same(solids[i].extents,Source[i].extents),"Literal original collider dimensions");
    const auto cabinet=FuseCaseSolid();Need(Same(cabinet.center,{-7.9f,.45f,59})&&Same(cabinet.extents,{.35f,.45f,.4f}),"Original fuse cabinet");
    const auto old=WorkshopVisual::Create(),mesh=W::Create();const auto originalHash=W::Fingerprint(old);std::string reason;
    if(!Validate(mesh,&reason))throw std::runtime_error(reason);
    Need(W::FrameMembers==54&&W::Vertices==12480&&W::Indices==17160,"Expected visible frame membership");
    for(std::size_t i=0;i<1920;++i)Need(SameVertex(old.vertices[i],mesh.vertices[i]),"Roof vertices bit exact");
    for(std::size_t i=0;i<old.indices.size();++i)Need(old.indices[i]==mesh.indices[i],"Original board topology and sequence unchanged");
    std::size_t first=1920;
    for(unsigned part=1;part<4;++part){const unsigned count=std::array<unsigned,4>{20,16,20,20}[part]*96;
        for(std::size_t i=first;i<first+count;++i){const auto& a=old.vertices[i];const auto& b=mesh.vertices[i];
            if(part==1)Need(b.x==solids[part].center.x+(a.x-solids[part].center.x)*.8f&&Bits(a.y,b.y)&&Bits(a.z,b.z),"West thickness-only inset");
            else Need(b.z==solids[part].center.z+(a.z-solids[part].center.z)*.8f&&Bits(a.y,b.y)&&Bits(a.x,b.x),"North south thickness-only inset");
            Need(Bits(a.u,b.u)&&Bits(a.v,b.v)&&a.color==b.color,"Original wall weathered UV and color preserved");}
        first+=count;}
    const auto ranges=W::PartRanges();first=0;std::size_t indexFirst{};
    for(unsigned g=0;g<58;++g){const auto& range=ranges[g];Need(range.firstVertex==first&&range.firstIndex==indexFirst&&range.frame==(g>=4)&&
        range.part==(g<4?g:1+(g-4)/18),"Complete deterministic part metadata");first+=range.vertexCount;indexFirst+=range.indexCount;}
    Need(first==mesh.vertices.size()&&indexFirst==mesh.indices.size(),"Metadata accounts for every vertex and index");
    // 原東向入口身體範圍與工具箱互動路線未增加碰撞或可見阻擋。
    for(unsigned step=0;step<=40;++step){const Vec3 centre{-3.f-.1f*float(step),.9f,59},radius{.25f,.9f,.25f};const Bounds body{centre-radius,centre+radius};
        for(const auto& range:ranges)Need(!Intersects(body,Measure(mesh,range.firstVertex,range.vertexCount)),"East standing entry remains unobstructed");}
    const auto repeat=W::Create();Need(repeat.indices==mesh.indices&&repeat.vertices.size()==mesh.vertices.size(),"Deterministic topology");
    for(std::size_t i=0;i<mesh.vertices.size();++i)Need(SameVertex(mesh.vertices[i],repeat.vertices[i]),"Bit exact repeat geometry");
    Need(W::Fingerprint(mesh)==W::Fingerprint(repeat)&&W::Fingerprint(WorkshopVisual::Create())==originalHash,"Determinism and unchanged old generator");
    const auto after=WorkshopSolids();for(unsigned i=0;i<4;++i)Need(Same(after[i].center,solids[i].center)&&Same(after[i].extents,solids[i].extents),"Colliders unchanged after generation");
    const auto cabinetAfter=FuseCaseSolid();Need(Same(cabinetAfter.center,cabinet.center)&&Same(cabinetAfter.extents,cabinet.extents),"Cabinet unchanged after generation");
    unsigned negatives{};const auto reject=[&](const Mesh3D& broken){Need(!Validate(broken),"Mutated geometry accepted");++negatives;};
    auto bad=mesh;bad.vertices[7296].x=-100;reject(bad);
    bad=mesh;std::swap(bad.indices[10032],bad.indices[10033]);reject(bad);
    bad=mesh;bad.indices[10033]=bad.indices[10032];reject(bad);
    bad=mesh;bad.vertices[7296].normal=0;reject(bad);
    bad=mesh;for(std::size_t i=7296;i<7392;++i){bad.vertices[i].u=0;bad.vertices[i].v=0;}reject(bad);
    bad=mesh;for(std::size_t i=7296;i<7392;++i)std::swap(bad.vertices[i].u,bad.vertices[i].v);reject(bad);
    bad=mesh;bad.indices.pop_back();reject(bad);
    std::cout<<"WORKSHOP_FRAME_CPU_PASS vertices="<<mesh.vertices.size()<<" indices="<<mesh.indices.size()
        <<" frame_members="<<W::FrameMembers<<" fingerprint="<<W::Fingerprint(mesh)<<" uv_fingerprint="<<WorkshopVisual::Fingerprint(mesh)
        <<" roof_bit_exact=1 colliders_unchanged=1 cabinet_unchanged=1 framed_faces=6 negative_controls="<<negatives<<" native=0 art=0\n";
    return 0;
}catch(const std::exception& e){std::cerr<<"WORKSHOP_FRAME_FIRST_DIVERGENCE "<<e.what()<<'\n';return 1;}
