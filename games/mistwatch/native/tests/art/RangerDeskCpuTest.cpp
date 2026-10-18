#include "MistwatchRangerDesk.h"
#include <algorithm>
#include <array>
#include <cstring>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {
using namespace Aevrynd;
using Part=Mistwatch::RangerDeskPart;
using Kind=Mistwatch::RangerDeskPartKind;
void Require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
bool Point(Vec3 a,Vec3 b){return a.x==b.x&&a.y==b.y&&a.z==b.z;}
bool Solids(const std::vector<BoxCollider3D>& a,const std::vector<BoxCollider3D>& b){
    if(a.size()!=b.size())return false;for(std::size_t i=0;i<a.size();++i)
        if(!Point(a[i].center,b[i].center)||!Point(a[i].extents,b[i].extents))return false;
    return true;
}
bool Raw(const Mesh3D& a,const Mesh3D& b){
    static_assert(sizeof(Vertex3D)==28);
    return a.indices==b.indices&&a.vertices.size()==b.vertices.size()&&
        std::equal(a.vertices.begin(),a.vertices.end(),b.vertices.begin(),[](const Vertex3D& x,const Vertex3D& y){
            return std::memcmp(&x,&y,28)==0;
        });
}
bool Sound(const Mesh3D& mesh){
    if(!mesh.Valid())return false;
    for(const auto& v:mesh.vertices){
        if(!std::isfinite(v.x)||!std::isfinite(v.y)||!std::isfinite(v.z)||!std::isfinite(v.u)||!std::isfinite(v.v)||
            v.x<-.35f||v.x>.35f||v.y<-.5f||v.y>.5f||v.z<-.8f||v.z>.8f)return false;
        const auto n=UnpackNormalSnorm8(v.normal);if(std::abs(Length(Vec3{n.x,n.y,n.z})-1.f)>.02f)return false;
    }
    for(std::size_t i=0;i<mesh.indices.size();i+=3){
        const auto& a=mesh.vertices[mesh.indices[i]],b=mesh.vertices[mesh.indices[i+1]],c=mesh.vertices[mesh.indices[i+2]];
        const auto normal=Cross(Vec3{b.x-a.x,b.y-a.y,b.z-a.z},Vec3{c.x-a.x,c.y-a.y,c.z-a.z});
        const auto n=UnpackNormalSnorm8(a.normal);
        if(Length(normal)<1.e-7f||Dot(normal,{n.x,n.y,n.z})<=0)return false;
        const double determinant=(double(b.u)-a.u)*(double(c.v)-a.v)-(double(b.v)-a.v)*(double(c.u)-a.u);
        if(!std::isfinite(determinant)||determinant<=0)return false;
    }
    return true;
}
struct Hit {float t{2};};
Hit First(Vec3 from,Vec3 to,const Mesh3D& mesh){
    Hit first;const auto d=to-from;
    for(std::size_t i=0;i<mesh.indices.size();i+=3){
        const auto pos=[&](std::size_t index){const auto& v=mesh.vertices[mesh.indices[index]];return Vec3{v.x,v.y,v.z};};
        const auto a=pos(i),b=pos(i+1),c=pos(i+2),e=b-a,f=c-a,h=Cross(d,f);
        const float det=Dot(e,h);if(std::abs(det)<1.e-7f)continue;
        const float inv=1.f/det;const auto s=from-a;
        const float u=inv*Dot(s,h);if(u<0||u>1)continue;const auto q=Cross(s,e);
        const float v=inv*Dot(d,q),t=inv*Dot(f,q);
        if(v>=0&&u+v<=1&&t>.0001f&&t<.9999f&&t<first.t)first={t};
    }return first;
}
bool VisibleSpace(const Mesh3D& mesh){
    // 不是collider測試：射線逐真triangle確認桌下中央空間，完整原proxy必拒絕。
    for(const float y:{-.35f,-.05f,.20f})for(const float offset:{-.12f,0.f,.12f}){
        if(First({-.5f,y,offset},{.5f,y,offset},mesh).t<1)return false;
        if(First({offset,y,-1},{offset,y,1},mesh).t<1)return false;
    }
    return true;
}
bool FootAndTop(const Mesh3D& mesh){
    const auto top=First({0,.65f,0},{0,.35f,0},mesh);
    if(top.t>1||std::abs((.65f-.30f*top.t)-.5f)>1.e-6f)return false;
    for(const float x:{-(.35f-.04f),.35f-.04f})for(const float z:{-(.8f-.04f),.8f-.04f}){
        const auto foot=First({x,-.65f,z},{x,-.35f,z},mesh);
        if(foot.t>1||std::abs((-.65f+.30f*foot.t)+.5f)>1.e-6f)return false;
    }
    return true;
}
bool ExactParts(const Mesh3D& mesh,const std::vector<Part>& parts){
    if(mesh.vertices.size()!=216||mesh.indices.size()!=324||parts.size()!=9)return false;
    const float ix=.35f-.08f,iz=.8f-.08f,under=.5f-.06f;
    // 獨立固定原尺寸與木料配置，不引用作者layout或由candidate.Bounds產生expected。
    const std::array<Part,9> expected{{
        {Kind::Top,0,36,{-.35f,under,-.8f},{.35f,.5f,.8f}},
        {Kind::Leg,36,36,{-.35f,-.5f,-.8f},{-ix,under,-iz}},
        {Kind::Leg,72,36,{-.35f,-.5f,iz},{-ix,under,.8f}},
        {Kind::Leg,108,36,{ix,-.5f,-.8f},{.35f,under,-iz}},
        {Kind::Leg,144,36,{ix,-.5f,iz},{.35f,under,.8f}},
        {Kind::Apron,180,36,{-.35f,under-.14f,-iz},{-.35f+.04f,under,iz}},
        {Kind::Apron,216,36,{.35f-.04f,under-.14f,-iz},{.35f,under,iz}},
        {Kind::Apron,252,36,{-ix,under-.14f,-.8f},{ix,under,-.8f+.04f}},
        {Kind::Apron,288,36,{-ix,under-.14f,.8f-.04f},{ix,under,.8f}}
    }};
    const auto box=Mesh3D::CreateUnitBox();
    for(std::size_t p=0;p<parts.size();++p){
        const auto& part=parts[p];const auto& e=expected[p];
        if(part.kind!=e.kind||part.firstIndex!=e.firstIndex||part.indexCount!=36||!Point(part.low,e.low)||!Point(part.high,e.high))return false;
        const auto base=std::uint32_t(p*24);
        std::array<bool,8> corners{};
        for(std::size_t j=0;j<24;++j){
            const auto& v=mesh.vertices[base+j];const auto& unit=box.vertices[j];
            if(v.x!=(unit.x<0?e.low.x:e.high.x)||v.y!=(unit.y<0?e.low.y:e.high.y)||v.z!=(unit.z<0?e.low.z:e.high.z)||
                v.normal!=unit.normal||v.color!=unit.color)return false;
            const unsigned mask=(v.x==e.high.x?1u:0u)|(v.y==e.high.y?2u:0u)|(v.z==e.high.z?4u:0u);corners[mask]=true;
        }
        if(!std::all_of(corners.begin(),corners.end(),[](bool x){return x;}))return false;
        for(std::size_t j=0;j<36;++j)if(mesh.indices[part.firstIndex+j]!=base+box.indices[j])return false;
    }
    return true;
}
void FillOldProxy(Mesh3D& mesh){
    const auto box=Mesh3D::CreateUnitBox();const auto base=std::uint32_t(mesh.vertices.size());
    for(auto v:box.vertices){v.x*=.7f;v.y*=1.f;v.z*=1.6f;mesh.vertices.push_back(v);}
    for(const auto index:box.indices)mesh.indices.push_back(base+index);
}
}
int main(){try{
    using namespace Aevrynd;
    const auto before=Mistwatch::RangerHutSolids();
    const auto proxy=before.at(2);
    Require(Point(proxy.center,Mistwatch::Session::rangerLog+Vec3{.9f,.5f,0})&&Point(proxy.extents,{.35f,.5f,.8f}),
        "Original desk collision/source identity changed");
    const auto worldBefore=Mistwatch::WorldSolids(true,Mistwatch::FirLayoutSelector::ForestWood);
    Mistwatch::Session session;const auto saved=session.Serialize();
    std::vector<Part> parts;const auto mesh=Mistwatch::CreateRangerDesk(proxy,&parts);
    Require(Raw(mesh,Mistwatch::CreateRangerDesk())&&Sound(mesh)&&ExactParts(mesh,parts),"Actual desk mesh/raw/parts/UV/normal guard failed");
    Require(mesh.indices.size()/3==108,"Fixed desk exceeded108 triangles");
    const auto bounds=mesh.Bounds();
    Require(Point(bounds.center,{0,0,0})&&Point(bounds.extents,proxy.extents),"Original desk footprint/top/sole changed");
    Require(proxy.center.y+bounds.center.y+bounds.extents.y==1&&proxy.center.y+bounds.center.y-bounds.extents.y==0,
        "Original world tabletop1m or feet0 changed");
    const auto book=Mistwatch::Session::rangerLog+Vec3{.9f,1.02f,0};
    Require(book.x==proxy.center.x&&book.z==proxy.center.z&&book.y-.03f*.5f>1,"Existing book position/top clearance changed");
    Require(VisibleSpace(mesh)&&FootAndTop(mesh),"Real tabletop/feet or underneath visibility failed");
    Require(Solids(before,Mistwatch::RangerHutSolids())&&Solids(worldBefore,Mistwatch::WorldSolids(true,Mistwatch::FirLayoutSelector::ForestWood))&&
        session.Serialize()==saved,"Pure desk author changed collision/world/session");
    unsigned negative{};
    {auto bad=mesh;bad.indices[0]=std::uint32_t(bad.vertices.size());Require(!Sound(bad),"Bad index passed");++negative;}
    {auto bad=mesh;bad.vertices[0].x=std::numeric_limits<float>::quiet_NaN();Require(!Sound(bad),"NaN passed");++negative;}
    {auto bad=mesh;bad.vertices[0].x=-.36f;Require(!Sound(bad),"Expanded footprint passed");++negative;}
    {auto bad=mesh;std::swap(bad.indices[1],bad.indices[2]);Require(!Sound(bad),"Reverse winding passed");++negative;}
    {auto bad=mesh;const auto n=UnpackNormalSnorm8(bad.vertices[0].normal);
        bad.vertices[0].normal=PackNormalSnorm8({-n.x,-n.y,-n.z});Require(!Sound(bad),"Reverse normal passed");++negative;}
    {auto bad=mesh;bad.indices[1]=bad.indices[0];Require(!Sound(bad),"Degenerate triangle passed");++negative;}
    {auto bad=mesh;for(unsigned k=0;k<3;++k){auto& v=bad.vertices[bad.indices[k]];v.u=v.v=0;}Require(!Sound(bad),"Degenerate UV passed");++negative;}
    {auto bad=mesh;FillOldProxy(bad);Require(!VisibleSpace(bad),"Original solid proxy accepted as visible furniture");++negative;}
    {auto bad=mesh;bad.indices.erase(bad.indices.begin(),bad.indices.begin()+36);Require(!FootAndTop(bad),"Missing tabletop passed");++negative;}
    {auto bad=mesh;bad.indices.erase(bad.indices.begin()+36,bad.indices.begin()+72);Require(!FootAndTop(bad),"Missing true foot passed");++negative;}
    {auto bad=mesh;for(std::size_t j=24;j<48;++j)bad.vertices[j].y+=.01f;Require(!ExactParts(bad,parts),"Floating leg passed");++negative;}
    {auto bad=parts;bad[1]=bad[2];Require(!ExactParts(mesh,bad),"Duplicate leg membership passed");++negative;}
    unsigned invalid{};
    for(unsigned mutation=0;mutation<4;++mutation){
        auto bad=proxy;
        if(mutation==0)bad.center.x=std::numeric_limits<float>::quiet_NaN();
        else if(mutation==1)bad.extents.y=std::numeric_limits<float>::infinity();
        else if(mutation==2)bad.extents.x=-.35f;
        else bad.extents.z=.81f;
        auto originalParts=parts;bool rejected=false;
        try{(void)Mistwatch::CreateRangerDesk(bad,&originalParts);}catch(const std::invalid_argument&){rejected=true;}
        Require(rejected&&ExactParts(mesh,originalParts),"Invalid proxy or parts transaction accepted");++invalid;
    }
    std::cout<<"RANGER_DESK_CONTRACT vertices=216 triangles=108 parts=9 tabletop=1 legs=4 aprons=4"
        <<" tabletop_world_y=1 feet_world_y=0 visible_void_rays=18 old_collision_exact=1 world_exact=1 source_book_exact=1"
        <<" negative_controls="<<negative<<" invalid_inputs="<<invalid<<" gpu_executed=0\n";
    return 0;
}catch(const std::exception& e){std::cerr<<"FIRST_DIVERGENCE: "<<e.what()<<'\n';return 1;}}
