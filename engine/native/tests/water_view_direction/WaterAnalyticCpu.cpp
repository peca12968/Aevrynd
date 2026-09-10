#include "Aevrynd/Math.h"
#include <algorithm>
#include <cmath>
#include <iostream>
using namespace Aevrynd;
Vec3 operator*(Vec3 a,Vec3 b){return{a.x*b.x,a.y*b.y,a.z*b.z};}
float dot(Vec2 a,Vec2 b){return a.x*b.x+a.y*b.y;}
float clamp(float a,float lo,float hi){return std::clamp(a,lo,hi);}
float max(float a,float b){return std::max(a,b);}
Vec3 lerp(Vec3 a,Vec3 b,float w){return a+(b-a)*w;}
#include "WaterAnalyticExtract.inc"
int main(){
 const Vec3 dark=WaterLitScatter3D({.009f,.055f,.078f},{.1f,.5f,.6f},{0,0,0});
 const Vec3 day=WaterLitScatter3D({.009f,.055f,.078f},{.1f,.5f,.6f},{1,1,1});
 const Vec3 dusk=WaterLitScatter3D({.009f,.055f,.078f},{.1f,.5f,.6f},{.2f,.1f,.04f});
 bool valid=Length(dark)<1e-7f&&dusk.x<day.x&&dusk.y<day.y&&dusk.z<day.z;
 std::cout<<"unlitScatter="<<Length(dark)<<" analyticPresent="<<WATER_ANALYTIC_PRESENT<<'\n';
#if WATER_ANALYTIC_PRESENT
 const Vec3 env{.2f,.3f,.4f},hit{2,1,.5f};
 valid=valid&&Length(WaterReflectionFallback3D(env,hit,0)-env)<1e-7f&&Length(WaterReflectionFallback3D(env,hit,1)-hit)<1e-7f&&Length(WaterReflectionFallback3D(env,hit,-1)-env)<1e-7f;
 float derivativeError=0,maxSlope=0,maxHeight=0,gradientXX=0,gradientXZ=0,gradientZZ=0;
 for(int x=-16;x<=16;++x)for(int z=-16;z<=16;++z){const Vec2 p{x*.31f,z*.27f},dir{.28f,.96f};const auto v=WaterAnalyticWave3D(p,.7f,.18f,.12f,.11f,dir);const float eps=.001f;const float dx=(WaterAnalyticWave3D({p.x+eps,p.y},.7f,.18f,.12f,.11f,dir).x-WaterAnalyticWave3D({p.x-eps,p.y},.7f,.18f,.12f,.11f,dir).x)/(2*eps);const float dz=(WaterAnalyticWave3D({p.x,p.y+eps},.7f,.18f,.12f,.11f,dir).x-WaterAnalyticWave3D({p.x,p.y-eps},.7f,.18f,.12f,.11f,dir).x)/(2*eps);derivativeError=std::max({derivativeError,std::abs(dx-v.y),std::abs(dz-v.z)});maxSlope=std::max(maxSlope,std::hypot(v.y,v.z));maxHeight=std::max(maxHeight,std::abs(v.x));gradientXX+=v.y*v.y;gradientXZ+=v.y*v.z;gradientZZ+=v.z*v.z;}
 const float gradientEnergy=gradientXX+gradientZZ;
 const float directionConcentration=std::sqrt((gradientXX-gradientZZ)*(gradientXX-gradientZZ)+4.f*gradientXZ*gradientXZ)/gradientEnergy;
 const Vec2 legacyDirection{.28f,.96f};const float legacyXX=legacyDirection.x*legacyDirection.x,legacyXZ=legacyDirection.x*legacyDirection.y,legacyZZ=legacyDirection.y*legacyDirection.y;
 const float legacyDirectionConcentration=std::sqrt((legacyXX-legacyZZ)*(legacyXX-legacyZZ)+4.f*legacyXZ*legacyXZ)/(legacyXX+legacyZZ);
 const auto zero=WaterAnalyticWave3D({1,2},3,.18f,0,.11f,{.28f,.96f});
 std::cout<<"derivativeError="<<derivativeError<<" maxSlope="<<maxSlope<<" maxHeight="<<maxHeight<<" directionConcentration="<<directionConcentration<<" legacy="<<legacyDirectionConcentration<<'\n';valid=valid&&derivativeError<.0001f&&maxSlope>.1f&&maxSlope<.14f&&maxHeight<=.12f&&directionConcentration<.75f&&legacyDirectionConcentration>.95f&&Length(zero)==0;
#else
 valid=false;
#endif
 return valid?0:1;
}
