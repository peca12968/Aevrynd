#include "MistwatchFieldJournal.h"
#include "MistwatchPlaytestClock.h"
#include <chrono>
#include <iostream>
#include <stdexcept>

namespace {
void Require(bool good,const char* reason){
    if(!good)throw std::runtime_error(reason);
}
}

int main(){
    using namespace Mistwatch;
    using namespace std::chrono_literals;
    try{
        Session session;
        Require(session.Restore("MISTWATCH 3 7 80 1 1 1 0 0 69 1 1 7"),"invalid completed session fixture");
        Require(session.Valid()&&session.CurrentStage()==Stage::Complete,"completion not reached");
        ObjectiveGuide guide;
        guide.valid=true;
        Encounter enemy;
        const auto before=session.Serialize();
        auto debrief=FieldPresentation::Build(session,guide,enemy,true,false,true);
        Require(debrief.valid&&!debrief.journal.empty()&&debrief.journal.front()=="救援任務完成"&&FieldPresentation::JournalTitle(session)=="巡守結案（J 關閉）",
            "completed journal still shows active mission instructions");
        Require(debrief.controls.find("J 結案")!=std::string::npos,"completed controls still point to the active journal");
        Require(session.Serialize()==before,"debrief changed mission state");
        const auto closed=FieldPresentation::Build(session,guide,enemy,true,false,false);
        Require(closed.journal.empty(),"debrief opened without the journal key");
        const auto paused=FieldPresentation::Build(session,guide,enemy,true,true,true);
        Require(paused.journal==debrief.journal,"pause lost completion debrief");
        Require(paused.controls.find("J 結案")!=std::string::npos,"paused controls still point to the active journal");
        const auto active=FieldPresentation::Build(Session{},guide,enemy,true,false,true);
        Require(active.valid&&active.journal.front().starts_with("巡守手冊")&&
            FieldPresentation::JournalTitle(Session{}).starts_with("巡守手冊"),"active journal changed");
        PlaytestClock clock;
        using Leg=PlaytestClock::Leg;
        const PlaytestClock::Clock::time_point start{};
        Require(!clock.CompletedLegs()&&clock.CompletedSummary().empty(),"unmeasured game has a split");
        clock.Start(start);
        clock.EnterLeg(start+1750ms,Leg::Encounter);
        clock.SetActive(start+3000ms,false);
        clock.EnterLeg(start+7000ms,Leg::RescueSignal);
        clock.SetActive(start+9000ms,true);
        clock.EnterLeg(start+10250ms,Leg::SettlementPower);
        clock.EnterLeg(start+12500ms,Leg::RescueSignal);
        clock.SetActive(start+14000ms,false);
        Require(!clock.CompletedLegs()&&clock.CompletedSummary().empty(),"incomplete game has a split");
        clock.SetActive(start+20000ms,true);
        clock.Finish(start+21750ms);
        const PlaytestClock::LegSeconds expected{1s,3s,2s,3s};
        const auto measured=clock.CompletedLegs();
        Require(measured==expected&&clock.CompletedTime()==9s,"split timing changed total or includes paused time");
        Require(clock.CompletedSummary()=="MISTWATCH_PLAYTEST 1\nactive_s=9\nbridge_s=1\n"
            "encounter_s=3\npower_s=2\nsignal_s=3\n","export does not match debrief timing");
        FieldPresentation::AppendDebriefTimes(debrief,*measured);
        Require(debrief.journal.size()==7&&debrief.journal[3]=="南岸備料與修橋  0 分 1 秒"&&
            debrief.journal[6]=="中繼站與救援信標  0 分 3 秒","debrief split formatting failed");
        clock.Finish(start+50000ms);
        Require(clock.CompletedLegs()==expected,"completed split was mutable");
        clock.Clear();
        Require(!clock.CompletedLegs()&&!clock.CompletedTime()&&clock.CompletedSummary().empty(),"readback retained old timing");
        clock.Start(start+60s);
        clock.Finish(start+61s);
        Require(clock.CompletedLegs()==PlaytestClock::LegSeconds{1s,0s,0s,0s},"new playtest inherited the old split");
        const auto baseline=FieldPresentation::Build(session,guide,enemy,true,false,true);
        auto timedPlayer=baseline;
        FieldPresentation::AppendPlaytestDebrief(timedPlayer,clock,false,true,true);
        Require(timedPlayer.journal.size()==8&&timedPlayer.journal.back().starts_with("K 匯出"),
            "timed player lost the export control");
        auto timedNoPath=baseline;
        FieldPresentation::AppendPlaytestDebrief(timedNoPath,clock,false,true,false);
        Require(timedNoPath.journal.size()==8&&timedNoPath.journal.back().find("無法匯出")!=std::string::npos,
            "missing save path was not explained");
        auto timedSmoke=baseline;
        FieldPresentation::AppendPlaytestDebrief(timedSmoke,clock,false,false,true);
        Require(timedSmoke.journal.size()==7,"synthetic debrief gained a player prompt");
        clock.Clear();
        auto unmeasured=baseline;
        FieldPresentation::AppendPlaytestDebrief(unmeasured,clock,false,true,true);
        Require(unmeasured.journal.size()==5&&unmeasured.journal[3].find("沒有完整計時")!=std::string::npos&&
            unmeasured.journal.back().starts_with("按 R"),"unmeasured completion did not explain a fresh run");
        auto pausedUnmeasured=paused;
        FieldPresentation::AppendPlaytestDebrief(pausedUnmeasured,clock,true,true,true);
        Require(pausedUnmeasured.journal.size()==5&&pausedUnmeasured.journal.back().starts_with("先按 Esc"),
            "paused completion directs the player to an inactive R key");
        auto smokeUnmeasured=paused;
        FieldPresentation::AppendPlaytestDebrief(smokeUnmeasured,clock,true,false,true);
        Require(smokeUnmeasured.journal==paused.journal,"unmeasured synthetic debrief gained prompts");
        std::cout<<"DEBRIEF_TIMING_FEEDBACK_PASS measured=3 unmeasured=3\n";
        std::cout<<"PLAYTEST_DEBRIEF_PROBE_PASS legs=4 wall_time_exclusions=pass journal=pass\n";
        return 0;
    }catch(const std::exception& error){
        std::cerr<<error.what()<<'\n';
        return 1;
    }
}
