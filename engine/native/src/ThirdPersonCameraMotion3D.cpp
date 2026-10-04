#include <Aevrynd/ThirdPersonCameraMotion3D.h>
#include <algorithm>
#include <cmath>

namespace Aevrynd {
namespace {
bool Finite(Vec3 p){return std::isfinite(p.x)&&std::isfinite(p.y)&&std::isfinite(p.z);}
bool ValidCamera(const Camera3D& camera){
    const auto& p=camera.projection;
    return Finite(camera.position)&&Finite(camera.target)&&Finite(camera.up)&&
        p.mode==ProjectionMode::Perspective&&std::isfinite(p.verticalFieldOfView)&&p.verticalFieldOfView>.01f&&
        p.verticalFieldOfView<3.13f&&std::isfinite(p.nearPlane)&&p.nearPlane>0&&
        std::isfinite(p.farPlane)&&p.farPlane>p.nearPlane&&std::isfinite(Length(camera.position-camera.target))&&Length(camera.position-camera.target)>.001f&&
        Finite(Cross(camera.up,camera.target-camera.position))&&std::isfinite(Length(Cross(camera.up,camera.target-camera.position)))&&Length(Cross(camera.up,camera.target-camera.position))>.001f;
}
bool ValidBoxes(std::span<const BoxCollider3D> boxes){
    for(const auto& box:boxes)if(!Finite(box.center)||!Finite(box.extents)||
        box.extents.x<0||box.extents.y<0||box.extents.z<0||!Finite(box.center+box.extents)||!Finite(box.center-box.extents))return false;
    return true;
}
bool Framed(const Camera3D& camera,std::span<const Vec3> samples){
    if(!ValidCamera(camera)||samples.empty()||Length(camera.position-camera.target)<1.5f)return false;
    for(const auto p:samples)if(!Finite(p)||!ProjectToViewport(p,camera,{1280,720}).visible)return false;
    return true;
}
bool Visible(const Camera3D& camera,std::span<const Vec3> samples,std::span<const BoxCollider3D> solids,
    std::span<const BoxCollider3D> extra,const HeightField3D& terrain){
    for(const auto p:samples){
        const auto offset=p-camera.position;const float distance=Length(offset);
        if(distance<=.03f)return false;
        const auto ray=MakeRay3D(camera.position,offset);
        for(const auto boxes:{solids,extra})for(const auto& box:boxes)
            if(RayIntersectsAabb(ray,box.center,box.extents,distance-.03f))return false;
        // 可見性品質仍是有限 LOS 取樣；相機體積安全使用下方格子上界，不混為一談。
        for(unsigned i=1;i<32;++i){const auto point=camera.position+offset*(float(i)/32.f);
            if(terrain.Contains(point.x,point.z)){
                const float height=terrain.SampleHeight(point.x,point.z);
                if(!std::isfinite(height)||point.y<height+.02f)return false;
            }}
    }
    return true;
}
}
bool ThirdPersonCameraPathClear3D(Vec3 start,Vec3 end,std::span<const BoxCollider3D> solids,
    const HeightField3D& terrain,std::span<const BoxCollider3D> extra,float radius){
    if(!Finite(start)||!Finite(end)||!std::isfinite(radius)||radius<=0)return false;
    const Vec3 shift=end-start;const float length=Length(shift);
    if(!Finite(shift)||!std::isfinite(length))return false;
    for(const auto boxes:{solids,extra})for(const auto& box:boxes){
        if(!Finite(box.center)||!Finite(box.extents)||box.extents.x<0||box.extents.y<0||box.extents.z<0)return false;
        const auto extents=box.extents+Vec3{radius,radius,radius};
        const auto local=start-box.center;
        if(!Finite(extents)||!Finite(local)||!Finite(box.center+extents)||!Finite(box.center-extents))return false;
        // 直接以線段參數裁切，包含零長度與微小位移，不依賴 Normalize 的 epsilon。
        double enter=0,exit=1;
        const auto clip=[&](double origin,double delta,double extent){
            if(delta==0)return origin>=-extent&&origin<=extent;
            double first=(-extent-origin)/delta,last=(extent-origin)/delta;
            if(first>last)std::swap(first,last);
            enter=(std::max)(enter,first);exit=(std::min)(exit,last);return enter<=exit;
        };
        if(clip(local.x,shift.x,extents.x)&&clip(local.y,shift.y,extents.y)&&clip(local.z,shift.z,extents.z))return false;
    }
    const auto& grid=terrain.Settings();
    if(!Finite(grid.origin)||grid.width<2||grid.depth<2||grid.width>1000000||grid.depth>1000000||
       !std::isfinite(grid.cellSize)||grid.cellSize<=0||!Finite(terrain.SamplePosition(grid.width-1,grid.depth-1)))return false;
    const auto index=[&](float p,float origin,unsigned count){
        const double cell=std::floor((double(p)-origin)/grid.cellSize);
        return int(std::clamp(cell,0.0,double(count)-2));
    };
    const int x0=index((std::min)(start.x,end.x)-radius,grid.origin.x,grid.width);
    const int x1=index((std::max)(start.x,end.x)+radius,grid.origin.x,grid.width);
    const int z0=index((std::min)(start.z,end.z)-radius,grid.origin.z,grid.depth);
    const int z1=index((std::max)(start.z,end.z)+radius,grid.origin.z,grid.depth);
    if(uint64_t(x1-x0+1)*uint64_t(z1-z0+1)>4096)return false;
    for(int z=z0;z<=z1;++z)for(int x=x0;x<=x1;++x){
        const auto corner=terrain.SamplePosition(unsigned(x),unsigned(z));
        double enter=0,exit=1;
        const auto clip=[&](double p,double delta,double minimum,double maximum){
            if(delta==0)return p>=minimum&&p<=maximum;
            double first=(minimum-p)/delta,last=(maximum-p)/delta;
            if(first>last)std::swap(first,last);
            enter=(std::max)(enter,first);exit=(std::min)(exit,last);return enter<=exit;
        };
        if(!clip(start.x,shift.x,double(corner.x)-radius,double(corner.x)+grid.cellSize+radius)||
           !clip(start.z,shift.z,double(corner.z)-radius,double(corner.z)+grid.cellSize+radius))continue;
        const std::array corners{terrain.Height(unsigned(x),unsigned(z)),terrain.Height(unsigned(x+1),unsigned(z)),
            terrain.Height(unsigned(x),unsigned(z+1)),terrain.Height(unsigned(x+1),unsigned(z+1))};
        for(float height:corners)if(!std::isfinite(height))return false;
        const float highest=*std::max_element(corners.begin(),corners.end());
        if((std::min)(double(start.y)+shift.y*enter,double(start.y)+shift.y*exit)<double(highest)+radius)return false;
    }
    return true;
}
ThirdPersonCameraMotionResult3D ThirdPersonCameraMotion3D::Update(Camera3D desired,std::span<const Vec3> samples,
    std::span<const BoxCollider3D> solids,std::span<const BoxCollider3D> ceilings,const HeightField3D& terrain,
    float dt,std::span<const BoxCollider3D> extra){
    ThirdPersonCameraMotionResult3D result;result.camera=camera_;
    // 拒絕的更新不提交路徑快取，讓呼叫者的控制器回滾與重試保持原子性。
    auto stagedRoute=route_;unsigned stagedCount=routeCount_,stagedIndex=routeIndex_;
    float stagedDirection=routeDirection_,stagedWantedPitch=routeWantedPitch_,stagedWantedRange=routeWantedRange_;
    if(!ValidCamera(desired)||samples.empty()||samples.size()>64||!std::isfinite(dt)||dt<0||dt>.1f||
        !ValidBoxes(solids)||!ValidBoxes(ceilings)||!ValidBoxes(extra)){
        result.status=ThirdPersonCameraMotionStatus3D::InvalidInput;return result;
    }
    for(const auto p:samples)if(!Finite(p)){result.status=ThirdPersonCameraMotionStatus3D::InvalidInput;return result;}
    const float tangent=std::tan(desired.projection.verticalFieldOfView*.5f);
    const float radius=(std::max)(.16f,.005f+desired.projection.nearPlane*std::sqrt(1.f+tangent*tangent*(1.f+1280.f/720.f*1280.f/720.f)));
    if(!std::isfinite(radius)){result.status=ThirdPersonCameraMotionStatus3D::InvalidInput;return result;}
    const auto safe=[&](const Camera3D& camera){return Framed(camera,samples)&&ThirdPersonCameraPathClear3D(camera.position,camera.position,solids,terrain,extra,radius);};
    const auto commit=[&](Camera3D camera,ThirdPersonCameraMotionStatus3D status){
        result.displacement=initialized_?Length(camera.position-camera_.position):0;
        camera_=camera;initialized_=true;result.camera=camera;result.accepted=true;
        route_=stagedRoute;routeCount_=stagedCount;routeIndex_=stagedIndex;routeDirection_=stagedDirection;
        routeWantedPitch_=stagedWantedPitch;routeWantedRange_=stagedWantedRange;
        result.subjectVisible=Visible(camera,samples,solids,extra,terrain);
        result.status=status;
        if(status==ThirdPersonCameraMotionStatus3D::TrackingVisible&&!result.subjectVisible)
            result.status=ThirdPersonCameraMotionStatus3D::TrackingOccluded;
    };
    if(dt==0){result.status=ThirdPersonCameraMotionStatus3D::Paused;return result;}
    if(!initialized_){
        if(safe(desired))commit(desired,ThirdPersonCameraMotionStatus3D::ResetCut);
        return result;
    }
    const auto& prior=camera_.projection;
    if(desired.projection.verticalFieldOfView!=prior.verticalFieldOfView||desired.projection.nearPlane!=prior.nearPlane||
       desired.projection.farPlane!=prior.farPlane){result.status=ThirdPersonCameraMotionStatus3D::InvalidInput;return result;}
    const auto targetShift=desired.target-camera_.target;
    if(Length(targetShift)>2.f)return result;
    auto anchor=camera_;anchor.target=desired.target;anchor.position=anchor.position+targetShift;
    if(!ThirdPersonCameraPathClear3D(camera_.position,anchor.position,solids,terrain,extra,radius))anchor.position=camera_.position;
    const auto from=anchor.position-anchor.target,to=desired.position-desired.target;
    const float range=Length(from),wantedRange=Length(to);
    if(range<=.01f||wantedRange<=.01f)return result;
    const float yaw=std::atan2(from.x,-from.z),wantedYaw=std::atan2(to.x,-to.z);
    const float nextYaw=yaw+std::clamp(std::remainder(wantedYaw-yaw,6.2831853f),-2.4f*dt,2.4f*dt);
    const float pitch=std::atan2(from.y,std::hypot(from.x,from.z)),wantedPitch=std::atan2(to.y,std::hypot(to.x,to.z));
    const float nextPitch=pitch+std::clamp(wantedPitch-pitch,-1.8f*dt,1.8f*dt);
    const float nextRange=range+std::clamp(wantedRange-range,-10.f*dt,10.f*dt);
    auto candidate=MakeThirdPersonCamera(desired.target,nextYaw,nextPitch,nextRange,desired.projection);
    candidate=ConstrainThirdPersonCameraCeiling(candidate,ceilings);
    candidate=ConstrainThirdPersonCameraCeiling(candidate,extra);
    const auto continuous=[&](const Camera3D& camera){
        const float budget=30.f*dt+Length(targetShift);
        return Length(camera.position-camera_.position)<=budget+.00001f&&Framed(camera,samples)&&
            ThirdPersonCameraPathClear3D(camera_.position,camera.position,solids,terrain,extra,radius);
    };
    const float error=std::remainder(wantedYaw-yaw,6.2831853f);
    if(stagedIndex<stagedCount){
        const float promised=std::remainder(stagedRoute[stagedCount-1].x-yaw,6.2831853f)*stagedDirection;
        if(error*stagedDirection<promised-.001f||std::abs(wantedPitch-stagedWantedPitch)>.0001f||
           std::abs(wantedRange-stagedWantedRange)>.0001f)stagedCount=stagedIndex=0;
    }
    const auto advanceRoute=[&](){
        // 世界、目標與角色姿態可能已改變；重新確認整條剩餘出口，不只驗下一步。
        auto proof=camera_;
        for(unsigned i=stagedIndex;i<stagedCount;++i){const auto spec=stagedRoute[i];
            const auto next=MakeThirdPersonCamera(desired.target,spec.x,spec.y,spec.z,desired.projection);
            if(!Framed(next,samples)||!ThirdPersonCameraPathClear3D(proof.position,next.position,solids,terrain,extra,radius)){
                stagedCount=stagedIndex=0;return false;
            }
            proof=next;
        }
        while(stagedIndex<stagedCount){
            const auto spec=stagedRoute[stagedIndex];
            const auto endpoint=MakeThirdPersonCamera(desired.target,spec.x,spec.y,spec.z,desired.projection);
            const auto shift=endpoint.position-anchor.position;
            const float distance=Length(shift);
            if(distance<.0001f){++stagedIndex;continue;}
            float fraction=(std::min)(1.f,30.f*dt/distance);
            for(unsigned attempt=0;attempt<16;++attempt,fraction*=.5f){
                auto next=anchor;next.position=anchor.position+shift*fraction;
                const auto offset=next.position-next.target;
                const float newYaw=std::atan2(offset.x,-offset.z),newPitch=std::atan2(offset.y,std::hypot(offset.x,offset.z));
                if(std::abs(std::remainder(newYaw-yaw,6.2831853f))>2.4f*dt+.00001f||
                   std::abs(newPitch-pitch)>1.8f*dt+.00001f||std::abs(Length(offset)-range)>10.f*dt+.00001f)continue;
                // 執行已驗證的直線邊；不把直線證據錯用在極座標弧線。每步仍檢查目前世界。
                if(!continuous(next)){stagedCount=stagedIndex=0;return false;}
                commit(next,ThirdPersonCameraMotionStatus3D::TrackingVisible);return true;
            }
            stagedCount=stagedIndex=0;return false;
        }
        return false;
    };
    if(advanceRoute())return result;
    if(continuous(candidate))commit(candidate,ThirdPersonCameraMotionStatus3D::TrackingVisible);
    else if(std::abs(error)>.001f){
        // 最多 81 節點的向前極座標圖；只有已找到完整出口才移動，避免盲目縮距後卡住。
        constexpr unsigned count=81;
        const float direction=error>0?1.f:-1.f,horizon=(std::min)(std::abs(error),.34906585f);
        const std::array ranges{range,(std::max)(1.5f,range-1.5f),(std::min)(10.f,range+1.5f)};
        const std::array pitches{pitch,(std::min)(1.1f,pitch+.25f),(std::min)(1.1f,pitch+.5f)};
        std::array<Camera3D,count> nodes{};std::array<Vec3,count> specs{};
        std::array<bool,count> allowed{},visited{};std::array<float,count> costs{};std::array<int,count> parents{};
        costs.fill(1e30f);parents.fill(-1);
        for(unsigned i=0;i<count;++i){const unsigned plane=i/9,r=(i%9)/3,p=i%3;
            specs[i]={yaw+direction*horizon*float(plane)/8.f,pitches[p],ranges[r]};
            nodes[i]=MakeThirdPersonCamera(desired.target,specs[i].x,specs[i].y,specs[i].z,desired.projection);
            allowed[i]=safe(nodes[i]);
        }
        if(allowed[0])costs[0]=0;
        int goal=-1;
        for(unsigned iteration=0;iteration<count;++iteration){
            int best=-1;for(unsigned i=0;i<count;++i)if(!visited[i]&&allowed[i]&&costs[i]<1e29f&&(best<0||costs[i]<costs[unsigned(best)]))best=int(i);
            if(best<0)break;
            const unsigned index=unsigned(best);visited[index]=true;
            if(index/9==8){goal=best;break;}
            const int plane=int(index/9),r=int((index%9)/3),p=int(index%3);
            const std::array<std::array<int,3>,5> adjacent{{{plane+1,r,p},{plane,r==0?1:0,p},{plane,r==0?2:0,p},{plane,r,p-1},{plane,r,p+1}}};
            for(const auto xyz:adjacent){if(xyz[0]>8||xyz[1]<0||xyz[1]>2||xyz[2]<0||xyz[2]>2)continue;
                const unsigned next=unsigned(xyz[0]*9+xyz[1]*3+xyz[2]);
                if(visited[next]||!allowed[next]||!ThirdPersonCameraPathClear3D(nodes[index].position,nodes[next].position,solids,terrain,extra,radius))continue;
                const float cost=costs[index]+Length(nodes[next].position-nodes[index].position)+.001f;
                if(cost<costs[next]){costs[next]=cost;parents[next]=best;}
            }
        }
        if(goal>=0){
            std::array<Vec3,count> reversed{};unsigned size{};
            for(int node=goal;node>0&&size<count;node=parents[unsigned(node)])reversed[size++]=specs[unsigned(node)];
            stagedCount=size;stagedIndex=0;stagedDirection=direction;
            stagedWantedPitch=wantedPitch;stagedWantedRange=wantedRange;
            for(unsigned i=0;i<size;++i)stagedRoute[i]=reversed[size-1-i];
            if(advanceRoute())return result;
        }
        if(continuous(anchor))commit(anchor,ThirdPersonCameraMotionStatus3D::Held);
    }
    else if(continuous(anchor))commit(anchor,ThirdPersonCameraMotionStatus3D::Held);
    return result;
}
}
