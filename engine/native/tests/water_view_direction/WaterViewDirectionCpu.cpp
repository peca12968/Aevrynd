#include <Aevrynd/Camera.h>
#include <iostream>
using namespace Aevrynd;
#include "WaterDirection.inc"
int main(){
 const Vec3 identityPoint{.25f,.5f,.75f},identityCamera{0,0,0};
 const Vec3 identityExpected=Normalize(identityCamera-identityPoint);
 if(Length(WaterSurfaceViewDirection(identityPoint,identityCamera,Matrix4::Identity())-identityExpected)>1e-6f){std::cerr<<"FAIL identity clip fallback";return 3;}
 Camera3D camera{{8,12,-15},{1,0,3},{0,1,0}};camera.projection.mode=ProjectionMode::Orthographic;
 const auto matrix=CreateViewProjectionMatrix(camera,16.f/9.f);const auto expected=Normalize(camera.position-camera.target);
 float maximum=0;for(int z=-20;z<=20;++z)for(int x=-20;x<=20;++x){const Vec3 p{float(x),-.42f,float(z)};maximum=(std::max)(maximum,Length(WaterSurfaceViewDirection(p,camera.position,matrix)-expected));}
 if(maximum>1e-5f){std::cerr<<"FAIL orthographic variation="<<maximum;return 1;}
 camera.projection.mode=ProjectionMode::Perspective;const auto perspective=CreateViewProjectionMatrix(camera,16.f/9.f);
 const Vec3 a{-10,0,-10},b{10,0,10};const auto da=WaterSurfaceViewDirection(a,camera.position,perspective),db=WaterSurfaceViewDirection(b,camera.position,perspective);
 if(Length(da-db)<.1f||Length(da-Normalize(camera.position-a))>1e-6f)return 2;
 std::cout<<"PASS orthographic 1681 samples maxError="<<maximum<<" perspective delta="<<Length(da-db)<<" reflection retained GPU=false\n";
}
