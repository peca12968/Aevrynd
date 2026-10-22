#pragma once
#include "MistwatchEncounter.h"
#include "MistwatchEvasion.h"
#include "MistwatchPlayerLocomotion.h"
#include <array>
#include <charconv>
#include <istream>
#include <limits>
#include <locale>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>

namespace Mistwatch {
struct SavedPlayerMotion {
    Aevrynd::CharacterMotionState3D controller;
    PlayerLocomotionMotionState locomotion;
    float explorerYaw{},explorerRequestedYaw{},explorerPhase{};
    float explorerDistance{},explorerSeconds{};
};
// 遊戲層快照，不改 Session 的舊格式，也不替引擎保存遊戲規則。
// 相機、輸入邊沿、音效及接地腳步軌跡不屬於此快照；整合端須明確處理。
// 版本 2 允許地形跟隨的敵人高度；版本 1 仍保留舊固定高度限制。
// 版本 3 保留運動狀態；沒有 motion 的舊呼叫端仍明確輸出版本 2，不捏造速度。
// EVASION 為必填；完整身體與地形支撐另由 PrepareSavedGame 驗證，不搬移不安全舊檔。
struct SaveGame {
    static constexpr std::size_t MaximumBytes=2048;
    Session session;
    Encounter encounter;
    Evasion evasion;
    std::optional<SavedPlayerMotion> motion;
    std::optional<std::uint64_t> worldIdentity;

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
    bool ValidMotion() const {
        if(!motion)return true;
        const auto& c=motion->controller;const auto& l=motion->locomotion;const auto& m=l.motor;
        constexpr float pi=3.14159275f,tau=6.2831853f;
        // 此處只檢查可序列化狀態；速度設定、碰撞、支撐及地形仍須由載入預檢驗證。
        if(!ValidPoint(c.feet)||c.feet.x!=session.feet.x||c.feet.y!=session.feet.y||c.feet.z!=session.feet.z||
           !Session::Finite(c.velocity)||!std::isfinite(c.height)||c.height<.6f||c.height>3||
           !std::isfinite(c.yaw)||std::abs(c.yaw)>pi||(c.grounded&&c.velocity.y!=0)||
           !Session::Finite(m.velocity)||m.velocity.y!=0||m.velocity.x!=c.velocity.x||m.velocity.z!=c.velocity.z||
           !std::isfinite(m.heading)||std::abs(m.heading)>pi||m.supportTicks>10||
           m.tick==std::numeric_limits<std::uint64_t>::max()||
           !std::isfinite(l.remainder)||l.remainder<0||l.remainder>=double(LocomotionMotor::FixedStep)||
           (evasion.remaining>0&&!l.dodgeFlight)||(l.dodgeFlight&&m.runFlight)||
           !std::isfinite(motion->explorerYaw)||std::abs(motion->explorerYaw)>pi||
           !std::isfinite(motion->explorerRequestedYaw)||std::abs(motion->explorerRequestedYaw)>pi||
           !std::isfinite(motion->explorerPhase)||motion->explorerPhase<0||motion->explorerPhase>=tau||
           !std::isfinite(motion->explorerDistance)||motion->explorerDistance<0||
           !std::isfinite(motion->explorerSeconds)||motion->explorerSeconds<0||motion->explorerSeconds>.1f)return false;
        const float speed=Aevrynd::Length(m.velocity);
        if(!std::isfinite(speed)||(!m.headingReady&&(m.heading!=0||speed>0)))return false;
        switch(m.phase){
        case LocomotionPhase::Standing:if(!c.grounded||speed>.001f||m.runFlight||m.supportTicks)return false;break;
        case LocomotionPhase::GroundMotion:if(!c.grounded||speed<=.001f||m.runFlight||m.supportTicks)return false;break;
        case LocomotionPhase::RunSupport:if(!c.grounded||speed<5.4f||m.runFlight)return false;break;
        case LocomotionPhase::RunFlight:if(c.grounded||!m.runFlight||m.supportTicks)return false;break;
        case LocomotionPhase::JumpOrFall:if(c.grounded||m.runFlight||m.supportTicks)return false;break;
        default:return false;
        }
        return true;
    }
    bool Valid() const {
        if((worldIdentity&&(!*worldIdentity||!motion))||!session.Valid()||!encounter.Valid()||!ValidEvasion(evasion)||!ValidMotion()||!ValidPoint(encounter.feet)||!ValidPoint(encounter.strikeTarget)||
           encounter.health>3||!std::isfinite(encounter.phaseTime)||!std::isfinite(encounter.playerCooldown)||
           encounter.playerCooldown<0||encounter.playerCooldown>.65f)return false;
        // 此層僅驗證快照結構；地形高度不是常數，正式載入必須通過世界預檢。
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
        out<<"MISTWATCH_SAVE "<<(worldIdentity?4:motion?3:2)<<'\n';
        if(worldIdentity)out<<"WORLD "<<*worldIdentity<<'\n';
        out<<session.Serialize()<<"\nENCOUNTER "
           <<encounter.health<<' '<<static_cast<unsigned>(encounter.phase)<<' '
           <<encounter.feet.x<<' '<<encounter.feet.y<<' '<<encounter.feet.z<<' '
           <<encounter.strikeTarget.x<<' '<<encounter.strikeTarget.y<<' '<<encounter.strikeTarget.z<<' '
           <<encounter.phaseTime<<' '<<encounter.playerCooldown<<' '<<unsigned(encounter.countered)
           <<"\nEVASION "<<evasion.direction.x<<' '<<evasion.direction.y<<' '<<evasion.direction.z<<' '
           <<evasion.remaining<<' '<<evasion.cooldown;
        if(motion){
            const auto& c=motion->controller;const auto& l=motion->locomotion;const auto& m=l.motor;
            out<<"\nMOTION_CONTROLLER "<<c.feet.x<<' '<<c.feet.y<<' '<<c.feet.z<<' '
               <<c.velocity.x<<' '<<c.velocity.y<<' '<<c.velocity.z<<' '<<c.height<<' '<<c.yaw<<' '<<unsigned(c.grounded)
               <<"\nMOTION_MOTOR "<<m.velocity.x<<' '<<m.velocity.y<<' '<<m.velocity.z<<' '<<m.heading<<' '
               <<unsigned(m.headingReady)<<' '<<static_cast<unsigned>(m.phase)<<' '<<m.supportTicks<<' '
               <<m.tick<<' '<<unsigned(m.runFlight)<<' '<<unsigned(m.wasDashing);
            out.precision(std::numeric_limits<double>::max_digits10);
            out<<"\nMOTION_OWNER "<<l.remainder<<' '<<unsigned(l.dodgeFlight);
            out.precision(std::numeric_limits<float>::max_digits10);
            out<<"\nMOTION_EXPLORER "<<motion->explorerYaw<<' '<<motion->explorerRequestedYaw<<' '<<motion->explorerPhase
               <<' '<<motion->explorerDistance<<' '<<motion->explorerSeconds
               <<"\nMOTION_END";
        }
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
            if(magic!="MISTWATCH_SAVE"||!(in>>version)||(version!=1&&version!=2&&version!=3&&version!=4))return false;
            if(!std::getline(in,remainder)||remainder.find_first_not_of(" \t\r")!=std::string::npos)return false;
            if(version==4){std::string key;std::uint64_t identity{};
                if(!(in>>magic>>key)||magic!="WORLD"||key.empty())return false;
                for(char c:key)if(c<'0'||c>'9')return false;
                const auto parsed=std::from_chars(key.data(),key.data()+key.size(),identity);
                if(parsed.ec!=std::errc{}||parsed.ptr!=key.data()+key.size()||!identity||!std::getline(in,remainder)||
                    remainder.find_first_not_of(" \t\r")!=std::string::npos)return false;
                candidate.worldIdentity=identity;
            }
            if(!std::getline(in,sessionLine)||!candidate.session.Restore(sessionLine))return false;
            unsigned phase{},countered{};
            auto& enemy=candidate.encounter;
            if(!(in>>magic>>enemy.health>>phase>>enemy.feet.x>>enemy.feet.y>>enemy.feet.z
                 >>enemy.strikeTarget.x>>enemy.strikeTarget.y>>enemy.strikeTarget.z
                 >>enemy.phaseTime>>enemy.playerCooldown>>countered)||magic!="ENCOUNTER"||
                 phase>static_cast<unsigned>(EnemyPhase::Defeated)||countered>1)return false;
            enemy.phase=static_cast<EnemyPhase>(phase);enemy.countered=countered!=0;
            if(version==1&&enemy.feet.y!=Session::arena.y)return false;
            auto& action=candidate.evasion;
            if(!(in>>magic>>action.direction.x>>action.direction.y>>action.direction.z>>action.remaining>>action.cooldown)||magic!="EVASION")return false;
            if(version==3||version==4){
                SavedPlayerMotion saved;auto& c=saved.controller;auto& l=saved.locomotion;auto& m=l.motor;
                // 整數必須完整、非負且不溢位；不能讓 -2 被 unsigned 轉成很大的合法 tick。
                const auto integer=[&](auto& value){
                    std::string token;if(!(in>>token)||token.empty())return false;
                    for(char ch:token)if(ch<'0'||ch>'9')return false;
                    const auto result=std::from_chars(token.data(),token.data()+token.size(),value);
                    return result.ec==std::errc{}&&result.ptr==token.data()+token.size();
                };
                const auto boolean=[&](bool& value){unsigned raw{};if(!integer(raw)||raw>1)return false;value=raw!=0;return true;};
                unsigned motorPhase{};
                if(!(in>>magic>>c.feet.x>>c.feet.y>>c.feet.z>>c.velocity.x>>c.velocity.y>>c.velocity.z>>c.height>>c.yaw)||
                   magic!="MOTION_CONTROLLER"||!boolean(c.grounded))return false;
                if(!(in>>magic>>m.velocity.x>>m.velocity.y>>m.velocity.z>>m.heading)||magic!="MOTION_MOTOR"||
                   !boolean(m.headingReady)||!integer(motorPhase)||motorPhase>static_cast<unsigned>(LocomotionPhase::JumpOrFall)||
                   !integer(m.supportTicks)||!integer(m.tick)||!boolean(m.runFlight)||!boolean(m.wasDashing))return false;
                m.phase=static_cast<LocomotionPhase>(motorPhase);
                if(!(in>>magic>>l.remainder)||magic!="MOTION_OWNER"||!boolean(l.dodgeFlight))return false;
                if(!(in>>magic>>saved.explorerYaw>>saved.explorerRequestedYaw>>saved.explorerPhase
                     >>saved.explorerDistance>>saved.explorerSeconds)||magic!="MOTION_EXPLORER")return false;
                if(!(in>>magic)||magic!="MOTION_END")return false;
                candidate.motion=saved;
            }
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
