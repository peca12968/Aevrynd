#include "Aevrynd/Renderer.h"
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>

namespace {
void Require(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
bool Structure(const std::string& s) {
    const auto totalBegin=s.find("                  timestampStart);");
    const auto shadowBegin=s.find("D3D12_QUERY_TYPE_TIMESTAMP,timestampStart+10);");
    const auto shadowEnd=s.find("D3D12_QUERY_TYPE_TIMESTAMP,timestampStart+11);");
    const auto depthBegin=s.find("D3D12_QUERY_TYPE_TIMESTAMP,timestampStart+14);");
    const auto opaqueBegin=s.find("D3D12_QUERY_TYPE_TIMESTAMP,timestampStart+15);");
    const auto atmosphereBegin=s.find("D3D12_QUERY_TYPE_TIMESTAMP,timestampStart+16);");
    const auto transparentBegin=s.find("D3D12_QUERY_TYPE_TIMESTAMP,timestampStart+17);");
    const auto copyBegin=s.find("D3D12_QUERY_TYPE_TIMESTAMP,timestampStart+12);");
    const auto copyEnd=s.find("D3D12_QUERY_TYPE_TIMESTAMP,timestampStart+13);");
    const auto totalEnd=s.find("D3D12_QUERY_TYPE_TIMESTAMP,timestampStart+7);");
    const auto close=s.find("Check(d.list->Close(),\"Close failed\");",totalEnd);
    return totalBegin<shadowBegin && shadowBegin<shadowEnd && shadowEnd<depthBegin && depthBegin<opaqueBegin && opaqueBegin<atmosphereBegin &&
      atmosphereBegin<transparentBegin && transparentBegin<copyBegin && copyBegin<copyEnd && copyEnd<totalEnd && totalEnd<close &&
      s.find("TimestampQueriesPerFrame=18")!=s.npos &&
      s.find("const uint64_t total=elapsed(0,7);")!=s.npos &&
      s.find("timestampStart+10,4,d.timestampReadback.Get()")!=s.npos &&
      s.find("timestampStart+14,4,d.timestampReadback.Get()")!=s.npos &&
      s.find("values[10],values[11],values[14],values[15],values[16],values[17],values[12],values[13]")!=s.npos &&
      s.find("timestampStart,10,d.timestampReadback.Get()")!=s.npos &&
      s.find("timestampShadowRenderingRecorded[index]=timestampProductionCaptureCopyRecorded[index]=false;")!=s.npos &&
      s.find("if(timestampSerial[index]>timestampPublishedSerial)")!=s.npos &&
      s.find("FromCompleted(timestampPublishedSerial,values[0],values[7],values[10],values[11]")!=s.npos &&
      s.find("FromCompleted(timestampPublishedSerial,values[0],values[7],values[12],values[13]")!=s.npos;
}
}
int main(int argc,char**argv) {
 try {
    using Pass=Aevrynd::RendererStats::CompletedGpuPassTimingReceipt3D;
    const auto good=Pass::FromCompleted(42,100,1000,200,350,1000000000,true);
    Require(good.valid && good.recorded && good.serial==42 && good.nanoseconds==150,"valid pass");
    const auto idle=Pass::FromCompleted(43,100,1000,200,350,1000000000,false);
    Require(idle.valid && !idle.recorded && idle.serial==43 && idle.nanoseconds==0,"idle must clear duration");
    Require(!Pass::FromCompleted(0,100,1000,200,350,1000,true).valid,"zero serial");
    Require(!Pass::FromCompleted(42,100,1000,200,350,0,true).valid,"zero frequency");
    Require(!Pass::FromCompleted(42,100,1000,350,200,1000,true).valid,"reverse pass");
    Require(!Pass::FromCompleted(42,1000,100,200,350,1000,true).valid,"reverse total");
    Require(!Pass::FromCompleted(42,100,1000,99,350,1000,true).valid,"before total");
    Require(!Pass::FromCompleted(42,100,1000,200,1001,1000,true).valid,"after total");
    Require(!Pass::FromCompleted(42,0,UINT64_MAX,0,UINT64_MAX,1,true).valid,"overflow");
    Require(Pass::FromCompleted(42,100,1000,200,200,1000,true).nanoseconds==0,"zero duration");
    Require(argc==2,"source required");std::ifstream file(argv[1]);Require(bool(file),"source read");
    const std::string source{std::istreambuf_iterator<char>(file),{}};Require(Structure(source),"production structure");
    auto hostile=source;hostile.replace(hostile.find("elapsed(0,7)"),12,"elapsed(1,7)");Require(!Structure(hostile),"shortened total rejected");
    hostile=source;const auto start=hostile.find("D3D12_QUERY_TYPE_TIMESTAMP,timestampStart+13);");hostile.erase(start,43);Require(!Structure(hostile),"missing copy end rejected");
    hostile=source;hostile.replace(hostile.find("TimestampQueriesPerFrame=18"),27,"TimestampQueriesPerFrame=10");Require(!Structure(hostile),"undersized query heap rejected");
    hostile=source;const auto phase=hostile.find("D3D12_QUERY_TYPE_TIMESTAMP,timestampStart+15);");hostile.erase(phase,43);Require(!Structure(hostile),"missing opaque boundary rejected");
    std::cout<<"PASS receipt/idle/serial/range/overflow/production-order/hostile; CPU-only, GPU not executed\n";
    return 0;
 } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
