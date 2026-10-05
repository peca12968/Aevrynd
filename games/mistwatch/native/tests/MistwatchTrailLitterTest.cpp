#include "MistwatchTrailLitter.h"
#include "MistwatchMiddleLayer.h"
#include <iostream>
#include <algorithm>
#include <bit>
#include <limits>

namespace {
using namespace Aevrynd;using namespace Mistwatch;
void Require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
struct Range {double minimum{},maximum{};};
Range SinRange(double a,double b) {
    constexpr double Pi=3.14159265358979323846;
    Range result{(std::min)(std::sin(a),std::sin(b)),(std::max)(std::sin(a),std::sin(b))};
    const auto first=int(std::ceil((a-Pi*.5)/Pi)),last=int(std::floor((b-Pi*.5)/Pi));
    for(int k=first;k<=last;++k){const double value=k%2?-1.:1.;result.minimum=(std::min)(result.minimum,value);result.maximum=(std::max)(result.maximum,value);}
    return result;
}
// 直接利用實際地形網格三角形的重心座標，不调用 production SampleHeight 判定其自身。
float IndependentHeight(const HeightField3D& terrain,const Mesh3D& ground,float px,float pz) {
    const auto& grid=terrain.Settings();
    Require(terrain.Contains(px,pz),"Litter vertex outside core terrain");
    const unsigned x=(std::min)(grid.width-2,unsigned(std::floor((px-grid.origin.x)/grid.cellSize)));
    const unsigned z=(std::min)(grid.depth-2,unsigned(std::floor((pz-grid.origin.z)/grid.cellSize)));
    const auto offset=(std::size_t(z)*(grid.width-1)+x)*6;
    for(unsigned face=0;face<2;++face) {
        const auto& a=ground.vertices[ground.indices[offset+face*3]];
        const auto& b=ground.vertices[ground.indices[offset+face*3+1]];
        const auto& c=ground.vertices[ground.indices[offset+face*3+2]];
        const double denominator=(double(b.z)-c.z)*(a.x-c.x)+(double(c.x)-b.x)*(a.z-c.z);
        const double u=((double(b.z)-c.z)*(px-c.x)+(double(c.x)-b.x)*(pz-c.z))/denominator;
        const double v=((double(c.z)-a.z)*(px-c.x)+(double(a.x)-c.x)*(pz-c.z))/denominator;
        if(u>=-1.e-5&&v>=-1.e-5&&u+v<=1.00001)return float(u*a.y+v*b.y+(1.-u-v)*c.y);
    }
    throw std::runtime_error("Litter point not in actual ground triangle");
}
void IndependentClearance(const TrailLitterFootprint& patch,std::span<const BoxCollider3D> solids) {
    const double minX=double(patch.centre.x)-patch.halfX,maxX=double(patch.centre.x)+patch.halfX;
    const double minZ=double(patch.centre.z)-patch.halfZ,maxZ=double(patch.centre.z)+patch.halfZ;
    const auto centre=SinRange(minZ*.19,maxZ*.19),width=SinRange(minZ*.37,maxZ*.37);
    const double left=.35*centre.minimum-2.8-.3*width.maximum;
    const double right=.35*centre.maximum+2.8+.3*width.maximum;
    Require(maxX<left||minX>right,"Full litter footprint intersects continuous visible road");
    Require(maxZ<=29.||minZ>=43.,"Litter enters bridge strip");
    const auto circle=[&](Vec3 point,double radius) {
        const double closestX=std::clamp(double(point.x),minX,maxX),closestZ=std::clamp(double(point.z),minZ,maxZ);
        Require(std::hypot(point.x-closestX,point.z-closestZ)>=radius,"Full litter footprint intersects objective circle");
    };
    const auto branch=[&](Vec3 point,double zMargin,double xMargin) {
        Require(!(maxZ>point.z-zMargin&&minZ<point.z+zMargin&&
            maxX>(std::min)(0.,double(point.x))-xMargin&&minX<(std::max)(0.,double(point.x))+xMargin),
            "Litter enters objective approach branch");
    };
    for(const auto point:Session::supplies){circle(point,3.);branch(point,2.,2.);}
    for(const auto point:Session::repairSites){circle(point,4.);branch(point,1.5,1.);}
    for(const auto point:Session::relaySites){circle(point,4.);branch(point,1.8,1.);}
    for(const auto point:{Session::fuse,Session::generator,Session::rangerLog}){circle(point,5.);branch(point,1.15,0.);}
    circle(Session::arena,7.);circle(Session::beacon,5.);
    for(unsigned step=0;step<=128;++step) {
        const float x=float(minX+(maxX-minX)*double(step)/128.);
        const double river=RiverCenterZ(x),closestZ=std::clamp(river,minZ,maxZ);
        Require(std::abs(river-closestZ)>=RiverOuterWidth(x)+2.,"Full litter footprint intersects river clearance");
    }
    for(const auto& box:solids)
        Require(!(maxX>box.center.x-box.extents.x&&minX<box.center.x+box.extents.x&&
            maxZ>box.center.z-box.extents.z&&minZ<box.center.z+box.extents.z),"Litter enters solid full projection");
}
void Measure(const TrailLitterResult& result,const HeightField3D& terrain,std::span<const BoxCollider3D> solids,
    const TrailLitterSettings& settings) {
    const auto ground=terrain.CreateMesh();
    Require(result.patches.size()<=settings.targetPatches&&result.patches.size()<=64,"Patch count exceeds cap");
    Require(result.branchCount==result.patches.size()*settings.branchesPerPatch&&
        result.stoneCount==result.patches.size()*settings.stonesPerPatch,"Piece counts do not match accepted whole patches");
    Require((result.branches.indices.size()+result.stones.indices.size())/3<=settings.maximumTriangles,"Triangle cap exceeded");
    unsigned left{},right{};
    for(const auto& patch:result.patches) {
        IndependentClearance(patch,solids);
        Require(std::abs(patch.centre.y-IndependentHeight(terrain,ground,patch.centre.x,patch.centre.z))<1.e-5f,
            "Patch root lacks actual triangle support");
        left+=patch.centre.x<0;right+=patch.centre.x>0;
    }
    Require(left&&right,"Actual world missing one road shoulder");
    float maximumHeight{};std::size_t interiorChecks{};
    for(const auto* mesh:{&result.branches,&result.stones}) {
        Require(mesh->Valid(),"Merged litter mesh invalid");
        for(const auto& vertex:mesh->vertices) {
            Require(Session::Finite({vertex.x,vertex.y,vertex.z}),"Nonfinite litter vertex");
            const float delta=vertex.y-IndependentHeight(terrain,ground,vertex.x,vertex.z);
            Require(delta>=.0024f&&delta<=.04501f,"Vertex not within low litter surface envelope");
            maximumHeight=(std::max)(maximumHeight,delta);
            const auto normal=UnpackNormalSnorm8(vertex.normal);
            Require(std::isfinite(normal.x)&&std::isfinite(normal.y)&&std::isfinite(normal.z),"Invalid litter normal");
            bool covered=false;
            for(const auto& patch:result.patches)
                covered|=std::abs(vertex.x-patch.centre.x)<=patch.halfX&&std::abs(vertex.z-patch.centre.z)<=patch.halfZ;
            Require(covered,"Actual geometry escaped all authored complete footprints");
        }
        for(std::size_t face=0;face<mesh->indices.size();face+=3) {
            const auto& a=mesh->vertices[mesh->indices[face]],b=mesh->vertices[mesh->indices[face+1]],c=mesh->vertices[mesh->indices[face+2]];
            const Vec3 pa{a.x,a.y,a.z},pb{b.x,b.y,b.z},pc{c.x,c.y,c.z};
            Require(Length(Cross(pb-pa,pc-pa))>1.e-10f,"Degenerate litter triangle");
            for(const auto weights:std::array<Vec3,7>{{{1.f/3,1.f/3,1.f/3},{.8f,.1f,.1f},{.1f,.8f,.1f},{.1f,.1f,.8f},
                {.5f,.5f,0},{0,.5f,.5f},{.5f,0,.5f}}}) {
                const auto point=pa*weights.x+pb*weights.y+pc*weights.z;
                const float offset=point.y-IndependentHeight(terrain,ground,point.x,point.z);
                Require(offset>=.0023f&&offset<=.0451f,"Triangle interior crosses terrain or exceeds low envelope");
                ++interiorChecks;
            }
        }
    }
    std::cout<<"patches="<<result.patches.size()<<" sides="<<left<<','<<right<<" branches="<<result.branchCount<<
        " stones="<<result.stoneCount<<" triangles="<<(result.branches.indices.size()+result.stones.indices.size())/3<<
        " max_height="<<maximumHeight<<" interior_checks="<<interiorChecks<<" fingerprint="<<result.fingerprint<<'\n';
}
bool Equal(const Mesh3D& a,const Mesh3D& b) {
    if(a.indices!=b.indices||a.vertices.size()!=b.vertices.size())return false;
    for(std::size_t index=0;index<a.vertices.size();++index) {
        const auto& x=a.vertices[index];const auto& y=b.vertices[index];
        if(x.x!=y.x||x.y!=y.y||x.z!=y.z||x.normal!=y.normal||x.color!=y.color||x.u!=y.u||x.v!=y.v)return false;
    }
    return true;
}
template<class F> void Invalid(F run) {
    bool rejected=false;try{run();}catch(const std::invalid_argument&){rejected=true;}
    Require(rejected,"Invalid input did not fail closed");
}
}
int main() {
    try {
        const auto terrain=CreateTerrain();auto solids=WorldSolids(false);
        const auto trees=TreePositions();const auto props=ForestProps(terrain,trees);
        const auto understory=CreateUnderstory(terrain,trees,props);
        const auto broadleaf=CreateBroadleafTrees(terrain,trees,props,understory);
        const auto middle=CreateMiddleLayer(terrain,trees,props,understory,broadleaf,solids);
        Require(middle.reachedBudget,"Production middle-layer baseline incomplete");
        for(const auto& tree:middle.trees)solids.push_back(tree.trunk);
        const TrailLitterSettings settings;
        const auto result=CreateTrailLitter(terrain,solids,settings),repeat=CreateTrailLitter(terrain,solids,settings);
        Require(result.reachedBudget,"Actual world did not reach authored litter population");
        Measure(result,terrain,solids,settings);
        Require(result.fingerprint==repeat.fingerprint&&Equal(result.branches,repeat.branches)&&Equal(result.stones,repeat.stones),
            "Fixed seed litter is not byte-value deterministic");
        auto changed=settings;changed.seed^=0x12345678u;
        Require(CreateTrailLitter(terrain,solids,changed).fingerprint!=result.fingerprint,"Changed seed does not alter litter geometry");
        // 合成折坡覆蓋內部；不是僅以平地根部通過宣稱坡面貼合。
        auto slope=terrain;
        for(unsigned z=0;z<slope.Settings().depth;++z)for(unsigned x=0;x<slope.Settings().width;++x) {
            const auto p=slope.SamplePosition(x,z);
            slope.SetHeight(x,z,.12f*p.x+.07f*p.z+((x+z)%2?.035f:-.035f));
        }
        slope.RecalculateRange();const auto draped=CreateTrailLitter(slope,solids,settings);
        Require(draped.reachedBudget,"Folded slope lacks litter");Measure(draped,slope,solids,settings);
        for(unsigned field=0;field<8;++field) {
            auto invalid=settings;const float nan=(std::numeric_limits<float>::quiet_NaN)();
            switch(field){case 0:invalid.minimumZ=nan;break;case 1:invalid.maximumZ=nan;break;
                case 2:invalid.minimumAbsX=nan;break;case 3:invalid.maximumAbsX=nan;break;
                case 4:invalid.patchHalfX=nan;break;case 5:invalid.patchHalfZ=nan;break;
                case 6:invalid.targetPatches=65;break;default:invalid.maximumAttempts=4097;break;}
            Invalid([&](){(void)CreateTrailLitter(terrain,solids,invalid);});
        }
        for(unsigned field=0;field<9;++field) {
            auto invalid=settings;
            switch(field){case 0:invalid.maximumTriangles=0;break;case 1:invalid.maximumTriangles=32769;break;
                case 2:invalid.branchesPerPatch=9;break;case 3:invalid.stonesPerPatch=13;break;
                case 4:invalid.minimumZ=invalid.maximumZ;break;case 5:invalid.patchHalfX=.01f;break;
                case 6:invalid.patchHalfZ=1.f;break;case 7:invalid.minimumAbsX=0;break;default:invalid.maximumAttempts=0;break;}
            Invalid([&](){(void)CreateTrailLitter(terrain,solids,invalid);});
        }
        auto brokenSolid=solids;brokenSolid.front().center.x=(std::numeric_limits<float>::quiet_NaN)();
        Invalid([&](){(void)CreateTrailLitter(terrain,brokenSolid,settings);});
        brokenSolid=solids;brokenSolid.front().extents.x=-1;
        Invalid([&](){(void)CreateTrailLitter(terrain,brokenSolid,settings);});
        std::array<BoxCollider3D,1> blocker{{{{0,0,0},{100,100,100}}}};
        const auto blocked=CreateTrailLitter(terrain,blocker,settings);
        Require(!blocked.reachedBudget&&blocked.patches.empty()&&blocked.branches.vertices.empty()&&blocked.stones.vertices.empty(),
            "Fully blocked world produced a partial patch");
        auto tiny=settings;tiny.maximumTriangles=1;tiny.maximumAttempts=32;
        const auto bounded=CreateTrailLitter(terrain,solids,tiny);
        Require(!bounded.reachedBudget&&bounded.patches.empty()&&bounded.branches.indices.empty()&&bounded.stones.indices.empty(),
            "Triangle-budget rejection committed partial geometry");
        auto zero=settings;zero.targetPatches=0;
        const auto empty=CreateTrailLitter(terrain,solids,zero);
        Require(empty.reachedBudget&&empty.patches.empty()&&empty.branchCount==0&&empty.stoneCount==0,"Zero population not bounded");
        std::cout<<"TrailLitterTest CPU PASS; no GPU or native visual acceptance claimed\n";
        return 0;
    }catch(const std::exception& error){std::cerr<<"TrailLitterTest FAIL: "<<error.what()<<'\n';return 1;}
}
