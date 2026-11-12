#pragma once
#include "MistwatchPlaytestClock.h"
#include "MistwatchSession.h"
#include "MistwatchWorld.h"
#include "MistwatchFieldInput.h"
#include "MistwatchObjectiveGuide.h"
#include "MistwatchActionFeedback.h"
#include "MistwatchCameraSettings.h"
#include <Aevrynd/Renderer.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <functional>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace Mistwatch::FieldPresentation {
enum class TitleCheckpoint { Unchecked, Missing, Found, Unreadable, NoPath };
enum class DefeatCause { Unknown, Ravine, Boar };

inline const char* TitleCheckpointHint(TitleCheckpoint status) {
    switch(status) {
    case TitleCheckpoint::Missing:return "尚無存檔。進入遊戲後可按 F5 儲存。";
    case TitleCheckpoint::Found:return "找到存檔，按 F9 嘗試繼續。";
    case TitleCheckpoint::Unreadable:return "目前無法載入存檔。";
    case TitleCheckpoint::NoPath:return "目前無法使用存檔功能。";
    default:return "F9 嘗試讀取存檔。";
    }
}

struct Hud {
    std::string objective,navigation,instruction,context,controls;
    std::vector<std::string> journal;
    bool valid{},compactJournal{};
};
inline void PresentCameraSettings(Hud& hud,const CameraSettings& settings,bool started,bool paused) {
    // 選單才使用原方位列；遊玩時保留實際目標導航，也不擠壓任務與存讀檔提示。
    if(hud.valid&&settings.Valid()&&(!started||paused))hud.navigation=settings.MenuLine();
}
// 安全重試只在執行中的場景出現，不搶占暫停、倒下或結案提示。
inline bool PresentSafeRetry(Hud& hud,const Session& session,bool started,bool paused,bool recoveryRequested) {
    if(!recoveryRequested||!hud.valid||!started||paused||!session.Alive()||session.beaconLit)return false;
    hud.navigation.clear();
    hud.instruction="目前位置需要調整。可按 R 嘗試回到安全位置。";
    hud.controls="R 安全重試 · Esc 暫停 · J 手冊";
    return true;
}
inline constexpr std::string_view JournalTitle(const Session& session) noexcept {
    return session.beaconLit?"巡守結案（J 關閉）":"巡守手冊（J 關閉；手冊不會自動暫停遊戲）";
}
// 18 像素字型縮至約 10 像素會丟失中文字細筆畫；最低維持原生字框尺度。
inline constexpr float MinimumChineseTextScale=2.6f;
inline const char* Bearing(ObjectiveBearing bearing) {
    switch(bearing) {
    case ObjectiveBearing::AtTarget:return "目標旁";
    case ObjectiveBearing::Ahead:return "前方";
    case ObjectiveBearing::Right:return "右側";
    case ObjectiveBearing::Behind:return "後方";
    case ObjectiveBearing::Left:return "左側";
    }
    return "方位待確認";
}
inline const char* Node(FieldRelayTask::CircuitNode node) {
    switch(node) {
    case FieldRelayTask::CircuitNode::Source:return "電源";
    case FieldRelayTask::CircuitNode::ConnectorA:return "接頭 A";
    case FieldRelayTask::CircuitNode::ConnectorB:return "接頭 B";
    case FieldRelayTask::CircuitNode::Receiver:return "中繼";
    case FieldRelayTask::CircuitNode::Ground:return "接地";
    }
    return "無效";
}
inline const char* Route(unsigned route) {
    constexpr std::array names{"河谷涉水：未確認支撐","已修橋主路：已有通路","北側捷徑：尚未確認"};
    return route<names.size()?names[route]:"入口資料無效";
}
inline std::string Circuit(const Session& session) {
    const auto trace=FieldRelayTask::TraceCircuit(session.relayPatchA,session.relayPatchB);
    std::string result;
    for(unsigned i=0;i<trace.pathLength;++i){if(i)result+=" > ";result+=Node(trace.path[i]);}
    switch(trace.status) {
    case FieldRelayTask::CircuitStatus::Connected:return result+"（連續）";
    case FieldRelayTask::CircuitStatus::Grounded:return result+"（流向接地）";
    case FieldRelayTask::CircuitStatus::Loop:return result+"（回路，未抵中繼）";
    default:return "線路資料無效";
    }
}
inline std::string_view Toast(ActionFeedbackEvent event) {
    switch(event) {
    case ActionFeedbackEvent::Saved:return "上次操作：已儲存";
    case ActionFeedbackEvent::SaveFailed:return "上次操作：寫入失敗，未確認新存檔";
    case ActionFeedbackEvent::SavePositionUnavailable:return "上次操作：此位置無法安全建立存檔";
    case ActionFeedbackEvent::SaveUnavailableAfterDefeat:return "倒下時無法儲存，已保留原存檔";
    case ActionFeedbackEvent::Loaded:return "上次操作：已載入";
    case ActionFeedbackEvent::LoadUnavailable:return "上次操作：無法載入，保留目前狀態";
    case ActionFeedbackEvent::RetryFromBridge:return "已回到橋頭，保留已完成進度";
    case ActionFeedbackEvent::RetryFromStart:return "已回到起點，保留已取得物資";
    case ActionFeedbackEvent::BridgeRepaired:return "跨谷橋已修復。沿橋前往谷北。";
    default:return {};
    }
}
inline std::string_view LastAction(std::string_view text) {
    constexpr std::array<std::pair<std::string_view,std::string_view>,45> names{{
        {"ENTER: begin expedition","等待開始巡守"},{"Saved","已儲存"},{"Save failed","儲存失敗"},
        {"Loaded","已載入"},{"Save unavailable","無法載入，保留目前狀態"},
        {"Save position unavailable","此位置無法安全建立存檔"},{"Audio settings save failed","音量設定寫入失敗"},
        {"New expedition","新一輪巡守已開始"},
        {"Retry from bridgehead","已回到橋頭，保留已完成進度"},
        {"Retry from starting point","已回到起點，保留已取得物資"},
        {"Low ceiling - move clear before standing","上方空間不足；保持蹲姿移出屋簷，再放開 Ctrl"},
        {"Headroom clear - standing restored","已離開低頂，恢復站立"},
        {"Initial body overlap - R: safe retry / ESC: pause","目前位置有碰撞。可按 R 嘗試回到安全位置。"},
        {"NPC has no legal yield route - R: safe retry / ESC: pause","附近的野豬無法讓路。可按 R 嘗試回到安全位置。"},
        {"Safe retry - expedition progress retained","已回到安全位置，巡守進度已保留。"},
        {"Safe retry unavailable - ESC pauses; prior state retained","暫時無法回到安全位置，進度已保留。可按 Esc 暫停。"},
        {"Camera clearance - movement held","鏡頭空間不足，本步保留原位置"},{"Fell into the ravine - R to retry","跌入谷底，按 R 重試"},
        {"Antenna rotated - find the green signal lock, then E","天線已旋轉，訊號 100 後按 E"},
        {"Move character clear before bridge repair","目前站位無法修橋。移到橋頭旁，再按 E。"},
        {"Move camera clear before bridge repair","鏡頭空間不足。拖曳轉動鏡頭後，再按 E。"},
        {"Objective updated","任務已更新"},
        {"Find tools in west shelter (CTRL), and both timber caches","請蹲入西側低棚取工具，並找齊兩組木料"},
        {"Ridge relay: 1 left / 2 right, green signal lock, then E","中繼站：依近場指引操作，訊號 100 後按 E"},
        {"Move closer to the objective","請接近目前目標"},{"Boar defeated - find the ranger log","野豬已擊退，接著找巡守日誌"},
        {"Leave the marked circle - Q: evade","離開預警圈，Q 閃避沒有無敵時間"},
        {"Boar recovering - counter with F","野豬恢復中；符合近場條件才能按 F 反擊"},
        {"Counter landed - prepare for the next attack","反擊命中；準備閃避下一次攻擊"},
        {"Counter cooling down - keep clear","反擊尚未恢復；保持安全距離"},
        {"Boar approaching - watch for the marked circle","野豬正在接近；留意地面預警圈"},
        {"Capture mode - initial playable scene","初始場景擷取，非遊玩紀錄"},
        {"TEST FIXTURE - not a playthrough or player save","測試配置，非通關或玩家存檔"},
        {"World collision query unavailable - controls retained","場景碰撞資料暫時無法確認；位置已保留。可按 Esc 暫停。"},
        {"Input queue full - ESC cancels pending controls","尚有操作未完成；按 Esc 暫停可清除待處理操作。"},
        {"Frame preparation unavailable - input retained","本次操作暫時無法準備；已保留操作。可按 Esc 暫停。"},
        {"Frame transaction unavailable","本次移動暫時無法處理；可按 Esc 暫停後再繼續。"},
        {"Invalid expedition state - prior frame retained","巡守資料需要確認；已保留原狀態。可按 Esc 暫停。"},
        {"Contact query unavailable - controls retained","接觸位置暫時無法確認；位置已保留。可按 Esc 暫停。"},
        {"Body contact policy pending - controls retained; ESC pauses","身體接觸尚未處理完成；位置已保留。可按 Esc 暫停。"},
        {"Foot placement unavailable - controls retained","落腳位置暫時無法確認；位置已保留。可按 Esc 暫停。"},
        {"Movement unavailable - controls retained","本次移動尚未完成；位置已保留。可按 Esc 暫停。"},
        {"Boar footing unavailable - controls retained","野豬落腳位置暫時無法確認；位置已保留。可按 Esc 暫停。"},
        {"NPC movement unavailable - controls retained","野豬移動尚未完成；位置已保留。可按 Esc 暫停。"},
        {"Frame candidate unavailable - prior state and controls retained","本次操作尚未完成；狀態與操作已保留。可按 Esc 暫停。"}
    }};
    for(const auto& item:names)if(text==item.first)return item.second;
    return text;
}

