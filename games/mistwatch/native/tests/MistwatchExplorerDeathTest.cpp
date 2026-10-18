#include "MistwatchExplorer.h"
#include "MistwatchExplorerNapeHairBinding.h"
#include "MistwatchExplorerBackpack.h"
#include <iostream>
#include <stdexcept>
#include <limits>
#include <bit>
using namespace Aevrynd;
using namespace Mistwatch;
namespace {
void Need(bool good,const char* why){if(!good)throw std::runtime_error(why);}
Explorer Player(){Explorer e;ExplorerNapeHairBinding::Apply(e);BackpackFit1250::Apply(e,{+[](void*){},nullptr});return e;}
std::uint64_t Identity(const Explorer& e){
    std::uint64_t hash=14695981039346656037ull;
    const auto mix=[&](std::uint32_t v){hash^=v;hash*=1099511628211ull;};
    for(const auto& v:e.model.bindMesh.vertices){for(float f:{v.x,v.y,v.z,v.u,v.v})mix(std::bit_cast<std::uint32_t>(f));mix(v.normal);mix(v.color);}
    for(auto i:e.model.bindMesh.indices)mix(i);
    for(const auto& w:e.model.influences)for(unsigned i=0;i<4;++i){mix(w.joints[i]);mix(w.weights[i]);}
    for(const auto& m:e.model.inverseBindMatrices)for(const auto& row:m.values)for(float v:row)mix(std::bit_cast<std::uint32_t>(v));
    for(const auto& j:e.model.skeleton.joints){mix(static_cast<std::uint32_t>(j.parent));for(const auto& row:j.bindTransform.Matrix().values)for(float v:row)mix(std::bit_cast<std::uint32_t>(v));}
    return hash;
}
float Difference(const AnimationPose3D& a,const AnimationPose3D& b){
    float d{};Need(a.globalMatrices.size()==b.globalMatrices.size(),"pose size");
    for(std::size_t i=0;i<a.globalMatrices.size();++i)for(unsigned r=0;r<4;++r)for(unsigned c=0;c<4;++c)
        d=(std::max)(d,std::abs(a.globalMatrices[i][r][c]-b.globalMatrices[i][r][c]));
    return d;
}
void Run(bool bridge,float yaw,bool moving){
    HeightField3D terrain({{-4,0,-4},17,17,.5f});terrain.Fill(bridge?-8.f:0.f);
    std::vector<BoxCollider3D> solids;
    if(bridge)solids.push_back({{0,-.1f,0},{2,.1f,2}});
    Explorer e=Player();e.Update(moving?Vec3{0,0,.025f}:Vec3{},1.f/60.f,false);e.yaw=yaw;
    Need(e.PlaceFeet({},true,false,terrain,solids),"initial feet");
    const auto original=e.pose;const auto identity=Identity(e);const auto compatibility=e.model.skeletonCompatibilityId;
    const float headStart=TransformPoint(original.globalMatrices[2],{}).y;
    const float pelvisStart=TransformPoint(original.globalMatrices[0],{}).y;
    Need(e.UpdateDeath(0,{},terrain,solids)&&Difference(original,e.pose)==0,"start continuity");
    Need(!e.DeathSupported(),"starting pose cannot claim supported collapse");
    float maximumStep{};
    for(unsigned tick=0;tick<61;++tick){
        const auto before=e.pose;
        if(!e.UpdateDeath(1.f/60.f,{},terrain,solids)){
            std::cerr<<"tick="<<tick<<" progress="<<e.DeathProgress()<<'\n';throw std::runtime_error("death update rejected");
        }
        maximumStep=(std::max)(maximumStep,Difference(before,e.pose));
        for(const auto& matrix:e.pose.globalMatrices)for(const auto& row:matrix.values)for(float value:row)Need(std::isfinite(value),"nonfinite posed matrix");
        Need(e.DeathMinimumClearance()>=-.002f,"intermediate real surface penetrated support");
        if(e.DeathLimited()){
            float low=100;std::size_t at{};for(std::size_t i=0;i<e.deathSurface.vertices.size();++i)
                if(e.deathSurface.vertices[i].y<low){low=e.deathSurface.vertices[i].y;at=i;}
            const auto& v=e.model.bindMesh.vertices[at];
            std::cerr<<"limited tick="<<tick<<" progress="<<e.DeathProgress()<<" candidateBottom="<<low
                <<" vertex="<<at<<" source="<<v.x<<','<<v.y<<','<<v.z<<'\n';
            break;
        }
    }
    SkinningWorkspace3D workspace;Mesh3D surface;Need(SkinModelInto(e.model,e.pose,workspace,surface),"skin");
    float bottom=100,top=-100;std::array<float,4> contacts{100,100,100,100};
    for(std::size_t i=0;i<surface.vertices.size();++i){const auto& v=surface.vertices[i];
        bottom=(std::min)(bottom,v.y);top=(std::max)(top,v.y);
        const auto& source=e.model.bindMesh.vertices[i];
        if(source.y>.40f&&source.y<.56f)contacts[source.x<0?0:1]=(std::min)(contacts[source.x<0?0:1],v.y);
        for(unsigned side=0;side<2;++side)if(e.model.influences[i].joints[0]==5+side*3&&e.model.influences[i].weights[0]==65535)
            contacts[2+side]=(std::min)(contacts[2+side],v.y);
    }
    std::cout<<"bridge="<<bridge<<" yaw="<<yaw<<" moving="<<moving<<" bottom="<<bottom<<" top="<<top<<" maxStep="<<maximumStep<<" contacts=";
    for(float contact:contacts)std::cout<<contact<<',';
    std::cout<<'\n';
    Need(e.DeathFinished()&&e.DeathSupported()&&!e.DeathLimited()&&e.model.skeletonCompatibilityId==compatibility&&Identity(e)==identity,"identity or finish");
    Need(bottom>=-.002f&&top<1.f&&maximumStep<.10f,"surface clearance or collapse continuity");
    for(float contact:contacts)Need(contact>=-.002f&&contact<.04f,"knees and hands need real surface support");
    Need(TransformPoint(e.pose.globalMatrices[0],{}).y<pelvisStart-.25f&&TransformPoint(e.pose.globalMatrices[2],{}).y<headStart-.6f,"pelvis or head did not collapse");
    for(unsigned side=0;side<2;++side){
        const auto hip=TransformPoint(e.pose.globalMatrices[9+side*3],{}),knee=TransformPoint(e.pose.globalMatrices[10+side*3],{}),ankle=TransformPoint(e.pose.globalMatrices[11+side*3],{});
        Need(knee.z>hip.z+.15f&&ankle.z<knee.z-.2f,"knees bend backwards instead of supported kneeling");
    }
    const auto final=e.pose;const auto progress=e.DeathProgress();
    Need(e.UpdateDeath(1.f/60.f,{},terrain,solids)&&Difference(final,e.pose)==0&&e.DeathProgress()==progress,"terminal pose drifted");
    // 暫停契約是呼叫端不更新；讀取狀態不得推進动画。
    Need(e.DeathFinished()&&Difference(final,e.pose)==0,"paused observation changed pose");
    auto single=Player();single.pose=original;single.yaw=yaw;
    Need(single.UpdateDeath(Explorer::DeathDuration,{},terrain,solids)&&single.DeathFinished()&&Difference(single.pose,final)<.00001f,"single-step loaded terminal differs");
    e.ResetDeath();e.Update({},0,false);Need(e.PlaceFeet({},true,false,terrain,solids),"reset feet");
    Need(!e.DeathFinished()&&!e.DeathLimited()&&!e.DeathSupported()&&e.DeathProgress()==0&&e.deathStart.localTransforms.empty()&&e.deathEnd.localTransforms.empty(),"reset retained death state");
    Need(TransformPoint(e.pose.globalMatrices[2],{}).y>1.4f&&Identity(e)==identity,"reset kept slumped skeleton or changed asset");
}
void EdgeCases(){
    HeightField3D terrain({{-4,0,-4},17,17,.5f});terrain.Fill(0);
    auto e=Player();const auto start=e.pose;
    for(float dt:{-1.f,1.01f,(std::numeric_limits<float>::infinity)(),(std::numeric_limits<float>::quiet_NaN)()}){
        Need(!e.UpdateDeath(dt,{},terrain,{})&&!e.deathActive&&Difference(start,e.pose)==0,"invalid delta changed pose");
    }
    auto invalid=e;invalid.pose.globalMatrices.clear();Need(!invalid.UpdateDeath(.1f,{},terrain,{}),"missing global pose accepted");
    auto hierarchy=e;hierarchy.model.skeleton.joints[1].parent=99;Need(!hierarchy.UpdateDeath(.1f,{},terrain,{}),"invalid hierarchy accepted");
    auto nanPose=e;nanPose.pose.localTransforms[1].translation.x=(std::numeric_limits<float>::quiet_NaN)();
    Need(!nanPose.UpdateDeath(.1f,{},terrain,{}),"nonfinite local pose accepted");
    auto zeroRotation=e;zeroRotation.pose.localTransforms[1].usesQuaternion=true;zeroRotation.pose.localTransforms[1].quaternion={0,0,0,0};
    Need(!zeroRotation.UpdateDeath(.1f,{},terrain,{}),"zero quaternion accepted");
    const std::array<BoxCollider3D,1> nanSolid{{{{0,(std::numeric_limits<float>::quiet_NaN)(),0},{1,1,1}}}};
    Need(!e.UpdateDeath(.1f,{},terrain,nanSolid)&&Difference(start,e.pose)==0,"nonfinite solid accepted");
    auto air=Player();Need(air.UpdateDeath(1,{0,3,0},terrain,{})&&air.DeathFinished()&&!air.DeathSupported()&&!air.DeathLimited(),"airborne dead save rejected or claimed supported");
    Need(air.DeathMinimumClearance()>2.f,"airborne pose snapped to ground");
    auto blocked=Player();const auto old=blocked.pose;
    const std::array<BoxCollider3D,1> obstacle{{{{0,.35f,.55f},{.5f,.35f,.20f}}}};
    Need(blocked.UpdateDeath(1,{},terrain,obstacle)&&blocked.DeathLimited()&&!blocked.DeathFinished()&&!blocked.DeathSupported()&&Difference(old,blocked.pose)==0,"obstructed legal location crashed, penetrated or claimed full collapse");
    auto partial=Player();Need(partial.UpdateDeath(.3f,{},terrain,{}),"partial baseline");const auto safe=partial.pose;
    Need(partial.UpdateDeath(.7f,{},terrain,obstacle)&&partial.DeathLimited()&&Difference(safe,partial.pose)==0,"limited pose did not preserve last safe pose");
    auto tiny=Player();Need(tiny.PlaceFeet({},true,false,terrain,{}),"tiny start feet");const auto tinyStart=tiny.pose;
    Need(tiny.UpdateDeath(.00001f,{},terrain,{})&&Difference(tinyStart,tiny.pose)<.0001f,"small delta caused discontinuous snap");
    HeightField3D slope({{-4,0,-4},17,17,.5f});
    for(unsigned z=0;z<17;++z)for(unsigned x=0;x<17;++x)slope.SetHeight(x,z,(float(z)*.5f-4.f)*.5f);
    auto hill=Player();Need(hill.PlaceFeet({},true,false,slope,{}),"slope baseline feet");
    Need(hill.UpdateDeath(1,{},slope,{})&&(hill.DeathLimited()||hill.DeathFinished()),"legal slope caused fatal failure");
    if(hill.DeathLimited())Need(!hill.DeathSupported(),"limited slope claims supported collapse");
    std::cout<<"EXPLORER_DEATH_EDGES_PASS air_supported=0 obstacle_limited=1 slope_limited="<<hill.DeathLimited()<<'\n';
}
// 保留原死亡動作測試；以下直接驗證正式保守索引 helper 的容量、接觸與回退契約。
// 非有限頂點優先順序使用合成 helper＋原順序 reducer；不是完整 production NaN 或場景證據。
void DeathSupportFilterCases(){
    using ExplorerDeathSupportDetail::Build;Mesh3D mesh;mesh.vertices.push_back({1,0,1});mesh.vertices.push_back({2,1,2});
    const auto identity=CreateTransformMatrix({}, {}, {1,1,1});
    std::vector<BoxCollider3D> boxes(128,{{1.5f,-10,1.5f},{1,.1f,1}});
    auto selection=Build(mesh,identity,boxes);Need(selection.filtered&&selection.count==128,"128 boundary");
    boxes.push_back(boxes.back());selection=Build(mesh,identity,boxes);Need(!selection.filtered&&selection.Size(boxes)==129,"129 fallback");
    unsigned boundaryCases{};
    for(float center:{-10000000.f,-2.f,0.f,2.f,10000000.f})for(float extent:{0.f,.002f,1.f,10000000.f}){
        const float boundary=center+extent;
        for(float point:{std::nextafter(boundary,-INFINITY),boundary,std::nextafter(boundary,INFINITY)}){
            Mesh3D pointMesh;pointMesh.vertices.push_back({point,0,point});
            const std::array<BoxCollider3D,3> ordered{{{{center,0,center},{extent,1,extent}},{{point,0,point},{0,0,0}},{{-100,-100,-100},{1,1,1}}}};
            const auto chosen=Build(pointMesh,identity,ordered);Need(chosen.filtered,"finite boundary filtered");
            std::size_t previous=0;bool first=true;
            for(std::size_t n=0;n<chosen.count;++n){Need(first||chosen.indices[n]>previous,"original source order");previous=chosen.indices[n];first=false;}
            for(std::size_t i=0;i<ordered.size();++i){const auto& b=ordered[i];
                const bool raw=std::abs(point-b.center.x)<=b.extents.x&&std::abs(point-b.center.z)<=b.extents.z;
                if(raw){bool found=false;for(std::size_t n=0;n<chosen.count;++n)found|=chosen.indices[n]==i;Need(found,"touch ULP false clear");}}
            ++boundaryCases;
        }
    }
    Mesh3D huge;huge.vertices.push_back({(std::numeric_limits<float>::max)(),0,0});
    const std::array<BoxCollider3D,1> opposite{{{{-(std::numeric_limits<float>::max)(),0,0},{1,1,1}}}};
    Need(!Build(huge,identity,opposite).filtered,"nonfinite endpoint subtraction fallback");
    Mesh3D late;late.vertices.push_back({0,-1,0});late.vertices.push_back({(std::numeric_limits<float>::quiet_NaN)(),0,0});
    const std::array<BoxCollider3D,1> irrelevant{{{{100,0,100},{1,1,1}}}};
    const auto lateSelection=Build(late,identity,irrelevant);Need(!lateSelection.filtered,"late invalid fallback");
    const auto reduce=[&](const Mesh3D& m){for(const auto& v:m.vertices){const auto p=TransformPoint(identity,{v.x,v.y,v.z});
        if(!std::isfinite(p.x)||!std::isfinite(p.y)||!std::isfinite(p.z))return 2;
        if(p.y<-.002f)return 1;}return 0;};
    Need(reduce(late)==1,"late invalid preempted earlier limited");std::reverse(late.vertices.begin(),late.vertices.end());
    Need(!Build(late,identity,irrelevant).filtered&&reduce(late)==2,"early invalid order");
    Need(!Build({},identity,{}).filtered,"empty surface fallback");
    std::cout<<"FILTER_CPU_PASS full128=1 fallback129=1 touch_ulp="<<boundaryCases
        <<" original_order=1 nonfinite_subtract_fallback=1 late_nonfinite_fallback_order=2 empty_fallback=1\n";
}
}
int main()try{for(float yaw:{0.f,1.57079632679f,3.14159265359f})for(bool bridge:{false,true})Run(bridge,yaw,false);Run(false,0,true);EdgeCases();DeathSupportFilterCases();std::cout<<"EXPLORER_DEATH_CPU_PASS native_visual_evidence=0\n";return 0;}
catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
