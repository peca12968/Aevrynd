#include "WorldPartition3DFakeFixtures.h"
#include <Aevrynd/Vegetation3D.h>
#include <iostream>
using namespace Aevrynd;using namespace Aevrynd::Tests;

static WorldPartitionSettings3D Settings(unsigned radius=0){WorldPartitionSettings3D value{};value.spatial={32,radius,radius,4,4,0x5147};value.workerCount=2;value.maximumInflightLoads=4;value.maximumCpuReadyBytes=4096;value.maximumPreparationBytesPerUpdate=4096;value.maximumActivationBytesPerUpdate=4096;value.maximumGpuResidentBytes=4096;return value;}
int main(){
    // 相同座標與 world seed 必須產生相同請求 seed。
    FakeChunkSource3D firstSource,secondSource;FakeActivationTarget3D firstTarget,secondTarget;WorldPartition3D first(firstSource,firstTarget,Settings()),second(secondSource,secondTarget,Settings());first.Update({0,0,0});second.Update({0,0,0});if(firstSource.began.size()!=1||secondSource.began.size()!=1||firstSource.began[0].seed!=secondSource.began[0].seed){std::cerr<<"deterministic source request failed\n";return 1;}
    // Teleport 後舊 epoch payload 必須由 partition 丟棄，不能進入 activation target。
    first.Update({320,0,320});firstSource.CompleteOne();first.Update({320,0,320});if(!first.Stats().staleCompletions||firstTarget.activates){std::cerr<<"stale payload reached target\n";return 2;}
    for(int checkpoint=0;checkpoint<4;++checkpoint){FakeChunkSource3D source;source.autoComplete=true;FakeActivationTarget3D target;target.failNext=true;WorldPartition3D partition(source,target,Settings());partition.Update({0,0,0});partition.Update({0,0,0});const auto stats=partition.Stats();if(stats.failed!=1||stats.cpuReadyBytes||stats.residentGpuBytes||target.logicalReferences){std::cerr<<"failure checkpoint rollback failed "<<checkpoint<<'\n';return 3;}}
    {FakeChunkSource3D source;source.defaultCpuBytes=8192;source.autoComplete=true;FakeActivationTarget3D target;WorldPartition3D partition(source,target,Settings());partition.Update({0,0,0});partition.Update({0,0,0});if(partition.Stats().lastFailure!=WorldPartitionFailureReason3D::CpuBudget){std::cerr<<"budget rejection failed\n";return 4;}}
    // 四個 chunk 共用一次實體 vegetation 資產，但各自保有 logical reference。
    {FakeActivationTarget3D target;target.sharePhysical=true;const WorldChunkPayload3D payload{std::make_shared<int>(1),128,256};WorldChunkActivation3D handles[4]{};for(std::uint64_t index=0;index<4;++index)handles[index]=target.Activate({{int(index),0},0,1,index+1,1},payload);if(target.physicalAllocations!=1||target.logicalReferences!=4){std::cerr<<"shared vegetation ownership failed\n";return 5;}for(const auto handle:handles)target.Rollback(handle);if(target.logicalReferences){std::cerr<<"shared rollback failed\n";return 6;}}
    {FakeChunkSource3D source;source.autoComplete=true;FakeActivationTarget3D target;target.CompleteActivations();target.CompleteRetirements();auto settings=Settings();settings.spatial.retainRadius=0;WorldPartition3D partition(source,target,settings);for(int cycle=0;cycle<128;++cycle){partition.Update({float(cycle&1)*64,0,0});partition.Update({float(cycle&1)*64,0,0});}partition.WaitIdle();partition.TrimUnusedMemory();if(partition.Stats().resident!=1||partition.Stats().peakInflightLoads>4){std::cerr<<"cycle lifecycle failed\n";return 7;}}
    std::cout<<"terrain vegetation adapter fake contract passed\n";
}
