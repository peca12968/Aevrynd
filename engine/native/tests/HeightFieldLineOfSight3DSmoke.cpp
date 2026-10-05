#include <Aevrynd/HeightFieldLineOfSight3D.h>
#include <Aevrynd/Geometry3D.h>
#include <iostream>
#include <limits>
#include <stdexcept>
namespace {
using namespace Aevrynd;
unsigned checks{};
void Need(bool value,const char* reason){++checks;if(!value)throw std::runtime_error(reason);}
void Segments(){
    const Vec3 center{1,0,0},extents{.0000001f,1,1};
    Need(SegmentIntersectsAabb3D({0,0,0},{2,0,0},center,extents),"thin box missed");
    Need(!SegmentIntersectsAabb3D({0,0,0},{.5f,0,0},center,extents),"segment extended behind endpoint");
    Need(SegmentIntersectsAabb3D({0,0,0},{1,0,0},center,extents),"endpoint contact missed");
    Need(SegmentIntersectsAabb3D(center,center,center,extents),"zero length inside missed");
    Need(!SegmentIntersectsAabb3D({0,0,0},{0,0,0},center,extents),"zero length outside hit");
    Need(SegmentIntersectsAabb3D({0,0,0},{.00000008f,0,0},{.00000004f,0,0},{.000000001f,1,1}),"micro segment normalized away");
    Need(!SegmentIntersectsAabb3D({0,2,0},{2,2,0},center,extents),"parallel segment false hit");
    Need(SegmentIntersectsAabb3D({0,0,0},{2,0,0},center,{-1,1,1}),"invalid bounds did not fail closed");
}
void Terrain(){
    using Status=HeightFieldLineStatus3D;
    HeightField3D t({{0,0,0},2,2,1});t.Fill(0);
    Need(QueryHeightFieldLine3D({0,.65f,0},{1,.65f,1},t)==Status::Clear,"flat clear failed");
    Need(QueryHeightFieldLine3D({0,0,0},{1,.65f,1},t)==Status::Occluded,"ground contact missed");
    Need(QueryHeightFieldLine3D({.5f,1,.5f},{.5f,-1,.5f},t)==Status::Occluded,"vertical crossing missed");
    Need(QueryHeightFieldLine3D({.5f,1,.5f},{.5f,1,.5f},t)==Status::Clear,"stationary above terrain failed");
    t.SetHeight(1,1,4);
    Need(QueryHeightFieldLine3D({.1f,.65f,.1f},{.4f,.65f,.2f},t)==Status::Clear,"unused high corner caused false cover");
    t.Fill(0);t.SetHeight(1,0,2);t.SetHeight(0,1,2);
    Need(QueryHeightFieldLine3D({0,1.5f,0},{1,1.5f,1},t)==Status::Occluded,"triangle diagonal peak missed by bilinear model");
    Need(QueryHeightFieldLine3D({1,1.5f,1},{0,1.5f,0},t)==Status::Occluded,"reversed diagonal peak missed");
    Need(QueryHeightFieldLine3D({0,2.1f,0},{1,2.1f,1},t)==Status::Clear,"clear diagonal blocked");
    Need(QueryHeightFieldLine3D({-.01f,3,0},{1,3,1},t)==Status::OutOfBounds,"outside endpoint clamped");
    t.SetHeight(1,0,std::numeric_limits<float>::quiet_NaN());
    Need(QueryHeightFieldLine3D({0,3,0},{1,3,1},t)==Status::InvalidInput,"nonfinite corner accepted");
    HeightField3D large({{0,0,0},70,70,1});large.Fill(0);
    Need(QueryHeightFieldLine3D({0,1,0},{69,1,69},large)==Status::BudgetExceeded,"cell budget bypassed");
    HeightField3D translated({{1000000,0,1000000},2,2,.3f});translated.Fill(0);
    const auto edge=translated.SamplePosition(1,0);
    Need(QueryHeightFieldLine3D(edge,edge,translated)==Status::Occluded,"native rounded mesh endpoint missed");
    HeightField3D fractional({{.1f,0,0},3,2,.1f});fractional.Fill(10);
    const auto tip=fractional.SamplePosition(2,0);
    Need(QueryHeightFieldLine3D({tip.x,0,tip.z},{tip.x,0,tip.z},fractional)==Status::Occluded,"fractional cell outer endpoint uncovered");
    Need(QueryHeightFieldLine3D({tip.x,11,tip.z},{tip.x,11,tip.z},fractional)==Status::Clear,"fractional cell clear endpoint rejected");
    HeightField3D collapsed({{100000000,0,100000000},2,2,.05f});collapsed.Fill(0);
    Need(QueryHeightFieldLine3D({100000000,1,100000000},{100000000,1,100000000},collapsed)==Status::InvalidInput,"collapsed native mesh cell accepted");
    const auto nan=std::numeric_limits<float>::quiet_NaN();
    Need(QueryHeightFieldLine3D({nan,1,0},{1,1,1},fractional)==Status::InvalidInput,"nonfinite endpoint accepted");
}
void TriangleOracle(){
    HeightField3D t({{0,0,0},3,3,1});
    for(unsigned z=0;z<3;++z)for(unsigned x=0;x<3;++x)t.SetHeight(x,z,float((x*11+z*7)%9)*.3f-.6f);
    unsigned seed=173;
    const auto random=[&](){seed=seed*1664525u+1013904223u;return float(seed>>8)/16777216.f;};
    for(unsigned i=0;i<1000;++i){
        const Vec3 a{random()*2,random()*3-.5f,random()*2},b{random()*2,random()*3-.5f,random()*2};
        bool blocked=a.y<=t.SampleHeight(a.x,a.z)||b.y<=t.SampleHeight(b.x,b.z);
        const auto shift=b-a;const float length=Length(shift);const Ray3D ray{a,shift*(1.f/length)};
        for(unsigned z=0;z<2;++z)for(unsigned x=0;x<2;++x){
            const auto p=t.SamplePosition(x,z),q=t.SamplePosition(x+1,z),r=t.SamplePosition(x,z+1),s=t.SamplePosition(x+1,z+1);
            blocked|=bool(RayIntersectsTriangle(ray,p,r,q,length))||bool(RayIntersectsTriangle(ray,q,r,s,length));
        }
        const auto status=QueryHeightFieldLine3D(a,b,t);
        Need((status==HeightFieldLineStatus3D::Occluded)==blocked,"independent native triangle ray oracle differs");
        Need(status==HeightFieldLineStatus3D::Clear||status==HeightFieldLineStatus3D::Occluded,"finite oracle query invalid");
    }
}
}
int main(){try{Segments();Terrain();TriangleOracle();std::cout<<"HEIGHTFIELD_LINE_PASS checks="<<checks<<" triangle_oracle=1000\n";return 0;}
catch(const std::exception& error){std::cerr<<"FIRST_DIVERGENCE "<<error.what()<<" checks="<<checks<<'\n';return 1;}}
