#include "MistwatchTuningInputRepeat.h"
#include "MistwatchFieldInput.h"
#include "MistwatchFieldJournal.h"
#include <array>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>

namespace {
using Repeat=Mistwatch::TuningInputRepeat;
unsigned checks{};
void Need(bool value,const char* reason){
    if(!value)throw std::runtime_error(reason);++checks;
}
void Emit(Repeat& input,double dt,std::uint64_t context,unsigned pressed,unsigned down,unsigned expected,const char* reason){
    Need(input.Tick(dt,context,pressed,down)==expected,reason);
}
// 取得嚴格小於剩餘時間的double；沒有epsilon、沒有提早核准門檻。
double Before(long double remaining){
    double value=static_cast<double>(remaining);
    if(static_cast<long double>(value)>remaining)value=std::nextafter(value,0.);
    return std::nextafter(value,0.);
}
double AtOrAfter(long double remaining){
    double value=static_cast<double>(remaining);
    if(static_cast<long double>(value)<remaining)value=std::nextafter(value,(std::numeric_limits<double>::infinity)());
    return value;
}
void Deadline(Repeat& input,unsigned bit,unsigned index){
    Emit(input,.1,19,0,bit,0,"DELAY_FIRST_100MS");
    Emit(input,.1,19,0,bit,0,"DELAY_SECOND_100MS");
    Emit(input,.1,19,0,bit,0,"DELAY_THIRD_100MS");
    Need(input.Remaining(index)>0&&input.Remaining(index)<.051L,"DELAY_REMAINDER");
    const auto before=Before(input.Remaining(index));
    Need(before>0&&static_cast<long double>(before)<input.Remaining(index),"BOUNDARY_BEFORE_STRICT");
    Emit(input,before,19,0,bit,0,"BOUNDARY_NOT_EARLY");
    const auto last=AtOrAfter(input.Remaining(index));
    Need(last>0&&last<=.1,"BOUNDARY_LAST_STEP_VALID");
    Emit(input,last,19,0,bit,bit,"BOUNDARY_AT_OR_AFTER");
    Need(input.Remaining(index)>0&&input.Remaining(index)<=.12L,"PERIOD_REMAINDER_POSITIVE");
    Emit(input,.1,19,0,bit,0,"PERIOD_FIRST_100MS_NOT_DUE");
    const auto periodBefore=Before(input.Remaining(index));
    Need(periodBefore>0&&static_cast<long double>(periodBefore)<input.Remaining(index),"PERIOD_BEFORE_STRICT");
    Emit(input,periodBefore,19,0,bit,0,"PERIOD_NOT_EARLY");
    Emit(input,AtOrAfter(input.Remaining(index)),19,0,bit,bit,"PERIOD_AT_OR_AFTER");
}
unsigned Count(const std::array<double,4>& pattern,unsigned cycles){
    Repeat input;Emit(input,.1,71,1,1,1,"COUNT_INITIAL_PRESS");
    unsigned count=1;
    for(unsigned cycle=0;cycle<cycles;++cycle)for(double dt:pattern){
        const auto emitted=input.Tick(dt,71,0,1);
        Need(emitted==0||emitted==1,"COUNT_NO_MULTI_EVENT_OR_OTHER_KEY");
        count+=emitted;
        Need(input.ArmedMask()==1&&input.Remaining(0)>0&&input.Remaining(0)<=.35L,"COUNT_LIVE_TIMER");
    }
    return count;
}
void StationIntegration(){
    using namespace Mistwatch;
    Session session;
    Need(FieldInput::RepeatContext(session)==0,"NO_EQUIPMENT_AT_START");
    for(unsigned i=0;i<3;++i){session.feet=Session::supplies[i];Need(session.Collect(i),"STATION_SUPPLIES");}
    for(unsigned i=0;i<3;++i){session.feet=Session::repairSites[i];Need(session.CollectRepairMaterial(i,i==0),"STATION_MATERIALS");}
    session.feet=Session::bridge;Need(session.RepairBridge(),"STATION_BRIDGE");
    session.feet=Session::arena;Need(session.FinishEncounter(),"STATION_ENCOUNTER");
    session.feet=Session::fuse;
    Need(FieldInput::RepeatContext(session)==1&&!session.fuseClueRead,"DIAL_AVAILABLE_BEFORE_CLUE_AS_BEFORE");
    Repeat repeat;
    const auto apply=[&](unsigned mask){for(unsigned i=0;i<3;++i)if(mask&(1u<<i))Need(session.CycleFuseDial(i),"ACTUAL_DIAL_METHOD");};
    apply(repeat.Tick(0,FieldInput::RepeatContext(session),1,1));
    Need((session.fuseDials&3)==1,"TAP_TURNS_ONCE");
    for(unsigned i=0;i<3;++i)apply(repeat.Tick(.1,FieldInput::RepeatContext(session),0,1));
    Need((session.fuseDials&3)==1,"HOLD_DELAY_DOES_NOT_TURN_EARLY");
    apply(repeat.Tick(.06,FieldInput::RepeatContext(session),0,1));
    Need((session.fuseDials&3)==2&&!session.fuseCollected,"HOLD_TURNS_BUT_NEVER_COLLECTS");
    auto remote=session;remote.feet.x+=1;
    Need(FieldInput::RepeatContext(remote)==0,"DIAL_OUTSIDE_ORIGINAL_RANGE");
    Need(repeat.Tick(.1,FieldInput::RepeatContext(remote),0,1)==0,"LEAVING_STATION_CANCELS_HOLD");
    Need(repeat.Tick(.1,FieldInput::RepeatContext(session),0,1)==0,"REENTRY_REQUIRES_NEW_PRESS");
    auto dead=session;dead.health=0;Need(FieldInput::RepeatContext(dead)==0,"DEAD_CANNOT_REPEAT");
    auto bad=session;bad.fuseDials=64;Need(FieldInput::RepeatContext(bad)==0,"INVALID_CANNOT_REPEAT");
    session.feet=Session::rangerLog;Need(session.ReadFuseClue(),"STATION_READ_CLUE");
    session.feet=Session::fuse;
    for(unsigned i=0;i<3;++i)while(((session.fuseDials>>(2*i))&3)!=Session::fuseCode[i])
        Need(session.CycleFuseDial(i),"STATION_SOLVE_DIAL");
    Need(!session.fuseCollected,"SOLVED_DIAL_STILL_NEEDS_E");
    Need(session.CollectFuse(),"STATION_EXPLICIT_COLLECT");
    Need(FieldInput::RepeatContext(session)==0,"STAGE_CHANGE_CANCELS_OLD_STATION");
    session.feet=Session::generator;Need(FieldInput::RepeatContext(session)==2,"GENERATOR_CONTEXT_DISTINCT");
    Need(session.ToggleCircuit(0)&&session.ToggleCircuit(2),"STATION_POWER_SWITCHES");
    Need(!session.powerRestored,"SWITCHES_CANNOT_AUTO_CONFIRM_POWER");
    Need(session.RepairGenerator(),"STATION_EXPLICIT_POWER");
    for(unsigned i=0;i<3;++i){
        session.feet=Session::relaySites[i];
        Need(FieldInput::RepeatContext(session)==3+i,"RELAY_CONTEXT_DISTINCT");
        auto high=session;high.feet.y+=.13f;
        Need(FieldInput::RepeatContext(high)==0,"RELAY_REQUIRES_GROUND_CONTACT");
    }
    session.feet=Session::relaySites[0];
    repeat.Reset();
    const auto patchContext=FieldInput::RepeatContext(session);
    Need(repeat.Tick(0,patchContext,1,1)==1,"STATION_PATCH_HOLD_ARM");
    Need(FieldInput::Lever(session,0)==FieldInput::Result::PatchChanged,"STATION_WEST_PATCH");
    Need(session.relayFieldTasks==0&&session.relayLinked==0,"PATCH_DOES_NOT_CONFIRM_OR_LINK");
    Need(FieldInput::Interact(session)==FieldInput::Result::CircuitConfirmed,"STATION_EXPLICIT_CIRCUIT_CONFIRM");
    const auto antennaContext=FieldInput::RepeatContext(session);
    Need(antennaContext!=patchContext&&antennaContext==6,"CONFIRMED_PATCH_IS_DIFFERENT_INPUT_MODE");
    const auto sectors=session.relaySectors;
    for(unsigned i=0;i<10;++i)
        Need(repeat.Tick(.1,antennaContext,0,1)==0,"HOLD_CANNOT_CROSS_PATCH_TO_ANTENNA");
    Need(session.relaySectors==sectors&&repeat.ArmedMask()==0,"CROSS_MODE_NO_GHOST_AIM");
    Need(repeat.Tick(0,antennaContext,1,1)==1,"NEW_PRESS_AFTER_CONFIRM_ALLOWED");
    for(unsigned i=0;i<16&&session.RelaySignal(0)!=100;++i)Need(session.TuneRelay(0,1),"STATION_AIM");
    Need(FieldInput::Interact(session)==FieldInput::Result::RelayConnected,"STATION_EXPLICIT_LINK");
    Need(FieldInput::RepeatContext(session)==0,"LINKED_RELAY_NOT_EDITABLE");
    session.feet=Session::relaySites[2];
    repeat.Reset();
    Need(repeat.Tick(0,FieldInput::RepeatContext(session),4,4)==4,"STATION_ROUTE_HOLD_ARM");
    Need(FieldInput::Lever(session,2)==FieldInput::Result::RouteChanged,"STATION_ROUTE_CARD");
    Need(FieldInput::Interact(session)==FieldInput::Result::RouteConfirmed,"STATION_ROUTE_CONFIRM");
    Need(FieldInput::RepeatContext(session)==8,"CONFIRMED_ROUTE_IS_DIFFERENT_INPUT_MODE");
    Need(repeat.Tick(.1,FieldInput::RepeatContext(session),0,4)==0&&repeat.ArmedMask()==0,"ROUTE_CONFIRM_CANCELS_HOLD");
    session.feet=Session::relaySites[1];
    ObjectiveGuide guide{};guide.valid=true;guide.index=1;
    const auto hud=FieldPresentation::Build(session,guide,Encounter{},true,false,false);
    Need(hud.valid&&hud.instruction.find("長按")!=std::string::npos,"HUD_DOCUMENTS_HOLD");
    const auto paused=FieldPresentation::Build(session,guide,Encounter{},true,true,false);
    Need(paused.instruction.find("長按")==std::string::npos,"PAUSE_DOES_NOT_ADVERTISE_ACTIVE_HOLD");
}
// 提示與真實按鍵語意分開計數，不改原測試的checks或成功marker。
unsigned hintChecks{},hintModes{};
void HintNeed(bool value,const char* reason){
    if(!value)throw std::runtime_error(reason);++hintChecks;
}
void TruthfulTuningHints(){
    using namespace Mistwatch;
    Session session;
    for(unsigned i=0;i<3;++i){session.feet=Session::supplies[i];HintNeed(session.Collect(i),"HINT_SETUP_SUPPLIES");}
    for(unsigned i=0;i<3;++i){session.feet=Session::repairSites[i];HintNeed(session.CollectRepairMaterial(i,i==0),"HINT_SETUP_MATERIALS");}
    session.feet=Session::bridge;HintNeed(session.RepairBridge(),"HINT_SETUP_BRIDGE");
    session.feet=Session::arena;HintNeed(session.FinishEncounter(),"HINT_SETUP_ENCOUNTER");
    auto terrain=CreateTerrain();const std::array<Aevrynd::BoxCollider3D,0> solids{};
    Aevrynd::Camera3D camera;camera.position={0,3,-6};camera.target={0,1,0};
    const Encounter enemy;const Evasion evasion;
    const auto inspect=[&](const Session& current,bool three,std::uint64_t context){
        const auto before=current.Serialize();
        HintNeed(!before.empty()&&current.Valid(),"HINT_VALID_PUBLIC_STATE");
        HintNeed(FieldInput::RepeatContext(current)==context,"HINT_ACTUAL_CONTEXT");
        const auto guide=BuildObjectiveGuide(current,enemy,evasion,camera,false,true,solids,terrain);
        const auto hud=FieldPresentation::Build(current,guide,enemy,true,false,false);
        HintNeed(guide.valid&&hud.valid,"HINT_REAL_GUIDE_VALID");
        HintNeed(current.Serialize()==before,"HINT_PRESENTATION_CHANGED_SERIALIZED_STATE");
        HintNeed(hud.instruction.ends_with(three?
            " 點按微調；長按 1、2、3 連續調整；E 仍需另按確認。":
            " 點按微調；長按 1、2 連續調整；E 仍需另按確認。"),
            three?"TRUTHFUL_HINT_VALID_KEY3_MISSING":"TRUTHFUL_HINT_UNSUPPORTED_KEY3_ADVERTISED");
        auto changed=current;const auto beforeKey=changed.Serialize();
        bool key3{};
        if(current.CurrentStage()==Stage::Fuse)key3=changed.CycleFuseDial(2);
        else if(current.CurrentStage()==Stage::Generator)key3=changed.ToggleCircuit(2);
        else{
            const auto result=FieldInput::Lever(changed,2);
            key3=result!=FieldInput::Result::None;
            if(!three)HintNeed(result==FieldInput::Result::None,"HINT_KEY3_MUST_BE_NONE");
        }
        HintNeed(key3==three,"HINT_KEY3_MATCHES_ACTUAL_PUBLIC_OPERATION");
        HintNeed((changed.Serialize()!=beforeKey)==three,"HINT_KEY3_FULL_SERIALIZED_STATE");
        HintNeed(current.Serialize()==before,"HINT_PUBLIC_OPERATION_PROBE_CHANGED_ORIGINAL");
        if(!three){
            auto repeated=current;Repeat repeat;unsigned emitted{};
            for(unsigned tick=0;tick<7;++tick){
                const auto keys=repeat.Tick(tick?.1:0,context,tick?0u:4u,4);
                HintNeed(keys==0||keys==4,"HINT_HOLD_ONLY_KEY3");
                if(keys&4u){++emitted;HintNeed(FieldInput::Lever(repeated,2)==FieldInput::Result::None,"HINT_REPEATED_KEY3_NONE");}
                HintNeed(repeated.Serialize()==before,"HINT_REPEATED_KEY3_STATE_CHANGED");
            }
            HintNeed(emitted>=2,"HINT_HOLD_EXERCISED_PRESS_AND_REPEAT");
        }
        const auto paused=FieldPresentation::Build(current,guide,enemy,true,true,false);
        const auto unstarted=FieldPresentation::Build(current,guide,enemy,false,false,false);
        HintNeed(paused.instruction.find("長按")==std::string::npos&&
            unstarted.instruction.find("長按")==std::string::npos,"HINT_INACTIVE_NO_HOLD");
        HintNeed(current.Serialize()==before,"HINT_INACTIVE_CHANGED_SERIALIZED_STATE");
        ++hintModes;
    };
    session.feet=Session::rangerLog;HintNeed(session.ReadFuseClue(),"HINT_READ_REAL_CLUE");
    session.feet=Session::fuse;inspect(session,true,1);
    for(unsigned wheel=0;wheel<3;++wheel){
        for(unsigned turn=0;turn<4&&session.FuseDial(wheel)!=Session::fuseCode[wheel];++turn)
            HintNeed(session.CycleFuseDial(wheel),"HINT_SOLVE_REAL_DIAL");
        HintNeed(session.FuseDial(wheel)==Session::fuseCode[wheel],"HINT_DIAL_SOLUTION_REACHED");
    }
    HintNeed(!session.fuseCollected&&session.CollectFuse(),"HINT_EXPLICIT_FUSE_COLLECT");
    session.feet=Session::generator;inspect(session,true,2);
    HintNeed(session.ToggleCircuit(0)&&session.ToggleCircuit(2),"HINT_REAL_POWER_SWITCHES");
    HintNeed(!session.powerRestored&&session.RepairGenerator(),"HINT_EXPLICIT_POWER");
    session.feet=Session::relaySites[0];inspect(session,false,3);
    HintNeed(FieldInput::Lever(session,0)==FieldInput::Result::PatchChanged,"HINT_REAL_WEST_PATCH");
    HintNeed(session.relayFieldTasks==0&&session.relayLinked==0,"HINT_PATCH_NOT_AUTO_CONFIRMED");
    HintNeed(FieldInput::Interact(session)==FieldInput::Result::CircuitConfirmed,"HINT_EXPLICIT_CIRCUIT_CONFIRM");
    HintNeed(session.relayLinked==0,"HINT_CONFIRM_NOT_AUTO_LINK");
    inspect(session,false,6);
    session.feet=Session::relaySites[1];inspect(session,false,4);
    session.feet=Session::relaySites[2];inspect(session,true,5);
    HintNeed(FieldInput::Lever(session,2)==FieldInput::Result::RouteChanged,"HINT_REAL_ROUTE_CARD");
    HintNeed(!(session.relayFieldTasks&FieldRelayTask::NorthWestRouteBit),"HINT_ROUTE_NOT_AUTO_CONFIRMED");
    HintNeed(FieldInput::Interact(session)==FieldInput::Result::RouteConfirmed,"HINT_EXPLICIT_ROUTE_CONFIRM");
    HintNeed(session.relayLinked==0,"HINT_ROUTE_CONFIRM_NOT_AUTO_LINK");
    inspect(session,false,8);
    HintNeed(hintModes==7,"HINT_SEVEN_REAL_MODES");
    std::cout<<"TRUTHFUL_TUNING_HINT_PASS modes="<<hintModes<<" checks="<<hintChecks
        <<" serialized_unchanged=1 original_counter_unchanged=1 native=false GPU=false\n";
}
}
int main()try{
    Repeat input;
    Need(input.ArmedMask()==0&&input.Remaining(0)==0&&input.Remaining(3)==0,"DEFAULT_DISARMED");
    Emit(input,.1,0,7,7,0,"INACTIVE_PRESS_IGNORED");
    Emit(input,0,10,1,0,1,"TAP_PRESS_RELEASE_SAME_TICK");
    Need(input.ArmedMask()==0,"TAP_MUST_NOT_ARM");
    for(unsigned i=0;i<10;++i)Emit(input,.1,10,0,1,0,"DOWN_WITHOUT_PRESS_MUST_NOT_ARM");
    Emit(input,.1,10,1,1,1,"PRESS_IMMEDIATE");
    Need(input.ArmedMask()==1&&input.Remaining(0)==.35L,"LITERAL_DELAY_AND_NEW_PRESS_DT_IGNORED");
    Emit(input,0,10,0,1,0,"ZERO_DT_NO_REPEAT");
    Need(input.Remaining(0)==.35L,"ZERO_DT_TIMER_UNCHANGED");
    input.Reset();Emit(input,0,19,1,1,1,"DEADLINE_ARM");Deadline(input,1,0);
    input.Reset();Emit(input,0,19,2,2,2,"SECOND_KEY_DEADLINE_ARM");Deadline(input,2,1);
    input.Reset();Emit(input,0,19,4,4,4,"THIRD_KEY_DEADLINE_ARM");Deadline(input,4,2);

    input.Reset();Emit(input,0,24,1,1,1,"FRACTION_ARM");
    for(unsigned i=0;i<3;++i)Emit(input,.1,24,0,1,0,"FRACTION_DELAY_WAIT");
    const auto before=input.Remaining(0);Emit(input,.1,24,0,1,1,"FRACTION_REPEAT_AFTER_DELAY");
    Need(input.Remaining(0)==.12L-(static_cast<long double>(.1)-before),"LITERAL_PERIOD_AND_FRACTION_RETAINED");
    Need(input.Remaining(0)<.071L&&input.Remaining(0)>.069L,"NO_TIMER_RESTART_LOSING_FRACTION");
    Emit(input,0,24,0,1,0,"NO_ZERO_DT_BURST");
    Emit(input,.1,24,0,1,1,"MAX_DT_SINGLE_REPEAT");
    Need(input.Remaining(0)>0&&input.Remaining(0)<.1L,"MAX_DT_REMAINDER");
    Emit(input,.1,24,0,0,0,"RELEASE_DISARMS");
    Need(input.ArmedMask()==0&&input.Remaining(0)==0,"RELEASE_CLEARS_TIMER");
    for(unsigned i=0;i<10;++i)Emit(input,.1,24,0,1,0,"RELEASE_THEN_DOWN_NOT_REARMED");

    input.Reset();Emit(input,0,31,1,1,1,"INDEPENDENT_FIRST_PRESS");
    Emit(input,.1,31,2,3,2,"INDEPENDENT_SECOND_PRESS");
    Need(input.Remaining(0)<.251L&&input.Remaining(0)>.249L&&input.Remaining(1)==.35L,"KEY_TIMERS_INDEPENDENT");
    Emit(input,.1,31,4,7,4,"INDEPENDENT_THIRD_PRESS");
    Need(input.Remaining(0)<input.Remaining(1)&&input.Remaining(1)<input.Remaining(2),"THREE_INDEPENDENT_DEADLINES");
    Emit(input,.1,31,0,5,0,"ONE_KEY_RELEASE_OTHERS_CONTINUE");
    Need(input.ArmedMask()==5&&input.Remaining(1)==0,"ONE_KEY_RELEASE_MASK");
    Emit(input,.1,31,0,5,1,"FIRST_KEY_ONLY_DUE");
    Emit(input,.1,31,0,5,1,"FIRST_KEY_NEXT_PERIOD_THIRD_STILL_WAITING");
    Emit(input,.06,31,0,5,4,"THIRD_KEY_ONLY_DUE");
    input.Reset();Emit(input,.1,32,7,7,7,"THREE_SIMULTANEOUS_PRESSES");
    Need(input.ArmedMask()==7&&input.Remaining(0)==.35L&&input.Remaining(1)==.35L&&input.Remaining(2)==.35L,"THREE_KEYS_SAME_DELAY");
    for(unsigned i=0;i<3;++i)Emit(input,.1,32,0,7,0,"THREE_KEYS_WAIT");
    Emit(input,.1,32,0,7,7,"THREE_KEYS_DUE_ONCE_EACH");

    Emit(input,.1,33,0,7,0,"CONTEXT_CHANGE_CANNOT_TRANSFER_HOLD");
    Need(input.ArmedMask()==0,"CONTEXT_CHANGE_DISARMS");
    for(unsigned i=0;i<10;++i)Emit(input,.1,33,0,7,0,"NEW_CONTEXT_DOWN_ONLY_SILENT");
    Emit(input,.1,34,2,7,2,"NEW_CONTEXT_EXPLICIT_PRESS_ALLOWED");
    Need(input.ArmedMask()==2&&input.Remaining(1)==.35L,"NEW_CONTEXT_ONLY_EXPLICIT_KEY_ARMED");
    Emit(input,.1,0,0,7,0,"INACTIVE_RESETS_ALL");
    Need(input.ArmedMask()==0,"INACTIVE_CLEARS_ARM");
    Emit(input,.1,34,0,7,0,"REACTIVATION_NO_REPLAY");
    Emit(input,0,34,1,1,1,"ZERO_DT_NEW_PRESS_ALLOWED");
    Need(input.Remaining(0)==.35L,"ZERO_DT_PRESS_DELAY");
    Emit(input,.1,34,0,1,0,"REPRESS_WAIT_FIRST");
    Emit(input,.1,34,1,1,1,"REPRESS_IMMEDIATE_NOT_DOUBLE");
    Need(input.Remaining(0)==.35L,"REPRESS_RESTARTS_DELAY_WITHOUT_OLD_DT");
    Emit(input,.1,34,1,0,1,"ARMED_TAP_PRESS_RELEASE_SAME_TICK");
    Need(input.ArmedMask()==0&&input.Remaining(0)==0,"ARMED_TAP_CLEARS_HOLD");
    input.Reset();input.Reset();
    Need(input.ArmedMask()==0&&input.Remaining(0)==0,"RESET_IDEMPOTENT");
    Emit(input,.1,34,0,1,0,"RESET_NO_REPLAY");

    const std::array<double,5> invalid{{-.001,std::numeric_limits<double>::quiet_NaN(),
        (std::numeric_limits<double>::infinity)(),-(std::numeric_limits<double>::infinity)(),std::nextafter(.1,1.)}};
    for(double dt:invalid){
        Emit(input,0,48,7,7,7,"INVALID_DT_FIXTURE_ARM");
        Emit(input,dt,48,7,7,0,"INVALID_DT_RESET_EMIT_ZERO");
        Need(input.ArmedMask()==0&&input.Remaining(0)==0&&input.Remaining(1)==0&&input.Remaining(2)==0,"INVALID_DT_CLEARS_ALL");
        Emit(input,.1,48,0,7,0,"INVALID_DT_NO_PHANTOM_REARM");
    }
    const std::array<std::array<unsigned,2>,4> badMasks{{{8,0},{0,8},{15,7},{7,(std::numeric_limits<unsigned>::max)()}}};
    for(const auto masks:badMasks){
        Emit(input,0,49,7,7,7,"INVALID_MASK_FIXTURE_ARM");
        Emit(input,.1,49,masks[0],masks[1],0,"INVALID_MASK_RESET_EMIT_ZERO");
        Need(input.ArmedMask()==0,"INVALID_MASK_CLEARS_ALL");
        Emit(input,.1,49,0,7,0,"INVALID_MASK_NO_PHANTOM_REARM");
    }
    // 皆為100秒，但使用不同切分；解析期望由測試自己的literal推導。
    const unsigned expected=1+1+static_cast<unsigned>(std::floor((100.L-.35L)/.12L));
    const auto fine=Count({.01,.01,.01,.01},2500);
    const auto coarse=Count({.1,.1,.1,.1},250);
    const auto mixed=Count({.03,.07,.02,.08},500);
    Need(fine==expected&&coarse==expected&&mixed==expected,"LONG_RUN_AVERAGE_CADENCE_INDEPENDENT_OF_DT_SPLIT");
    StationIntegration();
    TruthfulTuningHints();
    std::cout<<"TUNING_INPUT_REPEAT_CPU_PASS checks="<<checks<<" long_run_emits="<<expected
        <<" native=false GPU=false manualplay=false\n";return 0;
}catch(const std::exception& error){std::cerr<<"TUNING_INPUT_REPEAT_FIRST_DIVERGENCE "<<error.what()<<'\n';return 1;}
