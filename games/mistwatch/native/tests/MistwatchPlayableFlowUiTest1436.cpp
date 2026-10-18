#include "MistwatchFieldInput.h"
#include "MistwatchFieldJournal.h"
#include <Aevrynd/Renderer.h>
#include <algorithm>
#include <bit>
#include <iostream>
#include <limits>
#include <utility>

namespace {
using namespace Mistwatch;
unsigned checks{},negativeCases{},inputCases{},presentationCases{};
void Need(bool ok,const char* why){++checks;if(!ok)throw std::runtime_error(why);}
bool Same(const Session& a,const Session& b){
    return a.collected==b.collected&&a.health==b.health&&a.bridgeRepaired==b.bridgeRepaired&&
        a.guardianDefeated==b.guardianDefeated&&a.beaconLit==b.beaconLit&&a.fuseCollected==b.fuseCollected&&
        a.powerRestored==b.powerRestored&&a.circuit==b.circuit&&a.repairMaterials==b.repairMaterials&&
        a.fuseClueRead==b.fuseClueRead&&a.fuseDials==b.fuseDials&&a.relayLinked==b.relayLinked&&
        a.relaySectors==b.relaySectors&&a.relayPatchA==b.relayPatchA&&a.relayPatchB==b.relayPatchB&&
        a.relayFieldTasks==b.relayFieldTasks&&a.extractionRoute==b.extractionRoute&&
        std::bit_cast<unsigned>(a.feet.x)==std::bit_cast<unsigned>(b.feet.x)&&
        std::bit_cast<unsigned>(a.feet.y)==std::bit_cast<unsigned>(b.feet.y)&&
        std::bit_cast<unsigned>(a.feet.z)==std::bit_cast<unsigned>(b.feet.z);
}
Session Powered(){
    Session s;
    for(unsigned i=0;i<3;++i){s.feet=Session::supplies[i];Need(s.Collect(i),"collect");}
    for(unsigned i=0;i<3;++i){s.feet=Session::repairSites[i];Need(s.CollectRepairMaterial(i,i==0),"materials");}
    s.feet=Session::bridge;Need(s.RepairBridge(),"bridge");s.feet=Session::arena;Need(s.FinishEncounter(),"encounter");
    s.feet=Session::rangerLog;Need(s.ReadFuseClue(),"clue");s.feet=Session::fuse;
    for(unsigned i=0;i<3;++i)for(unsigned turn=0;turn<Session::fuseCode[i];++turn)Need(s.CycleFuseDial(i),"dial");
    Need(s.CollectFuse(),"fuse");s.feet=Session::generator;
    Need(s.ToggleCircuit(0)&&s.ToggleCircuit(2)&&s.RepairGenerator()&&s.Valid(),"power");return s;
}
void Aim(Session& s,unsigned index){
    for(unsigned n=0;n<16&&s.RelaySector(index)!=Session::RelayTargetSector(index);++n)
        Need(s.TuneRelay(index,1),"aim unchanged original gate");
    Need(s.RelaySignal(index)==100,"aim signal");
}
void InteractionFeedbackCases(){
    using R=FieldInput::Result;
    for(unsigned a=0;a<3;++a)for(unsigned b=0;b<3;++b){
        auto s=Powered();s.feet=Session::relaySites[0];
        s.relayPatchA=a;s.relayPatchB=b;const auto before=s;
        const auto result=FieldInput::Interact(s);
        const auto* feedback=FieldInput::InteractionFeedback(s,result);
        Need(feedback!=nullptr,"west E feedback missing");
        const std::string visible(FieldPresentation::LastAction(feedback));
        if(a==1&&b==0){
            Need(result==R::CircuitConfirmed&&visible==FieldInput::Feedback(result),"success feedback changed");
            Need(s.relayLinked==0,"feedback auto linked west");
        }else{
            Need(result==R::None&&Same(s,before),"rejected feedback mutated wiring");
            const auto status=FieldRelayTask::TraceCircuit(a,b).status;
            Need(visible.find(status==FieldRelayTask::CircuitStatus::Grounded?"接地":"回路")!=std::string::npos&&
                 visible.find("接頭")!=std::string::npos&&visible.find("100")==std::string::npos,
                 "west rejection omitted cause or requested antenna too soon");
            ++negativeCases;
        }
        const auto after=s;(void)FieldInput::InteractionFeedback(s,result);
        Need(Same(s,after),"feedback query mutated state");++presentationCases;
    }
    for(unsigned route=0;route<3;++route){
        auto s=Powered();s.feet=Session::relaySites[2];s.extractionRoute=route;const auto before=s;
        const auto result=FieldInput::Interact(s);const auto* feedback=FieldInput::InteractionFeedback(s,result);
        Need(feedback!=nullptr,"north E feedback missing");
        if(route==1)Need(result==R::RouteConfirmed&&std::string(feedback)==FieldInput::Feedback(result)&&s.relayLinked==0,"north success feedback changed");
        else Need(result==R::None&&Same(s,before)&&std::string(feedback).find("3 切換入口卡")!=std::string::npos,
                  "north rejection requested antenna before route");
        ++presentationCases;
    }
    for(unsigned index=0;index<3;++index)for(unsigned sector=0;sector<16;++sector){
        auto s=Powered();s.feet=Session::relaySites[index];
        if(index==0){s.relayPatchA=1;Need(s.ConfirmRelayCircuit(),"feedback west setup");}
        if(index==2){s.extractionRoute=1;Need(s.ConfirmExtractionRoute(),"feedback north setup");}
        for(unsigned n=0;n<sector;++n)Need(s.TuneRelay(index,1),"feedback antenna setup");
        const auto before=s;const bool aligned=s.RelaySignal(index)==100;
        const auto result=FieldInput::Interact(s);const auto* feedback=FieldInput::InteractionFeedback(s,result);
        Need(feedback!=nullptr,"antenna E feedback missing");
        if(aligned)Need(result==R::RelayConnected&&std::string(feedback)==FieldInput::Feedback(result),"link feedback changed");
        else Need(result==R::None&&Same(s,before)&&std::string(feedback).find("天線尚未對準")!=std::string::npos,
                  "unaligned antenna rejection missing");
        const auto after=s;(void)FieldInput::InteractionFeedback(s,result);Need(Same(s,after),"antenna feedback mutated state");
        ++presentationCases;
    }
    for(unsigned variant=0;variant<7;++variant){
        auto s=Powered();s.feet=Session::relaySites[0];
        if(variant==0)s.health=0;if(variant==1)s.feet.x+=2;if(variant==2)s.feet.y+=.13f;
        if(variant==3)s.feet.y=(std::numeric_limits<float>::quiet_NaN)();
        if(variant==4)s.relayPatchA=3;if(variant==5)s={};
        if(variant==6){s.relayPatchA=1;Need(s.ConfirmRelayCircuit(),"linked setup circuit");Aim(s,0);Need(s.ConnectRelay(0),"linked setup connect");}
        const auto before=s;const auto result=FieldInput::Interact(s);
        const auto* feedback=FieldInput::InteractionFeedback(s,result);
        if(result==R::Invalid)Need(feedback&&std::string(feedback)==FieldInput::Feedback(R::Invalid),"invalid feedback changed");
        else Need(result==R::None&&feedback==nullptr,"unavailable site invented rejection reason");
        Need(Same(s,before),"unavailable feedback mutated progress");++negativeCases;
    }
    std::cout<<"INTERACTION_FEEDBACK_CPU wiring=9 routes=3 antenna=48 unavailable=7 state_rules=unchanged native=not-run\n";
}
void Input(){
    using R=FieldInput::Result;
    for(unsigned a=0;a<3;++a)for(unsigned b=0;b<3;++b){
        auto s=Powered();s.feet=Session::relaySites[0];Aim(s,0);const auto sectors=s.relaySectors;
        for(unsigned n=0;n<a;++n)Need(FieldInput::Lever(s,0)==R::PatchChanged,"west A key");
        for(unsigned n=0;n<b;++n)Need(FieldInput::Lever(s,1)==R::PatchChanged,"west B key");
        Need(s.relaySectors==sectors,"patch key silently aimed antenna");
        const auto before=s;const bool correct=a==1&&b==0;
        Need(FieldInput::Interact(s)==(correct?R::CircuitConfirmed:R::None),"west confirmation result");
        Need(s.relayLinked==0,"one E confirmed and linked");
        if(!correct){Need(Same(s,before),"wrong topology changed progress");++negativeCases;}
        else {Need(FieldInput::Interact(s)==R::RelayConnected&&s.relayLinked==1,"second E did not link");
            const auto locked=s;Need(FieldInput::Lever(s,0)==R::None&&Same(s,locked),"linked west remained editable");}
        ++inputCases;
    }
    for(unsigned route=0;route<3;++route){
        auto s=Powered();s.feet=Session::relaySites[2];Aim(s,2);
        for(unsigned n=0;n<route;++n)Need(FieldInput::Lever(s,2)==R::RouteChanged,"north route key");
        const auto before=s;Need(FieldInput::Interact(s)==(route==1?R::RouteConfirmed:R::None),"north route confirmation");
        Need(s.relayLinked==0,"north one E also linked");
        if(route!=1){Need(Same(s,before),"unsafe route changed progress");++negativeCases;}
        else Need(FieldInput::Interact(s)==R::RelayConnected&&s.relayLinked==4,"second north E did not link");
        ++inputCases;
    }
    std::array order{0u,1u,2u};
    do {
        auto s=Powered();
        for(auto index:order){s.feet=Session::relaySites[index];
            if(index==0){Need(FieldInput::Lever(s,0)==R::PatchChanged,"order west patch");Need(FieldInput::Interact(s)==R::CircuitConfirmed,"order west confirm");}
            if(index==2){Need(FieldInput::Lever(s,2)==R::RouteChanged,"order north card");Need(FieldInput::Interact(s)==R::RouteConfirmed,"order north confirm");}
            for(unsigned n=0;n<16&&s.RelaySignal(index)!=100;++n)Need(FieldInput::Lever(s,1)==R::AntennaChanged,"order aim");
            Need(FieldInput::Interact(s)==R::RelayConnected,"order link");
        }
        s.feet=Session::beacon;Need(s.LightBeacon()&&s.Valid(),"order complete");++inputCases;
    }while(std::next_permutation(order.begin(),order.end()));
    for(unsigned variant=0;variant<6;++variant){
        auto s=Powered();s.feet=Session::relaySites[0];
        if(variant==0)s.health=0;if(variant==1)s.feet.x+=2;if(variant==2)s.feet.y+=.13f;
        if(variant==3)s.feet.y=(std::numeric_limits<float>::quiet_NaN)();
        if(variant==4)s.relayPatchA=3;if(variant==5)s={};
        const auto before=s;const auto result=FieldInput::Lever(s,0);
        Need(result==R::None||result==R::Invalid,"unavailable lever allowed");Need(Same(s,before),"rejected lever mutated state");
        const auto used=FieldInput::Interact(s);Need(used==R::None||used==R::Invalid,"unavailable E allowed");
        Need(Same(s,before),"rejected E mutated state");++negativeCases;
    }
}
std::string AllText(const FieldPresentation::Hud& hud){
    std::string result=hud.objective+hud.navigation+hud.instruction+hud.context+hud.controls;
    for(const auto& line:hud.journal)result+=line;return result;
}
void EarlyToolsRedRegression(){
    Aevrynd::HeightField3D terrain({{-64,0,-16},129,145,1});terrain.Fill(0);
    Aevrynd::Camera3D camera;camera.target={0,1,0};camera.position={0,3,-6};
    const std::vector<Aevrynd::BoxCollider3D> solids;Encounter enemy;Evasion evasion;
    Session session;session.collected=1;session.feet=Session::repairSites[0]+Aevrynd::Vec3{.75f,0,0};
    const auto before=session;
    // 沿用既有低棚可達取物站位；就緒答案來自正式操作副本，不以座標自行猜測。
    auto pickupProbe=session;const bool ready=pickupProbe.CollectRepairMaterial(0,true);
    Need(ready&&pickupProbe.repairMaterials==1&&session.CurrentStage()==Stage::Supplies,
        "early tools RED fixture did not permit actual crouched pickup");
    const auto guide=BuildObjectiveGuide(session,enemy,evasion,camera,true,true,solids,terrain);
    auto supplyProbe=session;
    Need(guide.valid&&guide.hasTarget&&guide.index==1&&!guide.useReady&&
        guide.useReady==supplyProbe.Collect(guide.index),"early tools RED changed supply readiness");
    Need(guide.target.x==Session::supplies[1].x&&guide.target.y==Session::supplies[1].y&&
        guide.target.z==Session::supplies[1].z,"early tools RED changed supply target");
    const auto hud=FieldPresentation::Build(session,guide,enemy,true,false,false);
    Need(hud.valid&&Same(session,before)&&hud.navigation.starts_with("補給箱 2；"),
        "early tools RED changed session or supply navigation");
    const bool toolPrompt=hud.instruction=="E：拾取低棚工具。";
    std::cout<<"EARLY_TOOLS_RED_PROBE actual_pickup_ready="<<ready<<" hud_tool_prompt="<<toolPrompt
        <<" crouched=1 collected=1 tool_offset_x=0.75 supply_target_preserved=1 state_unchanged=1\n"<<std::flush;
    Need(toolPrompt,"EARLY_TOOLS_READY_PICKUP_PROMPT_MISSING");
    ++presentationCases;
}
void EarlyToolsPresentation(){
    Aevrynd::HeightField3D terrain({{-64,0,-16},129,145,1});terrain.Fill(0);
    Aevrynd::Camera3D camera;camera.target={0,1,0};camera.position={0,3,-6};
    const std::vector<Aevrynd::BoxCollider3D> solids;Encounter enemy;Evasion evasion;
    unsigned cases{},readyCases{},rejectedCases{},hiddenReadyCases{};
    const std::string prompt="E：拾取低棚工具。";
    const auto inspect=[&](Session session,bool crouched,bool started=true,bool paused=false){
        const auto before=session;
        auto pickupProbe=session;const bool ready=pickupProbe.CollectRepairMaterial(0,crouched);
        auto expectedPickup=session;if(ready)expectedPickup.repairMaterials|=1u;
        Need(Same(pickupProbe,expectedPickup),"early tools probe changed unrelated state or rejected pickup state");
        const auto guide=BuildObjectiveGuide(session,enemy,evasion,camera,crouched,true,solids,terrain);
        Need(guide.valid&&session.CurrentStage()==Stage::Supplies&&Same(session,before),
            "early tools guide changed state or left supplies phase");
        Need(guide.earlyToolsReady==ready,"early tools readiness differs from real Session pickup gate");
        auto supplyProbe=session;
        Need(guide.useReady==supplyProbe.Collect(guide.index),"early tools changed original supply readiness");
        const auto hud=FieldPresentation::Build(session,guide,enemy,started,paused,false);
        Need(hud.valid&&Same(session,before),"early tools HUD changed session");
        if(session.Alive()){
            const unsigned supplyIndex=session.collected==6?0u:1u;
            Need(guide.hasTarget&&guide.index==supplyIndex&&
                guide.target.x==Session::supplies[supplyIndex].x&&guide.target.y==Session::supplies[supplyIndex].y&&
                guide.target.z==Session::supplies[supplyIndex].z,"early tools replaced original supply target");
            Need(hud.navigation.starts_with("補給箱 "+std::to_string(supplyIndex+1)+"；"),
                "early tools replaced original supply navigation");
            Need(!guide.useReady,"early tools fixture unexpectedly reached supply crate");
        }
        const bool shown=started&&!paused&&session.Alive()&&ready&&!guide.useReady;
        Need((hud.instruction==prompt)==shown,"early tools E prompt differs from active actual pickup gate");
        if(!shown){Need(hud.instruction.find(prompt)==std::string::npos,"inactive early tools prompt remained visible");++negativeCases;}
        if(ready){++readyCases;if(!shown)++hiddenReadyCases;}else ++rejectedCases;
        ++cases;++presentationCases;return hud;
    };
    // 用正式 API 決定每個邊界的答案；半徑沿 z 放置，避免 -8+.8 的 float 相減跨越門檻。
    for(unsigned collected:{0u,1u,6u})for(unsigned materials:{0u,2u,4u,6u})
    for(float radial:{0.f,.75f,.8f,.8001f})for(float height:{0.f,.3f,-.3f,.3001f,-.3001f})
    for(bool crouched:{false,true}){
        Session session;session.collected=collected;session.repairMaterials=materials;
        session.feet=Session::repairSites[0]+Aevrynd::Vec3{0,height,radial};
        (void)inspect(session,crouched);
    }
    Session reachable;reachable.collected=1;reachable.feet=Session::repairSites[0]+Aevrynd::Vec3{.75f,0,0};
    for(unsigned materials:{1u,3u,5u,7u}){
        auto collected=reachable;collected.repairMaterials=materials;
        Need(inspect(collected,true).instruction!=prompt,"already collected tools advertised pickup");
    }
    for(const auto visibility:std::array<std::pair<bool,bool>,3>{{{false,false},{true,true},{false,true}}})
        Need(inspect(reachable,true,visibility.first,visibility.second).instruction!=prompt,
            "unstarted or paused early tools prompt remained active");
    auto dead=reachable;dead.health=0;
    Need(inspect(dead,true).instruction!=prompt,"dead player advertised tools pickup");
    const auto resumed=inspect(reachable,true);
    Need(resumed.instruction==prompt,"resume lost still-ready tools prompt");
    Aevrynd::DrawList draw;ActionFeedbackToast toast;
    FieldPresentation::Draw(draw,resumed,reachable,"",toast,true,false,80,true,false);
    Need(draw.Stats2D().textGlyphs>0&&draw.Stats2D().textSourcePixels>0,
        "early tools actual CPU HUD draw produced no glyph spans");
    // 真正拾取只提交工具 bit；重複 E 不再拾取，也不改原補給進度或位置。
    auto picked=reachable;const auto before=reachable;
    Need(inspect(picked,true).instruction==prompt,"actual pickup missing initial tools prompt");
    Need(picked.CollectRepairMaterial(0,true),"actual early tools pickup rejected");
    auto expected=before;expected.repairMaterials=1;
    Need(Same(picked,expected),"actual tools pickup changed unrelated progress");
    const auto afterPickup=picked;
    Need(!picked.CollectRepairMaterial(0,true)&&Same(picked,afterPickup),"repeated tools pickup changed state");
    Need(inspect(picked,true).instruction!=prompt,"picked tools retained E prompt");
    // 進入原維修階段後仍使用原 useReady；新欄位不能取代既有工具目標。
    auto repair=reachable;repair.collected=7;repair.repairMaterials=6;const auto beforeRepair=repair;
    const auto repairGuide=BuildObjectiveGuide(repair,enemy,evasion,camera,true,true,solids,terrain);
    const auto repairHud=FieldPresentation::Build(repair,repairGuide,enemy,true,false,false);
    Need(repairGuide.valid&&!repairGuide.earlyToolsReady&&repairGuide.useReady&&repairGuide.index==0&&
        repairHud.valid&&repairHud.instruction=="E：拾取維修物資"&&Same(repair,beforeRepair),
        "early tools changed existing repair-stage pickup");++cases;++presentationCases;
    // 無效 Session 或鏡頭不能沿用之前的就緒資料。
    for(unsigned invalid=0;invalid<2;++invalid){
        auto session=reachable;if(invalid==0)session.collected=8;else session.feet.x=(std::numeric_limits<float>::quiet_NaN)();
        const auto unchanged=session;
        const auto guide=BuildObjectiveGuide(session,enemy,evasion,camera,true,true,solids,terrain);
        const auto hud=FieldPresentation::Build(session,guide,enemy,true,false,false);
        Need(!guide.valid&&!guide.earlyToolsReady&&!hud.valid&&hud.instruction!=prompt&&Same(session,unchanged),
            "invalid early tools state advertised readiness");++cases;++negativeCases;
    }
    auto invalidCamera=camera;invalidCamera.position.x=(std::numeric_limits<float>::quiet_NaN)();
    const auto invalidGuide=BuildObjectiveGuide(reachable,enemy,evasion,invalidCamera,true,true,solids,terrain);
    Need(!invalidGuide.valid&&!invalidGuide.earlyToolsReady&&
        !FieldPresentation::Build(reachable,invalidGuide,enemy,true,false,false).valid&&Same(reachable,before),
        "invalid camera retained early tools readiness");++cases;++negativeCases;
    Need(cases==495,"early tools bounded case count changed");
    std::cout<<"EARLY_TOOLS_PRESENTATION_CPU cases="<<cases<<" boundary_cases=480 ready_cases="<<readyCases
        <<" rejected_cases="<<rejectedCases<<" hidden_ready_cases="<<hiddenReadyCases
        <<" actual_session_gate=preserved supply_target_useReady_navigation=preserved state_bits=unchanged"
        <<" CPU_text_draw=1 GPU=not-run human-play=not-run\n";
}
void EarlyTimberPresentation(){
    Aevrynd::HeightField3D terrain({{-64,0,-16},129,145,1});terrain.Fill(0);
    Aevrynd::Camera3D camera;camera.target={0,1,0};camera.position={0,3,-6};
    const std::vector<Aevrynd::BoxCollider3D> solids;Encounter enemy;Evasion evasion;
    unsigned cases{};
    const auto inspect=[&](Session s,bool started=true,bool paused=false){
        const auto before=s;
        const auto guide=BuildObjectiveGuide(s,enemy,evasion,camera,false,true,solids,terrain);
        Need(guide.valid&&s.CurrentStage()==Stage::Supplies,"early timber fixture left supplies phase");
        const auto hud=FieldPresentation::Build(s,guide,enemy,started,paused,false);
        Need(hud.valid&&Same(s,before),"early timber HUD changed session");
        auto supplyProbe=s;
        Need(guide.useReady==supplyProbe.Collect(guide.index),"early timber changed original supply readiness");
        if(s.Alive()){
            Need(guide.hasTarget&&guide.target.x==Session::supplies[guide.index].x&&
                guide.target.y==Session::supplies[guide.index].y&&guide.target.z==Session::supplies[guide.index].z,
                "early timber replaced long-range supply objective");
            Need(hud.navigation.starts_with("補給箱 "),"early timber replaced supply navigation");
        }
        ++cases;++presentationCases;return hud;
    };
    // 正式 E 允許補給尚未齊全時先取木料；提示必須沿用同一個 Session 取物門檻。
    for(unsigned site:{1u,2u})for(unsigned collected:{0u,1u,6u})
    for(float x:{0.f,2.5f,2.501f})for(float y:{0.f,2.f,2.001f}){
        Session s;s.collected=collected;s.feet=Session::repairSites[site]+Aevrynd::Vec3{x,y,0};
        auto probe=s;const bool ready=probe.CollectRepairMaterial(site,false);
        const auto hud=inspect(s);
        const std::string prompt=site==1?"E：拾取東南木料。":"E：拾取西岸木料。";
        Need((hud.instruction==prompt)==ready,"early timber E prompt differs from actual pickup gate");
        if(!ready){Need(hud.instruction.find("E：拾取")==std::string::npos,"out-of-range timber advertised active pickup");++negativeCases;}
    }
    for(unsigned site:{1u,2u}){
        Session s;s.feet=Session::repairSites[site];s.repairMaterials=1u<<site;
        auto probe=s;Need(!probe.CollectRepairMaterial(site,false),"collected timber fixture still usable");
        Need(inspect(s).instruction.find("E：拾取")==std::string::npos,"collected timber advertised active pickup");++negativeCases;
        s.repairMaterials=0;
        Need(inspect(s,false,false).instruction.find("E：拾取")==std::string::npos,"prestart timber advertised active pickup");++negativeCases;
        Need(inspect(s,true,true).instruction.find("E：拾取")==std::string::npos,"paused timber advertised active pickup");++negativeCases;
        s.health=0;
        Need(inspect(s).instruction.find("E：拾取")==std::string::npos,"dead player advertised active timber pickup");++negativeCases;
    }
    Session supplies;supplies.feet=Session::supplies[0];
    Need(inspect(supplies).instruction=="E：拾取補給","existing supply pickup prompt changed");
    Session tools;tools.feet=Session::repairSites[0];auto standing=tools;
    Need(!standing.CollectRepairMaterial(0,false),"standing tools fixture bypassed crouch gate");
    Need(inspect(tools).instruction.find("E：拾取")==std::string::npos,"timber hint granted standing tool pickup");++negativeCases;
    std::cout<<"EARLY_TIMBER_PRESENTATION_CPU cases="<<cases
        <<" original_pickup_gates=preserved long_range_supply_navigation=preserved\n";
}
void CompletionControlsHud(){
    // 新檢查在原summary後獨立計數；只讀HUD，不啟動Renderer或修改控制/任務規則。
    unsigned terminalChecks{},terminalCases{};
    const auto check=[&](bool ok,const char* why){++terminalChecks;if(!ok)throw std::runtime_error(why);};
    Session complete;
    for(unsigned i=0;i<3;++i){complete.feet=Session::supplies[i];check(complete.Collect(i),"terminal supplies");}
    for(unsigned i=0;i<3;++i){complete.feet=Session::repairSites[i];check(complete.CollectRepairMaterial(i,i==0),"terminal materials");}
    complete.feet=Session::bridge;check(complete.RepairBridge(),"terminal bridge");
    complete.feet=Session::arena;check(complete.FinishEncounter(),"terminal encounter");
    complete.feet=Session::rangerLog;check(complete.ReadFuseClue(),"terminal clue");complete.feet=Session::fuse;
    for(unsigned i=0;i<3;++i)for(unsigned n=0;n<Session::fuseCode[i];++n)check(complete.CycleFuseDial(i),"terminal dial");
    check(complete.CollectFuse(),"terminal fuse");complete.feet=Session::generator;
    check(complete.ToggleCircuit(0)&&complete.ToggleCircuit(2)&&complete.RepairGenerator(),"terminal power");
    for(unsigned i=0;i<3;++i){
        complete.feet=Session::relaySites[i];
        if(i==0)check(complete.CycleRelayPatch(0)&&complete.ConfirmRelayCircuit(),"terminal west");
        if(i==2)check(complete.CycleExtractionRoute()&&complete.ConfirmExtractionRoute(),"terminal northwest");
        for(unsigned n=0;n<16&&complete.RelaySignal(i)!=100;++n)check(complete.TuneRelay(i,1),"terminal antenna");
        check(complete.ConnectRelay(i),"terminal link");
    }
    complete.feet=Session::beacon;check(complete.LightBeacon()&&complete.Valid(),"terminal complete");
    const auto inspect=[&](const Session& state,bool started,bool paused,bool journal){
        const auto before=state.Serialize();ObjectiveGuide guide;guide.valid=true;Encounter enemy;
        const auto hud=FieldPresentation::Build(state,guide,enemy,started,paused,journal);
        check(hud.valid,"terminal HUD validity");
        const bool inspection=started&&!paused&&state.Alive()&&state.beaconLit;
        if(inspection){
            for(const auto* word:{"拖曳環繞","滾輪縮放","←→","F5 保存","F9 恢復","R 重開","Esc 暫停","J 手冊"})
                check(hud.controls.find(word)!=std::string::npos,"terminal actual controls omitted");
            for(const auto* word:{"WASD","Shift","Space","Ctrl","Q 閃避","E 互動","F 反擊"})
                check(hud.controls.find(word)==std::string::npos,"terminal advertised frozen player controls");
            check(hud.instruction.find("角色與任務不再前進")!=std::string::npos,"terminal freeze explanation omitted");
            check(hud.context=="F5 保存；F9 恢復；R 開始新一輪。","terminal save restart context changed");
        }else{
            check(hud.controls.find("拖曳環繞")==std::string::npos,"inactive HUD advertised completed camera controls");
            if(started&&paused)check(hud.controls.find("Esc 繼續")!=std::string::npos&&hud.controls.find("R 重試")==std::string::npos,"terminal pause priority");
            else if(!state.Alive())check(hud.objective=="巡守失敗"&&hud.controls.find("R 重試")!=std::string::npos,"terminal death priority");
            else if(!started)check(hud.instruction.find("Enter")!=std::string::npos,"terminal start priority");
            else check(hud.controls.find("WASD")!=std::string::npos,"ordinary live controls changed");
        }
        Aevrynd::DrawList draw;ActionFeedbackToast toast;
        const auto wrapped=FieldPresentation::Wrap(hud.controls,1240.f,[&](std::string_view value){return draw.MeasureSystemText(value,7.f*2.6f).x;});
        std::string joined;for(const auto& row:wrapped)joined+=row;
        check(joined==hud.controls,"terminal UTF8 controls lost");
        (void)FieldPresentation::MeasureLayout(draw,hud,state,"",toast,started,paused,true,false,45);
        FieldPresentation::Draw(draw,hud,state,"",toast,true,false,45,started,paused);
        check(draw.Stats2D().textGlyphs>0&&draw.Stats2D().textSourcePixels>0,"terminal actual glyph draw missing");
        check(state.Serialize()==before,"terminal presentation mutated Session");++terminalCases;
    };
    for(bool journal:{false,true}){
        inspect(complete,true,false,journal);inspect(complete,true,true,journal);inspect(complete,false,false,journal);
        auto dead=complete;dead.Damage(100);inspect(dead,true,false,journal);inspect(dead,true,true,journal);
        inspect(Session{},true,false,journal);
    }
    check(terminalCases==12,"terminal exact bounded cases");
    std::cout<<"COMPLETION_CONTROLS_HUD_CPU_PASS checks="<<terminalChecks<<" cases="<<terminalCases
        <<" complete_only=1 priority_preserved=1 actual_glyphs=1 state_readonly=1 GPU=not-run human-play=not-run\n";
}

void Presentation(){
    ObjectiveGuide guide;guide.valid=true;guide.hasTarget=true;guide.distance=12;guide.index=0;
    Encounter enemy;Aevrynd::DrawList draw;
    const auto inspect=[&](Session s,bool started,bool paused,bool journal){
        const auto before=s;const auto hud=FieldPresentation::Build(s,guide,enemy,started,paused,journal);
        Need(hud.valid&&Same(s,before),"HUD mutated progress or rejected valid state");
        Need(hud.journal.size()<=14,"journal unbounded");
        std::vector<std::string> values{hud.objective,hud.navigation,hud.instruction,hud.context,hud.controls};
        values.insert(values.end(),hud.journal.begin(),hud.journal.end());
        unsigned wrappedCount{};
        for(const auto& value:values){
            const auto lines=FieldPresentation::Wrap(value,1040.f,[&](std::string_view t){return draw.MeasureText(t,2.6f).x;});
            std::string joined;
            for(const auto& line:lines){Need(draw.MeasureText(line,2.6f).x<=1040.f,"real GDI text overflow");joined+=line;++wrappedCount;}
            Need(joined==value,"UTF8 text dropped while wrapping");
        }
        Need(wrappedCount<=26,"measured journal/content exceeded bounded budget");
        draw.Clear();ActionFeedbackToast toast;
        FieldPresentation::Draw(draw,hud,s,"",toast,true,false,80,started,paused);
        Need(draw.Stats2D().textGlyphs>0&&draw.Stats2D().textSourcePixels>0,"actual CPU HUD draw produced no glyph spans");
        ++presentationCases;return hud;
    };
    Session fresh;const auto freshHud=inspect(fresh,false,false,true);
    Need(AllText(freshHud).find("2 - 0 - 3")==std::string::npos,"unread clue disclosed");
    auto s=Powered();s.feet=Session::relaySites[0];
    for(unsigned a=0;a<3;++a)for(unsigned b=0;b<3;++b){s.relayPatchA=a;s.relayPatchB=b;const auto hud=inspect(s,true,false,true);
        Need(hud.context==FieldPresentation::Circuit(s),"live circuit trace missing");}
    guide.index=2;s.feet=Session::relaySites[2];
    for(unsigned route=0;route<3;++route){s.extractionRoute=route;const auto hud=inspect(s,true,false,true);
        Need(hud.context==FieldPresentation::Route(route),"live route card missing");}
    s=Powered();inspect(s,true,true,true);
    s={};s.collected=7;s.repairMaterials=7;s.bridgeRepaired=true;s.feet=Session::arena;
    const auto combat=inspect(s,true,false,true);Need(combat.compactJournal&&combat.journal.size()==2,"combat large journal hides world");
    const auto paused=inspect(s,true,true,true);Need(!paused.compactJournal,"paused journal unavailable");
    auto dead=s;dead.health=0;const auto deadHud=inspect(dead,true,false,true);Need(deadHud.objective=="巡守失敗","death wording");
    Need(deadHud.navigation.empty()&&deadHud.instruction.find("橋頭")!=std::string::npos,"Dead player navigated to enemy or wrong checkpoint");
    Need(deadHud.context.find("已修復")!=std::string::npos&&deadHud.controls.find("R 重試")!=std::string::npos,"Death checkpoint progress and retry control missing");
    for(const auto prohibited:{"WASD","Q 閃避","F 反擊","E 互動","Space 跳躍"})
        Need(AllText(deadHud).find(prohibited)==std::string::npos,"Death HUD advertised live player controls");
    const auto stale=FieldPresentation::LastAction("Leave the marked circle - Q: evade");
    const auto deadStatus=FieldPresentation::StatusLine(dead,stale);
    Need(deadStatus=="生命 0；移動、跳躍、互動與戰鬥已停止。"&&deadStatus.find(stale)==std::string::npos,"Stale windup survived in death status line");
    draw.Clear();ActionFeedbackToast deathToast;deathToast.Publish(ActionFeedbackEvent::LoadUnavailable);
    FieldPresentation::Draw(draw,deadHud,dead,stale,deathToast,true,false,80,true,false);
    Need(draw.Stats2D().textGlyphs>0,"Dead HUD failed real text rendering");
    auto earlyDeath=Session{};earlyDeath.health=0;earlyDeath.collected=1;
    const auto earlyHud=inspect(earlyDeath,true,false,false);
    Need(earlyHud.instruction.find("起點")!=std::string::npos&&earlyHud.instruction.find("橋頭")==std::string::npos&&earlyHud.context.find("未修復")!=std::string::npos,"Early death invented repaired checkpoint");
    const auto deadPaused=inspect(dead,true,true,true);
    Need(deadPaused.controls.find("R 重試")==std::string::npos&&deadPaused.controls.find("Esc 繼續")!=std::string::npos,"Paused death advertised an inactive retry key");
    const auto aliveAgain=inspect(s,true,false,false);
    Need(aliveAgain.controls.find("WASD")!=std::string::npos&&FieldPresentation::StatusLine(s,"已儲存").find("已儲存")!=std::string::npos,"Revived player lost normal controls or action feedback");
    const auto invalidGuide=guide;guide.distance=(std::numeric_limits<double>::infinity)();
    Need(!FieldPresentation::Build(s,guide,enemy,true,false,true).valid,"nonfinite bearing accepted");++negativeCases;guide=invalidGuide;
    s.relayPatchA=3;Need(!FieldPresentation::Build(s,guide,enemy,true,false,true).valid,"invalid session displayed as ready");++negativeCases;
    for(unsigned event=1;event<=5;++event)Need(!FieldPresentation::Toast(static_cast<ActionFeedbackEvent>(event)).empty(),"missing truthful feedback translation");
    Need(FieldPresentation::Toast(ActionFeedbackEvent::None).empty(),"invented last action");
    Need(FieldPresentation::LastAction("Low ceiling - move clear before standing")==
        "上方空間不足；保持蹲姿移出屋簷，再放開 Ctrl","Low roof actionable hint missing");
    Need(FieldPresentation::LastAction("Headroom clear - standing restored")==
        "已離開低頂，恢復站立","Standing recovery feedback missing");
    Need(Aevrynd::DrawList::SystemGlyphCacheStats().residentGlyphs>0,"Chinese glyphs never measured");
    for(const auto& bad:std::array<std::string,5>{std::string("\xc0\xaf"),std::string("\xe0\x80\xaf"),std::string("\xed\xa0\x80"),std::string("\xf4\x90\x80\x80"),std::string("\xe4\xb8")}){
        bool rejected=false;try{FieldPresentation::Wrap(bad,1040.f,[&](std::string_view t){return draw.MeasureText(t,2.6f).x;});}catch(const std::exception&){rejected=true;}
        Need(rejected,"malformed UTF8 not rejected");++negativeCases;
    }
}
// 下列為真實系統字型的 CPU 版面檢查，不是 GPU 截圖或真人操作驗收。
void MeasuredLayout(){
    using namespace FieldPresentation;
    Aevrynd::DrawList draw;
    ObjectiveGuide guide;guide.valid=true;guide.hasTarget=true;guide.distance=12;guide.index=0;
    Encounter enemy;auto session=Powered();session.feet=Session::relaySites[0];
    const auto before=session;
    const auto covers=[&](const PanelRect& panel,float x,float y,std::string_view text,float scale){
        const auto size=draw.MeasureSystemText(text,7.f*scale);
        Need(std::isfinite(size.x)&&std::isfinite(size.y),"nonfinite actual system measure");
        Need(x>=panel.left&&y>=panel.top&&x+size.x<=panel.Right()&&y+size.y<=panel.Bottom(),
            "actual system text not covered by panel");
    };
    const auto bounded=[&](const PanelRect& panel){
        Need(panel.left>=0&&panel.top>=0&&panel.width>=0&&panel.height>=0&&
            panel.Right()<=1280&&panel.Bottom()<=720,"panel outside real canvas");
    };
    const auto drawnPanel=[&](const PanelRect& panel,Aevrynd::Color color){
        Aevrynd::DrawList expected;
        expected.Quad({panel.left+panel.width*.5f,panel.top+panel.height*.5f},
            {panel.width,panel.height},color);
        Need(expected.vertices.size()==6,"reference panel did not emit a quad");
        bool found=false;
        for(std::size_t i=0;i+6<=draw.vertices.size();i+=6){
            bool same=true;
            for(std::size_t j=0;j<6;++j){
                const auto& actual=draw.vertices[i+j];const auto& want=expected.vertices[j];
                if(actual.x!=want.x||actual.y!=want.y||actual.color!=want.color)same=false;
            }
            if(same){found=true;break;}
        }
        Need(found,"Draw did not use measured layout panel");
    };
    ActionFeedbackToast toast;
    const auto plain=Build(session,guide,enemy,true,false,false);
    const auto normal=MeasureLayout(draw,plain,session,"",toast,true);
    Need(!normal.showJournal&&!normal.showToast,"normal layout invented overlay");
    Need(normal.top.width<1280&&normal.bottom.width<1280,"full width black bar retained");
    Need(normal.top.width*normal.top.height+normal.bottom.width*normal.bottom.height<
        1280.f*(76.f+144.f),"normal panel area not reduced");
    covers(normal.top,20,12,"霧境守望 · 孤橋復聯",2.9f);
    covers(normal.top,20,42,plain.objective,2.7f);
    covers(normal.bottom,20,584,plain.navigation,2.6f);
    covers(normal.bottom,20,609,plain.instruction,2.7f);
    covers(normal.bottom,20,634,plain.context,2.6f);
    covers(normal.bottom,20,661,plain.controls,2.6f);
    covers(normal.bottom,20,690,StatusLine(session,""),2.6f);
    const float exactTopWidth=(std::max)(draw.MeasureSystemText("霧境守望 · 孤橋復聯",7.f*2.9f).x,
        draw.MeasureSystemText(plain.objective,7.f*2.7f).x)+24;
    Need(normal.top.width==exactTopWidth,"top not sized by actual SystemText");
    float exactBottomWidth=0;
    for(const auto& entry:std::array<std::pair<std::string,float>,5>{{
        {plain.navigation,2.6f},{plain.instruction,2.7f},{plain.context,2.6f},
        {plain.controls,2.6f},{StatusLine(session,""),2.6f}}})
        exactBottomWidth=(std::max)(exactBottomWidth,draw.MeasureSystemText(entry.first,7.f*entry.second).x);
    Need(normal.bottom.width==exactBottomWidth+24,"bottom not sized by actual SystemText");
    draw.Clear();Draw(draw,plain,session,"",toast,true,false,80,true,false);
    drawnPanel(normal.top,{.02f,.04f,.04f,.9f});
    drawnPanel(normal.bottom,{.02f,.04f,.04f,.83f});
    Need(plain.navigation.find("不保證直線可通行")!=std::string::npos,"navigation safety warning omitted");
    Need(LastAction("TEST FIXTURE - not a playthrough or player save").find("非通關或玩家存檔")!=std::string::npos,
        "test fixture disclaimer omitted");
    Session combat;combat.collected=7;combat.repairMaterials=7;combat.bridgeRepaired=true;
    combat.feet=Session::arena;
    const auto compact=Build(combat,guide,enemy,true,false,true);
    Need(compact.compactJournal,"fixture not actual compact combat journal");
    const auto full=Build(session,guide,enemy,true,false,true);
    Need(!full.compactJournal&&!full.journal.empty(),"fixture not actual full journal");
    // 逐一檢查每種操作結果；版面計算／繪圖均不得重設或倒數通知。
    for(unsigned event=1;event<=5;++event){
        Need(toast.Publish(static_cast<ActionFeedbackEvent>(event)),"toast publication failed");
        Need(toast.Advance(.5,ActionFeedbackClock::Active),"toast advance failed");
        const auto remaining=toast.RemainingSeconds();const auto savedEvent=toast.Event();
        for(unsigned mode=0;mode<3;++mode){
            const auto& hud=mode==0?plain:(mode==1?compact:full);
            const auto& observed=mode==1?combat:session;
            const auto layout=MeasureLayout(draw,hud,observed,"",toast,true);
            bounded(layout.top);bounded(layout.bottom);bounded(layout.toast);
            Need(layout.showToast,"visible toast omitted");
            covers(layout.toast,20,layout.toastTextY,Toast(toast.Event()),2.6f);
            if(mode==0)Need(layout.toastTextY==120,"ordinary toast coordinate changed");
            else{
                bounded(layout.journal);
                Need(layout.showJournal&&layout.toast.top>=layout.journal.Bottom()+8,
                    "toast overlaps journal or loses reserved gap");
                float y=mode==1?101.f:layout.journalRowsY;
                const auto& lines=mode==1?hud.journal:layout.journalRows;
                for(const auto& text:lines){
                    covers(layout.journal,mode==1?40.f:80.f,y,text,2.6f);y+=mode==1?28.f:24.f;
                }
            }
            Need(layout.toast.Bottom()+4<=layout.bottom.top,"toast overlaps bottom panel");
            draw.Clear();Draw(draw,hud,observed,"",toast,true,false,80,true,false);
            drawnPanel(layout.toast,{.02f,.04f,.04f,.9f});
            if(mode!=0)drawnPanel(layout.journal,{.02f,.04f,.04f,mode==1?.9f:.95f});
            Need(toast.Event()==savedEvent&&toast.RemainingSeconds()==remaining,"layout changed toast timing");
            ++presentationCases;
        }
    }
    Need(!MeasureLayout(draw,full,session,"",toast,false).showToast,"prestart toast incorrectly visible");
    Need(Same(session,before),"layout mutated gameplay state");
    // 刻意使用最末列可達舊底板邊界的合法列數，確認延展底板仍保留通知空間。
    auto sixteen=full;sixteen.journal.assign(16,"中文列");
    const auto edge=MeasureLayout(draw,sixteen,session,"",toast,true);
    Need(edge.journalRows.size()==16,"journal row boundary changed");
    covers(edge.journal,80,edge.journalRowsY+15*24,"中文列",2.6f);
    covers(edge.journal,80,edge.journalTitleY,"巡守手冊（J 關閉；手冊不會自動暫停遊戲）",2.9f);
    const float h=draw.MeasureSystemText("中文列",7.f*2.6f).y;
    const float th=draw.MeasureSystemText(Toast(toast.Event()),7.f*2.6f).y;
    const float overflow=(std::max)(520.f,505.f+h+4.f)+8.f+(std::max)(30.f,th+9.f)+4.f-576.f;
    const float shift=overflow>0?std::ceil(overflow):0;
    Need(edge.journalTitleY==108-shift&&edge.journalRowsY==145-shift&&shift<=20,
        "readable journal displacement must match actual overflow only");
    draw.Clear();Draw(draw,sixteen,session,"",toast,true,false,80,true,false);
    drawnPanel(edge.journal,{.02f,.04f,.04f,.95f});drawnPanel(edge.toast,{.02f,.04f,.04f,.9f});
    Need(edge.toast.top>=edge.journal.Bottom()+8&&edge.toast.Bottom()+4<=edge.bottom.top,
        "sixteen-row journal loses toast gap");
    std::cout<<"HUD_MEASURED_LAYOUT_CPU top_width="<<normal.top.width<<" bottom_width="<<normal.bottom.width
        <<" panel_area="<<normal.top.width*normal.top.height+normal.bottom.width*normal.bottom.height
        <<" original_area=281600 sixteen_row_shift="<<shift<<" font_height="<<h<<'\n';
    // 音效狀態不可因開始／暫停時開啟手冊而消失；所有合法音量皆以真字形量測。
    unsigned audioLayouts{},audioDraws{};
    for(unsigned mode=0;mode<5;++mode){
        const bool started=mode>=2,paused=mode>=2,journal=mode==1||mode>=3;
        auto hud=Build(session,guide,enemy,started,paused,journal);
        if(mode==4)hud.journal.assign(16,"中文列");
        for(unsigned available=0;available<2;++available)for(unsigned muted=0;muted<2;++muted)
        for(unsigned event=0;event<=5;++event)for(unsigned volume=0;volume<=100;++volume){
            // 全範圍音量量測一次；另外逐種通知驗證首尾及預設音量邊界。
            if(event!=0&&volume!=0&&volume!=80&&volume!=100)continue;
            ActionFeedbackToast audioToast;
            if(event)Need(audioToast.Publish(static_cast<ActionFeedbackEvent>(event)),"audio toast publish failed");
            const auto remaining=audioToast.RemainingSeconds();
            const auto layout=MeasureLayout(draw,hud,session,"",audioToast,started,paused,available!=0,muted!=0,volume);
            const auto audioSize=draw.MeasureSystemText(layout.audioText,7.f*MinimumChineseTextScale);
            const auto titleSize=draw.MeasureSystemText("霧境守望 · 孤橋復聯",7.f*2.9f);
            const std::string expectedAudio=std::string(available?"音效：":"音效裝置不可用；")+
                (muted?"已靜音":std::to_string(volume)+"%")+"；M 靜音，PageUp / PageDown 音量。";
            Need(layout.showAudio&&layout.audioText==expectedAudio,"audio state missing or inaccurate");
            bounded(layout.top);bounded(layout.bottom);
            covers(layout.top,layout.audioTextX,layout.audioTextY,layout.audioText,MinimumChineseTextScale);
            Need(layout.audioTextX>=20+titleSize.x+24&&layout.audioTextY+audioSize.y+4<=42,
                "audio overlaps title or objective");
            Need(layout.top.Bottom()<=80,"audio top panel covers full journal");
            Need(layout.showJournal==journal,"audio changed journal visibility");
            if(journal){
                bounded(layout.journal);
                Need(layout.audioTextY+audioSize.y+4<=layout.journal.top,"later journal panel covers audio");
                if(mode==4)Need(layout.journalRows.size()==16,"audio removed boundary journal content");
                float y=layout.journalRowsY;
                for(const auto& row:layout.journalRows){covers(layout.journal,80,y,row,2.6f);y+=24;}
            }
            Need(layout.showToast==(started&&event!=0),"audio changed toast visibility");
            if(layout.showToast){
                bounded(layout.toast);covers(layout.toast,20,layout.toastTextY,Toast(audioToast.Event()),2.6f);
                Need(layout.toast.Bottom()+4<=layout.bottom.top,"audio toast overlaps bottom panel");
                if(journal)Need(layout.toast.top>=layout.journal.Bottom()+8,"audio journal loses toast gap");
            }
            Need(audioToast.RemainingSeconds()==remaining,"audio layout changed toast timer");
            if(event==5&&volume==100){
                draw.Clear();Draw(draw,hud,session,"",audioToast,available!=0,muted!=0,volume,started,paused);
                drawnPanel(layout.top,{.02f,.04f,.04f,.9f});
                Aevrynd::DrawList expected;
                expected.SystemText({layout.audioTextX,layout.audioTextY},expectedAudio,7.f*MinimumChineseTextScale,{1,1,1,1});
                Need(!expected.vertices.empty(),"audio reference draw empty");
                const auto found=std::search(draw.vertices.begin(),draw.vertices.end(),expected.vertices.begin(),expected.vertices.end(),
                    [](const auto& actual,const auto& want){return actual.x==want.x&&actual.y==want.y&&actual.color==want.color;});
                Need(found!=draw.vertices.end(),"Draw did not emit measured audio glyphs at reserved location");
                ++audioDraws;
            }
            ++audioLayouts;
        }
    }
    const auto noAudio=MeasureLayout(draw,full,session,"",toast,true,false,false,true,100);
    Need(!noAudio.showAudio&&noAudio.audioText.empty(),"active gameplay invented audio overlay");
    bool invalidVolumeRejected=false;
    try{(void)MeasureLayout(draw,full,session,"",toast,true,true,true,false,101);}catch(const std::exception&){invalidVolumeRejected=true;}
    Need(invalidVolumeRejected,"invalid audio percentage accepted");++negativeCases;
    Need(Same(session,before),"audio layout mutated gameplay state");
    std::cout<<"HUD_AUDIO_LAYOUT_CPU layouts="<<audioLayouts<<" draws="<<audioDraws
        <<" volume_range=0..100 device_states=2 mute_states=2 toast_states=6 sixteen_rows=preserved\n";
    for(unsigned bad=0;bad<3;++bad){
        auto invalid=plain;
        if(bad==0)invalid.objective=std::string(2000,'W');
        if(bad==1)invalid.context=std::string("\xc0\xaf");
        if(bad==2){invalid.journal.assign(17,"中文列");invalid.compactJournal=false;}
        bool rejected=false;
        try{(void)MeasureLayout(draw,invalid,session,"",toast,true);}catch(const std::exception&){rejected=true;}
        Need(rejected,"invalid measured layout accepted");++negativeCases;
    }
}

// 獨立計數的播放邊界展示；使用正式 Guide，不啟動世界更新或改變原測試的計數與斷言。
void PausedEncounterReadinessPresentation(){
    unsigned boundaryChecks{},boundaryCases{},readyGuides{},unreadyGuides{},terminalCases{};
    const auto check=[&](bool ok,const char* why){++boundaryChecks;if(!ok)throw std::runtime_error(why);};
    const auto sameFloat=[](float a,float b){return std::bit_cast<unsigned>(a)==std::bit_cast<unsigned>(b);};
    const auto sameVector=[&](Aevrynd::Vec3 a,Aevrynd::Vec3 b){
        return sameFloat(a.x,b.x)&&sameFloat(a.y,b.y)&&sameFloat(a.z,b.z);};
    const auto sameEnemy=[&](const Encounter& a,const Encounter& b){
        return sameVector(a.feet,b.feet)&&sameVector(a.strikeTarget,b.strikeTarget)&&a.health==b.health&&
            a.phase==b.phase&&sameFloat(a.phaseTime,b.phaseTime)&&sameFloat(a.playerCooldown,b.playerCooldown)&&a.countered==b.countered;};
    const auto sameEvasion=[&](const Evasion& a,const Evasion& b){
        return sameVector(a.direction,b.direction)&&sameFloat(a.remaining,b.remaining)&&sameFloat(a.cooldown,b.cooldown);};
    const auto sameGuide=[&](const ObjectiveGuide& a,const ObjectiveGuide& b){
        return a.valid==b.valid&&a.hasTarget==b.hasTarget&&a.useReady==b.useReady&&a.counterReady==b.counterReady&&
            a.evadeReady==b.evadeReady&&a.tuneReady==b.tuneReady&&a.earlyToolsReady==b.earlyToolsReady&&
            a.bridgeIntent==b.bridgeIntent&&a.index==b.index&&sameVector(a.target,b.target)&&
            std::bit_cast<std::uint64_t>(a.distance)==std::bit_cast<std::uint64_t>(b.distance)&&
            std::bit_cast<std::uint64_t>(a.relativeRadians)==std::bit_cast<std::uint64_t>(b.relativeRadians)&&
            a.bearing==b.bearing&&a.objective==b.objective&&a.navigation==b.navigation&&
            a.instruction==b.instruction&&a.context==b.context;};
    Session encounterSession;
    for(unsigned i=0;i<3;++i){encounterSession.feet=Session::supplies[i];check(encounterSession.Collect(i),"paused Q supplies setup");}
    for(unsigned i=0;i<3;++i){encounterSession.feet=Session::repairSites[i];
        check(encounterSession.CollectRepairMaterial(i,i==0),"paused Q repair items setup");}
    encounterSession.feet=Session::bridge;check(encounterSession.RepairBridge(),"paused Q bridge setup");
    encounterSession.feet=Session::arena+Aevrynd::Vec3{0,0,2};
    check(encounterSession.Valid()&&encounterSession.CurrentStage()==Stage::Encounter,"paused Q real encounter setup");
    Aevrynd::HeightField3D terrain({{-64,0,-16},129,145,1});terrain.Fill(0);
    const std::vector<Aevrynd::BoxCollider3D> boxes;
    Aevrynd::Camera3D camera;camera.target={0,1,48};camera.position={0,3,42};
    const auto inspect=[&](const Session& state,const Encounter& enemy,const Evasion& evasion,
        bool crouched,bool grounded,bool started,bool paused,bool journal,bool expectedReady){
        const auto beforeSession=state;const auto serialized=state.Serialize();const auto beforeEnemy=enemy;const auto beforeEvasion=evasion;
        const auto guide=BuildObjectiveGuide(state,enemy,evasion,camera,crouched,grounded,boxes,terrain);const auto beforeGuide=guide;
        check(guide.valid,"paused Q real guide invalid");
        if(state.Alive()&&state.CurrentStage()==Stage::Encounter)
            check(guide.evadeReady==expectedReady,"paused Q fixture did not produce real readiness");
        const auto hud=FieldPresentation::Build(state,guide,enemy,started,paused,journal);
        check(hud.valid,"paused Q HUD invalid");
        if(!state.Alive()){
            check(hud.context=="補給 3 / 3；工具與木料 3 / 3；橋：已修復","paused Q death progress changed");
            check(hud.controls.find(paused?"Esc 繼續":"R 重試")!=std::string::npos,"paused Q death priority changed");++terminalCases;
        }else if(state.beaconLit){
            check(hud.context=="F5 保存；F9 恢復；R 開始新一輪。","paused Q completion context changed");
            if(started&&!paused){
                check(hud.controls.find("拖曳環繞")!=std::string::npos&&hud.controls.find("WASD")==std::string::npos,
                    "paused Q completion actual controls changed");
                check(hud.instruction.find("角色與任務不再前進")!=std::string::npos,"paused Q terminal freeze explanation changed");
            }++terminalCases;
        }else{
            const auto prefix="野豬生命 "+std::to_string(enemy.health)+" / 3";
            if(started&&!paused){
                const auto expected=prefix+"；"+(expectedReady?std::string("Q 可閃避"):"Q 目前不可用：冷卻、蹲下或離地");
                check(hud.context==expected,"paused Q active or resumed exact context changed");
            }else{
                check(hud.context==prefix,paused?"PAUSED_Q_CONTEXT_STILL_ADVERTISES_ACTIVE_READINESS":"PRESTART_Q_CONTEXT_STILL_ADVERTISES_ACTIVE_READINESS");
                check(hud.context.find("Q 可閃避")==std::string::npos&&hud.context.find("Q 目前不可用")==std::string::npos,
                    "paused Q inactive context advertises a gameplay action");
            }
        }
        if(started&&paused){
            check(hud.objective=="已暫停；目前目標保留"&&
                hud.instruction=="角色與世界暫停更新。"&&
                hud.controls=="Esc 繼續 · M 靜音 · PageUp / PageDown 音量 · J 手冊","paused Q unrelated pause guidance changed");
        }
        if(!started)check(hud.instruction=="Enter 開始巡守；J 查看任務手冊。","paused Q prestart guidance changed");
        Aevrynd::DrawList draw;ActionFeedbackToast toast;
        check(toast.Publish(ActionFeedbackEvent::Loaded)&&toast.Advance(.5,ActionFeedbackClock::Active),"paused Q toast setup");
        const auto event=toast.Event();const auto remaining=toast.RemainingSeconds();
        const auto historical=FieldPresentation::LastAction("Leave the marked circle - Q: evade");
        const auto layout=FieldPresentation::MeasureLayout(draw,hud,state,historical,toast,started,paused,true,false,45);
        FieldPresentation::Draw(draw,hud,state,historical,toast,true,false,45,started,paused);
        check(draw.Stats2D().textGlyphs>0&&draw.Stats2D().textSourcePixels>0,"paused Q real CPU glyph drawing missing");
        check(layout.showAudio==(!started||paused)&&layout.showJournal==journal&&layout.showToast==started,
            "paused Q changed overlay visibility");
        for(const auto panel:std::array<FieldPresentation::PanelRect,4>{layout.top,layout.bottom,layout.journal,layout.toast})
            check(panel.left>=0&&panel.top>=0&&panel.Right()<=1280&&panel.Bottom()<=720,"paused Q measured panel out of canvas");
        if(layout.showToast){
            check(layout.toast.Bottom()+4<=layout.bottom.top,"paused Q toast covers bottom guidance");
            if(layout.showJournal)check(layout.toast.top>=layout.journal.Bottom()+8,"paused Q toast covers journal");
        }
        const auto rows=FieldPresentation::Wrap(hud.context,1240.f,[&](std::string_view text){
            return draw.MeasureSystemText(text,7.f*FieldPresentation::MinimumChineseTextScale).x;});
        std::string joined;for(const auto& row:rows)joined+=row;
        check(joined==hud.context,"paused Q context UTF8 text lost");
        check(toast.Event()==event&&toast.RemainingSeconds()==remaining,"paused Q layout advanced feedback timing");
        check(Same(state,beforeSession)&&state.Serialize()==serialized&&sameEnemy(enemy,beforeEnemy)&&
            sameEvasion(evasion,beforeEvasion)&&sameGuide(guide,beforeGuide),"paused Q presentation mutated gameplay inputs");
        ++boundaryCases;
    };
    for(unsigned phase=0;phase<3;++phase){
        Encounter enemy;enemy.health=3-phase;enemy.phase=static_cast<EnemyPhase>(phase);
        enemy.phaseTime=phase==0?0.f:phase==1?.5f:1.f;enemy.strikeTarget=encounterSession.feet;
        check(enemy.Valid(),"paused Q real enemy phase invalid");
        for(unsigned variant=0;variant<5;++variant){
            Evasion evasion;
            if(variant==1||variant==2){
                check(evasion.Tick(.01f,true,true,false,{0,0,1}).dashing,"paused Q real dodge setup");
                if(variant==1)for(unsigned i=0;i<3;++i)(void)evasion.Tick(.1f,false,true,false,{0,0,1});
                check(variant==1?evasion.remaining==0&&evasion.cooldown>0:evasion.remaining>0,"paused Q unready cause setup");
            }
            if(variant==0)++readyGuides;else ++unreadyGuides;
            for(bool journal:{false,true})for(unsigned playback=0;playback<4;++playback)
                inspect(encounterSession,enemy,evasion,variant==3,variant!=4,playback!=3,playback==1,journal,variant==0);
        }
    }
    Encounter enemy;Evasion evasion;auto dead=encounterSession;dead.Damage(100);
    for(bool journal:{false,true})for(bool paused:{false,true})inspect(dead,enemy,evasion,false,true,true,paused,journal,false);
    auto complete=encounterSession;complete.feet=Session::arena;check(complete.FinishEncounter(),"paused Q terminal encounter setup");
    complete.feet=Session::rangerLog;check(complete.ReadFuseClue(),"paused Q terminal clue setup");complete.feet=Session::fuse;
    for(unsigned i=0;i<3;++i)for(unsigned n=0;n<Session::fuseCode[i];++n)check(complete.CycleFuseDial(i),"paused Q terminal dial setup");
    check(complete.CollectFuse(),"paused Q terminal fuse setup");complete.feet=Session::generator;
    check(complete.ToggleCircuit(0)&&complete.ToggleCircuit(2)&&complete.RepairGenerator(),"paused Q terminal power setup");
    for(unsigned i=0;i<3;++i){
        complete.feet=Session::relaySites[i];
        if(i==0)check(complete.CycleRelayPatch(0)&&complete.ConfirmRelayCircuit(),"paused Q terminal west setup");
        if(i==2)check(complete.CycleExtractionRoute()&&complete.ConfirmExtractionRoute(),"paused Q terminal northwest setup");
        for(unsigned n=0;n<16&&complete.RelaySignal(i)!=100;++n)check(complete.TuneRelay(i,1),"paused Q terminal antenna setup");
        check(complete.ConnectRelay(i),"paused Q terminal relay setup");
    }
    complete.feet=Session::beacon;check(complete.LightBeacon()&&complete.Valid(),"paused Q terminal complete setup");
    for(bool journal:{false,true})for(unsigned playback=0;playback<3;++playback)
        inspect(complete,enemy,evasion,false,true,playback!=2,playback==1,journal,false);
    check(boundaryCases==130&&readyGuides==3&&unreadyGuides==12&&terminalCases==10,"paused Q bounded case inventory");
    std::cout<<"PAUSED_ENCOUNTER_READINESS_HUD_CPU_PASS checks="<<boundaryChecks<<" cases="<<boundaryCases
        <<" real_guide_ready="<<readyGuides<<" real_guide_unready="<<unreadyGuides<<" terminal="<<terminalCases
        <<" original_counters_unchanged=1 exact_active_context=1 state_readonly=1 GPU=not-run human-play=not-run\n";
}

// 獨立六種手冊情境：只驗證正式 Build/字型版面，不啟動世界或更動既有案例計數。
void ContextualManualControlsPresentation(){
    const std::array<unsigned,4> originalCounters{checks,negativeCases,inputCases,presentationCases};
    unsigned localChecks{},localCases{};const char* currentCase="setup";
    const auto check=[&](bool ok,const char* why){++localChecks;if(!ok){
        std::cerr<<"CONTEXTUAL_MANUAL_FIRST_DIVERGENCE case="<<currentCase<<" reason="<<why<<'\n';
        throw std::runtime_error(why);}};
    Session encounterState;
    for(unsigned i=0;i<3;++i){encounterState.feet=Session::supplies[i];check(encounterState.Collect(i),"MANUAL_SUPPLY_SETUP");}
    for(unsigned i=0;i<3;++i){encounterState.feet=Session::repairSites[i];check(encounterState.CollectRepairMaterial(i,i==0),"MANUAL_MATERIAL_SETUP");}
    encounterState.feet=Session::bridge;check(encounterState.RepairBridge(),"MANUAL_BRIDGE_SETUP");
    auto complete=encounterState;complete.feet=Session::arena;check(complete.FinishEncounter(),"MANUAL_ENCOUNTER_SETUP");
    complete.feet=Session::rangerLog;check(complete.ReadFuseClue(),"MANUAL_CLUE_SETUP");complete.feet=Session::fuse;
    for(unsigned i=0;i<3;++i)for(unsigned n=0;n<Session::fuseCode[i];++n)check(complete.CycleFuseDial(i),"MANUAL_DIAL_SETUP");
    check(complete.CollectFuse(),"MANUAL_FUSE_SETUP");complete.feet=Session::generator;
    check(complete.ToggleCircuit(0)&&complete.ToggleCircuit(2)&&complete.RepairGenerator(),"MANUAL_POWER_SETUP");
    for(unsigned i=0;i<3;++i){
        complete.feet=Session::relaySites[i];
        if(i==0)check(complete.CycleRelayPatch(0)&&complete.ConfirmRelayCircuit(),"MANUAL_WEST_SETUP");
        if(i==2)check(complete.CycleExtractionRoute()&&complete.ConfirmExtractionRoute(),"MANUAL_NORTHWEST_SETUP");
        for(unsigned n=0;n<Session::RelayTargetSector(i);++n)check(complete.TuneRelay(i,1),"MANUAL_ANTENNA_SETUP");
        check(complete.ConnectRelay(i),"MANUAL_RELAY_SETUP");
    }
    complete.feet=Session::beacon;check(complete.LightBeacon()&&complete.Valid(),"MANUAL_COMPLETION_SETUP");
    Aevrynd::HeightField3D terrain({{-64,0,-16},129,145,1});terrain.Fill(0);
    const std::vector<Aevrynd::BoxCollider3D> boxes;
    Aevrynd::Camera3D camera;camera.target={0,1,0};camera.position={0,3,-6};const Evasion evasion;
    constexpr std::string_view activeRow="操作：拖曳／滾輪調鏡頭；C 回正；F5 儲存；F9 載入；Esc 暫停。";
    const auto inspect=[&](const char* name,const Session& state,const Encounter& enemy,bool started,bool paused,
                           std::string_view operation,std::size_t rows,bool compact,const char* mismatch){
        currentCase=name;const auto before=state;const auto serialized=state.Serialize();
        const auto guide=BuildObjectiveGuide(state,enemy,evasion,camera,false,true,boxes,terrain);
        check(state.Valid()&&enemy.Valid()&&guide.valid,"MANUAL_REAL_GUIDE_OR_STATE_INVALID");
        const auto hud=FieldPresentation::Build(state,guide,enemy,started,paused,true);
        check(hud.valid&&hud.journal.size()==rows&&hud.compactJournal==compact,"MANUAL_ROW_COUNT_OR_MODE_CHANGED");
        if(!operation.empty())check(hud.journal.at(4)==operation,mismatch);
        if(!state.Alive()){
            const std::vector<std::string> expected{
                "生命已耗盡；目前不能移動、拾取物品或反擊。",
                "恢復遊戲後按 R 回起點，已取得物資會保留。",
                "F9 嘗試載入既有存檔；沒有可用存檔時保留目前狀態。",
                "暫停時先按 Esc 繼續，再重試或讀檔；J 關閉手冊。"};
            check(hud.journal==expected,"MANUAL_DEATH_TEXT_CHANGED");
        }
        if(compact)check(hud.journal==std::vector<std::string>{
            "J 手冊摘要：先離橙圈，恢復時近場 F；Q 不提供免傷。","遊戲未暫停；Esc 才會暫停。"},"MANUAL_COMPACT_TEXT_CHANGED");
        Aevrynd::DrawList draw;ActionFeedbackToast toast;
        const auto event=toast.Event();const auto remaining=toast.RemainingSeconds();
        const auto layout=FieldPresentation::MeasureLayout(draw,hud,state,"",toast,started,paused,true,false,45);
        check(layout.showJournal&&layout.showAudio==(!started||paused)&&!layout.showToast,"MANUAL_OVERLAY_VISIBILITY_CHANGED");
        for(const auto panel:std::array<FieldPresentation::PanelRect,3>{layout.top,layout.bottom,layout.journal})
            check(panel.left>=0&&panel.top>=0&&panel.Right()<=1280&&panel.Bottom()<=720,"MANUAL_MEASURED_PANEL_OUTSIDE_CANVAS");
        check(layout.journalRows.size()<=16,"MANUAL_WRAPPED_ROW_BUDGET");
        if(!compact){
            std::string originalText,wrappedText;for(const auto& row:hud.journal)originalText+=row;
            for(const auto& row:layout.journalRows)wrappedText+=row;
            check(originalText==wrappedText,"MANUAL_WRAP_LOST_UTF8_TEXT");
        }else check(layout.journalRows.empty(),"MANUAL_COMPACT_UNEXPECTED_WRAP");
        FieldPresentation::Draw(draw,hud,state,"",toast,true,false,45,started,paused);
        check(draw.Stats2D().textGlyphs>0&&draw.Stats2D().textSourcePixels>0,"MANUAL_ACTUAL_GLYPH_DRAW_MISSING");
        check(Same(state,before)&&state.Serialize()==serialized&&toast.Event()==event&&toast.RemainingSeconds()==remaining,
              "MANUAL_PRESENTATION_MUTATED_STATE_OR_TOAST");++localCases;
    };
    Session plain;Encounter enemy;
    inspect("paused-alive",plain,enemy,true,true,"暫停中：Esc 繼續後，F5 儲存／F9 載入；J 關閉手冊。",5,false,
            "PAUSED_MANUAL_SAVE_LOAD_CONTEXT_MISSING");
    inspect("prestart-alive",plain,enemy,false,false,"Enter 開始後可用 F5 儲存／F9 載入；J 關閉手冊。",5,false,
            "PRESTART_MANUAL_SAVE_LOAD_CONTEXT_MISSING");
    inspect("active-ordinary",plain,enemy,true,false,activeRow,5,false,"MANUAL_ACTIVE_OPERATION_CHANGED");
    Encounter defeated;defeated.health=0;defeated.phase=EnemyPhase::Defeated;defeated.countered=true;
    inspect("active-complete",complete,defeated,true,false,activeRow,14,false,"MANUAL_COMPLETE_OPERATION_CHANGED");
    auto dead=plain;dead.Damage(100);
    inspect("paused-dead",dead,enemy,true,true,"",4,false,"MANUAL_DEATH_OPERATION_CHANGED");
    encounterState.feet=Session::arena+Aevrynd::Vec3{0,0,2};
    inspect("active-encounter-compact",encounterState,enemy,true,false,"",2,true,"MANUAL_COMPACT_OPERATION_CHANGED");
    check(localCases==6,"MANUAL_BOUNDED_CASE_COUNT");
    check(std::array<unsigned,4>{checks,negativeCases,inputCases,presentationCases}==originalCounters,"MANUAL_CHANGED_ORIGINAL_COUNTERS");
    std::cout<<"CONTEXTUAL_MANUAL_HUD_CPU_PASS cases="<<localCases<<" checks="<<localChecks
        <<" active_exact=1 death_compact_unchanged=1 original_counters_unchanged=1 GPU=not-run human-play=not-run\n";
}

// 先驗證本輪去重問題；獨立計數，不變更既有測試、原130個邊界案例或其通過輸出。
void PausedShortcutDeduplicationPresentation(){
    const std::array<unsigned,4> originalCounters{checks,negativeCases,inputCases,presentationCases};
    unsigned localChecks{},localCases{},pausedCases{},controlCases{};
    const auto check=[&](bool ok,const char* why){++localChecks;if(!ok)throw std::runtime_error(why);};
    const auto count=[](std::string_view text,std::string_view token){
        unsigned found{};std::size_t at{};
        while((at=text.find(token,at))!=std::string_view::npos){++found;at+=token.size();}return found;
    };
    const auto sameFloat=[](float a,float b){return std::bit_cast<unsigned>(a)==std::bit_cast<unsigned>(b);};
    const auto sameVector=[&](Aevrynd::Vec3 a,Aevrynd::Vec3 b){return sameFloat(a.x,b.x)&&sameFloat(a.y,b.y)&&sameFloat(a.z,b.z);};
    const auto sameEnemy=[&](const Encounter& a,const Encounter& b){return sameVector(a.feet,b.feet)&&sameVector(a.strikeTarget,b.strikeTarget)&&
        a.health==b.health&&a.phase==b.phase&&sameFloat(a.phaseTime,b.phaseTime)&&sameFloat(a.playerCooldown,b.playerCooldown)&&a.countered==b.countered;};
    const auto sameEvasion=[&](const Evasion& a,const Evasion& b){return sameVector(a.direction,b.direction)&&sameFloat(a.remaining,b.remaining)&&sameFloat(a.cooldown,b.cooldown);};
    const auto sameGuide=[&](const ObjectiveGuide& a,const ObjectiveGuide& b){return a.valid==b.valid&&a.hasTarget==b.hasTarget&&
        a.useReady==b.useReady&&a.counterReady==b.counterReady&&a.evadeReady==b.evadeReady&&a.tuneReady==b.tuneReady&&
        a.earlyToolsReady==b.earlyToolsReady&&a.bridgeIntent==b.bridgeIntent&&a.index==b.index&&sameVector(a.target,b.target)&&
        std::bit_cast<std::uint64_t>(a.distance)==std::bit_cast<std::uint64_t>(b.distance)&&
        std::bit_cast<std::uint64_t>(a.relativeRadians)==std::bit_cast<std::uint64_t>(b.relativeRadians)&&a.bearing==b.bearing&&
        a.objective==b.objective&&a.navigation==b.navigation&&a.instruction==b.instruction&&a.context==b.context;};
    Session supplies;supplies.feet=Session::supplies[0];supplies.Damage(17);
    Session encounter;
    for(unsigned i=0;i<3;++i){encounter.feet=Session::supplies[i];check(encounter.Collect(i),"dedup supplies setup");}
    for(unsigned i=0;i<3;++i){encounter.feet=Session::repairSites[i];check(encounter.CollectRepairMaterial(i,i==0),"dedup materials setup");}
    encounter.feet=Session::bridge;check(encounter.RepairBridge(),"dedup bridge setup");
    encounter.feet=Session::arena+Aevrynd::Vec3{0,0,2};encounter.Damage(29);
    auto dead=encounter;dead.Damage(100);
    auto complete=encounter;complete.feet=Session::arena;check(complete.FinishEncounter(),"dedup encounter setup");
    complete.feet=Session::rangerLog;check(complete.ReadFuseClue(),"dedup clue setup");complete.feet=Session::fuse;
    for(unsigned i=0;i<3;++i)for(unsigned n=0;n<Session::fuseCode[i];++n)check(complete.CycleFuseDial(i),"dedup dials setup");
    check(complete.CollectFuse(),"dedup fuse setup");complete.feet=Session::generator;
    check(complete.ToggleCircuit(0)&&complete.ToggleCircuit(2)&&complete.RepairGenerator(),"dedup power setup");
    for(unsigned i=0;i<3;++i){complete.feet=Session::relaySites[i];
        if(i==0)check(complete.CycleRelayPatch(0)&&complete.ConfirmRelayCircuit(),"dedup west setup");
        if(i==2)check(complete.CycleExtractionRoute()&&complete.ConfirmExtractionRoute(),"dedup northwest setup");
        for(unsigned n=0;n<16&&complete.RelaySignal(i)!=100;++n)check(complete.TuneRelay(i,1),"dedup aim setup");
        check(complete.ConnectRelay(i),"dedup relay setup");
    }
    complete.feet=Session::beacon;check(complete.LightBeacon(),"dedup beacon setup");
    check(supplies.Valid()&&encounter.Valid()&&dead.Valid()&&complete.Valid()&&
        supplies.CurrentStage()==Stage::Supplies&&encounter.CurrentStage()==Stage::Encounter&&
        !dead.Alive()&&complete.CurrentStage()==Stage::Complete,"dedup real Session fixtures");
    Aevrynd::HeightField3D terrain({{-64,0,-16},129,145,1});terrain.Fill(0);
    const std::vector<Aevrynd::BoxCollider3D> boxes;
    Aevrynd::Camera3D camera;camera.target={0,1,48};camera.position={0,3,42};
    Encounter enemy;Evasion evasion;
    constexpr const char* activeControls="WASD 移動 · Shift 跑步 · Space 跳躍 · Ctrl 蹲下 · Q 閃避 · E 互動 · F 反擊 · J 完整操作";
    constexpr const char* pauseControls="Esc 繼續 · M 靜音 · PageUp / PageDown 音量 · J 手冊";
    const auto inspect=[&](const Session& state,bool started,bool paused,bool journal,
        std::string_view expectedInstruction,std::string_view expectedContext,std::string_view expectedControls){
        const auto before=state;const auto serialized=state.Serialize();const auto beforeEnemy=enemy;const auto beforeEvasion=evasion;
        const auto guide=BuildObjectiveGuide(state,enemy,evasion,camera,false,true,boxes,terrain);const auto beforeGuide=guide;
        check(guide.valid,"dedup real Guide invalid");
        const auto hud=FieldPresentation::Build(state,guide,enemy,started,paused,journal);
        check(hud.valid,"dedup real HUD invalid");
        if(started&&paused){
            // 紅版必須先在新去重斷言失敗，而不是撞到後面的舊字面期待。
            for(const auto* token:{"Esc","M","PageUp","PageDown","J"}){
                check(count(hud.instruction,token)==0,"PAUSED_BOTTOM_SHORTCUT_DUPLICATED");
                check(count(hud.controls,token)==1,"PAUSED_HUD_SINGLE_CONTROL_MISSING_OR_DUPLICATED");
            }
            check(hud.objective=="已暫停；目前目標保留","dedup paused objective changed");++pausedCases;
        }else{check(hud.objective!="已暫停；目前目標保留","dedup pause objective leaked");++controlCases;}
        check(hud.instruction==expectedInstruction,"dedup exact instruction changed");
        check(hud.context==expectedContext,"dedup HP or progress context changed");
        check(hud.controls==expectedControls,"dedup exact controls changed");
        const std::string historical="操作結果保留";
        const auto expectedStatus=state.Alive()?"生命 "+std::to_string(state.health)+"；上次操作："+historical:
            std::string("生命 0；移動、跳躍、互動與戰鬥已停止。");
        check(FieldPresentation::StatusLine(state,historical)==expectedStatus,"dedup actual player HP missing");
        ActionFeedbackToast toast;check(toast.Publish(ActionFeedbackEvent::Loaded),"dedup toast setup");
        const auto event=toast.Event();const auto remaining=toast.RemainingSeconds();
        Aevrynd::DrawList draw;
        const auto layout=FieldPresentation::MeasureLayout(draw,hud,state,historical,toast,started,paused,true,false,45);
        FieldPresentation::Draw(draw,hud,state,historical,toast,true,false,45,started,paused);
        check(draw.Stats2D().textGlyphs>0&&draw.Stats2D().textSourcePixels>0,"dedup actual CPU glyph draw missing");
        check(layout.showAudio==(!started||paused)&&layout.showJournal==journal&&layout.showToast==started,"dedup overlay visibility changed");
        for(const auto panel:std::array<FieldPresentation::PanelRect,4>{layout.top,layout.bottom,layout.journal,layout.toast})
            check(panel.left>=0&&panel.top>=0&&panel.Right()<=1280&&panel.Bottom()<=720,"dedup actual panel exceeds canvas");
        if(layout.showToast){check(layout.toast.Bottom()+4<=layout.bottom.top,"dedup toast covers bottom HUD");
            if(layout.showJournal)check(layout.toast.top>=layout.journal.Bottom()+8,"dedup toast covers journal");}
        check(Same(state,before)&&state.Serialize()==serialized&&sameEnemy(enemy,beforeEnemy)&&sameEvasion(evasion,beforeEvasion)&&
            sameGuide(guide,beforeGuide),"dedup presentation mutated gameplay state");
        check(toast.Event()==event&&toast.RemainingSeconds()==remaining,"dedup presentation changed toast clock");++localCases;
    };
    for(bool journal:{false,true}){
        inspect(supplies,true,true,journal,"角色與世界暫停更新。","補給 0 / 3；工具與木料 0 / 3",pauseControls);
        inspect(encounter,true,true,journal,"角色與世界暫停更新。","野豬生命 3 / 3",pauseControls);
        inspect(dead,true,true,journal,"角色與世界暫停更新。","補給 3 / 3；工具與木料 3 / 3；橋：已修復",pauseControls);
        inspect(complete,true,true,journal,"角色與世界暫停更新。","F5 保存；F9 恢復；R 開始新一輪。",pauseControls);
    }
    inspect(supplies,true,false,false,"E：拾取補給","補給 0 / 3；工具與木料 0 / 3",activeControls);
    inspect(encounter,true,false,false,"留意蓄力預警，避免直迎衝撞。","野豬生命 3 / 3；Q 可閃避",activeControls);
    inspect(supplies,false,false,false,"Enter 開始巡守；J 查看任務手冊。","補給 0 / 3；工具與木料 0 / 3",activeControls);
    inspect(complete,true,false,false,"巡守已完成；可調整鏡頭檢視，角色與任務不再前進。","F5 保存；F9 恢復；R 開始新一輪。",
        "左鍵拖曳環繞／滾輪縮放／←→ 旋轉 · F5 保存 · F9 恢復 · R 重開 · Esc 暫停 · J 手冊");
    check(localCases==12&&pausedCases==8&&controlCases==4,"dedup bounded case accounting");
    check(std::array<unsigned,4>{checks,negativeCases,inputCases,presentationCases}==originalCounters,"dedup changed original counters");
    std::cout<<"PAUSED_BOTTOM_SHORTCUT_HUD_CPU_PASS checks="<<localChecks<<" cases="<<localCases<<" paused="<<pausedCases
        <<" controls="<<controlCases<<" original_counters_unchanged=1 HP=preserved GPU=not-run human-play=not-run\n";
}

}
int main(){try{ContextualManualControlsPresentation();PausedShortcutDeduplicationPresentation();EarlyToolsRedRegression();EarlyToolsPresentation();InteractionFeedbackCases();Input();EarlyTimberPresentation();Presentation();MeasuredLayout();std::cout<<"FIELD_INPUT_PRESENTATION_CPU_PASS checks="<<checks<<" inputs="<<inputCases<<" presentations="<<presentationCases<<" negative="<<negativeCases<<" GPU=not-run human-play=not-run\n";CompletionControlsHud();PausedEncounterReadinessPresentation();return 0;}
catch(const std::exception& error){std::cerr<<"FIELD_UI_FIRST_DIVERGENCE "<<error.what()<<" checks="<<checks<<'\n';return 1;}}
