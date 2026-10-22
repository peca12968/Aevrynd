#include "Aevrynd/CharacterController3D.h"
#include "Aevrynd/CollisionWorld3D.h"
#include "Aevrynd/Geometry3D.h"
#include "Aevrynd/ThirdPersonCameraMotion3D.h"
#include "Aevrynd/ConvexAabbSweep3D.h"
#include "Aevrynd/TerrainUphillLimit3D.h"
#include <algorithm>
#include <bit>
#include <cmath>
#include <limits>

namespace Aevrynd {
namespace {
bool Finite(Vec3 value){return std::isfinite(value.x)&&std::isfinite(value.y)&&std::isfinite(value.z);}
bool ValidTerrainGrade(float value){return std::isfinite(value)&&value>=0&&value<=16;}
bool ValidTerrainPolicies(const CharacterControllerSettings3D& s){return ValidTerrainGrade(s.maximumTerrainUphillGrade)&&ValidTerrainGrade(s.maximumTerrainStandingGrade)&&std::isfinite(s.jumpGraceSeconds)&&s.jumpGraceSeconds>=0&&s.jumpGraceSeconds<=.25f;}
bool HasSolidStandingSupport(Vec3,Vec3,std::span<const BoxCollider3D>,std::span<const ConvexAabbSweep3D* const>,bool&);
struct TerrainStandingSurface {bool inside{},steep{};float height{};Vec3 downhill{},contour{};};
// 使用實際 a/c/b、b/c/d 三角面，不用鄰域平均法線掩蓋尖坡；域外不是接觸。
bool ReadTerrainStandingSurface(const HeightField3D* terrain,Vec3 feet,float maximumGrade,TerrainStandingSurface& out,bool includeGradient=false){
    out={};if(!terrain||(maximumGrade==0&&!includeGradient))return true;
    const auto& s=terrain->Settings();
    if(!Finite(feet)||!Finite(s.origin)||s.width<2||s.depth<2||s.width>1000000||s.depth>1000000||
       !std::isfinite(s.cellSize)||s.cellSize<=0||std::uint64_t(s.width)*s.depth!=terrain->SampleCount())return false;
    const float right=s.origin.x+float(s.width-1)*s.cellSize,front=s.origin.z+float(s.depth-1)*s.cellSize;
    if(!std::isfinite(right)||!std::isfinite(front)||right<=s.origin.x||front<=s.origin.z)return false;
    if(!terrain->Contains(feet.x,feet.z))return true;
    const auto locate=[&](float value,float origin,unsigned count){
        unsigned lo=0,hi=count-1;while(lo<hi){const auto mid=lo+(hi-lo+1)/2;
            if(origin+float(mid)*s.cellSize<=value)lo=mid;else hi=mid-1;}return std::min(lo,count-2);
    };
    const unsigned x=locate(feet.x,s.origin.x,s.width),z=locate(feet.z,s.origin.z,s.depth);
    const double left=s.origin.x+float(x)*s.cellSize,back=s.origin.z+float(z)*s.cellSize;
    const double dx=double(s.origin.x+float(x+1)*s.cellSize)-left,dz=double(s.origin.z+float(z+1)*s.cellSize)-back;
    const double a=terrain->Height(x,z),b=terrain->Height(x+1,z),c=terrain->Height(x,z+1),d=terrain->Height(x+1,z+1);
    if(dx<=0||dz<=0||!std::isfinite(dx)||!std::isfinite(dz)||!std::isfinite(a)||!std::isfinite(b)||!std::isfinite(c)||!std::isfinite(d))return false;
    const bool upper=(double(feet.x)-left)/dx+(double(feet.z)-back)/dz>1;
    const double gx=upper?(d-c)/dx:(b-a)/dx,gz=upper?(d-b)/dz:(c-a)/dz,grade=std::hypot(gx,gz);
    out.inside=true;out.height=terrain->SampleHeight(feet.x,feet.z);out.steep=maximumGrade>0&&grade>maximumGrade;
    if(!std::isfinite(out.height)||!std::isfinite(grade))return false;
    if(out.steep||(includeGradient&&grade>0)){const double scale=std::hypot(1.,grade);
        out.downhill={float(-gx/grade/scale),float(-grade/scale),float(-gz/grade/scale)};
        out.contour={float(-gz/grade),0,float(gx/grade)};
    }
    return Finite(out.downhill)&&Finite(out.contour);
}
struct TerrainPathContact {bool valid{},clear{true};double enter{},leave{1},firstContact{2},minimumGap{std::numeric_limits<double>::infinity()};};
// 足點線段逐一裁切真實三角面；只驗證高度場交集，不能把域外空間當成壞資料。
TerrainPathContact TerrainPathClear(const HeightField3D& terrain,Vec3 from,Vec3 motion){
    const auto& s=terrain.Settings();
    if(!Finite(from)||!Finite(motion)||!Finite(from+motion)||!Finite(s.origin)||s.width<2||s.depth<2||
       s.width>1000000||s.depth>1000000||!std::isfinite(s.cellSize)||s.cellSize<=0||
       std::uint64_t(s.width)*s.depth!=terrain.SampleCount())return {};
    const auto coordinate=[&](float origin,unsigned i){return double(origin+float(i)*s.cellSize);};
    const double right=coordinate(s.origin.x,s.width-1),front=coordinate(s.origin.z,s.depth-1);
    if(!std::isfinite(right)||!std::isfinite(front)||right<=s.origin.x||front<=s.origin.z)return {};
    TerrainPathContact result;result.valid=true;
    const auto clip=[](double p,double d,double lo,double hi,double& begin,double& end){
        if(d==0)return p>=lo&&p<=hi;
        double a=(lo-p)/d,b=(hi-p)/d;if(a>b)std::swap(a,b);
        begin=std::max(begin,a);end=std::min(end,b);return begin<=end;
    };
    if(!clip(from.x,motion.x,s.origin.x,right,result.enter,result.leave)||
       !clip(from.z,motion.z,s.origin.z,front,result.enter,result.leave))return result;
    const auto firstCell=[&](double p,float origin,unsigned count){
        unsigned lo=0,hi=count-1;while(lo<hi){const auto mid=lo+(hi-lo)/2;
            if(coordinate(origin,mid)<p)lo=mid+1;else hi=mid;}return lo?lo-1:0u;
    };
    const auto lastCell=[&](double p,float origin,unsigned count){
        unsigned lo=0,hi=count-1;while(lo<hi){const auto mid=lo+(hi-lo+1)/2;
            if(coordinate(origin,mid)<=p)lo=mid;else hi=mid-1;}return std::min(lo,count-2);
    };
    const double ax=double(from.x)+motion.x*result.enter,bx=double(from.x)+motion.x*result.leave;
    const double az=double(from.z)+motion.z*result.enter,bz=double(from.z)+motion.z*result.leave;
    const auto x0=firstCell(std::min(ax,bx),s.origin.x,s.width),x1=lastCell(std::max(ax,bx),s.origin.x,s.width);
    const auto z0=firstCell(std::min(az,bz),s.origin.z,s.depth),z1=lastCell(std::max(az,bz),s.origin.z,s.depth);
    if(x0>x1||z0>z1||std::uint64_t(x1-x0+1)*(z1-z0+1)>4096)return {};
    for(unsigned z=z0;z<=z1;++z)for(unsigned x=x0;x<=x1;++x){
        const double left=coordinate(s.origin.x,x),back=coordinate(s.origin.z,z);
        const double dx=coordinate(s.origin.x,x+1)-left,dz=coordinate(s.origin.z,z+1)-back;
        if(!std::isfinite(dx)||!std::isfinite(dz)||dx<=0||dz<=0)return {};
        double begin=result.enter,end=result.leave;
        if(!clip(from.x,motion.x,left,left+dx,begin,end)||!clip(from.z,motion.z,back,back+dz,begin,end))continue;
        const double a=terrain.Height(x,z),b=terrain.Height(x+1,z),c=terrain.Height(x,z+1),d=terrain.Height(x+1,z+1);
        if(!std::isfinite(a)||!std::isfinite(b)||!std::isfinite(c)||!std::isfinite(d))return {};
        const double diagonal=(double(from.x)-left)/dx+(double(from.z)-back)/dz-1;
        const double delta=double(motion.x)/dx+double(motion.z)/dz;
        for(unsigned triangle=0;triangle<2;++triangle){
            double low=begin,high=end;const double sign=triangle?-1.:1.,value=sign*diagonal,rate=sign*delta;
            if(rate==0){if(value>0)continue;}else{const double crossing=-value/rate;
                if(rate>0)high=std::min(high,crossing);else low=std::max(low,crossing);}
            if(low>high)continue;
            const double gx=triangle?(d-c)/dx:(b-a)/dx,gz=triangle?(d-b)/dz:(c-a)/dz;
            const double base=triangle?d-gx*dx-gz*dz:a;
            const auto gap=[&](double t){
                const double ground=base+gx*(double(from.x)+motion.x*t-left)+gz*(double(from.z)+motion.z*t-back);
                const double feet=double(from.y)+motion.y*t;
                return feet-ground;
            };
            const double lowGap=gap(low),highGap=gap(high);
            if(!std::isfinite(lowGap)||!std::isfinite(highGap))return {};
            result.minimumGap=std::min({result.minimumGap,lowGap,highGap});
            // 非零長度貼面移動不是離地；只有起點／終點的單點接觸仍保留空中前段。
            if(lowGap<0||highGap<0||(lowGap==0&&highGap==0&&high>low)){
                result.clear=false;
                const double contact=lowGap<=0?low:low+(high-low)*lowGap/(lowGap-highGap);
                result.firstContact=std::min(result.firstContact,contact);
            }
        }
    }
    return result;
}
// 只裁切將實際提交的 segment；足點全程在地形上方時，跳躍或橋面不套坡度。
bool LimitTerrainSegment(const HeightField3D* terrain,float grade,Vec3 from,Vec3& segment,bool& limited){
    if(!terrain||grade==0)return true;
    const auto path=TerrainPathClear(*terrain,from,segment);
    if(!path.valid)return false;
    if(path.clear)return true;
    const auto& s=terrain->Settings();
    const auto restrictXZ=[&](Vec3 p){p.x=Clamp(p.x,s.origin.x,s.origin.x+float(s.width-1)*s.cellSize);
        p.z=Clamp(p.z,s.origin.z,s.origin.z+float(s.depth-1)*s.cellSize);return p;};
    // 先保留空中真正可行的前段；不能因落地後是陡面而把整段水平量歸零。
    const double contact=std::max(path.enter,path.firstContact);
    const Vec3 a=restrictXZ(from+segment*float(contact)),b=restrictXZ(from+segment*float(path.leave));
    const auto result=LimitTerrainUphill3D(*terrain,a,b-a,grade);
    if(!result.valid)return false;
    if(result.limited){const float at=float(contact+(path.leave-contact)*result.fraction);
        const float fraction=at>0?std::nextafter(at,0.f):0.f;
        segment.x*=fraction;segment.z*=fraction;limited=true;}
    return Finite(segment);
}
bool ValidBox(const BoxCollider3D& box){if(!Finite(box.center)||!Finite(box.extents)||box.extents.x<0||box.extents.y<0||box.extents.z<0)return false;return Finite(box.center-box.extents)&&Finite(box.center+box.extents);}
bool ValidExpandedBox(const BoxCollider3D& box,Vec3 extents){return ValidBox(box)&&Finite(box.center-box.extents-extents)&&Finite(box.center+box.extents+extents);}
float NormalizeYaw(float yaw){constexpr double Pi=3.14159265358979323846,TwoPi=Pi*2;double value=std::remainder(double(yaw),TwoPi);if(value<=-Pi)value+=TwoPi;return float(value);}
bool OverlapAabb(Vec3 center,Vec3 extents,const BoxCollider3D& box){const Vec3 minimum=box.center-box.extents-extents,maximum=box.center+box.extents+extents;return center.x>minimum.x&&center.x<maximum.x&&center.y>minimum.y&&center.y<maximum.y&&center.z>minimum.z&&center.z<maximum.z;}
struct SweepCandidate {double fraction{2};unsigned normalMask{};};
SweepCandidate SweepAabb(Vec3 center,Vec3 extents,Vec3 displacement,const BoxCollider3D& box){
    const Vec3 minimum=box.center-box.extents-extents,maximum=box.center+box.extents+extents;double enter=0,leave=1;unsigned mask{};
    const double starts[3]{center.x,center.y,center.z},motions[3]{displacement.x,displacement.y,displacement.z},mins[3]{minimum.x,minimum.y,minimum.z},maxs[3]{maximum.x,maximum.y,maximum.z};
    for(unsigned axis=0;axis<3;++axis){if(motions[axis]==0){if(starts[axis]<mins[axis]||starts[axis]>maxs[axis])return{};continue;}double nearTime=(mins[axis]-starts[axis])/motions[axis],farTime=(maxs[axis]-starts[axis])/motions[axis];unsigned face=1u<<(axis*2+(motions[axis]>0?0:1));if(nearTime>farTime)std::swap(nearTime,farTime);if(nearTime>enter){enter=nearTime;mask=face;}else if(nearTime==enter)mask|=face;leave=(std::min)(leave,farTime);if(enter>leave)return{};}
    if(enter<0||enter>1||!mask)return{};bool entering{};for(unsigned axis=0;axis<3;++axis){if((mask&(3u<<(axis*2)))&&motions[axis]!=0)entering=true;}return entering?SweepCandidate{enter,mask}:SweepCandidate{};
}
bool SameFraction(double left,double right){return left==right||std::nextafter(left,right)==right||std::nextafter(right,left)==left;}
Vec3 RemoveBlocked(Vec3 value,unsigned mask){if(mask&3u)value.x=0;if(mask&12u)value.y=0;if(mask&48u)value.z=0;return value;}
std::uint64_t HashResult(const CharacterDirectDisplacementResult3D& value){std::uint64_t hash=1469598103934665603ull;auto add=[&](std::uint64_t v){for(int i=0;i<8;++i){hash^=(v>>(i*8))&255u;hash*=1099511628211ull;}};auto scalar=[&](float v){add(std::bit_cast<std::uint32_t>(v));};add(std::uint8_t(value.status));add(value.tick);for(Vec3 v:{value.requestedWorld,value.consumedWorld,value.remainderWorld,value.groundCorrectionWorld}){scalar(v.x);scalar(v.y);scalar(v.z);}scalar(value.requestedYawRadians);scalar(value.consumedYawRadians);scalar(value.remainderYawRadians);add(value.grounded);add(value.collisionCount);return hash;}
bool FiniteResult(const CharacterDirectDisplacementResult3D& value){return Finite(value.requestedWorld)&&Finite(value.consumedWorld)&&Finite(value.remainderWorld)&&Finite(value.groundCorrectionWorld)&&std::isfinite(value.requestedYawRadians)&&std::isfinite(value.consumedYawRadians)&&std::isfinite(value.remainderYawRadians);}
}

void CharacterController3D::Reset(Vec3 feetPosition){InvalidateDirectPreview(true);position_=feetPosition;velocity_={};yaw_=0;directLastTick_=0;grounded_=false;stats_={};}

bool CharacterController3D::PrepareDirectDisplacement(std::size_t maximumWorldColliders){if(!maximumWorldColliders||maximumWorldColliders>(1u<<20))return false;try{std::vector<BoxCollider3D> active,staging;active.reserve(maximumWorldColliders);staging.reserve(maximumWorldColliders);collisionScratch_.swap(active);collisionCandidateScratch_.swap(staging);maximumDirectWorldColliders_=maximumWorldColliders;InvalidateDirectPreview(true);return true;}catch(...){return false;}}

bool CharacterController3D::CanResetDirectDisplacement(Vec3 feetPosition,float yawRadians)const noexcept{return directGeneration_!=(std::numeric_limits<uint64_t>::max)()&&Finite(feetPosition)&&std::isfinite(yawRadians)&&std::isfinite(NormalizeYaw(yawRadians));}
bool CharacterController3D::CanTeleportDirectDisplacement(Vec3 feetPosition,float yawRadians,uint64_t previousTick)const noexcept{return CanResetDirectDisplacement(feetPosition,yawRadians)&&previousTick!=(std::numeric_limits<uint64_t>::max)();}
void CharacterController3D::ResetDirectDisplacementUnchecked(Vec3 feetPosition,float yawRadians)noexcept{directPreviewPending_=false;jumpGraceRemaining_=0;position_=feetPosition;velocity_={};yaw_=NormalizeYaw(yawRadians);directLastTick_=0;grounded_=false;stats_={};++directGeneration_;}
void CharacterController3D::TeleportDirectDisplacementUnchecked(Vec3 feetPosition,float yawRadians,uint64_t previousTick)noexcept{directPreviewPending_=false;jumpGraceRemaining_=0;position_=feetPosition;velocity_={};yaw_=NormalizeYaw(yawRadians);directLastTick_=previousTick;grounded_=false;++directGeneration_;}
bool CharacterController3D::Teleport(Vec3 feetPosition,float yawRadians,uint64_t previousTick)noexcept{if(!CanTeleportDirectDisplacement(feetPosition,yawRadians,previousTick))return false;const float yaw=NormalizeYaw(yawRadians);InvalidateDirectPreview(true);position_=feetPosition;velocity_={};yaw_=yaw;directLastTick_=previousTick;grounded_=false;return true;}

bool CharacterController3D::PreviewDirectDisplacementTickInternal(CharacterDirectDisplacementTick3D tick,std::span<const BoxCollider3D> colliders,const HeightField3D* terrain,bool usesWorldScratch,CharacterDirectDisplacementPreviewToken3D& token,CharacterDirectDisplacementResult3D* output)noexcept{
    if(!ValidTerrainPolicies(settings_))return false;
    const auto finiteSettings=[&]{return std::isfinite(settings_.radius)&&settings_.radius>0&&std::isfinite(settings_.height)&&settings_.height>0&&std::isfinite(settings_.groundSnapDistance)&&settings_.groundSnapDistance>=0&&std::isfinite(settings_.maximumDirectDisplacement)&&settings_.maximumDirectDisplacement>0&&settings_.maximumSlideIterations>=1&&settings_.maximumSlideIterations<=3;};
    if(directPreviewPending_||directGeneration_==(std::numeric_limits<uint64_t>::max)()||directPreviewSerial_==(std::numeric_limits<uint64_t>::max)()||!finiteSettings()||!Finite(position_)||!Finite(velocity_)||!std::isfinite(yaw_)||!Finite(Extents())||!Finite(Center())||!Finite(tick.translationWorld)||!std::isfinite(tick.yawDeltaRadians)||directLastTick_==(std::numeric_limits<uint64_t>::max)()||tick.tick!=directLastTick_+1)return false;
    const double lengthSquared=double(tick.translationWorld.x)*tick.translationWorld.x+double(tick.translationWorld.y)*tick.translationWorld.y+double(tick.translationWorld.z)*tick.translationWorld.z;if(!std::isfinite(lengthSquared)||lengthSquared>double(settings_.maximumDirectDisplacement)*settings_.maximumDirectDisplacement)return false;
    const Vec3 validatedExtents=Extents();for(const auto& collider:colliders)if(!ValidExpandedBox(collider,validatedExtents))return false;
    Vec3 candidatePosition=position_,candidateVelocity=velocity_;float candidateYaw=NormalizeYaw(yaw_+tick.yawDeltaRadians);if(!std::isfinite(candidateYaw))return false;bool candidateGrounded=false;uint32_t collisionCount{};const Vec3 extents=Extents(),start=position_,startCenter=Center();
    bool startedOverlapping{};for(const auto& collider:colliders)if(OverlapAabb(startCenter,extents,collider)){startedOverlapping=true;break;}
    Vec3 remaining=tick.translationWorld;bool terrainLimited=false;
    TerrainStandingSurface initialSurface;
    if(!ReadTerrainStandingSurface(terrain,start,settings_.maximumTerrainStandingGrade,initialSurface))return false;
    Vec3 terrainProbe{};bool probeLimited=false;
    if(!LimitTerrainSegment(terrain,settings_.maximumTerrainUphillGrade,start,terrainProbe,probeLimited))return false;
    const auto advance=[&](Vec3 segment){
        if(terrain&&settings_.maximumTerrainStandingGrade>0&&!TerrainPathClear(*terrain,candidatePosition,segment).valid)return false;
        if(!LimitTerrainSegment(terrain,settings_.maximumTerrainUphillGrade,candidatePosition,segment,terrainLimited))return false;
        if(terrainLimited){
            // 裁切水平量但保留重力／直接垂直量，必須重新掃掠這條不同路徑。
            double first=2;unsigned mask{};
            for(const auto& box:colliders){const auto hit=SweepAabb(candidatePosition+Vec3{0,extents.y,0},extents,segment,box);
                if(hit.fraction<first){first=hit.fraction;mask=hit.normalMask;}else if(hit.fraction==first)mask|=hit.normalMask;}
            if(first<1){const float f=first>0?std::nextafter(float(first),0.f):0.f;segment=segment*f;++collisionCount;if(mask&8u)candidateGrounded=true;}
        }
        candidatePosition=candidatePosition+segment;return Finite(candidatePosition);
    };
    if(!startedOverlapping){
        for(unsigned iteration=0;iteration<settings_.maximumSlideIterations;++iteration){if(remaining.x==0&&remaining.y==0&&remaining.z==0)break;double earliest=2;unsigned combinedMask{};uint32_t simultaneous{};const Vec3 center=candidatePosition+Vec3{0,settings_.height*.5f,0};for(const auto& collider:colliders){const auto hit=SweepAabb(center,extents,remaining,collider);if(hit.fraction>1)continue;if(hit.fraction<earliest&&!SameFraction(hit.fraction,earliest)){earliest=hit.fraction;combinedMask=hit.normalMask;simultaneous=1;}else if(SameFraction(hit.fraction,earliest)){combinedMask|=hit.normalMask;++simultaneous;}}
            if(earliest>=1){if(!advance(remaining))return false;remaining={};break;}const double clamped=(std::clamp)(earliest,0.0,1.0);const float reported=float(clamped),fraction=clamped>0?std::nextafter(reported,0.f):0.f;if(!advance(remaining*fraction)||!Finite(candidatePosition+Vec3{0,settings_.height*.5f,0}))return false;if(terrainLimited)break;remaining=RemoveBlocked(remaining*(1.f-fraction),combinedMask);if(!Finite(remaining))return false;collisionCount+=simultaneous;if(combinedMask&8u)candidateGrounded=true;
        }
    }
    if(terrainLimited){++collisionCount;candidateVelocity.x=0;candidateVelocity.z=0;}
    Vec3 groundCorrection{};const bool descending=tick.translationWorld.y<=0;
    // 已接地角色的水平位移要重新確認 endpoint 支撐；離開平台時不可沿用上一 tick 的 grounded。
    if(!startedOverlapping&&descending){float support=-(std::numeric_limits<float>::infinity)();for(const auto& collider:colliders){const float top=collider.center.y+collider.extents.y,gap=candidatePosition.y-top,totalX=extents.x+collider.extents.x,totalZ=extents.z+collider.extents.z;if(!std::isfinite(top)||!std::isfinite(gap)||!std::isfinite(totalX)||!std::isfinite(totalZ))return false;const bool horizontal=std::abs(candidatePosition.x-collider.center.x)<=totalX&&std::abs(candidatePosition.z-collider.center.z)<=totalZ;const float allowance=grounded_?settings_.groundSnapDistance:.00001f;if(horizontal&&gap>=-.00001f&&gap<=allowance)support=(std::max)(support,top);}if(std::isfinite(support)){groundCorrection.y=support-candidatePosition.y;candidatePosition.y=support;candidateGrounded=true;if(candidateVelocity.y<0)candidateVelocity.y=0;}}
    if(terrain&&!startedOverlapping&&terrain->Contains(candidatePosition.x,candidatePosition.z)){
        const float ground=terrain->SampleHeight(candidatePosition.x,candidatePosition.z);if(!std::isfinite(ground))return false;
        TerrainStandingSurface surface;
        if(!ReadTerrainStandingSurface(terrain,candidatePosition,settings_.maximumTerrainStandingGrade,surface))return false;
        const bool penetrated=candidatePosition.y<ground;
        const bool snap=!surface.steep&&grounded_&&descending&&candidatePosition.y-ground<=settings_.groundSnapDistance;
        const bool touching=settings_.maximumTerrainStandingGrade>0&&descending&&candidatePosition.y==ground;
        if(penetrated||snap||touching){
            groundCorrection.y+=ground-candidatePosition.y;candidatePosition.y=ground;
            bool solid=false;
            if(settings_.maximumTerrainStandingGrade>0&&!HasSolidStandingSupport(candidatePosition,extents,colliders,{},solid))return false;
            candidateGrounded=!surface.steep||solid;
            // direct 只處理位移與接觸；陡面保留速度，不捏造無 dt 的重力積分。
            if(candidateGrounded&&candidateVelocity.y<0)candidateVelocity.y=0;
        }
    }
    if(!Finite(candidatePosition)||!Finite(candidatePosition+Vec3{0,settings_.height*.5f,0})||!Finite(candidateVelocity)||!Finite(groundCorrection))return false;CharacterDirectDisplacementResult3D result{};result.tick=tick.tick;result.requestedWorld=tick.translationWorld;result.consumedWorld=startedOverlapping?Vec3{}:candidatePosition-start-groundCorrection;result.remainderWorld=result.requestedWorld-result.consumedWorld;result.groundCorrectionWorld=groundCorrection;result.requestedYawRadians=tick.yawDeltaRadians;result.consumedYawRadians=tick.yawDeltaRadians;result.remainderYawRadians=0;result.grounded=candidateGrounded;result.collisionCount=collisionCount;if(!FiniteResult(result))return false;
    const bool translationBlocked=result.remainderWorld.x!=0||result.remainderWorld.y!=0||result.remainderWorld.z!=0;if(startedOverlapping)result.status=CharacterDirectDisplacementStatus3D::StartedOverlapping;else if(translationBlocked)result.status=CharacterDirectDisplacementStatus3D::Blocked;else if(tick.translationWorld.x==0&&tick.translationWorld.y==0&&tick.translationWorld.z==0&&tick.yawDeltaRadians==0&&groundCorrection.y==0)result.status=CharacterDirectDisplacementStatus3D::NoMotion;else result.status=CharacterDirectDisplacementStatus3D::Applied;result.deterministicHash=HashResult(result);
    auto candidateStats=stats_;++candidateStats.directTicks;if(result.status==CharacterDirectDisplacementStatus3D::Blocked)++candidateStats.directBlocked;if(startedOverlapping)++candidateStats.directOverlaps;candidateStats.collisions+=collisionCount;if(candidateGrounded&&!grounded_)++candidateStats.landings;if(groundCorrection.y!=0)++candidateStats.terrainContacts;
    directPreviewPosition_=candidatePosition;directPreviewVelocity_=candidateVelocity;directPreviewYaw_=candidateYaw;directPreviewGrounded_=candidateGrounded;directPreviewStats_=candidateStats;directPreviewResult_=result;directPreviewUsesWorldScratch_=usesWorldScratch;directPreviewBaseStateHash_=StateHash();directPreviewPending_=true;++directPreviewSerial_;CharacterDirectDisplacementPreviewToken3D candidate{};candidate.owner=reinterpret_cast<std::uintptr_t>(this);candidate.generation=directGeneration_;candidate.serial=directPreviewSerial_;candidate.tick=tick.tick;candidate.baseStateHash=directPreviewBaseStateHash_;candidate.candidateHash=result.deterministicHash;candidate.valid=true;token=candidate;if(output)*output=result;return true;
}

bool CharacterController3D::PreviewDirectDisplacementTick(CharacterDirectDisplacementTick3D tick,std::span<const BoxCollider3D> colliders,CharacterDirectDisplacementPreviewToken3D& token,CharacterDirectDisplacementResult3D* output)noexcept{return PreviewDirectDisplacementTickInternal(tick,colliders,nullptr,false,token,output);}
bool CharacterController3D::PreviewDirectDisplacementTick(CharacterDirectDisplacementTick3D tick,std::span<const BoxCollider3D> colliders,const HeightField3D& terrain,CharacterDirectDisplacementPreviewToken3D& token,CharacterDirectDisplacementResult3D* output)noexcept{return PreviewDirectDisplacementTickInternal(tick,colliders,&terrain,false,token,output);}
bool CharacterController3D::PreviewDirectDisplacementTick(CharacterDirectDisplacementTick3D tick,const CollisionWorld3D& world,CollisionQueryFilter3D filter,CharacterDirectDisplacementPreviewToken3D& token,CharacterDirectDisplacementResult3D* output)noexcept{if(directPreviewPending_||!maximumDirectWorldColliders_||world.Stats().colliders>maximumDirectWorldColliders_)return false;world.CollectBoxes(collisionCandidateScratch_,filter);return PreviewDirectDisplacementTickInternal(tick,collisionCandidateScratch_,nullptr,true,token,output);}
bool CharacterController3D::PreviewDirectDisplacementTick(CharacterDirectDisplacementTick3D tick,const CollisionWorld3D& world,const HeightField3D& terrain,CollisionQueryFilter3D filter,CharacterDirectDisplacementPreviewToken3D& token,CharacterDirectDisplacementResult3D* output)noexcept{if(directPreviewPending_||!maximumDirectWorldColliders_||world.Stats().colliders>maximumDirectWorldColliders_)return false;world.CollectBoxes(collisionCandidateScratch_,filter);return PreviewDirectDisplacementTickInternal(tick,collisionCandidateScratch_,&terrain,true,token,output);}
bool CharacterController3D::CanCommitDirectDisplacementPreview(const CharacterDirectDisplacementPreviewToken3D& token)const noexcept{return directPreviewPending_&&token.valid&&token.owner==reinterpret_cast<std::uintptr_t>(this)&&token.generation==directGeneration_&&token.serial==directPreviewSerial_&&token.tick==directPreviewResult_.tick&&token.baseStateHash==directPreviewBaseStateHash_&&token.candidateHash==directPreviewResult_.deterministicHash;}
bool CharacterController3D::CancelDirectDisplacementPreview(CharacterDirectDisplacementPreviewToken3D& token)noexcept{if(!directPreviewPending_||!token.valid||token.owner!=reinterpret_cast<std::uintptr_t>(this)||token.serial!=directPreviewSerial_)return false;directPreviewPending_=false;token={};return true;}
void CharacterController3D::CommitDirectDisplacementPreviewUnchecked(CharacterDirectDisplacementPreviewToken3D& token)noexcept{jumpGraceRemaining_=0;position_=directPreviewPosition_;velocity_=directPreviewVelocity_;yaw_=directPreviewYaw_;grounded_=directPreviewGrounded_;directLastTick_=directPreviewResult_.tick;stats_=directPreviewStats_;if(directPreviewUsesWorldScratch_)collisionScratch_.swap(collisionCandidateScratch_);directPreviewPending_=false;++directGeneration_;token={};}
void CharacterController3D::InvalidateDirectPreview(bool committedMutation)noexcept{directPreviewPending_=false;if(committedMutation){jumpGraceRemaining_=0;if(directGeneration_!=(std::numeric_limits<uint64_t>::max)())++directGeneration_;}}
bool CharacterController3D::ApplyDirectDisplacementTick(CharacterDirectDisplacementTick3D tick,std::span<const BoxCollider3D> colliders,CharacterDirectDisplacementResult3D* output)noexcept{InvalidateDirectPreview(false);CharacterDirectDisplacementPreviewToken3D token{};CharacterDirectDisplacementResult3D result{};if(!PreviewDirectDisplacementTick(tick,colliders,token,&result)||!CanCommitDirectDisplacementPreview(token)){CancelDirectDisplacementPreview(token);return false;}CommitDirectDisplacementPreviewUnchecked(token);if(output)*output=result;return true;}
bool CharacterController3D::ApplyDirectDisplacementTick(CharacterDirectDisplacementTick3D tick,std::span<const BoxCollider3D> colliders,const HeightField3D& terrain,CharacterDirectDisplacementResult3D* output)noexcept{InvalidateDirectPreview(false);CharacterDirectDisplacementPreviewToken3D token{};CharacterDirectDisplacementResult3D result{};if(!PreviewDirectDisplacementTick(tick,colliders,terrain,token,&result)||!CanCommitDirectDisplacementPreview(token)){CancelDirectDisplacementPreview(token);return false;}CommitDirectDisplacementPreviewUnchecked(token);if(output)*output=result;return true;}
bool CharacterController3D::ApplyDirectDisplacementTick(CharacterDirectDisplacementTick3D tick,const CollisionWorld3D& world,CollisionQueryFilter3D filter,CharacterDirectDisplacementResult3D* output)noexcept{InvalidateDirectPreview(false);CharacterDirectDisplacementPreviewToken3D token{};CharacterDirectDisplacementResult3D result{};if(!PreviewDirectDisplacementTick(tick,world,filter,token,&result)||!CanCommitDirectDisplacementPreview(token)){CancelDirectDisplacementPreview(token);return false;}CommitDirectDisplacementPreviewUnchecked(token);if(output)*output=result;return true;}
bool CharacterController3D::ApplyDirectDisplacementTick(CharacterDirectDisplacementTick3D tick,const CollisionWorld3D& world,const HeightField3D& terrain,CollisionQueryFilter3D filter,CharacterDirectDisplacementResult3D* output)noexcept{InvalidateDirectPreview(false);CharacterDirectDisplacementPreviewToken3D token{};CharacterDirectDisplacementResult3D result{};if(!PreviewDirectDisplacementTick(tick,world,terrain,filter,token,&result)||!CanCommitDirectDisplacementPreview(token)){CancelDirectDisplacementPreview(token);return false;}CommitDirectDisplacementPreviewUnchecked(token);if(output)*output=result;return true;}

ReusableMemoryStats CharacterController3D::MemoryStats()const noexcept{const std::size_t bytes=(collisionScratch_.capacity()+collisionCandidateScratch_.capacity())*sizeof(BoxCollider3D);return{bytes,bytes,lastReleasedBytes_,trimCount_};}
std::size_t CharacterController3D::TrimUnusedMemory(){InvalidateDirectPreview(true);const std::size_t before=MemoryStats().retainedCapacityBytes;std::vector<BoxCollider3D>{}.swap(collisionScratch_);std::vector<BoxCollider3D>{}.swap(collisionCandidateScratch_);maximumDirectWorldColliders_=0;lastReleasedBytes_=before;++trimCount_;return lastReleasedBytes_;}
uint64_t CharacterController3D::StateHash()const noexcept{uint64_t hash=1469598103934665603ull;auto add=[&](uint64_t value){hash^=value+0x9e3779b97f4a7c15ull+(hash<<6)+(hash>>2);hash*=1099511628211ull;};auto scalar=[&](float value){add(std::bit_cast<uint32_t>(value));};add(directGeneration_);scalar(position_.x);scalar(position_.y);scalar(position_.z);scalar(velocity_.x);scalar(velocity_.y);scalar(velocity_.z);scalar(yaw_);add(directLastTick_);add(grounded_);add(stats_.steps);add(stats_.collisions);add(stats_.jumps);add(stats_.landings);add(stats_.terrainContacts);add(stats_.directTicks);add(stats_.directBlocked);add(stats_.directOverlaps);add(maximumDirectWorldColliders_);add(collisionScratch_.capacity());add(collisionScratch_.size());if(settings_.maximumTerrainUphillGrade!=0){add(0x5445525241494e31ull);scalar(settings_.maximumTerrainUphillGrade);}if(settings_.maximumTerrainStandingGrade!=0){add(0x5445525253544e31ull);scalar(settings_.maximumTerrainStandingGrade);}if(settings_.jumpGraceSeconds!=0){add(0x4A554D5047524143ull);scalar(settings_.jumpGraceSeconds);add(std::bit_cast<uint64_t>(jumpGraceRemaining_));}return hash;}

void CharacterController3D::Step(CharacterControl3D control,std::span<const BoxCollider3D> colliders,float deltaTime){
    if(!ValidTerrainPolicies(settings_))return;
    if(settings_.jumpGraceSeconds>0){(void)StepConvexInternal(control,colliders,{},nullptr,deltaTime);return;}
    InvalidateDirectPreview(true);
    const float dt=Clamp(deltaTime,0.f,.1f);Vec3 movement{control.movement.x,0,control.movement.z};const float length=Length(movement);if(length>1)movement=movement*(1.f/length);const float speedMultiplier=std::isfinite(control.speedMultiplier)&&control.speedMultiplier>0.f?Clamp(control.speedMultiplier,.25f,4.f):1.f;velocity_.x=movement.x*settings_.moveSpeed*speedMultiplier;velocity_.z=movement.z*settings_.moveSpeed*speedMultiplier;
    const bool wasGrounded=grounded_;if(control.jump&&grounded_){velocity_.y=settings_.jumpSpeed;grounded_=false;++stats_.jumps;}
    const float maximumDisplacement=(std::max)({std::abs(velocity_.x)*dt,std::abs(velocity_.z)*dt,settings_.maximumFallSpeed*dt});
    const float maximumSubstep=(std::max)(.05f,settings_.radius*.5f);
    const unsigned substeps=(std::max)(1u,(std::min)(64u,static_cast<unsigned>(std::ceil(maximumDisplacement/maximumSubstep))));
    const float substepDt=substeps?dt/float(substeps):0.f;const Vec3 extents=Extents();
    for(unsigned substep=0;substep<substeps;++substep){
      velocity_.y=(std::max)(-settings_.maximumFallSpeed,velocity_.y-settings_.gravity*substepDt);grounded_=false;
      // 決定性子步進後再分軸解算，4x 與 dt 上限也不能跨過薄牆。
      for(unsigned axis:{1u,0u,2u}){float* coordinate=axis==0?&position_.x:axis==1?&position_.y:&position_.z;float* speed=axis==0?&velocity_.x:axis==1?&velocity_.y:&velocity_.z;const float displacement=*speed*substepDt;if(std::abs(displacement)<.000001f)continue;*coordinate+=displacement;for(const BoxCollider3D& collider:colliders){Vec3 center=Center(),delta=center-collider.center;if(axis!=1&&center.y-extents.y>=collider.center.y+collider.extents.y-.001f)continue;if(std::abs(delta.x)>=extents.x+collider.extents.x-.0001f||std::abs(delta.y)>=extents.y+collider.extents.y-.0001f||std::abs(delta.z)>=extents.z+collider.extents.z-.0001f)continue;const float colliderCoordinate=axis==0?collider.center.x:axis==1?collider.center.y:collider.center.z,colliderExtent=axis==0?collider.extents.x:axis==1?collider.extents.y:collider.extents.z,characterExtent=axis==0?extents.x:axis==1?extents.y:extents.z;float corrected=displacement>0?colliderCoordinate-colliderExtent-characterExtent:colliderCoordinate+colliderExtent+characterExtent;if(axis==1)corrected-=extents.y;*coordinate=corrected;if(axis==1&&displacement<=0)grounded_=true;*speed=0;++stats_.collisions;}}
    }
    if(grounded_&&!wasGrounded)++stats_.landings;++stats_.steps;
}

void CharacterController3D::Step(CharacterControl3D control,const CollisionWorld3D& world,float deltaTime,uint32_t layerMask,uint32_t characterLayer){if(!ValidTerrainPolicies(settings_))return;world.CollectBoxes(collisionScratch_,{layerMask,characterLayer,false});Step(control,collisionScratch_,deltaTime);}
// 實體橋面已提供支撐時，不可再向下吸附到橋下的高度場。
void CharacterController3D::Step(CharacterControl3D control,std::span<const BoxCollider3D> colliders,const HeightField3D& terrain,float deltaTime){
    if(!ValidTerrainPolicies(settings_))return;
    if(settings_.maximumTerrainUphillGrade>0||settings_.maximumTerrainStandingGrade>0||settings_.jumpGraceSeconds>0){(void)StepConvexInternal(control,colliders,{},&terrain,deltaTime);return;}
    const bool wasGrounded=grounded_;Step(control,colliders,deltaTime);if(!terrain.Contains(position_.x,position_.z))return;const float ground=terrain.SampleHeight(position_.x,position_.z);if(velocity_.y<=0&&(position_.y<=ground||(!grounded_&&wasGrounded&&position_.y-ground<=settings_.groundSnapDistance))){if(!wasGrounded&&!grounded_)++stats_.landings;position_.y=ground;velocity_.y=0;grounded_=true;++stats_.terrainContacts;}
}
void CharacterController3D::Step(CharacterControl3D control,const CollisionWorld3D& world,const HeightField3D& terrain,float deltaTime,uint32_t layerMask,uint32_t characterLayer){
    if(!ValidTerrainPolicies(settings_))return;
    if(settings_.maximumTerrainUphillGrade>0||settings_.maximumTerrainStandingGrade>0||settings_.jumpGraceSeconds>0){
        std::vector<BoxCollider3D> boxes;world.CollectBoxes(boxes,{layerMask,characterLayer,false});
        if(StepConvexInternal(control,boxes,{},&terrain,deltaTime)&&
           !((settings_.maximumTerrainStandingGrade>0||settings_.jumpGraceSeconds>0)&&deltaTime==0))collisionScratch_.swap(boxes);
        return;
    }
    world.CollectBoxes(collisionScratch_,{layerMask,characterLayer,false});Step(control,collisionScratch_,terrain,deltaTime);
}

namespace {
constexpr float ConvexSkin=.0001f,WalkableY=.65f;
enum BodyContactOrigin:unsigned char { StaticContact=1,DynamicSideContact=2,DynamicAboveContact=4,DynamicBelowContact=8 };
std::array<std::uint64_t,2> DynamicBodyMask(std::size_t i){std::array<std::uint64_t,2> mask{};if(i<128)mask[i/64]=std::uint64_t{1}<<(i%64);return mask;}
struct BodySweep {
    double fraction{2};
    std::array<Vec3,128> normals{};
    std::array<unsigned char,128> origins{};
    std::array<std::array<std::uint64_t,2>,128> bodies{};
    std::array<std::uint64_t,2> touchedBodies{};
    std::size_t count{};
    bool dynamicSide{},staticWalkable{};
    CharacterDynamicContactState3D dynamicContact{};
    bool Add(Vec3 n,unsigned char origin=StaticContact,std::array<std::uint64_t,2> bodyMask={}){
        touchedBodies[0]|=bodyMask[0];touchedBodies[1]|=bodyMask[1];
        staticWalkable=staticWalkable||((origin&StaticContact)!=0&&n.y>=WalkableY);
        dynamicSide=dynamicSide||(origin&DynamicSideContact)!=0;
        dynamicContact.blockedSide=dynamicContact.blockedSide||(origin&DynamicSideContact)!=0;
        dynamicContact.blockedAbove=dynamicContact.blockedAbove||(origin&DynamicAboveContact)!=0;
        dynamicContact.blockedBelow=dynamicContact.blockedBelow||(origin&DynamicBelowContact)!=0;
        for(std::size_t i=0;i<count;++i)if(normals[i].x==n.x&&normals[i].y==n.y&&normals[i].z==n.z){origins[i]|=origin;bodies[i][0]|=bodyMask[0];bodies[i][1]|=bodyMask[1];return true;}
        // 已有同號座標軸約束的非負組合必然冗餘；精確比較，不用角度容差合併斜面。
        bool x=n.x==0,y=n.y==0,z=n.z==0;
        for(std::size_t i=0;i<count;++i){const auto a=normals[i];
            x=x||(a.y==0&&a.z==0&&((a.x==1&&n.x>0)||(a.x==-1&&n.x<0)));
            y=y||(a.x==0&&a.z==0&&((a.y==1&&n.y>0)||(a.y==-1&&n.y<0)));
            z=z||(a.x==0&&a.y==0&&((a.z==1&&n.z>0)||(a.z==-1&&n.z<0)));
        }
        if(x&&y&&z)return true;
        if(count==normals.size())return false;normals[count]=n;origins[count]=origin;bodies[count]=bodyMask;++count;return true;
    }
};
void RecordDynamicContact(const BodySweep& hit,CharacterDynamicContactState3D* out){
    if(!out)return;
    out->blockedSide=out->blockedSide||hit.dynamicContact.blockedSide;
    out->blockedAbove=out->blockedAbove||hit.dynamicContact.blockedAbove;
    out->blockedBelow=out->blockedBelow||hit.dynamicContact.blockedBelow;
}
bool DynamicProjectionFeasible(const BodySweep& hit,Vec3 motion,Vec3 velocity){
    constexpr unsigned char dynamicMask=DynamicSideContact|DynamicAboveContact|DynamicBelowContact;
    for(std::size_t i=0;i<hit.count;++i)if((hit.origins[i]&dynamicMask)!=0&&
        (Dot(hit.normals[i],motion)<0||Dot(hit.normals[i],velocity)<0))return false;
    return Finite(motion)&&Finite(velocity);
}

// 只修復既有投影的向內浮點殘差；原可行值不動，判準不加容差。
bool CompleteDynamicProjection(const BodySweep& hit,Vec3& motion,Vec3& velocity){
    if(hit.count>hit.normals.size()||!Finite(motion)||!Finite(velocity))return false;
    for(std::size_t i=0;i<hit.count;++i)if(!Finite(hit.normals[i]))return false;
    if(DynamicProjectionFeasible(hit,motion,velocity))return true;
    Vec3 candidateMotion=motion,candidateVelocity=velocity;
    const auto doubleDot=[](Vec3 n,Vec3 value){return
        double(n.x)*value.x+double(n.y)*value.y+double(n.z)*value.z;};
    const auto feasible=[&](Vec3 value){
        if(!Finite(value))return false;
        for(std::size_t i=0;i<hit.count;++i){
            const float f=Dot(hit.normals[i],value);const double d=doubleDot(hit.normals[i],value);
            if(!std::isfinite(f)||!std::isfinite(d)||f<0||d<0)return false;
        }
        return true;
    };
    const auto repair=[&](Vec3 n,Vec3& value){
        const float f=Dot(n,value);const double d=doubleDot(n,value);
        if(!std::isfinite(f)||!std::isfinite(d))return false;
        if(f>=0&&d>=0)return true;
        float coefficient=n.x;float* component=&value.x;
        if(std::abs(n.y)>std::abs(coefficient)){coefficient=n.y;component=&value.y;}
        if(std::abs(n.z)>std::abs(coefficient)){coefficient=n.z;component=&value.z;}
        if(coefficient==0)return false;
        const float toward=coefficient>0?(std::numeric_limits<float>::infinity)():-(std::numeric_limits<float>::infinity)();
        const float next=std::nextafter(*component,toward);
        if(!std::isfinite(next)||next==*component)return false;
        *component=next;return true;
    };
    // 固定輪數只是有界嘗試；全部原平面（含靜態）都嚴格可行才原子提交。
    for(unsigned pass=0;pass<16;++pass){
        for(std::size_t i=0;i<hit.count;++i)
            if(!repair(hit.normals[i],candidateMotion)||!repair(hit.normals[i],candidateVelocity))return false;
        if(feasible(candidateMotion)&&feasible(candidateVelocity)&&
            DynamicProjectionFeasible(hit,candidateMotion,candidateVelocity)){
            motion=candidateMotion;velocity=candidateVelocity;return true;
        }
    }
    return false;
}

// 舊政策只解側面；顯式 BlockOnly 解上下阻擋但不提供承重或起始去穿透。
bool DynamicContactAllowed(const BoxCylinderContact3D& contact,CharacterDynamicStepStatus3D* status,
    CharacterDynamicVerticalResponse3D response=CharacterDynamicVerticalResponse3D::Reject) {
    CharacterDynamicStepStatus3D failure=CharacterDynamicStepStatus3D::Applied;
    switch(contact.kind){
    case BoxCylinderContactKind3D::Clear:case BoxCylinderContactKind3D::SideEntry:return true;
    case BoxCylinderContactKind3D::VerticalEntry:
        if(response==CharacterDynamicVerticalResponse3D::BlockOnly)return true;
        failure=CharacterDynamicStepStatus3D::UnresolvedVerticalEntry;break;
    case BoxCylinderContactKind3D::SimultaneousEntry:
        if(response==CharacterDynamicVerticalResponse3D::BlockOnly)return true;
        failure=CharacterDynamicStepStatus3D::UnresolvedSimultaneousEntry;break;
    case BoxCylinderContactKind3D::StartedOverlapping:failure=CharacterDynamicStepStatus3D::UnresolvedStartedOverlap;break;
    default:failure=CharacterDynamicStepStatus3D::UnresolvedNumerics;break;
    }
    if(status)*status=failure;return false;
}
bool ReadDynamicContact(Vec3 c,Vec3 e,Vec3 d,const DynamicCylinder3D& body,
    BoxCylinderContact3D& out,CharacterDynamicStepStatus3D* status,
    CharacterDynamicVerticalResponse3D response=CharacterDynamicVerticalResponse3D::Reject) {
    if(!QueryBoxCylinderContact3D(c,e,d,body,out)){
        if(status)*status=CharacterDynamicStepStatus3D::UnresolvedNumerics;return false;
    }
    return DynamicContactAllowed(out,status,response);
}
bool DynamicBodiesClear(Vec3 c,Vec3 e,std::span<const DynamicCylinder3D> bodies,CharacterDynamicStepStatus3D* status) {
    for(const auto& body:bodies){BoxCylinderContact3D q;
        if(!ReadDynamicContact(c,e,{},body,q,status))return false;
        if(q.kind!=BoxCylinderContactKind3D::Clear){if(status)*status=CharacterDynamicStepStatus3D::UnresolvedNumerics;return false;}
    }
    return true;
}
// 靜態跨階／吸地仍只由原靜態場景決定；額外路段不可跳過動態體積。
bool DynamicAuxiliaryClear(Vec3 c,Vec3 e,Vec3 d,std::span<const DynamicCylinder3D> bodies,
    CharacterDynamicStepStatus3D* status,bool& blocked,
    CharacterDynamicVerticalResponse3D response=CharacterDynamicVerticalResponse3D::Reject) {
    blocked=false;
    for(const auto& body:bodies){BoxCylinderContact3D q;
        if(!ReadDynamicContact(c,e,d,body,q,status,response))return false;
        blocked=blocked||q.kind!=BoxCylinderContactKind3D::Clear;
    }
    return true;
}
// 可選跨階不是實際已採用的路徑：有效接觸只取消此選項，數值失敗仍拒絕交易。
bool DynamicOptionalStepClear(Vec3 c,Vec3 e,Vec3 d,std::span<const DynamicCylinder3D> bodies,
    CharacterDynamicStepStatus3D* status,bool& blocked) {
    blocked=false;
    for(const auto& body:bodies){BoxCylinderContact3D q;
        if(!QueryBoxCylinderContact3D(c,e,d,body,q)){
            if(status)*status=CharacterDynamicStepStatus3D::UnresolvedNumerics;return false;
        }
        switch(q.kind){
        case BoxCylinderContactKind3D::Clear:break;
        case BoxCylinderContactKind3D::SideEntry:case BoxCylinderContactKind3D::VerticalEntry:
        case BoxCylinderContactKind3D::SimultaneousEntry:case BoxCylinderContactKind3D::StartedOverlapping:
            blocked=true;break;
        default:if(status)*status=CharacterDynamicStepStatus3D::UnresolvedNumerics;return false;
        }
        // 已被阻擋也仍核對後續來源，不能讓來源順序吞掉數值錯誤。
    }
    return true;
}
// 新凸形路徑的方盒也只阻擋進入內部；沿地板頂面滑行不應撞到接縫側面。
bool EntersBoxInterior(Vec3 c,Vec3 e,Vec3 d,const BoxCollider3D& box,double enter){
    const Vec3 lo=box.center-box.extents-e,hi=box.center+box.extents+e;
    const double p[3]{c.x,c.y,c.z},v[3]{d.x,d.y,d.z},low[3]{lo.x,lo.y,lo.z},high[3]{hi.x,hi.y,hi.z};
    double leave=std::numeric_limits<double>::infinity();
    for(unsigned axis=0;axis<3;++axis){
        if(v[axis]==0){if(p[axis]<=low[axis]||p[axis]>=high[axis])return false;}
        else leave=std::min(leave,std::max((low[axis]-p[axis])/v[axis],(high[axis]-p[axis])/v[axis]));
    }
    return leave>enter&&leave>0;
}
// 完整入口驗證後，僅對本次不可變 span 使用保守的逐查詢窗口。
struct StepQueryWindowContext {
    std::span<const BoxCollider3D> source;
    std::array<BoxCollider3D,256> filtered;
    std::size_t count{};
    Vec3 extents{};
    double lowX{},highX{},lowZ{},highZ{};
};
thread_local const StepQueryWindowContext* activeStepQueryWindow{};
class StepQueryWindowScope final {
    const StepQueryWindowContext* previous_;
    StepQueryWindowContext context_;
public:
    StepQueryWindowScope()noexcept:previous_(activeStepQueryWindow){activeStepQueryWindow=nullptr;}
    StepQueryWindowScope(const StepQueryWindowScope&)=delete;
    StepQueryWindowScope& operator=(const StepQueryWindowScope&)=delete;
    ~StepQueryWindowScope()noexcept {activeStepQueryWindow=previous_;}
    void Prepare(std::span<const BoxCollider3D> source,Vec3 feet,Vec3 extents,float dt)noexcept {
        // 未適用或容量不足不能繼承外層已準備窗口；完整退回每個查詢原 span。
        activeStepQueryWindow=nullptr;
        if(source.size()<=64||!Finite(feet)||!Finite(extents)||!std::isfinite(dt)||dt<=0||dt>.1f||
           std::abs(feet.x)>100000.f||std::abs(feet.z)>100000.f||
           extents.x<=0||extents.y<=0||extents.z<=0||extents.x>4.f||extents.y>4.f||extents.z>4.f)return;
        constexpr double inner=4.;
        const double magnitude=std::max({std::abs(double(feet.x)),std::abs(double(feet.z)),
            double(extents.x),double(extents.z),inner,1.});
        const double padding=1.+2048.*double(std::numeric_limits<float>::epsilon())*magnitude;
        const double lowX=double(feet.x)-inner,highX=double(feet.x)+inner;
        const double lowZ=double(feet.z)-inner,highZ=double(feet.z)+inner;
        const double outerLowX=lowX-padding,outerHighX=highX+padding;
        const double outerLowZ=lowZ-padding,outerHighZ=highZ+padding;
        std::size_t count=0;
        for(const auto& box:source){
            // 展開順序與原 SweepAabb 相同；完整入口已驗證，這裡仍保守拒用非法邊界。
            const Vec3 low=box.center-box.extents-extents,high=box.center+box.extents+extents;
            if(!Finite(low)||!Finite(high))return;
            if(double(low.x)>outerHighX||double(high.x)<outerLowX||
               double(low.z)>outerHighZ||double(high.z)<outerLowZ)continue;
            if(count==context_.filtered.size())return;
            context_.filtered[count++]=box;
        }
        context_.source=source;context_.count=count;context_.extents=extents;
        context_.lowX=lowX;context_.highX=highX;context_.lowZ=lowZ;context_.highZ=highZ;
        activeStepQueryWindow=&context_;
    }
};
std::span<const BoxCollider3D> StepQueryBoxes(Vec3 c,Vec3 e,Vec3 d,std::span<const BoxCollider3D> source)noexcept {
    const auto* context=activeStepQueryWindow;
    if(!context||source.data()!=context->source.data()||source.size()!=context->source.size()||
       !Finite(c)||!Finite(e)||!Finite(d))return source;
    const auto same=[](float a,float b){return std::bit_cast<std::uint32_t>(a)==std::bit_cast<std::uint32_t>(b);};
    if(!same(e.x,context->extents.x)||!same(e.y,context->extents.y)||!same(e.z,context->extents.z))return source;
    const double endX=double(c.x)+double(d.x),endZ=double(c.z)+double(d.z);
    if(!std::isfinite(endX)||!std::isfinite(endZ)||
       double(c.x)<context->lowX||double(c.x)>context->highX||endX<context->lowX||endX>context->highX||
       double(c.z)<context->lowZ||double(c.z)>context->highZ||endZ<context->lowZ||endZ>context->highZ)return source;
    return {context->filtered.data(),context->count};
}
bool BodyClear(Vec3 c,Vec3 e,std::span<const BoxCollider3D> boxes,std::span<const ConvexAabbSweep3D* const> hulls){
    if(!Finite(c)||!Finite(e))return false;
    boxes=StepQueryBoxes(c,e,{},boxes);
    for(const auto& box:boxes)if(!ValidExpandedBox(box,e)||OverlapAabb(c,e,box))return false;
    for(const auto* hull:hulls){ConvexAabbSweep3D::Result q;
        if(!hull||!hull->Sweep(c,e,{},q)||q.status==ConvexAabbSweepStatus3D::StartedOverlapping)return false;}
    return true;
}
bool SweepBody(Vec3 c,Vec3 e,Vec3 d,std::span<const BoxCollider3D> boxes,std::span<const ConvexAabbSweep3D* const> hulls,BodySweep& out,
    std::span<const DynamicCylinder3D> dynamic={},CharacterDynamicStepStatus3D* dynamicStatus=nullptr,
    CharacterDynamicVerticalResponse3D response=CharacterDynamicVerticalResponse3D::Reject){
    boxes=StepQueryBoxes(c,e,d,boxes);
    BodySweep result;
    const auto include=[&](double t){
        if(t<result.fraction&&!SameFraction(t,result.fraction)){
            result.fraction=t;result.count=0;result.dynamicSide=false;result.staticWalkable=false;result.dynamicContact={};result.touchedBodies={};
        }
        return SameFraction(t,result.fraction);
    };
    for(const auto& box:boxes){
        const auto q=SweepAabb(c,e,d,box);if(q.fraction>1||!EntersBoxInterior(c,e,d,box,q.fraction)||!include(q.fraction))continue;
        for(unsigned axis=0;axis<3;++axis)for(unsigned side=0;side<2;++side)if(q.normalMask&(1u<<(axis*2+side))){
            Vec3 n{};const float sign=side?1.f:-1.f;if(axis==0)n.x=sign;else if(axis==1)n.y=sign;else n.z=sign;
            if(!result.Add(n))return false;
        }
    }
    for(const auto* hull:hulls){
        ConvexAabbSweep3D::Result q;if(!hull||!hull->Sweep(c,e,d,q)||q.status==ConvexAabbSweepStatus3D::StartedOverlapping)return false;
        if(q.status!=ConvexAabbSweepStatus3D::Hit||!include(q.first))continue;
        for(std::size_t i=0;i<hull->AxisCount();++i)for(bool positive:{false,true}){
            if(!(positive?q.positiveContacts:q.negativeContacts)[i])continue;Vec3 n;
            if(!hull->Normal(i,positive,n))return false;
            if(Dot(n,d)<0&&!result.Add(n))return false;
        }
    }
    if(!dynamic.empty()){
        if(dynamic.size()>128){if(dynamicStatus)*dynamicStatus=CharacterDynamicStepStatus3D::InvalidInput;return false;}
        std::array<BoxCylinderContact3D,128> queries{};double firstDynamic=2;
        for(std::size_t i=0;i<dynamic.size();++i){
            if(!QueryBoxCylinderContact3D(c,e,d,dynamic[i],queries[i])){
                if(dynamicStatus)*dynamicStatus=CharacterDynamicStepStatus3D::UnresolvedNumerics;return false;
            }
            if(queries[i].kind!=BoxCylinderContactKind3D::Clear)firstDynamic=std::min(firstDynamic,queries[i].first);
        }
        // 先選真正最早事件；早牆截斷之後的預測頂面事件不代表實際已走到。
        // 同時或既有一 ULP 時序不可分辨的未解事件仍保守拒絕整筆交易。
        if(firstDynamic<=1&&include(firstDynamic)){
            for(std::size_t i=0;i<dynamic.size();++i){
                const auto& q=queries[i];
                if(q.kind!=BoxCylinderContactKind3D::Clear&&SameFraction(q.first,result.fraction)&&
                    !DynamicContactAllowed(q,dynamicStatus,response))return false;
            }
            for(std::size_t i=0;i<dynamic.size();++i){
                const auto& q=queries[i];
                if(q.kind==BoxCylinderContactKind3D::Clear||!SameFraction(q.first,result.fraction))continue;
                const bool side=q.kind==BoxCylinderContactKind3D::SideEntry||q.kind==BoxCylinderContactKind3D::SimultaneousEntry;
                const bool vertical=q.kind==BoxCylinderContactKind3D::VerticalEntry||q.kind==BoxCylinderContactKind3D::SimultaneousEntry;
                if(side&&(!Finite(q.horizontalNormal)||q.horizontalNormal.y!=0||Length(q.horizontalNormal)<.5f||
                    Dot(q.horizontalNormal,d)>=0||!result.Add(q.horizontalNormal,DynamicSideContact,DynamicBodyMask(i)))){
                    if(dynamicStatus)*dynamicStatus=CharacterDynamicStepStatus3D::UnresolvedNumerics;return false;
                }
                if(vertical&&(!Finite(q.verticalNormal)||q.verticalNormal.x!=0||q.verticalNormal.z!=0||
                    (q.verticalNormal.y!=1&&q.verticalNormal.y!=-1)||Dot(q.verticalNormal,d)>=0||
                    !result.Add(q.verticalNormal,q.verticalNormal.y>0?DynamicBelowContact:DynamicAboveContact,DynamicBodyMask(i)))){
                    if(dynamicStatus)*dynamicStatus=CharacterDynamicStepStatus3D::UnresolvedNumerics;return false;
                }
            }
        }
    }
    if(result.fraction<=1&&!result.count)return false;
    out=result;return true;
}
bool HasSolidStandingSupport(Vec3 feet,Vec3 extents,std::span<const BoxCollider3D> boxes,
    std::span<const ConvexAabbSweep3D* const> hulls,bool& supported){
    BodySweep hit;if(!SweepBody(feet+Vec3{0,extents.y,0},extents,{0,-ConvexSkin*2,0},boxes,hulls,hit))return false;
    supported=false;if(hit.fraction<=1)for(std::size_t i=0;i<hit.count;++i)supported=supported||hit.normals[i].y>=WalkableY;
    return true;
}
// 無持久滑動旗標：沿下降切線保留動量，控制只加等高線分量。
Vec3 TerrainSlideVelocity(Vec3 previous,Vec3 requested,const TerrainStandingSurface& surface,float gravity,float dt,float maximumSpeed){
    const double prior=std::max(0.,double(previous.x)*surface.downhill.x+double(previous.y)*surface.downhill.y+double(previous.z)*surface.downhill.z);
    const float speed=float(std::min(double(maximumSpeed),prior-double(gravity)*surface.downhill.y*dt));
    return surface.downhill*speed+surface.contour*Dot(requested,surface.contour);
}
// 地形切面上的二維接觸錐：最近可行速度只可能在原點、原速度或某一邊界。
// 保留本子步真實掃掠法線，不能在地形投影後重新注入朝牆速度。
Vec3 ConstrainTerrainContactVelocity(Vec3 velocity,const TerrainStandingSurface& surface,const BodySweep& contacts,float maximumSpeed){
    if(!contacts.count)return velocity;
    const auto dot=[](Vec3 a,Vec3 b){return double(a.x)*b.x+double(a.y)*b.y+double(a.z)*b.z;};
    const double u=dot(velocity,surface.downhill),w=dot(velocity,surface.contour);
    Vec3 best{};double bestDistance=dot(velocity,velocity);
    const auto consider=[&](double along,double across){
        if(!std::isfinite(along)||!std::isfinite(across)||along<0)return;
        const double scale=along>maximumSpeed?double(maximumSpeed)/along:1.;
        Vec3 candidate{float((surface.downhill.x*along+surface.contour.x*across)*scale),
            float((surface.downhill.y*along+surface.contour.y*across)*scale),
            float((surface.downhill.z*along+surface.contour.z*across)*scale)};
        if(!Finite(candidate))return;
        for(std::size_t i=0;i<contacts.count;++i)if(dot(candidate,contacts.normals[i])<0)return;
        const Vec3 delta=candidate-velocity;const double distance=dot(delta,delta);
        if(distance<bestDistance){best=candidate;bestDistance=distance;}
    };
    consider(u,w);consider(0,w);
    for(std::size_t i=0;i<contacts.count;++i){
        const double a=dot(contacts.normals[i],surface.downhill),b=dot(contacts.normals[i],surface.contour),length=a*a+b*b;
        if(length==0)continue;const double inward=(a*u+b*w)/length;
        consider(u-a*inward,w-b*inward);
    }
    return best;
}
// 有限次解算，剩餘位移不能解開時留在已驗證位置；不使用穿透後回推。
bool MoveBody(Vec3& feet,Vec3& velocity,Vec3 e,Vec3 motion,std::span<const BoxCollider3D> boxes,
              std::span<const ConvexAabbSweep3D* const> hulls,bool& supported,uint64_t& collisions,
              const HeightField3D* terrain,float maximumGrade,float standingGrade,bool& terrainLimited,BodySweep* contacts,
              std::span<const DynamicCylinder3D> dynamic={},CharacterDynamicStepStatus3D* dynamicStatus=nullptr,bool* dynamicBlocked=nullptr,
              CharacterDynamicVerticalResponse3D response=CharacterDynamicVerticalResponse3D::Reject,
              CharacterDynamicContactState3D* appliedContact=nullptr){
    const auto center=[&]{return feet+Vec3{0,e.y,0};};
    const auto advance=[&](Vec3 segment){
        if(terrain&&standingGrade>0&&!TerrainPathClear(*terrain,feet,segment).valid)return false;
        if(!LimitTerrainSegment(terrain,maximumGrade,feet,segment,terrainLimited))return false;
        if(terrainLimited){
            BodySweep hit;if(!SweepBody(center(),e,segment,boxes,hulls,hit,dynamic,dynamicStatus,response))return false;
            if(hit.fraction<=1){
                if(contacts)for(std::size_t i=0;i<hit.count;++i)if(!contacts->Add(hit.normals[i],hit.origins[i],hit.bodies[i]))return false;
                const double length=std::sqrt(double(segment.x)*segment.x+double(segment.y)*segment.y+double(segment.z)*segment.z);
                if(length>0)segment=segment*float(std::max(0.,hit.fraction-double(ConvexSkin)/length));
                if(hit.staticWalkable)supported=true;
                for(std::size_t i=0;i<hit.count;++i)if(hit.normals[i].y>=WalkableY&&velocity.y<0)velocity.y=0;
                if(response==CharacterDynamicVerticalResponse3D::BlockOnly){
                    constexpr unsigned char mask=DynamicSideContact|DynamicAboveContact|DynamicBelowContact;
                    for(unsigned pass=0;pass<4;++pass)for(std::size_t i=0;i<hit.count;++i)if((hit.origins[i]&mask)!=0){
                        const auto n=hit.normals[i];const float inward=Dot(n,velocity);if(inward<0)velocity=velocity-n*inward;
                    }
                    Vec3 zeroDynamicMotion{};
                    if(!CompleteDynamicProjection(hit,zeroDynamicMotion,velocity)){if(dynamicStatus)*dynamicStatus=CharacterDynamicStepStatus3D::UnresolvedNumerics;return false;}
                }
                RecordDynamicContact(hit,appliedContact);++collisions;
            }
        }
        feet=feet+segment;return Finite(feet);
    };
    for(unsigned iteration=0;iteration<8;++iteration){
        if(motion.x==0&&motion.y==0&&motion.z==0)break;
        BodySweep hit;if(!SweepBody(center(),e,motion,boxes,hulls,hit,dynamic,dynamicStatus,response))return false;
        if(hit.fraction>1){
            if(!advance(motion))return false;
            if(terrainLimited){++collisions;velocity.x=0;velocity.z=0;}break;
        }
        if(dynamicBlocked)*dynamicBlocked=*dynamicBlocked||hit.dynamicSide||hit.dynamicContact.blockedAbove||hit.dynamicContact.blockedBelow;
        if(contacts)for(std::size_t i=0;i<hit.count;++i)if(!contacts->Add(hit.normals[i],hit.origins[i],hit.bodies[i]))return false;
        const double length=std::sqrt(double(motion.x)*motion.x+double(motion.y)*motion.y+double(motion.z)*motion.z);
        const float fraction=float(std::max(0.,hit.fraction-double(ConvexSkin)/length));
        Vec3 segment=motion*fraction;
        if(!advance(segment))return false;++collisions;
        RecordDynamicContact(hit,appliedContact);
        if(terrainLimited){velocity.x=0;velocity.z=0;break;}
        motion=motion*(1-fraction);
        for(unsigned pass=0;pass<4;++pass)for(std::size_t i=0;i<hit.count;++i){
            const auto n=hit.normals[i];const float inward=Dot(n,motion);
            if(n.y>=WalkableY){
                // 動態頂面只去除向內法向量；Grounded 只由真靜態支撐授權。
                if(hit.staticWalkable)supported=true;
                if(inward<0)motion.y=std::max(motion.y,-(n.x*motion.x+n.z*motion.z)/n.y);
                if(velocity.y<0)velocity.y=0;
            }else{
                if(inward<0)motion=motion-n*inward;
                const float v=Dot(n,velocity);if(v<0)velocity=velocity-n*v;
            }
        }
        if(response==CharacterDynamicVerticalResponse3D::BlockOnly&&!CompleteDynamicProjection(hit,motion,velocity)){
            if(dynamicStatus)*dynamicStatus=CharacterDynamicStepStatus3D::UnresolvedNumerics;return false;
        }
        if(!BodyClear(center(),e,boxes,hulls)||!DynamicBodiesClear(center(),e,dynamic,dynamicStatus))return false;
    }
    return BodyClear(center(),e,boxes,hulls)&&DynamicBodiesClear(center(),e,dynamic,dynamicStatus)&&Finite(velocity);
}
// 已接地的小落差採上、前、下三段連續查詢；最高20cm，不允許空中攀牆。
bool TryConvexStep(Vec3 start,Vec3 motion,Vec3 e,std::span<const BoxCollider3D> boxes,
                   std::span<const ConvexAabbSweep3D* const> hulls,Vec3& result){
    constexpr float rise=.20f;
    const Vec3 horizontal{motion.x,0,motion.z};
    if(horizontal.x==0&&horizontal.z==0)return false;
    BodySweep up;if(!SweepBody(start+Vec3{0,e.y,0},e,{0,rise,0},boxes,hulls,up)||up.fraction<=1)return false;
    Vec3 candidate=start+Vec3{0,rise,0};
    BodySweep forward;if(!SweepBody(candidate+Vec3{0,e.y,0},e,horizontal,boxes,hulls,forward)||forward.fraction<=1)return false;
    candidate=candidate+horizontal;
    BodySweep down;constexpr float distance=rise+ConvexSkin*2;
    if(!SweepBody(candidate+Vec3{0,e.y,0},e,{0,-distance,0},boxes,hulls,down)||down.fraction>1)return false;
    bool walkable=false;for(std::size_t n=0;n<down.count;++n)walkable=walkable||down.normals[n].y>=WalkableY;
    if(!walkable)return false;
    candidate.y-=std::max(0.f,float(down.fraction*distance)-ConvexSkin);
    if(candidate.y<start.y-ConvexSkin*2||candidate.y>start.y+rise||!BodyClear(candidate+Vec3{0,e.y,0},e,boxes,hulls))return false;
    result=candidate;return true;
}
}
bool CharacterController3D::StepConvexInternal(CharacterControl3D control,std::span<const BoxCollider3D> boxes,
    std::span<const ConvexAabbSweep3D* const> hulls,const HeightField3D* terrain,float deltaTime,
    std::span<const DynamicCylinder3D> dynamic,CharacterDynamicStepStatus3D* dynamicStatus,
    CharacterDynamicVerticalResponse3D verticalResponse,CharacterDynamicContactState3D* appliedContact){
    StepQueryWindowScope queryWindow;
    if(dynamicStatus)*dynamicStatus=CharacterDynamicStepStatus3D::InvalidInput;
    if(verticalResponse!=CharacterDynamicVerticalResponse3D::Reject&&verticalResponse!=CharacterDynamicVerticalResponse3D::BlockOnly)return false;
    CharacterDynamicContactState3D pendingContact;
    if(!ValidTerrainPolicies(settings_))return false;
    if(settings_.jumpGraceSeconds==0&&dynamic.empty()&&hulls.empty()&&(!terrain||(settings_.maximumTerrainUphillGrade==0&&settings_.maximumTerrainStandingGrade==0))){if(terrain)Step(control,boxes,*terrain,deltaTime);else Step(control,boxes,deltaTime);return true;}
    const auto& s=settings_;
    if(!std::isfinite(deltaTime)||deltaTime<0||!Finite(control.movement)||!Finite(position_)||!Finite(velocity_)||
       !std::isfinite(s.radius)||s.radius<=0||!std::isfinite(s.height)||s.height<=0||
       !std::isfinite(s.moveSpeed)||s.moveSpeed<0||!std::isfinite(s.gravity)||s.gravity<0||
       !std::isfinite(s.jumpSpeed)||s.jumpSpeed<0||!std::isfinite(s.maximumFallSpeed)||s.maximumFallSpeed<=0||
       !std::isfinite(s.groundSnapDistance)||s.groundSnapDistance<0||!BodyClear(Center(),Extents(),boxes,hulls))return false;
    if(dynamic.size()>128)return false;
    for(const auto& body:dynamic){
        const Vec3 e{body.radius,body.halfHeight,body.radius};
        if(!Finite(body.center)||!std::isfinite(body.radius)||body.radius<=0||!std::isfinite(body.halfHeight)||
            body.halfHeight<=0||!Finite(body.center+e)||!Finite(body.center-e))return false;
    }
    if(!DynamicBodiesClear(Center(),Extents(),dynamic,dynamicStatus))return false;
    Vec3 p=position_,v=velocity_;auto stats=stats_;bool grounded=grounded_;const bool wasGrounded=grounded;
    const float dt=Clamp(deltaTime,0,.1f),multiplier=std::isfinite(control.speedMultiplier)&&control.speedMultiplier>0?Clamp(control.speedMultiplier,.25f,4.f):1.f;
    queryWindow.Prepare(boxes,p,Extents(),dt);
    // 全量入口驗證已通過；只縮小本次完整解算器的靜態 XZ 查詢清單，窄相位與順序不變。
    std::array<BoxCollider3D,256> windowBoxes;
    if(boxes.size()>64&&hulls.empty()&&dynamic.empty()&&s.maximumTerrainUphillGrade==0&&
       s.maximumTerrainStandingGrade==0&&dt>0&&std::abs(p.x)<=100000.f&&std::abs(p.z)<=100000.f&&s.radius<=4.f){
        const double budget=double(s.moveSpeed)*double(multiplier)*double(dt);
        if(std::isfinite(budget)&&budget<=16.){
            // 額外一公尺與座標誤差墊保守包住 64 子步、每步最多 8 次軸向滑動。
            const double magnitude=std::max({std::abs(double(p.x)),std::abs(double(p.z)),budget,double(s.radius),1.});
            const double window=double(s.radius)+budget+1.+2048.*double(std::numeric_limits<float>::epsilon())*magnitude;
            const double lowX=double(p.x)-window,highX=double(p.x)+window;
            const double lowZ=double(p.z)-window,highZ=double(p.z)+window;
            const Vec3 e=Extents();std::size_t count=0;bool overflow=false;
            for(const auto& box:boxes){
                // 與原 SweepAabb 相同的 float 展開運算；保留邊界相等及全部 Y 範圍。
                const Vec3 low=box.center-box.extents-e,high=box.center+box.extents+e;
                if(double(low.x)>highX||double(high.x)<lowX||double(low.z)>highZ||double(high.z)<lowZ)continue;
                if(count==windowBoxes.size()){overflow=true;break;}
                windowBoxes[count++]=box;
            }
            // 超量時不拒絕、不配置、不改排序，完整回到原 span；零候選亦是合法清單。
            if(!overflow)boxes=std::span<const BoxCollider3D>(windowBoxes.data(),count);
        }
    }
    const bool standingPolicy=terrain&&s.maximumTerrainStandingGrade>0;
    const bool graceEnabled=s.jumpGraceSeconds>0;
    double grace=graceEnabled?jumpGraceRemaining_:0;
    if(standingPolicy&&!std::isfinite(float(std::hypot(double(s.maximumFallSpeed),double(s.moveSpeed)*4))))return false;
    TerrainStandingSurface initialSurface;
    if(!ReadTerrainStandingSurface(terrain,p,graceEnabled&&s.maximumTerrainStandingGrade==0?std::numeric_limits<float>::max():s.maximumTerrainStandingGrade,initialSurface))return false;
    // 新政策零時間只驗證；不注入重力、跳躍、吸附、統計或預覽失效。
    if((standingPolicy||graceEnabled)&&dt==0){if(dynamicStatus)*dynamicStatus=CharacterDynamicStepStatus3D::Applied;if(appliedContact)*appliedContact={};return true;}
    bool initialSolid=false;
    if((standingPolicy||graceEnabled)&&!HasSolidStandingSupport(p,Extents(),boxes,hulls,initialSolid))return false;
    const bool initialSliding=standingPolicy&&initialSurface.inside&&initialSurface.steep&&!initialSolid&&
        p.y<=initialSurface.height+ConvexSkin*2&&v.y<=0;
    if((standingPolicy||graceEnabled)&&grounded){
        const bool terrainSupported=initialSurface.inside&&!initialSurface.steep&&
            p.y>=initialSurface.height&&p.y<=initialSurface.height+ConvexSkin*2;
        grounded=initialSolid||terrainSupported;
    }
    Vec3 input{control.movement.x,0,control.movement.z};const float inputLength=Length(input);
    if(!std::isfinite(inputLength))return false;if(inputLength>1)input=input*(1/inputLength);
    const Vec3 requested{input.x*s.moveSpeed*multiplier,0,input.z*s.moveSpeed*multiplier};
    if(!Finite(requested))return false;
    if(!initialSliding){v.x=requested.x;v.z=requested.z;}
    if(control.jump&&(grounded||(graceEnabled&&s.jumpSpeed>0&&grace>0&&v.y<=0&&!initialSliding))){v.y=s.jumpSpeed;grounded=false;++stats.jumps;}
    const double maxDistance=std::max({std::abs(double(v.x))*dt,std::abs(double(v.z))*dt,
        (standingPolicy?std::hypot(double(s.maximumFallSpeed),double(s.moveSpeed)*4):double(s.maximumFallSpeed))*dt});
    const unsigned steps=unsigned(std::clamp(std::ceil(maxDistance/std::max(.05,double(s.radius)*.5)),1.,64.));
    const float h=dt/float(steps);const Vec3 extents=Extents();bool anyTerrainLimited=false,terrainUndersideBlocked=false;
    Vec3 terrainProbe{};bool probeLimited=false;
    if(!LimitTerrainSegment(terrain,s.maximumTerrainUphillGrade,p,terrainProbe,probeLimited))return false;
    for(unsigned i=0;i<steps;++i){
        const auto priorSubstepStats=stats;const auto priorSubstepContact=pendingContact;const bool priorSubstepTerrainLimited=anyTerrainLimited;
        TerrainStandingSurface surface;
        if(!ReadTerrainStandingSurface(terrain,p,s.maximumTerrainStandingGrade,surface))return false;
        bool solidSupport=false;
        if(standingPolicy&&!HasSolidStandingSupport(p,extents,boxes,hulls,solidSupport))return false;
        const bool sliding=standingPolicy&&surface.inside&&surface.steep&&!solidSupport&&
            p.y<=surface.height+ConvexSkin*2&&v.y<=0;
        if(sliding)grounded=false;
        const bool previousSupport=grounded;grounded=false;
        if(sliding)v=TerrainSlideVelocity(v,requested,surface,s.gravity,h,s.maximumFallSpeed);
        else v.y=std::max(-s.maximumFallSpeed,v.y-s.gravity*h);
        const Vec3 before=p,desired=v*h;
        bool terrainLimited=false;
        BodySweep contacts;bool dynamicBlocked=false;
        if(!Finite(v)||!MoveBody(p,v,extents,desired,boxes,hulls,grounded,stats.collisions,terrain,s.maximumTerrainUphillGrade,s.maximumTerrainStandingGrade,terrainLimited,standingPolicy?&contacts:nullptr,dynamic,dynamicStatus,&dynamicBlocked,verticalResponse,&pendingContact))return false;
        anyTerrainLimited=anyTerrainLimited||terrainLimited;
        const double intended=double(desired.x)*desired.x+double(desired.z)*desired.z;
        const double advanced=double(p.x-before.x)*desired.x+double(p.z-before.z)*desired.z;
        if(!terrainLimited&&!dynamicBlocked&&previousSupport&&desired.y<=0&&intended>0&&advanced<intended*.999){
            Vec3 stepped;
            if(TryConvexStep(before,desired,extents,boxes,hulls,stepped)){
                bool blocked=false,partBlocked=false;
                const Vec3 up{0,.20f,0},horizontal{desired.x,0,desired.z};
                if(!DynamicOptionalStepClear(before+Vec3{0,extents.y,0},extents,up,dynamic,dynamicStatus,partBlocked))return false;
                blocked=blocked||partBlocked;
                if(!DynamicOptionalStepClear(before+up+Vec3{0,extents.y,0},extents,horizontal,dynamic,dynamicStatus,partBlocked))return false;
                blocked=blocked||partBlocked;
                const Vec3 raised=before+up+horizontal;
                if(!DynamicOptionalStepClear(raised+Vec3{0,extents.y,0},extents,stepped-raised,dynamic,dynamicStatus,partBlocked))return false;
                blocked=blocked||partBlocked;
                if(!blocked){
                Vec3 stepSegment=stepped-before;bool stepLimited=false;
                if(standingPolicy&&!TerrainPathClear(*terrain,before,stepSegment).valid)return false;
                if(!LimitTerrainSegment(terrain,s.maximumTerrainUphillGrade,before,stepSegment,stepLimited))return false;
                if(stepLimited){anyTerrainLimited=true;v.x=0;v.z=0;}
                else{
                p=stepped;v.x=input.x*s.moveSpeed*multiplier;v.z=input.z*s.moveSpeed*multiplier;v.y=0;grounded=true;
                }
                }
            }
        }
        if(v.y<=0&&(previousSupport||grounded||dt==0)){
            // 零時間讀檔預檢只重建原地接觸，不吸附或移動已保存的腳座標。
            BodySweep support;const float distance=dt==0?ConvexSkin*2:std::max(s.groundSnapDistance,ConvexSkin*2);
            if(!SweepBody(p+Vec3{0,extents.y,0},extents,{0,-distance,0},boxes,hulls,support))return false;
            bool walkable=false;for(std::size_t n=0;n<support.count;++n)walkable=walkable||support.normals[n].y>=WalkableY;
            grounded=support.fraction<=1&&walkable;
            if(grounded){
                const float down=dt==0?0.f:std::max(0.f,float(support.fraction*distance)-ConvexSkin);
                bool blocked=false;
                if(!DynamicAuxiliaryClear(p+Vec3{0,extents.y,0},extents,{0,-down,0},dynamic,dynamicStatus,blocked,verticalResponse))return false;
                if(blocked){
                    if(verticalResponse!=CharacterDynamicVerticalResponse3D::BlockOnly){if(dynamicStatus)*dynamicStatus=CharacterDynamicStepStatus3D::UnresolvedNumerics;return false;}
                    // 較遠靜態地面不能授權穿過先到的動態體；取消可選吸附。
                    grounded=false;
                }else{p.y-=down;v.y=0;}
            }
        }
        if(terrain&&terrain->Contains(p.x,p.z)){
            const float ground=terrain->SampleHeight(p.x,p.z);if(!std::isfinite(ground))return false;
            TerrainStandingSurface endpoint;
            if(!ReadTerrainStandingSurface(terrain,p,s.maximumTerrainStandingGrade,endpoint))return false;
            // 本子步已掃掠確認實體支撐時，不再向較低地形吸附穿過橋面。
            // 真正低於地形的修正仍由下方 p.y<=ground 獨立處理。
            const bool touch=standingPolicy&&!grounded&&p.y<=ground+ConvexSkin*2;
            // 上升中仍可能水平切入上坡；必要地形修正不受垂直速度符號限制。
            if(p.y<ground||(v.y<=0&&(p.y<=ground||touch||(!endpoint.steep&&!grounded&&previousSupport&&p.y-ground<=s.groundSnapDistance)))){
                // 高度場修正也必須走完整體積的連續路徑，不能跳到頂棚另一側。
                BodySweep correction;
                const Vec3 terrainCorrection{0,(ground+extents.y)-(p.y+extents.y),0};
                // 掃掠以實際float身體中心的起終點為準；feet差量會在緊淨空下算出超過終點的假命中。
                if(!SweepBody(p+Vec3{0,extents.y,0},extents,terrainCorrection,boxes,hulls,correction))return false;
                if(correction.fraction<1){
                    // 只有已驗支撐的靜態上坡步行可保留已完成子步；上升、動態或未知入口仍整筆拒絕。
                    bool underside=correction.count>0;
                    for(std::size_t n=0;n<correction.count;++n)underside=underside&&correction.normals[n].y<0;
                    TerrainStandingSurface prefix;
                    if(!dynamic.empty()||!wasGrounded||velocity_.y>0||control.jump||!previousSupport||v.y>0||(desired.x==0&&desired.z==0)||
                       !terrain->Contains(before.x,before.z)||ground<=before.y||!underside||
                       !ReadTerrainStandingSurface(terrain,before,s.maximumTerrainStandingGrade,prefix,true)||
                       !prefix.inside||prefix.steep||before.y!=prefix.height||
                       !BodyClear(before+Vec3{0,extents.y,0},extents,boxes,hulls))return false;
                    p=before;v={};grounded=true;stats=priorSubstepStats;++stats.collisions;
                    pendingContact=priorSubstepContact;anyTerrainLimited=priorSubstepTerrainLimited;terrainUndersideBlocked=true;
                    // 不把可行的等高輸入一起歸零；任何試走仍須完整掃掠、全程地形與終態體積檢查。
                    const Vec3 tangent=prefix.contour*Dot(requested,prefix.contour),trialMotion=tangent*h;
                    if(!Finite(tangent)||!Finite(trialMotion))return false;
                    if(trialMotion.x==0&&trialMotion.z==0)break;
                    const auto path=TerrainPathClear(*terrain,p,trialMotion);if(!path.valid)return false;
                    if(!std::isfinite(path.minimumGap))return false;
                    // Float SampleHeight的支撐值可低於解析三角面末位；不是把負gap當clear。
                    // 完整路徑各面端點給出真最高高度，向外轉float，再以原20cm跨階上限及完整三段掃掠處理。
                    const Vec3 trialEnd=p+trialMotion;
                    if(!terrain->Contains(trialEnd.x,trialEnd.z))break;
                    const float endpointHeight=terrain->SampleHeight(trialEnd.x,trialEnd.z);
                    if(!std::isfinite(endpointHeight))return false;
                    const double requiredHeight=std::max({double(p.y),double(endpointHeight),double(p.y)-path.minimumGap});
                    float raisedHeight=float(requiredHeight);
                    if(double(raisedHeight)<requiredHeight)raisedHeight=std::nextafter(raisedHeight,(std::numeric_limits<float>::infinity)());
                    const float rise=raisedHeight-p.y;
                    if(!std::isfinite(rise)||rise<0)return false;
                    if(rise>.20f)break;
                    BodySweep lift;
                    if(!SweepBody(p+Vec3{0,extents.y,0},extents,{0,(raisedHeight+extents.y)-(p.y+extents.y),0},boxes,hulls,lift))return false;
                    if(lift.fraction<1)break;
                    Vec3 trial{p.x,raisedHeight,p.z},trialVelocity=tangent;bool trialSupported=false,trialLimited=false;auto trialCollisions=stats.collisions;
                    if(!BodyClear(trial+Vec3{0,extents.y,0},extents,boxes,hulls))return false;
                    if(!MoveBody(trial,trialVelocity,extents,trialMotion,boxes,hulls,trialSupported,trialCollisions,
                        terrain,s.maximumTerrainUphillGrade,s.maximumTerrainStandingGrade,trialLimited,nullptr))return false;
                    if(!terrain->Contains(trial.x,trial.z))break;
                    const float trialGround=terrain->SampleHeight(trial.x,trial.z);
                    if(!std::isfinite(trialGround))return false;
                    if(std::abs(trialGround-p.y)>.20f)break;
                    Vec3 wholeStep=trial-p;wholeStep.y=trialGround-p.y;bool wholeLimited=false;
                    if(!LimitTerrainSegment(terrain,s.maximumTerrainUphillGrade,p,wholeStep,wholeLimited))return false;
                    if(wholeLimited)break;
                    TerrainStandingSurface finalSurface;
                    if(!ReadTerrainStandingSurface(terrain,{trial.x,trialGround,trial.z},s.maximumTerrainStandingGrade,finalSurface))return false;
                    if(finalSurface.inside&&finalSurface.steep)break;
                    BodySweep settle;
                    if(!SweepBody(trial+Vec3{0,extents.y,0},extents,{0,(trialGround+extents.y)-(trial.y+extents.y),0},boxes,hulls,settle))return false;
                    if(settle.fraction<1)break;
                    trial.y=trialGround;if(!BodyClear(trial+Vec3{0,extents.y,0},extents,boxes,hulls))return false;
                    p=trial;v=trialVelocity;grounded=true;stats.collisions=trialCollisions;anyTerrainLimited|=trialLimited;
                    continue;
                }
                bool blocked=false;
                if(!DynamicAuxiliaryClear(p+Vec3{0,extents.y,0},extents,terrainCorrection,dynamic,dynamicStatus,blocked,verticalResponse))return false;
                if(blocked){
                    // 上方的可選吸附可取消；已低於真地形的必要修正無解時仍整筆拒絕。
                    if(verticalResponse!=CharacterDynamicVerticalResponse3D::BlockOnly||p.y<ground){
                        if(dynamicStatus)*dynamicStatus=CharacterDynamicStepStatus3D::UnresolvedNumerics;return false;
                    }
                    grounded=false;
                }else{
                    p.y=ground;
                    bool solid=false;if(standingPolicy&&!HasSolidStandingSupport(p,extents,boxes,hulls,solid))return false;
                    if(endpoint.steep&&!solid){
                        grounded=false;v=TerrainSlideVelocity(v,v,endpoint,0,0,s.maximumFallSpeed);
                        v=ConstrainTerrainContactVelocity(v,endpoint,contacts,s.maximumFallSpeed);
                    }
                    else{v.y=0;grounded=true;}
                    ++stats.terrainContacts;
                }
            }
        }
        if(!Finite(v)||!BodyClear(p+Vec3{0,extents.y,0},extents,boxes,hulls)||
            !DynamicBodiesClear(p+Vec3{0,extents.y,0},extents,dynamic,dynamicStatus))return false;
    }
    if(anyTerrainLimited&&!terrainUndersideBlocked&&dt>0){v.x=(p.x-position_.x)/dt;v.z=(p.z-position_.z)/dt;}
    if(grounded&&!wasGrounded)++stats.landings;++stats.steps;
    if(dynamicStatus)*dynamicStatus=CharacterDynamicStepStatus3D::Applied;
    // 只由成功的真支撐補足寬限；起跳／上升／陡坡滑行立即作廢。失敗與零時間不消耗。
    if(graceEnabled){
        if(stats.jumps!=stats_.jumps||v.y>0||initialSliding)grace=0;
        else if(grounded)grace=double(s.jumpGraceSeconds);
        else grace=(std::max)(0.,grace-double(dt));
    }
    InvalidateDirectPreview(true);position_=p;velocity_=v;grounded_=grounded;stats_=stats;
    jumpGraceRemaining_=grace;
    if(appliedContact)*appliedContact=pendingContact;return true;
}
bool CharacterController3D::Step(CharacterControl3D c,std::span<const BoxCollider3D> b,std::span<const ConvexAabbSweep3D* const> h,float dt){return StepConvexInternal(c,b,h,nullptr,dt);}
bool CharacterController3D::Step(CharacterControl3D c,std::span<const BoxCollider3D> b,std::span<const ConvexAabbSweep3D* const> h,const HeightField3D& t,float dt){return StepConvexInternal(c,b,h,&t,dt);}
CharacterDynamicStepStatus3D CharacterController3D::Step(CharacterControl3D c,std::span<const BoxCollider3D> b,
    std::span<const ConvexAabbSweep3D* const> h,std::span<const DynamicCylinder3D> dynamic,float dt){
    if(dynamic.empty())return StepConvexInternal(c,b,h,nullptr,dt)?CharacterDynamicStepStatus3D::Applied:CharacterDynamicStepStatus3D::InvalidInput;
    CharacterDynamicStepStatus3D status=CharacterDynamicStepStatus3D::InvalidInput;
    (void)StepConvexInternal(c,b,h,nullptr,dt,dynamic,&status);return status;
}
CharacterDynamicStepStatus3D CharacterController3D::Step(CharacterControl3D c,std::span<const BoxCollider3D> b,
    std::span<const ConvexAabbSweep3D* const> h,const HeightField3D& t,std::span<const DynamicCylinder3D> dynamic,float dt){
    if(dynamic.empty())return StepConvexInternal(c,b,h,&t,dt)?CharacterDynamicStepStatus3D::Applied:CharacterDynamicStepStatus3D::InvalidInput;
    CharacterDynamicStepStatus3D status=CharacterDynamicStepStatus3D::InvalidInput;
    (void)StepConvexInternal(c,b,h,&t,dt,dynamic,&status);return status;
}
CharacterDynamicStepStatus3D CharacterController3D::Step(CharacterControl3D c,std::span<const BoxCollider3D> b,
    std::span<const ConvexAabbSweep3D* const> h,std::span<const DynamicCylinder3D> dynamic,float dt,
    CharacterDynamicVerticalResponse3D response,CharacterDynamicContactState3D* appliedContact){
    if(response!=CharacterDynamicVerticalResponse3D::Reject&&response!=CharacterDynamicVerticalResponse3D::BlockOnly)return CharacterDynamicStepStatus3D::InvalidInput;
    if(dynamic.empty()){
        const bool accepted=StepConvexInternal(c,b,h,nullptr,dt);
        if(accepted&&appliedContact)*appliedContact={};
        return accepted?CharacterDynamicStepStatus3D::Applied:CharacterDynamicStepStatus3D::InvalidInput;
    }
    CharacterDynamicStepStatus3D status=CharacterDynamicStepStatus3D::InvalidInput;
    CharacterDynamicContactState3D pending;
    const bool accepted=StepConvexInternal(c,b,h,nullptr,dt,dynamic,&status,response,&pending);
    if(accepted&&appliedContact)*appliedContact=pending;
    return status;
}
CharacterDynamicStepStatus3D CharacterController3D::Step(CharacterControl3D c,std::span<const BoxCollider3D> b,
    std::span<const ConvexAabbSweep3D* const> h,const HeightField3D& t,std::span<const DynamicCylinder3D> dynamic,float dt,
    CharacterDynamicVerticalResponse3D response,CharacterDynamicContactState3D* appliedContact){
    if(response!=CharacterDynamicVerticalResponse3D::Reject&&response!=CharacterDynamicVerticalResponse3D::BlockOnly)return CharacterDynamicStepStatus3D::InvalidInput;
    if(dynamic.empty()){
        const bool accepted=StepConvexInternal(c,b,h,&t,dt);
        if(accepted&&appliedContact)*appliedContact={};
        return accepted?CharacterDynamicStepStatus3D::Applied:CharacterDynamicStepStatus3D::InvalidInput;
    }
    CharacterDynamicStepStatus3D status=CharacterDynamicStepStatus3D::InvalidInput;
    CharacterDynamicContactState3D pending;
    const bool accepted=StepConvexInternal(c,b,h,&t,dt,dynamic,&status,response,&pending);
    if(accepted&&appliedContact)*appliedContact=pending;
    return status;
}
bool CharacterController3D::TryLaunchFromGround(float speed,std::span<const BoxCollider3D> boxes,
    std::span<const ConvexAabbSweep3D* const> hulls,const HeightField3D& terrain,std::span<const DynamicCylinder3D> dynamic)noexcept{
    try{
        if(dynamic.empty())return TryLaunchFromGround(speed,boxes,hulls,terrain);
        if(dynamic.size()>128||!DynamicBodiesClear(Center(),Extents(),dynamic,nullptr))return false;
        return TryLaunchFromGround(speed,boxes,hulls,terrain);
    }catch(...){return false;}
}
bool CharacterController3D::TrySetHeight(float height,std::span<const BoxCollider3D> boxes,
    std::span<const ConvexAabbSweep3D* const> hulls,std::span<const DynamicCylinder3D> dynamic){
    if(dynamic.empty())return TrySetHeight(height,boxes,hulls);
    if(!std::isfinite(height)||height<.6f||height>3.f||dynamic.size()>128||
        !DynamicBodiesClear(Center(),Extents(),dynamic,nullptr))return false;
    const Vec3 nextExtents{settings_.radius,height*.5f,settings_.radius},nextCenter=position_+Vec3{0,nextExtents.y,0};
    // 增高的形狀逐一包含舊形狀；最終體積全淨空才可採用，縮身不作起始去穿透。
    if(!DynamicBodiesClear(nextCenter,nextExtents,dynamic,nullptr))return false;
    return TrySetHeight(height,boxes,hulls);
}
bool CharacterController3D::TryLaunchFromGround(float speed,std::span<const BoxCollider3D> boxes,
    std::span<const ConvexAabbSweep3D* const> hulls,const HeightField3D& terrain)noexcept{
    try{
        if(!ValidTerrainPolicies(settings_)||!grounded_||!Finite(position_)||!BodyClear(Center(),Extents(),boxes,hulls))return false;
        if(settings_.maximumTerrainStandingGrade==0)return LaunchFromVerifiedGround(speed);
        TerrainStandingSurface surface;
        if(!ReadTerrainStandingSurface(&terrain,position_,settings_.maximumTerrainStandingGrade,surface))return false;
        bool solid=false;if(!HasSolidStandingSupport(position_,Extents(),boxes,hulls,solid))return false;
        const bool terrainSupported=surface.inside&&!surface.steep&&position_.y>=surface.height&&position_.y<=surface.height+ConvexSkin*2;
        return (solid||terrainSupported)&&LaunchFromVerifiedGround(speed);
    }catch(...){return false;}
}
bool CharacterController3D::TryRestoreMotionState(const CharacterMotionState3D& state,
    std::span<const BoxCollider3D> boxes,std::span<const ConvexAabbSweep3D* const> hulls,
    const HeightField3D& terrain)noexcept{
    try{
        const auto& s=settings_;
        if(!ValidTerrainPolicies(s))return false;
        if(directGeneration_==(std::numeric_limits<uint64_t>::max)()||!Finite(state.feet)||!Finite(state.velocity)||
           !std::isfinite(state.height)||state.height<.6f||state.height>3.f||!std::isfinite(state.yaw)||
           state.yaw < -3.141592654f||state.yaw>3.141592654f||
           !std::isfinite(s.radius)||s.radius<=0||!std::isfinite(s.height)||s.height<.6f||s.height>3.f||
           !std::isfinite(s.moveSpeed)||s.moveSpeed<=0||!std::isfinite(s.jumpSpeed)||s.jumpSpeed<0||
           !std::isfinite(s.gravity)||s.gravity<0||!std::isfinite(s.maximumFallSpeed)||s.maximumFallSpeed<=0||
           !std::isfinite(s.groundSnapDistance)||s.groundSnapDistance<0||
           !std::isfinite(s.maximumDirectDisplacement)||s.maximumDirectDisplacement<=0||
           s.maximumSlideIterations<1||s.maximumSlideIterations>3)return false;
        // 一般 Step 的水平輸入上限為四倍；以 double 防止極值平方溢位。
        const double speed=std::hypot(double(state.velocity.x),double(state.velocity.z));
        const float maximumHorizontal=s.maximumTerrainStandingGrade>0?float(std::hypot(double(s.maximumFallSpeed),double(s.moveSpeed)*4)):s.moveSpeed*4.f;
        if(!std::isfinite(maximumHorizontal))return false;
        // 正規化的 float 向量允許一個速度 ULP 的捨入，不新增遊戲速度倍率。
        const double horizontalLimit=std::nextafter(maximumHorizontal,std::numeric_limits<float>::infinity());
        if(speed>horizontalLimit||state.velocity.y>s.jumpSpeed||
           state.velocity.y< -s.maximumFallSpeed||(state.grounded&&state.velocity.y!=0))return false;
        const Vec3 extents{s.radius,state.height*.5f,s.radius},center=state.feet+Vec3{0,extents.y,0};
        if(!Finite(center-extents)||!Finite(center+extents)||!BodyClear(center,extents,boxes,hulls))return false;
        const auto& grid=terrain.Settings();
        if(!Finite(grid.origin)||grid.width<2||grid.depth<2||!std::isfinite(grid.cellSize)||grid.cellSize<=0||
           terrain.SampleCount()!=std::size_t(grid.width)*grid.depth||!terrain.Contains(state.feet.x,state.feet.z))return false;
        const float ground=terrain.SampleHeight(state.feet.x,state.feet.z);
        if(!std::isfinite(ground)||state.feet.y<ground)return false;
        // 與角色地形接觸相同，以腳中心支撐；另精確裁切底面覆蓋的三角形，拒絕穿過頭頂的尖峰。
        // 不把坡地的局部高於腳中心誤當完整身體穿透，也不以稀疏角點漏掉窄峰。
        const double endX=double(grid.origin.x)+double(grid.width-1)*grid.cellSize;
        const double endZ=double(grid.origin.z)+double(grid.depth-1)*grid.cellSize;
        if(!std::isfinite(endX)||!std::isfinite(endZ))return false;
        const double loX=std::max(double(grid.origin.x),double(state.feet.x)-s.radius),hiX=std::min(endX,double(state.feet.x)+s.radius);
        const double loZ=std::max(double(grid.origin.z),double(state.feet.z)-s.radius),hiZ=std::min(endZ,double(state.feet.z)+s.radius);
        const auto cell=[&](double p,float origin,unsigned count){return unsigned(std::clamp(std::floor((p-origin)/grid.cellSize),0.,double(count-2)));};
        const auto x0=cell(loX,grid.origin.x,grid.width),x1=cell(hiX,grid.origin.x,grid.width);
        const auto z0=cell(loZ,grid.origin.z,grid.depth),z1=cell(hiZ,grid.origin.z,grid.depth);
        if(std::size_t(x1-x0+1)*(z1-z0+1)>4096)return false;
        struct Point{double x,y,z;};
        const auto triangleClear=[&](std::array<Point,3> triangle){
            std::array<Point,12> polygon{},next{};std::copy(triangle.begin(),triangle.end(),polygon.begin());std::size_t count=3;
            for(unsigned plane=0;plane<4&&count;++plane){
                const bool xAxis=plane<2,lower=plane==0||plane==2;
                const double edge=plane==0?loX:plane==1?hiX:plane==2?loZ:hiZ;
                const auto coordinate=[&](Point p){return xAxis?p.x:p.z;};
                const auto inside=[&](Point p){return lower?coordinate(p)>=edge:coordinate(p)<=edge;};
                std::size_t size=0;auto prior=polygon[count-1];bool wasInside=inside(prior);
                for(std::size_t i=0;i<count;++i){const auto current=polygon[i];const bool isInside=inside(current);
                    if(isInside!=wasInside){const double t=(edge-coordinate(prior))/(coordinate(current)-coordinate(prior));
                        next[size++]={prior.x+(current.x-prior.x)*t,prior.y+(current.y-prior.y)*t,prior.z+(current.z-prior.z)*t};}
                    if(isInside)next[size++]=current;prior=current;wasInside=isInside;
                }
                polygon=next;count=size;
            }
            for(std::size_t i=0;i<count;++i)if(!std::isfinite(polygon[i].y)||polygon[i].y>=double(state.feet.y)+state.height)return false;
            return true;
        };
        for(unsigned z=z0;z<=z1;++z)for(unsigned x=x0;x<=x1;++x){
            const double a=terrain.Height(x,z),b=terrain.Height(x+1,z),c=terrain.Height(x,z+1),d=terrain.Height(x+1,z+1);
            if(!std::isfinite(a)||!std::isfinite(b)||!std::isfinite(c)||!std::isfinite(d))return false;
            const double left=double(grid.origin.x)+double(x)*grid.cellSize,back=double(grid.origin.z)+double(z)*grid.cellSize;
            const Point pa{left,a,back},pb{left+grid.cellSize,b,back},pc{left,c,back+grid.cellSize},pd{left+grid.cellSize,d,back+grid.cellSize};
            if(!triangleClear({pa,pc,pb})||!triangleClear({pb,pc,pd}))return false;
        }
        if(state.grounded){
            TerrainStandingSurface surface;
            if(!ReadTerrainStandingSurface(&terrain,state.feet,s.maximumTerrainStandingGrade,surface))return false;
            bool supported=state.feet.y==ground&&!surface.steep;
            BodySweep support;
            if(!SweepBody(center,extents,{0,-ConvexSkin*2,0},boxes,hulls,support))return false;
            if(support.fraction<=1)for(std::size_t i=0;i<support.count;++i)supported=supported||support.normals[i].y>=WalkableY;
            if(!supported)return false;
        }
        // 唯一寫入點：不消耗時間、不改統計、不移動腳座標；空中狀態不作支撐吸附。
        InvalidateDirectPreview(true);position_=state.feet;velocity_=state.velocity;
        settings_.height=state.height;yaw_=state.yaw;grounded_=state.grounded;return true;
    }catch(...){return false;}
}
bool CharacterController3D::TrySetHeight(float height,std::span<const BoxCollider3D> boxes,std::span<const ConvexAabbSweep3D* const> hulls){
    if(hulls.empty())return TrySetHeight(height,boxes);
    if(!std::isfinite(height)||height<.6f||height>3.f)return false;
    if(!BodyClear(position_+Vec3{0,height*.5f,0},{settings_.radius,height*.5f,settings_.radius},boxes,hulls))return false;
    if(height==settings_.height)return true;InvalidateDirectPreview(true);settings_.height=height;return true;
}

bool CharacterController3D::SetHeight(float height){if(!std::isfinite(height)||height<.6f||height>3.f)return false;InvalidateDirectPreview(true);settings_.height=height;return true;}
bool CharacterController3D::TrySetHeight(float height,std::span<const BoxCollider3D> colliders){
    if(!std::isfinite(height)||height<.6f||height>3.f)return false;
    if(height==settings_.height)return true;
    if(height>settings_.height){
        const Vec3 center=position_+Vec3{0,height*.5f,0},extents{settings_.radius,height*.5f,settings_.radius};
        for(const auto& collider:colliders)if(OverlapAabb(center,extents,collider))return false;
    }
    return SetHeight(height);
}
bool CharacterController3D::CanRebaseOrigin(Vec3 delta)const noexcept{const Vec3 position=position_-delta;return std::isfinite(delta.x)&&std::isfinite(delta.y)&&std::isfinite(delta.z)&&std::isfinite(position.x)&&std::isfinite(position.y)&&std::isfinite(position.z);}
void CharacterController3D::RebaseOrigin(Vec3 delta)noexcept{InvalidateDirectPreview(true);position_=position_-delta;}
Camera3D ConstrainThirdPersonCameraCeiling(Camera3D camera,std::span<const BoxCollider3D> colliders,float padding,float footprintMargin){
    if(!Finite(camera.position)||!Finite(camera.target)||!std::isfinite(padding)||padding<0||
       !std::isfinite(footprintMargin)||footprintMargin<0)return camera;
    const Vec3 offset=camera.position-camera.target;
    const double horizontal=std::hypot(double(offset.x),double(offset.z));
    const double distance=std::hypot(horizontal,double(offset.y));
    if(offset.y<=0||horizontal<=.001||!std::isfinite(distance))return camera;
    double rise=offset.y;
    for(const auto& collider:colliders){
        if(!Finite(collider.center)||!Finite(collider.extents)||collider.extents.x<0||collider.extents.y<0||collider.extents.z<0)continue;
        const double bottom=double(collider.center.y)-collider.extents.y;
        if(bottom<=camera.target.y||std::abs(double(camera.target.x)-collider.center.x)>double(collider.extents.x)+footprintMargin||
           std::abs(double(camera.target.z)-collider.center.z)>double(collider.extents.z)+footprintMargin)continue;
        rise=(std::min)(rise,(std::max)(0.0,bottom-camera.target.y-padding));
    }
    if(rise>=offset.y)return camera;
    const double scale=std::sqrt((std::max)(0.0,distance*distance-rise*rise))/horizontal;
    const Vec3 candidate=camera.target+Vec3{float(double(offset.x)*scale),float(rise),float(double(offset.z)*scale)};
    if(Finite(candidate))camera.position=candidate;
    return camera;
}
Camera3D ConstrainThirdPersonCamera(Camera3D camera,Vec3 target,std::span<const BoxCollider3D> colliders,float padding){const Vec3 offset=camera.position-target;const float desiredDistance=Length(offset);if(desiredDistance<=.001f)return camera;const Ray3D ray=MakeRay3D(target,offset);float nearest=desiredDistance;for(const BoxCollider3D& collider:colliders){float distance{};if(RayIntersectsAabb(ray,collider.center,collider.extents,nearest,&distance))nearest=(std::min)(nearest,(std::max)(.1f,distance-(std::max)(0.f,padding)));}camera.position=target+ray.direction*nearest;camera.target=target;return camera;}
Camera3D ConstrainThirdPersonCamera(Camera3D camera,Vec3 target,const CollisionWorld3D& world,float padding,uint32_t layerMask,uint32_t cameraLayer){const Vec3 offset=camera.position-target;const float desiredDistance=Length(offset);if(desiredDistance<=.001f)return camera;const Ray3D ray=MakeRay3D(target,offset);const CollisionRayHit3D hit=world.Raycast(ray,desiredDistance,{layerMask,cameraLayer,false});if(hit)camera.position=target+ray.direction*(std::max)(.1f,hit.distance-(std::max)(0.f,padding));camera.target=target;return camera;}
Camera3D ConstrainThirdPersonCamera(Camera3D camera,Vec3 target,const HeightField3D& terrain,float padding,unsigned samples){padding=(std::max)(0.f,padding);samples=(std::max)(2u,samples);const Vec3 offset=camera.position-target;float safeAmount=1.f;for(unsigned sample=1;sample<=samples;++sample){const float amount=float(sample)/float(samples),previous=float(sample-1)/float(samples);const Vec3 point=target+offset*amount;if(terrain.Contains(point.x,point.z)&&point.y<terrain.SampleHeight(point.x,point.z)+padding){safeAmount=previous;break;}}camera.position=target+offset*safeAmount;if(terrain.Contains(camera.position.x,camera.position.z))camera.position.y=(std::max)(camera.position.y,terrain.SampleHeight(camera.position.x,camera.position.z)+padding);camera.target=target;return camera;}
ThirdPersonFramingResult3D ResolveThirdPersonFraming3D(Vec3 target,float yaw,float pitch,float distance,
    std::span<const Vec3> subjectSamples,std::span<const BoxCollider3D> solids,
    std::span<const BoxCollider3D> ceilings,const HeightField3D& terrain,
    std::span<const BoxCollider3D> cameraOnly,Vec2 viewport,float minimumDistance,
    float maximumYawAdjustment,unsigned yawSteps) {
    ThirdPersonFramingResult3D result;
    if(!Finite(target)||!std::isfinite(yaw)||!std::isfinite(pitch)||!std::isfinite(distance)||distance<=0)return result;
    const auto candidate=[&](float adjustment){
        auto camera=MakeThirdPersonCamera(target,yaw+adjustment,pitch,distance);
        camera=ConstrainThirdPersonCameraCeiling(camera,ceilings);
        camera=ConstrainThirdPersonCamera(camera,target,solids);
        camera=ConstrainThirdPersonCameraCeiling(camera,cameraOnly);
        camera=ConstrainThirdPersonCamera(camera,target,cameraOnly);
        return ConstrainThirdPersonCamera(camera,target,terrain);
    };
    result.camera=candidate(0);
    if(subjectSamples.empty()||!std::isfinite(viewport.x)||!std::isfinite(viewport.y)||viewport.x<=0||viewport.y<=0||
       !std::isfinite(minimumDistance)||minimumDistance<=0||!std::isfinite(maximumYawAdjustment)||
       maximumYawAdjustment<0||maximumYawAdjustment>3.1415927f||yawSteps>18)return result;
    for(const auto sample:subjectSamples)if(!Finite(sample))return result;
    for(const auto boxes:{solids,ceilings,cameraOnly})for(const auto& box:boxes)
        if(!Finite(box.center)||!Finite(box.extents)||box.extents.x<0||box.extents.y<0||box.extents.z<0)return result;
    const auto visible=[&](const Camera3D& camera){
        if(!Finite(camera.position)||Length(camera.position-target)<minimumDistance)return false;
        // 射線退讓不是壁面法向間距；初始切鏡也必須滿足固定更新的鏡頭體積／地形安全。
        const float tangent=std::tan(camera.projection.verticalFieldOfView*.5f);
        const float aspect=viewport.x/viewport.y;
        const float radius=(std::max)(.16f,.005f+camera.projection.nearPlane*std::sqrt(1.f+tangent*tangent*(1.f+aspect*aspect)));
        if(!ThirdPersonCameraPathClear3D(camera.position,camera.position,solids,terrain,cameraOnly,radius))return false;
        for(const auto sample:subjectSamples){
            if(!ProjectToViewport(sample,camera,viewport).visible)return false;
            const Vec3 offset=sample-camera.position;
            const float length=Length(offset);
            if(length<=.03f)return false;
            const auto ray=MakeRay3D(camera.position,offset);
            for(const auto boxes:{solids,cameraOnly})for(const auto& box:boxes){
                if(RayIntersectsAabb(ray,box.center,box.extents,length-.03f))return false;
            }
            for(unsigned i=1;i<32;++i){
                const auto point=camera.position+offset*(float(i)/32.f);
                if(terrain.Contains(point.x,point.z)&&point.y<terrain.SampleHeight(point.x,point.z)+.02f)return false;
            }
        }
        return true;
    };
    result.candidatesTested=1;
    if(visible(result.camera)){result.subjectVisible=true;return result;}
    for(unsigned step=1;step<=yawSteps;++step){
        const float amount=maximumYawAdjustment*float(step)/float(yawSteps);
        for(float adjustment:{amount,-amount}){
            const auto camera=candidate(adjustment);++result.candidatesTested;
            if(!visible(camera))continue;
            result.camera=camera;result.yawAdjustment=adjustment;result.subjectVisible=true;return result;
        }
    }
    return result;
}
}
