#pragma once
#include <cstdint>
#include <filesystem>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace Aevrynd {
struct AudioClip {
    uint32_t sampleRate{48000};
    uint16_t channels{1},bitsPerSample{16};
    std::vector<uint8_t> samples;
    bool Valid()const{return sampleRate>=8000&&sampleRate<=192000&&(channels==1||channels==2)&&(bitsPerSample==8||bitsPerSample==16)&&!samples.empty()&&samples.size()%(channels*(bitsPerSample/8))==0;}
    std::size_t FrameCount()const{return Valid()?samples.size()/(channels*(bitsPerSample/8)):0;}
    float DurationSeconds()const{return sampleRate?float(FrameCount())/float(sampleRate):0;}
};

struct AudioLoadResult {
    AudioClip clip;
    std::string error;
    explicit operator bool()const{return error.empty()&&clip.Valid();}
};

AudioLoadResult LoadWave(std::span<const uint8_t> source);
AudioLoadResult LoadWaveFile(const std::filesystem::path& path);
AudioClip GenerateTone(float frequency,float durationSeconds,float amplitude=.25f,uint32_t sampleRate=48000);

struct SoundHandle {uint32_t value{};explicit operator bool()const{return value!=0;}bool operator==(const SoundHandle&)const=default;};
struct AudioPlayback {float volume{1},pitch{1};bool loop{};};
struct AudioStats {bool deviceAvailable{};uint32_t initializationError{};std::size_t residentSounds{},residentBytes{},activeVoices{};uint64_t playRequests{},successfulPlays{},droppedPlays{};float masterVolume{1};};

class AudioEngine {
public:
    static constexpr std::size_t MaximumVoices=64;
    AudioEngine();
    ~AudioEngine();
    AudioEngine(const AudioEngine&)=delete;
    AudioEngine& operator=(const AudioEngine&)=delete;
    bool Initialize();
    void Shutdown();
    SoundHandle CreateSound(AudioClip clip);
    void DestroySound(SoundHandle sound);
    bool Play(SoundHandle sound,AudioPlayback playback={});
    void StopAll();
    void Update();
    void SetMasterVolume(float volume);
    AudioStats GetStats()const;
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
}
