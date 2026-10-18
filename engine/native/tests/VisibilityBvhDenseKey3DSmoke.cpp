#include <Aevrynd/VisibilityBvh3D.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <new>
#include <stdexcept>
#include <vector>

namespace AllocationProbe {
thread_local std::size_t remaining=(std::numeric_limits<std::size_t>::max)();
thread_local std::size_t injected{};
thread_local std::size_t allocations{};
void Arm(std::size_t ordinal){remaining=ordinal;}
void Disarm(){remaining=(std::numeric_limits<std::size_t>::max)();}
}
void* operator new(std::size_t size){
    if(AllocationProbe::remaining!=(std::numeric_limits<std::size_t>::max)()){
        if(!AllocationProbe::remaining){AllocationProbe::Disarm();++AllocationProbe::injected;throw std::bad_alloc{};}
        --AllocationProbe::remaining;
    }
    if(void* memory=std::malloc(size?size:1)){++AllocationProbe::allocations;return memory;}
    throw std::bad_alloc{};
}
void* operator new[](std::size_t size){return ::operator new(size);}
void operator delete(void* memory)noexcept{std::free(memory);}
void operator delete(void* memory,std::size_t)noexcept{std::free(memory);}
void operator delete[](void* memory)noexcept{std::free(memory);}
void operator delete[](void* memory,std::size_t)noexcept{std::free(memory);}

