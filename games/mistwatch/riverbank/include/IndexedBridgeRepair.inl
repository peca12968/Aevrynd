inline BridgeRepairPlayerResult PrepareBridgeRepairPlayer(const Aevrynd::CharacterController3D& controller,Explorer& explorer,
    const IndexedValleySaveDomain& source,std::span<const Aevrynd::ConvexAabbSweep3D* const> hulls,
    const Aevrynd::HeightField3D& terrain,const Aevrynd::Camera3D& camera,float cameraDistance,std::span<const Aevrynd::BoxCollider3D> cameraOnly={})noexcept {
    try{
        std::vector<Aevrynd::BoxCollider3D> body,view;
        if(!source.Body(true,controller.Position(),body)||!source.Camera(true,controller.Position(),cameraDistance,&camera,view))return {BridgeRepairPlayerReason::PreparationUnavailable};
        if(LoadPreparationDetail::BodyOverlaps(controller.Center(),controller.Extents(),body,hulls))return {BridgeRepairPlayerReason::BodyOverlap};
        if(!Aevrynd::ThirdPersonCameraPathClear3D(camera.position,camera.position,view,terrain,cameraOnly,.16f))return {BridgeRepairPlayerReason::CameraBlocked};
        PlayerLocomotionPoseState prior(explorer);
        try{const bool crouched=controller.Settings().height<1.5f;explorer.RebuildLocomotionPose(crouched);
            if(explorer.PlaceFeet(controller.Position(),controller.Grounded(),crouched,terrain,body,hulls,nullptr,nullptr,ExplorerFootPlacementMode::RevalidateCurrent))return {BridgeRepairPlayerReason::Accepted};
        }catch(...){/* 只撤銷本次角色姿態，不修改世界或修橋任務。 */}
        prior.Restore(explorer);return {BridgeRepairPlayerReason::PoseRejected};
    }catch(...){return {BridgeRepairPlayerReason::PreparationUnavailable};}
}
