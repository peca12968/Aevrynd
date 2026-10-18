// 只借用正式真 Player fixture 與交易雜湊；不執行其原 main，也不改寫腳點。
#define main BrakingBoundaryOriginalHandoffMain
#include "MistwatchDashJumpHandoffTest.cpp"
#undef main

namespace {
unsigned boundaryRoutes{},shortPlans{},falseRollbacks{},throwRollbacks{},retryMatches{},zeroControls{},defaultControls{},revalidateControls{};
bool SamePointBits(Vec3 a,Vec3 b){return std::bit_cast<std::uint32_t>(a.x)==std::bit_cast<std::uint32_t>(b.x)&&
    std::bit_cast<std::uint32_t>(a.y)==std::bit_cast<std::uint32_t>(b.y)&&std::bit_cast<std::uint32_t>(a.z)==std::bit_cast<std::uint32_t>(b.z);}
bool NewPlan(const Explorer& before,const Explorer& after,unsigned side){
    const auto& a=before.footSwings[side];const auto& b=after.footSwings[side];
    return b.Active()&&b.Elapsed()==0&&(!a.Active()||a.Elapsed()!=0||!SamePointBits(a.Landing(),b.Landing())||a.Duration()!=b.Duration());
}
struct BrakingWitness {bool found{};unsigned side{};float speed{},oldLead{},firstLead{};double duration{},distance{};Vec3 landing{},oldLanding{};};
BrakingWitness FindShortPlan(const Explorer& entry,const Fixture& f){
    const auto& e=f.explorer;const float speed=std::hypot(e.locomotionVelocity.x,e.locomotionVelocity.z);
    if(!f.actor.Grounded()||f.actor.Settings().height<1.5f||!entry.gaitGrounded||
       e.locomotionDistance<=.0001f||e.locomotionSeconds<=0||!std::isfinite(speed)||speed<=.0001f)return {};
    const auto direction=e.locomotionVelocity*(1.f/speed);
    if(std::abs(std::remainder(std::atan2(direction.x,direction.z)-e.yaw,6.2831853f))>.26179939f)return {};
    const auto feet=f.actor.Position();
    const auto world=CreateTransformMatrix(feet,{0,e.yaw,0},{1,1,1});
    const auto inverse=CreateRotationMatrix({0,-e.yaw,0})*CreateTranslationMatrix(feet*-1.f);
    for(unsigned side=0;side<2;++side)if(NewPlan(entry,e,side)){
        const auto& curve=e.footSwings[side];
        const auto other=TransformPoint(inverse,e.footSwings[1-side].Landing());
        const float lateral=side?(std::max)(.12f,other.x+.12f):(std::min)(-.12f,other.x-.12f);
        const auto rest=TransformPoint(world,{lateral,.1f,0});
        const float oldLead=(std::max)(.4f,float(double(speed)*curve.Duration()));
        const double distance=double(speed)*double(speed)/(2*LocomotionMotor::Deceleration);
        if(!std::isfinite(oldLead)||!std::isfinite(distance)||distance<0||distance>=double(oldLead))continue;
        const float firstLead=float(distance);const auto shorter=rest+direction*firstLead,old=rest+direction*oldLead;
        const auto landing=curve.Landing();
        if(std::bit_cast<std::uint32_t>(landing.x)==std::bit_cast<std::uint32_t>(shorter.x)&&
           std::bit_cast<std::uint32_t>(landing.z)==std::bit_cast<std::uint32_t>(shorter.z)&&
           (std::bit_cast<std::uint32_t>(landing.x)!=std::bit_cast<std::uint32_t>(old.x)||
            std::bit_cast<std::uint32_t>(landing.z)!=std::bit_cast<std::uint32_t>(old.z)))
            return {true,side,speed,oldLead,firstLead,curve.Duration(),distance,landing,old};
    }
    return {};
}
void FootGates(const Fixture& f,const Explorer& before){
    const auto& e=f.explorer;Need(f.actor.Grounded()&&e.gaitGrounded,"BRAKE_BOUNDARY_TRUE_GROUND_REQUIRED");
    const auto world=CreateTransformMatrix(f.actor.Position(),{0,e.yaw,0},{1,1,1});
    const float blend=std::clamp(std::hypot(e.locomotionVelocity.x,e.locomotionVelocity.z)/2.f,0.f,1.f);
    const float movingLower=.09f*blend*blend*(3-2*blend);
    Need(e.pose.localTransforms[0].translation.y>=.9f-movingLower-.35f-.000001f,"BRAKE_PELVIS_GATE_UNCHANGED");
    for(unsigned side=0;side<2;++side){
        const unsigned upper=9+side*3;const auto hip=TransformPoint(world,TransformPoint(e.pose.globalMatrices[upper],{}));
        const auto knee=TransformPoint(world,TransformPoint(e.pose.globalMatrices[upper+1],{}));
        const auto ankle=TransformPoint(world,TransformPoint(e.pose.globalMatrices[upper+2],{}));
        Need(std::abs(Length(knee-hip)-.4f)<.0001f&&std::abs(Length(ankle-knee)-.38f)<.0001f,"BRAKE_NO_STRETCHED_BONES");
        Need(Length(ankle-e.footTargets[side])<.005f,"BRAKE_TRUE_ANKLE_TARGET_GATE");
        Need(e.footTargets[side].y>=f.terrain.SampleHeight(e.footTargets[side].x,e.footTargets[side].z)+.1f-.0005f,
            "BRAKE_TARGET_CLEARANCE_GATE");
        Need(Length(e.footTargets[side]-before.footTargets[side])<60*LocomotionMotor::FixedStep+.0001f&&
            e.footSwings[side].SpeedBound()<=60,"BRAKE_FOOT_SPEED_GATE");
        Need(!e.soleOffsets[side].empty(),"BRAKE_TRUE_SOLE_CONTOUR_REQUIRED");
        for(const auto& sole:e.soleOffsets[side]){const auto p=TransformPoint(world,TransformPoint(e.pose.globalMatrices[upper+2],sole));
            Need(Finite(p)&&p.y>=f.terrain.SampleHeight(p.x,p.z)-.005f,"BRAKE_ACTUAL_SOLE_CLEARANCE");}
    }
}
std::uint64_t TemporalHash(const Explorer& e){
    std::uint64_t hash{};const auto add=[&](std::uint64_t value){hash=(hash^value)*1099511628211ull;};
    for(float value:{e.phase,e.yaw,e.requestedYaw,e.locomotionDistance,e.locomotionSeconds,
        e.locomotionVelocity.x,e.locomotionVelocity.y,e.locomotionVelocity.z})add(std::bit_cast<std::uint32_t>(value));
    for(const auto& curve:e.footSwings){add(curve.Ready());add(curve.Active());add(curve.Replanned());
        for(double value:{curve.Elapsed(),curve.Duration(),curve.LiftHeight(),curve.SpeedBound()})add(std::bit_cast<std::uint64_t>(value));
        for(const auto p:{curve.Start(),curve.Landing()})for(float value:{p.x,p.y,p.z})add(std::bit_cast<std::uint32_t>(value));}
    return hash;
}
void DefaultAndRevalidate(const Explorer& entry,const Fixture& accepted,Vec3 previous){
    auto omitted=entry,explicitFalse=entry;const auto delta=accepted.actor.Position()-previous;
    omitted.Update(delta,LocomotionMotor::FixedStep,false);explicitFalse.Update(delta,LocomotionMotor::FixedStep,false);
    const bool a=omitted.PlaceFeet(accepted.actor.Position(),true,false,accepted.terrain,{},{});
    const bool b=explicitFalse.PlaceFeet(accepted.actor.Position(),true,false,accepted.terrain,{}, {},nullptr,nullptr,ExplorerFootPlacementMode::Advance,false);
    Need(a&&b&&PoseHash(omitted)==PoseHash(explicitFalse),"DEFAULT_FALSE_EQUALS_EXPLICIT_FALSE");++defaultControls;
    auto sameFalse=accepted.explorer,sameTrue=accepted.explorer;
    sameFalse.RebuildLocomotionPose(false);sameTrue.RebuildLocomotionPose(false);const auto temporal=TemporalHash(sameFalse);
    const bool c=sameFalse.PlaceFeet(accepted.actor.Position(),true,false,accepted.terrain,{}, {},nullptr,nullptr,ExplorerFootPlacementMode::RevalidateCurrent,false);
    const bool d=sameTrue.PlaceFeet(accepted.actor.Position(),true,false,accepted.terrain,{}, {},nullptr,nullptr,ExplorerFootPlacementMode::RevalidateCurrent,true);
    Need(c&&d&&PoseHash(sameFalse)==PoseHash(sameTrue)&&TemporalHash(sameFalse)==temporal&&TemporalHash(sameTrue)==temporal,
        "REVALIDATE_HINT_EXCLUDED_WITHOUT_TIME_ADVANCE");++revalidateControls;
}
void ZeroTime(Fixture& f){
    CharacterDirectDisplacementPreviewToken3D token;
    Need(f.actor.PreviewDirectDisplacementTick({f.actor.DirectLastTick()+1,{},0},std::span<const BoxCollider3D>{},f.terrain,token),"BRAKE_ZERO_PREVIEW_SETUP");
    const auto before=f.State();const auto result=f.Advance({},0,[]{return true;});
    Need(result.accepted&&result.steps==0&&f.State()==before&&f.actor.CanCommitDirectDisplacementPreview(token),"BRAKE_ZERO_DT_STATE_AND_PREVIEW");
    Need(f.actor.CancelDirectDisplacementPreview(token),"BRAKE_ZERO_PREVIEW_CANCEL");++zeroControls;
}
PlayerLocomotionInput Prelude(unsigned tick){
    PlayerLocomotionInput input;
    if(tick<90)input.movement={0,0,1};
    else if(tick<270){input.movement={0,0,1};input.speedMultiplier=1.5f;input.running=true;}
    else if(tick<390)input.movement={0,0,-1};return input;
}
void NaturalBoundary(unsigned resume){
    Fixture f(120,false),direct(120,false);constexpr float h=LocomotionMotor::FixedStep;
    for(unsigned tick=0;tick<390;++tick){const auto input=Prelude(tick);f.Accepted(input,h,1);direct.Accepted(input,h,1);
        Need(f.State()==direct.State(),"BRAKE_INDEPENDENT_NATURAL_PREFIX");}
    bool witnessed{};unsigned event{};
    for(;event<180&&!witnessed;++event){
        const Explorer entry=f.explorer;const auto previous=f.actor.Position();
        CharacterDirectDisplacementPreviewToken3D token;
        Need(f.actor.PreviewDirectDisplacementTick({f.actor.DirectLastTick()+1,{},0},std::span<const BoxCollider3D>{},f.terrain,token),"BRAKE_TARGET_PREVIEW_SETUP");
        const auto before=f.State();Need(before==direct.State(),"BRAKE_PREVIEW_DOES_NOT_CHANGE_COMMITTED_PREFIX");
        BrakingWitness witness;Snapshot candidate{};bool called{};
        const auto result=f.Advance({},h,[&]{called=true;witness=FindShortPlan(entry,f);if(!witness.found)return true;
            candidate=f.State();return false;});
        Need(called,"BRAKE_CAMERA_CALLBACK_REACHED");
        if(!witness.found){Need(result.accepted&&result.steps==1,"BRAKE_RELEASE_BEFORE_FIRST_SHORT_PLAN");
            direct.Accepted({},h,1);Need(f.State()==direct.State()&&!f.actor.CanCommitDirectDisplacementPreview(token),"BRAKE_ACCEPTED_PREFIX_AND_PREVIEW_INVALIDATION");
            FootGates(f,entry);continue;}
        witnessed=true;++shortPlans;
        Need(!result.accepted&&!result.poseRejected&&f.State()==before&&f.actor.CanCommitDirectDisplacementPreview(token),"BRAKE_FIRST_SHORT_PLAN_FALSE_FULL_ROLLBACK");++falseRollbacks;
        bool threw{};const auto rejected=f.Advance({},h,[&]()->bool{
            const auto again=FindShortPlan(entry,f);Need(again.found&&f.State()==candidate,"BRAKE_SAME_TARGET_ON_THROW_RETRY");
            threw=true;throw std::runtime_error("EXPECTED_BRAKING_BOUNDARY_CAMERA_THROW");});
        Need(threw&&!rejected.accepted&&!rejected.poseRejected&&f.State()==before&&f.actor.CanCommitDirectDisplacementPreview(token),"BRAKE_FIRST_SHORT_PLAN_THROW_FULL_ROLLBACK");++throwRollbacks;
        f.Accepted({},h,1);direct.Accepted({},h,1);
        Need(f.State()==direct.State()&&f.State()==candidate&&!f.actor.CanCommitDirectDisplacementPreview(token),"BRAKE_SAME_INPUT_RETRY_EQUALS_SINGLE_ACCEPTED");++retryMatches;
        FootGates(f,entry);DefaultAndRevalidate(entry,f,previous);ZeroTime(f);
        Need(f.State()==direct.State(),"BRAKE_API_CONTROLS_DO_NOT_TOUCH_LIVE_FIXTURES");
        std::cout<<"BRAKE_FIRST_SHORT_PLAN resume="<<resume<<" release_tick="<<event<<" side="<<witness.side
            <<" speed="<<witness.speed<<" duration="<<witness.duration<<" stopping_distance="<<witness.distance
            <<" old_lead="<<witness.oldLead<<" first_lead="<<witness.firstLead<<" landing_x="<<witness.landing.x
            <<" landing_z="<<witness.landing.z<<" old_landing_x="<<witness.oldLanding.x<<" old_landing_z="<<witness.oldLanding.z<<'\n';
    }
    Need(witnessed,"FIRST_SHORTENED_NATURAL_PLAN_NOT_REACHED_IN_180_RELEASE_STEPS");
    const auto start=f.actor.Position();const Vec3 direction{0,0,resume==0?-1.f:1.f};
    for(unsigned tick=0;tick<360;++tick){PlayerLocomotionInput input;if(tick<120)input.movement=direction;
        const Explorer entry=f.explorer;f.Accepted(input,h,1);direct.Accepted(input,h,1);
        Need(f.State()==direct.State(),"BRAKE_CONTINUATION_EQUALS_UNREJECTED_TWIN");FootGates(f,entry);}
    Need(Dot(f.actor.Position()-start,direction)>0,"BRAKE_NEXT_TICK_REPRESS_OR_REVERSE_REAL_PROGRESS");
    Need(f.actor.Grounded()&&Length(f.actor.Velocity())<=.001f&&!f.explorer.footSwings[0].Active()&&!f.explorer.footSwings[1].Active(),
        "BRAKE_FINITE_REPRESS_AND_RELEASE_DRAIN");++boundaryRoutes;
    std::cout<<"BRAKE_BOUNDARY_ROUTE resume="<<resume<<" forward_projection="<<Dot(f.actor.Position()-start,direction)
        <<" final_owner="<<f.owner.StateHash()<<" final_pose="<<PoseHash(f.explorer)<<" native=0 GPU=0\n";
}
bool ApiExclusion(bool crouched){
    Fixture f(120,crouched);constexpr float h=LocomotionMotor::FixedStep;
    if(!crouched)for(unsigned tick=0;tick<120;++tick){PlayerLocomotionInput input;input.movement={0,0,1};f.Accepted(input,h,1);}
    bool covered{};
    for(unsigned tick=0;tick<120&&!covered;++tick){const Explorer entry=f.explorer;const auto previous=f.actor.Position();
        PlayerLocomotionInput input;input.movement={0,0,crouched?1.f:-1.f};input.crouched=crouched;f.Accepted(input,h,1);
        const auto& e=f.explorer;const bool moving=e.locomotionDistance>.0001f&&e.locomotionSeconds>0;
        const float speed=std::hypot(e.locomotionVelocity.x,e.locomotionVelocity.z);
        const auto direction=speed>.0001f?e.locomotionVelocity*(1.f/speed):Vec3{};
        const bool turning=moving&&std::abs(std::remainder(std::atan2(direction.x,direction.z)-e.yaw,6.2831853f))>.26179939f;
        if(!f.actor.Grounded()||!moving||(!crouched&&!turning)||(!NewPlan(entry,e,0)&&!NewPlan(entry,e,1)))continue;
        auto a=entry,b=entry;const auto delta=f.actor.Position()-previous;a.Update(delta,h,crouched);b.Update(delta,h,crouched);
        const bool first=a.PlaceFeet(f.actor.Position(),true,crouched,f.terrain,{}, {},nullptr,nullptr,ExplorerFootPlacementMode::Advance,false);
        const bool second=b.PlaceFeet(f.actor.Position(),true,crouched,f.terrain,{}, {},nullptr,nullptr,ExplorerFootPlacementMode::Advance,true);
        Need(first&&second&&PoseHash(a)==PoseHash(b),"BRAKE_TRUE_CROUCH_OR_TURN_EXCLUDES_HINT");covered=true;
    }
    std::cout<<"BRAKE_API_EXCLUSION name="<<(crouched?"crouch":"moving-turn")<<" new_plan_witnessed="<<covered
        <<" synthetic_hint=1 natural_player_state=1 uncovered_if_zero=1\n";return covered;
}
}
int main()try{
    std::cout.imbue(std::locale::classic());std::cerr.imbue(std::locale::classic());
    std::cout<<std::setprecision(std::numeric_limits<double>::max_digits10);
    NaturalBoundary(0);NaturalBoundary(1);const bool crouch=ApiExclusion(true),turn=ApiExclusion(false);
    Need(crouch&&turn,"BRAKE_CROUCH_AND_TURN_NEW_PLAN_COVERAGE_REQUIRED");
    Need(boundaryRoutes==2&&shortPlans==2&&falseRollbacks==2&&throwRollbacks==2&&retryMatches==2&&
        zeroControls==2&&defaultControls==2&&revalidateControls==2,"BRAKE_BOUNDARY_FINITE_INVENTORY");
    std::cout<<"BRAKING_BOUNDARIES_PASS natural_routes=2 first_short_plans=2 callback_false=2 callback_throw=2 retry_matches=2"
        <<" zero_dt=2 default_false=2 revalidate=2 crouch_new_plan="<<crouch<<" turn_new_plan="<<turn
        <<" checks="<<checks<<" private_state_fabricated=0 native=0 GPU=0\n";return 0;
}catch(const std::exception& error){std::cerr<<"BRAKING_BOUNDARY_FIRST_DIVERGENCE "<<error.what()<<" checks="<<checks<<'\n';return 2;}
