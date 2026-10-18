#include "MistwatchLandscapeStandFill.h"
#include <Aevrynd/MeshLoader.h>
#include <filesystem>
#include <iostream>
#include <limits>
#include <string>

namespace {
using namespace Aevrynd;
namespace S=Mistwatch::LandscapeStandFill;
void Need(bool yes,const char* what){if(!yes)throw std::runtime_error(what);}
bool SamePose(const Transform3D& a,const Transform3D& b){
    const std::array<float,9> x{a.position.x,a.position.y,a.position.z,a.rotation.x,a.rotation.y,a.rotation.z,a.scale.x,a.scale.y,a.scale.z};
    const std::array<float,9> y{b.position.x,b.position.y,b.position.z,b.rotation.x,b.rotation.y,b.rotation.z,b.scale.x,b.scale.y,b.scale.z};
    for(unsigned i=0;i<9;++i)if(std::bit_cast<std::uint32_t>(x[i])!=std::bit_cast<std::uint32_t>(y[i]))return false;
    return true;
}
template<class F>void Reject(F&& action,const char* reason){
    try{action();}catch(const std::invalid_argument& e){Need(std::string_view(e.what())==reason,"wrong rejection");return;}
    throw std::runtime_error("missing rejection");
}
// 獨立逐真實網格三角面找高度，不重用 LandscapeSurfaceHeight。
double Surface(const Mesh3D& mesh,float x,float z){
    for(std::size_t i=0;i<mesh.indices.size();i+=3){
        const auto& a=mesh.vertices[mesh.indices[i]];const auto& b=mesh.vertices[mesh.indices[i+1]];const auto& c=mesh.vertices[mesh.indices[i+2]];
        if(x<(std::min)({a.x,b.x,c.x})||x>(std::max)({a.x,b.x,c.x})||z<(std::min)({a.z,b.z,c.z})||z>(std::max)({a.z,b.z,c.z}))continue;
        const double ax=a.x,az=a.z,bx=b.x,bz=b.z,cx=c.x,cz=c.z;
        const double d=(bz-cz)*(ax-cx)+(cx-bx)*(az-cz);
        if(d==0)continue;
        const double wa=((bz-cz)*(double(x)-cx)+(cx-bx)*(double(z)-cz))/d;
        const double wb=((cz-az)*(double(x)-cx)+(ax-cx)*(double(z)-cz))/d,wc=1-wa-wb;
        if(wa< -1e-10||wb< -1e-10||wc< -1e-10)continue;
        return wa*a.y+wb*b.y+wc*c.y;
    }throw std::runtime_error("no actual supporting triangle");
}
std::uint64_t IndependentFingerprint(const S::Plan& p){
    std::uint64_t h=14695981039346656037ull;
    const auto word=[&](std::uint32_t w){for(unsigned j=0;j<4;++j){h^=(w>>(8*j))&255u;h*=1099511628211ull;}};
    word(static_cast<std::uint32_t>(p.combined.size()));
    for(const auto& t:p.combined)for(float f:{t.position.x,t.position.y,t.position.z,t.rotation.x,t.rotation.y,t.rotation.z,t.scale.x,t.scale.y,t.scale.z})word(std::bit_cast<std::uint32_t>(f));
    for(auto b:p.addedBands)word(b);return h;
}
}
int main(int argc,char** argv){try{
    Need(argc==3&&std::string_view(argv[1])=="--runtime-assets","usage: --runtime-assets ASSETS_DIRECTORY");
    const std::filesystem::path root(argv[2]);
    std::array<Mesh3D,5> meshes;
    constexpr std::array<const char*,4> names{"bark","twig","dead-branches","trunk-c"};
    Vec3 lo{10000,10000,10000},hi{-10000,-10000,-10000};
    for(unsigned slot=0;slot<5;++slot){const unsigned part=slot==4?1:slot;const std::string lod=slot==4?"1":"2";
        const auto path=root/"mistwatch-forest"/"meshes"/("fir-tree-01-c-lod"+lod+"-node0-mesh0-primitive"+std::to_string(part)+"-material"+std::to_string(part)+"-fir-tree-01-"+names[part]+".aemesh");
        auto loaded=LoadAevryndMeshFile(path);Need(bool(loaded)&&loaded.mesh.Valid(),"actual fir asset missing");meshes[slot]=std::move(loaded.mesh);
        for(const auto& v:meshes[slot].vertices){Need(std::isfinite(v.x)&&std::isfinite(v.y)&&std::isfinite(v.z),"nonfinite asset");
            lo.x=(std::min)(lo.x,v.x);lo.y=(std::min)(lo.y,v.y);lo.z=(std::min)(lo.z,v.z);
            hi.x=(std::max)(hi.x,v.x);hi.y=(std::max)(hi.y,v.y);hi.z=(std::max)(hi.z,v.z);}
    }
    const Vec3 extents=(hi-lo)*.5f;const MeshBounds3D bounds{(hi+lo)*.5f,Length(extents),extents};
    const auto terrain=Mistwatch::CreateTerrain();const auto base=Mistwatch::LandscapeTrees(terrain),saved=base;
    const auto landscape=Mistwatch::CreateLandscapeMesh(terrain);
    const auto plan=S::Create(terrain,base,bounds),again=S::Create(terrain,base,bounds);
    Need(plan.combined.size()==520+plan.addedBands.size()&&plan.addedBands.size()<=96,"population cap");
    Need(plan.generated<=3456&&plan.fingerprint==IndependentFingerprint(plan),"budget or fingerprint");
    Need(plan.combined.size()==again.combined.size()&&plan.fingerprint==again.fingerprint&&plan.generated==again.generated,"repeat metadata");
    for(unsigned i=0;i<base.size();++i)Need(SamePose(base[i],saved[i])&&SamePose(base[i],plan.combined[i]),"base mutation");
    for(unsigned i=0;i<plan.combined.size();++i)Need(SamePose(plan.combined[i],again.combined[i]),"repeat pose");
    unsigned proposals{},accepted{};std::array<unsigned,6> observed{};double maxError{};
    for(unsigned i=520;i<plan.combined.size();++i){const auto& p=plan.combined[i];const auto band=plan.addedBands[i-520];
        Need(band<6,"band id");++observed[band];const auto& region=S::Bands[band];
        Need(p.position.x>=region.minX&&p.position.x<=region.maxX&&p.position.z>=region.minZ&&p.position.z<=region.maxZ,"band membership");
        Need(Mistwatch::LandscapeNormal(terrain,p.position.x,p.position.z).y>=.78f,"slope");
        const double error=std::abs(double(p.position.y)-Surface(landscape,p.position.x,p.position.z));maxError=(std::max)(maxError,error);
        Need(error<=.00005,"actual triangle root gap");
        for(unsigned j=0;j<i;++j)Need(std::hypot(double(p.position.x)-plan.combined[j].position.x,double(p.position.z)-plan.combined[j].position.z)>=5.0,"spacing");
        // 逐五個作者網格所有頂點獨立驗證，不呼叫候選 transformed-bounds 判定。
        Vec3 vmin{10000,10000,10000},vmax{-10000,-10000,-10000};const auto m=p.Matrix();
        for(const auto& mesh:meshes)for(const auto& v:mesh.vertices){const auto w=TransformPoint(m,{v.x,v.y,v.z});
            vmin.x=(std::min)(vmin.x,w.x);vmin.z=(std::min)(vmin.z,w.z);
            vmax.x=(std::max)(vmax.x,w.x);vmax.z=(std::max)(vmax.z,w.z);}
        Need(vmax.x< -53.f||vmin.x>53.f||vmax.z< -23.f||vmin.z>103.f,"actual whole tree enters core");
        Need(vmin.x>=-200.f&&vmax.x<=200.f&&vmin.z>=-170.f&&vmax.z<=250.f,"whole tree outside landscape");
    }
    for(unsigned b=0;b<6;++b){const auto& c=plan.bands[b];const auto& r=again.bands[b];
        Need(c.accepted==observed[b]&&c.accepted<=16&&c.proposals<=576,"per-band cap");
        Need(c.proposals==c.accepted+c.slope+c.core+c.edge+c.spacing,"rejection accounting");
        Need(c.proposals==r.proposals&&c.accepted==r.accepted&&c.slope==r.slope&&c.core==r.core&&c.edge==r.edge&&c.spacing==r.spacing,"repeat counters");
        proposals+=c.proposals;accepted+=c.accepted;
        std::cout<<"STAND_BAND id="<<b<<" accepted="<<c.accepted<<" proposals="<<c.proposals<<" slope="<<c.slope<<" core="<<c.core<<" edge="<<c.edge<<" spacing="<<c.spacing<<'\n';
    }
    Need(proposals==plan.generated&&accepted==plan.addedBands.size()&&plan.complete==(accepted==96),"completion accounting");
    const auto empty=S::Create(terrain,base,bounds,0);Need(!empty.complete&&empty.combined.size()==520&&empty.generated==0,"zero-budget shortfall");
    const auto one=S::Create(terrain,base,bounds,1);Need(!one.complete&&one.generated==1&&one.addedBands.size()<=1,"bounded shortfall");
    auto enormous=bounds;enormous.extents={1000,1000,1000};enormous.radius=2000;
    const auto none=S::Create(terrain,base,enormous);Need(!none.complete&&none.addedBands.empty()&&none.generated==3456,"geometric shortfall");
    Reject([&]{(void)S::Create(terrain,base,bounds,3457);},"STAND_PROPOSAL_LIMIT");
    auto fewer=base;fewer.pop_back();Reject([&]{(void)S::Create(terrain,fewer,bounds);},"STAND_SOURCE_COUNT");
    auto altered=base;altered[0].position.x=std::nextafter(altered[0].position.x,10000.f);
    Reject([&]{(void)S::Create(terrain,altered,bounds);},"STAND_SOURCE_POSE");
    auto invalid=bounds;invalid.extents.x=0;Reject([&]{(void)S::Create(terrain,base,invalid);},"STAND_UNION_BOUNDS");
    invalid=bounds;invalid.radius=std::numeric_limits<float>::quiet_NaN();Reject([&]{(void)S::Create(terrain,base,invalid);},"STAND_UNION_BOUNDS");
    const HeightField3D wrong;Reject([&]{(void)S::Create(wrong,base,bounds);},"STAND_TERRAIN_LAYOUT");
    std::cout<<"LANDSCAPE_STAND_DRAFT_RESULT actual="<<accepted<<" target=96 complete="<<plan.complete<<" proposals="<<plan.generated
        <<" fingerprint="<<plan.fingerprint<<" source_fingerprint="<<plan.sourceFingerprint<<" max_root_error="<<maxError
        <<" negative_checks=6 native_passed=false\n";
    return plan.complete?0:2;
}catch(const std::exception& e){std::cerr<<"LANDSCAPE_STAND_DRAFT_FAIL "<<e.what()<<'\n';return 1;}}
