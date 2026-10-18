#pragma once
#include "MistwatchEncounterMotion.h"
#include <CharacterDynamicContact3D.h>
#include <array>
#include <bit>
#include <optional>

namespace Mistwatch::PairContact {
using Aevrynd::Vec3;
struct PlayerBox {Vec3 center{},extents{};};
enum class Status {Applied,Yielded,RecoveryRequired,YieldBlocked,UnresolvedInitialOverlap,InvalidInput,UnresolvedNumerics,PoseRejected};
struct State {std::uint64_t lastSerial{};};
struct LandingReservation {bool active{};PlayerBox box{};};
struct Segment {
    Vec3 from{},to{},envelopeCenter{},envelopeHalf{};
    unsigned phase{};
    bool valid{},clear{},committed{};
    Aevrynd::BoxCylinderContactKind3D sweep{},envelope{};
};
struct Audit {
    std::array<Segment,16> segments{};
    unsigned count{},direct{},correction{},candidates{},staticBlocked{},numericRejected{};
    bool axisEnclosed{};
};
struct Result {
    Status status{Status::InvalidInput};
    EncounterMotionResult motion;
    Audit audit;
    unsigned candidateTicks{},committedTicks{};
    bool inputConsumed{};
};
inline Aevrynd::DynamicCylinder3D BodyAt(Vec3 feet)noexcept {
    return {feet+Vec3{0,EncounterBodyBottom+EncounterBodyHeight*.5f,0},EncounterBodyRadius,EncounterBodyHeight*.5f};
}
inline std::optional<Aevrynd::DynamicCylinder3D> LiveBody(const Session& session,const Encounter& enemy)noexcept {
    // 舞台尚未進戰鬥時仍可見；同幀生命歸零即移除，不等待下一次舞台顯示。
    if(session.guardianDefeated||!enemy.health||enemy.phase==EnemyPhase::Defeated)return {};
    return BodyAt(enemy.feet);
}
inline bool ValidBox(PlayerBox b)noexcept {
    return Session::Finite(b.center)&&Session::Finite(b.extents)&&b.extents.x>0&&b.extents.y>0&&b.extents.z>0;
}
inline bool Same(Vec3 a,Vec3 b)noexcept {
    return std::bit_cast<std::uint32_t>(a.x)==std::bit_cast<std::uint32_t>(b.x)&&
        std::bit_cast<std::uint32_t>(a.y)==std::bit_cast<std::uint32_t>(b.y)&&std::bit_cast<std::uint32_t>(a.z)==std::bit_cast<std::uint32_t>(b.z);
}
inline bool Same(const Encounter& a,const Encounter& b)noexcept {
    return Same(a.feet,b.feet)&&Same(a.strikeTarget,b.strikeTarget)&&a.health==b.health&&a.phase==b.phase&&
        std::bit_cast<std::uint32_t>(a.phaseTime)==std::bit_cast<std::uint32_t>(b.phaseTime)&&
        std::bit_cast<std::uint32_t>(a.playerCooldown)==std::bit_cast<std::uint32_t>(b.playerCooldown)&&a.countered==b.countered;
}
// 本候選只保留當前水平腳印；不宣稱預知玩家下一幀改向的落點。
inline bool ReserveLanding(const Aevrynd::CharacterController3D& player,const Aevrynd::HeightField3D& terrain,LandingReservation& output)noexcept {
    LandingReservation next;
    if(player.Grounded()){output=next;return true;}
    const auto feet=player.Position(),extent=player.Extents();
    if(!terrain.Contains(feet.x-extent.x,feet.z-extent.z)||!terrain.Contains(feet.x+extent.x,feet.z+extent.z))return false;
    const auto ground=terrain.SampleHeight(feet.x,feet.z);
    if(!std::isfinite(ground)||feet.y<ground)return false;
    next.active=true;next.box={Vec3{feet.x,ground+extent.y,feet.z},extent};
    if(!ValidBox(next.box))return false;output=next;return true;
}
// 以 double 邊界建構包絡，再把 half extent 向外捨入；不能因 midpoint 少一 ULP 漏掉路段。
inline bool Envelope(Vec3 from,Vec3 to,Vec3& center,Vec3& half)noexcept {
    if(!Session::Finite(from)||!Session::Finite(to))return false;
    const auto axis=[](float a,float b,float& c,float& h){
        const double low=(std::min)(double(a),double(b)),high=(std::max)(double(a),double(b));
        c=float(low+(high-low)*.5);if(!std::isfinite(c))return false;
        const double required=(std::max)(double(c)-low,high-double(c));h=float(required);
        if(double(h)<required)h=std::nextafter(h,(std::numeric_limits<float>::infinity)());
        return std::isfinite(h)&&double(c)-h<=low&&double(c)+h>=high;
    };
    return axis(from.x,to.x,center.x,half.x)&&axis(from.y,to.y,center.y,half.y)&&axis(from.z,to.z,center.z,half.z);
}
inline bool Expand(Vec3 original,Vec3 half,Vec3& expanded)noexcept {
    const auto axis=[](float a,float b,float& c){const double exact=double(a)+b;c=float(exact);
        if(double(c)<exact)c=std::nextafter(c,(std::numeric_limits<float>::infinity)());return std::isfinite(c)&&c>0&&double(c)>=exact;};
    return axis(original.x,half.x,expanded.x)&&axis(original.y,half.y,expanded.y)&&axis(original.z,half.z,expanded.z);
}
inline bool CheckSegment(Vec3 from,Vec3 to,PlayerBox player,unsigned phase,Audit& audit,bool& numeric)noexcept {
    using namespace Aevrynd;
    if(audit.count==audit.segments.size()){numeric=true;return false;}
    auto& record=audit.segments[audit.count++];record.from=from;record.to=to;record.phase=phase;
    if(phase==0)++audit.direct;else ++audit.correction;
    const auto bodyFrom=BodyAt(from),bodyTo=BodyAt(to);
    BoxCylinderContact3D swept,envelope;
    if(!QueryBoxCylinderContact3D(player.center,player.extents,bodyFrom.center-bodyTo.center,bodyFrom,swept)||
        !Envelope(bodyFrom.center,bodyTo.center,record.envelopeCenter,record.envelopeHalf)){
        numeric=true;++audit.numericRejected;return false;
    }
    Vec3 expanded;if(!Expand(player.extents,record.envelopeHalf,expanded)||
        !QueryBoxCylinderContact3D(player.center,expanded,{},
            {record.envelopeCenter,EncounterBodyRadius,EncounterBodyHeight*.5f},envelope)){
        numeric=true;++audit.numericRejected;return false;
    }
    record.valid=true;record.sweep=swept.kind;record.envelope=envelope.kind;
    record.clear=swept.kind==BoxCylinderContactKind3D::Clear&&envelope.kind==BoxCylinderContactKind3D::Clear;
    return record.clear;
}
inline double LandingSeparation(PlayerBox landing,Vec3 enemy)noexcept {
    const double x=std::abs(double(landing.center.x)-enemy.x)-landing.extents.x;
    const double z=std::abs(double(landing.center.z)-enemy.z)-landing.extents.z;
    return std::hypot((std::max)(0.,x),(std::max)(0.,z))+(std::min)((std::max)(x,z),0.)-EncounterBodyRadius;
}
inline bool AxisEnclosed(Vec3 feet,std::span<const Aevrynd::BoxCollider3D> solids)noexcept {
    const double r=EncounterBodyRadius,bottom=double(feet.y)+EncounterBodyBottom,top=bottom+EncounterBodyHeight;
    bool left{},right{},back{},front{};
    for(const auto& b:solids){
        const double x0=double(b.center.x)-b.extents.x,x1=double(b.center.x)+b.extents.x;
        const double z0=double(b.center.z)-b.extents.z,z1=double(b.center.z)+b.extents.z;
        if(double(b.center.y)-b.extents.y>bottom||double(b.center.y)+b.extents.y<top)continue;
        if(z0<=double(feet.z)-r&&z1>=double(feet.z)+r){
            left|=x1==double(feet.x)-r;right|=x0==double(feet.x)+r;
        }
        if(x0<=double(feet.x)-r&&x1>=double(feet.x)+r){
            back|=z1==double(feet.z)-r;front|=z0==double(feet.z)+r;
        }
    }
    return left&&right&&back&&front;
}
class EncounterPlan {
public:
    bool Valid()const noexcept{return valid_;}
    const Encounter& Next()const noexcept{return next_;}
    Vec3 Delta()const noexcept{return delta_;}
    unsigned Damage()const noexcept{return damage_;}
    unsigned TickCalls()const noexcept{return ticks_;}
    bool Matches(const Encounter& current)const noexcept{return valid_&&Same(before_,current);}
private:
    Encounter before_{},next_{};Vec3 delta_{};unsigned damage_{},ticks_{};bool valid_{};
    friend EncounterPlan PreviewEncounter(const Encounter&,Vec3,float,bool,bool,std::span<const Aevrynd::BoxCollider3D>,const Aevrynd::HeightField3D&)noexcept;
};
inline EncounterPlan PreviewEncounter(const Encounter& enemy,Vec3 player,float dt,bool attack,bool active,
    std::span<const Aevrynd::BoxCollider3D> solids,const Aevrynd::HeightField3D& terrain)noexcept {
    using namespace Aevrynd;EncounterPlan plan;
    if(!enemy.Valid()||!Session::Finite(player)||!std::isfinite(dt)||dt<=0||dt>.1f||!ValidEncounterWorld(solids,terrain))return plan;
    bool malformed{},playerClear=true,strikeClear=true;
    const auto clear=[&](Vec3 target){const auto start=enemy.feet+Vec3{0,.65f,0},end=target+Vec3{0,.65f,0};
        for(const auto& box:solids)if(SegmentIntersectsAabb3D(start,end,box.center,box.extents))return false;
        const auto status=QueryHeightFieldLine3D(start,end,terrain);malformed|=status==HeightFieldLineStatus3D::InvalidInput;
        return status==HeightFieldLineStatus3D::Clear;};
    if(active){
        if(enemy.phase==EnemyPhase::Windup||std::hypot(player.x-enemy.feet.x,player.z-enemy.feet.z)<=2.5f)playerClear=clear(player);
        if(enemy.phase==EnemyPhase::Windup)strikeClear=clear(enemy.strikeTarget);
    }
    if(malformed)return plan;plan.before_=enemy;plan.next_=enemy;
    if(active){plan.damage_=plan.next_.Tick(player,dt,attack,playerClear,strikeClear);plan.ticks_=1;}
    plan.delta_={plan.next_.feet.x-enemy.feet.x,0,plan.next_.feet.z-enemy.feet.z};plan.valid_=true;return plan;
}
inline EncounterMotionResult CheckedMove(Vec3 from,Vec3 delta,PlayerBox player,const LandingReservation& reservation,
    bool forbidReservation,std::span<const Aevrynd::BoxCollider3D> solids,const Aevrynd::HeightField3D& terrain,Audit& audit,bool& numeric)noexcept {
    unsigned pending{};
    auto check=[&](Vec3 a,Vec3 b,unsigned phase){
        if(phase==2){for(unsigned i=pending;i<audit.count;++i)audit.segments[i].committed=true;pending=audit.count;return true;}
        if(!CheckSegment(a,b,player,phase,audit,numeric))return false;
        if(forbidReservation&&reservation.active){
            Audit reserveAudit;if(!CheckSegment(a,b,reservation.box,phase,reserveAudit,numeric))return false;
        }
        return true;
    };
    return ResolveEncounterMoveChecked(from,delta,solids,terrain,check);
}
// 純遊戲層、玩家先／NPC後；回呼只驗 NPC 真姿態。鏡頭、E、存檔的整幀交易仍由外層負責。
template<class AcceptPose>
Result AdvanceNpc(State& state,const Session& session,Encounter& enemy,const Aevrynd::CharacterController3D& player,
    float dt,bool attack,std::uint64_t serial,std::span<const Aevrynd::BoxCollider3D> solids,
    const Aevrynd::HeightField3D& terrain,AcceptPose&& acceptPose,bool enabled=true)noexcept {
    using namespace Aevrynd;Result result;result.motion.feet=enemy.feet;
    if(!serial||state.lastSerial==(std::numeric_limits<std::uint64_t>::max)()||serial!=state.lastSerial+1||
        !enemy.Valid()||!std::isfinite(dt)||dt<=0||dt>.1f||
        !session.Valid()||!Same(session.feet,player.Position())||!ValidBox({player.Center(),player.Extents()}))return result;
    const bool active=session.CurrentStage()==Stage::Encounter;
    if(!enabled||!LiveBody(session,enemy)){
        auto next=enemy;
        auto legacy=active?TickEncounterWorld(next,session.feet,dt,attack,solids,terrain):ResolveEncounterMove(enemy.feet,{},solids,terrain);
        result.candidateTicks=active?1u:0u;
        if(legacy.status==EncounterMotionStatus::Invalid)return result;
        try{if(!acceptPose(next)){result.status=Status::PoseRejected;return result;}}catch(...){result.status=Status::PoseRejected;return result;}
        enemy=next;state.lastSerial=serial;result.motion=legacy;result.committedTicks=result.candidateTicks;
        result.status=Status::Applied;result.inputConsumed=true;return result;
    }
    const auto plan=PreviewEncounter(enemy,session.feet,dt,attack,active,solids,terrain);
    if(!plan.Valid())return result;result.candidateTicks=plan.TickCalls();
    auto next=plan.Next();
    // 反擊在本次單一 Tick 擊倒後，不再保留活碰撞體。
    if(!LiveBody(session,next)){
        try{if(!acceptPose(next)){result.status=Status::PoseRejected;return result;}}catch(...){result.status=Status::PoseRejected;return result;}
        result.motion={EncounterMotionStatus::Applied,next.feet,0,plan.Damage()};enemy=next;state.lastSerial=serial;
        result.status=Status::Applied;result.inputConsumed=true;result.committedTicks=plan.TickCalls();return result;
    }
    BoxCylinderContact3D initial;
    if(!QueryBoxCylinderContact3D(player.Center(),player.Extents(),{},BodyAt(enemy.feet),initial)){
        result.status=Status::UnresolvedNumerics;return result;
    }
    if(initial.kind!=BoxCylinderContactKind3D::Clear){
        result.status=Status::UnresolvedInitialOverlap;return result;
    }
    LandingReservation reservation;if(!ReserveLanding(player,terrain,reservation)){result.status=Status::UnresolvedNumerics;return result;}
    const PlayerBox footprint{player.Center(),player.Extents()};bool numeric{};EncounterMotionResult movement;
    bool yielding=reservation.active&&LandingSeparation(reservation.box,enemy.feet)<0;
    if(yielding){
        constexpr float diagonal=.7071067811865475f;
        constexpr std::array<Vec3,8> directions{{{1,0,0},{-1,0,0},{0,0,1},{0,0,-1},
            {diagonal,0,diagonal},{-diagonal,0,diagonal},{diagonal,0,-diagonal},{-diagonal,0,-diagonal}}};
        const double before=LandingSeparation(reservation.box,enemy.feet);double best=before;bool found{};
        unsigned candidates{},blocked{};
        for(const auto& direction:directions){
            Audit candidateAudit;bool candidateNumeric{};++candidates;
            const auto candidate=CheckedMove(enemy.feet,direction*(2.f*dt),footprint,reservation,false,solids,terrain,candidateAudit,candidateNumeric);
            if(candidateNumeric){result.status=Status::UnresolvedNumerics;result.audit=candidateAudit;return result;}
            if(candidate.status==EncounterMotionStatus::Invalid)return result;
            const auto progress=LandingSeparation(reservation.box,candidate.feet);
            if(Same(candidate.feet,enemy.feet))++blocked;
            if(progress>best){best=progress;movement=candidate;result.audit=candidateAudit;found=true;}
        }
        result.audit.candidates=candidates;result.audit.staticBlocked=blocked;
        if(!found){
            result.audit.axisEnclosed=AxisEnclosed(enemy.feet,solids);
            result.status=result.audit.axisEnclosed?Status::RecoveryRequired:Status::YieldBlocked;return result;
        }
    }else{
        movement=CheckedMove(enemy.feet,plan.Delta(),footprint,reservation,true,solids,terrain,result.audit,numeric);
        if(numeric){result.status=Status::UnresolvedNumerics;return result;}
        if(movement.status==EncounterMotionStatus::Invalid)return result;
    }
    if(!plan.Matches(enemy))return result;
    next.feet=movement.feet;
    try{if(!acceptPose(next)){result.status=Status::PoseRejected;return result;}}catch(...){result.status=Status::PoseRejected;return result;}
    result.motion=movement;result.motion.damage=plan.Damage();enemy=next;state.lastSerial=serial;
    result.committedTicks=plan.TickCalls();result.status=yielding?Status::Yielded:Status::Applied;result.inputConsumed=true;return result;
}
}
