#include <Aevrynd/Profiling.h>
#include <algorithm>
#include <cstdio>
#include <functional>
#include <iomanip>
#include <limits>
#include <locale>
#include <sstream>
#include <thread>

namespace Aevrynd {
namespace {
size_t StringResident(const std::string& value)noexcept{return value.size()+1;}
size_t StringCapacity(const std::string& value)noexcept{return value.capacity()+1;}
std::string JsonEscape(std::string_view value){std::string result;result.reserve(value.size()+8);constexpr char Hex[]="0123456789abcdef";for(const unsigned char character:value){switch(character){case '"':result+="\\\"";break;case '\\':result+="\\\\";break;case '\b':result+="\\b";break;case '\f':result+="\\f";break;case '\n':result+="\\n";break;case '\r':result+="\\r";break;case '\t':result+="\\t";break;default:if(character<0x20){result+="\\u00";result+=Hex[character>>4];result+=Hex[character&15];}else result+=char(character);break;}}return result;}
std::string CsvEscape(std::string_view value){bool quote=false;for(const char character:value)quote|=character==','||character=='"'||character=='\r'||character=='\n';if(!quote)return std::string(value);std::string result;result.reserve(value.size()+2);result+='"';for(const char character:value){if(character=='"')result+='"';result+=character;}result+='"';return result;}
uint64_t CurrentThreadId()noexcept{return uint64_t(std::hash<std::thread::id>{}(std::this_thread::get_id()));}
double Milliseconds(std::chrono::steady_clock::duration duration)noexcept{return std::chrono::duration<double,std::milli>(duration).count();}
bool ValidUtf8(std::string_view text)noexcept{for(size_t index=0;index<text.size();){const unsigned char first=static_cast<unsigned char>(text[index]);if(first<0x80){++index;continue;}size_t count{};uint32_t code{};if((first&0xe0)==0xc0){count=2;code=first&31;if(code<2)return false;}else if((first&0xf0)==0xe0){count=3;code=first&15;}else if((first&0xf8)==0xf0){count=4;code=first&7;}else return false;if(index+count>text.size())return false;for(size_t part=1;part<count;++part){const unsigned char value=static_cast<unsigned char>(text[index+part]);if((value&0xc0)!=0x80)return false;code=(code<<6)|(value&63);}if((count==3&&code<0x800)||(count==4&&code<0x10000)||code>0x10ffff||(code>=0xd800&&code<=0xdfff))return false;index+=count;}return true;}
void AdvanceEpoch(uint64_t& epoch)noexcept{++epoch;if(!epoch)++epoch;}
}

void CpuProfiler::Configure(ProfilerConfig config){
    config.frameCapacity=(std::clamp)(config.frameCapacity,size_t{1},MaximumFrameCapacity);config.maximumSamplesPerFrame=(std::clamp)(config.maximumSamplesPerFrame,size_t{1},MaximumSamplesPerFrame);config.maximumMetricsPerFrame=(std::clamp)(config.maximumMetricsPerFrame,size_t{1},MaximumMetricsPerFrame);config.maximumNameLength=(std::clamp)(config.maximumNameLength,size_t{1},MaximumProfileNameLength);
    std::vector<ProfileFrame> frames(config.frameCapacity);ProfileFrame pending;pending.samples.reserve(config.maximumSamplesPerFrame);pending.metrics.reserve(config.maximumMetricsPerFrame);
    std::scoped_lock lock(mutex_);enabled_.store(false,std::memory_order_relaxed);config_=config;frames_=std::move(frames);pending_=std::move(pending);firstFrame_=frameCount_=0;frameOpen_=false;AdvanceEpoch(frameEpoch_);totals_={};lastReleasedBytes_=0;trimCount_=0;enabled_.store(config.enabled,std::memory_order_relaxed);
}

void CpuProfiler::SetEnabled(bool enabled){
    std::scoped_lock lock(mutex_);
    if(!enabled){enabled_.store(false,std::memory_order_relaxed);frameOpen_=false;AdvanceEpoch(frameEpoch_);pending_.samples.clear();pending_.metrics.clear();pending_.droppedSamples=pending_.droppedMetrics=0;return;}
    if(frames_.empty()){
        config_=ProfilerConfig{};frames_.resize(config_.frameCapacity);pending_.samples.reserve(config_.maximumSamplesPerFrame);pending_.metrics.reserve(config_.maximumMetricsPerFrame);firstFrame_=frameCount_=0;totals_={};lastReleasedBytes_=0;trimCount_=0;
    }
    enabled_.store(true,std::memory_order_relaxed);
}

bool CpuProfiler::BeginFrame(uint64_t frameNumber)noexcept{
    if(!Enabled())return false;try{std::scoped_lock lock(mutex_);if(!enabled_.load(std::memory_order_relaxed)||frameOpen_)return false;pending_.frameNumber=frameNumber;pending_.durationMilliseconds=0;pending_.samples.clear();pending_.metrics.clear();pending_.droppedSamples=0;pending_.droppedMetrics=0;frameStarted_=std::chrono::steady_clock::now();AdvanceEpoch(frameEpoch_);frameOpen_=true;return true;}catch(...){return false;}
}

bool CpuProfiler::EndFrame()noexcept{
    if(!Enabled())return false;try{std::scoped_lock lock(mutex_);if(!frameOpen_)return false;pending_.durationMilliseconds=Milliseconds(std::chrono::steady_clock::now()-frameStarted_);size_t slot{};if(frameCount_<frames_.size()){slot=(firstFrame_+frameCount_)%frames_.size();++frameCount_;}else{slot=firstFrame_;firstFrame_=(firstFrame_+1)%frames_.size();}std::swap(frames_[slot],pending_);pending_.samples.clear();pending_.metrics.clear();pending_.droppedSamples=pending_.droppedMetrics=0;frameOpen_=false;++totals_.framesRecorded;return true;}catch(...){return false;}
}

void CpuProfiler::DropSampleLocked()noexcept{++pending_.droppedSamples;++totals_.droppedSamples;}
void CpuProfiler::DropMetricLocked()noexcept{++pending_.droppedMetrics;++totals_.droppedMetrics;}
bool CpuProfiler::BeginScope(std::string_view name,std::string& ownedName,std::chrono::steady_clock::time_point& started,uint64_t& frameEpoch)noexcept{
    if(!Enabled())return false;try{std::scoped_lock lock(mutex_);if(!frameOpen_||!enabled_.load(std::memory_order_relaxed))return false;if(!ValidUtf8(name)){DropSampleLocked();return false;}ownedName.assign(name.substr(0,config_.maximumNameLength));started=std::chrono::steady_clock::now();frameEpoch=frameEpoch_;return true;}catch(...){return false;}
}
void CpuProfiler::RecordScope(std::string_view name,std::chrono::steady_clock::time_point started,std::chrono::steady_clock::time_point finished,uint64_t threadId,uint64_t frameEpoch)noexcept{
    if(!Enabled())return;try{std::scoped_lock lock(mutex_);if(!frameOpen_||!enabled_.load(std::memory_order_relaxed))return;if(frameEpoch!=frameEpoch_){++totals_.droppedSamples;return;}const double start=Milliseconds(started-frameStarted_),duration=Milliseconds(finished-started);if(!std::isfinite(start)||!std::isfinite(duration)||duration<0){DropSampleLocked();return;}if(pending_.samples.size()>=config_.maximumSamplesPerFrame){DropSampleLocked();return;}try{pending_.samples.push_back({std::string(name),threadId,start,duration});++totals_.samplesRecorded;}catch(...){DropSampleLocked();}}catch(...){}
}

void CpuProfiler::MergeMetricLocked(std::string_view name,ProfileMetricKind kind,double value)noexcept{
    if(!std::isfinite(value)||!ValidUtf8(name)||(kind!=ProfileMetricKind::Counter&&kind!=ProfileMetricKind::Gauge)){DropMetricLocked();return;}name=name.substr(0,config_.maximumNameLength);for(ProfileMetric& metric:pending_.metrics)if(metric.kind==kind&&metric.name==name){const double merged=kind==ProfileMetricKind::Counter?metric.value+value:value;if(!std::isfinite(merged)){DropMetricLocked();return;}metric.value=merged;++totals_.metricsRecorded;return;}
    if(pending_.metrics.size()>=config_.maximumMetricsPerFrame){DropMetricLocked();return;}
    try{pending_.metrics.push_back({std::string(name),kind,value});++totals_.metricsRecorded;}catch(...){DropMetricLocked();}
}

void CpuProfiler::Counter(std::string_view name,double delta)noexcept{if(!Enabled())return;try{std::scoped_lock lock(mutex_);if(frameOpen_&&enabled_.load(std::memory_order_relaxed))MergeMetricLocked(name,ProfileMetricKind::Counter,delta);}catch(...){} }
void CpuProfiler::Gauge(std::string_view name,double value)noexcept{if(!Enabled())return;try{std::scoped_lock lock(mutex_);if(frameOpen_&&enabled_.load(std::memory_order_relaxed))MergeMetricLocked(name,ProfileMetricKind::Gauge,value);}catch(...){} }

void CpuProfiler::Merge(const ProfileBatch& batch)noexcept{
    if(!Enabled())return;try{std::scoped_lock lock(mutex_);if(!frameOpen_||!enabled_.load(std::memory_order_relaxed))return;for(size_t index=0;index<batch.samples.size();++index){if(pending_.samples.size()>=config_.maximumSamplesPerFrame){const uint64_t dropped=uint64_t(batch.samples.size()-index);pending_.droppedSamples+=dropped;totals_.droppedSamples+=dropped;break;}const ProfileSample& source=batch.samples[index];if(!ValidUtf8(source.name)||!std::isfinite(source.startMilliseconds)||!std::isfinite(source.durationMilliseconds)||source.durationMilliseconds<0){DropSampleLocked();continue;}try{ProfileSample sample=source;sample.name.resize((std::min)(sample.name.size(),config_.maximumNameLength));pending_.samples.push_back(std::move(sample));++totals_.samplesRecorded;}catch(...){DropSampleLocked();}}for(const ProfileMetric& metric:batch.metrics)MergeMetricLocked(metric.name,metric.kind,metric.value);}catch(...){}
}

std::vector<ProfileFrame> CpuProfiler::Frames()const{std::scoped_lock lock(mutex_);std::vector<ProfileFrame> result;result.reserve(frameCount_);for(size_t index=0;index<frameCount_;++index)result.push_back(frames_[(firstFrame_+index)%frames_.size()]);return result;}
ProfilerStats CpuProfiler::Stats()const noexcept{try{std::scoped_lock lock(mutex_);ProfilerStats result=totals_;result.enabled=Enabled();result.frameOpen=frameOpen_;result.frameCapacity=frames_.size();result.frameCount=frameCount_;result.pendingSamples=pending_.samples.size();result.pendingMetrics=pending_.metrics.size();return result;}catch(...){return{};}}

size_t CpuProfiler::FrameResidentBytes(const ProfileFrame& frame)noexcept{size_t bytes=frame.samples.size()*sizeof(ProfileSample)+frame.metrics.size()*sizeof(ProfileMetric);for(const auto& sample:frame.samples)bytes+=StringResident(sample.name);for(const auto& metric:frame.metrics)bytes+=StringResident(metric.name);return bytes;}
size_t CpuProfiler::FrameCapacityBytes(const ProfileFrame& frame)noexcept{size_t bytes=frame.samples.capacity()*sizeof(ProfileSample)+frame.metrics.capacity()*sizeof(ProfileMetric);for(const auto& sample:frame.samples)bytes+=StringCapacity(sample.name);for(const auto& metric:frame.metrics)bytes+=StringCapacity(metric.name);return bytes;}
ProfilerMemoryStats CpuProfiler::MemoryStats()const noexcept{try{std::scoped_lock lock(mutex_);size_t resident=frames_.size()*sizeof(ProfileFrame)+FrameResidentBytes(pending_),capacity=frames_.capacity()*sizeof(ProfileFrame)+FrameCapacityBytes(pending_);for(const auto& frame:frames_){resident+=FrameResidentBytes(frame);capacity+=FrameCapacityBytes(frame);}return{resident,capacity,capacity>resident?capacity-resident:0,lastReleasedBytes_,trimCount_};}catch(...){return{};}}

size_t CpuProfiler::TrimUnusedMemory()noexcept{
    try{std::scoped_lock lock(mutex_);size_t before=frames_.capacity()*sizeof(ProfileFrame)+FrameCapacityBytes(pending_);for(const auto& frame:frames_)before+=FrameCapacityBytes(frame);auto trim=[](ProfileFrame& frame){frame.samples.shrink_to_fit();frame.metrics.shrink_to_fit();for(auto& sample:frame.samples)sample.name.shrink_to_fit();for(auto& metric:frame.metrics)metric.name.shrink_to_fit();};trim(pending_);for(auto& frame:frames_)trim(frame);size_t after=frames_.capacity()*sizeof(ProfileFrame)+FrameCapacityBytes(pending_);for(const auto& frame:frames_)after+=FrameCapacityBytes(frame);lastReleasedBytes_=before>after?before-after:0;++trimCount_;return lastReleasedBytes_;}catch(...){return 0;}
}

void CpuProfiler::Clear()noexcept{try{std::scoped_lock lock(mutex_);for(auto& frame:frames_){frame.samples.clear();frame.metrics.clear();frame={};}pending_.samples.clear();pending_.metrics.clear();pending_.frameNumber=0;pending_.durationMilliseconds=0;pending_.droppedSamples=pending_.droppedMetrics=0;firstFrame_=frameCount_=0;frameOpen_=false;AdvanceEpoch(frameEpoch_);totals_={};}catch(...){} }

std::string CpuProfiler::ExportJson()const{
    const std::vector<ProfileFrame> frames=Frames();std::ostringstream output;output.imbue(std::locale::classic());output<<std::setprecision(std::numeric_limits<double>::max_digits10)<<"{\"frames\":[";for(size_t frameIndex=0;frameIndex<frames.size();++frameIndex){if(frameIndex)output<<',';const ProfileFrame& frame=frames[frameIndex];output<<"{\"frame\":"<<frame.frameNumber<<",\"durationMs\":"<<frame.durationMilliseconds<<",\"droppedSamples\":"<<frame.droppedSamples<<",\"droppedMetrics\":"<<frame.droppedMetrics<<",\"samples\":[";for(size_t index=0;index<frame.samples.size();++index){if(index)output<<',';const auto& sample=frame.samples[index];output<<"{\"name\":\""<<JsonEscape(sample.name)<<"\",\"thread\":"<<sample.threadId<<",\"startMs\":"<<sample.startMilliseconds<<",\"durationMs\":"<<sample.durationMilliseconds<<'}';}output<<"],\"metrics\":[";for(size_t index=0;index<frame.metrics.size();++index){if(index)output<<',';const auto& metric=frame.metrics[index];output<<"{\"name\":\""<<JsonEscape(metric.name)<<"\",\"kind\":\""<<(metric.kind==ProfileMetricKind::Counter?"counter":"gauge")<<"\",\"value\":"<<metric.value<<'}';}output<<"]}";}output<<"]}";return output.str();
}

std::string CpuProfiler::ExportCsv()const{
    const std::vector<ProfileFrame> frames=Frames();std::ostringstream output;output.imbue(std::locale::classic());output<<std::setprecision(std::numeric_limits<double>::max_digits10)<<"frame,frame_duration_ms,type,name,thread,start_ms,duration_ms,value\n";for(const ProfileFrame& frame:frames){for(const auto& sample:frame.samples)output<<frame.frameNumber<<','<<frame.durationMilliseconds<<",scope,"<<CsvEscape(sample.name)<<','<<sample.threadId<<','<<sample.startMilliseconds<<','<<sample.durationMilliseconds<<",\n";for(const auto& metric:frame.metrics)output<<frame.frameNumber<<','<<frame.durationMilliseconds<<','<<(metric.kind==ProfileMetricKind::Counter?"counter":"gauge")<<','<<CsvEscape(metric.name)<<",,,,"<<metric.value<<'\n';}return output.str();
}

CpuScopeTimer::CpuScopeTimer(CpuProfiler& profiler,std::string_view name)noexcept{if(!profiler.BeginScope(name,name_,started_,frameEpoch_))return;profiler_=&profiler;threadId_=CurrentThreadId();}
CpuScopeTimer::~CpuScopeTimer()noexcept{if(profiler_)profiler_->RecordScope(name_,started_,std::chrono::steady_clock::now(),threadId_,frameEpoch_);}

}