// 失敗狀態不能沿用戰鬥提示；保留活人既有操作回饋與獨立存讀檔通知。
inline std::string StatusLine(const Session& session,std::string_view lastAction) {
    if(!session.Alive())return "生命 0；移動、跳躍、互動與戰鬥已停止。";
    return "生命 "+std::to_string(session.health)+"；上次操作："+std::string(lastAction);
}

inline const char* ShelterToolApproachHint(const Session& session) {
    if(session.repairMaterials&1u)return nullptr;
    static const auto roof=RepairShelterSolids().front();
    constexpr float approachDepth=1.5f,groundLevelTolerance=.6f;
    const float eastEdge=roof.center.x+roof.extents.x;
    const auto feet=session.feet;
    if(feet.x<roof.center.x-roof.extents.x||feet.x>eastEdge+approachDepth||
       std::abs(feet.z-roof.center.z)>roof.extents.z||
       std::abs(feet.y-Session::repairSites[0].y)>groundLevelTolerance)return nullptr;
    return feet.x>=eastEdge?"按住 Ctrl 從低棚東側開口進入，靠近工具後按 E。":
        "保持蹲姿（按住 Ctrl），靠近工具後按 E。";
}

// 只讀原引導及 Session 的近場取物門檻；不另造安全通路或提前公布轉盤答案。
inline Hud Build(const Session& session,const ObjectiveGuide& guide,const Encounter& enemy,
    bool started,bool paused,bool journalOpen,TitleCheckpoint checkpoint=TitleCheckpoint::Unchecked,
    DefeatCause defeat=DefeatCause::Unknown) {
    Hud hud;
    if(!session.Valid()||!guide.valid||(guide.hasTarget&&(!std::isfinite(guide.distance)||guide.distance<0||
        guide.distance>double((std::numeric_limits<unsigned>::max)())-1||guide.index>=3))){hud.objective="任務資料待確認";return hud;}
    // 追蹤資料不可沿用舊近場名字解讀遠端距離；舊非追蹤fixture維持原語意。
    if(guide.relayTrackingActive&&(!guide.hasTarget||!CanTrackRelay(session,guide.navigationRelayIndex))){
        hud.objective="任務資料待確認";return hud;
    }
    hud.valid=true;
    hud.controls="WASD 移動 · Shift 跑步 · Space 跳躍 · Ctrl 蹲下 · Q 後撤／WASD＋Q 定向閃避 · E 互動 · F 反擊";
    hud.controls+=started?" · Esc 地圖 · J 手冊":" · J 手冊";
    const auto count=[](unsigned bits){return unsigned(bool(bits&1))+unsigned(bool(bits&2))+unsigned(bool(bits&4));};
    std::string target;
    const bool cabinetNeedsClue=session.CurrentStage()==Stage::Fuse&&!session.fuseClueRead&&
        session.NearInterior(Session::fuse);
    switch(session.CurrentStage()) {
    case Stage::Supplies: {
        hud.objective="備齊跨谷物資：三箱補給、工具與兩組木料";
        target="補給箱 "+std::to_string(guide.index+1);
        hud.instruction=guide.useReady?"E：拾取補給":"沿主土路尋找箱子；沿途也能先取木料。";
        // 工具就緒包含正式蹲姿門檻；木料仍以副本查原門檻，不改任務或長程導航。
        if(started&&!paused&&session.Alive()&&!guide.useReady){
            if(guide.earlyToolsReady)hud.instruction="E：拾取低棚工具。";
            else {
                if(const auto* hint=ShelterToolApproachHint(session))hud.instruction=hint;
                for(unsigned site:{1u,2u}){
                    auto pickupProbe=session;
                    if(pickupProbe.CollectRepairMaterial(site,false)){
                        hud.instruction=site==1?"E：拾取東南木料。":"E：拾取西岸木料。";
                        break;
                    }
                }
            }
        }
        hud.context="補給 "+std::to_string(count(session.collected))+" / 3；工具與木料 "+std::to_string(count(session.repairMaterials))+" / 3";break;
    }
    case Stage::RepairMaterials: {
        constexpr std::array names{"西側低棚工具","東南木料","西岸木料"};
        target=guide.shelterApproach?"低棚入口路線":names[guide.index%3];hud.objective="取齊工具與兩組木料，修通跨谷橋";
        hud.instruction=guide.useReady?"E：拾取維修物資":guide.shelterApproach?
            "沿指引繞過棚外，到東側開口後按住 Ctrl 蹲入。":guide.index==0?
            "由低棚東側開口進入，按住 Ctrl 接近工具。":"沿南岸支路找木料，接近後按 E。";
        if(started&&!paused&&guide.index==0&&!guide.useReady&&!guide.shelterApproach){
            if(const auto* hint=ShelterToolApproachHint(session))hud.instruction=hint;
        }
        hud.context=std::string("工具：")+((session.repairMaterials&1)?"已取得":"未取得")+"；木料 "+std::to_string(count(session.repairMaterials&6))+" / 2";break;
    }
    case Stage::Bridge:
        target="跨谷橋";hud.objective="修橋，建立通往谷北聚落的入口";
        hud.instruction=guide.bridgeIntent==BridgeRepairIntent::AttemptRequiresAuthoritativeCheck?
            "E：嘗試修橋。":"沿主土路到橋頭。";
        hud.context="工具與木料已齊。修好後可沿橋進入谷北。";break;
    case Stage::Encounter:
        target="野豬";hud.objective="離開攻擊預警，在恢復空檔反擊";
        if(enemy.phase==EnemyPhase::Windup)hud.instruction="離開橙色攻擊區；Q 閃避沒有無敵時間。";
        else switch(guide.counterOpportunity){
        case CounterOpportunity::Ready:hud.instruction="F：本次恢復空檔可反擊一次。";break;
        case CounterOpportunity::Used:hud.instruction="本次已反擊；準備下一次攻擊。";break;
        case CounterOpportunity::Evading:hud.instruction="等閃避動作結束，出現 F 提示時再反擊。";break;
        case CounterOpportunity::Cooldown:hud.instruction="反擊尚在冷卻，留意下一次攻擊。";break;
        case CounterOpportunity::TooFar:hud.instruction="恢復空檔：進入 2.5 公尺內，出現 F 提示時反擊。";break;
        case CounterOpportunity::HeightDifference:hud.instruction="高低差過大；先移到與野豬相近的地面。";break;
        case CounterOpportunity::Obstructed:hud.instruction="反擊路徑被擋住，繞到能接近野豬的位置。";break;
        case CounterOpportunity::Closed:hud.instruction="留意蓄力預警，避免直迎衝撞。";break;
        }
        hud.context="野豬生命 "+std::to_string(enemy.health)+" / 3";
        if(started&&!paused&&session.Alive())
            hud.context+="；"+(guide.evadeReady?std::string("Q 可閃避"):"Q 目前不可用：冷卻、蹲下或離地");break;
    case Stage::Fuse:
        hud.objective="讀巡守日誌，開啟工棚的保險絲櫃";
        target=session.fuseClueRead?"西側工棚保險絲櫃":"北側巡守日誌";
        hud.instruction=!session.fuseClueRead?(guide.useReady?"E：閱讀巡守日誌":"由木屋西門進入，接近日誌。"):
            guide.useReady?"E：打開櫃子，取得保險絲。":guide.tuneReady?"1、2、3：轉動各轉盤；可用 J 重看已讀線索。":"由工棚東側開口進入，接近櫃子。";
        if(guide.settlementApproach){
            target=session.fuseClueRead?"工棚東側入口":"木屋西側入口";
            hud.instruction=session.fuseClueRead?"沿箭頭繞到工棚東側入口。":"沿箭頭繞到木屋西側入口。";
        }else if(guide.rangerDeskApproach){
            target="木屋內繞行";
            hud.instruction="沿箭頭繞過桌子，靠近日誌後按 E。";
        }
        if(cabinetNeedsClue)hud.instruction="保險絲櫃需要日誌線索；先到北側木屋讀完日誌，再回來。";
        hud.context=session.fuseClueRead?"日誌線索：2 - 0 - 3":"請先到北側木屋閱讀日誌。";
        if(session.fuseClueRead&&session.NearInterior(Session::fuse))hud.context+="；目前 "+std::to_string(session.FuseDial(0))+" - "+std::to_string(session.FuseDial(1))+" - "+std::to_string(session.FuseDial(2));break;
    case Stage::Generator:
        target="東側發電機";hud.objective="裝回保險絲，恢復谷北設備供電";
        hud.instruction=guide.useReady?"E：三燈已亮，啟動發電機。":guide.tuneReady?"1 切換 A+B；2 切換 B+C；3 切換 C；讓三盞燈全亮。":"沿聚落東側支路接近發電機。";
        hud.context="已亮燈 "+std::to_string(count(session.circuit))+" / 3";
        if(guide.tuneReady||guide.useReady){
            hud.context+=(session.circuit&1u)?"；A 亮":"；A 熄";
            hud.context+=(session.circuit&2u)?"、B 亮":"、B 熄";
            hud.context+=(session.circuit&4u)?"、C 亮":"、C 熄";
        }
        break;
    case Stage::Relays: {
        constexpr std::array names{"西側山脊中繼站","東側瞭望中繼站","西北中繼站"};
        const unsigned index=guide.index%3;
        const bool tracking=guide.relayTrackingActive&&guide.index<3&&CanTrackRelay(session,guide.navigationRelayIndex);
        const unsigned navigation=tracking?guide.navigationRelayIndex:index;
        target=names[navigation];hud.objective="逐站查線、調向與確認救援入口";
        hud.context="已連線 "+std::to_string(count(session.relayLinked))+" / 3";
        if(!session.NearRelay(index)){hud.instruction="沿主路與上坡支路接近站台地面；三站可任意順序。";break;}
        if(index==0&&!(session.relayFieldTasks&FieldRelayTask::WestCircuitBit)){
            hud.instruction="1 切換接頭 A；2 切換接頭 B；E 確認電源是否抵達中繼。";
            hud.context=Circuit(session);
        }else if(index==2&&!(session.relayFieldTasks&FieldRelayTask::NorthWestRouteBit)){
            hud.instruction="3 切換入口卡；E 確認通路；1、2 仍可調向。";
            hud.context=Route(session.extractionRoute);
        }else{
            hud.instruction=guide.useReady?"訊號 100：按 E 連站。":"1 向左、2 向右旋轉天線；訊號 100 後按 E。";
            hud.context+="；訊號 "+std::to_string(session.RelaySignal(index));
        }
        if(tracking)hud.context="近場："+std::string(names[index])+"；"+hud.context;
        break;
    }
    case Stage::Beacon:
        target="谷中救援信標";hud.objective="送出救援定位與已確認的橋頭入口";
        hud.instruction=guide.useReady?"E：送出信標。":"回主土路，依方位尋找藍色面板的信標。";
        hud.context="橋已修復；供電與三站已完成；入口已確認。";break;
    case Stage::Complete:
        hud.objective="巡守完成：救援台已收到定位";
        hud.instruction="救援台：由南側已修橋入谷。這次巡守結束。";
        if(started&&!paused&&session.Alive()){
            hud.instruction="巡守已完成；可調整鏡頭檢視，角色與任務不再前進。";
            hud.controls="左鍵拖曳環繞／滾輪縮放／←→ 旋轉 · F5 保存 · F9 恢復 · R 重開 · Esc 暫停 · J 結案";
        }
        hud.context="F5 保存；F9 恢復；R 開始新一輪。";break;
    }
    if(guide.hasTarget)hud.navigation=target+"；"+Bearing(guide.bearing)+"；約 "+std::to_string(unsigned(std::ceil(guide.distance)))+" 公尺（方位提示，不保證直線可通行）";
    if(!session.Alive()){
        hud.objective="巡守失敗";
        if(defeat==DefeatCause::Ravine)hud.objective+=" · 跌落河谷";
        if(defeat==DefeatCause::Boar)hud.objective+=" · 遭野豬擊倒";
        hud.navigation.clear();
        hud.instruction=session.bridgeRepaired?"R：從橋頭重試，保留已完成任務。":"R：從起點重試，保留已取得物資。";
        hud.context="補給 "+std::to_string(count(session.collected))+" / 3；工具與木料 "+std::to_string(count(session.repairMaterials))+" / 3；橋："+(session.bridgeRepaired?std::string("已修復"):"未修復");
        hud.controls="R 重試 · F9 嘗試讀檔 · Esc 暫停 · J 查看手冊";
    }
    if(!started){
        hud.objective="孤橋復聯：暴雨切斷谷北通訊";
        hud.instruction="Enter 開始新巡守；J 查看任務手冊。";
        hud.context=TitleCheckpointHint(checkpoint);
    }
    else if(paused){
        hud.objective.insert(0,"已暫停 · ");
        if(!session.Alive()){
            hud.instruction=session.bridgeRepaired?"先按 Esc 繼續，再按 R 從橋頭重試。":"先按 Esc 繼續，再按 R 從起點重試。";
        }else{
            hud.instruction=journalOpen?(session.beaconLit?"按 J 關閉結案，查看巡守地圖。":"按 J 關閉手冊，查看巡守地圖。"):"角色與世界暫停更新。";
            if(session.beaconLit)hud.context="按 Esc 繼續後，可按 R 開始新一輪。";
        }
        hud.controls=session.Alive()?"Esc 繼續 · F5 儲存":"Esc 繼續";
        hud.controls+=" · F9 載入 · M 靜音 · PageUp / PageDown 音量";
        hud.controls+=session.beaconLit?" · J 結案":" · J 手冊";
    }
    if(started&&!paused&&!guide.useReady&&!cabinetNeedsClue&&FieldInput::RepeatContext(session)){
        // 只描述本模式可用的數字鍵，不改調整、確認或長按規則。
        const bool thirdKey=session.CurrentStage()!=Stage::Relays||
            (session.NearRelay(FieldRelayTask::NorthWestRelay)&&
             !(session.relayFieldTasks&FieldRelayTask::NorthWestRouteBit));
        hud.instruction+=thirdKey?" 點按微調；長按 1、2、3 連續調整；E 仍需另按確認。":
            " 點按微調；長按 1、2 連續調整；E 仍需另按確認。";
    }
    if(!journalOpen)return hud;
    // 手冊打開時方向箭頭隱藏。保留目的地，但提示如何恢復指引。
    if(started&&!paused&&session.Alive()&&!guide.useReady&&!guide.tuneReady&&!cabinetNeedsClue&&
        (guide.shelterApproach||guide.settlementApproach||guide.rangerDeskApproach))
        hud.instruction="按 J 關閉手冊，再沿方向箭頭前進。";
    if(!session.Alive()){
        hud.journal={"生命已耗盡；目前不能移動、拾取物品或反擊。",
            session.bridgeRepaired?"恢復遊戲後按 R 回橋頭，已取得物資與修橋進度會保留。":"恢復遊戲後按 R 回起點，已取得物資會保留。",
            "F9 嘗試載入既有存檔；沒有可用存檔時保留目前狀態。",
            "暫停中可按 F9 讀檔；按 Esc 繼續後可用 R 重試。J 關閉手冊。"};
        return hud;
    }
    if(session.beaconLit){
        hud.journal={"救援任務完成",
            "跨谷橋已修復，谷北已恢復供電，三座中繼站已連線。",
            "救援台已收到定位與橋頭入口。F5 可保存完成進度。"};
        return hud;
    }
    hud.compactJournal=session.Alive()&&started&&!paused&&session.CurrentStage()==Stage::Encounter;
    if(hud.compactJournal){hud.journal={"J 手冊摘要：先離橙圈，恢復時近場 F；Q 不提供免傷。","遊戲未暫停；Esc 才會暫停。"};return hud;}
    hud.journal={"巡守手冊：修橋 > 恢復供電 > 三站復聯 > 送出信標。",
        "主路穿過橋頭；箱子與木料在南岸，工具在西側低棚。",
        "補給 "+std::to_string(count(session.collected))+"/3；工具與木料 "+std::to_string(count(session.repairMaterials))+"/3；橋："+(session.bridgeRepaired?std::string("已修復"):"尚未修復"),
        "互動需接近且符合站位；方位箭頭不是穿牆通行線。",
        !started?"Enter 開始新巡守；J 關閉手冊。":
        paused?"F5 儲存／F9 載入；完成後仍維持暫停。J 關閉手冊查看地圖。":
        "操作：拖曳／滾輪調鏡頭；C 回正；F5 儲存；F9 載入；Esc 暫停看地圖。"};
    if(session.guardianDefeated)hud.journal.push_back("野豬已擊退：谷北木屋與工棚內有供電線索。");
    if(session.fuseClueRead)hud.journal.push_back("已讀日誌：保險絲櫃轉盤 2 - 0 - 3；1、2、3 分別轉對應轉盤，調至線索數字。");
    if(session.fuseCollected)hud.journal.push_back("發電機：1 切換 A+B，2 切換 B+C，3 切換 C；三燈全亮才送電。");
    if(session.powerRestored){
        constexpr std::array stationNames{"西站","東站","西北站"};
        std::string progress="中繼進度 ";
        for(unsigned station=0;station<stationNames.size();++station){
            if(station)progress+=" · ";
            progress+=stationNames[station];
            progress+=' ';
            if(session.relayLinked&(1u<<station))progress+="已連線";
            else if(!session.RelayFieldTaskReady(station))
                progress+=station==FieldRelayTask::WestRelay?"待查線":"待確認入口";
            else progress+=session.RelaySignal(station)==100?"待連線":"待調向";
        }
        hud.journal.push_back(std::move(progress));
        hud.journal.push_back("三站沒有固定先後：西站查線，東站調向，西北站回報入口。");
        hud.journal.push_back("西站接線圖：電源先到 A；A 可接地、接 B 或回電源。");
        hud.journal.push_back("B 可接中繼、接地或回 A；檢查畫面上的實際電流路徑。");
        hud.journal.push_back("西北入口卡：涉水與未知捷徑不能回報安全；使用已修橋主路。");
    }
    if(session.relayLinked&2)hud.journal.push_back("救援台已接到東站呼叫：請提供谷北定位與可用入口。");
    return hud;
}

