#include "MistwatchRepairShelterVisual.h"
#include <bit>
#include <iostream>
#include <set>
using namespace Aevrynd;

namespace {
void Need(bool value,const char* message){if(!value)throw std::runtime_error(message);}
bool Same(Vec3 a,Vec3 b){return a.x==b.x&&a.y==b.y&&a.z==b.z;}
}

int main(){try{
    const auto before=Mistwatch::RepairShelterSolids();
    const auto a=Mistwatch::CreatePlankedRepairShelter(),b=Mistwatch::CreatePlankedRepairShelter();
    Need(before.size()==5,"original five collision solids");
    Need(a.Valid()&&a.vertices.size()==7392&&a.indices.size()==10164,"actual fixed board geometry");
    Need(a.indices==b.indices&&a.vertices.size()==b.vertices.size(),"same input determinism");
    std::size_t begin=0;std::set<std::uint32_t> colors;
    for(std::size_t part=0;part<before.size();++part){
        const auto box=before[part];const auto end=begin+Mistwatch::ShelterBoardCounts[part]*96u;
        for(std::size_t i=begin;i<end;++i){const auto& v=a.vertices[i];const auto& w=b.vertices[i];
            Need(std::bit_cast<std::uint32_t>(v.x)==std::bit_cast<std::uint32_t>(w.x)&&
                 std::bit_cast<std::uint32_t>(v.y)==std::bit_cast<std::uint32_t>(w.y)&&
                 std::bit_cast<std::uint32_t>(v.z)==std::bit_cast<std::uint32_t>(w.z)&&
                 std::bit_cast<std::uint32_t>(v.u)==std::bit_cast<std::uint32_t>(w.u)&&
                 std::bit_cast<std::uint32_t>(v.v)==std::bit_cast<std::uint32_t>(w.v)&&
                 v.color==w.color&&v.normal==w.normal,"all vertex fields repeat bit-exact");
            Need(std::isfinite(v.x)&&std::isfinite(v.y)&&std::isfinite(v.z)&&
                 std::isfinite(v.u)&&std::isfinite(v.v),"finite mesh");
            Need(v.x>=box.center.x-box.extents.x&&v.x<=box.center.x+box.extents.x&&
                 v.y>=box.center.y-box.extents.y&&v.y<=box.center.y+box.extents.y&&
                 v.z>=box.center.z-box.extents.z&&v.z<=box.center.z+box.extents.z,
                 "original collision exact containment");
            colors.insert(v.color);
        }begin=end;
    }
    Need(begin==a.vertices.size()&&colors.size()==7,"complete boards and bounded tint variation");
    for(std::size_t i=0;i<a.indices.size();i+=3){
        const auto& x=a.vertices[a.indices[i]];const auto& y=a.vertices[a.indices[i+1]];
        const auto& z=a.vertices[a.indices[i+2]];
        Vec3 n{};Need(TryComputeTriangleNormal({x.x,x.y,x.z},{y.x,y.y,y.z},{z.x,z.y,z.z},n),"nonzero actual face");
        const auto packed=UnpackNormalSnorm8(x.normal);
        Need(Dot(n,{packed.x,packed.y,packed.z})>.99f,"true geometric packed normal");
        const double jac=(double(y.u)-x.u)*(double(z.v)-x.v)-(double(z.u)-x.u)*(double(y.v)-x.v);
        Need(std::isfinite(jac)&&jac!=0,"actual signed UV jacobian nonzero");
    }
    const auto after=Mistwatch::RepairShelterSolids();
    Need(before.size()==after.size(),"collision membership unchanged");
    for(std::size_t i=0;i<before.size();++i)
        Need(Same(before[i].center,after[i].center)&&Same(before[i].extents,after[i].extents),"source collision unchanged");
    std::cout<<"SHELTER_CPU_PASS boards=77 vertices="<<a.vertices.size()<<" indices="<<a.indices.size()
             <<" triangles="<<a.indices.size()/3<<" original_solids=5 native=false art=false\n";
    return 0;
}catch(const std::exception& e){std::cerr<<"FIRST_DIVERGENCE "<<e.what()<<'\n';return 1;}}
