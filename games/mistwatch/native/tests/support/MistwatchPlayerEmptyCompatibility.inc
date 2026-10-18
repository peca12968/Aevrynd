// 空 dynamic 使用同一已跑的36tick Q配方；兩個獨立執行檔只換 Player header。
void WriteEmptyWords(unsigned tick,const Fixture& f){
    const auto scalar=[](float value){std::cout<<','<<std::bit_cast<std::uint32_t>(value);};
    const auto vec=[&](Vec3 v){scalar(v.x);scalar(v.y);scalar(v.z);};
    const auto motion=f.actor.CaptureMotionState();const auto stats=f.actor.Stats();
    const auto player=f.owner.CaptureMotionState();const auto& motor=player.motor;
    std::cout<<"EMPTY_WORD_TAPE tick="<<tick<<','<<f.actor.StateHash()<<','<<f.owner.StateHash();
    vec(motion.feet);vec(motion.velocity);scalar(motion.height);scalar(motion.yaw);std::cout<<','<<motion.grounded;
    for(auto value:{stats.steps,stats.collisions,stats.jumps,stats.landings,stats.terrainContacts,stats.directTicks,stats.directBlocked,stats.directOverlaps})
        std::cout<<','<<value;
    vec(motor.velocity);scalar(motor.heading);
    std::cout<<','<<motor.headingReady<<','<<unsigned(motor.phase)<<','<<motor.supportTicks<<','<<motor.tick
        <<','<<motor.runFlight<<','<<motor.wasDashing<<','<<std::bit_cast<std::uint64_t>(player.remainder)<<','<<player.dodgeFlight;
    vec(f.evasion.direction);scalar(f.evasion.remaining);scalar(f.evasion.cooldown);
    const auto& e=f.explorer;scalar(e.phase);scalar(e.yaw);scalar(e.requestedYaw);vec(e.lastFeet);
    scalar(e.locomotionDistance);scalar(e.locomotionSeconds);vec(e.locomotionVelocity);
    std::cout<<','<<e.feetInitialized<<','<<e.gaitGrounded<<','<<e.nextFoot<<','<<e.pose.localTransforms.size();
    for(const auto& t:e.pose.localTransforms){
        vec(t.translation);vec(t.rotation);vec(t.scale);scalar(t.quaternion.x);scalar(t.quaternion.y);
        scalar(t.quaternion.z);scalar(t.quaternion.w);std::cout<<','<<t.usesQuaternion;
    }
    std::cout<<','<<e.pose.globalMatrices.size();
    for(const auto& matrix:e.pose.globalMatrices)for(const auto& row:matrix.values)for(float value:row)scalar(value);
    for(unsigned side=0;side<2;++side){
        vec(e.footTargets[side]);std::cout<<','<<e.planted[side];scalar(e.footFacing[side]);
        scalar(e.swingFacingStart[side]);scalar(e.swingFacingEnd[side]);const auto& swing=e.footSwings[side];
        std::cout<<','<<swing.Ready()<<','<<swing.Active()<<','<<swing.Replanned();
        for(double value:{swing.Elapsed(),swing.Duration(),swing.LiftHeight(),swing.SpeedBound()})
            std::cout<<','<<std::bit_cast<std::uint64_t>(value);
        // 只對副本採樣，保留原足步的當下與未來公開軌跡；不假稱完整私有曲線序列化。
        for(unsigned at=0;at<5;++at){
            auto copy=swing;FootSwingSample3D sample;
            const bool ok=copy.Advance((std::max)(0.,swing.Duration()-swing.Elapsed())*at/4.,sample);
            std::cout<<','<<ok;if(ok){vec(sample.positionWorld);vec(sample.velocityWorld);std::cout<<','<<sample.planted;}
        }
    }
    std::cout<<'\n';
}
int RunPlayerEmptyCompatibility(){
    try{
        Fixture f;
        for(unsigned tick=0;tick<36;++tick){
            PlayerLocomotionInput input;input.evadePressed=tick==0;input.dodgeDirection={0,0,1};
            CharacterDynamicContactState3D report{true,true,true};
            const auto result=f.owner.Advance(f.actor,f.evasion,f.explorer,input,H,NoBoxes,NoHulls,f.terrain,
                []{return true;},std::span<const DynamicCylinder3D>{},CharacterDynamicVerticalResponse3D::BlockOnly,&report);
            Need(result.motion.accepted&&!result.motion.poseRejected&&result.motion.steps==1&&!result.contact&&
                !report.blockedAbove&&!report.blockedBelow&&!report.blockedSide,"EMPTY_DYNAMIC_REAL_PLAYER_NOT_ACCEPTED");
            WriteEmptyWords(tick,f);
        }
        std::cout<<"EMPTY_DYNAMIC_COMPATIBILITY_TAPE_COMPLETE rows=36\n";return 0;
    }catch(const std::exception& e){std::cerr<<"EMPTY_DYNAMIC_COMPATIBILITY_FAILURE "<<e.what()<<'\n';return 1;}
}
