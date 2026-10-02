#include "MistwatchPlayback.h"
#include "MistwatchEncounter.h"
#include <iostream>

int main() {
    bool started{},paused{};
    Mistwatch::UpdatePlaybackControl(started,paused,true,false,true);
    if(started||paused)return 1;
    Mistwatch::UpdatePlaybackControl(started,paused,false,true,true);
    if(started||paused)return 2;
    Mistwatch::UpdatePlaybackControl(started,paused,true,true,false);
    if(!started||paused)return 3;
    Mistwatch::Encounter enemy;
    enemy.phase=Mistwatch::EnemyPhase::Windup;enemy.phaseTime=.3f;
    const auto before=enemy.phaseTime;
    unsigned simulated{};
    for(unsigned frame=0;frame<600;++frame) {
        Mistwatch::UpdatePlaybackControl(started,paused,frame>=300,false,false);
        if(started&&!paused){enemy.Tick({0,0,45},1.f/60.f,false);++simulated;}
    }
    if(!paused||simulated||enemy.phaseTime!=before)return 4;
    Mistwatch::UpdatePlaybackControl(started,paused,true,true,false);
    if(!paused)return 5;
    Mistwatch::UpdatePlaybackControl(started,paused,true,false,true);
    if(paused)return 6;
    enemy.Tick({0,0,45},1.f/60.f,false);
    if(enemy.phaseTime==before)return 7;
    Mistwatch::UpdatePlaybackControl(started,paused,true,false,true);
    if(!paused)return 8;
    std::cout<<"playback_focus_pause=pass background_ticks=600 enemy_time_frozen=1 explicit_resume=1\n";
}
