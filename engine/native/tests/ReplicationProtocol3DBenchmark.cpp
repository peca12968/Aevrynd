#include <Aevrynd/NetworkTransport3D.h>
#include <Aevrynd/ReplicationProtocol3D.h>
#include <chrono>
#include <iostream>
using namespace Aevrynd;
int main(){ReplicationSnapshot3D snapshot{1,60,59,2,1,1,false};for(std::uint64_t i=0;i<512;++i)snapshot.entities.push_back({{1,i+1},{{std::int64_t(i/32),0,-20},{float(i%32),1,2}},{1,0,1},0});constexpr int Iterations=2000;std::vector<std::byte> bytes;ReplicationMessage3D decoded;const auto begin=std::chrono::steady_clock::now();std::size_t total{};for(int i=0;i<Iterations;++i){snapshot.serverTick=60+i;if(!EncodeReplicationPacket3D(snapshot,bytes)){std::cerr<<"encode failed entities="<<snapshot.entities.size()<<'\n';return 1;}if(!DecodeReplicationPacket3D(bytes,decoded)){std::cerr<<"decode failed bytes="<<bytes.size()<<'\n';return 1;}total+=bytes.size();}const auto ns=std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now()-begin).count();const auto per=ns/Iterations;std::cout<<"entities=512 iterations="<<Iterations<<" packetBytes="<<bytes.size()<<" nsPerRoundTrip="<<per<<" throughputMiBps="<<(double(total)*2e9/double(ns)/(1024*1024))<<'\n';return per<2'000'000?0:2;}
