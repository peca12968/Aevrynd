#include <Aevrynd/Camera.h>
#include <Aevrynd/Camera2D.h>
#include <Aevrynd/Geometry3D.h>
#include <Aevrynd/Input.h>
#include <Aevrynd/Viewport.h>
#include <cmath>
#include <iostream>

using namespace Aevrynd;

constexpr bool Near(float left,float right,float tolerance=.001f){return left>=right-tolerance&&left<=right+tolerance;}
bool Finite(const Matrix4& matrix){for(const auto& row:matrix.values)for(float value:row)if(!std::isfinite(value))return false;return true;}

constexpr Matrix4 Identity=Matrix4::Identity();
static_assert(Identity[0][0]==1.f&&Identity[3][3]==1.f);
static_assert((Identity*Vec4{2,3,4,1}).x==2.f);
static_assert(Dot({1,2,3},{4,5,6})==32.f);
static_assert(Cross({1,0,0},{0,1,0}).z==1.f);

int main(){
    CameraRig2D rig2D;const Vec2 identityScreen=rig2D.WorldToScreen({100,50});if(!Near(identityScreen.x,100)||!Near(identityScreen.y,50)){std::cerr<<"Default 2D camera identity failed\n";return 26;}
    rig2D.SetCamera({{740,410},{1280,720},1.5f,.3f});const Vec2 worldPoint{812,477},screenPoint=rig2D.WorldToScreen(worldPoint),roundTrip=rig2D.ScreenToWorld(screenPoint),transformedPoint=TransformPoint(rig2D.DrawTransform(),worldPoint);if(!Near(roundTrip.x,worldPoint.x,.01f)||!Near(roundTrip.y,worldPoint.y,.01f)||!Near(transformedPoint.x,screenPoint.x,.01f)||!Near(transformedPoint.y,screenPoint.y,.01f)){std::cerr<<"2D camera coordinate round trip failed\n";return 27;}
    const Vec2 parallaxPoint=rig2D.WorldToScreen({640,360},.25f);if(parallaxPoint.x>=640||parallaxPoint.y>=360){std::cerr<<"2D camera parallax failed\n";return 28;}
    rig2D.Reset({1280,720});for(int step=0;step<120;++step)rig2D.Update({740,410},5.f,6.1f,.1f);if(!Near(rig2D.Camera().position.x,740,.01f)||!Near(rig2D.Camera().position.y,410,.01f)||!Near(rig2D.Camera().zoom,2.f,.01f)||std::abs(rig2D.Camera().rotation)>.3f){std::cerr<<"2D camera smoothing or clamping failed\n";return 29;}
    const CanvasViewport wide=CalculateCanvasViewport({1280,720},{1024,768}),fit=CalculateCanvasViewport({1280,720},{853,480});
    if(!Near(wide.x,0)||!Near(wide.y,96)||!Near(wide.width,1024)||!Near(wide.height,576)||!Near(fit.x,0)||!Near(fit.y,.09375f)){std::cerr<<"Canvas letterbox calculation failed\n";return 21;}
    const Vec2 mapped=MapClientToCanvas({512,384},wide);if(!Near(mapped.x,640)||!Near(mapped.y,360)||!wide.Contains({512,384})||wide.Contains({512,32})){std::cerr<<"Canvas input mapping failed\n";return 22;}
    Input input;input.SetMouse({10,10});input.SetMouse({15,7});input.AddMouseWheel(2);
    if(!Near(input.MouseDelta().x,5)||!Near(input.MouseDelta().y,-3)||!Near(input.MouseWheel(),2)){std::cerr<<"Mouse motion accumulation failed\n";return 11;}input.SetMouse({20,20},false);input.SetMouse({30,30},true);if(!Near(input.MouseDelta().x,5)||input.MouseInsideCanvas()!=true){std::cerr<<"Letterbox mouse filtering failed\n";return 23;}input.ConsumePresses();if(!Near(input.MouseDelta().x,0)||!Near(input.MouseWheel(),0)){std::cerr<<"Mouse motion consumption failed\n";return 12;}
    CameraRig3D rig;rig.Reset(CameraViewMode3D::FirstPerson,{0,2,0});Camera3D controlled=rig.Update({{0,0,1},{1,0},0},1.f);if(rig.Mode()!=CameraViewMode3D::FirstPerson||!Near(Length(rig.Anchor()-Vec3{0,2,0}),1.4f)||!Near(rig.Yaw(),.18f)||controlled.projection.mode!=ProjectionMode::Perspective){std::cerr<<"First-person camera rig failed\n";return 13;}
    rig.Reset(CameraViewMode3D::ThirdPerson,{0,1,10});controlled=rig.Update({{}, {},1},.1f);if(!Near(rig.Distance(),36.2f)||!Near(Length(controlled.position-controlled.target),rig.Distance(),.01f)){std::cerr<<"Third-person camera rig failed\n";return 14;}
    rig.Reset(CameraViewMode3D::GodView,{5,0,7});controlled=rig.Update({{1,0,1},{1,0},1},.1f);if(controlled.projection.mode!=ProjectionMode::Orthographic||controlled.position.y<=controlled.target.y||!Finite(CreateViewProjectionMatrix(controlled,16.f/9.f))){std::cerr<<"God-view camera rig failed\n";return 15;}
    rig.Reset(CameraViewMode3D::Isometric,{0,0,0});controlled=rig.Update({{}, {-1,1},-1},.1f);if(controlled.projection.mode!=ProjectionMode::Orthographic||!Near(Length(controlled.position-controlled.target),rig.Distance(),.01f)||!Finite(CreateViewProjectionMatrix(controlled,16.f/9.f))){std::cerr<<"Isometric camera rig failed\n";return 16;}
    const Matrix4 model=CreateTransformMatrix({3,4,5},{.3f,.7f,.2f},{2,4,.5f}),normalMatrix=CreateNormalMatrix(model);
    const Vec3 localNormal=Normalize({1,1,0}),localTangent=Normalize({1,-1,0});
    const Vec3 worldNormal=TransformNormal(normalMatrix,localNormal),worldTangent=Normalize(TransformPoint(model,localTangent)-TransformPoint(model,{}));
    if(!Near(Length(worldNormal),1)||!Near(Dot(worldNormal,worldTangent),0,.002f)){std::cerr<<"Inverse-transpose normal transform failed\n";return 10;}
    Camera3D firstPerson=MakeFirstPersonCamera({0,0,0},0,0);
    const Ray3D centerRay=ScreenPointToRay(firstPerson,{1280,720},{640,360}),rightRay=ScreenPointToRay(firstPerson,{1280,720},{1280,360});if(!Near(centerRay.origin.z,0)||!Near(centerRay.direction.x,0)||!Near(centerRay.direction.z,1)||rightRay.direction.x<=0){std::cerr<<"Perspective screen ray failed\n";return 17;}
    float entryDistance{};if(!RayIntersectsAabb(centerRay,{0,0,5},{1,1,1},100,&entryDistance)||!Near(entryDistance,4)||RayIntersectsAabb(centerRay,{5,0,5},{1,1,1},100)){std::cerr<<"Ray AABB intersection failed\n";return 18;}const RayTriangleHit3D triangleHit=RayIntersectsTriangle(centerRay,{-1,-1,5},{0,1,5},{1,-1,5});if(!triangleHit||!Near(triangleHit.distance,5)||!Near(triangleHit.normal.z,-1)||!Near(triangleHit.barycentric.x+triangleHit.barycentric.y+triangleHit.barycentric.z,1)||!RayIntersectsTriangle(centerRay,{-1,-1,5},{0,1,5},{1,-1,5},100,true)||RayIntersectsTriangle(centerRay,{1,-1,5},{0,1,5},{-1,-1,5},100,true)){std::cerr<<"Ray triangle intersection failed\n";return 19;}
    ProjectedPoint center=ProjectToViewport({0,0,10},firstPerson,{1280,720});
    if(!center.visible||!Near(center.position.x,640)||!Near(center.position.y,360)||center.depth<=0||center.depth>=1){std::cerr<<"First-person projection failed\n";return 1;}
    if(ProjectToViewport({0,0,-10},firstPerson,{1280,720}).visible){std::cerr<<"Point behind camera must be hidden\n";return 2;}
    if(!Finite(CreateViewMatrix(MakeFirstPersonCamera({0,0,0},0,1.5707963f)))){std::cerr<<"Vertical first-person view must remain finite\n";return 7;}
    if(!SphereIntersectsFrustum({0,0,10},1,firstPerson,16.f/9.f)||SphereIntersectsFrustum({100,0,10},1,firstPerson,16.f/9.f)||SphereIntersectsFrustum({0,0,-10},1,firstPerson,16.f/9.f)){std::cerr<<"Perspective sphere frustum test failed\n";return 8;}
    if(!Near(ProjectedSphereRadiusPixels({0,0,10},1,firstPerson,{1280,720}),62.3538f,.01f)||ProjectedSphereRadiusPixels({0,0,-10},1,firstPerson,{1280,720})!=0){std::cerr<<"Perspective projected radius failed\n";return 24;}

    Camera3D thirdPerson=MakeThirdPersonCamera({0,1,0},0,0,10);
    if(!Near(thirdPerson.position.x,0)||!Near(thirdPerson.position.y,1)||!Near(thirdPerson.position.z,-10)){std::cerr<<"Third-person orbit failed\n";return 3;}
    if(!ProjectToViewport(thirdPerson.target,thirdPerson,{1280,720}).visible){std::cerr<<"Third-person target must be visible\n";return 4;}

    Camera3D overhead=MakeOverheadCamera({5,0,7},20,true);
    const Ray3D overheadCenterRay=ScreenPointToRay(overhead,{800,600},{400,300}),overheadCornerRay=ScreenPointToRay(overhead,{800,600},{800,0});if(!Near(overheadCenterRay.direction.y,-1)||!Near(overheadCenterRay.origin.x,5)||Near(overheadCornerRay.origin.x,5)||!Near(Dot(overheadCenterRay.direction,overheadCornerRay.direction),1)){std::cerr<<"Orthographic screen ray failed\n";return 20;}
    ProjectedPoint overheadCenter=ProjectToViewport(overhead.target,overhead,{800,600});
    if(overhead.projection.mode!=ProjectionMode::Orthographic||!overheadCenter.visible||!Near(overheadCenter.position.x,400)||!Near(overheadCenter.position.y,300)){std::cerr<<"Overhead projection failed\n";return 5;}
    if(!SphereIntersectsFrustum(overhead.target,1,overhead,4.f/3.f)||SphereIntersectsFrustum({100,0,7},1,overhead,4.f/3.f)){std::cerr<<"Orthographic sphere frustum test failed\n";return 9;}
    if(!Near(ProjectedSphereRadiusPixels(overhead.target,1,overhead,{800,600}),20)){std::cerr<<"Orthographic projected radius failed\n";return 25;}

    Camera3D isometric=MakeIsometricCamera({0,0,0},30,24);
    if(isometric.projection.mode!=ProjectionMode::Orthographic||!Near(Length(isometric.position-isometric.target),30,.01f)||!ProjectToViewport(isometric.target,isometric,{1280,720}).visible){std::cerr<<"Isometric projection failed\n";return 6;}
    return 0;
}
