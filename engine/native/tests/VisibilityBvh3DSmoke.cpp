#include <Aevrynd/VisibilityBvh3D.h>
#include <Aevrynd/Scene3D.h>
#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <vector>

using namespace Aevrynd;
namespace {std::size_t AllocationCount{};}
void* operator new(std::size_t size){if(void* memory=std::malloc(size)){++AllocationCount;return memory;}throw std::bad_alloc{};}
void* operator new[](std::size_t size){return ::operator new(size);}void operator delete(void* memory)noexcept{std::free(memory);}void operator delete(void* memory,std::size_t)noexcept{std::free(memory);}void operator delete[](void* memory)noexcept{std::free(memory);}void operator delete[](void* memory,std::size_t)noexcept{std::free(memory);}
namespace {
bool VerifySinglePassBuildMap(){
    // 兩個符號、全部指數及代表性尾數，逐一放入9個分量；獨立標準分類不可變。
    std::size_t finiteChecks{};
    for(std::uint32_t sign:{0u,0x80000000u})for(std::uint32_t exponent=0;exponent<256;++exponent)
        for(std::uint32_t mantissa:{0u,1u,0x003fffffu,0x00400000u,0x007fffffu}){
            const float value=std::bit_cast<float>(sign|(exponent<<23)|mantissa);
            for(unsigned component=0;component<9;++component){
                Transform3D transform{};
                std::array<float*,9> fields{{&transform.position.x,&transform.position.y,&transform.position.z,
                    &transform.rotation.x,&transform.rotation.y,&transform.rotation.z,
                    &transform.scale.x,&transform.scale.y,&transform.scale.z}};
                *fields[component]=value;
                if(Scene3DDetail::FiniteTransform(transform)!=std::isfinite(value)){
                    std::cerr<<"Finite scene transform classification differs bits="<<(sign|(exponent<<23)|mantissa)
                        <<" component="<<component<<'\n';return false;
                }
                ++finiteChecks;
            }
        }
    std::cout<<"finiteTransformEquivalentCases="<<finiteChecks<<'\n';
    constexpr std::uint32_t Count=4096,Side=64;
    std::vector<VisibilityBvhItem3D> population;population.reserve(Count);
    for(std::uint32_t index=0;index<Count;++index)
        population.push_back({std::uint64_t(index)+1,{{float(index%Side)*2.f-64.f,0,float(index/Side)*2.f+4.f},1.75f,{1,1,1}},index});
    VisibilityBvh3D tree;
    Camera3D camera{{0,8,0},{0,8,1}};
    const VisibilityBvhQuery3D all{camera,16.f/9.f,0,0,false};
    std::array<std::size_t,3> buildAllocations{};
    for(std::size_t round=0;round<buildAllocations.size();++round){
        AllocationCount=0;
        const bool built=tree.Build(population);
        buildAllocations[round]=AllocationCount;
        // 一個物件只配置一個最終 map 節點；固定容器開銷另留 32 次，不容許舊兩輪節點路徑。
        if(!built||!buildAllocations[round]||buildAllocations[round]>Count+32){
            std::cerr<<"Single-pass BVH build allocation budget failed round="<<round<<" allocations="<<buildAllocations[round]<<'\n';return false;
        }
        const auto result=tree.Query(all);
        if(result.size()!=Count){std::cerr<<"Single-pass BVH build lost payloads\n";return false;}
        for(std::uint32_t index=0;index<Count;++index)
            if(result[index]!=index){std::cerr<<"Single-pass BVH build changed payload ordering\n";return false;}
    }
    const auto sameStats=[](const VisibilityBvhStats3D& a,const VisibilityBvhStats3D& b){
        return a.objects==b.objects&&a.nodes==b.nodes&&a.candidates==b.candidates&&
            a.nodesTested==b.nodesTested&&a.nodesRejected==b.nodesRejected&&
            a.distanceRejectedObjects==b.distanceRejectedObjects&&a.frustumRejectedObjects==b.frustumRejectedObjects&&
            a.refittedLeaves==b.refittedLeaves&&a.builds==b.builds&&a.refits==b.refits&&a.queries==b.queries;
    };
    const auto rejectsWithoutPublishing=[&](std::span<const VisibilityBvhItem3D> bad,const char* label){
        const auto beforeStats=tree.Stats();const auto beforeMemory=tree.MemoryStats();
        if(tree.Build(bad)||!sameStats(beforeStats,tree.Stats())||
            tree.MemoryStats().retainedCapacityBytes!=beforeMemory.retainedCapacityBytes||
            tree.MemoryStats().reclaimableCapacityBytes!=beforeMemory.reclaimableCapacityBytes){
            std::cerr<<"Single-pass BVH rejection changed published state: "<<label<<'\n';return false;
        }
        const auto result=tree.Query(all);
        if(result.size()!=Count){std::cerr<<"Single-pass BVH rejection lost old tree: "<<label<<'\n';return false;}
        for(std::uint32_t index=0;index<Count;++index)
            if(result[index]!=index){std::cerr<<"Single-pass BVH rejection changed old payloads: "<<label<<'\n';return false;}
        return true;
    };
    // 非相鄰重複鍵在排序後可能分居不同葉節點；相同／不同 bounds、反序都必須拒絕。
    auto bad=population;bad.back().key=bad.front().key;
    if(!rejectsWithoutPublishing(bad,"distant duplicate with different bounds"))return false;
    std::reverse(bad.begin(),bad.end());
    if(!rejectsWithoutPublishing(bad,"reversed distant duplicate"))return false;
    bad=population;bad.back()=bad.front();
    if(!rejectsWithoutPublishing(bad,"identical distant duplicate"))return false;
    bad=population;bad[Count/2].key=bad[Count/2-1].key;
    if(!rejectsWithoutPublishing(bad,"adjacent interior duplicate"))return false;
    bad=population;bad[Count/2].key=0;
    if(!rejectsWithoutPublishing(bad,"zero key"))return false;
    bad=population;bad[Count/2].bounds.center.z=(std::numeric_limits<float>::quiet_NaN)();
    if(!rejectsWithoutPublishing(bad,"non-finite center"))return false;
    bad=population;bad[Count/2].bounds.radius=(std::numeric_limits<float>::infinity)();
    if(!rejectsWithoutPublishing(bad,"non-finite radius"))return false;
    bad=population;bad[Count/2].bounds.extents.y=-1.f;
    if(!rejectsWithoutPublishing(bad,"negative extent"))return false;
    bad=population;bad[Count/2].bounds.radius=-1.f;
    if(!rejectsWithoutPublishing(bad,"negative radius"))return false;
    bad=population;bad[Count/2].bounds.extents.x=(std::numeric_limits<float>::infinity)();
    if(!rejectsWithoutPublishing(bad,"non-finite extent"))return false;
    auto moved=population.front().bounds;moved.center={0,8,4};
    // 最終索引必須指向 BuildNode 排序後的位置；失敗 Build 也不可破壞後續 Update／Refit。
    if(!tree.Update(population.front().key,moved)||!tree.Refit()){
        std::cerr<<"Single-pass BVH retained key could not update/refit\n";return false;
    }
    const auto local=tree.Query({camera,16.f/9.f,5.f,0,false});
    if(std::find(local.begin(),local.end(),population.front().payload)==local.end()){
        std::cerr<<"Single-pass BVH final key index points to wrong entry\n";return false;
    }
    const auto restored=tree.Query(all);
    if(restored.size()!=Count){std::cerr<<"Single-pass BVH update/refit lost membership\n";return false;}
    for(std::uint32_t index=0;index<Count;++index)
        if(restored[index]!=index){std::cerr<<"Single-pass BVH update/refit changed payload order\n";return false;}
    std::cout<<"singlePassBuildObjects="<<Count<<" buildAllocations="<<buildAllocations[0]<<','<<buildAllocations[1]<<','<<buildAllocations[2]<<'\n';
    return true;
}
}
int main(){
    if(!VerifySinglePassBuildMap())return 16;
    constexpr std::uint32_t Count=65536,Side=256;
    std::vector<VisibilityBvhItem3D> items;items.reserve(Count);
    for(std::uint32_t index=0;index<Count;++index){const float x=float(index%Side)*4.f-512.f,z=float(index/Side)*4.f+4.f;items.push_back({std::uint64_t(index)+1,{{x,0,z},1.75f,{1,1,1}},index});}
    VisibilityBvh3D bvh;bvh.Reserve(Count);if(!bvh.Build(items)){std::cerr<<"BVH build failed\n";return 1;}
    Camera3D camera{{0,8,0},{0,8,1}};camera.projection.farPlane=85.f;VisibilityBvhQuery3D query{camera,16.f/9.f,85.f,0,true};
    const auto first=bvh.Query(query);std::vector<std::uint32_t> golden;for(const auto& item:items)if(Length(item.bounds.center-camera.position)-item.bounds.radius<=85.f&&SphereIntersectsFrustum(item.bounds.center,item.bounds.radius,camera,query.aspect))golden.push_back(item.payload);std::sort(golden.begin(),golden.end());if(first.size()!=golden.size()||!std::equal(first.begin(),first.end(),golden.begin())){std::cerr<<"BVH and linear visibility differ bvh="<<first.size()<<" linear="<<golden.size()<<'\n';return 2;}
    constexpr std::uint32_t Movers=Count/100;const auto refitStart=std::chrono::steady_clock::now();for(std::uint32_t index=0;index<Movers;++index){items[index].bounds.center.x+=.25f;if(!bvh.Update(items[index].key,items[index].bounds)){std::cerr<<"Stable key update failed\n";return 3;}}if(!bvh.Refit()){std::cerr<<"Refit failed\n";return 4;}const auto refitUs=std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now()-refitStart).count();
    for(int warm=0;warm<8;++warm)bvh.Query(query);const auto memoryBefore=bvh.MemoryStats().retainedCapacityBytes;AllocationCount=0;const auto start=std::chrono::steady_clock::now();std::size_t checksum{};for(int frame=0;frame<64;++frame)checksum+=bvh.Query(query).size();const auto elapsedUs=std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now()-start).count(),averageUs=elapsedUs/64;const auto warmAllocations=AllocationCount;const auto stats=bvh.Stats();
    if(!checksum||stats.candidates>Count/10||Count-stats.candidates<Count*95/100||averageUs>1000||refitUs>1000||warmAllocations||bvh.MemoryStats().retainedCapacityBytes!=memoryBefore){std::cerr<<"Visibility budget failed candidates="<<stats.candidates<<" averageUs="<<averageUs<<" refitUs="<<refitUs<<" allocations="<<warmAllocations<<'\n';return 5;}
    VisibilityBvh3D atomic;if(!atomic.Build(std::span<const VisibilityBvhItem3D>(items.data(),4))||atomic.Build(std::array<VisibilityBvhItem3D,2>{{items[0],items[0]}})||atomic.Stats().objects!=4){std::cerr<<"Atomic duplicate-key rejection failed\n";return 6;}const auto atomicSpan=atomic.Query({camera,16.f/9.f,0,0,false});const std::vector<std::uint32_t> atomicBefore(atomicSpan.begin(),atomicSpan.end());auto invalid=items[0];invalid.bounds.radius=(std::numeric_limits<float>::infinity)();if(atomic.Build(std::span<const VisibilityBvhItem3D>(&invalid,1))||atomic.Stats().objects!=4){std::cerr<<"Non-finite radius was accepted\n";return 6;}invalid=items[0];invalid.bounds.center.x=(std::numeric_limits<float>::quiet_NaN)();if(atomic.Build(std::span<const VisibilityBvhItem3D>(&invalid,1))||atomic.Update(0,items[0].bounds)||atomic.Update(999999,items[0].bounds)){std::cerr<<"Invalid bounds or key was accepted\n";return 6;}VisibilityBvhItem3D zero{500,{{},0,{}},7};VisibilityBvh3D zeroBvh;if(!zeroBvh.Build(std::span<const VisibilityBvhItem3D>(&zero,1))||zeroBvh.Query({camera,1,0,0,false}).size()!=1){std::cerr<<"Zero bounds contract failed\n";return 6;}const float maximum=(std::numeric_limits<float>::max)();std::array<VisibilityBvhItem3D,2> huge{{{100,{{maximum,0,0},1,{1,1,1}},0},{101,{{-maximum,0,0},1,{1,1,1}},1}}};if(atomic.Build(huge)||atomic.Stats().objects!=4){std::cerr<<"Overflowing hierarchy was accepted\n";return 6;}if(!atomic.Update(items[0].key,huge[0].bounds)||atomic.Refit()){std::cerr<<"Overflowing refit was accepted\n";return 6;}const auto afterFailedRefit=atomic.Query({camera,16.f/9.f,0,0,false});if(afterFailedRefit.size()!=atomicBefore.size()||!std::equal(afterFailedRefit.begin(),afterFailedRefit.end(),atomicBefore.begin())){std::cerr<<"Failed refit changed published query\n";return 6;}if(!atomic.Update(items[0].key,items[0].bounds)||!atomic.Refit()){std::cerr<<"Failed refit could not recover\n";return 6;}
    const auto reserveBefore=atomic.MemoryStats();if(atomic.Reserve((std::numeric_limits<std::size_t>::max)())||atomic.MemoryStats().retainedCapacityBytes!=reserveBefore.retainedCapacityBytes||atomic.Stats().objects!=4){std::cerr<<"Overflowing BVH reserve changed state\n";return 6;}VisibilityEntityKey3D cursor=(std::numeric_limits<VisibilityEntityKey3D>::max)()-1;if(AllocateVisibilityEntityKey3D(cursor)!=(std::numeric_limits<VisibilityEntityKey3D>::max)()||AllocateVisibilityEntityKey3D(cursor)!=0||cursor!=(std::numeric_limits<VisibilityEntityKey3D>::max)()){std::cerr<<"Stable key exhaustion did not fail closed\n";return 6;}
    Scene3D scene;scene.Reserve({0,0,0,Count,0});MeshResource3D resource{{1},{{},1.75f,{1,1,1}},24,36};std::vector<VisibilityEntityKey3D> keys;keys.reserve(Count);for(const auto& item:items)keys.push_back(scene.Mesh(resource,{item.bounds.center,{},Vec3{2,2,2}}));DrawList draw;Scene3DSubmissionOptions accelerated{};accelerated.visibility.maximumDistance=85.f;draw.Clear();scene.Submit(draw,camera,{1280,720},accelerated);draw.Clear();const auto sceneStart=std::chrono::steady_clock::now();const Scene3DStats acceleratedStats=scene.Submit(draw,camera,{1280,720},accelerated);const auto sceneUs=std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now()-sceneStart).count();const std::size_t acceleratedDraws=draw.meshInstances.size();accelerated.linearVisibilityReference=true;draw.Clear();const Scene3DStats linearStats=scene.Submit(draw,camera,{1280,720},accelerated);if(acceleratedStats.visibleMeshInstances!=linearStats.visibleMeshInstances||acceleratedStats.culledMeshInstances!=linearStats.culledMeshInstances||acceleratedDraws!=draw.meshInstances.size()||acceleratedStats.visibilityCandidates>Count/10||sceneUs>1000){std::cerr<<"Scene3D accelerated/reference mismatch candidates="<<acceleratedStats.visibilityCandidates<<" sceneUs="<<sceneUs<<'\n';return 7;}
    std::array<long long,9> sceneRefitSamples{};for(std::size_t sample=0;sample<sceneRefitSamples.size();++sample){const auto sceneRefitStart=std::chrono::steady_clock::now();for(std::uint32_t index=0;index<Movers;++index)if(!scene.UpdateMeshTransform(keys[index],{items[index].bounds.center+Vec3{float(sample+1)*.01f,0,0},{},Vec3{2,2,2}})){std::cerr<<"Scene stable key update failed\n";return 8;}draw.Clear();accelerated.linearVisibilityReference=false;scene.Submit(draw,camera,{1280,720},accelerated);sceneRefitSamples[sample]=std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now()-sceneRefitStart).count();}std::sort(sceneRefitSamples.begin(),sceneRefitSamples.end());const auto sceneRefitUs=sceneRefitSamples[sceneRefitSamples.size()/2],sceneRefitP95Us=sceneRefitSamples.back();if(sceneRefitUs>1500||sceneRefitP95Us>2500){std::cerr<<"Scene update/refit/submit budget failed medianUs="<<sceneRefitUs<<" p95Us="<<sceneRefitP95Us<<'\n';return 9;}
    Scene3D mutations;mutations.SetBoxMesh(resource);const auto boxKey=mutations.MaterialBox({-2,8,12},{1,1,1},{}),meshKey=mutations.Mesh(resource,{{0,8,12},{},{1,1,1}}),swappedMeshKey=mutations.Mesh(resource,{{1,8,12},{},{1,1,1}});const std::array<MeshLodLevel3D,2> levels{{{resource,{},12,6},{resource,{}}}};const auto lodKey=mutations.MeshLodChain(levels,MeshLodDistanceMetric3D::Center,{{2,8,12},{},{1,1,1}});Scene3DSubmissionOptions mutationReference{};mutationReference.linearVisibilityReference=true;draw.Clear();const Scene3DStats mutationGolden=mutations.Submit(draw,camera,{1280,720},mutationReference);draw.Clear();const Scene3DStats mutationBefore=mutations.Submit(draw,camera,{1280,720});if(!boxKey||!meshKey||!swappedMeshKey||!lodKey||!mutationGolden.visibleMeshInstances||mutationBefore.visibleMeshInstances!=mutationGolden.visibleMeshInstances||mutationBefore.lodTransitionInstances!=mutationGolden.lodTransitionInstances){std::cerr<<"Stable mixed object setup differs from linear golden accelerated="<<mutationBefore.visibleMeshInstances<<" linear="<<mutationGolden.visibleMeshInstances<<'\n';return 10;}const Matrix4 meshMatrix=draw.meshInstances[1].transform;Transform3D invalidTransform{{(std::numeric_limits<float>::quiet_NaN)(),8,12},{},{1,1,1}};if(mutations.UpdateTransform(meshKey,invalidTransform)){std::cerr<<"Invalid scene transform was accepted\n";return 11;}draw.Clear();const Scene3DStats invalidPreserved=mutations.Submit(draw,camera,{1280,720});if(invalidPreserved.visibleMeshInstances!=mutationGolden.visibleMeshInstances||draw.meshInstances[1].transform[0][3]!=meshMatrix[0][3]){std::cerr<<"Invalid update was not atomic\n";return 12;}if(!mutations.UpdateTransform(boxKey,{{-3,8,12},{},{1,1,1}})||!mutations.UpdateTransform(lodKey,{{3,8,12},{},{1,1,1}})||!mutations.Remove(meshKey)||mutations.Remove(meshKey)||!mutations.UpdateMeshTransform(swappedMeshKey,{{1.5f,8,12},{},{1,1,1}})){std::cerr<<"Mixed update/remove failed\n";return 13;}draw.Clear();const Scene3DStats mutationAccelerated=mutations.Submit(draw,camera,{1280,720});draw.Clear();const Scene3DStats mutationLinear=mutations.Submit(draw,camera,{1280,720},mutationReference);if(mutationAccelerated.visibleMeshInstances!=mutationLinear.visibleMeshInstances||mutationAccelerated.visibleTriangles!=mutationLinear.visibleTriangles||mutationLinear.visibleMeshInstances+1!=mutationGolden.visibleMeshInstances){std::cerr<<"Remove/reorder parity failed\n";return 14;}
    const std::size_t beforeInvalidAdds=mutations.MeshCount();if(mutations.MaterialBox({(std::numeric_limits<float>::quiet_NaN)(),0,0},{1,1,1},{})||mutations.Mesh(resource,{{maximum,0,0},{},{1,1,1}})||mutations.MeshLodChain(levels,MeshLodDistanceMetric3D::Center,{{0,0,0},{},{maximum,1,1}})||mutations.MeshCount()!=beforeInvalidAdds){std::cerr<<"Invalid scene add changed state\n";return 14;}const auto sceneReserveBefore=mutations.MemoryStats();if(mutations.Reserve({(std::numeric_limits<std::size_t>::max)(),0,0,0,0})||mutations.MemoryStats().retainedCapacityBytes!=sceneReserveBefore.retainedCapacityBytes){std::cerr<<"Overflowing Scene reserve changed state\n";return 14;}
    Scene3D parity;parity.SetBoxMesh(resource);parity.MaterialBox({0,8,60},{.2f,.2f,.2f},{});Material3D caster{};caster.castsShadow=true;parity.Mesh(resource,{{30,8,20},{},{1,1,1}},caster);Scene3DSubmissionOptions parityOptions{};parityOptions.visibility={100,100};parityOptions.shadows={true,.025f,.2f,50,16};draw.SetDirectionalShadowMapEnabled(true);draw.Clear();const Scene3DStats parityFast=parity.Submit(draw,camera,{1280,720},parityOptions);parityOptions.linearVisibilityReference=true;draw.Clear();const Scene3DStats parityLinear=parity.Submit(draw,camera,{1280,720},parityOptions);draw.SetDirectionalShadowMapEnabled(false);if(parityFast.visibleMeshInstances!=parityLinear.visibleMeshInstances||parityFast.screenSizeCulledMeshInstances!=parityLinear.screenSizeCulledMeshInstances||parityFast.shadowOnlyCandidates!=parityLinear.shadowOnlyCandidates||parityFast.shadowOnlyInstances!=parityLinear.shadowOnlyInstances){std::cerr<<"Screen-size/shadow-only linear parity failed\n";return 15;}
    std::cout<<"objects="<<Count<<" candidates="<<stats.candidates<<" nodesTested="<<stats.nodesTested<<" averageUs="<<averageUs<<" refitUs="<<refitUs<<" sceneUs="<<sceneUs<<" sceneRefitMedianUs="<<sceneRefitUs<<" sceneRefitP95Us="<<sceneRefitP95Us<<'\n';return 0;
}