using namespace Aevrynd;
namespace {
std::size_t checks{};
std::array<std::size_t,3> denseAllocations{},genericAllocations{};
std::size_t denseFaultOrdinals{},genericFaultOrdinals{};
void Need(bool value,const char* reason){++checks;if(!value)throw std::runtime_error(reason);}
bool SameStats(const VisibilityBvhStats3D& a,const VisibilityBvhStats3D& b){return a.objects==b.objects&&a.nodes==b.nodes&&a.candidates==b.candidates&&a.nodesTested==b.nodesTested&&a.nodesRejected==b.nodesRejected&&a.distanceRejectedObjects==b.distanceRejectedObjects&&a.frustumRejectedObjects==b.frustumRejectedObjects&&a.refittedLeaves==b.refittedLeaves&&a.builds==b.builds&&a.refits==b.refits&&a.queries==b.queries;}
bool SameMemory(const ReusableMemoryStats& a,const ReusableMemoryStats& b){return a.retainedCapacityBytes==b.retainedCapacityBytes&&a.reclaimableCapacityBytes==b.reclaimableCapacityBytes&&a.lastReleasedBytes==b.lastReleasedBytes&&a.trimCount==b.trimCount;}
VisibilityBvhQuery3D All(){return {Camera3D{{0,0,0},{0,0,1}},16.f/9.f,0,0,false};}
std::vector<VisibilityBvhItem3D> Items(std::size_t count,std::uint64_t base=100,std::uint64_t spacing=1){
    std::vector<VisibilityBvhItem3D> result;result.reserve(count);
    for(std::size_t i=0;i<count;++i)result.push_back({base+spacing*i,{{float(i%64)*4.f-128.f,0,float(i/64)*4.f+8.f},.5f,{.25f,.25f,.25f}},std::uint32_t((i*37)%count)});
    return result;
}
void Members(VisibilityBvh3D& tree,std::span<const VisibilityBvhItem3D> values){
    std::vector<std::uint32_t> expected;expected.reserve(values.size());for(const auto& value:values)expected.push_back(value.payload);std::sort(expected.begin(),expected.end());
    const auto found=tree.Query(All());Need(found.size()==expected.size(),"all query count");Need(std::equal(found.begin(),found.end(),expected.begin()),"all query sorted payload words");
}
void LinearQueries(VisibilityBvh3D& tree,std::span<const VisibilityBvhItem3D> values){
    const std::array<VisibilityBvhQuery3D,5> queries{{All(),{Camera3D{{-100,2,0},{-100,2,1}},16.f/9.f,35,0,true},{Camera3D{{-100,0,8},{-99,0,8}},1,12,0,false},{Camera3D{{-120,1,20},{-119,1,21}},1,35,7,true},{Camera3D{{100,1,20},{100,1,21}},2,40,0,false}}};
    for(const auto& query:queries){
        std::vector<std::uint32_t> expected;
        for(const auto& item:values){const float distance=Length(item.bounds.center-query.camera.position)-item.bounds.radius;if(query.shadowDistance>0&&distance<=query.shadowDistance){expected.push_back(item.payload);continue;}if(query.maximumDistance>0&&distance>query.maximumDistance)continue;if(query.frustumCulling&&!SphereIntersectsFrustum(item.bounds.center,item.bounds.radius,query.camera,query.aspect))continue;expected.push_back(item.payload);}
        std::sort(expected.begin(),expected.end());const auto found=tree.Query(query);Need(found.size()==expected.size(),"linear query count");Need(std::equal(found.begin(),found.end(),expected.begin()),"linear query payload words");
    }
}
void MoveAndRestore(VisibilityBvh3D& tree,std::span<const VisibilityBvhItem3D> values){
    Need(values.size()>1,"move fixture too small");auto moved=values[1].bounds;moved.center={0,0,0};
    Need(tree.Update(values[1].key,moved)&&tree.Refit(),"retained key cannot update/refit");const auto local=tree.Query({Camera3D{{0,0,0},{0,0,1}},1,1,0,false});Need(local.size()==1&&local[0]==values[1].payload,"sorted entry lookup targets wrong payload");
    Need(tree.Update(values[1].key,values[1].bounds)&&tree.Refit(),"key could not restore");Members(tree,values);
}
void RejectUnchanged(VisibilityBvh3D& tree,std::span<const VisibilityBvhItem3D> bad,std::span<const VisibilityBvhItem3D> old){
    const auto stats=tree.Stats();const auto memory=tree.MemoryStats();Need(!tree.Build(bad),"invalid build accepted");Need(SameStats(stats,tree.Stats()),"failed build stats changed");Need(SameMemory(memory,tree.MemoryStats()),"failed build memory changed");Members(tree,old);MoveAndRestore(tree,old);
}
void AllocationCounts(){
    auto dense=Items(4096);VisibilityBvh3D tree;
    for(std::size_t round=0;round<denseAllocations.size();++round){const auto start=AllocationProbe::allocations;const bool built=tree.Build(dense);denseAllocations[round]=AllocationProbe::allocations-start;Need(built,"dense4096 build");Need(denseAllocations[round]>0&&denseAllocations[round]<=16,"dense4096 allocation budget");Need(denseAllocations[round]<=dense.size()+32,"original ordinary build allocation budget");Members(tree,dense);}
    Need(denseAllocations[0]==denseAllocations[1]&&denseAllocations[1]==denseAllocations[2],"dense repeated allocation count not constant");LinearQueries(tree,dense);MoveAndRestore(tree,dense);
    auto generic=Items(64,20000,2);
    for(std::size_t round=0;round<genericAllocations.size();++round){const auto start=AllocationProbe::allocations;const bool built=tree.Build(generic);genericAllocations[round]=AllocationProbe::allocations-start;Need(built,"generic odd-spaced build");Need(genericAllocations[round]>=generic.size()&&genericAllocations[round]<=generic.size()+32,"generic per-key map path allocation count");Members(tree,generic);}
    LinearQueries(tree,generic);MoveAndRestore(tree,generic);
    const auto stats=tree.Stats();const auto memory=tree.MemoryStats();Need(!tree.Update(dense[1].key,dense[1].bounds),"old dense key survived generic publication");Need(SameStats(stats,tree.Stats())&&SameMemory(memory,tree.MemoryStats()),"missing key changed state");
    // key 連續與否和輸入順序、payload 順序無關。
    std::reverse(dense.begin(),dense.end());Need(tree.Build(dense),"reversed dense keys");Members(tree,dense);MoveAndRestore(tree,dense);
    const auto injected=AllocationProbe::injected;AllocationProbe::Arm(0);const auto warm=tree.Query(All());AllocationProbe::Disarm();Need(AllocationProbe::injected==injected&&warm.size()==dense.size(),"warm query allocated");
}
void KeyDomains(){
    const auto maximum=(std::numeric_limits<VisibilityEntityKey3D>::max)();auto values=Items(8,maximum-7);VisibilityBvh3D tree;Need(tree.Build(values),"UINT64_MAX ending dense range");Members(tree,values);MoveAndRestore(tree,values);
    auto moved=values.back().bounds;moved.center={0,0,0};Need(tree.Update(maximum,moved)&&tree.Refit(),"UINT64_MAX dense update");const auto local=tree.Query({Camera3D{{0,0,0},{0,0,1}},1,1,0,false});Need(local.size()==1&&local[0]==values.back().payload,"UINT64_MAX maps to wrong entry");Need(tree.Update(maximum,values.back().bounds)&&tree.Refit(),"UINT64_MAX restore");
    const auto stats=tree.Stats();const auto memory=tree.MemoryStats();Need(!tree.Update(maximum-8,moved)&&!tree.Update(1,moved)&&!tree.Update(0,moved),"dense range check wrapped");Need(SameStats(stats,tree.Stats())&&SameMemory(memory,tree.MemoryStats()),"dense missing range changed state");
    std::array<VisibilityBvhItem3D,1> singleton{{{maximum,{{1,0,8},0,{}},17}}};Need(tree.Build(singleton),"UINT64_MAX singleton");Members(tree,singleton);Need(tree.Update(maximum,singleton[0].bounds)&&tree.Refit(),"singleton index");
    auto holes=Items(8,1000);holes.back().key+=2;Need(tree.Build(holes),"valid keys with holes rejected");Members(tree,holes);Need(!tree.Update(1007,holes[0].bounds),"generic hole accepted");MoveAndRestore(tree,holes);
    std::array<VisibilityBvhItem3D,2> wide{{{1,{{-2,0,8},.5f,{.5f,.5f,.5f}},91},{maximum,{{2,0,8},.5f,{.5f,.5f,.5f}},37}}};Need(tree.Build(wide),"full uint64 sparse domain");Members(tree,wide);Need(tree.Update(maximum,wide[1].bounds)&&tree.Refit(),"sparse maximum key");Need(!tree.Update(2,wide[0].bounds),"sparse hole accepted");
    auto duplicatePayload=Items(8,2000);duplicatePayload[2].payload=duplicatePayload[3].payload;Need(tree.Build(duplicatePayload),"valid duplicate payload rejected");Members(tree,duplicatePayload);MoveAndRestore(tree,duplicatePayload);
    tree.Clear();Need(tree.Query(All()).empty()&&tree.Stats().objects==0&&tree.Stats().nodes==0,"clear published dense contents");Need(!tree.Update(duplicatePayload[1].key,duplicatePayload[1].bounds),"clear did not invalidate dense base/index");Need(tree.Build(std::span<const VisibilityBvhItem3D>{})&&tree.Query(All()).empty(),"empty build");
}
void InvalidBuilds(){
    for(bool generic:{false,true}){
        auto old=Items(16,generic?4000u:2000u,generic?2u:1u);VisibilityBvh3D tree;Need(tree.Build(old),"invalid controls seed");
        for(unsigned control=0;control<10;++control){
            auto bad=Items(16,10000,control==1?2u:1u);
            switch(control){
            case 0:bad[4].key=bad[3].key;break;
            case 1:bad[4].key=bad[3].key;break;
            case 2:bad.back().key=0;break;
            case 3:bad.back().bounds.center.x=(std::numeric_limits<float>::quiet_NaN)();break;
            case 4:bad.back().bounds.radius=(std::numeric_limits<float>::infinity)();break;
            case 5:bad.back().bounds.extents.y=-1.f;break;
            case 6:bad.back().bounds.radius=-1.f;break;
            case 7:bad.back().bounds.extents.x=(std::numeric_limits<float>::infinity)();break;
            case 8:bad.front().bounds.center.x=(std::numeric_limits<float>::max)();bad.back().bounds.center.x=-(std::numeric_limits<float>::max)();break;
            default:bad.back().bounds.center.y=-(std::numeric_limits<float>::infinity)();break;
            }
            RejectUnchanged(tree,bad,old);
        }
        auto invalid=old[0].bounds;invalid.radius=-1;const auto stats=tree.Stats();const auto memory=tree.MemoryStats();Need(!tree.Update(old[0].key,invalid),"invalid update accepted");Need(SameStats(stats,tree.Stats())&&SameMemory(memory,tree.MemoryStats()),"invalid update changed state");MoveAndRestore(tree,old);
    }
}
std::size_t EnumerateBuildFailures(bool generic){
    auto old=Items(4,50,generic?1u:2u);auto target=Items(12,1000,generic?2u:1u);VisibilityBvh3D tree;Need(tree.Build(old),"failure enumeration seed");
    for(std::size_t ordinal=0;ordinal<128;++ordinal){
        const auto stats=tree.Stats();const auto memory=tree.MemoryStats();const auto injected=AllocationProbe::injected;const auto start=AllocationProbe::allocations;
        AllocationProbe::Arm(ordinal);const bool built=tree.Build(target);AllocationProbe::Disarm();const auto allocated=AllocationProbe::allocations-start;
        if(built){Need(AllocationProbe::injected==injected,"build succeeded despite injected failure");Need(allocated==ordinal,"first success did not reach measured allocation boundary");Members(tree,target);MoveAndRestore(tree,target);Need(!tree.Update(old[1].key,old[1].bounds),"old index survived successful replacement");return ordinal;}
        Need(AllocationProbe::injected==injected+1,"build failed without exercising armed allocation");Need(allocated==ordinal,"failed allocation ordinal not reached");Need(SameStats(stats,tree.Stats()),"enumerated failure changed stats");Need(SameMemory(memory,tree.MemoryStats()),"enumerated failure changed memory");Members(tree,old);MoveAndRestore(tree,old);
    }
    throw std::runtime_error("allocation enumeration did not terminate within finite fixture bound");
}
void MemoryAndTrim(){
    for(bool generic:{false,true}){
        auto values=Items(128,generic?5000u:2000u,generic?2u:1u);VisibilityBvh3D tree;Need(tree.Reserve(256),"empty reserve");Need(tree.Build(values),"memory build");Need(tree.Reserve(512),"active index reserve");MoveAndRestore(tree,values);
        const auto before=tree.MemoryStats();const auto released=tree.TrimUnusedMemory();const auto after=tree.MemoryStats();Need(after.trimCount==before.trimCount+1&&after.lastReleasedBytes==released,"active trim accounting metadata");Need(released==(before.retainedCapacityBytes>after.retainedCapacityBytes?before.retainedCapacityBytes-after.retainedCapacityBytes:0),"active trim retained bytes");Members(tree,values);MoveAndRestore(tree,values);
        const auto stats=tree.Stats();const auto memory=tree.MemoryStats();Need(!tree.Reserve((std::numeric_limits<std::size_t>::max)()),"overflow reserve accepted");Need(SameStats(stats,tree.Stats())&&SameMemory(memory,tree.MemoryStats()),"overflow reserve changed state");
        tree.Clear();Need(tree.Query(All()).empty(),"clear query");Need(!tree.Update(values[0].key,values[0].bounds),"clear retained valid index");Need(tree.Reserve(0),"reset reserve target");const auto cleared=tree.MemoryStats();Need(cleared.reclaimableCapacityBytes>0,"cleared capacities not reclaimable");const auto freed=tree.TrimUnusedMemory();const auto trimmed=tree.MemoryStats();Need(trimmed.retainedCapacityBytes<cleared.retainedCapacityBytes,"cleared trim released nothing");Need(freed==cleared.retainedCapacityBytes-trimmed.retainedCapacityBytes&&trimmed.lastReleasedBytes==freed,"cleared trim accounting");Need(tree.Query(All()).empty(),"trim republished objects");Need(tree.Build(values),"rebuild after trim");Members(tree,values);MoveAndRestore(tree,values);
    }
}
}
int main(){
    try{
        AllocationCounts();KeyDomains();InvalidBuilds();denseFaultOrdinals=EnumerateBuildFailures(false);genericFaultOrdinals=EnumerateBuildFailures(true);MemoryAndTrim();
        std::cout<<"DENSE_KEY_INDEX_CPU_PASS checks="<<checks<<" dense4096_allocations="<<denseAllocations[0]<<','<<denseAllocations[1]<<','<<denseAllocations[2]<<" generic64_allocations="<<genericAllocations[0]<<','<<genericAllocations[1]<<','<<genericAllocations[2]<<" fault_dense_ordinals="<<denseFaultOrdinals<<" fault_generic_ordinals="<<genericFaultOrdinals<<" injected_bad_alloc="<<AllocationProbe::injected<<" query_key_refit_atomic_memory=1 native=0 GPU=0\n";return 0;
    }catch(const std::exception& error){AllocationProbe::Disarm();std::cerr<<"FIRST_DENSE_KEY_INDEX_DIVERGENCE="<<error.what()<<" checks="<<checks<<'\n';return 2;}
}
