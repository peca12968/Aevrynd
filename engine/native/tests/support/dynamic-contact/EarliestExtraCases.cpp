#include <Aevrynd/CharacterController3D.h>
#include <array>
#include <bit>
#include <iostream>
#include <stdexcept>
namespace {
using namespace Aevrynd;
using S=CharacterDynamicStepStatus3D;
const std::span<const BoxCollider3D> EmptyBoxes{};
const std::span<const ConvexAabbSweep3D* const> EmptyHulls{};
void Check(bool value,const char* reason){if(!value)throw std::runtime_error(reason);}
std::array<std::uint64_t,18> State(const CharacterController3D& c){
    const auto m=c.CaptureMotionState();const auto s=c.Stats();
    const auto bits=[](float value){return std::uint64_t(std::bit_cast<std::uint32_t>(value));};
    return {bits(m.feet.x),bits(m.feet.y),bits(m.feet.z),bits(m.velocity.x),bits(m.velocity.y),bits(m.velocity.z),
        bits(m.height),bits(m.yaw),m.grounded,s.steps,s.collisions,s.jumps,s.landings,s.terrainContacts,
        s.directTicks,s.directBlocked,s.directOverlaps,c.StateHash()};
}
HeightField3D Flat(){HeightField3D t({{-16,0,-16},33,33,1});t.Fill(0);return t;}
void Restore(CharacterController3D& c,Vec3 feet,float fall,const HeightField3D& terrain){
    c.Reset(feet);Check(c.TryRestoreMotionState({feet,{0,fall,0},c.Settings().height,0,false},EmptyBoxes,EmptyHulls,terrain),
        "EARLIEST_FIXTURE_RESTORE_FAILED");
}
void Clear(const CharacterController3D& c,std::span<const DynamicCylinder3D> bodies){
    for(const auto& b:bodies){BoxCylinderContact3D q;Check(QueryBoxCylinderContact3D(c.Center(),c.Extents(),{},b,q)&&
        q.kind==BoxCylinderContactKind3D::Clear,"EARLIEST_FINAL_OVERLAP");}
}
void Rejected(CharacterController3D& c,std::span<const BoxCollider3D> boxes,const HeightField3D& terrain,
    std::span<const DynamicCylinder3D> bodies,float dt,const char* reason){
    CharacterDirectDisplacementPreviewToken3D token;
    Check(c.PreviewDirectDisplacementTick({1,{0,0,0},0},EmptyBoxes,token),"EARLIEST_PREVIEW_FAILED");
    const auto before=State(c);const auto status=c.Step({{1,0,0},false,c.Settings().moveSpeed==6?4.f:1.f},boxes,EmptyHulls,terrain,bodies,dt);
    if(status!=S::UnresolvedVerticalEntry)std::cerr<<reason<<" actual="<<unsigned(status)<<'\n';
    Check(status==S::UnresolvedVerticalEntry,reason);
    Check(State(c)==before&&c.CanCommitDirectDisplacementPreview(token),"EARLIEST_UNRESOLVED_NOT_ATOMIC");
}
}
void RunEarliestExtraCases(){
    const auto terrain=Flat();unsigned safe{},unresolved{};
    // 先遇到另一動態側面時，兩種來源順序都必須忽略被截斷路徑之外的頂面。
    for(bool reverse:{false,true}){
        const DynamicCylinder3D side{{0,2,0},1.32f,4},later{{0,.5f,0},1.3f,.8f};
        const std::array<DynamicCylinder3D,2> bodies=reverse?std::array<DynamicCylinder3D,2>{later,side}:
            std::array<DynamicCylinder3D,2>{side,later};
        CharacterController3D c;Restore(c,{-1.8f,1.3028f,0},-1,terrain);
        Check(c.Step({{1,0,0},false,4},EmptyBoxes,EmptyHulls,terrain,bodies,1.f/120.f)==S::Applied,"EARLIER_DYNAMIC_SIDE_DID_NOT_OCCLUDE");
        Check(c.Position().x<=-1.77f&&c.Position().x>-1.771f&&c.Position().y<1.3f&&!c.Grounded(),"EARLIER_DYNAMIC_SIDE_STOP_DESCENT_WRONG");
        Clear(c,bodies);++safe;
    }
    {
        // 早牆之後沿剩餘下落路徑真的會進頂面，必須重新查詢並回滾，不可只忽略第一次預測。
        CharacterController3D c;Restore(c,{-1.8f,1.3028f,0},-1,terrain);
        const std::array<BoxCollider3D,1> wall{{{{-1.31f,2,0},{.01f,2,10}}}};
        const std::array<DynamicCylinder3D,1> body{{{{-.1f,.5f,0},1.3f,.8f}}};
        Rejected(c,wall,terrain,body,1.f/120.f,"SLIDE_REMAINDER_VERTICAL_WAS_IGNORED");++unresolved;
    }
    {
        // 二進位可精確表示的幾何／速度讓靜態側面和動態頂面同時進入；不可按遍歷順序放行。
        CharacterControllerSettings3D s;s.radius=.5f;s.height=1;s.moveSpeed=16;s.gravity=0;s.maximumFallSpeed=16;
        CharacterController3D c(s);Restore(c,{-1.5f,1.5f,0},-16,terrain);
        const std::array<BoxCollider3D,1> wall{{{{0,2,0},{.5f,2,10}}}};
        const std::array<DynamicCylinder3D,1> body{{{{-1,.5f,0},1,.5f}}};
        Rejected(c,wall,terrain,body,.0625f,"STATIC_TIE_VERTICAL_WAS_NOT_CONSERVATIVE");++unresolved;
    }
    for(bool reverse:{false,true}){
        CharacterControllerSettings3D s;s.radius=.5f;s.height=1;s.moveSpeed=16;s.gravity=0;s.maximumFallSpeed=16;
        CharacterController3D c(s);Restore(c,{-1.5f,1.5f,0},-16,terrain);
        const DynamicCylinder3D side{{0,2,0},.5f,4},vertical{{-1,.5f,0},1,.5f};
        const std::array<DynamicCylinder3D,2> body=reverse?std::array<DynamicCylinder3D,2>{vertical,side}:
            std::array<DynamicCylinder3D,2>{side,vertical};
        Rejected(c,EmptyBoxes,terrain,body,.0625f,"DYNAMIC_TIE_VERTICAL_WAS_NOT_CONSERVATIVE");++unresolved;
    }
    {
        CharacterControllerSettings3D s;s.radius=1;s.height=1;s.moveSpeed=16;s.gravity=0;s.maximumFallSpeed=16;
        CharacterController3D c(s);Restore(c,{-1.875f,1.375f,0},-16,terrain);
        const std::array<BoxCollider3D,1> wall{{{{0,2,0},{.75f,2,10}}}};
        const std::array<DynamicCylinder3D,1> body{{{{0,.5f,0},.5f,.5f}}};
        BoxCylinderContact3D full;
        Check(QueryBoxCylinderContact3D(c.Center(),c.Extents(),{.5f,-.5f,0},body[0],full)&&
            full.kind==BoxCylinderContactKind3D::SimultaneousEntry&&full.first==.75,"OCCLUDED_SIMULTANEOUS_PRECONDITION_WRONG");
        Check(c.Step({{1,0,0}},wall,EmptyHulls,terrain,body,.0625f)==S::Applied,"LATER_SIMULTANEOUS_BEHIND_WALL_NOT_OCCLUDED");
        Check(c.Position().x<=-1.75f&&c.Position().x>-1.751f&&c.Position().y<1&&!c.Grounded(),"LATER_SIMULTANEOUS_SAFE_DESCENT_WRONG");
        Clear(c,body);++safe;
    }
    Check(safe==3&&unresolved==4,"EARLIEST_EXTRA_COVERAGE_WRONG");
    std::cout<<"EARLIEST_EVENT_EXTRA_CPU_PASS safe_occluded="<<safe<<" actual_or_tied_unresolved="<<unresolved
        <<" source_order_permutations=4 atomic_pending_preview_preserved=4\n";
}
