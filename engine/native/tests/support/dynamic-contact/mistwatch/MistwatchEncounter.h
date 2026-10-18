#pragma once
#include "MistwatchSession.h"
#include <algorithm>
namespace Mistwatch {
enum class EnemyPhase { Pursuit, Windup, Recovery, Defeated };
struct Encounter {
    static constexpr float StrikeRadius=1.4f, WindupDuration=.85f, RecoveryDuration=1.2f;
    Aevrynd::Vec3 feet{Session::arena};
    Aevrynd::Vec3 strikeTarget{Session::arena};
    unsigned health{3};
    EnemyPhase phase{EnemyPhase::Pursuit};
    float phaseTime{}, playerCooldown{};
    bool countered{};
    bool Valid() const noexcept {
        if(!Session::Finite(feet)||!Session::Finite(strikeTarget)||health>3||!std::isfinite(phaseTime)||
            !std::isfinite(playerCooldown)||playerCooldown<0||playerCooldown>.65f||(health==3&&countered))return false;
        switch(phase){
        case EnemyPhase::Pursuit:return health&&phaseTime>=-.1f&&phaseTime<=0;
        case EnemyPhase::Windup:return health&&!countered&&phaseTime>0&&phaseTime<=WindupDuration;
        case EnemyPhase::Recovery:return health&&phaseTime>0&&phaseTime<=RecoveryDuration;
        case EnemyPhase::Defeated:return !health&&countered&&phaseTime>=0&&phaseTime<=RecoveryDuration;
        default:return false;
        }
    }
    unsigned Tick(Aevrynd::Vec3 player,float dt,bool attack,bool playerPathClear=true,bool strikePathClear=true) {
        if(!Session::Finite(player) || !std::isfinite(dt) || dt<=0 || dt>.1f || !health) return 0;
        playerCooldown=std::max(0.f,playerCooldown-dt);
        const float distance=std::hypot(player.x-feet.x,player.z-feet.z);
        const bool reachable=std::abs(player.y-feet.y)<2.f;
        if(attack && playerPathClear && phase==EnemyPhase::Recovery && !countered && playerCooldown<=0 && distance<=2.5f && reachable) {
            playerCooldown=.65f;
            countered=true;
            if(--health==0) {phase=EnemyPhase::Defeated;return 0;}
        }
        if(phase==EnemyPhase::Windup) {
            phaseTime-=dt;
            if(phaseTime<=0) {
                phase=EnemyPhase::Recovery;phaseTime=RecoveryDuration;
                return playerPathClear && strikePathClear && std::hypot(player.x-strikeTarget.x,player.z-strikeTarget.z)<=StrikeRadius &&
                    std::abs(player.y-strikeTarget.y)<2.f?20u:0u;
            }
        } else if(phase==EnemyPhase::Recovery) {
            phaseTime-=dt;
            if(phaseTime<=0) phase=EnemyPhase::Pursuit;
        } else if(playerPathClear && reachable && distance<=2.2f) {
            // 蓄力起點鎖定攻擊區域，與畫面預警圈共用位置和半徑。
            phase=EnemyPhase::Windup;phaseTime=WindupDuration;strikeTarget=player;countered=false;
        } else if(distance>2.2f && distance<10.f && player.z>=42.f && player.z<=56.f) {
            // 追擊落點留在觸發距離內，避免尾端步長小於 float ULP 而卡在 2.2 公尺外。
            const float step=std::min(2.f*dt,distance-2.f);
            feet.x+=(player.x-feet.x)/distance*step;
            feet.z+=(player.z-feet.z)/distance*step;
        }
        return 0;
    }
};
}
