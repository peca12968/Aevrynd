#include "Aevrynd/Audio.h"
#include <xaudio2.h>
#include <wrl/client.h>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>
#include <numbers>
#include <sstream>
#include <unordered_map>

namespace Aevrynd {
namespace {
uint16_t ReadU16(std::span<const uint8_t> source,std::size_t offset){return uint16_t(source[offset])|uint16_t(source[offset+1])<<8;}
uint32_t ReadU32(std::span<const uint8_t> source,std::size_t offset){return uint32_t(source[offset])|uint32_t(source[offset+1])<<8|uint32_t(source[offset+2])<<16|uint32_t(source[offset+3])<<24;}
bool Matches(std::span<const uint8_t> source,std::size_t offset,std::string_view text){return offset+text.size()<=source.size()&&std::equal(text.begin(),text.end(),source.begin()+offset);}
}

AudioLoadResult LoadWave(std::span<const uint8_t> source){
    if(source.size()<12||!Matches(source,0,"RIFF")||!Matches(source,8,"WAVE"))return{{},"Invalid RIFF/WAVE header"};const std::size_t containerSize=std::size_t(ReadU32(source,4))+8;if(containerSize>source.size()||containerSize<12)return{{},"Invalid RIFF container size"};
    uint16_t formatTag{},channels{},bitsPerSample{},blockAlign{};uint32_t sampleRate{},byteRate{};std::span<const uint8_t> samples;bool hasFormat{};
    for(std::size_t offset=12;offset+8<=containerSize;){const uint32_t chunkSize=ReadU32(source,offset+4);const std::size_t payload=offset+8,next=payload+std::size_t(chunkSize)+(chunkSize&1u);if(payload+chunkSize>containerSize||next>containerSize)return{{},"WAVE chunk exceeds container"};if(Matches(source,offset,"fmt ")){if(chunkSize<16)return{{},"WAVE format chunk is too small"};formatTag=ReadU16(source,payload);channels=ReadU16(source,payload+2);sampleRate=ReadU32(source,payload+4);byteRate=ReadU32(source,payload+8);blockAlign=ReadU16(source,payload+12);bitsPerSample=ReadU16(source,payload+14);hasFormat=true;}else if(Matches(source,offset,"data"))samples=source.subspan(payload,chunkSize);offset=next;}
    if(!hasFormat||samples.empty())return{{},"WAVE requires format and sample data"};if(formatTag!=WAVE_FORMAT_PCM)return{{},"Only PCM WAVE data is supported"};AudioClip clip{sampleRate,channels,bitsPerSample,{samples.begin(),samples.end()}};const uint16_t expectedBlock=uint16_t(channels*(bitsPerSample/8));if(!clip.Valid()||blockAlign!=expectedBlock||byteRate!=sampleRate*blockAlign)return{{},"Unsupported or inconsistent PCM format"};return{std::move(clip),{}};
}

AudioLoadResult LoadWaveFile(const std::filesystem::path& path){std::ifstream file(path,std::ios::binary);if(!file)return{{},"Unable to open WAVE file: "+path.string()};std::vector<uint8_t> source{std::istreambuf_iterator<char>(file),std::istreambuf_iterator<char>()};return LoadWave(source);}

AudioClip GenerateTone(float frequency,float durationSeconds,float amplitude,uint32_t sampleRate){AudioClip clip;clip.sampleRate=(std::max)(8000u,(std::min)(192000u,sampleRate));clip.channels=1;clip.bitsPerSample=16;const float duration=(std::max)(.001f,durationSeconds),gain=(std::max)(0.f,(std::min)(1.f,amplitude)),safeFrequency=(std::max)(0.f,(std::min)(frequency,float(clip.sampleRate)*.5f));const std::size_t frames=std::size_t(std::ceil(duration*clip.sampleRate));clip.samples.resize(frames*sizeof(int16_t));for(std::size_t index=0;index<frames;++index){const float time=float(index)/float(clip.sampleRate),release=(std::min)(1.f,(duration-time)/.035f),attack=(std::min)(1.f,time/.006f),envelope=(std::max)(0.f,(std::min)(attack,release));const int16_t sample=int16_t(std::sin(time*safeFrequency*2.f*std::numbers::pi_v<float>)*gain*envelope*32767.f);std::memcpy(clip.samples.data()+index*sizeof(sample),&sample,sizeof(sample));}return clip;}

struct AudioEngine::Impl {
    struct Voice {IXAudio2SourceVoice* source{};std::shared_ptr<const AudioClip> clip;};
    Microsoft::WRL::ComPtr<IXAudio2> device;
    IXAudio2MasteringVoice* mastering{};
    std::unordered_map<uint32_t,std::shared_ptr<const AudioClip>> sounds;
    std::vector<Voice> voices;
    AudioStats stats{};
    uint32_t nextHandle{1};
    bool ownsComInitialization{};
};

AudioEngine::AudioEngine():impl_(std::make_unique<Impl>()){}
AudioEngine::~AudioEngine(){Shutdown();}
bool AudioEngine::Initialize(){if(impl_->stats.deviceAvailable)return true;HRESULT result=CoInitializeEx(nullptr,COINIT_MULTITHREADED);if(SUCCEEDED(result))impl_->ownsComInitialization=true;else if(result!=RPC_E_CHANGED_MODE){impl_->stats.initializationError=uint32_t(result);return false;}Microsoft::WRL::ComPtr<IXAudio2> device;result=XAudio2Create(device.GetAddressOf(),0,XAUDIO2_DEFAULT_PROCESSOR);if(FAILED(result)){impl_->stats.initializationError=uint32_t(result);if(impl_->ownsComInitialization){CoUninitialize();impl_->ownsComInitialization=false;}return false;}IXAudio2MasteringVoice* mastering{};result=device->CreateMasteringVoice(&mastering);if(FAILED(result)){impl_->stats.initializationError=uint32_t(result);device.Reset();if(impl_->ownsComInitialization){CoUninitialize();impl_->ownsComInitialization=false;}return false;}impl_->device=std::move(device);impl_->mastering=mastering;impl_->stats.deviceAvailable=true;impl_->stats.initializationError=0;impl_->mastering->SetVolume(impl_->stats.masterVolume);return true;}
void AudioEngine::Shutdown(){StopAll();if(impl_->mastering){impl_->mastering->DestroyVoice();impl_->mastering=nullptr;}impl_->device.Reset();if(impl_->ownsComInitialization){CoUninitialize();impl_->ownsComInitialization=false;}impl_->sounds.clear();impl_->stats.deviceAvailable=false;impl_->stats.residentSounds=0;impl_->stats.residentBytes=0;}
SoundHandle AudioEngine::CreateSound(AudioClip clip){if(!clip.Valid())return{};uint32_t handle=impl_->nextHandle++;if(!handle)handle=impl_->nextHandle++;auto resource=std::make_shared<AudioClip>(std::move(clip));impl_->stats.residentBytes+=resource->samples.size();impl_->sounds.emplace(handle,std::move(resource));impl_->stats.residentSounds=impl_->sounds.size();return{handle};}
void AudioEngine::DestroySound(SoundHandle sound){const auto found=impl_->sounds.find(sound.value);if(found==impl_->sounds.end())return;impl_->stats.residentBytes-=found->second->samples.size();impl_->sounds.erase(found);impl_->stats.residentSounds=impl_->sounds.size();}
bool AudioEngine::Play(SoundHandle sound,AudioPlayback playback){++impl_->stats.playRequests;const auto found=impl_->sounds.find(sound.value);if(found==impl_->sounds.end()||!impl_->stats.deviceAvailable||impl_->voices.size()>=MaximumVoices){++impl_->stats.droppedPlays;return false;}const AudioClip& clip=*found->second;WAVEFORMATEX format{};format.wFormatTag=WAVE_FORMAT_PCM;format.nChannels=clip.channels;format.nSamplesPerSec=clip.sampleRate;format.wBitsPerSample=clip.bitsPerSample;format.nBlockAlign=uint16_t(clip.channels*(clip.bitsPerSample/8));format.nAvgBytesPerSec=format.nSamplesPerSec*format.nBlockAlign;IXAudio2SourceVoice* voice{};if(FAILED(impl_->device->CreateSourceVoice(&voice,&format))){++impl_->stats.droppedPlays;return false;}voice->SetVolume((std::max)(0.f,(std::min)(4.f,playback.volume)));voice->SetFrequencyRatio((std::max)(XAUDIO2_MIN_FREQ_RATIO,(std::min)(XAUDIO2_MAX_FREQ_RATIO,playback.pitch)));XAUDIO2_BUFFER buffer{};buffer.AudioBytes=uint32_t(clip.samples.size());buffer.pAudioData=clip.samples.data();buffer.Flags=XAUDIO2_END_OF_STREAM;buffer.LoopCount=playback.loop?XAUDIO2_LOOP_INFINITE:0;if(FAILED(voice->SubmitSourceBuffer(&buffer))||FAILED(voice->Start())){voice->DestroyVoice();++impl_->stats.droppedPlays;return false;}impl_->voices.push_back({voice,found->second});++impl_->stats.successfulPlays;impl_->stats.activeVoices=impl_->voices.size();return true;}
void AudioEngine::StopAll(){for(auto& voice:impl_->voices){voice.source->Stop();voice.source->DestroyVoice();}impl_->voices.clear();impl_->stats.activeVoices=0;}
void AudioEngine::Update(){for(std::size_t index=0;index<impl_->voices.size();){XAUDIO2_VOICE_STATE state{};impl_->voices[index].source->GetState(&state,XAUDIO2_VOICE_NOSAMPLESPLAYED);if(state.BuffersQueued){++index;continue;}impl_->voices[index].source->DestroyVoice();impl_->voices.erase(impl_->voices.begin()+index);}impl_->stats.activeVoices=impl_->voices.size();}
void AudioEngine::SetMasterVolume(float volume){impl_->stats.masterVolume=(std::max)(0.f,(std::min)(1.f,volume));if(impl_->mastering)impl_->mastering->SetVolume(impl_->stats.masterVolume);}
AudioStats AudioEngine::GetStats()const{return impl_->stats;}
}
