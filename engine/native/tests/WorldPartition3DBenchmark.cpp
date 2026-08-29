#include "WorldPartition3DFakeFixtures.h"
#include <algorithm>
#include <chrono>
#include <iostream>
#include <string_view>
#include <vector>
using namespace Aevrynd;using namespace Aevrynd::Tests;
struct Samples{std::vector<uint64_t> values;void Add(auto&& operation){const auto begin=std::chrono::steady_clock::now();operation();values.push_back(uint64_t(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now()-begin).count()));}void Print(std::string_view name){std::sort(values.begin(),values.end());auto p=[&](double q){return values[size_t(q*double(values.size()-1))];};std::cout<<name<<"P50Ns="<<p(.5)<<' '<<name<<"P95Ns="<<p(.95)<<' '<<name<<"P99Ns="<<p(.99)<<' '<<name<<"MaxNs="<<values.back()<<' ';}};
int main(){
 FakeChunkSource3D source;source.autoComplete=true;FakeActivationTarget3D target;target.CompleteActivations();target.CompleteRetirements();WorldPartitionSettings3D settings{};settings.spatial={32,2,3,25,25,7};settings.maximumInflightLoads=2;settings.maximumCpuReadyBytes=8*1024*1024;settings.maximumGpuResidentBytes=8*1024*1024;WorldPartition3D partition(source,target,settings);Samples scheduler,boundary,teleport;
 for(int i=0;i<5000;++i){const Vec3 p{float((i/6)%31)*32.f,0,float((i/186)%31)*32.f};scheduler.Add([&]{partition.Update(p);});}
 for(int i=0;i<256;++i){const float edge=(i&1)?31.99f:32.01f;boundary.Add([&]{partition.Update({edge,0,float((i/2)%31)*32.f});});}
 for(int i=0;i<64;++i){const Vec3 p{float((i*13)%31)*32.f,0,float((i*19)%31)*32.f};teleport.Add([&]{partition.Update(p);});}
 FakeChunkSource3D phaseSource;phaseSource.autoComplete=true;FakeActivationTarget3D phaseTarget;phaseTarget.CompleteActivations();phaseTarget.CompleteRetirements();WorldPartitionSettings3D phaseSettings{};phaseSettings.spatial={32,0,0,1,1,19};phaseSettings.maximumCpuReadyBytes=1024;phaseSettings.maximumGpuResidentBytes=1024;WorldPartition3D phases(phaseSource,phaseTarget,phaseSettings);Samples activation,retire;for(int i=0;i<256;++i){const Vec3 here{float(i*2)*32.f,0,0};phases.Update(here);activation.Add([&]{phases.Update(here);});const Vec3 away{here.x+32.f,0,0};retire.Add([&]{phases.Update(away);});}
 scheduler.Print("scheduler");activation.Print("activation");retire.Print("retire");boundary.Print("boundary");teleport.Print("teleport");std::cout<<"peakInflight="<<partition.Stats().peakInflightLoads<<" pathChunks=961 maxCpuBudget="<<settings.maximumCpuReadyBytes<<" maxGpuBudget="<<settings.maximumGpuResidentBytes<<'\n';return partition.Stats().peakInflightLoads<=2&&phases.Stats().peakInflightLoads<=2?0:1;
}
