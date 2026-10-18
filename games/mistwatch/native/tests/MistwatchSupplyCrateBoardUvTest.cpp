#include "MistwatchSupplyCrateBoardUv.h"
#include <iostream>
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>

namespace {
using namespace Aevrynd;
unsigned checks{},negativeCases{};
void Check(bool ok,const char* reason){++checks;if(!ok)throw std::runtime_error(reason);}
bool SameFloat(float a,float b){return std::bit_cast<std::uint32_t>(a)==std::bit_cast<std::uint32_t>(b);}
bool SameVertex(const Vertex3D& a,const Vertex3D& b,bool uv){
    return SameFloat(a.x,b.x)&&SameFloat(a.y,b.y)&&SameFloat(a.z,b.z)&&a.normal==b.normal&&a.color==b.color&&
        (!uv||(SameFloat(a.u,b.u)&&SameFloat(a.v,b.v)));
}
struct Spec {unsigned first{},vertices{},index{},indices{},longAxis{};Vec3 center{},size{};std::array<Vec3,3> axes{};bool bevel{};};
std::array<Spec,34> Specs(){
    // 獨立來源book；不調候選SupplyCrateUvBoards/Face作oracle。
    std::array<Spec,34> result{};
    for(unsigned id=0;id<34;++id){auto& s=result[id];s.bevel=id>=26;
        s.first=id<26?id*24:624+(id-26)*96;s.vertices=s.bevel?96u:24u;
        s.index=id<26?id*36:936+(id-26)*132;s.indices=s.bevel?132u:36u;
        s.axes={{{1,0,0},{0,1,0},{0,0,1}}};
        if(id<16){const float y=.13f+.18f*float(id/4),sign=id%4<2?-1.f:1.f;
            if(id%2==0){s.center={0,y,sign*.346f};s.size={.78f,.171f,.056f};s.longAxis=0;}
            else{s.center={sign*.346f,y,0};s.size={.056f,.171f,.676f};s.longAxis=2;}}
        else if(id<26){s.center={(float((id-16)/2)-2.f)*.156f,id%2==0?.759f:.041f,0};s.size={.15f,.07f,.78f};s.longAxis=2;}
        else if(id<30){s.center={id<28?-.327f:.327f,.4f,id%2==0?-.327f:.327f};s.size={.076f,.66f,.076f};s.longAxis=1;}
        else{const float sign=id<32?-1.f:1.f;
            const Vec3 axis=id%2==0?Normalize(Vec3{.654f,.5f,0}):Normalize(Vec3{0,.5f,.654f});
            s.size={Length(Vec3{.654f,.5f,0}),.060f,.016f};s.longAxis=0;
            if(id%2==0){s.center={0,.4f,sign*.370f};s.axes={{axis,{-axis.y,axis.x,0},{0,0,1}}};}
            else{s.center={sign*.370f,.4f,0};s.axes={{axis,{0,-axis.z,axis.y},{1,0,0}}};}}
    }
    return result;
}
struct D {double x{},y{},z{};};
D operator+(D a,D b){return {a.x+b.x,a.y+b.y,a.z+b.z};}
D operator-(D a,D b){return {a.x-b.x,a.y-b.y,a.z-b.z};}
D operator*(D a,double s){return {a.x*s,a.y*s,a.z*s};}
double DotD(D a,D b){return a.x*b.x+a.y*b.y+a.z*b.z;}
D CrossD(D a,D b){return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
double Norm(D a){return std::sqrt(DotD(a,a));}
D P(const Vertex3D& v){return {v.x,v.y,v.z};}
D V(Vec3 v){return {v.x,v.y,v.z};}
float Size(Vec3 v,unsigned axis){return axis==0?v.x:axis==1?v.y:v.z;}
constexpr double rounding=64.0*double(std::numeric_limits<float>::epsilon());
void Raw(const Mistwatch::SupplyCrateGeometry& candidate,const Mistwatch::SupplyCrateGeometry& original){
    Check(candidate.wood.Valid()&&candidate.metal.Valid(),"invalid_mesh");
    Check(candidate.wood.vertices.size()==original.wood.vertices.size()&&candidate.metal.vertices.size()==original.metal.vertices.size(),"vertex_counts");
    Check(candidate.wood.indices==original.wood.indices&&candidate.metal.indices==original.metal.indices,"exact_indices");
    Check(candidate.wood.indices.size()==1992&&candidate.metal.indices.size()==708,"664_236_triangles");
    for(std::size_t i=0;i<original.metal.vertices.size();++i)Check(SameVertex(candidate.metal.vertices[i],original.metal.vertices[i],true),"exact_metal_words");
    for(std::size_t i=0;i<original.wood.vertices.size();++i)Check(SameVertex(candidate.wood.vertices[i],original.wood.vertices[i],false),"wood_non_uv_words");
}
void Contract(const Mistwatch::SupplyCrateGeometry& candidate,const Mistwatch::SupplyCrateGeometry& original){
    Raw(candidate,original);const auto specs=Specs();
    const double scale=double(Mistwatch::SupplyCrateStaticDecodedScale);
    Check(std::lround(.5f/16.f*32767.f)==1024,"static_15bit_pack_premise");
    Check(scale==double(1024.f*(16.f/32767.f)),"static_decoder_premise");
    unsigned vertices{},indices{},faces{},endFaces{},sideFaces{};
    std::array<unsigned,1392> faceUAxis{};
    for(unsigned id=0;id<34;++id){const auto& s=specs[id];const double center=(double(id%8)*128+64)/1024;
        unsigned first=s.first;
        for(unsigned face=0;face<(s.bevel?26u:6u);++face){const unsigned count=s.bevel&&face>=18?3u:4u;
            // 投影選擇依原幾何真面；變動UV不能改oracle。
            const auto a=P(original.wood.vertices[first]),b=P(original.wood.vertices[first+1]),c=P(original.wood.vertices[first+2]);
            const auto faceN=CrossD(b-a,c-a);Check(Norm(faceN)>0,"original_face_area");
            // 與原float作者同精度的幾何主軸，double僅用於下方獨立Jacobian檢查。
            const Vec3 fa{original.wood.vertices[first].x,original.wood.vertices[first].y,original.wood.vertices[first].z};
            const Vec3 fb{original.wood.vertices[first+1].x,original.wood.vertices[first+1].y,original.wood.vertices[first+1].z};
            const Vec3 fc{original.wood.vertices[first+2].x,original.wood.vertices[first+2].y,original.wood.vertices[first+2].z};
            const auto normal=Cross(fb-fa,fc-fa);unsigned dropped{};
            for(unsigned k=1;k<3;++k)if(std::abs(Dot(normal,s.axes[k]))>std::abs(Dot(normal,s.axes[dropped])))dropped=k;
            const bool cap=dropped==s.longAxis;endFaces+=cap;sideFaces+=!cap;
            const unsigned ua=cap?(s.longAxis+1)%3:s.longAxis,va=3-dropped-ua;
            for(unsigned k=0;k<count;++k){const auto& p=candidate.wood.vertices[first+k];
                faceUAxis[first+k]=ua;
                const double sampleU=double(p.u)*scale,sampleV=double(p.v)*scale;
                Check(std::isfinite(sampleU)&&std::isfinite(sampleV),"finite_sampled_uv");
                const auto local=P(p)-V(s.center);
                const double expectU=.5+DotD(local,V(s.axes[ua]))/double(Size(s.size,ua));
                const double expectV=center+DotD(local,V(s.axes[va]))/double(Size(s.size,va))*(48.0/1024);
                Check(std::abs(sampleU-expectU)<=rounding&&std::abs(sampleV-expectV)<=rounding,"all_board_axis_uv_oracle");
                Check(sampleU>=-rounding&&sampleU<=1+rounding&&sampleV>=center-24.0/1024-rounding&&sampleV<=center+24.0/1024+rounding,"all_board_tile_bounds");
            }
            first+=count;++faces;
        }
        Check(first==s.first+s.vertices,"face_spans_complete");
        for(unsigned offset=s.index;offset<s.index+s.indices;offset+=3){
            std::array<const Vertex3D*,3> p{};
            for(unsigned k=0;k<3;++k){const auto index=candidate.wood.indices[offset+k];
                Check(index>=s.first&&index<s.first+s.vertices,"source_piece_index_ownership");p[k]=&candidate.wood.vertices[index];}
            const double du1=(double(p[1]->u)-p[0]->u)*scale,du2=(double(p[2]->u)-p[0]->u)*scale;
            const double dv1=(double(p[1]->v)-p[0]->v)*scale,dv2=(double(p[2]->v)-p[0]->v)*scale;
            const double jac=du1*dv2-du2*dv1;
            Check(std::isfinite(jac)&&jac!=0,"all_664_uv_jacobians");
            const auto e1=P(*p[1])-P(*p[0]),e2=P(*p[2])-P(*p[0]);
            const double aa=DotD(e1,e1),ab=DotD(e1,e2),bb=DotD(e2,e2),gram=aa*bb-ab*ab;
            Check(std::isfinite(gram)&&gram>0,"geometry_gram");
            const auto grad=e1*((du1*bb-du2*ab)/gram)+e2*((du2*aa-du1*ab)/gram);
            const auto n=CrossD(e1,e2);
            // corner/bevel的face basis已逐頂點oracle檢查；此處全側面獨立核U-gradient沿真長軸投影。
            if(faceUAxis[candidate.wood.indices[offset]]==s.longAxis){const auto longAxis=V(s.axes[s.longAxis]);
                const auto projected=longAxis-n*(DotD(longAxis,n)/DotD(n,n));
                Check(Norm(grad)>0&&Norm(projected)>0,"long_axis_gradient_nonzero");
                Check(DotD(grad,projected)/(Norm(grad)*Norm(projected))>=1-rounding,"all_long_axis_alignment");}
        }
        vertices+=s.vertices;indices+=s.indices;
    }
    Check(vertices==1392&&indices==1992&&faces==364&&endFaces>0&&sideFaces>0,"34_piece_complete_coverage");
}
template<class F>void Reject(F mutate,const Mistwatch::SupplyCrateGeometry& candidate,const Mistwatch::SupplyCrateGeometry& original){
    auto bad=candidate;mutate(bad);bool rejected{};
    try{Contract(bad,original);}catch(const std::runtime_error&){rejected=true;}
    Check(rejected,"negative_control_was_accepted");++negativeCases;
}
void Negatives(const Mistwatch::SupplyCrateGeometry& candidate,const Mistwatch::SupplyCrateGeometry& original){
    const float scale=Mistwatch::SupplyCrateStaticDecodedScale;
    for(unsigned id=0;id<34;++id){const auto s=Specs()[id];const float center=(float(id%8)*128.f+64.f)/1024.f;
        Reject([&](auto& bad){for(unsigned i=s.first;i<s.first+s.vertices;++i){auto& v=bad.wood.vertices[i];
            const float u=v.u*scale,t=(v.v*scale-center)/(48.f/1024.f)+.5f;
            v.u=t/scale;v.v=(center+(.5f-u)*(48.f/1024.f))/scale;}},candidate,original);
        Reject([&](auto& bad){for(unsigned i=s.first;i<s.first+s.vertices;++i){bad.wood.vertices[i].u=0;bad.wood.vertices[i].v=0;}},candidate,original);
        Reject([&](auto& bad){auto& x=bad.wood.vertices[s.first].x;x=std::nextafter(x,std::numeric_limits<float>::infinity());},candidate,original);
        Reject([&](auto& bad){std::swap(bad.wood.indices[s.index+1],bad.wood.indices[s.index+2]);},candidate,original);
    }
    for(std::size_t i=0;i<candidate.metal.vertices.size();++i)
        Reject([&](auto& bad){bad.metal.vertices[i].color^=1u;},candidate,original);
    for(std::size_t i=0;i<candidate.metal.indices.size();++i)
        Reject([&](auto& bad){bad.metal.indices[i]=(bad.metal.indices[i]+1u)%unsigned(bad.metal.vertices.size());},candidate,original);
}
}
int main(){try{
    const auto original=Mistwatch::CreateSupplyCrate(),candidate=Mistwatch::CreateSupplyCrateBoardUv(),repeat=Mistwatch::CreateSupplyCrateBoardUv();
    Contract(candidate,original);Contract(repeat,original);
    Check(candidate.wood.indices==repeat.wood.indices&&candidate.metal.indices==repeat.metal.indices,"repeat_indices");
    for(std::size_t i=0;i<candidate.wood.vertices.size();++i)Check(SameVertex(candidate.wood.vertices[i],repeat.wood.vertices[i],true),"repeat_wood_words");
    for(std::size_t i=0;i<candidate.metal.vertices.size();++i)Check(SameVertex(candidate.metal.vertices[i],repeat.metal.vertices[i],true),"repeat_metal_words");
    Negatives(candidate,original);
    std::cout<<"CRATE_BOARD_UV_CPU_PASS checks="<<checks<<" negatives="<<negativeCases
        <<" pieces=34 wood_triangles=664 metal_triangles=236 uv_only=1 static_packed_scale=1024/32767*16 native_qualified=0\n";
    return 0;
}catch(const std::exception& e){std::cerr<<"FIRST_DIVERGENCE: "<<e.what()<<" checks="<<checks<<'\n';return 1;}}
