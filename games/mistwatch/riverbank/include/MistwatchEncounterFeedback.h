#pragma once
#include "MistwatchEncounter.h"
#include <string>

namespace Mistwatch {
// 完成操作確實成功才取代上一幀提示；不改戰鬥時間、傷害或存檔狀態規則。
inline void UpdateEncounterFeedback(Session& session,const Encounter& encounter,std::string& message,bool standingDenied=false) {
    if(!encounter.health&&session.FinishEncounter(encounter.feet))
        message="Boar defeated - find the ranger log";
    else if(session.Valid()&&session.Alive()&&session.CurrentStage()==Stage::Encounter&&encounter.Valid()&&encounter.health){
        if(encounter.phase==EnemyPhase::Windup)message="Leave the marked circle - Q: evade";
        else if(encounter.phase==EnemyPhase::Recovery){
            if(encounter.countered)message="Counter landed - prepare for the next attack";
            else if(encounter.playerCooldown>0)message="Counter cooling down - keep clear";
            else message="Boar recovering - counter with F";
        }else if(encounter.phase==EnemyPhase::Pursuit&&!standingDenied)message="Boar approaching - watch for the marked circle";
    }
}
}