inline void AppendDebriefTimes(Hud& hud,const PlaytestClock::LegSeconds& elapsed){
    constexpr std::array names{"南岸備料與修橋","過橋與野豬遭遇","聚落尋線索與供電","中繼站與救援信標"};
    for(std::size_t index=0;index<names.size();++index){
        const auto seconds=elapsed[index].count();
        hud.journal.push_back(std::string(names[index])+"  "+std::to_string(seconds/60)+" 分 "+std::to_string(seconds%60)+" 秒");
    }
}

inline void AppendPlaytestDebrief(Hud& hud,const PlaytestClock& clock,bool paused,
    bool playerFacing,bool hasSavePath) {
    if(const auto legs=clock.CompletedLegs()){
        AppendDebriefTimes(hud,*legs);
        if(playerFacing)hud.journal.push_back(hasSavePath?"K 匯出本輪用時至存檔資料夾":"目前沒有可用的存檔位置，無法匯出用時。");
    }else if(playerFacing){
        hud.journal.push_back("本輪沒有完整計時。讀檔後接續的進度不會計入首輪用時。");
        hud.journal.push_back(paused?"先按 Esc 繼續，再按 R 可開始新的計時巡守。":"按 R 可開始新的計時巡守。");
    }
}

// 以實際字型量測分行，不截斷 UTF-8；寬度不夠單字也不能被悄悄裁掉。
template<class Measure> inline std::vector<std::string> Wrap(std::string_view text,float maximumWidth,Measure measure) {
    if(!std::isfinite(maximumWidth)||maximumWidth<=0)throw std::runtime_error("HUD width invalid");
    std::vector<std::string> lines;std::string line;
    for(std::size_t offset=0;offset<text.size();){
        const unsigned char first=static_cast<unsigned char>(text[offset]);
        const std::size_t length=first<128?1:(first&0xe0)==0xc0?2:(first&0xf0)==0xe0?3:(first&0xf8)==0xf0?4:0;
        if(!length||offset+length>text.size())throw std::runtime_error("HUD invalid UTF8");
        for(std::size_t i=1;i<length;++i)if((static_cast<unsigned char>(text[offset+i])&0xc0)!=0x80)throw std::runtime_error("HUD invalid UTF8 continuation");
        if((length==2&&first<0xc2)||(length==3&&first==0xe0&&static_cast<unsigned char>(text[offset+1])<0xa0)||
            (length==3&&first==0xed&&static_cast<unsigned char>(text[offset+1])>=0xa0)||
            (length==4&&(first>0xf4||(first==0xf0&&static_cast<unsigned char>(text[offset+1])<0x90)||
                (first==0xf4&&static_cast<unsigned char>(text[offset+1])>0x8f))))throw std::runtime_error("HUD noncanonical UTF8");
        const auto glyph=text.substr(offset,length);offset+=length;
        if(glyph=="\n"){lines.push_back(line);line.clear();continue;}
        const auto next=line+std::string(glyph);
        const float width=measure(next);
        if(!std::isfinite(width)||width<0)throw std::runtime_error("HUD text measurement invalid");
        if(width>maximumWidth){
            if(line.empty())throw std::runtime_error("HUD glyph exceeds width");
            lines.push_back(line);line=std::string(glyph);
            const float glyphWidth=measure(line);
            if(!std::isfinite(glyphWidth)||glyphWidth<0||glyphWidth>maximumWidth)throw std::runtime_error("HUD glyph exceeds width");
        }else line=next;
    }
    if(!line.empty()||lines.empty())lines.push_back(line);
    return lines;
}

