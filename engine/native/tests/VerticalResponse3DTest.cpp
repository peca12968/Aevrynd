// 全新候選 fixture；保留舊 Reject 測試，未執行前不宣稱通過。
#include "support/dynamic-contact/PreservedOptionalStepCases.h"
#include "support/dynamic-contact/mistwatch/MistwatchEncounterMotion.h"
#include <algorithm>
#include <cmath>
#include <optional>
#include <iomanip>

namespace VerticalResponse {
using namespace Aevrynd;
using Status=CharacterDynamicStepStatus3D;
using Mode=CharacterDynamicVerticalResponse3D;
constexpr float H=1.f/120.f;
unsigned positive{},negative{},emptyParity{},eventCases{},auxiliary{};
const std::span<const BoxCollider3D> NoBoxes{};
const std::span<const ConvexAabbSweep3D* const> NoHulls{};
void Need(bool condition,const char* text){if(!condition)throw std::runtime_error(text);}
bool Same(CharacterDynamicContactState3D a,CharacterDynamicContactState3D b){
    return a.blockedSide==b.blockedSide&&a.blockedAbove==b.blockedAbove&&a.blockedBelow==b.blockedBelow;
}
std::array<std::uint64_t,18> Words(const CharacterController3D& c){
    const auto m=c.CaptureMotionState();const auto s=c.Stats();
    const auto f=[](float v){return std::uint64_t(std::bit_cast<std::uint32_t>(v));};
    return {f(m.feet.x),f(m.feet.y),f(m.feet.z),f(m.velocity.x),f(m.velocity.y),f(m.velocity.z),f(m.height),f(m.yaw),
        m.grounded,s.steps,s.collisions,s.jumps,s.landings,s.terrainContacts,s.directTicks,s.directBlocked,s.directOverlaps,c.StateHash()};
}
HeightField3D Flat(){HeightField3D terrain({{-20,0,-20},81,81,.5f});terrain.Fill(0);return terrain;}
void Restore(CharacterController3D& c,Vec3 feet,Vec3 velocity,const HeightField3D& terrain,bool grounded=false){
    c.Reset(feet);Need(c.TryRestoreMotionState({feet,velocity,c.Settings().height,0,grounded},NoBoxes,NoHulls,terrain),"VERTICAL_FIXTURE_RESTORE_FAILED");
}
void Clear(const CharacterController3D& c,std::span<const DynamicCylinder3D> bodies){
    for(const auto& body:bodies){BoxCylinderContact3D q;
        Need(QueryBoxCylinderContact3D(c.Center(),c.Extents(),{},body,q)&&q.kind==BoxCylinderContactKind3D::Clear,
            "VERTICAL_COMMITTED_BODY_OVERLAP");
    }
}
DynamicCylinder3D Body(Vec3 feet){return {feet+Vec3{0,Mistwatch::EncounterBodyBottom+Mistwatch::EncounterBodyHeight*.5f,0},
    Mistwatch::EncounterBodyRadius,Mistwatch::EncounterBodyHeight*.5f};}
void EmptySpan(){
    auto terrain=Flat();
    for(unsigned recipe=0;recipe<4;++recipe){
        CharacterControllerSettings3D settings;
        if(recipe>=2){settings.maximumTerrainUphillGrade=.5f;settings.maximumTerrainStandingGrade=.5f;}
        CharacterController3D a(settings),b(settings);a.Reset({-3,0,-2});b.Reset({-3,0,-2});
        const std::array<BoxCollider3D,2> boxes{{{{0,1,0},{.1f,1,3}},{{-1,.05f,-1},{.4f,.05f,.4f}}}};
        for(unsigned tick=0;tick<96;++tick){
            const CharacterControl3D control{{tick<48?1.f:-.6f,0,tick%9<4?.6f:-.3f},tick==16||tick==64,tick%13==0?2.f:1.f};
            const float dt=tick%19==0?0.f:H;
            const bool aa=recipe%2?a.Step(control,boxes,NoHulls,terrain,dt):a.Step(control,boxes,NoHulls,dt);
            CharacterDynamicContactState3D report{true,true,true};
            const auto bb=recipe%2?b.Step(control,boxes,NoHulls,terrain,{},dt,Mode::BlockOnly,&report):
                b.Step(control,boxes,NoHulls,{},dt,Mode::BlockOnly,&report);
            Need(aa==(bb==Status::Applied)&&Words(a)==Words(b)&&
                a.MemoryStats().retainedCapacityBytes==b.MemoryStats().retainedCapacityBytes,"BLOCK_EMPTY_SPAN_PARITY_CHANGED");
            if(aa)Need(Same(report,{}),"EMPTY_SPAN_REPORT_NOT_EMPTY");
            ++emptyParity;
        }
    }
}
void LateFall(){
    auto terrain=Flat();CharacterController3D c;
    Restore(c,{0,1.31f,0},{0,-1,0},terrain);
    Vec3 enemy{};std::array<DynamicCylinder3D,1> bodies{Body(enemy)};
    CharacterDynamicContactState3D report;
    const auto before=c.Position();const auto old=c.Stats();
    Need(c.Step({},NoBoxes,NoHulls,terrain,bodies,2*H,Mode::BlockOnly,&report)==Status::Applied,"LATE_FALL_BLOCK_ONLY_REJECTED");
    Need(report.blockedBelow&&!report.blockedAbove&&c.Position().y<before.y&&c.Position().y>=1.3f&&
        c.Position().y<1.3003f&&c.Velocity().y==0&&!c.Grounded()&&c.Stats().landings==old.landings&&
        c.Stats().jumps==old.jumps&&c.Stats().steps==old.steps+1,"LATE_FALL_NOT_REAL_NORMAL_RESPONSE");
    Clear(c,bodies);++positive;
    // 使用真正 NPC 靜態／地形 resolver；不呼叫 AI Tick 或聲稱完整 pair/main 已驗收。
    unsigned accepted{};float maxNpcStep{};
    for(unsigned tick=0;tick<240;++tick){
        CharacterDynamicContactState3D contact;
        Need(c.Step({},NoBoxes,NoHulls,terrain,bodies,H,Mode::BlockOnly,&contact)==Status::Applied,"LATE_FALL_FUTURE_FIXED_STEP_REJECTED");
        ++accepted;Clear(c,bodies);
        if(enemy.x<2.f){
            const float requested=std::min(2.f*H,2.f-enemy.x);
            const auto move=Mistwatch::ResolveEncounterMove(enemy,{requested,0,0},NoBoxes,terrain);
            Need(move.status!=Mistwatch::EncounterMotionStatus::Invalid,"ACTUAL_NPC_GEOMETRY_FIXTURE_REJECTED");
            const auto next=Body(move.feet);BoxCylinderContact3D relative;
            Need(QueryBoxCylinderContact3D(c.Center(),c.Extents(),bodies[0].center-next.center,bodies[0],relative)&&
                relative.kind==BoxCylinderContactKind3D::Clear,"NPC_YIELD_CROSSED_COMMITTED_PLAYER");
            maxNpcStep=std::max(maxNpcStep,std::hypot(move.feet.x-enemy.x,move.feet.z-enemy.z));
            Need(move.feet.x==enemy.x+requested&&move.feet.z==enemy.z&&requested<=2.f*H,
                "NPC_YIELD_DID_NOT_USE_ORIGINAL_SPEED_AND_FLOAT_UPDATE");
            enemy=move.feet;bodies[0]=next;Clear(c,bodies);
        }
    }
    Need(accepted==240&&c.Grounded()&&c.Position().y==0&&c.Stats().landings==old.landings+1,
        "LATE_FALL_DID_NOT_REACH_REAL_TERRAIN_ONCE");++positive;
    std::cout<<"VERTICAL_LATE_FALL accepted="<<accepted<<" y="<<c.Position().y<<" grounded="<<c.Grounded()
        <<" added_landings="<<c.Stats().landings-old.landings<<" npc_x="<<enemy.x<<" max_npc_step="<<maxNpcStep<<'\n';
}
void HeadAndRelease(){
    auto terrain=Flat();CharacterController3D c;
    Restore(c,{0,.39f,0},{0,3,0},terrain);
    const std::array<DynamicCylinder3D,1> above{{{{0,3,0},1.3f,.8f}}};
    CharacterDynamicContactState3D head;
    Need(c.Step({{.2f,0,0}},NoBoxes,NoHulls,terrain,above,H,Mode::BlockOnly,&head)==Status::Applied,
        "HEAD_RESPONSE_REJECTED");
    const bool originalHeadAssert=head.blockedAbove&&!head.blockedBelow&&c.Position().x>0&&c.Position().y>.39f&&c.Position().y<=.4f&&
        c.Velocity().y==0&&!c.Grounded()&&c.Stats().landings==0;
    // 只讀診斷：依目前非 standingPolicy 的原碼公式推導子步；不是額外物理保證。
    const auto& settings=c.Settings();
    const double maxDistance=std::max({std::abs(double(.2f*settings.moveSpeed))*H,0.,double(settings.maximumFallSpeed)*H});
    const unsigned innerCount=unsigned(std::clamp(std::ceil(maxDistance/std::max(.05,double(settings.radius)*.5)),1.,64.));
    const float innerH=H/float(innerCount);
    std::cout<<std::setprecision(std::numeric_limits<float>::max_digits10)
        <<"HEAD_DIAGNOSTIC original_assert="<<originalHeadAssert<<" above="<<head.blockedAbove<<" below="<<head.blockedBelow
        <<" side="<<head.blockedSide<<" px="<<c.Position().x<<" py="<<c.Position().y<<" pz="<<c.Position().z
        <<" vx="<<c.Velocity().x<<" vy="<<c.Velocity().y<<" vz="<<c.Velocity().z<<" grounded="<<c.Grounded()
        <<" landings="<<c.Stats().landings<<" jumps="<<c.Stats().jumps<<" steps="<<c.Stats().steps
        <<" outer_h="<<H<<" inner_count_derived="<<innerCount<<" inner_h_derived="<<innerH
        <<" max_fall="<<settings.maximumFallSpeed<<" radius="<<settings.radius<<" gravity="<<settings.gravity<<'\n';
    Need(innerCount==2,"HEAD_INTERNAL_COUNT_FIXTURE_DRIFT");
    CharacterController3D prefix;Restore(prefix,{0,.39f,0},{0,3,0},terrain);
    for(unsigned piece=0;piece<innerCount;++piece){
        CharacterDynamicContactState3D event;
        const auto previous=prefix.Position();
        const auto status=prefix.Step({{.2f,0,0}},NoBoxes,NoHulls,terrain,above,innerH,Mode::BlockOnly,&event);
        Need(status==Status::Applied&&!event.blockedBelow&&!event.blockedSide&&!prefix.Grounded()&&
            prefix.Stats().landings==0&&prefix.Stats().jumps==0&&prefix.Velocity().x==.2f*settings.moveSpeed&&
            prefix.Position().x==previous.x+prefix.Velocity().x*innerH,"HEAD_SHORT_CALL_TANGENT_OR_SUPPORT_FAILED");
        if(piece==0)Need(event.blockedAbove&&prefix.Velocity().y==0&&prefix.Position().y>previous.y,
            "HEAD_FIRST_SHORT_CALL_DID_NOT_STOP_UPWARD_SPEED");
        else Need(!event.blockedAbove&&prefix.Velocity().y==-settings.gravity*innerH&&prefix.Position().y<previous.y,
            "HEAD_NEXT_SHORT_CALL_DID_NOT_RESUME_GRAVITY");
        std::cout<<"HEAD_SHORT_CALL_DIAGNOSTIC piece="<<piece<<" status="<<unsigned(status)<<" above="<<event.blockedAbove
            <<" below="<<event.blockedBelow<<" side="<<event.blockedSide<<" px="<<prefix.Position().x<<" py="<<prefix.Position().y
            <<" pz="<<prefix.Position().z<<" vx="<<prefix.Velocity().x<<" vy="<<prefix.Velocity().y<<" vz="<<prefix.Velocity().z
            <<" grounded="<<prefix.Grounded()<<" landings="<<prefix.Stats().landings<<" jumps="<<prefix.Stats().jumps
            <<" steps="<<prefix.Stats().steps<<" whole_state_equivalent_claim=0"<<'\n';
    }
    Need(head.blockedAbove&&!head.blockedBelow&&c.Position().x>0&&c.Position().y>.39f&&c.Position().y<=.4f&&
        c.Velocity().y<=0&&!head.blockedSide&&!c.Grounded()&&c.Stats().landings==0&&c.Stats().jumps==0&&
        c.Position().x==.2f*c.Settings().moveSpeed*H&&c.Position().z==0&&
        c.Velocity().x==.2f*c.Settings().moveSpeed&&c.Velocity().z==0,"HEAD_STOP_LOST_TANGENT_OR_FAKE_GROUND");
    Clear(c,above);const auto y=c.Position().y;
    Need(c.Step({{.2f,0,0}},NoBoxes,NoHulls,terrain,above,H,Mode::BlockOnly)==Status::Applied&&
        c.Position().y<y&&c.Velocity().y<0,"HEAD_NEXT_H_GRAVITY_MISSING");++positive;
    CharacterController3D edge;Restore(edge,{1.73f,1.3002f,0},{},terrain);
    const std::array<DynamicCylinder3D,1> below{Body({})};CharacterDynamicContactState3D cap;
    Need(edge.Step({{1,0,0}},NoBoxes,NoHulls,terrain,below,H,Mode::BlockOnly,&cap)==Status::Applied&&
        cap.blockedBelow&&edge.Position().x>1.75f&&!edge.Grounded(),"CAP_TANGENT_EXIT_NOT_ACCEPTED");
    Clear(edge,below);const auto capY=edge.Position().y;
    Need(edge.Step({{1,0,0}},NoBoxes,NoHulls,terrain,below,H,Mode::BlockOnly)==Status::Applied&&
        edge.Position().y<capY&&edge.Velocity().y<0&&!edge.Grounded(),"CAP_RELEASE_GRAVITY_LATER_THAN_ONE_H");++positive;
}
void EventSelection(){
    const std::array<DynamicCylinder3D,1> body{{{{0,.5f,0},.5f,.5f}}};
    for(unsigned order=0;order<3;++order){
        const float cy=order==0?2.f:std::nextafter(2.f,order==1?std::numeric_limits<float>::infinity():0.f);
        BodySweep hit;Status status=Status::Applied;
        Need(SweepBody({-1.5f,cy,0},{.5f,.5f,.5f},{1,-1,0},NoBoxes,NoHulls,hit,body,&status,Mode::BlockOnly),
            "RAW_SELECTED_EVENT_FAILED");
        // 首次體積進入取水平與垂直入界的較晚者；不以浮點容差合併一個 ULP 的先後。
        const double tX=.5,tY=double(cy)-1.5;
        std::cout<<std::setprecision(std::numeric_limits<double>::max_digits10)
            <<"EVENT_ORDER_DIAGNOSTIC order="<<order<<" cy="<<cy<<" tX="<<tX<<" tY="<<tY
            <<" side="<<hit.dynamicContact.blockedSide<<" below="<<hit.dynamicContact.blockedBelow
            <<" above="<<hit.dynamicContact.blockedAbove<<" fraction="<<hit.fraction<<" normals="<<hit.count<<'\n';
        Need((order==0&&tX==tY)||(order==1&&tY>tX)||(order==2&&tY<tX),
            "EVENT_FIXTURE_ANALYTIC_ORDER_DRIFT");
        Need(hit.dynamicContact.blockedSide==(order!=1)&&hit.dynamicContact.blockedBelow==(order!=2)&&
            !hit.dynamicContact.blockedAbove&&!hit.staticWalkable,"EXACT_OR_ONE_ULP_EVENT_NORMALS_WRONG");
        if(order==0){
            bool hasSide=false,hasBelow=false;
            for(std::size_t i=0;i<hit.count;++i){
                const auto n=hit.normals[i];
                hasSide=hasSide||(n.x==-1&&n.y==0&&n.z==0);
                hasBelow=hasBelow||(n.x==0&&n.y==1&&n.z==0);
            }
            Need(hit.count==2&&hasSide&&hasBelow&&hit.dynamicContact.blockedSide&&hit.dynamicContact.blockedBelow,
                "EXACT_SIMULTANEOUS_TWO_NORMALS_MISSING");
        }
        ++eventCases;
    }
    auto terrain=Flat();CharacterControllerSettings3D settings;
    settings.radius=.5f;settings.height=1;settings.moveSpeed=10;settings.gravity=0;
    for(bool reverse:{false,true}){
        CharacterController3D c(settings);Restore(c,{-1.5f,1.5f,0},{0,-10,0},terrain);
        const DynamicCylinder3D distant{{10,.5f,10},.5f,.5f};
        const std::array<DynamicCylinder3D,2> bodies=reverse?std::array<DynamicCylinder3D,2>{distant,body[0]}:
            std::array<DynamicCylinder3D,2>{body[0],distant};
        CharacterDynamicContactState3D report;
        Need(c.Step({{1,0,0}},NoBoxes,NoHulls,terrain,bodies,.1f,Mode::BlockOnly,&report)==Status::Applied&&
            report.blockedSide&&report.blockedBelow&&!c.Grounded()&&c.Velocity().x==0&&c.Velocity().y==0&&
            c.Position().x<=-1&&c.Position().y>=1,"SIMULTANEOUS_COMMON_CONSTRAINT_FAILED");
        Clear(c,bodies);++positive;
    }
}
void SideAndOrder(){
    auto terrain=Flat();const std::array<DynamicCylinder3D,1> body{Body({})};
    for(unsigned direction=0;direction<8;++direction){
        const float angle=float(direction)*.785398163f;
        const Vec3 radial{std::cos(angle),0,std::sin(angle)},tangent{-radial.z,0,radial.x};
        CharacterController3D c;Restore(c,radial*2.3f,{},terrain,true);
        CharacterDynamicContactState3D report;
        Need(c.Step({radial*-1.f+tangent*.3f},NoBoxes,NoHulls,terrain,body,.1f,Mode::BlockOnly,&report)==Status::Applied&&
            report.blockedSide&&!report.blockedAbove&&!report.blockedBelow&&c.Grounded()&&c.Position().y==0,
            "BLOCK_ONLY_CURVED_SIDE_OR_STATIC_GROUND_CHANGED");
        Clear(c,body);++positive;
    }
    const DynamicCylinder3D side{{0,2,0},1.32f,4},later{{0,.5f,0},1.3f,.8f};
    std::optional<std::array<std::uint64_t,18>> first;
    for(bool reverse:{false,true}){
        const std::array<DynamicCylinder3D,2> bodies=reverse?std::array<DynamicCylinder3D,2>{later,side}:
            std::array<DynamicCylinder3D,2>{side,later};
        CharacterController3D c;Restore(c,{-1.8f,1.3028f,0},{0,-1,0},terrain);
        CharacterDynamicContactState3D report;
        Need(c.Step({{1,0,0},false,4},NoBoxes,NoHulls,terrain,bodies,H,Mode::BlockOnly,&report)==Status::Applied&&
            report.blockedSide&&!report.blockedBelow&&c.Position().y<1.3028f,"LATER_VERTICAL_NOT_OCCLUDED_BY_FIRST_SIDE");
        Clear(c,bodies);if(first)Need(*first==Words(c),"MULTIBODY_SOURCE_ORDER_CHANGED_RESULT");else first=Words(c);++positive;
    }
}
void Failed(CharacterController3D& c,const HeightField3D& terrain,std::span<const DynamicCylinder3D> bodies,
    Status wanted,Mode mode=Mode::BlockOnly){
    CharacterDirectDisplacementPreviewToken3D token;
    Need(c.PreviewDirectDisplacementTick({1,{},0},NoBoxes,token),"NEGATIVE_LIVE_PREVIEW_FIXTURE_FAILED");
    const auto old=Words(c);const auto capacity=c.MemoryStats().retainedCapacityBytes;
    CharacterDynamicContactState3D report{true,true,true};const auto sentinel=report;
    Need(c.Step({},NoBoxes,NoHulls,terrain,bodies,H,mode,&report)==wanted&&Words(c)==old&&Same(report,sentinel)&&
        c.MemoryStats().retainedCapacityBytes==capacity&&c.CanCommitDirectDisplacementPreview(token),
        "VERTICAL_FAILURE_NOT_FULLY_ATOMIC");++negative;
}
void AtomicAndAux(){
    auto terrain=Flat();
    {
        CharacterController3D c;c.Reset({});const std::array<DynamicCylinder3D,1> overlap{Body({})};
        Failed(c,terrain,overlap,Status::UnresolvedStartedOverlap);
    }
    {
        CharacterController3D c;Restore(c,{-4,0,0},{},terrain);
        const std::array<DynamicCylinder3D,1> invalid{{{{0,.5f,0},0,.8f}}};Failed(c,terrain,invalid,Status::InvalidInput);
    }
    {
        CharacterController3D c;Restore(c,{-4,0,0},{},terrain);
        const std::array<DynamicCylinder3D,1> unresolved{{{{1.e9f,1,0},.0001f,.5f}}};
        Failed(c,terrain,unresolved,Status::UnresolvedNumerics);
    }
    {
        CharacterController3D c;Restore(c,{-4,0,0},{},terrain);Failed(c,terrain,{},Status::InvalidInput,static_cast<Mode>(255));
    }
    {
        CharacterControllerSettings3D settings;settings.height=1.4f;
        CharacterController3D c(settings);Restore(c,{},{},terrain,true);
        const std::array<DynamicCylinder3D,1> cap{{{{0,2.4f,0},1.3f,.8f}}};
        CharacterDirectDisplacementPreviewToken3D token;Need(c.PreviewDirectDisplacementTick({1,{},0},NoBoxes,token),"HEIGHT_PREVIEW_FIXTURE");
        const auto before=Words(c);
        Need(!c.TrySetHeight(1.8f,NoBoxes,NoHulls,cap)&&Words(c)==before&&c.CanCommitDirectDisplacementPreview(token),
            "HEIGHT_CAP_FAILURE_CHANGED_CROUCH");
        Need(c.Step({{.1f,0,0}},NoBoxes,NoHulls,terrain,cap,H,Mode::BlockOnly)==Status::Applied&&c.Settings().height==1.4f,
            "FAILED_STAND_CAUSED_WHOLE_FRAME_REJECTION");++auxiliary;
    }
    {
        CharacterController3D c;Restore(c,{},{},terrain);
        const std::array<DynamicCylinder3D,1> overlap{{{{0,1.8f,0},1,.4f}}};const auto before=Words(c);
        Need(!c.TrySetHeight(1,NoBoxes,NoHulls,overlap)&&Words(c)==before,"SHRINK_SILENTLY_DEPENETRATED");++auxiliary;
    }
    {
        CharacterController3D c;Restore(c,{0,1.3002f,0},{},terrain);const std::array<DynamicCylinder3D,1> cap{Body({})};
        const auto before=Words(c);
        Need(!c.TryLaunchFromGround(3,NoBoxes,NoHulls,terrain,cap)&&Words(c)==before,"DYNAMIC_CAP_GRANTED_JUMP");++auxiliary;
    }
    {
        CharacterController3D c;Restore(c,{},{},terrain,true);
        const std::array<DynamicCylinder3D,1> above{{{{0,2.61f,0},1.3f,.8f}}};
        const auto unlaunchedStats=c.Stats();
        Need(c.TryLaunchFromGround(4,NoBoxes,NoHulls,terrain,above),"REAL_GROUND_LAUNCH_PREMATURELY_REJECTED");
        CharacterDynamicContactState3D report;
        const auto launchBefore=c.CaptureMotionState();const auto launchStatsBefore=c.Stats();
        const auto launchStatus=c.Step({},NoBoxes,NoHulls,terrain,above,H,Mode::BlockOnly,&report);
        const auto launchAfter=c.CaptureMotionState();const auto launchStatsAfter=c.Stats();
        const auto& launchSettings=c.Settings();
        const double maxDistance=double(launchSettings.maximumFallSpeed)*H;
        const unsigned innerCount=unsigned(std::clamp(std::ceil(maxDistance/std::max(.05,double(launchSettings.radius)*.5)),1.,64.));
        const float innerH=H/float(innerCount);
        const bool originalLaunchAssert=launchStatus==Status::Applied&&report.blockedAbove&&c.Velocity().y==0&&!c.Grounded();
        // 舊終速斷言只作觀測；固定子步首段截停後，次段重力仍必須完整消耗。
        std::cout<<std::setprecision(std::numeric_limits<float>::max_digits10)
            <<"LAUNCH_DIAGNOSTIC original_assert="<<originalLaunchAssert<<" status="<<unsigned(launchStatus)
            <<" above="<<report.blockedAbove<<" below="<<report.blockedBelow<<" side="<<report.blockedSide
            <<" before_px="<<launchBefore.feet.x<<" before_py="<<launchBefore.feet.y<<" before_pz="<<launchBefore.feet.z
            <<" before_vx="<<launchBefore.velocity.x<<" before_vy="<<launchBefore.velocity.y<<" before_vz="<<launchBefore.velocity.z
            <<" before_grounded="<<launchBefore.grounded<<" after_px="<<launchAfter.feet.x
            <<" after_py="<<launchAfter.feet.y<<" after_pz="<<launchAfter.feet.z
            <<" after_vx="<<launchAfter.velocity.x<<" after_vy="<<launchAfter.velocity.y<<" after_vz="<<launchAfter.velocity.z
            <<" after_grounded="<<launchAfter.grounded<<" before_steps="<<launchStatsBefore.steps
            <<" after_steps="<<launchStatsAfter.steps<<" before_collisions="<<launchStatsBefore.collisions
            <<" after_collisions="<<launchStatsAfter.collisions<<" before_jumps="<<launchStatsBefore.jumps
            <<" after_jumps="<<launchStatsAfter.jumps<<" before_landings="<<launchStatsBefore.landings
            <<" after_landings="<<launchStatsAfter.landings<<" before_terrain="<<launchStatsBefore.terrainContacts
            <<" after_terrain="<<launchStatsAfter.terrainContacts<<" outer_h="<<H
            <<" inner_count_derived="<<innerCount<<" inner_h_derived="<<innerH
            <<" radius="<<launchSettings.radius<<" gravity="<<launchSettings.gravity
            <<" max_fall="<<launchSettings.maximumFallSpeed<<" body_bottom="<<above[0].center.y-above[0].halfHeight<<'\n';
        CharacterController3D prefix;Restore(prefix,{},{},terrain,true);
        const auto prefixBeforeLaunch=prefix.Stats();
        const bool prefixLaunch=prefix.TryLaunchFromGround(4,NoBoxes,NoHulls,terrain,above);
        std::cout<<"LAUNCH_SHORT_SETUP_DIAGNOSTIC launched="<<prefixLaunch<<" whole_state_equivalent_claim=0\n";
        Need(prefixLaunch&&innerCount==2,"LAUNCH_SHORT_CALL_SETUP_OR_DERIVED_COUNT_DRIFT");
        for(unsigned piece=0;piece<innerCount;++piece){
            const auto before=prefix.CaptureMotionState();const auto statsBefore=prefix.Stats();
            CharacterDynamicContactState3D event;
            const auto status=prefix.Step({},NoBoxes,NoHulls,terrain,above,innerH,Mode::BlockOnly,&event);
            Need(status==Status::Applied&&!event.blockedBelow&&!event.blockedSide&&!prefix.Grounded()&&
                prefix.Stats().jumps==prefixBeforeLaunch.jumps+1&&prefix.Stats().landings==prefixBeforeLaunch.landings&&
                prefix.Velocity().x==0&&prefix.Velocity().z==0&&prefix.Position().x==0&&prefix.Position().z==0,
                "LAUNCH_SHORT_CALL_FAKE_SUPPORT_OR_REPEAT_JUMP");
            if(piece==0)Need(event.blockedAbove&&prefix.Velocity().y==0&&prefix.Position().y>before.feet.y,
                "LAUNCH_FIRST_SHORT_CALL_DID_NOT_STOP_UPWARD_SPEED");
            else Need(!event.blockedAbove&&prefix.Velocity().y==-launchSettings.gravity*innerH&&prefix.Position().y<before.feet.y,
                "LAUNCH_NEXT_SHORT_CALL_DID_NOT_RESUME_GRAVITY");
            std::cout<<"LAUNCH_SHORT_CALL_DIAGNOSTIC piece="<<piece<<" status="<<unsigned(status)
                <<" above="<<event.blockedAbove<<" below="<<event.blockedBelow<<" side="<<event.blockedSide
                <<" before_px="<<before.feet.x<<" before_py="<<before.feet.y<<" before_pz="<<before.feet.z
                <<" before_vy="<<before.velocity.y<<" px="<<prefix.Position().x<<" py="<<prefix.Position().y
                <<" pz="<<prefix.Position().z<<" vx="<<prefix.Velocity().x<<" vy="<<prefix.Velocity().y<<" vz="<<prefix.Velocity().z
                <<" grounded="<<prefix.Grounded()<<" before_steps="<<statsBefore.steps<<" steps="<<prefix.Stats().steps
                <<" before_jumps="<<statsBefore.jumps<<" jumps="<<prefix.Stats().jumps
                <<" before_landings="<<statsBefore.landings<<" landings="<<prefix.Stats().landings
                <<" whole_state_equivalent_claim=0"<<'\n';
        }
        Need(launchStatus==Status::Applied&&report.blockedAbove&&!report.blockedBelow&&!report.blockedSide&&
            c.Velocity().y<=0&&!c.Grounded()&&launchStatsBefore.jumps==unlaunchedStats.jumps+1&&
            launchStatsAfter.jumps==launchStatsBefore.jumps&&launchStatsAfter.landings==unlaunchedStats.landings&&
            c.Position().y>0&&c.Position().y<=above[0].center.y-above[0].halfHeight-launchSettings.height&&
            c.Position().x==0&&c.Position().z==0&&c.Velocity().x==0&&c.Velocity().z==0,
            "REAL_LAUNCH_HEAD_BLOCK_NOT_APPLIED");
        Clear(c,above);++auxiliary;
    }
    for(bool terrainSnap:{false,true}){
        // 真實靜態上平台離緣；較低支撐可吸附，但中間先有一個非可站立動態 cap。
        auto floor=Flat();if(terrainSnap)floor.Fill(.9f);
        std::vector<BoxCollider3D> solids{{{-1.5f,.6f,0},{1.5f,.6f,10}}};
        if(!terrainSnap)solids.push_back({{0,-.05f,0},{10,.95f,10}});
        const Vec3 feet{.4499f,1.2f,0};
        CharacterControllerSettings3D snapSettings;
        snapSettings.maximumTerrainUphillGrade=1;snapSettings.maximumTerrainStandingGrade=1;
        CharacterController3D control(snapSettings),subject(snapSettings);
        for(auto* c:{&control,&subject}){
            c->Reset(feet);Need(c->TryRestoreMotionState({feet,{},1.8f,0,true},solids,NoHulls,floor),"SNAP_FIXTURE_ACTUAL_PLATFORM_SUPPORT_FAILED");
        }
        const std::array<DynamicCylinder3D,1> obstacle{{{{1,1,0},.5f,.1f}}};
        Need(control.Step({{1,0,0}},solids,NoHulls,floor,H)&&control.Grounded()&&control.Position().y<1,
            "ORIGINAL_STATIC_OR_TERRAIN_SNAP_NOT_ACTIVE");
        CharacterDynamicContactState3D report;
        Need(subject.Step({{1,0,0}},solids,NoHulls,floor,obstacle,H,Mode::BlockOnly,&report)==Status::Applied&&
            !subject.Grounded()&&subject.Position().y>1.19f&&!report.blockedBelow&&subject.Stats().landings==0,
            "OPTIONAL_SNAP_TUNNELLED_CAP_OR_GROUNDED_IT");
        Clear(subject,obstacle);++auxiliary;
    }
}
}
int RunVerticalResponseCases(){
    try{
        VerticalResponse::Need(RunPreservedOptionalStepCandidate()==0,"PRESERVED_REJECT_OR_OPTIONAL_STEP_CHANGED");
        VerticalResponse::EmptySpan();VerticalResponse::LateFall();VerticalResponse::HeadAndRelease();
        VerticalResponse::EventSelection();VerticalResponse::SideAndOrder();VerticalResponse::AtomicAndAux();
        std::cout<<"VERTICAL_BLOCK_ONLY_CANDIDATE_CPU_PASS positive="<<VerticalResponse::positive
            <<" atomic_negative="<<VerticalResponse::negative<<" explicit_empty_parity="<<VerticalResponse::emptyParity
            <<" exact_ulp_events="<<VerticalResponse::eventCases<<" auxiliary="<<VerticalResponse::auxiliary
            <<" support_ride=0 main=0 Explorer_dynamic_clearance=0 GPU=0\n";
        return 0;
    }catch(const std::exception& error){std::cerr<<"VERTICAL_BLOCK_FIRST_DIVERGENCE "<<error.what()<<'\n';return 1;}
}

