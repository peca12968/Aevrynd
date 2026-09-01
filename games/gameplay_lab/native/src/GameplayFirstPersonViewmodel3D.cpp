#include "GameplayFirstPersonViewmodel3D.h"
#include <algorithm>
#include <cmath>
#include <limits>

using namespace Aevrynd;
namespace GameplayLab {
namespace {
constexpr float Width=1280.f,Height=720.f;
constexpr std::array<Vec3,8> WeaponEnvelope={Vec3{-.14f,-.15f,-.06f},Vec3{.14f,-.15f,-.06f},Vec3{-.14f,.68f,-.06f},Vec3{.14f,.68f,-.06f},Vec3{-.14f,-.15f,.06f},Vec3{.14f,-.15f,.06f},Vec3{-.14f,.68f,.06f},Vec3{.14f,.68f,.06f}};
// 雙臂包絡比武器握把寬，專門防止肩部切口滑出畫面。
struct Bounds {float minimumX{Width},minimumY{Height},maximumX{},maximumY{},minimumDepth{std::numeric_limits<float>::max()};};
void Include(Bounds& bounds,const Camera3D& camera,Vec3 world)noexcept{
    const auto point=ProjectToViewport(world,camera,{std::uint32_t(Width),std::uint32_t(Height)});
    bounds.minimumX=(std::min)(bounds.minimumX,point.position.x);bounds.minimumY=(std::min)(bounds.minimumY,point.position.y);
    bounds.maximumX=(std::max)(bounds.maximumX,point.position.x);bounds.maximumY=(std::max)(bounds.maximumY,point.position.y);
    bounds.minimumDepth=(std::min)(bounds.minimumDepth,TransformPoint(CreateViewMatrix(camera),world).z);
}
bool MarginClear(const Bounds& bounds,float margin)noexcept{return bounds.minimumX>=margin&&bounds.maximumX<=Width-margin&&bounds.minimumY>=margin&&bounds.maximumY<=Height-margin;}
bool InverseAffine(const Matrix4& source,Matrix4& inverse)noexcept{
    const double a=source[0][0],b=source[0][1],c=source[0][2],d=source[1][0],e=source[1][1],f=source[1][2],g=source[2][0],h=source[2][1],i=source[2][2],det=a*(e*i-f*h)-b*(d*i-f*g)+c*(d*h-e*g);if(!std::isfinite(det)||std::abs(det)<1e-12)return false;
    inverse=Matrix4::Identity();inverse[0][0]=float((e*i-f*h)/det);inverse[0][1]=float((c*h-b*i)/det);inverse[0][2]=float((b*f-c*e)/det);inverse[1][0]=float((f*g-d*i)/det);inverse[1][1]=float((a*i-c*g)/det);inverse[1][2]=float((c*d-a*f)/det);inverse[2][0]=float((d*h-e*g)/det);inverse[2][1]=float((b*g-a*h)/det);inverse[2][2]=float((a*e-b*d)/det);const float tx=source[0][3],ty=source[1][3],tz=source[2][3];inverse[0][3]=-(inverse[0][0]*tx+inverse[0][1]*ty+inverse[0][2]*tz);inverse[1][3]=-(inverse[1][0]*tx+inverse[1][1]*ty+inverse[1][2]*tz);inverse[2][3]=-(inverse[2][0]*tx+inverse[2][1]*ty+inverse[2][2]*tz);return true;
}
}

GameplayFirstPersonViewmodelLayout3D GameplayFirstPersonViewmodelLayout(GameplayViewmodelSex3D sex)noexcept{
    if(sex==GameplayViewmodelSex3D::Female)return {{.63f,-.10f,.92f},{-.12f,.08f,-.18f},.264f,.38f,-.54f,.505f,2.1f,.75f};
    return {{.6165f,-.11f,.96f},{-.11f,.40f,-.16f},.222f,.305f,-.54f,.50f,2.25f,.82f};
}
Matrix4 GameplayFirstPersonDesiredGripTransform3D(const GameplayFirstPersonViewmodelLayout3D& layout)noexcept{
    return CreateTransformMatrix(layout.gripTranslation,layout.gripRotation,{layout.weaponScale,layout.weaponScale,layout.weaponScale});
}
bool ComposeGameplayFirstPersonWeaponWorld3D(const Camera3D& camera,const Matrix4& modelHandSocket,const GameplayFirstPersonViewmodelLayout3D& layout,bool arms,Matrix4& weaponWorld)noexcept{
    const Vec3 forward=Normalize(camera.target-camera.position),right=Normalize(Cross(camera.up,forward)),up=Normalize(Cross(forward,right));if(Length(forward)<.001f||Length(right)<.001f||Length(up)<.001f)return false;
    Matrix4 cameraWorld=Matrix4::Identity();cameraWorld[0][0]=right.x;cameraWorld[1][0]=right.y;cameraWorld[2][0]=right.z;cameraWorld[0][1]=up.x;cameraWorld[1][1]=up.y;cameraWorld[2][1]=up.z;cameraWorld[0][2]=forward.x;cameraWorld[1][2]=forward.y;cameraWorld[2][2]=forward.z;cameraWorld[0][3]=camera.position.x;cameraWorld[1][3]=camera.position.y;cameraWorld[2][3]=camera.position.z;
    const Matrix4 modelGripSocket=modelHandSocket*CreateTranslationMatrix({0,.105f,.02f});Matrix4 inverse{};if(!InverseAffine(modelGripSocket,inverse))return false;const float scale=arms?layout.armsScale:layout.weaponScale;weaponWorld=cameraWorld*CreateTransformMatrix(layout.gripTranslation,layout.gripRotation,{scale,scale,scale})*inverse;return true;
}
GameplayFirstPersonViewmodelProjection3D EvaluateGameplayFirstPersonViewmodelProjection3D(const Camera3D& camera,const Matrix4& handSocketWorld,const GameplayFirstPersonViewmodelLayout3D& layout)noexcept{
    GameplayFirstPersonViewmodelProjection3D result{};Bounds weapon{},arms{};
    for(const auto local:WeaponEnvelope)Include(weapon,camera,TransformPoint(handSocketWorld,local));
    const std::array<Vec3,8> armsEnvelope={Vec3{layout.armsMinimumX,-.30f,-.15f},Vec3{layout.armsMaximumX,-.30f,-.15f},Vec3{layout.armsMinimumX,.38f,-.15f},Vec3{layout.armsMaximumX,.38f,-.15f},Vec3{layout.armsMinimumX,-.30f,.15f},Vec3{layout.armsMaximumX,-.30f,.15f},Vec3{layout.armsMinimumX,.38f,.15f},Vec3{layout.armsMaximumX,.38f,.15f}};
    for(const auto local:armsEnvelope)Include(arms,camera,TransformPoint(handSocketWorld,local));
    const float weaponWidth=(std::max)(0.f,weapon.maximumX-weapon.minimumX),weaponHeight=(std::max)(0.f,weapon.maximumY-weapon.minimumY);
    result.weaponCoverage=weaponWidth*weaponHeight/(Width*Height);result.minimumDepth=weapon.minimumDepth;
    result.weaponMinimumX=weapon.minimumX;result.weaponMinimumY=weapon.minimumY;result.weaponMaximumX=weapon.maximumX;result.weaponMaximumY=weapon.maximumY;
    result.armsMinimumX=arms.minimumX;result.armsMinimumY=arms.minimumY;result.armsMaximumX=arms.maximumX;result.armsMaximumY=arms.maximumY;
    const auto grip=ProjectToViewport(TransformPoint(handSocketWorld,{0,.105f,.02f}),camera,{1280,720});
    const auto blade=ProjectToViewport(TransformPoint(handSocketWorld,{0,.65f,0}),camera,{1280,720});
    result.gripX=grip.position.x;result.gripY=grip.position.y;result.bladeY=blade.position.y;
    result.centerExcluded=!(weapon.minimumX<=640&&weapon.maximumX>=640&&weapon.minimumY<=360&&weapon.maximumY>=360);
    result.nearClipClear=weapon.minimumDepth>camera.projection.nearPlane+.015f;
    const bool topHud=weapon.minimumY<66&&weapon.maximumY>0,paletteHud=weapon.minimumX<372&&weapon.maximumX>0&&weapon.minimumY<720&&weapon.maximumY>470,rightHud=weapon.minimumX<1272&&weapon.maximumX>778&&weapon.minimumY<638&&weapon.maximumY>512;
    result.hudClear=!(topHud||paletteHud||rightHud);result.gripEdgeAnchored=result.gripX>=1040&&result.gripX<=1264&&result.gripY>=80&&result.gripY<=510;
    result.bladeAboveGrip=result.bladeY+80<result.gripY;result.weaponMarginClear=MarginClear(weapon,16);result.armsMarginClear=MarginClear(arms,16);
    return result;
}
bool GameplayFirstPersonViewmodelProjection3D::Complete()const noexcept{
    return std::isfinite(weaponCoverage)&&weaponCoverage>=.02f&&weaponCoverage<=.06f&&std::isfinite(minimumDepth)&&centerExcluded&&nearClipClear&&hudClear&&gripEdgeAnchored&&bladeAboveGrip&&weaponMarginClear&&armsMarginClear;
}
}
