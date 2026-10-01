#pragma once
#include "MistwatchEncounter.h"
#include "MistwatchRiverProfile.h"
#include <Aevrynd/Audio.h>
#include <Aevrynd/Camera.h>
#include <cstring>
#include <numbers>
#include <stdexcept>

namespace Mistwatch {
enum class Sound : unsigned { Dirt0,Dirt1,Dirt2,Wood0,Wood1,Wood2,Landing,Interact,Tune,Warning,Hurt,Success,Wind,Water,Count };
inline constexpr unsigned SoundCount=unsigned(Sound::Count);
// 本作原創程序音色，並非森林錄音；所有樣本具有有限振幅與平滑端點。
inline Aevrynd::AudioClip CreateGameSound(Sound sound) {
    using namespace Aevrynd;
    if(unsigned(sound)>=SoundCount)return {};
    const bool wind=sound==Sound::Wind,water=sound==Sound::Water;
    const float duration=wind?4.f:water?1.9f:sound==Sound::Success?.65f:sound==Sound::Warning?.45f:.22f;
    AudioClip clip;clip.sampleRate=48000;clip.samples.resize(std::size_t(duration*48000)*2);
    std::uint32_t seed=0x41554449u+unsigned(sound)*1031u;
    float low{},middle{};
    for(std::size_t i=0;i<clip.FrameCount();++i) {
        seed=1664525u*seed+1013904223u;
        const float noise=float(seed>>8)/8388608.f-1.f;
        low+=.007f*(noise-low);middle+=.12f*(noise-middle);
        const float t=float(i)/48000.f;
        const float edge=(std::min)(t,(float(clip.FrameCount()-1-i)/48000.f));
        const float fade=std::clamp(edge/(wind||water?.05f:.006f),0.f,1.f);
        float value{};
        const unsigned kind=unsigned(sound);
        if(wind)value=low*.8f*(.65f+.20f*std::sin(t*std::numbers::pi_v<float>*.5f));
        else if(water)value=.18f*middle+.055f*(noise-middle);
        else if(kind<=unsigned(Sound::Wood2)) {
            const bool wood=kind>=unsigned(Sound::Wood0);
            const float frequency=wood?145.f+float(kind%3)*19.f:58.f+float(kind%3)*7.f;
            value=(.22f*std::sin(t*frequency*6.2831853f)+(wood?.08f:.22f)*middle)*std::exp(-t*(wood?22.f:18.f));
        } else if(sound==Sound::Landing||sound==Sound::Hurt)
            value=(.3f*std::sin(t*65.f*6.2831853f)+.25f*middle)*std::exp(-t*17.f);
        else if(sound==Sound::Warning)
            value=(.15f*std::sin(t*(90.f+25.f*std::sin(t*14.f))*6.2831853f)+.12f*low)*std::exp(-t*3.f);
        else {
            const float frequency=sound==Sound::Tune?720.f:sound==Sound::Interact?420.f:t<.2f?440.f:t<.4f?554.365f:659.255f;
            value=.13f*std::sin(t*frequency*6.2831853f)*std::exp(-t*(sound==Sound::Success?1.5f:15.f));
        }
        const auto sample=std::int16_t(std::clamp(value*fade,-.6f,.6f)*32767.f);
        std::memcpy(clip.samples.data()+i*2,&sample,2);
    }
    return clip;
}
struct AudioOptions {
    float volume{.45f};bool muted{};
    float Gain()const{return muted?0.f:volume;}
    std::string Serialize()const {
        std::ostringstream out;out.imbue(std::locale::classic());out.precision(9);
        out<<"MISTWATCH_AUDIO 1 "<<volume<<' '<<muted;return out.str();
    }
    bool Restore(std::string_view text) {
        std::istringstream in{std::string(text)};in.imbue(std::locale::classic());
        std::string tag;unsigned version{},mute{};float level{};
        if(!(in>>tag>>version>>level>>mute)||tag!="MISTWATCH_AUDIO"||version!=1||mute>1||
           !std::isfinite(level)||level<0||level>1)return false;
        in>>std::ws;if(!in.eof())return false;
        volume=level;muted=mute!=0;return true;
    }
};
inline Aevrynd::Vec3 AudioListenerRight(const Aevrynd::Camera3D& camera) {
    // 沿用引擎左手座標的視圖矩陣，避免將左右聲道反轉。
    const auto view=Aevrynd::CreateViewMatrix(camera);return {view[0][0],view[0][1],view[0][2]};
}
struct AudioState {
    Session session;EnemyPhase enemyPhase{EnemyPhase::Pursuit};unsigned enemyHealth{3};
    Aevrynd::Vec3 enemyFeet{Session::arena};
    bool active{},grounded{},crouched{};
};
struct AudioEvents {
    bool startWind{},stopWind{},water{},step{},wood{},landing{},interact{},tune{},warning{},hurt{},success{};
    float stepVolume{.7f},stepPitch{1.f};
};
struct AudioDirector {
    AudioState previous;bool initialized{},windPlaying{};
    float stepDistance{},waterTime{};unsigned variation{};
    void Reset(){initialized=false;stepDistance=0;waterTime=0;}
    AudioEvents Tick(const AudioState& state,float dt) {
        AudioEvents events;
        if(!std::isfinite(dt)||dt<=0||dt>.1f||!Session::Finite(state.session.feet))return events;
        if(!state.active) {
            events.hurt=initialized&&previous.active&&state.session.health<previous.session.health;
            events.stopWind=windPlaying;windPlaying=false;initialized=false;stepDistance=0;waterTime=0;
            previous=state;return events;
        }
        if(!windPlaying){events.startWind=true;windPlaying=true;}
        waterTime-=dt;
        if(waterTime<=0){events.water=true;waterTime=1.8f;}
        if(!initialized){previous=state;initialized=true;return events;}
        const auto delta=state.session.feet-previous.session.feet;
        const float distance=std::hypot(delta.x,delta.z);
        const bool teleported=Aevrynd::Length(delta)>3.f;
        if(!teleported) {
            events.landing=!previous.grounded&&state.grounded&&delta.y<0;
            if(state.grounded&&previous.grounded) {
                stepDistance+=distance;
                // 與目前探險者距離驅動的步態共用每半週期 pi/5 公尺。
                if(stepDistance>=std::numbers::pi_v<float>/5.f) {
                    stepDistance=std::fmod(stepDistance,std::numbers::pi_v<float>/5.f);events.step=true;
                    events.stepVolume=state.crouched?.25f:.7f;
                    events.stepPitch=std::clamp(distance/dt/5.f,.85f,1.15f);
                    // 對應場景橋面中心 (0,-.1,36)、尺寸 (4,.2,8)，不把兩岸當作木地板。
                    events.wood=state.session.bridgeRepaired&&std::abs(state.session.feet.x)<=2.f&&
                        state.session.feet.z>=32.f&&state.session.feet.z<=40.f&&std::abs(state.session.feet.y)<=.25f;
                }
            }else stepDistance=0;
        }else stepDistance=0;
        const auto& a=previous.session;const auto& b=state.session;
        events.interact=a.collected!=b.collected||a.repairMaterials!=b.repairMaterials||a.bridgeRepaired!=b.bridgeRepaired||
            a.fuseClueRead!=b.fuseClueRead||a.fuseCollected!=b.fuseCollected||a.powerRestored!=b.powerRestored||a.relayLinked!=b.relayLinked;
        events.tune=a.circuit!=b.circuit||a.fuseDials!=b.fuseDials||a.relaySectors!=b.relaySectors;
        events.warning=previous.enemyPhase!=EnemyPhase::Windup&&state.enemyPhase==EnemyPhase::Windup;
        events.hurt=b.health<a.health||state.enemyHealth<previous.enemyHealth;
        events.success=!a.beaconLit&&b.beaconLit;
        previous=state;return events;
    }
};
inline Aevrynd::Vec3 NearestRiverSource(Aevrynd::Vec3 listener) {
    Aevrynd::Vec3 result{};float best=1.e20f;
    for(unsigned i=0;i<=200;++i) {
        const float x=-50.f+float(i)*.5f;
        const Aevrynd::Vec3 p{x,RiverHeight,RiverCenterZ(x)};
        const float distance=Aevrynd::Length(p-listener);
        if(distance<best){best=distance;result=p;}
    }
    return result;
}
struct GameAudio {
    std::array<Aevrynd::SoundHandle,SoundCount> sounds{};
    std::array<std::uint64_t,SoundCount> requests{};
    AudioDirector director;AudioOptions options;
    void Initialize(Aevrynd::AudioEngine& audio) {
        for(unsigned i=0;i<SoundCount;++i){sounds[i]=audio.CreateSound(CreateGameSound(Sound(i)));if(!sounds[i])throw std::runtime_error("Game audio resource invalid");}
        audio.SetMasterVolume(options.Gain());
    }
    void Tick(Aevrynd::AudioEngine& audio,const AudioState& state,Aevrynd::Vec3 listener,Aevrynd::Vec3 right,float dt) {
        const auto events=director.Tick(state,dt);
        if(events.stopWind)audio.StopAll();
        // 沒有輸出裝置時保留遊戲與事件邏輯，不反覆送出必然失敗的播放請求。
        if(!audio.GetStats().deviceAvailable)return;
        const auto play=[&](Sound sound,Aevrynd::AudioPlayback playback) {++requests[unsigned(sound)];return audio.Play(sounds[unsigned(sound)],playback);};
        const auto spatial=[&](Sound sound,Aevrynd::Vec3 source,float range,float volume,float pitch) {
            const Aevrynd::AudioSpatial3D position{listener,right,source,range,1.5f,volume,pitch};
            if(Aevrynd::CalculateSpatialAudio(position).volume<=.001f)return;
            ++requests[unsigned(sound)];audio.PlaySpatial(sounds[unsigned(sound)],position);
        };
        if(events.startWind)play(Sound::Wind,{.35f,1,true});
        if(events.water)spatial(Sound::Water,NearestRiverSource(listener),30,.6f,1);
        if(events.step){const unsigned variant=director.variation++%3;spatial(Sound((events.wood?3u:0u)+variant),state.session.feet,20,events.stepVolume,events.stepPitch);}
        if(events.landing)spatial(Sound::Landing,state.session.feet,20,.75f,1);
        if(events.interact)play(Sound::Interact,{.65f});
        if(events.tune)play(Sound::Tune,{.45f});
        if(events.warning)spatial(Sound::Warning,state.enemyFeet,25,.8f,1);
        if(events.hurt)play(Sound::Hurt,{.7f});
        if(events.success)play(Sound::Success,{.7f});
    }
    void Shutdown(Aevrynd::AudioEngine& audio) {
        audio.StopAll();for(auto& sound:sounds){audio.DestroySound(sound);sound={};}director={};
    }
};
}
