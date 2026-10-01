#include "MistwatchAudio.h"
#include <filesystem>
#include <fstream>
#include <iostream>
int main(int argc,char** argv) {
    if(argc!=2||std::filesystem::exists(argv[1]))return 1;
    constexpr unsigned rate=48000,frames=8*rate;
    std::vector<float> mix(frames);
    const auto overlay=[&](Mistwatch::Sound sound,float start,float gain) {
        const auto clip=Mistwatch::CreateGameSound(sound);
        const unsigned begin=unsigned(start*rate);
        for(std::size_t i=0;i<clip.FrameCount()&&begin+i<mix.size();++i) {
            std::int16_t sample{};std::memcpy(&sample,clip.samples.data()+i*2,2);
            mix[begin+i]+=float(sample)/32767.f*gain*.45f;
        }
    };
    overlay(Mistwatch::Sound::Wind,0,.35f);overlay(Mistwatch::Sound::Wind,4,.35f);
    for(unsigned i=0;i<4;++i)overlay(Mistwatch::Sound::Water,float(i)*1.8f,.25f);
    for(unsigned i=0;i<6;++i)overlay(Mistwatch::Sound(i),.5f+float(i)*.5f,.7f);
    overlay(Mistwatch::Sound::Landing,3.5f,.75f);overlay(Mistwatch::Sound::Interact,4,.65f);
    overlay(Mistwatch::Sound::Tune,4.4f,.45f);overlay(Mistwatch::Sound::Warning,4.8f,.8f);
    overlay(Mistwatch::Sound::Hurt,5.6f,.7f);overlay(Mistwatch::Sound::Success,6.3f,.7f);
    std::ofstream out(argv[1],std::ios::binary);
    const auto u16=[&](std::uint16_t value){out.put(char(value&255));out.put(char(value>>8));};
    const auto u32=[&](std::uint32_t value){for(unsigned byte=0;byte<4;++byte)out.put(char((value>>(byte*8))&255));};
    out.write("RIFF",4);u32(36+frames*2);out.write("WAVEfmt ",8);u32(16);u16(1);u16(1);
    u32(rate);u32(rate*2);u16(2);u16(16);out.write("data",4);u32(frames*2);
    for(const float sample:mix)u16(std::uint16_t(std::int16_t(std::clamp(sample,-1.f,1.f)*32767.f)));
    out.close();if(!out)return 2;
    const auto loaded=Aevrynd::LoadWaveFile(argv[1]);
    if(!loaded||loaded.clip.FrameCount()!=frames||loaded.clip.channels!=1)return 3;
    std::cout<<"AUDIO_PREVIEW_READY; seconds=8; source=original_game_PCM; not_loopback_recording\n";
}
