#include <Aevrynd/ReplicationWorldAdapter3D.h>
#include <chrono>
#include <iostream>
using namespace Aevrynd;
int main(){ReplicationChunkInterest3D interest({4,5,32});ReplicationInterestDelta3D delta;constexpr int Updates=10000;const auto begin=std::chrono::steady_clock::now();for(int i=0;i<Updates;++i)if(!interest.Update({{i%100-50,0,(i*17)%100-50},{float(i%32),0,float((i*3)%32)}},delta))return 1;const auto ns=std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now()-begin).count();std::cout<<"updates="<<Updates<<" nsPerUpdate="<<ns/Updates<<" resident="<<interest.Chunks().size()<<'\n';return ns/Updates<500'000?0:2;}
