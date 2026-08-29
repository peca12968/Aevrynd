#include "Aevrynd/FootPlacement3D.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

namespace Aevrynd { namespace {
bool Finite(float v)noexcept{return std::isfinite(v);}
bool Finite(Vec3 v)noexcept{return Finite(v.x)&&Finite(v.y)&&Finite(v.z);}
bool Finite(const Matrix4& m)noexcept{for(auto& r:m.values)for(float v:r)if(!Finite(v))return false;return true;}
bool NearIdentity(const Matrix4& m)noexcept{for(std::size_t r=0;r<4;++r)for(std::size_t c=0;c<4;++c)if(std::abs(m[r][c]-(r==c?1.f:0.f))>2e-3f)return false;return true;}
bool NearMatrix(const Matrix4& a,const Matrix4& b)noexcept{for(std::size_t r=0;r<4;++r)for(std::size_t c=0;c<4;++c)if(std::abs(a[r][c]-b[r][c])>1e-3f)return false;return true;}
bool InversePair(const Matrix4& a,const Matrix4& b)noexcept{return Finite(a)&&Finite(b)&&NearIdentity(a*b)&&NearIdentity(b*a);}
Vec3 Position(const Matrix4& m)noexcept{return TransformPoint(m,{});}
Vec3 Direction(const Matrix4& m,Vec3 v)noexcept{const Vec4 q=m*Vec4{v.x,v.y,v.z,0};return{q.x,q.y,q.z};}
float Move(float from,float to,float maximum)noexcept{return from<to?(std::min)(from+maximum,to):(std::max)(from-maximum,to);}
Vec3 Lerp(Vec3 a,Vec3 b,float t)noexcept{return a+(b-a)*t;}
bool InverseLinear(const Matrix4& m,Vec3 v,Vec3& out)noexcept{
    const double a=m[0][0],b=m[0][1],c=m[0][2],d=m[1][0],e=m[1][1],f=m[1][2],g=m[2][0],h=m[2][1],i=m[2][2];
    const double det=a*(e*i-f*h)-b*(d*i-f*g)+c*(d*h-e*g);if(!std::isfinite(det)||std::abs(det)<1e-12)return false;
    const double x=((e*i-f*h)*v.x+(c*h-b*i)*v.y+(b*f-c*e)*v.z)/det;
    const double y=((f*g-d*i)*v.x+(a*i-c*g)*v.y+(c*d-a*f)*v.z)/det;
    const double z=((d*h-e*g)*v.x+(b*g-a*h)*v.y+(a*e-b*d)*v.z)/det;
    out={float(x),float(y),float(z)};return Finite(out);
}
Quaternion3D Multiply(Quaternion3D l,Quaternion3D r)noexcept{return NormalizeQuaternion3D({l.w*r.x+l.x*r.w+l.y*r.z-l.z*r.y,l.w*r.y-l.x*r.z+l.y*r.w+l.z*r.x,l.w*r.z+l.x*r.y-l.y*r.x+l.z*r.w,l.w*r.w-l.x*r.x-l.y*r.y-l.z*r.z});}
Quaternion3D Conjugate(Quaternion3D q)noexcept{return{-q.x,-q.y,-q.z,q.w};}
Quaternion3D MatrixRotation(const Matrix4& m)noexcept{
    const float t=m[0][0]+m[1][1]+m[2][2];Quaternion3D q{};
    if(t>0){const float s=std::sqrt(t+1.f)*2.f;q={ (m[2][1]-m[1][2])/s,(m[0][2]-m[2][0])/s,(m[1][0]-m[0][1])/s,.25f*s};}
    else if(m[0][0]>m[1][1]&&m[0][0]>m[2][2]){const float s=std::sqrt(1.f+m[0][0]-m[1][1]-m[2][2])*2.f;q={.25f*s,(m[0][1]+m[1][0])/s,(m[0][2]+m[2][0])/s,(m[2][1]-m[1][2])/s};}
    else if(m[1][1]>m[2][2]){const float s=std::sqrt(1.f+m[1][1]-m[0][0]-m[2][2])*2.f;q={(m[0][1]+m[1][0])/s,.25f*s,(m[1][2]+m[2][1])/s,(m[0][2]-m[2][0])/s};}
    else {const float s=std::sqrt(1.f+m[2][2]-m[0][0]-m[1][1])*2.f;q={(m[0][2]+m[2][0])/s,(m[1][2]+m[2][1])/s,.25f*s,(m[1][0]-m[0][1])/s};}
    return NormalizeQuaternion3D(q);
}
Quaternion3D FromTo(Vec3 a,Vec3 b)noexcept{a=Normalize(a);b=Normalize(b);const float d=Clamp(Dot(a,b),-1.f,1.f);if(d>.99999f)return{};if(d<-.99999f){Vec3 axis=Normalize(Cross(a,std::abs(a.x)<.8f?Vec3{1,0,0}:Vec3{0,0,1}));return{axis.x,axis.y,axis.z,0};}Vec3 axis=Cross(a,b);return NormalizeQuaternion3D({axis.x,axis.y,axis.z,1.f+d});}
bool PoseFinite(const AnimationPose3D& p)noexcept{for(const auto& m:p.globalMatrices)if(!Finite(m))return false;for(const auto& t:p.localTransforms)if(!Finite(t.translation)||!Finite(t.rotation)||!Finite(t.scale)||!Finite(t.quaternion.x)||!Finite(t.quaternion.y)||!Finite(t.quaternion.z)||!Finite(t.quaternion.w))return false;return true;}
const MovingPlatformSample3D* Platform(const FootPlacementFrame3D& f,ColliderHandle3D h,std::uint64_t u)noexcept{if(!h||!u||!f.collisionWorld)return nullptr;const auto* c=f.collisionWorld->Get(h);if(!c||c->userData!=u)return nullptr;for(const auto& p:f.movingPlatforms)if(p.collider==h&&p.userData==u)return &p;return nullptr;}
bool ValidPlatform(const MovingPlatformSample3D& p)noexcept{return p.collider&&p.userData&&InversePair(p.previousLocalToWorld,p.previousWorldToLocal)&&InversePair(p.currentLocalToWorld,p.currentWorldToLocal);}
}

bool FootPlacement3D::Configure(FootPlacementSettings3D s)noexcept{
    if(!Finite(s.probeUp)||!Finite(s.probeDown)||!Finite(s.probeRadius)||!Finite(s.minimumNormalY)||!Finite(s.maximumPelvisDrop)||!Finite(s.plantSpeed)||!Finite(s.releaseSpeed)||!Finite(s.pelvisSpeed)||!Finite(s.fullLodDistance)||!Finite(s.reducedLodDistance)||s.probeUp<0||s.probeDown<=0||s.probeRadius<0||s.minimumNormalY<0||s.minimumNormalY>1||s.maximumPelvisDrop<0||s.plantSpeed<=0||s.releaseSpeed<=0||s.pelvisSpeed<=0||s.fullLodDistance<0||s.reducedLodDistance<s.fullLodDistance||!s.reducedQueryInterval)return false;
    for(const auto& foot:s.feet){if(!Finite(foot.soleUpLocal)||!Finite(foot.soleForwardLocal)||!Finite(foot.soleOffset)||foot.soleOffset<0||std::abs(Length(foot.soleUpLocal)-1.f)>1e-3f||std::abs(Length(foot.soleForwardLocal)-1.f)>1e-3f||std::abs(Dot(foot.soleUpLocal,foot.soleForwardLocal))>.99f)return false;}
    settings_=s;return true;
}
bool FootPlacement3D::Prepare(const Skeleton3D& skeleton,const HumanoidSkeletonMap3D& map,SkeletonCompatibilityId3D expected){
    if(!expected||!map.Valid(skeleton,expected))return false;try{AnimationPose3D candidate;candidate.localTransforms.resize(skeleton.joints.size());candidate.globalMatrices.resize(skeleton.joints.size());scratchPose_=std::move(candidate);jointCount_=skeleton.joints.size();compatibility_=expected;Reset();return true;}catch(...){return false;}
}
bool FootPlacement3D::Evaluate(const Skeleton3D& skeleton,const HumanoidSkeletonMap3D& map,const FootPlacementFrame3D& frame,AnimationPose3D& pose,FootPlacementResult3D* output)noexcept{
    if(!compatibility_||jointCount_!=skeleton.joints.size()||!map.Valid(skeleton,compatibility_)||pose.localTransforms.size()!=jointCount_||pose.globalMatrices.size()!=jointCount_||scratchPose_.localTransforms.size()!=jointCount_||scratchPose_.globalMatrices.size()!=jointCount_||!PoseFinite(pose)||!InversePair(frame.modelToWorld,frame.worldToModel)||!Finite(frame.poseRoot)||!Finite(frame.deltaTime)||frame.deltaTime<=0||!Finite(frame.cameraDistance)||frame.cameraDistance<0)return false;
    for(std::size_t a=0;a<frame.movingPlatforms.size();++a){if(!ValidPlatform(frame.movingPlatforms[a]))return false;for(std::size_t b=0;b<a;++b)if(frame.movingPlatforms[a].collider==frame.movingPlatforms[b].collider&&frame.movingPlatforms[a].userData==frame.movingPlatforms[b].userData)return false;}
    FootPlacementResult3D result{};result.lod=frame.cameraDistance<=settings_.fullLodDistance?FootPlacementLod3D::Full:frame.cameraDistance<=settings_.reducedLodDistance?FootPlacementLod3D::Reduced:FootPlacementLod3D::Disabled;
    if(result.lod==FootPlacementLod3D::Disabled){left_={};right_={};pelvisOffsetWorld_=0;if(output)*output=result;return true;}
    for(std::size_t i=0;i<jointCount_;++i){scratchPose_.localTransforms[i]=pose.localTransforms[i];scratchPose_.globalMatrices[i]=pose.globalMatrices[i];}
    BuildAnimationPoseGlobalMatrices3D(skeleton,scratchPose_,frame.poseRoot);if(!PoseFinite(scratchPose_))return false;for(std::size_t i=0;i<jointCount_;++i)if(!NearMatrix(scratchPose_.globalMatrices[i],pose.globalMatrices[i]))return false;
    FootState states[2]{left_,right_};float pelvis=pelvisOffsetWorld_;
    const HumanoidJoint3D footSem[2]{HumanoidJoint3D::LeftFoot,HumanoidJoint3D::RightFoot};
    const HumanoidJoint3D upperSem[2]{HumanoidJoint3D::LeftUpperLeg,HumanoidJoint3D::RightUpperLeg};
    const HumanoidJoint3D lowerSem[2]{HumanoidJoint3D::LeftLowerLeg,HumanoidJoint3D::RightLowerLeg};
    const bool query=result.lod==FootPlacementLod3D::Full||frame.frameIndex%settings_.reducedQueryInterval==0||left_.weight<=0||right_.weight<=0;
    for(int side=0;side<2;++side){
        auto& state=states[side];bool lostPlatform=false;if(state.collider&&state.movingPlatform){if(const auto* p=Platform(frame,state.collider,state.userData)){const Matrix4 delta=p->currentLocalToWorld*p->previousWorldToLocal;state.contactWorld=TransformPoint(delta,state.contactWorld);state.targetWorld=TransformPoint(delta,state.targetWorld);state.normalWorld=TransformNormal(CreateNormalMatrix(delta),state.normalWorld);if(!Finite(state.contactWorld)||!Finite(state.targetWorld)||!Finite(state.normalWorld))return false;}else{state.collider={};state.userData=0;state.source=FootContactSource3D::None;state.movingPlatform=false;lostPlatform=true;}}else if(state.collider){const auto* collider=frame.collisionWorld?frame.collisionWorld->Get(state.collider):nullptr;if(!collider||collider->userData!=state.userData){state.collider={};state.userData=0;state.source=FootContactSource3D::None;lostPlatform=true;}}
        bool found=false;FootState hit{};float best=std::numeric_limits<float>::infinity();const Vec3 animated=TransformPoint(frame.modelToWorld,Position(scratchPose_.globalMatrices[map.Joint(footSem[side])])),start=animated+Vec3{0,settings_.probeUp,0};const float travel=settings_.probeUp+settings_.probeDown;
        if(query&&frame.collisionWorld){auto filter=settings_.collisionFilter;filter.includeTriggers=false;const auto h=frame.collisionWorld->SweepSphere(start,settings_.probeRadius,{0,-travel,0},filter);if(h&&Finite(h.position)&&Finite(h.normal)&&h.normal.y>=settings_.minimumNormalY){found=true;best=h.fraction;hit.source=FootContactSource3D::Collider;hit.collider=h.collider;hit.userData=h.userData;hit.contactWorld=h.position;hit.normalWorld=h.normal;hit.targetWorld=h.position+h.normal*settings_.feet[side].soleOffset;hit.movingPlatform=Platform(frame,h.collider,h.userData)!=nullptr;}}
        if(query&&frame.heightField&&frame.heightField->Contains(animated.x,animated.z)){const float y=frame.heightField->SampleHeight(animated.x,animated.z);const Vec3 n=frame.heightField->SampleNormal(animated.x,animated.z);const float centerY=y+(n.y>0?settings_.probeRadius/n.y:0),fraction=(start.y-centerY)/travel;const bool platformTie=fraction==best&&hit.source==FootContactSource3D::Collider&&Platform(frame,hit.collider,hit.userData);if(Finite(y)&&Finite(n)&&n.y>=settings_.minimumNormalY&&fraction>=0&&fraction<=1&&(fraction<best||(fraction==best&&!platformTie))){found=true;hit={};hit.source=FootContactSource3D::HeightField;hit.contactWorld={animated.x,y,animated.z};hit.normalWorld=n;hit.targetWorld=hit.contactWorld+n*settings_.feet[side].soleOffset;}}
        if(found){const float alpha=Clamp(settings_.plantSpeed*frame.deltaTime,0.f,1.f);if(state.weight<=0){state=hit;}else{state.contactWorld=Lerp(state.contactWorld,hit.contactWorld,alpha);state.targetWorld=Lerp(state.targetWorld,hit.targetWorld,alpha);state.normalWorld=Normalize(Lerp(state.normalWorld,hit.normalWorld,alpha));state.source=hit.source;state.collider=hit.collider;state.userData=hit.userData;}state.weight=Move(state.weight,1.f,settings_.plantSpeed*frame.deltaTime);}
        else if(query||lostPlatform){state.weight=Move(state.weight,0.f,settings_.releaseSpeed*frame.deltaTime);if(state.weight==0)state={};}
    }
    float desired=0;for(int side=0;side<2;++side)if(states[side].weight>0){const Vec3 foot=TransformPoint(frame.modelToWorld,Position(scratchPose_.globalMatrices[map.Joint(footSem[side])]));desired=(std::min)(desired,(states[side].targetWorld.y-foot.y)*states[side].weight);}desired=Clamp(desired,-settings_.maximumPelvisDrop,0.f);pelvis=Move(pelvis,desired,settings_.pelvisSpeed*frame.deltaTime);
    const std::size_t hips=map.Joint(HumanoidJoint3D::Hips);Vec3 modelDelta=Direction(frame.worldToModel,{0,pelvis,0}),localDelta=modelDelta;const int hipsParent=skeleton.joints[hips].parent;if(hipsParent>=0&&!InverseLinear(scratchPose_.globalMatrices[std::size_t(hipsParent)],modelDelta,localDelta))return false;scratchPose_.localTransforms[hips].translation=scratchPose_.localTransforms[hips].translation+localDelta;BuildAnimationPoseGlobalMatrices3D(skeleton,scratchPose_,frame.poseRoot);if(!PoseFinite(scratchPose_))return false;
    for(int side=0;side<2;++side)if(states[side].weight>0){const std::size_t foot=map.Joint(footSem[side]),upper=map.Joint(upperSem[side]),lower=map.Joint(lowerSem[side]);const Vec3 currentWorld=TransformPoint(frame.modelToWorld,Position(scratchPose_.globalMatrices[foot]));const Vec3 targetWorld=Lerp(currentWorld,states[side].targetWorld,states[side].weight),targetModel=TransformPoint(frame.worldToModel,targetWorld),upperModel=Position(scratchPose_.globalMatrices[upper]);TwoBoneIkResult3D ik{};if(!SolveTwoBoneIk3D(skeleton,{upper,lower,foot},{targetModel,upperModel+Vec3{0,0,1},1,0},scratchPose_,&ik,frame.poseRoot))return false;++result.ikSolves;
        if(result.lod==FootPlacementLod3D::Full){Vec3 desiredUp=TransformNormal(CreateNormalMatrix(frame.worldToModel),states[side].normalWorld),currentUp=Normalize(Direction(scratchPose_.globalMatrices[foot],settings_.feet[side].soleUpLocal));if(!Finite(desiredUp)||!Finite(currentUp)||Length(desiredUp)<.5f||Length(currentUp)<.5f)return false;Quaternion3D parent=MatrixRotation(skeleton.joints[foot].parent>=0?scratchPose_.globalMatrices[std::size_t(skeleton.joints[foot].parent)]:frame.poseRoot);Quaternion3D global=MatrixRotation(scratchPose_.globalMatrices[foot]),delta=FromTo(currentUp,desiredUp),desiredLocal=Multiply(Conjugate(parent),Multiply(delta,global));auto& t=scratchPose_.localTransforms[foot];t.quaternion=SlerpQuaternion3D(t.usesQuaternion?t.quaternion:QuaternionFromEuler3D(t.rotation),desiredLocal,states[side].weight);t.usesQuaternion=true;BuildAnimationPoseGlobalMatrices3D(skeleton,scratchPose_,frame.poseRoot);if(!PoseFinite(scratchPose_))return false;}
    }
    auto fill=[](const FootState& s){return FootContactResult3D{s.source,s.collider,s.userData,s.contactWorld,s.targetWorld,s.normalWorld,s.weight};};result.left=fill(states[0]);result.right=fill(states[1]);result.pelvisOffsetWorld=pelvis;result.applied=result.ikSolves>0||pelvis!=0;
    for(std::size_t i=0;i<jointCount_;++i){pose.localTransforms[i]=scratchPose_.localTransforms[i];pose.globalMatrices[i]=scratchPose_.globalMatrices[i];}left_=states[0];right_=states[1];pelvisOffsetWorld_=pelvis;if(output)*output=result;return true;
}
void FootPlacement3D::Reset()noexcept{left_={};right_={};pelvisOffsetWorld_=0;}
std::size_t FootPlacement3D::TrimUnusedMemory(){const std::size_t before=MemoryStats().retainedCapacityBytes;AnimationPose3D empty;scratchPose_.localTransforms.swap(empty.localTransforms);scratchPose_.globalMatrices.swap(empty.globalMatrices);jointCount_=0;compatibility_={};Reset();lastReleasedBytes_=before;++trimCount_;return before;}
ReusableMemoryStats FootPlacement3D::MemoryStats()const noexcept{const std::size_t bytes=scratchPose_.localTransforms.capacity()*sizeof(JointTransform3D)+scratchPose_.globalMatrices.capacity()*sizeof(Matrix4);return{bytes,jointCount_?0:bytes,lastReleasedBytes_,trimCount_};}
}
