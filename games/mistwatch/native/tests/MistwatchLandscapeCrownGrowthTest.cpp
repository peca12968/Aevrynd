#include "MistwatchLandscapeCrownGrowth.h"
#include "MistwatchLandscapeStandFill.h"
#include <Aevrynd/MeshLoader.h>
#include <filesystem>
#include <iostream>
#include <limits>
#include <string>

namespace {
using namespace Aevrynd;
namespace C=Mistwatch::LandscapeCrownGrowth;
namespace S=Mistwatch::LandscapeStandFill;
unsigned negatives{};
void Need(bool yes,const char* message){if(!yes)throw std::runtime_error(message);}
bool Same(float a,float b){return std::bit_cast<std::uint32_t>(a)==std::bit_cast<std::uint32_t>(b);}
bool SamePosition(const Vertex3D& a,const Vertex3D& b){return Same(a.x,b.x)&&Same(a.y,b.y)&&Same(a.z,b.z);}
template<class F>void Reject(F&& action,const char* expected){
    try{action();}catch(const std::invalid_argument& e){Need(std::string_view(e.what())==expected,"wrong rejection");++negatives;return;}
    throw std::runtime_error("negative accepted");
}
void Word(std::uint64_t& h,std::uint32_t w){for(unsigned i=0;i<4;++i){h^=(w>>(i*8))&255u;h*=1099511628211ull;}}
std::uint64_t MeshSeal(const Mesh3D& mesh){
    std::uint64_t h=14695981039346656037ull;Word(h,static_cast<unsigned>(mesh.vertices.size()));Word(h,static_cast<unsigned>(mesh.indices.size()));
    for(const auto& v:mesh.vertices){for(float f:{v.x,v.y,v.z,v.u,v.v})Word(h,std::bit_cast<std::uint32_t>(f));Word(h,v.normal);Word(h,v.color);}
    for(auto i:mesh.indices)Word(h,i);return h;
}
std::uint64_t StandSeal(const S::Plan& plan){
    std::uint64_t h=14695981039346656037ull;Word(h,static_cast<unsigned>(plan.combined.size()));
    for(const auto& p:plan.combined)for(float f:{p.position.x,p.position.y,p.position.z,p.rotation.x,p.rotation.y,p.rotation.z,p.scale.x,p.scale.y,p.scale.z})
        Word(h,std::bit_cast<std::uint32_t>(f));
    for(auto band:plan.addedBands)Word(h,band);return h;
}
struct Extrema {
    Vec3 lo{10000,10000,10000},hi{-10000,-10000,-10000};
    void Add(Vec3 p){
        Need(std::isfinite(p.x)&&std::isfinite(p.y)&&std::isfinite(p.z),"nonfinite bounds");
        lo.x=(std::min)(lo.x,p.x);lo.y=(std::min)(lo.y,p.y);lo.z=(std::min)(lo.z,p.z);
        hi.x=(std::max)(hi.x,p.x);hi.y=(std::max)(hi.y,p.y);hi.z=(std::max)(hi.z,p.z);
    }
    MeshBounds3D Bounds()const{const auto e=(hi-lo)*.5f;return {(hi+lo)*.5f,Length(e),e};}
};
MeshBounds3D Union(const std::array<Mesh3D,5>& meshes){Extrema result;
    for(const auto& mesh:meshes)for(const auto& v:mesh.vertices)result.Add({v.x,v.y,v.z});return result.Bounds();}
void ResolverControls(){
    Need(C::Profile=="overstory-crown-growth-v1","profile literal");
    for(bool capture:{false,true})for(bool replay:{false,true}){
        Need(!C::Resolve(capture,replay,L"")&&!C::Resolve(capture,replay,L"original"),"default changed");
        if(capture!=replay)Need(C::Resolve(capture,replay,L"overstory-crown-growth-v1"),"explicit context rejected");
        else Reject([&]{(void)C::Resolve(capture,replay,L"overstory-crown-growth-v1");},"CROWN_GROWTH_REQUIRED_CONTEXT");
        for(auto value:{L"Overstory-crown-growth-v1",L"overstory-crown-growth-v2",L"overstory-crown-growth-v1 ",L"1",L"mixed-edge-canopy-v1"})
            Reject([&]{(void)C::Resolve(capture,replay,value);},"CROWN_GROWTH_PROFILE");
    }
}
void VerifySource(const Mesh3D& raw,const C::Result& result,unsigned slot){
    const auto& mesh=result.mesh;Need(result.sourceFingerprint==MeshSeal(raw)&&result.sourceFingerprint==C::SourceFingerprints[slot],"source identity");
    Need(result.derivedFingerprint==MeshSeal(mesh)&&result.derivedFingerprint!=result.sourceFingerprint,"derived identity");
    Need(mesh.vertices.size()==raw.vertices.size()&&mesh.indices==raw.indices,"topology changed");
    std::vector<bool> frozen(raw.vertices.size()),changed(raw.vertices.size()),affected(raw.vertices.size());unsigned rootFaces{};
    // 獨立逐原面建構凍結頂點，再比對相同來源位置，不依賴 Grow 的旗標或雜湊。
    for(unsigned f=0;f<raw.indices.size();f+=3){bool low{};for(unsigned j=0;j<3;++j)low|=raw.vertices[raw.indices[f+j]].y<.35f;
        if(!low)continue;++rootFaces;
        for(unsigned j=0;j<3;++j){const auto& v=raw.vertices[raw.indices[f+j]];
            for(unsigned k=0;k<raw.vertices.size();++k)if(v.x==raw.vertices[k].x&&v.y==raw.vertices[k].y&&v.z==raw.vertices[k].z)frozen[k]=true;}
    }
    unsigned moved{};Extrema oldBounds,newBounds;
    for(unsigned i=0;i<raw.vertices.size();++i){const auto& a=raw.vertices[i];const auto& b=mesh.vertices[i];
        Need(Same(a.u,b.u)&&Same(a.v,b.v)&&a.color==b.color&&(a.normal&0xff000000u)==(b.normal&0xff000000u),"material fields changed");
        oldBounds.Add({a.x,a.y,a.z});newBounds.Add({b.x,b.y,b.z});
        if(frozen[i]||a.y<=3.f)Need(SamePosition(a,b),"root/lower position changed");
        else{
            // 用 double 重算獨立公式，再允許單次 float 算術的微小捨入，不容許任意縮放姿態。
            const double t=std::clamp((double(a.y)-3.)/3.,0.,1.),width=1.+.3*t*t*(3.-2.*t);
            Need(std::abs(double(b.x)-a.x*width)<3.e-6&&std::abs(double(b.z)-a.z*width)<3.e-6&&
                std::abs(double(b.y)-(a.y+.3*(a.y-3.)))<3.e-6,"crown formula mismatch");
        }
        changed[i]=!SamePosition(a,b);moved+=changed[i]?1u:0u;
    }
    Need(moved==result.movedVertices&&moved>0&&rootFaces==result.frozenRootFaces,"morph counters");
    Need(newBounds.hi.y>oldBounds.hi.y&&newBounds.hi.x>=oldBounds.hi.x&&newBounds.lo.x<=oldBounds.lo.x&&
        newBounds.hi.z>=oldBounds.hi.z&&newBounds.lo.z<=oldBounds.lo.z,"no real crown growth");
    std::vector<Vec3> accumulated(raw.vertices.size());
    for(unsigned f=0;f<raw.indices.size();f+=3){const auto a=raw.indices[f],b=raw.indices[f+1],c=raw.indices[f+2];
        const auto point=[](const Vertex3D& v){return Vec3{v.x,v.y,v.z};};
        const auto original=Cross(point(raw.vertices[b])-point(raw.vertices[a]),point(raw.vertices[c])-point(raw.vertices[a]));
        const auto normal=Cross(point(mesh.vertices[b])-point(mesh.vertices[a]),point(mesh.vertices[c])-point(mesh.vertices[a]));
        Need(std::isfinite(Length(normal))&&Length(normal)>0&&Dot(original,normal)>0,"face degenerate or flipped");
        accumulated[a]=accumulated[a]+normal;accumulated[b]=accumulated[b]+normal;accumulated[c]=accumulated[c]+normal;
        if(changed[a]||changed[b]||changed[c])affected[a]=affected[b]=affected[c]=true;
        if(raw.vertices[a].y<.35f||raw.vertices[b].y<.35f||raw.vertices[c].y<.35f)
            Need(SamePosition(raw.vertices[a],mesh.vertices[a])&&SamePosition(raw.vertices[b],mesh.vertices[b])&&
                SamePosition(raw.vertices[c],mesh.vertices[c]),"whole original lower face changed");
    }
    unsigned count{};
    for(unsigned i=0;i<raw.vertices.size();++i){if(!affected[i]){Need(mesh.vertices[i].normal==raw.vertices[i].normal,"unaffected normal changed");continue;}
        ++count;const auto n=UnpackNormalSnorm8(mesh.vertices[i].normal);const Vec3 actual{n.x,n.y,n.z};
        Need(std::abs(Length(actual)-1.f)<.015f&&Dot(Normalize(actual),Normalize(accumulated[i]))>.9999f,"derived adjacent normal mismatch");
    }
    Need(count==result.affectedNormals&&count>0,"normal counter");
    const auto repeat=C::Grow(raw);Need(repeat.derivedFingerprint==result.derivedFingerprint&&repeat.movedVertices==moved&&
        repeat.frozenRootFaces==rootFaces&&repeat.affectedNormals==count,"nondeterministic derivation");
    Reject([&]{(void)C::Grow(mesh);},"CROWN_GROWTH_SOURCE_SEAL");
    auto corrupt=raw;corrupt.vertices[0].normal^=0x01000000u;Reject([&]{(void)C::Grow(corrupt);},"CROWN_GROWTH_SOURCE_SEAL");
    corrupt=raw;corrupt.vertices[0].color^=1;Reject([&]{(void)C::Grow(corrupt);},"CROWN_GROWTH_SOURCE_SEAL");
    corrupt=raw;corrupt.vertices[0].u=std::nextafter(corrupt.vertices[0].u,10000.f);Reject([&]{(void)C::Grow(corrupt);},"CROWN_GROWTH_SOURCE_SEAL");
    std::cout<<"CROWN_SOURCE slot="<<slot<<" source="<<result.sourceFingerprint<<" derived="<<result.derivedFingerprint
        <<" moved="<<moved<<" root_faces="<<rootFaces<<" normals="<<count<<" old_top="<<oldBounds.hi.y<<" new_top="<<newBounds.hi.y<<'\n';
}
}
int main(int argc,char** argv){try{
    ResolverControls();Need(argc==3&&std::string_view(argv[1])=="--runtime-assets","usage: --runtime-assets ABSOLUTE_ASSETS_DIRECTORY");
    const std::filesystem::path root(argv[2]);Need(root.is_absolute(),"absolute runtime assets required");
    std::array<Mesh3D,5> raw,grown;std::array<std::uint64_t,5> saved{};
    constexpr std::array<const char*,4> names{"bark","twig","dead-branches","trunk-c"};
    for(unsigned slot=0;slot<5;++slot){const unsigned part=slot==4?1:slot;const std::string lod=slot==4?"1":"2";
        const auto file="fir-tree-01-c-lod"+lod+"-node0-mesh0-primitive"+std::to_string(part)+"-material"+std::to_string(part)+"-fir-tree-01-"+names[part]+".aemesh";
        auto loaded=LoadAevryndMeshFile(root/"mistwatch-forest"/"meshes"/file);Need(bool(loaded)&&loaded.mesh.Valid(),"real fir source missing");
        raw[slot]=std::move(loaded.mesh);saved[slot]=MeshSeal(raw[slot]);const auto result=C::Grow(raw[slot]);
        VerifySource(raw[slot],result,slot);grown[slot]=result.mesh;Need(saved[slot]==MeshSeal(raw[slot]),"source mutated");
    }
    auto invalid=raw[0];invalid.vertices[0].x=std::numeric_limits<float>::quiet_NaN();
    Reject([&]{(void)C::Grow(invalid);},"LANDSCAPE_SPECIES_NONFINITE_MESH");
    invalid=raw[0];invalid.indices[1]=invalid.indices[0];Reject([&]{(void)C::Grow(invalid);},"LANDSCAPE_SPECIES_DEGENERATE_FACE");
    const auto originalBounds=Union(raw),grownBounds=Union(grown);
    const auto terrain=Mistwatch::CreateTerrain();const auto base=Mistwatch::LandscapeTrees(terrain),savedBase=base;
    const auto stand=S::Create(terrain,base,originalBounds);const auto seal=StandSeal(stand);
    Need(base.size()==520&&stand.complete&&stand.combined.size()==616&&stand.addedBands.size()==96&&stand.fingerprint==seal,"canonical stand");
    const auto repeated=S::Create(terrain,base,originalBounds);
    Need(repeated.fingerprint==seal&&StandSeal(repeated)==seal&&repeated.generated==stand.generated&&repeated.addedBands==stand.addedBands,"stand seal changed");
    unsigned core{},edge{},vertexCore{},vertexEdge{};
    for(unsigned ordinal=0;ordinal<616;++ordinal){const auto& pose=stand.combined[ordinal];
        Need(S::Detail::Same(pose,repeated.combined[ordinal]),"canonical pose changed");
        if(ordinal<520)Need(S::Detail::Same(pose,base[ordinal])&&S::Detail::Same(base[ordinal],savedBase[ordinal]),"base root/pose moved");
        const auto& meshes=ordinal<520?grown:raw;const auto bounds=TransformBounds(ordinal<520?grownBounds:originalBounds,pose);
        const auto lo=bounds.center-bounds.extents,hi=bounds.center+bounds.extents;
        core+=!(hi.x< -50.f||lo.x>50.f||hi.z< -20.f||lo.z>100.f);
        edge+=lo.x< -200.f||hi.x>200.f||lo.z< -170.f||hi.z>250.f;
        // 另逐全部實際來源頂點獨立計算 world bounds；96 株使用原始網格而非放大版本。
        Extrema actual;const auto matrix=pose.Matrix();
        for(const auto& mesh:meshes)for(const auto& v:mesh.vertices)actual.Add(TransformPoint(matrix,{v.x,v.y,v.z}));
        vertexCore+=!(actual.hi.x< -50.f||actual.lo.x>50.f||actual.hi.z< -20.f||actual.lo.z>100.f);
        vertexEdge+=actual.lo.x< -200.f||actual.hi.x>200.f||actual.lo.z< -170.f||actual.hi.z>250.f;
    }
    for(unsigned slot=0;slot<5;++slot)Need(MeshSeal(raw[slot])==saved[slot],"understory source changed");
    std::cout<<"CROWN_LAYOUT base=520 understory=96 stand="<<seal<<" bound_core="<<core<<" bound_edge="<<edge
        <<" actual_core="<<vertexCore<<" actual_edge="<<vertexEdge<<" negatives="<<negatives
        <<" root_repair_passed=false native_passed=false art_passed=false\n";
    Need(core==0&&edge==0&&vertexCore==0&&vertexEdge==0,"crown growth violates actual core or landscape edge");
    return 0;
}catch(const std::exception& e){std::cerr<<"CROWN_GROWTH_FAIL "<<e.what()<<'\n';return 1;}}
