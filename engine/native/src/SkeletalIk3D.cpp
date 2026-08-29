#include <Aevrynd/SkeletalIk3D.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

namespace Aevrynd { namespace {
constexpr float Epsilon=1e-5f;
bool Finite(float value)noexcept{return std::isfinite(value);}
bool Finite(Vec3 value)noexcept{return Finite(value.x)&&Finite(value.y)&&Finite(value.z);}
bool Finite(Quaternion3D value)noexcept{return Finite(value.x)&&Finite(value.y)&&Finite(value.z)&&Finite(value.w);}
bool Finite(const Matrix4& value)noexcept{for(std::size_t row=0;row<4;++row)for(std::size_t column=0;column<4;++column)if(!Finite(value[row][column]))return false;return true;}
float DiagnosticDistance(Vec3 left,Vec3 right)noexcept{const double x=double(left.x)-double(right.x),y=double(left.y)-double(right.y),z=double(left.z)-double(right.z),distance=std::sqrt(x*x+y*y+z*z);return float((std::min)(distance,double((std::numeric_limits<float>::max)())));}
bool Near(const Matrix4& left,const Matrix4& right)noexcept{for(std::size_t row=0;row<4;++row)for(std::size_t column=0;column<4;++column)if(std::abs(left[row][column]-right[row][column])>1e-3f)return false;return true;}
Quaternion3D Multiply(Quaternion3D left,Quaternion3D right)noexcept{return NormalizeQuaternion3D({left.w*right.x+left.x*right.w+left.y*right.z-left.z*right.y,left.w*right.y-left.x*right.z+left.y*right.w+left.z*right.x,left.w*right.z+left.x*right.y-left.y*right.x+left.z*right.w,left.w*right.w-left.x*right.x-left.y*right.y-left.z*right.z});}
Quaternion3D Conjugate(Quaternion3D value)noexcept{return{-value.x,-value.y,-value.z,value.w};}
Quaternion3D LocalRotation(const JointTransform3D& value)noexcept{return value.usesQuaternion?NormalizeQuaternion3D(value.quaternion):QuaternionFromEuler3D(value.rotation);}
// Multiply 對純向量不可正規化，因此向量旋轉使用等價的封閉形式。
Vec3 RotateVector(Quaternion3D q,Vec3 value)noexcept{q=NormalizeQuaternion3D(q);const Vec3 u{q.x,q.y,q.z};return u*(2.f*Dot(u,value))+value*(q.w*q.w-Dot(u,u))+Cross(u,value)*(2.f*q.w);}
Quaternion3D FromTo(Vec3 from,Vec3 to,Vec3 fallbackAxis)noexcept{
    from=Normalize(from);to=Normalize(to);const float dot=Clamp(Dot(from,to),-1.f,1.f);
    if(dot>.999999f)return{};
    if(dot<-.999999f){Vec3 axis=Normalize(Cross(from,fallbackAxis));if(Length(axis)<Epsilon)axis=Normalize(Cross(from,std::abs(from.x)<.8f?Vec3{1,0,0}:Vec3{0,1,0}));return{axis.x,axis.y,axis.z,0};}
    const Vec3 axis=Cross(from,to);return NormalizeQuaternion3D({axis.x,axis.y,axis.z,1.f+dot});
}
bool UniformPositive(Vec3 scale)noexcept{return Finite(scale)&&scale.x>Epsilon&&std::abs(scale.x-scale.y)<1e-4f&&std::abs(scale.x-scale.z)<1e-4f;}
bool ValidTransform(const JointTransform3D& value)noexcept{
    if(!Finite(value.translation)||!Finite(value.rotation)||!UniformPositive(value.scale)||!Finite(value.quaternion))return false;
    if(!value.usesQuaternion)return true;const float length=value.quaternion.x*value.quaternion.x+value.quaternion.y*value.quaternion.y+value.quaternion.z*value.quaternion.z+value.quaternion.w*value.quaternion.w;
    return Finite(length)&&length>Epsilon*Epsilon;
}
bool MatrixRotation(const Matrix4& matrix,Quaternion3D& output)noexcept{
    Vec3 x{matrix[0][0],matrix[1][0],matrix[2][0]},y{matrix[0][1],matrix[1][1],matrix[2][1]},z{matrix[0][2],matrix[1][2],matrix[2][2]};
    const float sx=Length(x),sy=Length(y),sz=Length(z);if(!Finite(sx)||!Finite(sy)||!Finite(sz)||sx<Epsilon||sy<Epsilon||sz<Epsilon)return false;
    x=x*(1.f/sx);y=y*(1.f/sy);z=z*(1.f/sz);if(std::abs(Dot(x,y))>1e-3f||std::abs(Dot(x,z))>1e-3f||std::abs(Dot(y,z))>1e-3f||Dot(Cross(x,y),z)<.999f)return false;
    const float m00=x.x,m01=y.x,m02=z.x,m10=x.y,m11=y.y,m12=z.y,m20=x.z,m21=y.z,m22=z.z,trace=m00+m11+m22;
    Quaternion3D value;
    if(trace>0){const float s=std::sqrt(trace+1.f)*2.f;value={ (m21-m12)/s,(m02-m20)/s,(m10-m01)/s,.25f*s};}
    else if(m00>m11&&m00>m22){const float s=std::sqrt(1.f+m00-m11-m22)*2.f;value={.25f*s,(m01+m10)/s,(m02+m20)/s,(m21-m12)/s};}
    else if(m11>m22){const float s=std::sqrt(1.f+m11-m00-m22)*2.f;value={(m01+m10)/s,.25f*s,(m12+m21)/s,(m02-m20)/s};}
    else{const float s=std::sqrt(1.f+m22-m00-m11)*2.f;value={(m02+m20)/s,(m12+m21)/s,.25f*s,(m10-m01)/s};}
    if(!Finite(value))return false;output=NormalizeQuaternion3D(value);return Finite(output);
}
}

bool SolveTwoBoneIk3D(const Skeleton3D& skeleton,TwoBoneIkChain3D chain,TwoBoneIkTarget3D target,
    AnimationPose3D& pose,TwoBoneIkResult3D* result,Matrix4 poseRoot)noexcept{
    TwoBoneIkResult3D report;auto fail=[&](TwoBoneIkStatus3D status){report.status=status;if(result)*result=report;return false;};
    if(!skeleton.Valid()||skeleton.joints.size()>256)return fail(TwoBoneIkStatus3D::InvalidSkeleton);
    if(chain.upper>=skeleton.joints.size()||chain.middle>=skeleton.joints.size()||chain.end>=skeleton.joints.size()||
        skeleton.joints[chain.middle].parent!=int(chain.upper)||skeleton.joints[chain.end].parent!=int(chain.middle)||
        chain.upper==chain.middle||chain.middle==chain.end)return fail(TwoBoneIkStatus3D::InvalidChain);
    if(pose.localTransforms.size()!=skeleton.joints.size()||pose.globalMatrices.size()!=skeleton.joints.size())return fail(TwoBoneIkStatus3D::InvalidPose);
    if(!Finite(poseRoot))return fail(TwoBoneIkStatus3D::InvalidPose);
    for(std::size_t joint=0;joint<skeleton.joints.size();++joint){
        if(!ValidTransform(pose.localTransforms[joint])||!Finite(pose.globalMatrices[joint]))return fail(TwoBoneIkStatus3D::InvalidPose);
    }
    std::array<std::size_t,256> upperPath{};std::size_t upperPathCount=0,current=chain.upper;
    while(upperPathCount<upperPath.size()){upperPath[upperPathCount++]=current;const int parent=skeleton.joints[current].parent;if(parent<0)break;current=std::size_t(parent);}
    if(!upperPathCount||upperPathCount==upperPath.size()&&skeleton.joints[upperPath[upperPathCount-1]].parent>=0)return fail(TwoBoneIkStatus3D::InvalidPose);
    Matrix4 expected{};for(std::size_t index=upperPathCount;index-->0;){const std::size_t joint=upperPath[index];expected=index==upperPathCount-1?poseRoot*pose.localTransforms[joint].Matrix():expected*pose.localTransforms[joint].Matrix();if(!Finite(expected)||!Near(expected,pose.globalMatrices[joint]))return fail(TwoBoneIkStatus3D::InvalidPose);}
    expected=expected*pose.localTransforms[chain.middle].Matrix();if(!Finite(expected)||!Near(expected,pose.globalMatrices[chain.middle]))return fail(TwoBoneIkStatus3D::InvalidPose);
    expected=expected*pose.localTransforms[chain.end].Matrix();if(!Finite(expected)||!Near(expected,pose.globalMatrices[chain.end]))return fail(TwoBoneIkStatus3D::InvalidPose);
    if(!Finite(target.positionModel)||!Finite(target.poleModel)||!Finite(target.weight)||!Finite(target.softness)||target.weight<0||target.weight>1||target.softness<0||target.softness>1)return fail(TwoBoneIkStatus3D::InvalidTarget);
    const Vec3 upperPosition=TransformPoint(pose.globalMatrices[chain.upper],{}),middlePosition=TransformPoint(pose.globalMatrices[chain.middle],{}),endPosition=TransformPoint(pose.globalMatrices[chain.end],{});
    const float upperLength=Length(middlePosition-upperPosition),lowerLength=Length(endPosition-middlePosition);
    if(!Finite(upperPosition)||!Finite(middlePosition)||!Finite(endPosition)||upperLength<Epsilon||lowerLength<Epsilon)return fail(TwoBoneIkStatus3D::Degenerate);
    if(target.weight==0){report.status=TwoBoneIkStatus3D::Solved;report.solvedPositionModel=endPosition;report.endError=DiagnosticDistance(endPosition,target.positionModel);if(result)*result=report;return true;}
    const Vec3 weightedTarget=endPosition+(target.positionModel-endPosition)*target.weight,targetVector=weightedTarget-upperPosition;float distance=Length(targetVector);
    if(distance<Epsilon)return fail(TwoBoneIkStatus3D::Degenerate);const Vec3 direction=targetVector*(1.f/distance);const float maximum=upperLength+lowerLength-Epsilon,minimum=std::abs(upperLength-lowerLength)+Epsilon;
    float solvedDistance=Clamp(distance,minimum,maximum);report.clamped=solvedDistance!=distance;
    const float softDistance=(upperLength+lowerLength)*target.softness;
    if(softDistance>Epsilon&&solvedDistance>maximum-softDistance){const float start=maximum-softDistance;solvedDistance=start+softDistance*(1.f-std::exp(-(solvedDistance-start)/softDistance));report.clamped=true;}
    Vec3 poleDirection=target.poleModel-upperPosition;Vec3 bendNormal=Normalize(Cross(direction,poleDirection));
    if(Length(bendNormal)<Epsilon){bendNormal=Normalize(Cross(direction,middlePosition-upperPosition));if(Length(bendNormal)<Epsilon)bendNormal=Normalize(Cross(direction,std::abs(direction.y)<.8f?Vec3{0,1,0}:Vec3{1,0,0}));}
    if(Length(bendNormal)<Epsilon)return fail(TwoBoneIkStatus3D::Degenerate);const Vec3 bendDirection=Normalize(Cross(bendNormal,direction));
    const float cosine=Clamp((upperLength*upperLength+solvedDistance*solvedDistance-lowerLength*lowerLength)/(2.f*upperLength*solvedDistance),-1.f,1.f),sine=std::sqrt((std::max)(0.f,1.f-cosine*cosine));
    const Vec3 desiredMiddle=upperPosition+direction*(cosine*upperLength)+bendDirection*(sine*upperLength),desiredEnd=upperPosition+direction*solvedDistance;
    Quaternion3D parentGlobal{},upperGlobal{};const int upperParent=skeleton.joints[chain.upper].parent;
    if(!MatrixRotation(upperParent>=0?pose.globalMatrices[std::size_t(upperParent)]:poseRoot,parentGlobal)||!MatrixRotation(pose.globalMatrices[chain.upper],upperGlobal))return fail(TwoBoneIkStatus3D::InvalidPose);
    const Quaternion3D upperDelta=FromTo(middlePosition-upperPosition,desiredMiddle-upperPosition,bendNormal),newUpperGlobal=Multiply(upperDelta,upperGlobal),newUpperLocal=Multiply(Conjugate(parentGlobal),newUpperGlobal);
    const Vec3 lowerAfterUpper=RotateVector(upperDelta,endPosition-middlePosition);const Quaternion3D middleBaseGlobal=Multiply(newUpperGlobal,LocalRotation(pose.localTransforms[chain.middle]));
    const Quaternion3D middleDelta=FromTo(lowerAfterUpper,desiredEnd-desiredMiddle,bendNormal),newMiddleGlobal=Multiply(middleDelta,middleBaseGlobal),newMiddleLocal=Multiply(Conjugate(newUpperGlobal),newMiddleGlobal);
    if(!Finite(newUpperLocal)||!Finite(newMiddleLocal))return fail(TwoBoneIkStatus3D::InvalidPose);
    JointTransform3D upperTransform=pose.localTransforms[chain.upper],middleTransform=pose.localTransforms[chain.middle];upperTransform.quaternion=newUpperLocal;upperTransform.usesQuaternion=true;middleTransform.quaternion=newMiddleLocal;middleTransform.usesQuaternion=true;
    std::array<Matrix4,256> solvedGlobals{};std::array<bool,256> affected{};
    for(std::size_t joint=0;joint<skeleton.joints.size();++joint){const int parent=skeleton.joints[joint].parent;affected[joint]=joint==chain.upper||(parent>=0&&affected[std::size_t(parent)]);if(!affected[joint]){solvedGlobals[joint]=pose.globalMatrices[joint];continue;}const JointTransform3D& local=joint==chain.upper?upperTransform:joint==chain.middle?middleTransform:pose.localTransforms[joint];solvedGlobals[joint]=parent<0?poseRoot*local.Matrix():solvedGlobals[std::size_t(parent)]*local.Matrix();if(!Finite(solvedGlobals[joint]))return fail(TwoBoneIkStatus3D::InvalidPose);}
    const Vec3 solvedPosition=TransformPoint(solvedGlobals[chain.end],{});const float endError=DiagnosticDistance(solvedPosition,target.positionModel);if(!Finite(solvedPosition)||!Finite(endError)||!Finite(bendNormal))return fail(TwoBoneIkStatus3D::InvalidPose);
    pose.localTransforms[chain.upper]=upperTransform;pose.localTransforms[chain.middle]=middleTransform;for(std::size_t joint=0;joint<skeleton.joints.size();++joint)pose.globalMatrices[joint]=solvedGlobals[joint];
    report.solvedPositionModel=solvedPosition;report.bendNormalModel=bendNormal;report.endError=endError;report.status=report.clamped?TwoBoneIkStatus3D::Clamped:TwoBoneIkStatus3D::Solved;
    if(result)*result=report;return true;
}
}
