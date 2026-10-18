#include "MistwatchCounterWindowCue.h"
#include <bit>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <utility>
#include <vector>

namespace {
using namespace Mistwatch;using namespace Aevrynd;namespace Cue=Mistwatch::CounterWindowCue;
unsigned checks{},cases{};
void Need(bool ok,const char* why){++checks;if(!ok)throw std::runtime_error(why);}
template<class Fn>void Case(const char* name,Fn body){try{body();++cases;}
    catch(const std::exception& e){std::cerr<<"COUNTER_WINDOW_CUE_FIRST_DIVERGENCE case="<<name<<" reason="<<e.what()<<'\n';throw;}}
bool Same(Vec3 a,Vec3 b){return a.x==b.x&&a.y==b.y&&a.z==b.z;}
bool Same(const Encounter& a,const Encounter& b){return Same(a.feet,b.feet)&&Same(a.strikeTarget,b.strikeTarget)&&a.health==b.health&&
    a.phase==b.phase&&a.phaseTime==b.phaseTime&&a.playerCooldown==b.playerCooldown&&a.countered==b.countered;}
bool Same(const ObjectiveGuide& a,const ObjectiveGuide& b){return a.valid==b.valid&&a.hasTarget==b.hasTarget&&a.useReady==b.useReady&&
    a.counterReady==b.counterReady&&a.evadeReady==b.evadeReady&&a.tuneReady==b.tuneReady&&a.earlyToolsReady==b.earlyToolsReady&&
    a.bridgeIntent==b.bridgeIntent&&a.index==b.index&&Same(a.target,b.target)&&a.distance==b.distance&&a.relativeRadians==b.relativeRadians&&
    a.bearing==b.bearing&&a.objective==b.objective&&a.navigation==b.navigation&&a.instruction==b.instruction&&a.context==b.context;}
std::vector<float> Heights(const HeightField3D& t){std::vector<float> h;const auto& s=t.Settings();
    for(unsigned z=0;z<s.depth;++z)for(unsigned x=0;x<s.width;++x)h.push_back(t.Height(x,z));return h;}
Session EncounterState(){Session s;for(unsigned i=0;i<3;++i){s.feet=s.supplies[i];Need(s.Collect(i),"SUPPLY_SETUP");}
    for(unsigned i=0;i<3;++i){s.feet=s.repairSites[i];Need(s.CollectRepairMaterial(i,i==0),"MATERIAL_SETUP");}
    s.feet=s.bridge;Need(s.RepairBridge(),"BRIDGE_SETUP");s.feet=s.arena+Vec3{0,0,2};return s;}
Camera3D Camera(){Camera3D c;c.position={0,4,42};c.target={0,.5f,48};return c;}
ObjectiveGuide Guide(const Session& s,const Encounter& enemy,const Evasion& evasion,const HeightField3D& terrain,
                     std::span<const BoxCollider3D> boxes={}){
    return BuildObjectiveGuide(s,enemy,evasion,Camera(),false,true,boxes,terrain);
}
void EmptySubmission(const Cue::Plan& plan,const HeightField3D& terrain,Cue::Status expected){
    Scene3D scene;scene.Line({-1,0,0},{1,0,0},1,{1,1,1,1});
    const auto receipt=Cue::Submit(scene,plan,terrain);
    Need(receipt.status==expected&&receipt.submitted==0,"INVALID_OR_HIDDEN_SUBMITTED_LINES");
    DrawList draw;const auto stats=scene.Submit(draw,Camera(),{1280,720});
    Need(stats.inputLines==1&&stats.visibleLines==1&&draw.worldVertices.size()==4&&draw.worldIndices.size()==6&&draw.vertices.empty(),
         "FAILED_CUE_LEFT_PARTIAL_WORLD_OR_2D_GEOMETRY");
}
}
int main(){try{
    HeightField3D terrain({{-50,0,-20},201,241,.5f});terrain.Fill(0);
    const auto session=EncounterState();Encounter enemy;enemy.phase=EnemyPhase::Recovery;enemy.phaseTime=1;
    const Evasion evasion;const auto guide=Guide(session,enemy,evasion,terrain);
    Need(session.Valid()&&enemy.Valid()&&guide.valid&&guide.counterReady,"TRUE_READY_FIXTURE");
    Case("ready-fixed-recipe-actual-world-submit-readonly",[&]{
        const auto beforeSession=session.Serialize();const auto beforeEnemy=enemy;const auto beforeHeights=Heights(terrain);
        const auto beforeSettings=terrain.Settings();const float beforeMin=terrain.MinimumHeight(),beforeMax=terrain.MaximumHeight();
        const auto beforeGuide=guide;const auto plan=Cue::Build(session,guide,enemy,terrain,true,false);
        Need(plan.status==Cue::Status::Ready&&plan.reason==Cue::Reason::None&&plan.count==16,"READY_BUDGET");
        Need(Cue::Radius==.95f&&Cue::Width==2.f&&Cue::GroundOffset==.06f,"FIXED_RECIPE_CHANGED");
        for(unsigned i=0;i<16;++i){const auto& line=plan.segments[i];
            for(const auto p:{line.a,line.b})Need(Session::Finite(p)&&terrain.Contains(p.x,p.z)&&
                p.y==terrain.SampleHeight(p.x,p.z)+.06f&&std::abs(std::hypot(p.x-enemy.feet.x,p.z-enemy.feet.z)-.95f)<.00001f,"ENDPOINT_RECIPE");
            if(i%4)Need(Same(plan.segments[i-1].b,line.a),"BRACKET_SEGMENTS_NOT_CONTIGUOUS");
        }
        Scene3D scene;const auto receipt=Cue::Submit(scene,plan,terrain);
        Need(receipt.status==Cue::Status::Ready&&receipt.submitted==16,"READY_NOT_SUBMITTED");
        DrawList draw;const auto stats=scene.Submit(draw,Camera(),{1280,720});
        Need(stats.inputLines==16&&stats.visibleLines==16&&draw.worldVertices.size()==64&&draw.worldIndices.size()==96,"ACTUAL_WORLD_LINE_GEOMETRY_BUDGET");
        Need(draw.vertices.empty()&&draw.meshInstances.empty()&&draw.skinnedMeshInstances.empty(),"CUE_USED_2D_OR_RESOURCE_MESH");
        for(const auto& v:draw.worldVertices)Need(std::isfinite(v.x)&&std::isfinite(v.y)&&std::isfinite(v.z)&&v.color==PackColorRgba8(Cue::Cyan),"NONFINITE_OR_NONCYAN_WORLD_VERTEX");
        for(const auto index:draw.worldIndices)Need(index<64,"WORLD_INDEX_OUT_OF_BOUNDS");
        Need(session.Serialize()==beforeSession&&Same(enemy,beforeEnemy)&&Heights(terrain)==beforeHeights&&Same(guide,beforeGuide)&&
             Same(terrain.Settings().origin,beforeSettings.origin)&&terrain.Settings().width==beforeSettings.width&&
             terrain.Settings().depth==beforeSettings.depth&&terrain.Settings().cellSize==beforeSettings.cellSize&&
             terrain.MinimumHeight()==beforeMin&&terrain.MaximumHeight()==beforeMax,"CUE_MUTATED_GAME_OR_GUIDE");
    });
    Case("playback-death-prestart-and-stage-gates",[&]{
        auto unusedGuide=guide;unusedGuide.valid=false;auto unusedEnemy=enemy;
        unusedEnemy.feet.x=(std::numeric_limits<float>::quiet_NaN)();
        EmptySubmission(Cue::Build(session,unusedGuide,unusedEnemy,terrain,true,true),terrain,Cue::Status::Hidden);
        for(const auto flags:{std::pair{false,false},std::pair{true,true},std::pair{false,true}})
            EmptySubmission(Cue::Build(session,guide,enemy,terrain,flags.first,flags.second),terrain,Cue::Status::Hidden);
        auto dead=session;dead.Damage(100);EmptySubmission(Cue::Build(dead,Guide(dead,enemy,evasion,terrain),enemy,terrain,true,false),terrain,Cue::Status::Hidden);
        Session supplies;EmptySubmission(Cue::Build(supplies,Guide(supplies,enemy,evasion,terrain),enemy,terrain,true,false),terrain,Cue::Status::Hidden);
        auto after=session;after.feet=enemy.feet;Need(after.FinishEncounter(),"POST_ENCOUNTER_SETUP");
        EmptySubmission(Cue::Build(after,Guide(after,enemy,evasion,terrain),enemy,terrain,true,false),terrain,Cue::Status::Hidden);
        after.feet=Session::rangerLog;Need(after.ReadFuseClue(),"COMPLETE_CLUE_SETUP");after.feet=Session::fuse;
        for(unsigned i=0;i<3;++i)for(unsigned n=0;n<Session::fuseCode[i];++n)Need(after.CycleFuseDial(i),"COMPLETE_DIAL_SETUP");
        Need(after.CollectFuse(),"COMPLETE_FUSE_SETUP");after.feet=Session::generator;
        Need(after.ToggleCircuit(0)&&after.ToggleCircuit(2)&&after.RepairGenerator(),"COMPLETE_GENERATOR_SETUP");
        for(unsigned i=0;i<3;++i){after.feet=Session::relaySites[i];
            if(i==0)Need(after.CycleRelayPatch(0)&&after.ConfirmRelayCircuit(),"COMPLETE_WEST_SETUP");
            if(i==2)Need(after.CycleExtractionRoute()&&after.ConfirmExtractionRoute(),"COMPLETE_NORTHWEST_SETUP");
            for(unsigned n=0;n<Session::RelayTargetSector(i);++n)Need(after.TuneRelay(i,1),"COMPLETE_TUNE_SETUP");
            Need(after.ConnectRelay(i),"COMPLETE_RELAY_SETUP");}
        after.feet=Session::beacon;Need(after.LightBeacon()&&after.Valid(),"COMPLETE_BEACON_SETUP");
        EmptySubmission(Cue::Build(after,Guide(after,enemy,evasion,terrain),enemy,terrain,true,false),terrain,Cue::Status::Hidden);
    });
    Case("real-counter-window-range-cooldown-countered-evade",[&]{
        for(unsigned variant=0;variant<7;++variant){auto s=session;auto e=enemy;auto evade=evasion;
            if(variant==0)s.feet.z=e.feet.z+2.501f;
            if(variant==1)e.playerCooldown=.2f;
            if(variant==2){e.health=2;e.countered=true;}
            if(variant==3){evade.direction={1,0,0};evade.remaining=.1f;evade.cooldown=.5f;}
            if(variant==4){e.phase=EnemyPhase::Windup;e.phaseTime=.5f;}
            if(variant==5){e.phase=EnemyPhase::Pursuit;e.phaseTime=0;}
            if(variant==6){e.phase=EnemyPhase::Defeated;e.phaseTime=0;e.health=0;e.countered=true;}
            const auto g=Guide(s,e,evade,terrain);Need(g.valid&&!g.counterReady,"REAL_NONREADY_GUIDE");
            EmptySubmission(Cue::Build(s,g,e,terrain,true,false),terrain,Cue::Status::Hidden);
        }
        auto edge=session;edge.feet.z=enemy.feet.z+2.5f;const auto exact=Guide(edge,enemy,evasion,terrain);
        Need(exact.valid&&exact.counterReady&&Cue::Build(edge,exact,enemy,terrain,true,false).count==16,"EXACT_RANGE_BOUNDARY");
        edge.feet.y=enemy.feet.y+2.f;const auto high=Guide(edge,enemy,evasion,terrain);
        Need(high.valid&&!high.counterReady,"EXACT_VERTICAL_BOUNDARY");EmptySubmission(Cue::Build(edge,high,enemy,terrain,true,false),terrain,Cue::Status::Hidden);
    });
    Case("real-guide-wall-and-terrain-line-of-sight",[&]{
        const std::array wall{BoxCollider3D{{0,.65f,49},{.5f,1,.1f}}};
        const auto blocked=Guide(session,enemy,evasion,terrain,wall);Need(blocked.valid&&!blocked.counterReady,"WALL_FIXTURE");
        EmptySubmission(Cue::Build(session,blocked,enemy,terrain,true,false),terrain,Cue::Status::Hidden);
        auto ridge=terrain;Need(ridge.SetHeight(100,138,3),"RIDGE_FIXTURE");
        const auto ridgeGuide=Guide(session,enemy,evasion,ridge);Need(ridgeGuide.valid&&!ridgeGuide.counterReady,"TERRAIN_LOS_FIXTURE");
        EmptySubmission(Cue::Build(session,ridgeGuide,enemy,ridge,true,false),ridge,Cue::Status::Hidden);
    });
    Case("true-F-counters-once-and-closes-cue",[&]{
        auto hit=enemy;Need(hit.Tick(session.feet,1.f/60.f,true)==0&&hit.health==enemy.health-1&&hit.countered,"REAL_F_DID_NOT_COUNTER");
        const auto used=Guide(session,hit,evasion,terrain);Need(used.valid&&!used.counterReady,"COUNTERED_GUIDE_READY");
        EmptySubmission(Cue::Build(session,used,hit,terrain,true,false),terrain,Cue::Status::Hidden);
        const auto hp=hit.health;hit.Tick(session.feet,1.f/60.f,true);Need(hit.health==hp,"SECOND_F_SAME_RECOVERY");
    });
    Case("actual-nonflat-terrain-and-map-boundary",[&]{
        auto slope=terrain;for(unsigned z=0;z<241;++z)for(unsigned x=0;x<201;++x)
            Need(slope.SetHeight(x,z,float(x)*.0002f),"SLOPE_FIXTURE");
        auto s=session;auto e=enemy;s.feet.y=slope.SampleHeight(s.feet.x,s.feet.z);e.feet.y=slope.SampleHeight(e.feet.x,e.feet.z);
        const auto g=Guide(s,e,evasion,slope);Need(g.valid&&g.counterReady,"SLOPE_READY_FIXTURE");
        const auto plan=Cue::Build(s,g,e,slope,true,false);Need(plan.status==Cue::Status::Ready&&plan.count==16,"SLOPE_PLAN");
        for(const auto& line:plan.segments)for(const auto p:{line.a,line.b})Need(p.y==slope.SampleHeight(p.x,p.z)+.06f,"CALLER_TERRAIN_IGNORED");
        HeightField3D boundary({{-1,0,47},3,7,.5f});boundary.Fill(0);
        auto b=session;b.feet={-.5f,0,48};const auto bg=Guide(b,enemy,evasion,boundary);
        Need(bg.valid&&bg.counterReady,"EDGE_READY_FIXTURE");
        const auto rejected=Cue::Build(b,bg,enemy,boundary,true,false);
        Need(rejected.status==Cue::Status::Invalid&&rejected.reason==Cue::Reason::Endpoint&&rejected.count==0,"PARTIAL_EDGE_PLAN_NOT_REJECTED");
        EmptySubmission(rejected,boundary,Cue::Status::Invalid);
    });
    Case("invalid-input-and-final-endpoint-atomic-rejection",[&]{
        const float nan=(std::numeric_limits<float>::quiet_NaN)();auto bad=terrain;bad.Fill(nan);
        auto plan=Cue::Build(session,guide,enemy,bad,true,false);Need(plan.status==Cue::Status::Invalid&&plan.count==0,"NAN_TERRAIN");
        EmptySubmission(plan,bad,Cue::Status::Invalid);
        auto badGuide=guide;badGuide.valid=false;Need(Cue::Build(session,badGuide,enemy,terrain,true,false).status==Cue::Status::Invalid,"INVALID_GUIDE");
        auto badEnemy=enemy;badEnemy.feet.x=nan;Need(Cue::Build(session,guide,badEnemy,terrain,true,false).status==Cue::Status::Invalid,"INVALID_ENEMY");
        auto badSession=session;badSession.feet.z=nan;Need(Cue::Build(badSession,guide,enemy,terrain,true,false).status==Cue::Status::Invalid,"INVALID_SESSION");
        const auto good=Cue::Build(session,guide,enemy,terrain,true,false);
        for(unsigned variant=0;variant<5;++variant){plan=good;
            if(variant==0)plan.segments[15].b.x=nan;
            if(variant==1)plan.segments[15].b.x=1000;
            if(variant==2)plan.segments[15].b.y+=1;
            if(variant==3)plan.count=17;
            if(variant==4)plan.segments[15].b=plan.segments[15].a;
            EmptySubmission(plan,terrain,Cue::Status::Invalid);
        }
    });
    std::cout<<"COUNTER_WINDOW_CUE_CPU_PASS cases="<<cases<<" checks="<<checks<<" lines=16 world_vertices=64 world_indices=96 GPU=false\n";return 0;
}catch(const std::exception&){return 1;}}
