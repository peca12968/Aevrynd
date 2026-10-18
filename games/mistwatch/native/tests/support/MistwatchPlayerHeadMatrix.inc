// 完整有限清單先封存；接觸不變量違反要全數記錄，非預期姿態／輸入錯誤才提前停止。
int RunPlayerHeadMatrix(){
    unsigned completed{},queryClearCases{},guardedEvents{},violatingEvents{},violatingCases{},noAboveCases{};
    unsigned inferredFirst{},inferredSecond{},unclassified{};
    try{
        std::cout<<std::setprecision(std::numeric_limits<float>::max_digits10);
        constexpr unsigned Count=56,FirstMm=1825,StepMm=5;
        const unsigned tickCount=unsigned(std::ceil(double(Evasion::Duration+2*H)/H))+2u;
        Need(tickCount==38,"SEALED_SOURCE_OBSERVATION_WINDOW_CHANGED");
        std::cout<<"PHASE_AUDIT_BEGIN count="<<Count<<" first_mm="<<FirstMm<<" step_mm="<<StepMm
            <<" last_mm="<<FirstMm+(Count-1)*StepMm<<" ticks_per_case="<<tickCount
            <<" H="<<H<<" source_duration="<<Evasion::Duration<<" model_and_input_fixed=1 CPU_only=1 main_claim=0\n";
        for(unsigned index=0;index<Count;++index){
            const unsigned millimeters=FirstMm+index*StepMm;
            // 整數毫米先作 double 除法，再只 cast 一次；不累加 float 或改動清單順序。
            const float requestedBottom=float(double(millimeters)/1000.);
            const std::array<DynamicCylinder3D,1> body{{{{0,requestedBottom+.8f,0},1.3f,.8f}}};
            const auto dynamic=std::span<const DynamicCylinder3D>(body);
            Fixture f;BoxCylinderContact3D initial;
            const bool valid=QueryBoxCylinderContact3D(f.actor.Center(),f.actor.Extents(),{},body[0],initial);
            Need(valid&&initial.kind==BoxCylinderContactKind3D::Clear,"PHASE_CASE_INITIAL_QUERY_NOT_CLEAR");
            ++queryClearCases;
            const auto& settings=f.actor.Settings();
            const double maxDistance=std::hypot(double(settings.maximumFallSpeed),double(settings.moveSpeed)*4)*H;
            const unsigned innerCount=unsigned(std::clamp(std::ceil(maxDistance/std::max(.05,double(settings.radius)*.5)),1.,64.));
            const float innerH=H/float(innerCount);
            const auto name=std::string("cap-mm-")+std::to_string(millimeters);
            std::cout<<"PHASE_CASE_BEGIN index="<<index<<" millimeters="<<millimeters<<" bottom_f32="<<requestedBottom
                <<" bottom_bits="<<std::bit_cast<std::uint32_t>(requestedBottom)
                <<" center_y_bits="<<std::bit_cast<std::uint32_t>(body[0].center.y)
                <<" actual_bottom="<<double(body[0].center.y)-body[0].halfHeight
                <<" zero_query_clear=1 source_launch_pulse="<<.5f*settings.gravity*(Evasion::Duration+2*H)
                <<" inner_count="<<innerCount<<" inner_h="<<innerH<<" encounter_supported_claim=0\n";
            bool caseAbove=false,caseViolation=false;
            unsigned caseGuarded{},caseViolations{};
            for(unsigned tick=0;tick<tickCount;++tick){
                const auto before=f.actor.CaptureMotionState();const auto stats=f.actor.Stats();const auto evasion=f.evasion;
                PlayerLocomotionInput input;input.evadePressed=tick==0;input.dodgeDirection={0,0,1};
                CharacterDynamicContactState3D contact;
                const auto result=f.owner.Advance(f.actor,f.evasion,f.explorer,input,H,NoBoxes,NoHulls,f.terrain,
                    []{return true;},dynamic,CharacterDynamicVerticalResponse3D::BlockOnly,&contact);
                PrintStep(name.c_str(),tick,f,before,stats,evasion,result,contact);
                if(!result.motion.accepted){
                    std::cerr<<"PHASE_AUDIT_UNEXPECTED_REJECTION index="<<index<<" millimeters="<<millimeters<<" tick="<<tick
                        <<" pose_rejected="<<result.motion.poseRejected<<" failure_line="<<result.motion.poseFailureLine
                        <<" reason="<<unsigned(result.motion.footPlacementDiagnostic.reason)<<'\n';
                    return 3;
                }
                caseAbove=caseAbove||contact.blockedAbove;
                if(contact.blockedAbove&&before.velocity.y>0){
                    ++guardedEvents;++caseGuarded;
                    const float expectedCooldown=(std::max)(0.f,evasion.cooldown-H);
                    const bool remainingConsumed=f.evasion.remaining==0;
                    const bool cooldownNatural=f.evasion.cooldown==expectedCooldown;
                    const bool noAddedJump=f.actor.Stats().jumps==stats.jumps;
                    const bool noAddedLanding=f.actor.Stats().landings==stats.landings;
                    const bool noFakeGround=!f.actor.Grounded();
                    const bool violation=!remainingConsumed||!cooldownNatural||!noAddedJump||!noAddedLanding||!noFakeGround;
                    if(violation){++violatingEvents;++caseViolations;caseViolation=true;}
                    // 只用完整呼叫的真輸出及既有兩段半隱式積分解釋時相，沒有拆呼叫或新增物理解算。
                    unsigned inferredPhase{};
                    if(innerCount==2&&!contact.blockedBelow&&!contact.blockedSide){
                        if(f.actor.Velocity().y==-settings.gravity*innerH)inferredPhase=1;
                        else if(f.actor.Velocity().y==0)inferredPhase=2;
                    }
                    if(inferredPhase==1)++inferredFirst;else if(inferredPhase==2)++inferredSecond;else ++unclassified;
                    std::cout<<"PHASE_CONTACT index="<<index<<" millimeters="<<millimeters<<" tick="<<tick
                        <<" before_vy="<<before.velocity.y<<" after_vy="<<f.actor.Velocity().y
                        <<" above=1 before_remaining="<<evasion.remaining<<" after_remaining="<<f.evasion.remaining
                        <<" remaining_consumed_once="<<remainingConsumed<<" cooldown_natural="<<cooldownNatural
                        <<" expected_cooldown="<<expectedCooldown<<" actual_cooldown="<<f.evasion.cooldown
                        <<" no_added_jump="<<noAddedJump<<" no_added_landing="<<noAddedLanding<<" no_fake_ground="<<noFakeGround
                        <<" invariant_violation="<<violation<<" source_inferred_inner_phase="<<inferredPhase
                        <<" inference_not_internal_instrumentation=1 split_step_equivalence_claim=0\n";
                }
            }
            ++completed;if(caseViolation)++violatingCases;if(!caseAbove)++noAboveCases;
            std::cout<<"PHASE_CASE_END index="<<index<<" millimeters="<<millimeters<<" completed_ticks="<<tickCount
                <<" above_seen="<<caseAbove<<" guarded_events="<<caseGuarded<<" violating_events="<<caseViolations
                <<" hypothesis_violated="<<caseViolation<<" unhit_case_requires_contact=0\n";
        }
        Need(completed==56&&queryClearCases==56,"FIXED_CASE_LIST_NOT_COMPLETED");
        std::cout<<"PHASE_AUDIT_COMPLETE cases="<<completed<<" query_clear_cases="<<queryClearCases
            <<" guarded_events="<<guardedEvents<<" violating_events="<<violatingEvents<<" violating_cases="<<violatingCases
            <<" no_above_cases="<<noAboveCases<<" source_inferred_first="<<inferredFirst<<" source_inferred_second="<<inferredSecond
            <<" unclassified="<<unclassified<<" all_fixed_cases_retained=1 main_claim=0 GPU=0\n";
        return violatingEvents?1:0;
    }catch(const std::exception& e){
        std::cerr<<"PHASE_AUDIT_UNEXPECTED_FAILURE completed_cases="<<completed<<" error="<<e.what()<<'\n';return 5;
    }
}
