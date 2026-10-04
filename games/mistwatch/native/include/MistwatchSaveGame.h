#pragma once
#include "MistwatchEncounter.h"
#include "MistwatchEvasion.h"
#include <array>
#include <istream>
#include <limits>
#include <locale>
#include <sstream>
#include <string>
#include <string_view>

namespace Mistwatch {
// 遊戲層快照，不改 Session 的舊格式，也不替引擎保存遊戲規則。
// 相機、輸入邊沿、音效播放及角色控制器暫態不屬於此快照；整合端須明確處理。
// 此 MISTWATCH_SAVE 1 尚未發布；EVASION 為必填，不能默默重置既有快照的閃避冷卻。
struct SaveGame {
    static constexpr std::size_t MaximumBytes=2048;
    Session session;
    Encounter encounter;
    Evasion evasion;

    static bool ValidPoint(Aevrynd::Vec3 point) {
        // 與 Session::Valid 的世界座標界限一致。
        return Session::Finite(point)&&std::abs(point.x)<=10000&&std::abs(point.y)<=10000&&std::abs(point.z)<=10000;
    }
    static bool ValidEvasion(const Evasion& action) {
        if(!Session::Finite(action.direction)||action.direction.y!=0||
           !std::isfinite(action.remaining)||!std::isfinite(action.cooldown)||
           action.remaining<0||action.remaining>Evasion::Duration||
           action.cooldown<0||action.cooldown>Evasion::Cooldown)return false;
        const float length=std::hypot(action.direction.x,action.direction.z);
        const bool zero=action.direction.x==0&&action.direction.z==0;
        const bool unit=std::isfinite(length)&&std::abs(length-1.f)<=.00001f;
        if(!zero&&!unit)return false;
        if((action.remaining>0||action.cooldown>0)&&!unit)return false;
        // Tick 在啟動時同設兩個計時；只有 remaining 先扣掉當步 dt，之後同速遞減。
        // 一個微秒餘裕只容納 float 減法誤差，不准讀檔把活動短衝的冷卻變短。
        if(action.remaining>0&&action.cooldown+0.000001f<action.remaining+Evasion::Cooldown-Evasion::Duration)return false;
        // remaining/cooldown 都歸零時，Tick 仍保留上次水平單位方向。
        return true;
    }
    bool Valid() const {
        if(!session.Valid()||!ValidEvasion(evasion)||!ValidPoint(encounter.feet)||!ValidPoint(encounter.strikeTarget)||
           encounter.health>3||encounter.feet.y!=Session::arena.y||!std::isfinite(encounter.phaseTime)||!std::isfinite(encounter.playerCooldown)||
           encounter.playerCooldown<0||encounter.playerCooldown>.65f)return false;
        // 現有遭遇戰只移動 x/z，y 始終為 arena.y；不能載入地下或高空而永遠無法反擊的野豬。
        // 日後加入真正地形跟隨時，必須同步更新此契約及可達狀態測試，不能自行搬動壞檔位置。
        // .65 是現有 Encounter::Tick 的反擊冷卻，不是假設新的遊戲平衡值。
        switch(encounter.phase) {
        case EnemyPhase::Pursuit:
            // Recovery 到期後 Tick 保留減去 dt 的餘數；dt 的合法上限是 .1。
            if(encounter.phaseTime<-.1f||encounter.phaseTime>0||!encounter.health)return false;
            break;
        case EnemyPhase::Windup:
            if(encounter.phaseTime<=0||encounter.phaseTime>Encounter::WindupDuration||
               !encounter.health||encounter.countered)return false;
            break;
        case EnemyPhase::Recovery:
            if(encounter.phaseTime<=0||encounter.phaseTime>Encounter::RecoveryDuration||!encounter.health)return false;
            break;
        case EnemyPhase::Defeated:
            // 舊 Session 已通關時沒有剩餘戰鬥計時，允許遷移為零的 canonical 終態。
            if(encounter.health||!encounter.countered||encounter.phaseTime<0||
               encounter.phaseTime>Encounter::RecoveryDuration)return false;
            break;
        default:return false;
        }
        if(encounter.health==3&&encounter.countered)return false;
        if(session.guardianDefeated!=(encounter.phase==EnemyPhase::Defeated))return false;
        if(!session.bridgeRepaired) {
            const Encounter initial;
            if(encounter.health!=initial.health||encounter.phase!=initial.phase||encounter.phaseTime!=0||
               encounter.playerCooldown!=0||encounter.countered||
               Aevrynd::Length(encounter.feet-initial.feet)!=0||Aevrynd::Length(encounter.strikeTarget-initial.strikeTarget)!=0)return false;
        }
        return true;
    }
    std::string Serialize() const {
        if(!Valid())return {};
        std::ostringstream out;
        out.imbue(std::locale::classic());
        out.precision(std::numeric_limits<float>::max_digits10);
        out<<"MISTWATCH_SAVE 1\n"<<session.Serialize()<<"\nENCOUNTER "
           <<encounter.health<<' '<<static_cast<unsigned>(encounter.phase)<<' '
           <<encounter.feet.x<<' '<<encounter.feet.y<<' '<<encounter.feet.z<<' '
           <<encounter.strikeTarget.x<<' '<<encounter.strikeTarget.y<<' '<<encounter.strikeTarget.z<<' '
           <<encounter.phaseTime<<' '<<encounter.playerCooldown<<' '<<unsigned(encounter.countered)
           <<"\nEVASION "<<evasion.direction.x<<' '<<evasion.direction.y<<' '<<evasion.direction.z<<' '
           <<evasion.remaining<<' '<<evasion.cooldown;
        const auto text=out.str();
        return text.size()<=MaximumBytes?text:std::string{};
    }
    bool Restore(std::string_view text) {
        if(text.empty()||text.size()>MaximumBytes)return false;
        SaveGame candidate;
        std::istringstream in{std::string(text)};
        in.imbue(std::locale::classic());
        std::string magic;
        if(!(in>>magic))return false;
        if(magic=="MISTWATCH") {
            // 舊檔只記得任務；不能還原從未存過的敵人 HP 或階段，保留既有重開遭遇戰語義。
            if(!candidate.session.Restore(text))return false;
            if(candidate.session.guardianDefeated) {
                candidate.encounter.health=0;candidate.encounter.phase=EnemyPhase::Defeated;
                candidate.encounter.countered=true;
            }
        } else {
            unsigned version{};
            std::string remainder,sessionLine;
            if(magic!="MISTWATCH_SAVE"||!(in>>version)||version!=1)return false;
            if(!std::getline(in,remainder)||remainder.find_first_not_of(" \t\r")!=std::string::npos||
               !std::getline(in,sessionLine)||!candidate.session.Restore(sessionLine))return false;
            unsigned phase{},countered{};
            auto& enemy=candidate.encounter;
            if(!(in>>magic>>enemy.health>>phase>>enemy.feet.x>>enemy.feet.y>>enemy.feet.z
                 >>enemy.strikeTarget.x>>enemy.strikeTarget.y>>enemy.strikeTarget.z
                 >>enemy.phaseTime>>enemy.playerCooldown>>countered)||magic!="ENCOUNTER"||
                 phase>static_cast<unsigned>(EnemyPhase::Defeated)||countered>1)return false;
            enemy.phase=static_cast<EnemyPhase>(phase);enemy.countered=countered!=0;
            auto& action=candidate.evasion;
            if(!(in>>magic>>action.direction.x>>action.direction.y>>action.direction.z>>action.remaining>>action.cooldown)||magic!="EVASION")return false;
            if(in>>remainder)return false;
        }
        if(!candidate.Valid())return false;
        *this=candidate;
        return true;
    }
    bool RestoreStream(std::istream& in) {
        // 有界讀取：只多讀一個位元組偵測超限，失敗不改遊戲快照。
        std::array<char,MaximumBytes+1> bytes{};
        in.read(bytes.data(),static_cast<std::streamsize>(bytes.size()));
        const auto count=in.gcount();
        if(in.bad()||count<0||static_cast<std::size_t>(count)>MaximumBytes)return false;
        return Restore(std::string_view(bytes.data(),static_cast<std::size_t>(count)));
    }
};
}
