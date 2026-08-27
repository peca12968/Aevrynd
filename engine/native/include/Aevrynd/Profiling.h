#pragma once

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace Aevrynd {

enum class ProfileMetricKind : uint8_t {Counter,Gauge};

struct ProfileSample {
    std::string name;
    uint64_t threadId{};
    double startMilliseconds{};
    double durationMilliseconds{};
};

struct ProfileMetric {
    std::string name;
    ProfileMetricKind kind{ProfileMetricKind::Counter};
    double value{};
};

struct ProfileBatch {
    std::vector<ProfileSample> samples;
    std::vector<ProfileMetric> metrics;
};

struct ProfileFrame {
    uint64_t frameNumber{};
    double durationMilliseconds{};
    std::vector<ProfileSample> samples;
    std::vector<ProfileMetric> metrics;
    uint64_t droppedSamples{};
    uint64_t droppedMetrics{};
};

struct ProfilerConfig {
    bool enabled{};
    size_t frameCapacity{120};
    size_t maximumSamplesPerFrame{1024};
    size_t maximumMetricsPerFrame{256};
    size_t maximumNameLength{96};
};

struct ProfilerStats {
    bool enabled{};
    bool frameOpen{};
    size_t frameCapacity{};
    size_t frameCount{};
    size_t pendingSamples{};
    size_t pendingMetrics{};
    uint64_t framesRecorded{};
    uint64_t samplesRecorded{};
    uint64_t metricsRecorded{};
    uint64_t droppedSamples{};
    uint64_t droppedMetrics{};
};

struct ProfilerMemoryStats {
    size_t residentBytes{};
    size_t capacityBytes{};
    size_t reclaimableBytes{};
    size_t lastReleasedBytes{};
    uint64_t trimCount{};
};

class CpuProfiler {
public:
    static constexpr size_t MaximumFrameCapacity=240;
    static constexpr size_t MaximumSamplesPerFrame=2048;
    static constexpr size_t MaximumMetricsPerFrame=256;
    static constexpr size_t MaximumProfileNameLength=128;
    CpuProfiler()=default;
    explicit CpuProfiler(ProfilerConfig config){Configure(config);}
    void Configure(ProfilerConfig config);
    void SetEnabled(bool enabled);
    bool Enabled()const noexcept{return enabled_.load(std::memory_order_relaxed);}
    bool BeginFrame(uint64_t frameNumber)noexcept;
    bool EndFrame()noexcept;
    void Counter(std::string_view name,double delta=1)noexcept;
    void Gauge(std::string_view name,double value)noexcept;
    void Merge(const ProfileBatch& batch)noexcept;
    std::vector<ProfileFrame> Frames()const;
    ProfilerStats Stats()const noexcept;
    ProfilerMemoryStats MemoryStats()const noexcept;
    size_t TrimUnusedMemory()noexcept;
    void Clear()noexcept;
    std::string ExportJson()const;
    std::string ExportCsv()const;
private:
    static size_t FrameResidentBytes(const ProfileFrame& frame)noexcept;
    static size_t FrameCapacityBytes(const ProfileFrame& frame)noexcept;
    void MergeMetricLocked(std::string_view name,ProfileMetricKind kind,double value)noexcept;
    bool BeginScope(std::string_view name,std::string& ownedName,std::chrono::steady_clock::time_point& started,uint64_t& frameEpoch)noexcept;
    void RecordScope(std::string_view name,std::chrono::steady_clock::time_point started,std::chrono::steady_clock::time_point finished,uint64_t threadId,uint64_t frameEpoch)noexcept;
    void DropSampleLocked()noexcept;
    void DropMetricLocked()noexcept;
    mutable std::mutex mutex_;
    std::atomic_bool enabled_{};
    ProfilerConfig config_{};
    std::vector<ProfileFrame> frames_;
    size_t firstFrame_{},frameCount_{};
    ProfileFrame pending_{};
    std::chrono::steady_clock::time_point frameStarted_{};
    bool frameOpen_{};
    uint64_t frameEpoch_{};
    ProfilerStats totals_{};
    size_t lastReleasedBytes_{};
    uint64_t trimCount_{};
    friend class CpuScopeTimer;
};

class CpuScopeTimer {
public:
    CpuScopeTimer(CpuProfiler& profiler,std::string_view name)noexcept;
    ~CpuScopeTimer()noexcept;
    CpuScopeTimer(const CpuScopeTimer&)=delete;
    CpuScopeTimer& operator=(const CpuScopeTimer&)=delete;
private:
    CpuProfiler* profiler_{};
    std::string name_;
    std::chrono::steady_clock::time_point started_{};
    uint64_t threadId_{};
    uint64_t frameEpoch_{};
};

}
