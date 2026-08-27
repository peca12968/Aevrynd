#include <Aevrynd/Profiling.h>
#include <iostream>
#include <limits>
#include <locale>
#include <memory>
#include <string>
#include <thread>
#include <vector>

using namespace Aevrynd;

struct CommaDecimalPoint final:std::numpunct<char>{char do_decimal_point()const override{return',';}};

int main(){
    CpuProfiler disabled;for(int index=0;index<10000;++index){CpuScopeTimer scope(disabled,"disabled");disabled.Counter("ignored");disabled.Gauge("ignored",index);}if(disabled.Stats().framesRecorded||disabled.MemoryStats().capacityBytes||disabled.BeginFrame(1)||disabled.EndFrame()){std::cerr<<"Disabled profiling path retained data\n";return 1;}disabled.SetEnabled(true);if(!disabled.BeginFrame(1)){std::cerr<<"Lazy profiler enable failed\n";return 1;}disabled.SetEnabled(false);disabled.SetEnabled(true);if(!disabled.BeginFrame(2)||!disabled.EndFrame()){std::cerr<<"Profiler disable reset failed\n";return 1;}

    CpuProfiler profiler({true,3,64,8,96});if(!profiler.BeginFrame(10)){std::cerr<<"Frame begin failed\n";return 2;}
    constexpr int ThreadCount=4,ScopesPerThread=12;std::vector<std::thread> workers;for(int threadIndex=0;threadIndex<ThreadCount;++threadIndex)workers.emplace_back([&,threadIndex]{for(int sample=0;sample<ScopesPerThread;++sample){CpuScopeTimer scope(profiler,threadIndex==0?"worker,\"primary\"\n":"worker");profiler.Counter("jobs",1);profiler.Gauge("last_worker",threadIndex);}});for(auto& worker:workers)worker.join();if(!profiler.EndFrame()){std::cerr<<"Threaded frame end failed\n";return 3;}
    const auto threadedFrames=profiler.Frames();if(threadedFrames.size()!=1||threadedFrames[0].samples.size()!=ThreadCount*ScopesPerThread||threadedFrames[0].metrics.size()!=2){std::cerr<<"Thread-safe recording failed\n";return 4;}double jobCount{};for(const auto& metric:threadedFrames[0].metrics)if(metric.name=="jobs")jobCount=metric.value;if(jobCount!=ThreadCount*ScopesPerThread){std::cerr<<"Counter merge failed\n";return 5;}

    ProfileBatch batch;for(int index=0;index<40;++index)batch.samples.push_back({"batch",1,0,.1});batch.metrics.push_back({"jobs",ProfileMetricKind::Counter,2});batch.metrics.push_back({"temperature",ProfileMetricKind::Gauge,42});if(!profiler.BeginFrame(11)){std::cerr<<"Batch frame begin failed\n";return 6;}profiler.Merge(batch);profiler.Merge(batch);profiler.EndFrame();const auto batchFrames=profiler.Frames();if(batchFrames.back().samples.size()!=64||batchFrames.back().droppedSamples!=16||batchFrames.back().metrics.size()!=2){std::cerr<<"Bounded batch merge failed\n";return 7;}

    for(uint64_t frame=12;frame<17;++frame){profiler.BeginFrame(frame);for(int index=0;index<80;++index){CpuScopeTimer scope(profiler,"overflow");}for(int index=0;index<20;++index)profiler.Gauge("gauge_"+std::to_string(index),index);profiler.EndFrame();}const auto ringFrames=profiler.Frames();if(ringFrames.size()!=3||ringFrames.front().frameNumber!=14||ringFrames.back().frameNumber!=16){std::cerr<<"Frame ring overwrite failed\n";return 8;}for(const auto& frame:ringFrames)if(frame.samples.size()!=64||frame.metrics.size()!=8||frame.droppedSamples!=16||frame.droppedMetrics!=12){std::cerr<<"Per-frame bounds failed\n";return 9;}

    profiler.BeginFrame(17);{CpuScopeTimer scope(profiler,"quote \" slash \\ line\nnext");profiler.Counter("metric,\"line\n",3);}profiler.EndFrame();const std::string json=profiler.ExportJson(),csv=profiler.ExportCsv();if(json.find("quote \\\" slash \\\\ line\\nnext")==std::string::npos||json.find("metric,\\\"line\\n")==std::string::npos||csv.find("\"quote \"\" slash \\ line\nnext\"")==std::string::npos||csv.find("\"metric,\"\"line\n\"")==std::string::npos){std::cerr<<"Export escaping failed\n";return 10;}

    profiler.BeginFrame(18);std::string temporaryName="temporary-owned-name";{CpuScopeTimer scope(profiler,temporaryName);temporaryName.assign("mutated");}profiler.EndFrame();if(profiler.Frames().back().samples.size()!=1||profiler.Frames().back().samples[0].name!="temporary-owned-name"){std::cerr<<"Scope name ownership failed\n";return 11;}
    profiler.BeginFrame(19);{CpuScopeTimer late(profiler,"late-scope");profiler.EndFrame();profiler.BeginFrame(20);}profiler.EndFrame();if(!profiler.Frames().back().samples.empty()||profiler.Stats().droppedSamples==0){std::cerr<<"Cross-frame scope token failed\n";return 12;}

    ProfileBatch invalidBatch;invalidBatch.samples.push_back({"bad-duration",1,0,-1});invalidBatch.metrics.push_back({"bad-batch",ProfileMetricKind::Gauge,std::numeric_limits<double>::infinity()});profiler.BeginFrame(21);profiler.Gauge("bad-gauge",std::numeric_limits<double>::quiet_NaN());profiler.Counter("overflow",std::numeric_limits<double>::max());profiler.Counter("overflow",std::numeric_limits<double>::max());profiler.Merge(invalidBatch);profiler.EndFrame();const ProfileFrame invalidFrame=profiler.Frames().back();if(invalidFrame.samples.size()!=0||invalidFrame.metrics.size()!=1||invalidFrame.droppedSamples!=1||invalidFrame.droppedMetrics!=3){std::cerr<<"Invalid numeric rejection failed\n";return 13;}

    const std::locale previousLocale=std::locale();std::locale::global(std::locale(previousLocale,new CommaDecimalPoint));profiler.BeginFrame(22);profiler.Gauge("fraction",1.5);profiler.EndFrame();const std::string localeJson=profiler.ExportJson(),localeCsv=profiler.ExportCsv();std::locale::global(previousLocale);if(localeJson.find("\"value\":1.5")==std::string::npos||localeCsv.find(",1.5\n")==std::string::npos){std::cerr<<"Locale-independent export failed\n";return 14;}

    CpuProfiler bounded({true,std::numeric_limits<size_t>::max(),std::numeric_limits<size_t>::max(),std::numeric_limits<size_t>::max(),std::numeric_limits<size_t>::max()});if(bounded.Stats().frameCapacity!=CpuProfiler::MaximumFrameCapacity||!bounded.BeginFrame(1)){std::cerr<<"Profiler hard limits failed\n";return 15;}for(size_t index=0;index<CpuProfiler::MaximumSamplesPerFrame+1;++index){CpuScopeTimer scope(bounded,"bounded");}bounded.EndFrame();if(bounded.Frames()[0].samples.size()!=CpuProfiler::MaximumSamplesPerFrame||bounded.Frames()[0].droppedSamples!=1){std::cerr<<"Maximum sample limit failed\n";return 16;}bounded.BeginFrame(2);{CpuScopeTimer scope(bounded,std::string(CpuProfiler::MaximumProfileNameLength+20,'n'));}for(size_t index=0;index<CpuProfiler::MaximumMetricsPerFrame+1;++index)bounded.Gauge("metric-"+std::to_string(index),double(index));bounded.EndFrame();const ProfileFrame boundedFrame=bounded.Frames().back();if(boundedFrame.samples[0].name.size()!=CpuProfiler::MaximumProfileNameLength||boundedFrame.metrics.size()!=CpuProfiler::MaximumMetricsPerFrame||boundedFrame.droppedMetrics!=1){std::cerr<<"Maximum metric or name limit failed\n";return 16;}

    const ProfilerMemoryStats beforeTrim=profiler.MemoryStats();profiler.TrimUnusedMemory();const ProfilerMemoryStats afterTrim=profiler.MemoryStats();if(afterTrim.capacityBytes>beforeTrim.capacityBytes||afterTrim.trimCount!=1){std::cerr<<"Profiler trim failed\n";return 17;}profiler.Clear();const ProfilerStats cleared=profiler.Stats();if(cleared.frameCount||cleared.framesRecorded||cleared.pendingSamples){std::cerr<<"Profiler clear failed\n";return 18;}

    std::cout<<"threadedSamples="<<threadedFrames[0].samples.size()<<" ringFrames="<<ringFrames.size()<<" dropped="<<profiler.Stats().droppedSamples<<" jsonBytes="<<json.size()<<" csvBytes="<<csv.size()<<" retained="<<afterTrim.capacityBytes<<'\n';return 0;
}