#include <fstream>
#include <iterator>
#include <string>

// 固定預期資料是原控制器 1536 筆狀態逐位元組輸出，不含執行歷程或時間戳。
namespace {
std::string ReadDynamicContactFixtureBytes(const char* path) {
    std::ifstream in(path,std::ios::binary);
    if(!in)throw std::runtime_error("DYNAMIC_VERTICAL_PARITY_INPUT_UNAVAILABLE");
    std::string bytes((std::istreambuf_iterator<char>(in)),std::istreambuf_iterator<char>{});
    if(in.bad())throw std::runtime_error("DYNAMIC_VERTICAL_PARITY_INPUT_READ_FAILED");
    return bytes;
}
}
int main() {
    const int cases=RunVerticalResponseCases();
    if(cases!=0)return cases;
    try {
        const auto expected=ReadDynamicContactFixtureBytes("original-vertical1536.golden");
        const auto actual=ReadDynamicContactFixtureBytes("parity.tsv");
        if(expected.size()!=195129||actual!=expected)
            throw std::runtime_error("DYNAMIC_VERTICAL_ORIGINAL_1536_RAW_PARITY_CHANGED");
        std::cout<<"CHARACTER_DYNAMIC_VERTICAL_RESPONSE_CPU_PASS original_snapshots=1536 original_raw_bytes="
            <<actual.size()<<" original_raw_equal=1 explicit_empty_parity=384 main=0 ride=0 GPU=0\n";
        return 0;
    }catch(const std::exception& error) {
        std::cerr<<"DYNAMIC_VERTICAL_FORMAL_FIRST_DIVERGENCE "<<error.what()<<'\n';
        return 1;
    }
}
