#pragma once
#include "MistwatchSession.h"

namespace Mistwatch::FieldInput {
enum class Result { None, PatchChanged, RouteChanged, AntennaChanged,
    CircuitConfirmed, RouteConfirmed, RelayConnected, Invalid };

// 不同設備使用不同識別；離站、失效或已連線後不承接上一站的長按。
// 僅查詢既有操作範圍，不替玩家完成確認或修改任務進度。
inline std::uint64_t RepeatContext(const Session& session) {
    if(!session.Valid()||!session.Alive())return 0;
    switch(session.CurrentStage()) {
    case Stage::Fuse:return session.NearInterior(Session::fuse)?1:0;
    case Stage::Generator:return session.Near(Session::generator)?2:0;
    case Stage::Relays:
        for(unsigned index=0;index<3;++index){
            if((session.relayLinked&(1u<<index))||!session.NearRelay(index))continue;
            // E 確認後接頭鍵改為天線鍵，必須重新按下，不能沿用舊模式長按。
            const bool modeConfirmed=index==0?(session.relayFieldTasks&FieldRelayTask::WestCircuitBit)!=0:
                index==2&&(session.relayFieldTasks&FieldRelayTask::NorthWestRouteBit)!=0;
            return 3+index+(modeConfirmed?3:0);
        }
        return 0;
    default:return 0;
    }
}

// 1、2、3 的語意依目前站點切換；每次只提交一個實際近場操作。
inline Result Lever(Session& session,unsigned lever) {
    if(!session.Valid())return Result::Invalid;
    if(lever>=3||!session.Alive()||session.CurrentStage()!=Stage::Relays)return Result::None;
    if(session.NearRelay(0)&&!(session.relayFieldTasks&FieldRelayTask::WestCircuitBit))
        return lever<2&&session.CycleRelayPatch(lever)?Result::PatchChanged:Result::None;
    if(session.NearRelay(2)&&lever==2)
        return session.CycleExtractionRoute()?Result::RouteChanged:Result::None;
    if(lever<2)for(unsigned index=0;index<3;++index)
        if(session.TuneRelay(index,lever==0?-1:1))return Result::AntennaChanged;
    return Result::None;
}

// 確認與連站必須是不同次 E；失敗確認不得繼續嘗試連線。
inline Result Interact(Session& session) {
    if(!session.Valid())return Result::Invalid;
    if(!session.Alive()||session.CurrentStage()!=Stage::Relays)return Result::None;
    if(session.NearRelay(0)&&!(session.relayFieldTasks&FieldRelayTask::WestCircuitBit))
        return session.ConfirmRelayCircuit()?Result::CircuitConfirmed:Result::None;
    if(session.NearRelay(2)&&!(session.relayFieldTasks&FieldRelayTask::NorthWestRouteBit))
        return session.ConfirmExtractionRoute()?Result::RouteConfirmed:Result::None;
    for(unsigned index=0;index<3;++index)
        if(session.ConnectRelay(index))return Result::RelayConnected;
    return Result::None;
}
inline const char* Feedback(Result result) {
    switch(result) {
    case Result::PatchChanged:return "接頭已切換：檢查電源是否連到中繼，再按 E 確認。";
    case Result::RouteChanged:return "入口卡已切換：只回報已確認的通路，再按 E 確認。";
    case Result::AntennaChanged:return "天線已旋轉：訊號達到 100 後按 E 連站。";
    case Result::CircuitConfirmed:return "西站線路已確認。用 1、2 調向，訊號 100 後另按 E 連站。";
    case Result::RouteConfirmed:return "已修橋主路已回報。訊號 100 後另按 E 連站。";
    case Result::RelayConnected:return "中繼站已連線；繼續其他未完成站點。";
    case Result::Invalid:return "任務狀態無法確認，本次未執行操作。";
    default:return "尚未完成：請查看近場指引與 J 巡守手冊。";
    }
}

// 實際 E 操作後只讀目前模式；拒絕操作也要說明原因，不替玩家接線或連站。
// nullptr 代表不覆蓋呼叫端其他任務的回饋，成功結果則保留原提示及另按 E 的規則。
inline const char* InteractionFeedback(const Session& session,Result result) {
    if(result!=Result::None)return Feedback(result);
    if(!session.Valid()||!session.Alive()||session.CurrentStage()!=Stage::Relays)return nullptr;
    if(session.NearRelay(FieldRelayTask::WestRelay)&&
       !(session.relayFieldTasks&FieldRelayTask::WestCircuitBit)) {
        switch(FieldRelayTask::TraceCircuit(session.relayPatchA,session.relayPatchB).status) {
        case FieldRelayTask::CircuitStatus::Grounded:
            return "線路流向接地，尚未接通中繼；用 1、2 切換接頭，再按 E 確認。";
        case FieldRelayTask::CircuitStatus::Loop:
            return "線路形成回路，尚未接通中繼；用 1、2 切換接頭，再按 E 確認。";
        default:return nullptr;
        }
    }
    if(session.NearRelay(FieldRelayTask::NorthWestRelay)&&
       !(session.relayFieldTasks&FieldRelayTask::NorthWestRouteBit)&&
       !FieldRelayTask::SafeExtractionRoute(session.extractionRoute,session.bridgeRepaired))
        return "入口卡尚未指向已確認通路；用 3 切換入口卡，再按 E 確認。";
    for(unsigned index=0;index<3;++index)
        if(!(session.relayLinked&(1u<<index))&&session.NearRelay(index)&&
           session.RelayFieldTaskReady(index)&&session.RelaySignal(index)<100)
            return "天線尚未對準；用 1、2 調向，訊號達到 100 後按 E 連站。";
    return nullptr;
}
}
