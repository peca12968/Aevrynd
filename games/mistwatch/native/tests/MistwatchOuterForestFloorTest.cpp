#include "MistwatchOuterForestFloor.h"
#include "MistwatchForestHabitatFloor.h"
#include <Aevrynd/MeshLoader.h>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <limits>
#include <string>

namespace {
using namespace Aevrynd;
namespace O=Mistwatch::OuterForestFloor;
namespace S=Mistwatch::LandscapeStandFill;
std::size_t checks{},negativeChecks{};
void Need(bool yes,const char* reason){++checks;if(!yes)throw std::runtime_error(reason);}
bool Same(float a,float b){return std::bit_cast<std::uint32_t>(a)==std::bit_cast<std::uint32_t>(b);}
std::uint64_t Fingerprint(const Mesh3D& mesh,bool habitat=false){
    std::uint64_t h=14695981039346656037ull;
    const auto word=[&](std::uint32_t value){for(unsigned i=0;i<4;++i){h^=(value>>(i*8))&255u;h*=1099511628211ull;}};
    if(!habitat){word(static_cast<std::uint32_t>(mesh.vertices.size()));word(static_cast<std::uint32_t>(mesh.indices.size()));}
    for(const auto& v:mesh.vertices){for(float f:{v.x,v.y,v.z,v.u,v.v})word(std::bit_cast<std::uint32_t>(f));
        word(habitat?v.color:v.normal);word(habitat?v.normal:v.color);}
    for(const auto i:mesh.indices)word(i);return h;
}
std::uint64_t PoseFingerprint(const S::Plan& p){
    std::uint64_t h=14695981039346656037ull;
    const auto word=[&](std::uint32_t value){for(unsigned i=0;i<4;++i){h^=(value>>(i*8))&255u;h*=1099511628211ull;}};
    word(static_cast<std::uint32_t>(p.combined.size()));
    for(const auto& t:p.combined)for(float f:{t.position.x,t.position.y,t.position.z,t.rotation.x,t.rotation.y,t.rotation.z,t.scale.x,t.scale.y,t.scale.z})word(std::bit_cast<std::uint32_t>(f));
    for(const auto b:p.addedBands)word(b);return h;
}
MeshBounds3D SourceBounds(const std::filesystem::path& root){
    constexpr std::array<const char*,4> names{"bark","twig","dead-branches","trunk-c"};
    Vec3 lo{10000,10000,10000},hi{-10000,-10000,-10000};
    for(unsigned slot=0;slot<5;++slot){const unsigned part=slot==4?1:slot;const std::string lod=slot==4?"1":"2";
        const auto file="fir-tree-01-c-lod"+lod+"-node0-mesh0-primitive"+std::to_string(part)+"-material"+std::to_string(part)+"-fir-tree-01-"+names[part]+".aemesh";
        const auto loaded=LoadAevryndMeshFile(root/"mistwatch-forest"/"meshes"/file);
        Need(bool(loaded)&&loaded.mesh.Valid(),"actual five fir meshes required");
        for(const auto& v:loaded.mesh.vertices){Need(std::isfinite(v.x)&&std::isfinite(v.y)&&std::isfinite(v.z),"finite source asset");
            lo.x=(std::min)(lo.x,v.x);lo.y=(std::min)(lo.y,v.y);lo.z=(std::min)(lo.z,v.z);
            hi.x=(std::max)(hi.x,v.x);hi.y=(std::max)(hi.y,v.y);hi.z=(std::max)(hi.z,v.z);}
    }
    const auto e=(hi-lo)*.5f;return {(lo+hi)*.5f,Length(e),e};
}
// 獨立 double 參考，不呼叫候選的距離、坡度、場函式或覆蓋計算。
double Smooth(double value){if(value<=0)return 0;if(value>=1)return 1;return 3*value*value-2*value*value*value;}
double ShadingY(const Vertex3D& vertex){
    const auto channel=[](std::uint32_t p){return double(static_cast<std::int8_t>(static_cast<std::uint8_t>(p)))/127.0;};
    const double x=channel(vertex.normal),y=channel(vertex.normal>>8),z=channel(vertex.normal>>16);
    return y/std::sqrt(x*x+y*y+z*z);
}
double Field(const S::Plan& plan,double x,double z,double normalY){
    const double cx=std::clamp(x,-50.0,50.0),cz=std::clamp(z,-20.0,100.0);
    double uncovered=1.0;
    for(const auto& t:plan.combined){const double radius=6.0+6.0*std::clamp((double(t.scale.x)-.5)/.35,0.0,1.0);
        const double dx=x-t.position.x,dz=z-t.position.z,dist=std::sqrt(dx*dx+dz*dz);
        const double weight=Smooth((radius-dist)/radius);uncovered*=1.0-.85*weight;}
    return .72*(1-uncovered)*Smooth(std::sqrt((x-cx)*(x-cx)+(z-cz)*(z-cz))/12.0)*
        Smooth((normalY-double(O::SlopeLimit))/(.90-double(O::SlopeLimit)));
}
std::vector<bool> Frozen(const Mesh3D& mesh,std::size_t& count){
    std::vector<bool> mask(mesh.vertices.size(),false);count=0;
    for(std::size_t i=0;i<mesh.indices.size();i+=3){const auto ia=mesh.indices[i],ib=mesh.indices[i+1],ic=mesh.indices[i+2];
        const auto& a=mesh.vertices[ia];const auto& b=mesh.vertices[ib];const auto& c=mesh.vertices[ic];
        const double ux=double(b.x)-a.x,uy=double(b.y)-a.y,uz=double(b.z)-a.z;
        const double vx=double(c.x)-a.x,vy=double(c.y)-a.y,vz=double(c.z)-a.z;
        const double nx=uy*vz-uz*vy,ny=uz*vx-ux*vz,nz=ux*vy-uy*vx;
        if(ny/std::sqrt(nx*nx+ny*ny+nz*nz)<O::SlopeLimit||ShadingY(a)<O::SlopeLimit||ShadingY(b)<O::SlopeLimit||ShadingY(c)<O::SlopeLimit){
            mask[ia]=true;mask[ib]=true;mask[ic]=true;++count;}
    }return mask;
}
template<class Action>void RejectUnchanged(Mesh3D& mesh,Action action,const char* reason){
    const auto before=Fingerprint(mesh);
    try{action();}catch(const std::invalid_argument& e){
        Need(std::string_view(e.what())==reason,"wrong negative reason");Need(Fingerprint(mesh)==before,"partial writes on rejection");++negativeChecks;return;}
    throw std::runtime_error("invalid input accepted");
}
}
int main(int argc,char** argv)try{
    Need(argc==3&&std::string_view(argv[1])=="--runtime-assets","usage: --runtime-assets ASSETS_DIRECTORY");
    const auto bounds=SourceBounds(std::filesystem::path(argv[2]));
    const auto terrain=Mistwatch::CreateTerrain();const auto original=Mistwatch::LandscapeTrees(terrain);
    const auto plan=S::Create(terrain,original,bounds);const auto planBefore=PoseFingerprint(plan);
    Need(plan.complete&&plan.combined.size()==616&&planBefore==plan.fingerprint,"actual canonical 616 roots");
    auto base=Mistwatch::CreateLandscapeMesh(terrain,true,Mistwatch::ForestFloorMaskSelector::ForestWood,Mistwatch::BankCoverageSelector::Patchy);
    // 核心接縫不受林下樹根影響；外圈仍呼叫原 helper，以核對正式中間態。
    const Mistwatch::ForestHabitatRoots noCoreRoots{};
    const auto oldReceipt=Mistwatch::ApplyForestHabitatFloor(base,terrain,noCoreRoots,true);
    auto core=Mistwatch::CreateTerrainMesh(terrain,true,Mistwatch::ForestFloorMaskSelector::ForestWood,Mistwatch::BankCoverageSelector::Patchy);
    const auto coreBefore=Fingerprint(core);const auto baseBefore=Fingerprint(base);
    auto mesh=base,repeat=base;
    const auto r=O::Apply(mesh,terrain,plan,bounds),again=O::Apply(repeat,terrain,plan,bounds);
    Need(r.roots==616&&r.vertices==base.vertices.size()&&r.changed>0&&r.emptyVertices>0,"actual grid changes and untouched clearings");
    Need(r.poseFingerprint==planBefore&&PoseFingerprint(plan)==planBefore,"readonly actual roots");
    Need(r.beforeFingerprint==baseBefore&&r.afterFingerprint==Fingerprint(mesh)&&r.afterFingerprint==again.afterFingerprint,"independent mesh fingerprints");
    Need(r.beforeHabitatFingerprint==oldReceipt.fingerprint&&r.beforeHabitatFingerprint==Fingerprint(base,true)&&r.afterHabitatFingerprint==Fingerprint(mesh,true),"independent habitat fingerprints");
    Need(r.minimumRadius>=6&&r.maximumRadius<=12&&r.maximumRadius>r.minimumRadius,"world-metre radius extent");
    Need(mesh.indices==base.indices&&mesh.vertices.size()==base.vertices.size(),"immutable topology");
    std::size_t protectedTriangles{},protectedVertices{},seams{},changed{},empty{},clearings{};double delta{};
    std::array<std::size_t,6> bandChanged{};
    const auto frozen=Frozen(base,protectedTriangles);std::vector<double> oracle;oracle.reserve(base.vertices.size());
    for(std::size_t i=0;i<base.vertices.size();++i){const auto& a=base.vertices[i];const auto& b=mesh.vertices[i];
        Need(Same(a.x,b.x)&&Same(a.y,b.y)&&Same(a.z,b.z)&&a.normal==b.normal&&a.color==b.color,"non-UV geometry mutated");
        Need(Same(b.u,repeat.vertices[i].u)&&Same(b.v,repeat.vertices[i].v),"repeat UV bits");
        Need(Same(a.v,b.v)&&b.u>=a.u&&b.u+b.v<=1.f,"encoded three-layer domain");
        const bool seam=a.x>=-50&&a.x<=50&&a.z>=-20&&a.z<=100&&(a.x==-50||a.x==50||a.z==-20||a.z==100);
        const double f=Field(plan,a.x,a.z,ShadingY(a));empty+=f==0&&!seam&&!frozen[i];protectedVertices+=frozen[i];
        const double coreDistance=std::hypot(double(a.x)-std::clamp(double(a.x),-50.0,50.0),double(a.z)-std::clamp(double(a.z),-20.0,100.0));
        clearings+=f==0&&!seam&&!frozen[i]&&a.x>=-150&&a.x<=150&&a.z>=-110&&a.z<=190&&coreDistance>=12;
        const double expected=seam||frozen[i]?double(a.u):double(a.u)+(std::max)(0.0,1.0-double(a.u)-a.v)*f;
        oracle.push_back(expected);Need(std::abs(double(b.u)-expected)<2e-7,"independent field does not match uploaded UV");
        if(frozen[i]||seam)Need(Same(a.u,b.u)&&Same(a.v,b.v),"protected face or seam changed");
        if(seam){++seams;const auto ix=static_cast<unsigned>(std::lround((a.x+50)*2)),iz=static_cast<unsigned>(std::lround((a.z+20)*2));
            const auto& cv=core.vertices[std::size_t(iz)*201+ix];
            Need(Same(cv.x,b.x)&&Same(cv.y,b.y)&&Same(cv.z,b.z)&&cv.normal==b.normal&&cv.color==b.color&&Same(cv.u,b.u)&&Same(cv.v,b.v),"two sides of core seam differ");}
        changed+=!Same(a.u,b.u);delta+=double(b.u)-a.u;
        if(!Same(a.u,b.u))for(unsigned band=0;band<6;++band){const auto& region=S::Bands[band];
            if(a.x>=region.minX&&a.x<=region.maxX&&a.z>=region.minZ&&a.z<=region.maxZ)++bandChanged[band];}
    }
    Need(seams==880&&seams==r.seams&&changed==r.changed&&empty==r.emptyVertices,"independent vertex counts");
    Need(clearings>0&&clearings==r.clearingVertices,"uncovered clearings inside actual tree-population domain");
    Need(protectedTriangles==r.protectedTriangles&&protectedVertices==r.protectedVertices&&protectedTriangles>0,"full steep-face protection");
    Need(delta==r.soilDelta&&delta>0&&r.rockDelta==0&&Fingerprint(core)==coreBefore&&Fingerprint(base)==baseBefore,"immutable core and delta receipt");
    Need(bandChanged==r.bandChanged,"independent six-band vertex census");
    for(const auto count:r.bandChanged)Need(count>0,"six real stand bands require visible coverage changes");
    constexpr std::array<std::array<double,3>,4> bary{{{{.5,.5,0}},{{.5,0,.5}},{{0,.5,.5}},{{1.0/3,1.0/3,1.0/3}}}};
    constexpr std::array<std::array<double,3>,4> procedural{{{{1,0,0}},{{0,1,0}},{{0,0,1}},{{.3,.4,.3}}}};
    double largestRockReduction{},maxContinuousInterpolationError{},areaDelta{};std::size_t samples{},continuousSamples{},changedTriangles{};
    for(std::size_t face=0;face<mesh.indices.size()/3;++face){const auto ia=mesh.indices[face*3],ib=mesh.indices[face*3+1],ic=mesh.indices[face*3+2];
        const std::array<std::uint32_t,3> ids{ia,ib,ic};
        const auto& a=base.vertices[ia];const auto& b=base.vertices[ib];const auto& c=base.vertices[ic];
        const double ax=double(b.x)-a.x,ay=double(b.y)-a.y,az=double(b.z)-a.z;
        const double bx=double(c.x)-a.x,by=double(c.y)-a.y,bz=double(c.z)-a.z;
        const double nx=ay*bz-az*by,ny=az*bx-ax*bz,nz=ax*by-ay*bx;
        double faceDelta{};for(const auto id:ids)faceDelta+=double(mesh.vertices[id].u)-base.vertices[id].u;
        changedTriangles+=faceDelta>0;areaDelta+=std::sqrt(nx*nx+ny*ny+nz*nz)*faceDelta/6.0;
        for(const auto& weights:bary){double u{},v{},beforeU{},expected{},x{},z{},normalY{};
            for(unsigned j=0;j<3;++j){const auto id=ids[j];const auto& q=mesh.vertices[id];const auto w=weights[j];
                u+=w*q.u;v+=w*q.v;beforeU+=w*base.vertices[id].u;expected+=w*oracle[id];x+=w*q.x;z+=w*q.z;normalY+=w*ShadingY(q);}
            ++samples;Need(std::abs(u-expected)<2e-7&&u>=0&&v>=0&&u+v<=1.0,"edge/centroid barycentric coverage");
            for(const auto& p:procedural){const double grass=p[0]*(1-u-v),soil=p[1]*(1-u-v)+u,rock=p[2]*(1-u-v)+v;
                const double priorRock=p[2]*(1-beforeU-v)+v;
                Need(grass>=0&&soil>=0&&rock>=0&&std::abs(grass+soil+rock-1)<1e-12,"shader three-weight conservation");
                Need(std::abs((rock-priorRock)+p[2]*(u-beforeU))<1e-12,"fixed v does not imply fixed final rock");
                largestRockReduction=(std::max)(largestRockReduction,priorRock-rock);}
            // 連續場誤差只量測，不用未經畫面驗證的任意容差冒充畫質通過。
            if(face%61==0&&!frozen[ia]&&!frozen[ib]&&!frozen[ic]){
                const double continuous=beforeU+(std::max)(0.0,1-beforeU-v)*Field(plan,x,z,normalY);
                maxContinuousInterpolationError=(std::max)(maxContinuousInterpolationError,std::abs(u-continuous));++continuousSamples;}
        }
    }
    Need(samples>0&&continuousSamples>0&&largestRockReduction>0,"real interpolated field and rock-response observation");
    Need(changedTriangles==r.changedTriangles&&std::abs(areaDelta-r.areaWeightedSoilDelta)<=1e-6*(std::max)(1.0,areaDelta),"independent changed-face area integration");
    auto badPlan=plan;badPlan.complete=false;RejectUnchanged(mesh,[&]{(void)O::Apply(mesh,terrain,badPlan,bounds);},"OUTER_PLAN_COUNT");
    badPlan=plan;badPlan.combined.pop_back();RejectUnchanged(mesh,[&]{(void)O::Apply(mesh,terrain,badPlan,bounds);},"OUTER_PLAN_COUNT");
    badPlan=plan;badPlan.combined[0].position.x+=1;RejectUnchanged(mesh,[&]{(void)O::Apply(mesh,terrain,badPlan,bounds);},"OUTER_SOURCE_POSE");
    badPlan=plan;badPlan.combined[520]=badPlan.combined[521];RejectUnchanged(mesh,[&]{(void)O::Apply(mesh,terrain,badPlan,bounds);},"OUTER_SUFFIX_POSE");
    badPlan=plan;badPlan.combined.back().position.z+=.01f;RejectUnchanged(mesh,[&]{(void)O::Apply(mesh,terrain,badPlan,bounds);},"OUTER_SUFFIX_POSE");
    badPlan=plan;badPlan.addedBands[0]=5;RejectUnchanged(mesh,[&]{(void)O::Apply(mesh,terrain,badPlan,bounds);},"OUTER_BANDS");
    badPlan=plan;badPlan.fingerprint^=1;RejectUnchanged(mesh,[&]{(void)O::Apply(mesh,terrain,badPlan,bounds);},"OUTER_POSE_FINGERPRINT");
    badPlan=plan;badPlan.sourceFingerprint^=1;RejectUnchanged(mesh,[&]{(void)O::Apply(mesh,terrain,badPlan,bounds);},"OUTER_POSE_FINGERPRINT");
    badPlan=plan;++badPlan.generated;RejectUnchanged(mesh,[&]{(void)O::Apply(mesh,terrain,badPlan,bounds);},"OUTER_PROPOSALS");
    badPlan=plan;++badPlan.bands[0].slope;RejectUnchanged(mesh,[&]{(void)O::Apply(mesh,terrain,badPlan,bounds);},"OUTER_BAND_COUNTS");
    badPlan=plan;badPlan.combined.back().scale.y=std::numeric_limits<float>::infinity();RejectUnchanged(mesh,[&]{(void)O::Apply(mesh,terrain,badPlan,bounds);},"OUTER_POSE_FINITE");
    const HeightField3D wrongTerrain;RejectUnchanged(mesh,[&]{(void)O::Apply(mesh,wrongTerrain,plan,bounds);},"OUTER_TERRAIN_LAYOUT");
    auto badBounds=bounds;badBounds.radius=0;RejectUnchanged(mesh,[&]{(void)O::Apply(mesh,terrain,plan,badBounds);},"STAND_UNION_BOUNDS");
    auto bad=base;bad.vertices.pop_back();RejectUnchanged(bad,[&]{(void)O::Apply(bad,terrain,plan,bounds);},"OUTER_MESH_COUNT");
    bad=base;bad.indices.back()=static_cast<std::uint32_t>(bad.vertices.size());RejectUnchanged(bad,[&]{(void)O::Apply(bad,terrain,plan,bounds);},"OUTER_INDEX_RANGE");
    bad=base;std::swap(bad.indices[0],bad.indices[1]);RejectUnchanged(bad,[&]{(void)O::Apply(bad,terrain,plan,bounds);},"OUTER_TOPOLOGY");
    bad=base;bad.vertices.back().y=std::numeric_limits<float>::quiet_NaN();RejectUnchanged(bad,[&]{(void)O::Apply(bad,terrain,plan,bounds);},"OUTER_POSITION_FINITE");
    bad=base;bad.vertices.back().normal=0;RejectUnchanged(bad,[&]{(void)O::Apply(bad,terrain,plan,bounds);},"OUTER_NORMAL");
    bad=base;bad.vertices.back().color^=1;RejectUnchanged(bad,[&]{(void)O::Apply(bad,terrain,plan,bounds);},"OUTER_GEOMETRY");
    bad=base;bad.vertices.back().x=std::nextafter(bad.vertices.back().x,1000.f);RejectUnchanged(bad,[&]{(void)O::Apply(bad,terrain,plan,bounds);},"OUTER_GEOMETRY");
    for(const auto value:{-1.f,std::numeric_limits<float>::infinity(),std::numeric_limits<float>::quiet_NaN(),2.f}){
        bad=base;bad.vertices.back().u=value;RejectUnchanged(bad,[&]{(void)O::Apply(bad,terrain,plan,bounds);},"OUTER_COVERAGE");}
    bad=base;bad.vertices.back().u=.7f;bad.vertices.back().v=.7f;RejectUnchanged(bad,[&]{(void)O::Apply(bad,terrain,plan,bounds);},"OUTER_COVERAGE");
    Need(PoseFingerprint(plan)==planBefore&&Fingerprint(mesh)==r.afterFingerprint,"negative controls changed successful result");
    std::cout<<std::setprecision(17)<<"OUTER_FOREST_FLOOR_CPU_PASS roots="<<r.roots<<" vertices="<<r.vertices<<" changed="<<r.changed
        <<" seams="<<r.seams<<" protectedVertices="<<r.protectedVertices<<" protectedTriangles="<<r.protectedTriangles
        <<" emptyVertices="<<r.emptyVertices<<" clearingVertices="<<r.clearingVertices<<" changedTriangles="<<r.changedTriangles<<" poseFingerprint="<<r.poseFingerprint
        <<" beforeFingerprint="<<r.beforeFingerprint<<" afterFingerprint="<<r.afterFingerprint
        <<" beforeHabitatFingerprint="<<r.beforeHabitatFingerprint<<" afterHabitatFingerprint="<<r.afterHabitatFingerprint
        <<" soilDelta="<<r.soilDelta<<" rockDelta="<<r.rockDelta<<" areaWeightedSoilDelta="<<r.areaWeightedSoilDelta
        <<" minimumRadius="<<r.minimumRadius<<" maximumRadius="<<r.maximumRadius;
    for(unsigned b=0;b<6;++b)std::cout<<" band"<<b<<"="<<r.bandChanged[b];
    std::cout<<" samples="<<samples<<" continuousSamples="<<continuousSamples<<" maxContinuousInterpolationError="<<maxContinuousInterpolationError
        <<" largestRockReduction="<<largestRockReduction<<" negativeChecks="<<negativeChecks<<" checks="<<checks<<" native_passed=false\n";
    return 0;
}catch(const std::exception& e){std::cerr<<"OUTER_FOREST_FLOOR_CPU_FAIL "<<e.what()<<" checks="<<checks<<'\n';return 1;}
