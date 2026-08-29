#include "WorldPartition3DFakeFixtures.h"
#include <iostream>
#include <limits>
using namespace Aevrynd;using namespace Aevrynd::Tests;

static WorldPartitionSettings3D Settings(float chunkSize=32.f){WorldPartitionSettings3D value{};value.spatial={chunkSize,0,0,1,1,17};value.workerCount=1;value.maximumInflightLoads=1;value.maximumCpuReadyBytes=value.maximumGpuResidentBytes=value.maximumPreparationBytesPerUpdate=value.maximumActivationBytesPerUpdate=4096;return value;}

int main(){
 FakeChunkSource3D source;FakeActivationTarget3D target;WorldPartition3D partition(source,target,Settings());GlobalPosition3D observer{{-7,0,5},{3,0,4}};
 if(!partition.UpdateGlobal(observer)||source.began.size()!=1||source.began[0].coordinate!=WorldChunkCoordinate3D{-7,5})return 1;const auto request=source.began[0];
 // 相同全域觀測者代表 origin rebase；不得重建或取消 loading request。
 if(!partition.UpdateGlobal(observer)||source.began.size()!=1||partition.Stats().inflightLoads!=1||partition.Stats().totalCancelled||source.began[0].requestId!=request.requestId)return 2;
 source.CompleteOne();partition.UpdateGlobal(observer);target.CompleteActivations();partition.UpdateGlobal(observer);if(partition.Stats().resident!=1||partition.Stats().totalCancelled)return 3;
 if(!partition.UpdateGlobal(observer)||partition.Stats().resident!=1||partition.Stats().retiring)return 4;
 FakeChunkSource3D scaledSource;FakeActivationTarget3D scaledTarget;WorldPartition3D scaled(scaledSource,scaledTarget,Settings(20));if(!scaled.UpdateGlobal({{1000000,0,-1000000},{5,0,95}},100)||scaled.Stats().observerChunk!=WorldChunkCoordinate3D{5000000,-4999996})return 5;
 FakeChunkSource3D negativeSource;FakeActivationTarget3D negativeTarget;WorldPartition3D negative(negativeSource,negativeTarget,Settings(20));if(!negative.UpdateGlobal({{-1,0,-1},{99,0,79}},100)||negative.Stats().observerChunk!=WorldChunkCoordinate3D{-1,-2})return 6;
 if(scaled.UpdateGlobal({{(std::numeric_limits<std::int64_t>::max)(),0,0},{}}))return 7;
 std::cout<<"loading=1 resident=1 retiring_preserved=1 negative=1 large=1 mixed_chunk_size=1\n";
}
