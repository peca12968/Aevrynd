#include "MistwatchWorkshopWoodUv.h"
#include "fixtures/workshop-wood-uv/FrozenWholeMemberUvM2926.h"
#include "MistwatchWorkshopRoofCollision.h"
#include "MistwatchWorkshopRoofCamera.h"
#include <iostream>
#include <iomanip>
#include <set>
using namespace Aevrynd;
namespace W=Mistwatch::WorkshopWoodMetric;
namespace R=Mistwatch::WorkshopRoof;
namespace {
unsigned checks{},negatives{};
void Need(bool pass,const char* why){++checks;if(!pass)throw std::runtime_error(why);}
using V=std::array<double,3>;
V Pos(const Vertex3D& p){return{p.x,p.y,p.z};}
V Sub(V a,V b){return{a[0]-b[0],a[1]-b[1],a[2]-b[2]};}
V Lin(V a,double x,V b,double y){return{a[0]*x+b[0]*y,a[1]*x+b[1]*y,a[2]*x+b[2]*y};}
double Dot(V a,V b){return a[0]*b[0]+a[1]*b[1]+a[2]*b[2];}
struct Derivative {V u{},v{};double det{};};
Derivative Measure(const Mesh3D& m,std::size_t i){const auto& a=m.vertices[m.indices[i]];const auto& b=m.vertices[m.indices[i+1]];const auto& c=m.vertices[m.indices[i+2]];
    // 獨立以公開 static material 編碼公式回解，不用候選常數或候選面寬／基底。
    constexpr double decoded=32768./32767.;const auto e=Sub(Pos(b),Pos(a)),f=Sub(Pos(c),Pos(a));
    const double u=(double(b.u)-a.u)*decoded,v=(double(b.v)-a.v)*decoded,s=(double(c.u)-a.u)*decoded,t=(double(c.v)-a.v)*decoded,d=u*t-s*v;
    if(!std::isfinite(d)||d==0)return{{},{},0};return{Lin(e,t/d,f,-v/d),Lin(e,-s/d,f,u/d),d};
}
struct Metrics {unsigned bad{};double error{},orthogonal{};};
Metrics CheckMetric(const Mesh3D& mesh,double period){Metrics m;
    for(std::size_t i=0;i<mesh.indices.size();i+=3){const auto d=Measure(mesh,i);const double u=std::sqrt(Dot(d.u,d.u)),v=std::sqrt(Dot(d.v,d.v));
        const double err=(std::max)(std::abs(u/period-1),std::abs(v/period-1));const double orth=std::abs(Dot(d.u,d.v))/(u*v);
        if(d.det==0||!std::isfinite(err)||!std::isfinite(orth)||err>.003||orth>.003)++m.bad;
        if(std::isfinite(err))m.error=(std::max)(m.error,err);if(std::isfinite(orth))m.orthogonal=(std::max)(m.orthogonal,orth);
    }return m;
}
bool SameFloat(float a,float b){return std::bit_cast<std::uint32_t>(a)==std::bit_cast<std::uint32_t>(b);}
}
int main()try{
    std::vector<R::Part> parts;const auto source=R::Create(&parts);const auto result=W::Create(source,parts);const auto& mesh=result.mesh;
    Need(source.vertices.size()==17664&&source.indices.size()==24372&&parts.size()==75,"canonical membership");
    Need(Mistwatch::WorkshopFrame::Fingerprint(source)==358593194902208623ull,"sealed canonical unchanged");
    Need(W::Validate(source,parts,result)&&W::SameMesh(mesh,W::Create(source,parts).mesh),"valid deterministic candidate");
    Need(mesh.indices==source.indices,"topology exact");
    for(std::size_t i=0;i<17664;++i){const auto& a=source.vertices[i];const auto& b=mesh.vertices[i];Need(SameFloat(a.x,b.x)&&SameFloat(a.y,b.y)&&SameFloat(a.z,b.z)&&a.color==b.color&&a.normal==b.normal,"independent geometry bits");}
    const auto metric=CheckMetric(mesh,5.2);std::cout<<std::setprecision(12)<<"metric bad="<<metric.bad<<" max_relative_error="<<metric.error<<" max_nonorthogonality="<<metric.orthogonal<<'\n';Need(metric.bad==0,"per triangle world-distance metric");
    std::size_t vertex{},index{},faceIndex{};std::set<unsigned> phases;
    constexpr std::array<std::array<double,2>,4> strips{{{132./1024,216./1024},{360./1024,436./1024},{694./1024,758./1024},{932./1024,1006./1024}}};
    for(std::size_t member=0;member<205;++member){const unsigned faces=member<177?26:6;
        for(unsigned face=0;face<faces;++face){const auto& r=result.faces.at(faceIndex++);const std::size_t vc=member<177&&face>=18?3:4,ic=vc==3?3:6;
            Need(r.member==member&&r.firstVertex==vertex&&r.firstIndex==index&&r.vertexCount==vc&&r.indexCount==ic,"independent face membership");Need(r.band<4,"band");
            phases.insert(unsigned(std::lround(r.phase*4096)));
            for(std::size_t j=vertex;j<vertex+vc;++j){const double u=double(mesh.vertices[j].u)*32768./32767.;Need(u>=strips[r.band][0]+1.9/1024&&u<=strips[r.band][1]-1.9/1024,"real sampled uv safe strip");}
            for(std::size_t j=index;j<index+ic;++j)Need(mesh.indices[j]>=vertex&&mesh.indices[j]<vertex+vc,"face index ownership");
            for(std::size_t j=index;j<index+ic;j+=3){const auto a=Measure(source,j),b=Measure(mesh,j);Need(a.det*b.det>0,"no new mirror");const double cosine=Dot(a.v,b.v)/std::sqrt(Dot(a.v,a.v)*Dot(b.v,b.v));Need(cosine>.999,"grain direction preserved");}
            vertex+=vc;index+=ic;
        }
    }
    Need(vertex==17664&&index==24372&&faceIndex==4770&&phases.size()==202,"complete faces and phase counts");
    unsigned literalGrainFaces{};
    // 主要面的實際木紋方向用獨立作者尺寸與世界軸檢查，不只拿舊 UV 當 expected。
    for(const auto& face:result.faces){const auto d=Measure(mesh,face.firstIndex);const auto normal=UnpackNormalSnorm8(mesh.vertices[face.firstVertex].normal);V expected{};bool check{};
        if(face.member<76){if(face.member<20){check=std::abs(normal.y)>.99f;expected={0,0,1};}else{check=(face.member<36?std::abs(normal.x):std::abs(normal.z))>.99f;expected={0,1,0};}}
        else if(face.member<130){const auto ordinal=face.member-76,part=1+ordinal/18,local=ordinal%9;const V along=part==1?V{0,0,1}:V{1,0,0};check=(part==1?std::abs(normal.x):std::abs(normal.z))>.99f;
            if(local<3)expected={0,1,0};else if(local<7)expected=along;else{const double sign=local==7?-1.:1.,post=part==1?1.7:2.38;expected=Lin(along,sign*(post-.37),V{0,1,0},1.88);}}
        else if(face.member<170){const double side=face.member<150?-1.:1.;expected={0,.84,-side*1.94};const auto localVertex=face.firstVertex-face.member*96;check=localVertex==16||localVertex==20;}
        else if(face.member>=177){expected={0,1,0};check=std::abs(normal.x)>.99f;}
        if(check){const auto alignment=Dot(d.v,expected)/std::sqrt(Dot(d.v,d.v)*Dot(expected,expected));if(alignment<=.999)std::cout<<"grain_fail member="<<face.member<<" first="<<face.firstVertex<<" normal="<<normal.x<<","<<normal.y<<","<<normal.z<<" cosine="<<alignment<<'\n';Need(alignment>.999,"independent literal primary-face grain");++literalGrainFaces;}
    }
    Need(literalGrainFaces>=380,"literal grain coverage");
    // 不依候選 range 的外端板量測：真頂點包圍範圍必須反映只扣半接縫。
    for(unsigned m:{0u,19u,20u,35u,36u,55u,56u,75u}){double lo=1000,hi=-1000;for(unsigned v=0;v<96;++v){const auto& p=mesh.vertices[m*96+v];const double x=m>=20&&m<36?p.z:p.x;lo=(std::min)(lo,x);hi=(std::max)(hi,x);}Need(std::abs((hi-lo)-.2465)<.00002,"eight half-seam end boards actual width");}
    const auto old=Mistwatch::WorkshopWoodUv::Create(source,parts);const auto oldMetric=CheckMetric(old.mesh,3.2);
    std::cout<<"m2926_rejected_triangles="<<oldMetric.bad<<" old_max_relative_error="<<oldMetric.error<<'\n';Need(oldMetric.bad>0,"independent metric rejects old m2926");++negatives;
    const auto rejectMetric=[&](auto mutate){auto bad=mesh;mutate(bad);Need(CheckMetric(bad,5.2).bad>0,"metric mutant accepted");++negatives;};
    const auto scaleFace=[&](Mesh3D& m,std::size_t member,unsigned face,bool u,float factor){const auto first=result.faces[member*26+face].firstVertex;const auto count=result.faces[member*26+face].vertexCount;const float origin=u?m.vertices[first].u:m.vertices[first].v;for(std::size_t i=first;i<first+count;++i){auto& value=u?m.vertices[i].u:m.vertices[i].v;value=origin+(value-origin)*factor;}};
    rejectMetric([&](auto& m){scaleFace(m,0,2,true,.243f/.2465f);});
    rejectMetric([&](auto& m){scaleFace(m,76,0,true,10);});
    rejectMetric([&](auto& m){scaleFace(m,20,2,false,.05f/.16f);});
    rejectMetric([&](auto& m){scaleFace(m,0,2,true,.7011218f);});
    rejectMetric([](auto& m){for(auto& v:m.vertices)v.v=0;});
    const auto reject=[&](auto mutate){auto bad=result;mutate(bad);Need(!W::Validate(source,parts,bad),"candidate mutation accepted");++negatives;};
    reject([](auto& c){c.mesh.vertices[0].x+=.01f;});reject([](auto& c){c.mesh.vertices[0].normal^=1;});reject([](auto& c){c.mesh.vertices[0].color^=1;});
    reject([](auto& c){std::swap(c.mesh.indices[0],c.mesh.indices[1]);});reject([](auto& c){c.mesh.vertices[0].u=std::numeric_limits<float>::quiet_NaN();});
    reject([](auto& c){for(auto& v:c.mesh.vertices)v.u=-v.u;});reject([](auto& c){c.mesh.vertices[0].u=1;});reject([](auto& c){c.faces.pop_back();});reject([](auto& c){c.faces[1].firstVertex=0;});
    reject([](auto& c){c.roofParts[0].kind=R::PartKind::Ridge;});reject([](auto& c){c.faces[0].width*=.5;});
    bool incompatible{};try{(void)W::Create(source,parts,3.2);}catch(const std::runtime_error& e){incompatible=std::string(e.what()).find("METRIC_SAFE_BAND_INCOMPATIBLE")!=std::string::npos;std::cout<<"period_3p2_rejection="<<e.what()<<'\n';}Need(incompatible,"3.2m must reject incompatible safe band NOT clip");++negatives;
    for(double period:{0.,-1.,std::numeric_limits<double>::quiet_NaN()}){bool rejected{};try{(void)W::Create(source,parts,period);}catch(const std::invalid_argument&){rejected=true;}Need(rejected,"invalid period");++negatives;}
    auto wrong=source;wrong.vertices[0].u+=.001f;bool rejected{};try{(void)W::Create(wrong,parts);}catch(const std::invalid_argument&){rejected=true;}Need(rejected,"wrong canonical");++negatives;
    std::vector<Mistwatch::WorkshopRoofCollision::Member> hulls;std::vector<BoxCollider3D> camera;
    Need(Mistwatch::WorkshopRoofCollision::Build(source,parts,hulls)&&hulls.size()==75,"old body gate");Need(Mistwatch::BuildWorkshopRoofCamera(source,parts,camera)&&camera.size()==954,"old camera gate");
    Need(!Mistwatch::WorkshopRoofCollision::Build(mesh,parts,hulls)&&!Mistwatch::BuildWorkshopRoofCamera(mesh,parts,camera),"new UV cannot bypass canonical gates");
    std::cout<<"CPU_PASS checks="<<checks<<" negatives="<<negatives<<" faces=4770 vertices=17664 period_m=5.2 literal_grain_faces="<<literalGrainFaces<<"\nsource_fingerprint="<<Mistwatch::WorkshopFrame::Fingerprint(source)<<"\ncandidate_fingerprint="<<Mistwatch::WorkshopFrame::Fingerprint(mesh)<<"\nnative=0 visual_accepted=0 installed_in_game=0\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<" checks="<<checks<<'\n';return 1;}
