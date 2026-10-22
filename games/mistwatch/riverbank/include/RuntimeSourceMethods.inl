    std::vector<BoxCollider3D> runtimeNpcSolids,runtimeCameraSolids;
    unsigned normalBodyOracleMatches{},normalCameraOracleMatches{},normalNpcSourceMatches{},normalMotionFixedSteps{},normalActiveNpcFrames{};
    std::size_t normalBodyMaximum{},normalCameraMaximum{},normalNpcMaximum{};
    std::string runtimeSourceFailure;
    static bool InRectXZ(const BoxCollider3D& b,double lx,double hx,double lz,double hz){
        return double(b.center.x)+b.extents.x>=lx&&double(b.center.x)-b.extents.x<=hx&&double(b.center.z)+b.extents.z>=lz&&double(b.center.z)-b.extents.z<=hz;
    }
    template<class Predicate> void RequireCompleteQuery(std::span<const BoxCollider3D> actual,Predicate&& includes){
        if(!indexedGameplayProofEnabled)return;
        std::size_t at{};for(const auto& b:matureIndexedDomain->Registered(session.bridgeRepaired))if(includes(b)){
            MatureContact::Require(at<actual.size()&&std::memcmp(&actual[at],&b,sizeof(b))==0,"RUNTIME_QUERY_DIFFERS_FROM_COMPLETE_SOURCE_SCAN");++at;
        }
        MatureContact::Require(at==actual.size(),"RUNTIME_QUERY_HAS_EXTRA_OR_MISSING_SOURCE_OWNERS");
    }
    float RuntimeFrameRadius(float dt)const {
        if(!std::isfinite(dt)||dt<0||dt>.1f)throw std::runtime_error("RUNTIME_FRAME_DT_OUTSIDE_REAL_OWNER_CONTRACT");
        const auto& s=controller.Settings();const auto v=controller.Velocity();
        float speed=(std::max)(s.moveSpeed*4,std::hypot(v.x,v.z));
        if(s.maximumTerrainStandingGrade>0)speed=(std::max)(speed,std::hypot(s.maximumFallSpeed,s.moveSpeed*4));
        const double horizon=(std::min)(double(dt)+playerLocomotion.Remainder(),12*double(Mistwatch::LocomotionMotor::FixedStep));
        const float radius=(std::max)(4.f,float(speed*horizon)+s.radius+1.f);
        if(!std::isfinite(radius))throw std::runtime_error("RUNTIME_FRAME_QUERY_RADIUS_INVALID");return radius;
    }
    bool RefreshRuntimeBody(float dt){
        if(!matureIndexedDomain)return true;const auto p=controller.Position();const float r=RuntimeFrameRadius(dt);
        std::vector<BoxCollider3D> next;if(!matureIndexedDomain->Nearby(session.bridgeRepaired,p,r,next))return false;
        RequireCompleteQuery(next,[&](const auto& b){return InRectXZ(b,double(p.x)-r,double(p.x)+r,double(p.z)-r,double(p.z)+r);});
        normalBodyMaximum=(std::max)(normalBodyMaximum,next.size());solids.swap(next);return true;
    }
    std::vector<BoxCollider3D> RuntimeGameplayBody(bool repaired,Vec3 p)const {
        if(!matureIndexedDomain)return RuntimeWorldSolids(repaired);std::vector<BoxCollider3D> next;
        if(!matureIndexedDomain->Body(repaired,p,next))throw std::runtime_error("RUNTIME_RESET_BODY_QUERY_REJECTED");return next;
    }
    std::span<const BoxCollider3D> RuntimeNpcSource(std::span<const BoxCollider3D> fallback){
        if(!matureIndexedDomain)return fallback;const auto p=encounter.feet,player=session.feet,strike=encounter.strikeTarget;const bool windup=encounter.phase==Mistwatch::EnemyPhase::Windup;
        if(!matureIndexedDomain->Enemy(session.bridgeRepaired,p,player,strike,windup,runtimeNpcSolids))throw std::runtime_error("RUNTIME_NPC_QUERY_CAPACITY_OR_SOURCE_REJECTED");
        RequireCompleteQuery(runtimeNpcSolids,[&](const auto& b){
            if(InRectXZ(b,double(p.x)-4,double(p.x)+4,double(p.z)-4,double(p.z)+4))return true;
            if(windup)for(const auto q:{player,strike})if(InRectXZ(b,(std::min)(double(p.x),double(q.x)),(std::max)(double(p.x),double(q.x)),(std::min)(double(p.z),double(q.z)),(std::max)(double(p.z),double(q.z))))return true;
            return false;
        });
        ++normalNpcSourceMatches;normalNpcMaximum=(std::max)(normalNpcMaximum,runtimeNpcSolids.size());
        if(Mistwatch::PairContact::LiveBody(session,encounter))++normalActiveNpcFrames;return runtimeNpcSolids;
    }
    std::span<const BoxCollider3D> RuntimeGuideSource(std::span<const BoxCollider3D> fallback)const{return matureIndexedDomain?matureIndexedDomain->Registered(session.bridgeRepaired):fallback;}
    bool UpdateIndexedCamera(float dt,std::span<const BoxCollider3D> fallback){
        if(!matureIndexedDomain)return UpdatePlayerCameraCore(dt,fallback);
        const auto* prior=playerCameraMotion.Initialized()?&playerCameraMotion.Camera():nullptr;
        if(!matureIndexedDomain->Camera(session.bridgeRepaired,session.feet,orbit.distance,prior,runtimeCameraSolids))return false;
        if(!indexedGameplayProofEnabled){normalCameraMaximum=(std::max)(normalCameraMaximum,runtimeCameraSolids.size());return UpdatePlayerCameraCore(dt,runtimeCameraSolids);}
        auto reference=playerCameraMotion;const auto all=matureIndexedDomain->Registered(session.bridgeRepaired);const auto extra=RuntimeCameraOnly();
        const auto framing=Mistwatch::BuildPlayerCameraFraming(session.feet,controller.Settings().height,orbit,all,terrain,extra);
        const auto expected=reference.Update(Mistwatch::BuildPlayerCameraMotionRequest(session.feet,controller.Settings().height,orbit,reference.Initialized(),all,terrain,extra),
            Mistwatch::PlayerCameraSamples(session.feet,controller.Settings().height),all,Mistwatch::CameraCeilingSolids(),terrain,dt,extra);
        const bool actual=UpdatePlayerCameraCore(dt,runtimeCameraSolids);
        if(!actual||!expected.accepted||expected.status!=lastCameraMotion.status||expected.subjectVisible!=lastCameraMotion.subjectVisible||
            !MatureSameVec(expected.camera.position,lastCameraMotion.camera.position)||!MatureSameVec(expected.camera.target,lastCameraMotion.camera.target)||!MatureSameVec(expected.camera.up,lastCameraMotion.camera.up)||
            !MatureSameVec(framing.camera.position,lastPlayerFraming.camera.position)||!MatureSameVec(framing.camera.target,lastPlayerFraming.camera.target)){
            runtimeSourceFailure="RUNTIME_CAMERA_DIFFERS_FROM_COMPLETE_SOURCE";return false;
        }
        ++normalCameraOracleMatches;normalCameraMaximum=(std::max)(normalCameraMaximum,runtimeCameraSolids.size());return true;
    }
