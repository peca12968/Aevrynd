#include "MistwatchObjectivePointer.h"
#include <bit>
#include <iostream>
#include <stdexcept>

namespace Trial {
using namespace Aevrynd;using namespace Mistwatch;namespace Pointer=Mistwatch::ObjectivePointer;
unsigned checks{},cases{},groups{};
void Need(bool value,const char* reason){++checks;if(!value)throw std::runtime_error(reason);}
bool Same(float a,float b){return std::bit_cast<std::uint32_t>(a)==std::bit_cast<std::uint32_t>(b);}
bool Same(Vec3 a,Vec3 b){return Same(a.x,b.x)&&Same(a.y,b.y)&&Same(a.z,b.z);}
bool SameCamera(const Camera3D& a,const Camera3D& b){return Same(a.position,b.position)&&Same(a.target,b.target)&&Same(a.up,b.up)&&
    a.projection.mode==b.projection.mode&&Same(a.projection.verticalFieldOfView,b.projection.verticalFieldOfView)&&
    Same(a.projection.orthographicHeight,b.projection.orthographicHeight)&&Same(a.projection.nearPlane,b.projection.nearPlane)&&Same(a.projection.farPlane,b.projection.farPlane);}
bool SameGuide(const ObjectiveGuide& a,const ObjectiveGuide& b){return a.valid==b.valid&&a.hasTarget==b.hasTarget&&a.useReady==b.useReady&&
    a.counterReady==b.counterReady&&a.evadeReady==b.evadeReady&&a.tuneReady==b.tuneReady&&a.earlyToolsReady==b.earlyToolsReady&&
    a.bridgeIntent==b.bridgeIntent&&a.index==b.index&&a.navigationRelayIndex==b.navigationRelayIndex&&a.relayTrackingActive==b.relayTrackingActive&&
    Same(a.target,b.target)&&a.distance==b.distance&&a.relativeRadians==b.relativeRadians&&a.bearing==b.bearing&&a.objective==b.objective&&
    a.navigation==b.navigation&&a.instruction==b.instruction&&a.context==b.context;}
Camera3D Camera(const Session& session,double desiredRelative){
    const auto target=Session::supplies[0];const double heading=std::atan2(double(target.x)-session.feet.x,double(target.z)-session.feet.z);
    const double yaw=desiredRelative-heading;Camera3D camera;camera.target=session.feet+Vec3{0,1.2f,0};
    camera.position=camera.target+Vec3{float(std::sin(yaw)*6),2,float(-std::cos(yaw)*6)};return camera;
}
Session AtStage(Stage wanted){Session session;
    if(wanted==Stage::Supplies)return session;
    for(unsigned i=0;i<3;++i){session.feet=Session::supplies[i];Need(session.Collect(i),"STAGE_SUPPLY_SETUP");}
    if(wanted==Stage::RepairMaterials)return session;
    for(unsigned i=0;i<3;++i){session.feet=Session::repairSites[i];Need(session.CollectRepairMaterial(i,true),"STAGE_MATERIAL_SETUP");}
    if(wanted==Stage::Bridge)return session;
    session.feet=Session::bridge;Need(session.RepairBridge(),"STAGE_BRIDGE_SETUP");if(wanted==Stage::Encounter)return session;
    session.feet=Session::arena;Need(session.FinishEncounter(),"STAGE_ENCOUNTER_SETUP");if(wanted==Stage::Fuse)return session;
    session.feet=Session::rangerLog;Need(session.ReadFuseClue(),"STAGE_LOG_SETUP");session.feet=Session::fuse;
    for(unsigned i=0;i<3;++i)for(unsigned j=0;j<Session::fuseCode[i];++j)Need(session.CycleFuseDial(i),"STAGE_DIAL_SETUP");
    Need(session.CollectFuse(),"STAGE_FUSE_SETUP");if(wanted==Stage::Generator)return session;
    session.feet=Session::generator;Need(session.ToggleCircuit(0)&&session.ToggleCircuit(2)&&session.RepairGenerator(),"STAGE_POWER_SETUP");
    if(wanted==Stage::Relays)return session;
    for(unsigned i=0;i<3;++i){session.feet=Session::relaySites[i];
        if(i==0)Need(session.CycleRelayPatch(0)&&session.ConfirmRelayCircuit(),"STAGE_WEST_SETUP");
        if(i==2)Need(session.CycleExtractionRoute()&&session.ConfirmExtractionRoute(),"STAGE_ROUTE_SETUP");
        for(unsigned n=0;n<Session::RelayTargetSector(i);++n)Need(session.TuneRelay(i,1),"STAGE_AIM_SETUP");
        Need(session.ConnectRelay(i),"STAGE_LINK_SETUP");}
    if(wanted==Stage::Beacon)return session;
    session.feet=Session::beacon;Need(session.LightBeacon(),"STAGE_BEACON_SETUP");return session;
}
ObjectiveGuide Guide(const Session& session,const Camera3D& camera,const HeightField3D& terrain,unsigned preferred=NoTrackedRelay){
    return BuildObjectiveGuide(session,Encounter{},Evasion{},camera,false,true,{},terrain,preferred);
}
void No3D(const DrawList& draw){Need(draw.worldVertices.empty()&&draw.worldIndices.empty()&&draw.meshInstances.empty()&&
    draw.skinnedMeshInstances.empty()&&draw.skinPaletteJoints.empty()&&draw.PointLights().empty()&&draw.LocalReflectionProbes().empty()&&
    draw.StaticMeshSubmissionRevision3D()==0,"POINTER_CREATED_WORLD_OR_RESOURCE_SUBMISSION");}
void Submitted(const Pointer::Plan& plan){DrawList draw(DrawListCapacityProfile::Minimal2D);const auto receipt=Pointer::Submit(draw,plan);
    Need(receipt.submitted&&receipt.status==plan.status&&receipt.lines==plan.count*2&&receipt.lines<=8&&receipt.quads==1,"POINTER_SUBMISSION_BUDGET");
    Need(draw.vertices.size()==6+12*plan.count&&draw.Stats2D().submittedTriangles==2+4*plan.count&&draw.Stats2D().textGlyphs==0,"POINTER_TRUE_DRAW_GEOMETRY_COUNT");
    unsigned background{},outline{},mint{};
    for(const auto& vertex:draw.vertices){Need(std::isfinite(vertex.x)&&std::isfinite(vertex.y)&&vertex.x>=616&&vertex.x<=664&&vertex.y>=80&&vertex.y<=108,"POINTER_STROKE_ESCAPED_PANEL");
        if(vertex.color==PackColorRgba8(Pointer::Background))++background;
        else if(vertex.color==PackColorRgba8(Pointer::Outline))++outline;
        else if(vertex.color==PackColorRgba8(Pointer::Mint))++mint;
        else Need(false,"POINTER_UNEXPECTED_COLOR");}
    Need(background==6&&outline==6*plan.count&&mint==6*plan.count,"POINTER_TWO_STROKE_COLOR_COUNTS");No3D(draw);
}
void TrueAngles(){const auto terrain=CreateTerrain();Session session;
    constexpr std::array<double,16> angles{0,.2,-.2,std::numbers::pi/4,-std::numbers::pi/4,std::numbers::pi/2,-std::numbers::pi/2,
        3*std::numbers::pi/4,-3*std::numbers::pi/4,std::numbers::pi,-std::numbers::pi,2.7,-2.7,1.1,-1.1,.66};
    for(double angle:angles){++cases;const auto camera=Camera(session,angle);const auto guide=Guide(session,camera,terrain);
        Need(guide.valid&&guide.hasTarget&&guide.distance>.001,"TRUE_GUIDE_ANGLE_SETUP");const auto plan=Pointer::Build(session,guide,true,false,false);
        Need(plan.status==Pointer::Status::Arrow&&plan.count==3,"TRUE_GUIDE_ARROW_NOT_BUILT");
        const auto tip=plan.segments[0].b;
        Need(std::abs(double(tip.x-640)/9-std::sin(guide.relativeRadians))<.00001&&
            std::abs(double(tip.y-94)/9+std::cos(guide.relativeRadians))<.00001,"TRUE_CAMERA_RELATIVE_DIRECTION_WRONG");
        Submitted(plan);
    }++groups;
}
void Continuity(){const auto terrain=CreateTerrain();Session session;
    for(double boundary:std::array<double,5>{-3*std::numbers::pi/4,-std::numbers::pi/4,std::numbers::pi/4,3*std::numbers::pi/4,std::numbers::pi}){
        ++cases;const auto a=Guide(session,Camera(session,boundary-.0001),terrain),b=Guide(session,Camera(session,boundary+.0001),terrain);
        const auto p=Pointer::Build(session,a,true,false,false),q=Pointer::Build(session,b,true,false,false);
        Need(p.status==Pointer::Status::Arrow&&q.status==Pointer::Status::Arrow,"BOUNDARY_GUIDE_INVALID");
        for(unsigned i=0;i<3;++i)for(const auto pair:std::array<std::pair<Vec2,Vec2>,2>{{{p.segments[i].a,q.segments[i].a},{p.segments[i].b,q.segments[i].b}}})
            Need(std::hypot(pair.first.x-pair.second.x,pair.first.y-pair.second.y)<.01f,"POINTER_DISCONTINUOUS_AT_BEARING_BOUNDARY");
    }++groups;
}
void StagesAndVisibility(){const auto terrain=CreateTerrain();
    for(Stage stage:std::array{Stage::Supplies,Stage::RepairMaterials,Stage::Bridge,Stage::Encounter,Stage::Fuse,Stage::Generator,Stage::Relays,Stage::Beacon,Stage::Complete}){
        auto session=AtStage(stage);Need(session.Valid()&&session.CurrentStage()==stage,"TRUE_STAGE_FIXTURE_INVALID");const auto camera=Camera(session,0);const auto guide=Guide(session,camera,terrain);
        for(unsigned flags=0;flags<16;++flags){++cases;auto current=session;const bool started=(flags&1)!=0,paused=(flags&2)!=0,journal=(flags&4)!=0;
            if(flags&8)current.Damage(100);const auto actualGuide=Guide(current,camera,terrain);const auto plan=Pointer::Build(current,actualGuide,started,paused,journal);
            const bool visible=started&&!paused&&!journal&&current.Alive()&&stage!=Stage::Encounter&&stage!=Stage::Complete;
            Need(visible?(plan.status==Pointer::Status::Arrow||plan.status==Pointer::Status::AtTarget):plan.status==Pointer::Status::Hidden,"STAGE_VISIBILITY_GATE_CHANGED");
            if(!visible){DrawList draw(DrawListCapacityProfile::Minimal2D);Need(!Pointer::Submit(draw,plan).submitted&&draw.vertices.empty(),"HIDDEN_POINTER_SUBMITTED");No3D(draw);}
        }
        Need(guide.valid,"STAGE_TRUE_GUIDE_INVALID");
    }++groups;
}
void AtTarget(){const auto terrain=CreateTerrain();Session session;session.feet=Session::supplies[0];const auto camera=Camera(session,0);
    const auto guide=Guide(session,camera,terrain);Need(guide.valid&&guide.distance==0&&guide.useReady,"TRUE_AT_TARGET_SETUP");
    const auto plan=Pointer::Build(session,guide,true,false,false);Need(plan.status==Pointer::Status::AtTarget&&plan.count==4,"AT_TARGET_IS_FALSE_FORWARD_ARROW");
    Submitted(plan);++cases;++groups;
}
bool NearInteractionSame(const ObjectiveGuide& a,const ObjectiveGuide& b){return a.index==b.index&&a.useReady==b.useReady&&a.tuneReady==b.tuneReady&&
    a.counterReady==b.counterReady&&a.evadeReady==b.evadeReady&&a.earlyToolsReady==b.earlyToolsReady&&a.bridgeIntent==b.bridgeIntent&&a.instruction==b.instruction&&a.context==b.context;}
void RelaySelection(){const auto terrain=CreateTerrain();const auto base=AtStage(Stage::Relays);
    for(unsigned local=0;local<3;++local)for(unsigned preferred=0;preferred<3;++preferred)if(local!=preferred){++cases;auto session=base;
        const auto site=Session::relaySites[local];session.feet={site.x+(local==1?-1.5f:1.5f),0,site.z};session.feet.y=GroundElevation(session.feet.x,session.feet.z);
        Need(session.NearRelay(local),"RELAY_TRUE_NEAR_SETUP");const auto saved=session.Serialize();const auto camera=Camera(session,.4);
        const auto nearest=Guide(session,camera,terrain),selected=Guide(session,camera,terrain,preferred);
        Need(nearest.index==local&&NearInteractionSame(nearest,selected)&&selected.relayTrackingActive&&Same(selected.target,Session::relaySites[preferred]),"TRACKED_RELAY_CHANGED_LOCAL_TARGET");
        const auto p=Pointer::Build(session,selected,true,false,false);Need(p.status==Pointer::Status::Arrow,"TRACKED_RELAY_POINTER_MISSING");Submitted(p);
        for(unsigned lever=0;lever<3;++lever){auto a=session,b=session;const auto x=FieldInput::Lever(a,lever),y=FieldInput::Lever(b,lever);
            Need(x==y&&a.Serialize()==b.Serialize(),"POINTER_CHANGED_TRUE_LOCAL_LEVER");}
        auto a=session,b=session;const auto x=FieldInput::Interact(a),y=FieldInput::Interact(b);
        Need(x==y&&a.Serialize()==b.Serialize()&&session.Serialize()==saved,"POINTER_CHANGED_TRUE_LOCAL_E");
    }++groups;
}
std::uint64_t Vertices(const DrawList& draw){std::uint64_t hash=14695981039346656037ull;
    for(const auto& vertex:draw.vertices)for(auto word:{std::bit_cast<std::uint32_t>(vertex.x),std::bit_cast<std::uint32_t>(vertex.y),vertex.color})hash=(hash^word)*1099511628211ull;return hash;}
void Reject(const Pointer::Plan& plan){DrawList draw(DrawListCapacityProfile::Minimal2D);draw.Quad({20,20},{4,4},{1,0,1,1});
    const auto hash=Vertices(draw),size=draw.vertices.size();const auto stats=draw.Stats2D();const auto receipt=Pointer::Submit(draw,plan);
    const auto after=draw.Stats2D();Need(!receipt.submitted&&receipt.status==Pointer::Status::Invalid&&receipt.lines==0&&receipt.quads==0&&
        Vertices(draw)==hash&&draw.vertices.size()==size&&after.inputTriangles==stats.inputTriangles&&after.submittedTriangles==stats.submittedTriangles&&
        after.primitiveCullTests==stats.primitiveCullTests&&after.primitiveColorPacks==stats.primitiveColorPacks,"INVALID_PLAN_PARTIAL_DRAW");No3D(draw);++cases;}
void InvalidAndAtomic(){const auto terrain=CreateTerrain();Session session;const auto guide=Guide(session,Camera(session,0),terrain);
    for(unsigned i=0;i<13;++i){auto bad=guide;
        switch(i){case 0:bad.valid=false;break;case 1:bad.hasTarget=false;break;case 2:bad.target.x=(std::numeric_limits<float>::quiet_NaN)();break;
        case 3:bad.target.z=(std::numeric_limits<float>::infinity)();break;case 4:bad.distance=(std::numeric_limits<double>::quiet_NaN)();break;
        case 5:bad.distance=(std::numeric_limits<double>::infinity)();break;case 6:bad.distance=-.001;break;
        case 7:bad.relativeRadians=(std::numeric_limits<double>::quiet_NaN)();break;case 8:bad.relativeRadians=(std::numeric_limits<double>::infinity)();break;
        case 9:bad.relativeRadians=std::numbers::pi+.001;break;case 10:bad.relativeRadians=-std::numbers::pi-.001;break;
        case 11:bad.target.x=10001;break;case 12:bad.relayTrackingActive=true;bad.navigationRelayIndex=3;break;}
        const auto plan=Pointer::Build(session,bad,true,false,false);Need(plan.status==Pointer::Status::Invalid,"INVALID_GUIDE_NOT_REJECTED");Reject(plan);
        Need(Pointer::Build(session,bad,true,true,false).status==Pointer::Status::Hidden,"UNUSED_PAUSED_GUIDE_NEW_FAILURE");
    }
    const auto original=Pointer::Build(session,guide,true,false,false);
    for(unsigned i=0;i<9;++i){auto bad=original;
        switch(i){case 0:bad.count=5;break;case 1:bad.count=0;break;case 2:bad.status=static_cast<Pointer::Status>(999);break;
        case 3:bad.segments[0].a.x=(std::numeric_limits<float>::quiet_NaN)();break;
        case 4:bad.segments[2].b.y=(std::numeric_limits<float>::infinity)();break;
        case 5:bad.segments[2].b.y=107;break;case 6:bad.segments[2].b=bad.segments[2].a;break;
        case 7:bad.status=Pointer::Status::Hidden;break;case 8:bad.status=Pointer::Status::AtTarget;break;}
        Reject(bad);
    }
    auto invalidSession=session;invalidSession.health=101;Reject(Pointer::Build(invalidSession,guide,true,false,false));++groups;
}
bool Overlap(const FieldPresentation::PanelRect& a,const FieldPresentation::PanelRect& b){return a.left<b.Right()&&b.left<a.Right()&&a.top<b.Bottom()&&b.top<a.Bottom();}
void RealLayout(){const auto terrain=CreateTerrain();DrawList draw(DrawListCapacityProfile::Minimal2D);
    for(Stage stage:std::array{Stage::Supplies,Stage::RepairMaterials,Stage::Bridge,Stage::Encounter,Stage::Fuse,Stage::Generator,Stage::Relays,Stage::Beacon,Stage::Complete}){
        const auto session=AtStage(stage);const auto guide=Guide(session,Camera(session,.7),terrain);
        for(unsigned flags=0;flags<8;++flags){++cases;const bool started=(flags&1)!=0,paused=(flags&2)!=0,journal=(flags&4)!=0;
            const auto hud=FieldPresentation::Build(session,guide,Encounter{},started,paused,journal);ActionFeedbackToast toast;
            Need(toast.Publish(ActionFeedbackEvent::Saved),"REAL_LAYOUT_TOAST_SETUP");
            const auto layout=FieldPresentation::MeasureLayout(draw,hud,session,"已儲存",toast,started,paused,false,false,80);
            const auto plan=Pointer::Build(session,guide,started,paused,journal);
            Need(layout.top.Bottom()==76&&layout.bottom.top==576,"EXISTING_LAYOUT_BOUNDARY_CHANGED");
            if(plan.status==Pointer::Status::Arrow||plan.status==Pointer::Status::AtTarget){
                Need(!Overlap(Pointer::Panel,layout.top)&&!Overlap(Pointer::Panel,layout.bottom)&&
                    (!layout.showToast||(!Overlap(Pointer::Panel,layout.toast)&&layout.toast.top>=113))&&
                    (!layout.showJournal||!Overlap(Pointer::Panel,layout.journal)),"POINTER_OVERLAPPED_MEASURED_HUD");
            }
            if(layout.showJournal)Need(plan.status==Pointer::Status::Hidden,"JOURNAL_POINTER_NOT_HIDDEN");
        }
    }++groups;
}
std::uint64_t TerrainHash(const HeightField3D& terrain){std::uint64_t hash=14695981039346656037ull;const auto& s=terrain.Settings();
    const auto word=[&](std::uint32_t value){hash=(hash^value)*1099511628211ull;};
    word(s.width);word(s.depth);word(std::bit_cast<std::uint32_t>(s.cellSize));
    for(float v:{s.origin.x,s.origin.y,s.origin.z})word(std::bit_cast<std::uint32_t>(v));
    for(unsigned z=0;z<s.depth;++z)for(unsigned x=0;x<s.width;++x)word(std::bit_cast<std::uint32_t>(terrain.Height(x,z)));return hash;}
void ReadOnly(){auto session=AtStage(Stage::Fuse);auto terrain=CreateTerrain();auto camera=Camera(session,.3);auto guide=Guide(session,camera,terrain);
    const auto saved=session.Serialize();const auto cameraBefore=camera;const auto guideBefore=guide;const auto heights=TerrainHash(terrain);
    const auto plan=Pointer::Build(session,guide,true,false,false);Submitted(plan);
    Need(saved==session.Serialize()&&SameCamera(camera,cameraBefore)&&SameGuide(guide,guideBefore)&&TerrainHash(terrain)==heights,"POINTER_MUTATED_SOURCE_STATE");
    Need(guide.target.x==Session::rangerLog.x&&guide.target.z==Session::rangerLog.z&&guide.context.find("2 - 0 - 3")==std::string::npos,
        "UNREAD_LOG_GUIDE_LEAKED_CABINET");++cases;++groups;
}
}
int main(){try{using namespace Trial;TrueAngles();Continuity();StagesAndVisibility();AtTarget();RelaySelection();InvalidAndAtomic();RealLayout();ReadOnly();
    Need(groups==8&&cases==268,"OBJECTIVE_POINTER_FINITE_CASE_COUNT");
    std::cout<<"OBJECTIVE_POINTER_CPU_PASS groups="<<groups<<" cases="<<cases<<" checks="<<checks<<" max_lines=8 GPU=false OnStart=false\n";return 0;
}catch(const std::exception& error){std::cerr<<"OBJECTIVE_POINTER_FIRST_DIVERGENCE="<<error.what()<<'\n';return 1;}}
