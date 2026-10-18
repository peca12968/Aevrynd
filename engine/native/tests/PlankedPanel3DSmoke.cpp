#include <Aevrynd/PlankedPanel3D.h>
#include <bit>
#include <cstring>
#include <iostream>
#include <limits>
#include <map>
#include <stdexcept>

namespace {
using namespace Aevrynd;
unsigned checks{},cases{},negatives{};
void Require(bool condition,const char* message){++checks;if(!condition)throw std::runtime_error(message);}
using Point=std::array<double,3>;
Point Position(const Vertex3D& p){return {p.x,p.y,p.z};}
Point Sub(Point a,Point b){return {a[0]-b[0],a[1]-b[1],a[2]-b[2]};}
Point CrossD(Point a,Point b){return {a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],a[0]*b[1]-a[1]*b[0]};}
double DotD(Point a,Point b){return a[0]*b[0]+a[1]*b[1]+a[2]*b[2];}
void CheckPanel(Vec3 center,Vec3 size,unsigned axis,unsigned count,float seam,float bevel,unsigned phase){
    const auto a=CreatePlankedPanel3D(center,size,axis,count,seam,bevel,phase);
    const auto b=CreatePlankedPanel3D(center,size,axis,count,seam,bevel,phase);
    Require(a&&b,"valid finite panel");
    Require(a->vertices.size()==count*96u&&a->indices.size()==count*132u,"finite board topology");
    static_assert(sizeof(Vertex3D)==28);
    Require(a->indices==b->indices&&std::memcmp(a->vertices.data(),b->vertices.data(),a->vertices.size()*sizeof(Vertex3D))==0,"all geometry bytes deterministic");
    const auto lo=center-size*.5f,hi=center+size*.5f;
    for(unsigned board=0;board<count;++board){
        std::map<std::array<std::uint32_t,3>,unsigned> welded;std::array<unsigned,96> ids{};
        const auto base=std::size_t(board)*96;
        for(unsigned i=0;i<96;++i){
            const auto& v=a->vertices[base+i];
            Require(std::isfinite(v.x)&&std::isfinite(v.y)&&std::isfinite(v.z)&&std::isfinite(v.u)&&std::isfinite(v.v),"finite position and UV");
            Require(v.x>=lo.x&&v.x<=hi.x&&v.y>=lo.y&&v.y<=hi.y&&v.z>=lo.z&&v.z<=hi.z,"exact original bounds without tolerance");
            const auto bits=[](float x){return x==0?0u:std::bit_cast<std::uint32_t>(x);};
            const auto [found,inserted]=welded.emplace(std::array<std::uint32_t,3>{bits(v.x),bits(v.y),bits(v.z)},static_cast<unsigned>(welded.size()));
            (void)inserted;ids[i]=found->second;
        }
        Require(welded.size()==24,"independent24 chamfer corners per board");
        std::map<std::pair<unsigned,unsigned>,std::pair<unsigned,int>> edges;
        for(std::size_t i=std::size_t(board)*132;i<std::size_t(board+1)*132;i+=3){
            const auto ia=a->indices[i],ib=a->indices[i+1],ic=a->indices[i+2];
            Require(ia>=base&&ia<base+96&&ib>=base&&ib<base+96&&ic>=base&&ic<base+96,"no cross-board indices");
            const auto& x=a->vertices[ia];const auto& y=a->vertices[ib];const auto& z=a->vertices[ic];
            const auto area=CrossD(Sub(Position(y),Position(x)),Sub(Position(z),Position(x)));
            const auto length=std::sqrt(DotD(area,area));Require(std::isfinite(length)&&length>0,"true nondegenerate face");
            for(const auto index:{ia,ib,ic}){const auto n=UnpackNormalSnorm8(a->vertices[index].normal);Require(DotD(area,{n.x,n.y,n.z})/length>.99,"packed normal follows actual face");}
            const double uv=(double(y.u)-x.u)*(double(z.v)-x.v)-(double(z.u)-x.u)*(double(y.v)-x.v);
            Require(std::isfinite(uv)&&uv!=0,"actual nonzero UV determinant");
            for(const auto edge:std::array<std::pair<unsigned,unsigned>,3>{{{ids[ia-base],ids[ib-base]},{ids[ib-base],ids[ic-base]},{ids[ic-base],ids[ia-base]}}}){
                Require(edge.first!=edge.second,"no welded zero edge");const auto key=std::minmax(edge.first,edge.second);
                auto& value=edges[{key.first,key.second}];++value.first;value.second+=edge.first<edge.second?1:-1;
            }
        }
        Require(edges.size()==66,"closed-board edge count");
        for(const auto& [edge,value]:edges){(void)edge;Require(value.first==2&&value.second==0,"closed oriented manifold per board");}
    }
    ++cases;
}
void Refused(Vec3 center,Vec3 size,unsigned axis,unsigned count,float seam,float bevel){
    Require(!CreatePlankedPanel3D(center,size,axis,count,seam,bevel),"invalid input cannot return partial panel");++negatives;
}
}
int main(){try{
    for(unsigned axis=0;axis<3;++axis)for(unsigned count:{1u,4u,20u,128u})
        CheckPanel({-24.125f,2.75f,58.125f},{5,4,3},axis,count,.007f,.004f,13);
    CheckPanel({-22,1.55f,55},{5,.2f,4},0,20,.007f,.004f,0);
    CheckPanel({-24.4f,.7f,55},{.2f,1.4f,4},2,16,.007f,.004f,3);
    CheckPanel({-22,.7f,53.1f},{5,1.4f,.2f},0,20,.007f,.004f,6);
    CheckPanel({},{2,2,2},1,7,0,.004f,0);
    const float nan=std::numeric_limits<float>::quiet_NaN(),inf=std::numeric_limits<float>::infinity();
    Refused({}, {1,1,1},3,4,.01f,.004f);Refused({}, {1,1,1},0,0,.01f,.004f);
    Refused({}, {1,1,1},0,129,.001f,.0001f);Refused({}, {1,1,1},0,4,.25f,.004f);
    Refused({}, {1,1,1},0,4,.01f,.13f);Refused({}, {1,1,1},0,4,-.01f,.004f);
    Refused({nan,0,0},{1,1,1},0,4,.01f,.004f);Refused({inf,0,0},{1,1,1},0,4,.01f,.004f);
    Refused({}, {0,1,1},0,4,.01f,.004f);Refused({}, {-1,1,1},0,4,.01f,.004f);
    Refused({}, {1,nan,1},0,4,.01f,.004f);Refused({}, {1,1,inf},0,4,.01f,.004f);
    Refused({}, {1,1,1},0,4,nan,.004f);Refused({}, {1,1,1},0,4,inf,.004f);
    Refused({}, {1,1,1},0,4,.01f,0);Refused({}, {1,1,1},0,4,.01f,nan);
    Refused({1e30f,0,0},{1,1,1},0,4,.01f,.004f);
    std::cout<<"PLANKED_PANEL_PASS cases="<<cases<<" negatives="<<negatives<<" checks="<<checks<<" axes=3 board_limit=128\n";return 0;
}catch(const std::exception& error){std::cerr<<"FIRST_DIVERGENCE "<<error.what()<<'\n';return 1;}}
