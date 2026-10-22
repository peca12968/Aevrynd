#include <Aevrynd/StaticAabbIndex3D.h>
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace Aevrynd;
namespace {unsigned checks{};void Need(bool value){++checks;if(!value)throw std::runtime_error("STATIC_AABB_INDEX_ASSERT");}
StaticAabbQueryResult3D Linear(std::span<const ClosedAabb3D> boxes,const ClosedAabb3D& query){
    StaticAabbQueryResult3D r;r.status=StaticAabbQueryStatus3D::Disjoint;
    for(const auto& b:boxes){++r.boxesTested;bool separated{};
        for(unsigned a=0;a<3;++a)separated|=query.maximum[a]<b.minimum[a]||b.maximum[a]<query.minimum[a];
        if(!separated){r.status=StaticAabbQueryStatus3D::Overlapping;break;}
    }return r;
}}
int main(){try{
    const ClosedAabb3D box{{0,0,0},{1,1,1}};const StaticAabbIndex3D single(std::span(&box,1)),empty(std::span<const ClosedAabb3D>{});
    Need(single.Valid()&&empty.Valid()&&single.Size()==1&&empty.Size()==0);
    Need(empty.Query(box).status==StaticAabbQueryStatus3D::Disjoint);
    for(unsigned axis=0;axis<3;++axis){auto q=box;q.minimum[axis]=q.maximum[axis]=1;
        Need(single.Query(q).status==StaticAabbQueryStatus3D::Overlapping);
        q.minimum[axis]=q.maximum[axis]=std::nextafter(1.,2.);Need(single.Query(q).status==StaticAabbQueryStatus3D::Disjoint);
        q.minimum[axis]=q.maximum[axis]=std::nextafter(1.,0.);Need(single.Query(q).status==StaticAabbQueryStatus3D::Overlapping);
    }
    const double nan=std::numeric_limits<double>::quiet_NaN(),inf=std::numeric_limits<double>::infinity();
    for(double bad:{nan,inf,-inf})for(unsigned axis=0;axis<3;++axis){auto q=box;q.minimum[axis]=bad;
        Need(single.Query(q).status==StaticAabbQueryStatus3D::Invalid);Need(!StaticAabbIndex3D(std::span(&q,1)).Valid());
        q=box;q.maximum[axis]=bad;Need(single.Query(q).status==StaticAabbQueryStatus3D::Invalid);Need(!StaticAabbIndex3D(std::span(&q,1)).Valid());}
    auto reversed=box;reversed.minimum[1]=2;Need(!StaticAabbIndex3D(std::span(&reversed,1)).Valid());
    std::vector<ClosedAabb3D> excessive(StaticAabbIndex3D::MaximumBounds+1,box);Need(!StaticAabbIndex3D(excessive).Valid());excessive.pop_back();Need(StaticAabbIndex3D(excessive).Valid());
    const double huge=std::numeric_limits<double>::max();std::vector<ClosedAabb3D> extreme;
    for(unsigned i=0;i<16;++i)extreme.push_back(i&1?ClosedAabb3D{{huge*.5,0,0},{huge,1,1}}:ClosedAabb3D{{-huge,0,0},{-huge*.5,1,1}});
    const StaticAabbIndex3D extremeIndex(extreme);Need(extremeIndex.Valid()&&extremeIndex.Query(box).status==StaticAabbQueryStatus3D::Disjoint);
    std::vector<ClosedAabb3D> world;world.reserve(8192);
    for(unsigned z=0;z<64;++z)for(unsigned x=0;x<128;++x){const double px=double(x)*2.-128.,pz=double(z)*2.-64.;world.push_back({{px-.45,-.2,pz-.45},{px+.45,1.2,pz+.45}});}
    const StaticAabbIndex3D index(world);Need(index.Valid()&&index.Size()==8192&&index.Nodes()<16384);
    std::uint32_t seed=0x424f554eu;const auto random=[&](){seed=seed*1664525u+1013904223u;return double(seed>>8)/16777216.;};
    std::size_t indexedTests{},linearTests{},nodeTests{};
    for(unsigned i=0;i<4096;++i){const double x=(random()-.5)*300.,z=(random()-.5)*160.;const ClosedAabb3D q{{x-.08,0,z-.08},{x+.08,1,z+.08}};
        const auto a=index.Query(q),b=Linear(world,q);Need(a.status==b.status);indexedTests+=a.boxesTested;linearTests+=b.boxesTested;nodeTests+=a.nodesTested;}
    Need(indexedTests*32<linearTests);
    std::cout<<"STATIC_AABB_INDEX_PASS checks="<<checks<<" queries=4096 boxes=8192 indexed_box_tests="<<indexedTests<<" linear_box_tests="<<linearTests<<" node_tests="<<nodeTests<<'\n';return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 20;}}