// 底板直接使用 SystemText 的同一套字形量測；不改字級、內容或既有文字座標。
struct PanelRect {
    float left{},top{},width{},height{};
    float Right() const noexcept{return left+width;}
    float Bottom() const noexcept{return top+height;}
};
struct MeasuredHudLayout {
    PanelRect top,bottom,journal,toast;
    std::vector<std::string> journalRows;
    std::string audioText;
    float audioTextX{},audioTextY{12};
    float toastTextY{};
    float journalTitleY{108},journalRowsY{145};
    bool showJournal{},showToast{},showAudio{};
};
inline MeasuredHudLayout MeasureLayout(const Aevrynd::DrawList& draw,const Hud& hud,
    const Session& session,std::string_view lastAction,const ActionFeedbackToast& toast,bool started,
    bool paused=false,bool audioAvailable=true,bool muted=false,unsigned volume=80) {
    MeasuredHudLayout result;
    const auto measured=[&](std::string_view text,float scale,float maximumWidth){
        const auto size=draw.MeasureSystemText(text,7.f*scale);
        if(!std::isfinite(size.x)||!std::isfinite(size.y)||size.x<0||size.y<0||size.x>maximumWidth)
            throw std::runtime_error("HUD actual line exceeds layout");
        return size;
    };
    const auto contained=[](const PanelRect& panel,float x,float y,Aevrynd::Vec2 size){
        if(x<panel.left||y<panel.top||x+size.x>panel.Right()||y+size.y>panel.Bottom())
            throw std::runtime_error("HUD measured text exceeds panel");
    };
    const auto title=measured("霧境守望 · 孤橋復聯",2.9f,1240);
    const auto objective=measured(hud.objective,2.7f,1240);
    result.top={8,0,(std::max)(title.x,objective.x)+24,76};
    contained(result.top,20,12,title);contained(result.top,20,42,objective);
    result.showAudio=!started||paused;
    if(result.showAudio){
        if(volume>100)throw std::runtime_error("HUD audio volume outside percentage range");
        result.audioText=std::string(audioAvailable?"音效：":"音效裝置不可用；")+
            (muted?"已靜音":std::to_string(volume)+"%")+"；M 靜音，PageUp / PageDown 音量。";
        const auto audio=measured(result.audioText,2.6f,1240);
        // 音效放在標題右側，沿用同一底板；不能再被稍後繪製的手冊遮住。
        result.audioTextX=1260-audio.x;
        if(result.audioTextX<20+title.x+24||result.audioTextY+audio.y+4>42)
            throw std::runtime_error("HUD audio has no reserved gap beside title");
        result.top.width=(std::max)(result.top.width,result.audioTextX+audio.x+12-result.top.left);
        contained(result.top,result.audioTextX,result.audioTextY,audio);
    }
    const std::array<Aevrynd::Vec2,5> sizes{
        measured(hud.navigation,2.6f,1240),measured(hud.instruction,2.7f,1240),
        measured(hud.context,2.6f,1240),measured(hud.controls,2.6f,1240),
        measured(StatusLine(session,lastAction),2.6f,1240)};
    float width=0;
    for(const auto size:sizes)width=(std::max)(width,size.x);
    result.bottom={8,576,width+24,144};
    constexpr std::array<float,5> rows{584,609,634,661,690};
    for(std::size_t i=0;i<sizes.size();++i)contained(result.bottom,20,rows[i],sizes[i]);
    result.showJournal=!hud.journal.empty();
    const float journalBottomFloor=session.beaconLit?0.f:520.f;
    if(result.showJournal){
        if(hud.compactJournal){
            result.journal={20,92,1240,70};
            float y=101;
            for(const auto& text:hud.journal){
                contained(result.journal,40,y,measured(text,2.6f,1200));y+=28;
            }
        }else{
            for(const auto& text:hud.journal){
                const auto wrapped=Wrap(text,1120.f,[&](std::string_view value){
                    return draw.MeasureSystemText(value,7.f*2.6f).x;
                });
                result.journalRows.insert(result.journalRows.end(),wrapped.begin(),wrapped.end());
            }
            if(result.journalRows.size()>16)throw std::runtime_error("HUD journal exceeds measured row budget");
            // 結案底板貼合文字，讓玩家繼續看見救援信標及周圍場景。
            const auto journalTitle=measured(JournalTitle(session),2.9f,1120);
            const float bottomPadding=session.beaconLit?16.f:4.f;
            float bottom=(std::max)(journalBottomFloor,result.journalTitleY+journalTitle.y+bottomPadding);
            float contentWidth=journalTitle.x;
            float y=result.journalRowsY;
            for(const auto& text:result.journalRows){
                const auto size=measured(text,2.6f,1120);
                bottom=(std::max)(bottom,y+size.y+bottomPadding);
                contentWidth=(std::max)(contentWidth,size.x);
                y+=24;
            }
            result.journal={60,80,session.beaconLit?std::ceil(contentWidth)+40.f:1160.f,bottom-80};
            contained(result.journal,80,result.journalTitleY,journalTitle);
        }
    }
    result.showToast=started&&toast.Visible();
    if(result.showToast){
        const auto size=measured(Toast(toast.Event()),2.6f,680);
        // 手冊和通知之間保留八像素，不讓後繪製的手冊覆蓋操作結果。
        const float top=result.showJournal?(std::max)(113.f,result.journal.Bottom()+8.f):113.f;
        result.toast={8,top,size.x+24,(std::max)(30.f,size.y+9.f)};
        result.toastTextY=top+7;
        if(result.toast.Bottom()+4>result.bottom.top&&result.showJournal&&!hud.compactJournal){
            // 極限列數的真字形高度可能超過名義字級；只移動手冊，不縮字或壓縮列距。
            const float shift=std::ceil(result.toast.Bottom()+4-result.bottom.top);
            if(shift>result.journalTitleY-result.journal.top-8)
                throw std::runtime_error("HUD journal cannot reserve readable toast gap");
            result.journalTitleY-=shift;result.journalRowsY-=shift;
            result.journal.height=(std::max)(journalBottomFloor,result.journal.Bottom()-shift)-result.journal.top;
            result.toast.top=(std::max)(113.f,result.journal.Bottom()+8.f);
            result.toastTextY=result.toast.top+7;
        }
        contained(result.toast,20,result.toastTextY,size);
        if(result.toast.Bottom()+4>result.bottom.top)
            throw std::runtime_error("HUD toast has no reserved gap before bottom panel");
    }
    if(result.showJournal&&!hud.compactJournal){
        contained(result.journal,80,result.journalTitleY,measured(JournalTitle(session),2.9f,1120));
        float y=result.journalRowsY;
        for(const auto& text:result.journalRows){contained(result.journal,80,y,measured(text,2.6f,1120));y+=24;}
    }
    for(const auto panel:std::array<PanelRect,4>{result.top,result.bottom,result.journal,result.toast})
        if(panel.left<0||panel.top<0||panel.width<0||panel.height<0||panel.Right()>1280||panel.Bottom()>720)
            throw std::runtime_error("HUD panel exceeds canvas");
    return result;
}

