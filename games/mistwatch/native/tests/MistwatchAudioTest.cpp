#include "MistwatchAudio.h"
#include <iostream>
#include <limits>
int main() {
    using namespace Mistwatch;using namespace Aevrynd;
    std::size_t bytes{};
    for(unsigned i=0;i<SoundCount;++i) {
        const auto clip=CreateGameSound(Sound(i)),repeat=CreateGameSound(Sound(i));
        if(!clip.Valid()||clip.samples!=repeat.samples||clip.FrameCount()<1000)return 1;
        std::int16_t first{},last{};std::memcpy(&first,clip.samples.data(),2);std::memcpy(&last,clip.samples.data()+clip.samples.size()-2,2);
        if(first||last)return 2;
        bool audible=false;
        for(std::size_t offset=0;offset<clip.samples.size();offset+=2){std::int16_t value{};std::memcpy(&value,clip.samples.data()+offset,2);if(std::abs(int(value))>19661)return 3;audible|=std::abs(int(value))>200;}
        if(!audible)return 4;bytes+=clip.samples.size();
    }
    if(bytes>1000000||CreateGameSound(Sound::Count).Valid())return 5;
    AudioOptions options;
    if(!options.Restore("MISTWATCH_AUDIO 1 0.7 1")||options.Gain()!=0)return 6;
    const auto saved=options.Serialize();
    AudioOptions restored;if(!restored.Restore(saved)||restored.Serialize()!=saved)return 33;
    for(const auto invalid:{"MISTWATCH_AUDIO 1 nan 0","MISTWATCH_AUDIO 1 -0.1 0","MISTWATCH_AUDIO 1 1.1 0","MISTWATCH_AUDIO 1 0.3 2","MISTWATCH_AUDIO 2 0.3 0","MISTWATCH_AUDIO 1 0.3 0 junk"})
        if(options.Restore(invalid)||options.Serialize()!=saved)return 7;
    AudioDirector director;AudioState state;state.active=true;state.grounded=true;
    auto events=director.Tick(state,.016f);if(!events.startWind||events.step||events.interact)return 8;
    unsigned steps{};
    for(unsigned i=0;i<120;++i){state.session.feet.z+=.025f;events=director.Tick(state,1.f/60.f);steps+=events.step;}
    if(steps!=4)return 9;
    state.grounded=false;
    for(unsigned i=0;i<60;++i){state.session.feet.z+=.025f;state.session.feet.y+=.01f;if(director.Tick(state,1.f/60.f).step)return 10;}
    state.grounded=true;state.session.feet.y-=.01f;
    if(!director.Tick(state,.016f).landing||director.Tick(state,.016f).landing)return 11;
    state.active=false;events=director.Tick(state,.016f);if(!events.stopWind||events.step)return 12;
    if(director.Tick(state,.016f).stopWind)return 13;
    state.session.collected=7;state.session.beaconLit=true;state.active=true;
    events=director.Tick(state,.016f);if(!events.startWind||events.interact||events.success)return 14;
    director.Reset();state.session={};state.session.bridgeRepaired=true;state.session.feet={0,0,32};director.Tick(state,.016f);
    state.session.feet.z+=.7f;state.crouched=true;events=director.Tick(state,.1f);
    if(!events.step||!events.wood||events.stepVolume!=.25f)return 15;
    // 木步聲必須與實際橋面相符，土岸、橋外及橋下不能套用木質音色。
    for(const Vec3 point:std::array<Vec3,5>{{{0,0,30},{2.3f,0,36},{0,0,42},{0,-4,36},{0,0,36}}}) {
        AudioDirector surface;AudioState sample;sample.active=true;sample.grounded=true;
        sample.session.bridgeRepaired=true;sample.session.feet=point-Vec3{0,0,.7f};surface.Tick(sample,.1f);
        sample.session.feet=point;const auto step=surface.Tick(sample,.1f);
        const bool deck=point.x==0&&point.y==0&&point.z==36;
        if(!step.step||step.wood!=deck)return 32;
    }
    state.session.feet.z+=20;if(director.Tick(state,.016f).step)return 16;
    state.enemyPhase=EnemyPhase::Windup;if(!director.Tick(state,.016f).warning||director.Tick(state,.016f).warning)return 17;
    state.session.health=80;if(!director.Tick(state,.016f).hurt||director.Tick(state,.016f).hurt)return 18;
    state.session.collected=1;if(!director.Tick(state,.016f).interact||director.Tick(state,.016f).interact)return 19;
    state.session.circuit=1;if(!director.Tick(state,.016f).tune||director.Tick(state,.016f).tune)return 20;
    state.session.beaconLit=true;if(!director.Tick(state,.016f).success||director.Tick(state,.016f).success)return 21;
    if(director.Tick(state,(std::numeric_limits<float>::quiet_NaN)()).step||director.Tick(state,.5f).water)return 22;
    const auto source=NearestRiverSource({0,0,36});
    if(source.x!=0||source.y!=RiverHeight||source.z!=36)return 23;
    const auto near=CalculateSpatialAudio(AudioSpatial3D{{0,0,36},{1,0,0},source,30,1.5f,.6f,1});
    const auto away=CalculateSpatialAudio(AudioSpatial3D{{0,0,100},{1,0,0},source,30,1.5f,.6f,1});
    if(near.volume<.4f||away.volume!=0)return 24;
    for(unsigned sector=0;sector<16;++sector) {
        const float angle=float(sector)*6.2831853f/16;
        Camera3D camera;camera.position={};camera.target={std::sin(angle),0,std::cos(angle)};
        const auto right=AudioListenerRight(camera);
        if(Length(right-Vec3{std::cos(angle),0,-std::sin(angle)})>.00001f)return 29;
        const auto toRight=CalculateSpatialAudio(AudioSpatial3D{{},right,right*10.f,30,1,1,1});
        const auto toLeft=CalculateSpatialAudio(AudioSpatial3D{{},right,right*-10.f,30,1,1,1});
        if(toRight.pan<.9999f||toLeft.pan>-.9999f)return 30;
    }
    state.active=false;state.session.health=0;events=director.Tick(state,.016f);
    if(!events.stopWind||!events.hurt||director.Tick(state,.016f).hurt)return 31;
    AudioEngine audio;const bool available=audio.Initialize();GameAudio game;game.options.muted=true;game.Initialize(audio);
    if(audio.GetStats().residentSounds!=SoundCount||audio.GetStats().masterVolume!=0)return 25;
    state={};state.active=true;state.grounded=true;game.Tick(audio,state,{0,2,36},{1,0,0},.016f);
    state.session.feet.z+=.7f;game.Tick(audio,state,{0,2,36},{1,0,0},.1f);
    const auto played=audio.GetStats();
    if(available&&(played.successfulPlays<2||played.droppedPlays||game.requests[unsigned(Sound::Wind)]!=1))return 26;
    state.active=false;game.Tick(audio,state,{},{1,0,0},.016f);
    if(audio.GetStats().activeVoices)return 27;
    game.Shutdown(audio);if(audio.GetStats().residentSounds||audio.GetStats().residentBytes)return 28;
    std::cout<<"Game audio PASS; clips="<<SoundCount<<" bytes="<<bytes<<" steps="<<steps<<" device="<<available<<" plays="<<played.successfulPlays<<" dropped="<<played.droppedPlays<<" resources_released=1\n";
}
