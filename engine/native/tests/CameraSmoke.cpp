#include <Aevrynd/Camera.h>
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
    Camera3D firstPerson=MakeFirstPersonCamera({0,0,0},0,0);
    ProjectedPoint center=ProjectToViewport({0,0,10},firstPerson,{1280,720});
    if(!center.visible||!Near(center.position.x,640)||!Near(center.position.y,360)||center.depth<=0||center.depth>=1){std::cerr<<"First-person projection failed\n";return 1;}
    if(ProjectToViewport({0,0,-10},firstPerson,{1280,720}).visible){std::cerr<<"Point behind camera must be hidden\n";return 2;}
    if(!Finite(CreateViewMatrix(MakeFirstPersonCamera({0,0,0},0,1.5707963f)))){std::cerr<<"Vertical first-person view must remain finite\n";return 7;}
    if(!SphereIntersectsFrustum({0,0,10},1,firstPerson,16.f/9.f)||SphereIntersectsFrustum({100,0,10},1,firstPerson,16.f/9.f)||SphereIntersectsFrustum({0,0,-10},1,firstPerson,16.f/9.f)){std::cerr<<"Perspective sphere frustum test failed\n";return 8;}

    Camera3D thirdPerson=MakeThirdPersonCamera({0,1,0},0,0,10);
    if(!Near(thirdPerson.position.x,0)||!Near(thirdPerson.position.y,1)||!Near(thirdPerson.position.z,-10)){std::cerr<<"Third-person orbit failed\n";return 3;}
    if(!ProjectToViewport(thirdPerson.target,thirdPerson,{1280,720}).visible){std::cerr<<"Third-person target must be visible\n";return 4;}

    Camera3D overhead=MakeOverheadCamera({5,0,7},20,true);
    ProjectedPoint overheadCenter=ProjectToViewport(overhead.target,overhead,{800,600});
    if(overhead.projection.mode!=ProjectionMode::Orthographic||!overheadCenter.visible||!Near(overheadCenter.position.x,400)||!Near(overheadCenter.position.y,300)){std::cerr<<"Overhead projection failed\n";return 5;}
    if(!SphereIntersectsFrustum(overhead.target,1,overhead,4.f/3.f)||SphereIntersectsFrustum({100,0,7},1,overhead,4.f/3.f)){std::cerr<<"Orthographic sphere frustum test failed\n";return 9;}

    Camera3D isometric=MakeIsometricCamera({0,0,0},30,24);
    if(isometric.projection.mode!=ProjectionMode::Orthographic||!Near(Length(isometric.position-isometric.target),30,.01f)||!ProjectToViewport(isometric.target,isometric,{1280,720}).visible){std::cerr<<"Isometric projection failed\n";return 6;}
    return 0;
}