inline void Draw(Aevrynd::DrawList& draw,const Hud& hud,const Session& session,
    std::string_view lastAction,const ActionFeedbackToast& toast,bool audioAvailable,bool muted,unsigned volume,
    bool started,bool paused) {
    using Aevrynd::Color;
    const auto layout=MeasureLayout(draw,hud,session,lastAction,toast,started,paused,audioAvailable,muted,volume);
    const auto panel=[&](const PanelRect& bounds,Color color){
        draw.Quad({bounds.left+bounds.width*.5f,bounds.top+bounds.height*.5f},{bounds.width,bounds.height},color);
    };
    const auto line=[&](float x,float y,std::string_view text,float scale,float width,Color color){
        if(std::ranges::any_of(text,[](unsigned char byte){return byte>=128;})&&scale<MinimumChineseTextScale)
            throw std::runtime_error("HUD Chinese text below readable source-pixel scale");
        const auto measured=draw.MeasureSystemText(text,7.f*scale);
        if(!std::isfinite(measured.x)||measured.x>width)throw std::runtime_error("HUD actual line exceeds layout");
        draw.SystemText({x,y},text,7.f*scale,color);
    };
    constexpr Color white{1,1,1,1},green{.7f,1,.7f,1},gold{1,.9f,.65f,1};
    panel(layout.top,{.02f,.04f,.04f,.9f});
    line(20,12,"霧境守望 · 孤橋復聯",2.9f,1240,green);
    line(20,42,hud.objective,2.7f,1240,white);
    if(layout.showAudio)line(layout.audioTextX,layout.audioTextY,layout.audioText,2.6f,1240,white);
    panel(layout.bottom,{.02f,.04f,.04f,.83f});
    line(20,584,hud.navigation,2.6f,1240,green);
    line(20,609,hud.instruction,2.7f,1240,green);
    line(20,634,hud.context,2.6f,1240,gold);
    line(20,661,hud.controls,2.6f,1240,white);
    line(20,690,StatusLine(session,lastAction),2.6f,1240,white);
    if(layout.showJournal){
        if(hud.compactJournal){
            panel(layout.journal,{.02f,.04f,.04f,.9f});
            float y=101;
            for(const auto& text:hud.journal){line(40,y,text,2.6f,1200,gold);y+=28;}
        }else{
            panel(layout.journal,{.02f,.04f,.04f,.95f});
            line(80,layout.journalTitleY,JournalTitle(session),2.9f,1120,gold);
            float y=layout.journalRowsY;
            for(const auto& text:layout.journalRows){line(80,y,text,2.6f,1120,white);y+=24;}
        }
    }
    if(layout.showToast){
        panel(layout.toast,{.02f,.04f,.04f,.9f});
        line(20,layout.toastTextY,Toast(toast.Event()),2.6f,680,gold);
    }
}
}
