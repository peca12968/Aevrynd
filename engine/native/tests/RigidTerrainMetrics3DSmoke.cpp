#include <Aevrynd/RigidTerrainMetrics3D.h>
#include <array>
#include <cmath>
#include <cstring>
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace Aevrynd;
namespace {
void Need(bool value,const char* message){if(!value)throw std::runtime_error(message);}
Mesh3D SupportedOverhang(){
    Mesh3D m;
    for(float y:{0.f,1.f})for(float z:{-1.f,1.f})for(float x:{-1.f,1.f}){
        const float radius=y==0?.25f:1.f;Vertex3D v{};v.x=x*radius;v.y=y;v.z=z*radius;m.vertices.push_back(v);
    }
    m.indices={0,1,3,0,3,2,4,6,7,4,7,5,0,4,5,0,5,1,2,3,7,2,7,6,0,2,6,0,6,4,1,5,7,1,7,3};
    return m;
}
}
int main(){try{
    auto mesh=SupportedOverhang();const std::array<std::uint32_t,4> supports{0,1,2,3};
    HeightField3D original({{-3,0,-3},25,25,.25f});original.Fill(.06f);auto deeper=original;
    for(unsigned z:{8u,16u})for(unsigned x:{8u,16u})Need(deeper.SetHeight(x,z,-3.f),"DEEP_ONLY_UNDERWATER_EDIT");
    deeper.RecalculateRange();RigidTerrainMetrics3D a,b,withoutWater;
    Need(TryMeasureRigidTerrainMetrics3D(mesh,{},original,supports,.5f,a),"ORIGINAL_METRICS");
    Need(TryMeasureRigidTerrainMetrics3D(mesh,{},deeper,supports,.5f,b),"DEEP_BED_METRICS");
    Need(a.worldHeight==1&&b.worldHeight==1&&a.maximumEmergence==.5&&b.maximumEmergence==.5,"HEIGHT_AND_EMERGENCE_NOT_BED_GAP");
    Need(a.maximumSupportGap==b.maximumSupportGap&&a.minimumSupportGap==b.minimumSupportGap&&b.maximumSupportGap<0,"SAME_EXPLICIT_LOWER_SUPPORT");
    Need(a.maximumTerrainGap<1&&b.maximumTerrainGap==4&&b.maximumTerrainGap>1.8,"OLD_GAP_PROXY_COUNTEREXAMPLE");
    Need(TryMeasureRigidTerrainMetrics3D(mesh,{},deeper,supports,std::nullopt,withoutWater)&&withoutWater.maximumEmergence==withoutWater.maximumTerrainGap,"NO_WATER_IS_EXPLICIT_TERRAIN_REFERENCE");
    Need(a.vertexCount==8&&a.supportVertexCount==4&&b.vertexCount==8&&b.supportVertexCount==4,"COMPLETE_SOURCE_IDENTITIES");
    RigidTerrainMetrics3D sentinel;sentinel.worldHeight=123;sentinel.vertexCount=999;
    std::array<std::byte,sizeof(sentinel)> bytes{};std::memcpy(bytes.data(),&sentinel,sizeof(sentinel));unsigned negative{};
    const auto rejected=[&](const Mesh3D&m,const Transform3D&p,std::span<const std::uint32_t>s,std::optional<float>w){
        Need(!TryMeasureRigidTerrainMetrics3D(m,p,deeper,s,w,sentinel),"INVALID_INPUT_ACCEPTED");
        Need(std::memcmp(bytes.data(),&sentinel,sizeof(sentinel))==0,"FAILED_MEASUREMENT_MUTATED_OUTPUT");++negative;
    };
    const std::array<std::uint32_t,2> duplicate{0,0};const std::array<std::uint32_t,1> outside{8};
    rejected(mesh,{},duplicate,.5f);rejected(mesh,{},outside,.5f);rejected(mesh,{},std::span<const std::uint32_t>{},.5f);
    rejected(mesh,{},supports,std::numeric_limits<float>::quiet_NaN());
    auto bad=mesh;bad.vertices[7].y=std::numeric_limits<float>::infinity();rejected(bad,{},supports,.5f);
    Transform3D pose;pose.scale.x=0;rejected(mesh,pose,supports,.5f);
    pose={};pose.position.x=100;rejected(mesh,pose,supports,.5f);
    pose={};pose.rotation.y=std::numeric_limits<float>::infinity();rejected(mesh,pose,supports,.5f);
    rejected(Mesh3D{}, {}, supports,.5f);
    pose={{.2f,.4f,.3f},{.1f,.2f,.3f},{1.2f,1.2f,1.2f}};
    Need(TryMeasureRigidTerrainMetrics3D(mesh,pose,deeper,supports,.5f,a),"FINITE_TRANSFORMED_SOURCE");
    float low=1.e6f,high=-1.e6f;for(const auto&v:mesh.vertices){const auto p=TransformPoint(pose.Matrix(),{v.x,v.y,v.z});low=(std::min)(low,p.y);high=(std::max)(high,p.y);}
    Need(a.worldHeight==double(high)-low&&a.vertexCount==8,"TRANSFORMED_WORLD_RANGE_NOT_LOCAL_HEIGHT");
    std::cout<<"RIGID_TERRAIN_METRICS_PASS world_height=1 emergence=0.5 deeper_bed_gap=4 negative_cases="<<negative<<" old_height_gate_not_reclassified=1\n";return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
