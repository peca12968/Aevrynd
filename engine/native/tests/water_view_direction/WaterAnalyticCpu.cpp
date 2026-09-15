#include "Aevrynd/Math.h"
#include <algorithm>
#include <cmath>
#include <iostream>
using namespace Aevrynd;
Vec3 operator*(Vec3 a,Vec3 b){return{a.x*b.x,a.y*b.y,a.z*b.z};}
float dot(Vec2 a,Vec2 b){return a.x*b.x+a.y*b.y;}
float clamp(float a,float lo,float hi){return std::clamp(a,lo,hi);}
float max(float a,float b){return std::max(a,b);}
float min(float a,float b){return std::min(a,b);}
float saturate(float a){return clamp(a,0.f,1.f);}
float lerp(float a,float b,float w){return a+(b-a)*w;}
Vec3 lerp(Vec3 a,Vec3 b,float w){return a+(b-a)*w;}
Vec3 WaterOrderedScaleAdd3D(Vec3 a,float aw,Vec3 b,float bw){return a*aw+b*bw;}
Vec3 WaterOrderedMultiply3D(Vec3 a,Vec3 b){return a*b;}
Vec3 WaterOrderedLerp3D(Vec3 a,Vec3 b,float w){return a*(1.f-w)+b*w;}
#include "WaterAnalyticExtract.inc"

static Vec3 WarpedDetail(Vec2 p,float time,float broadScale,float broadStrength,float detailScale,float detailStrength){
 const Vec2 tangent{.28f,.96f},across{-.96f,.28f};
 const auto broad=WaterAnalyticWave3D(p,time,broadScale,broadStrength,.18f,tangent,.31f);
 const float distance=WaterDomainWarpDistance3D(broadScale,detailScale);
 const Vec2 warped=WaterDomainWarpPosition3D(p,across,broad,broadStrength,distance);
 auto detail=WaterAnalyticWave3D(warped,time,detailScale,detailStrength,.43f,tangent*.965925826f-across*.258819045f,4.79f);
 const Vec2 gradient=WaterDomainWarpGradient3D({detail.y,detail.z},across,broad,broadStrength,distance);
 detail.y=gradient.x;detail.z=gradient.y;return detail;
}
static Vec2 ProductionGradient(Vec2 p,float time){
 const Vec2 tangent{.28f,.96f},across{-.96f,.28f};
 const auto broad=WaterAnalyticWave3D(p,time,.14f,.16f,.18f,tangent,.31f);
 const auto cross=WaterAnalyticWave3D(p,time,.14f*1.31f,.16f*.47f,.18f*.73f,tangent*.923879533f+across*.382683432f,2.03f);
 const auto detail=WarpedDetail(p,time,.14f,.16f,.92f,.055f);
 const float distance=WaterDomainWarpDistance3D(.14f*1.31f,.92f*.71f);
 const Vec2 warped=WaterDomainWarpPosition3D(p,across,cross,.16f*.47f,distance);
 auto crossDetail=WaterAnalyticWave3D(warped,time,.92f*.71f,.055f*.58f,.43f*1.27f,tangent*.866025404f+across*.5f,7.31f);
 const Vec2 crossGradient=WaterDomainWarpGradient3D({crossDetail.y,crossDetail.z},across,cross,.16f*.47f,distance);
 return Vec2{broad.y,broad.z}*.30f+Vec2{cross.y,cross.z}*.17f+Vec2{detail.y,detail.z}*.24f+crossGradient*.29f;
}
int main(){
 const Vec3 dark=WaterLitScatter3D({.009f,.055f,.078f},{.1f,.5f,.6f},{0,0,0});
 const Vec3 day=WaterLitScatter3D({.009f,.055f,.078f},{.1f,.5f,.6f},{1,1,1});
 const Vec3 dusk=WaterLitScatter3D({.009f,.055f,.078f},{.1f,.5f,.6f},{.2f,.1f,.04f});
 bool valid=Length(dark)<1e-7f&&dusk.x<day.x&&dusk.y<day.y&&dusk.z<day.z;
 std::cout<<"unlitScatter="<<Length(dark)<<" analyticPresent="<<WATER_ANALYTIC_PRESENT<<'\n';
#if WATER_ANALYTIC_PRESENT
 const Vec3 env{.2f,.3f,.4f},hit{2,1,.5f};
 valid=valid&&Length(WaterReflectionFallback3D(env,hit,0)-env)<1e-7f&&Length(WaterReflectionFallback3D(env,hit,1)-hit)<1e-7f&&Length(WaterReflectionFallback3D(env,hit,-1)-env)<1e-7f;
 float derivativeError=0,derivativeErrorSum=0,maxSlope=0,maxHeight=0,gradientXX=0,gradientXZ=0,gradientZZ=0;unsigned derivativeSamples=0;
 for(int x=-16;x<=16;++x)for(int z=-16;z<=16;++z){const Vec2 p{x*.31f,z*.27f},dir{.28f,.96f};const auto v=WaterAnalyticWave3D(p,.7f,.18f,.12f,.11f,dir,0.f);const float eps=.01f;const float dx=(WaterAnalyticWave3D({p.x+eps,p.y},.7f,.18f,.12f,.11f,dir,0.f).x-WaterAnalyticWave3D({p.x-eps,p.y},.7f,.18f,.12f,.11f,dir,0.f).x)/(2*eps);const float dz=(WaterAnalyticWave3D({p.x,p.y+eps},.7f,.18f,.12f,.11f,dir,0.f).x-WaterAnalyticWave3D({p.x,p.y-eps},.7f,.18f,.12f,.11f,dir,0.f).x)/(2*eps);const float error=std::max(std::abs(dx-v.y),std::abs(dz-v.z));derivativeError=std::max(derivativeError,error);derivativeErrorSum+=error;++derivativeSamples;maxSlope=std::max(maxSlope,std::hypot(v.y,v.z));maxHeight=std::max(maxHeight,std::abs(v.x));gradientXX+=v.y*v.y;gradientXZ+=v.y*v.z;gradientZZ+=v.z*v.z;}
 const float gradientEnergy=gradientXX+gradientZZ;
 const float directionConcentration=std::sqrt((gradientXX-gradientZZ)*(gradientXX-gradientZZ)+4.f*gradientXZ*gradientXZ)/gradientEnergy;
 const Vec2 legacyDirection{.28f,.96f};const float legacyXX=legacyDirection.x*legacyDirection.x,legacyXZ=legacyDirection.x*legacyDirection.y,legacyZZ=legacyDirection.y*legacyDirection.y;
 const float legacyDirectionConcentration=std::sqrt((legacyXX-legacyZZ)*(legacyXX-legacyZZ)+4.f*legacyXZ*legacyXZ)/(legacyXX+legacyZZ);
 const auto zero=WaterAnalyticWave3D({1,2},3,.18f,0,.11f,{.28f,.96f},0.f);
 float warpDerivativeError=0,warpDerivativeErrorSum=0,minimumJacobian=1.f,warpedXX=0,warpedXZ=0,warpedZZ=0;unsigned warpSamples=0;
 for(int x=-24;x<=24;++x)for(int z=-24;z<=24;++z){const Vec2 p{x*.23f,z*.19f};const auto v=WarpedDetail(p,.7f,.14f,.16f,.92f,.055f);const float eps=.01f;const float dx=(WarpedDetail({p.x+eps,p.y},.7f,.14f,.16f,.92f,.055f).x-WarpedDetail({p.x-eps,p.y},.7f,.14f,.16f,.92f,.055f).x)/(2*eps);const float dz=(WarpedDetail({p.x,p.y+eps},.7f,.14f,.16f,.92f,.055f).x-WarpedDetail({p.x,p.y-eps},.7f,.14f,.16f,.92f,.055f).x)/(2*eps);const float error=std::max(std::abs(dx-v.y),std::abs(dz-v.z));warpDerivativeError=std::max(warpDerivativeError,error);warpDerivativeErrorSum+=error;++warpSamples;const Vec2 production=ProductionGradient(p,.7f);warpedXX+=production.x*production.x;warpedXZ+=production.x*production.y;warpedZZ+=production.y*production.y;const auto driver=WaterAnalyticWave3D(p,.7f,.14f,.16f,.18f,{.28f,.96f},.31f);const float warpDistance=WaterDomainWarpDistance3D(.14f,.92f),inverseStrength=1.f/.16f;const float determinant=1.f+warpDistance*inverseStrength*(-.96f*driver.y+.28f*driver.z);minimumJacobian=std::min(minimumJacobian,determinant);}
 const float warpedEnergy=warpedXX+warpedZZ;
 const float warpedConcentration=std::sqrt((warpedXX-warpedZZ)*(warpedXX-warpedZZ)+4.f*warpedXZ*warpedXZ)/warpedEnergy;
 const float calmRoughness=WaterReflectionRoughness3D(.055f,.20f,0.f,0.f,0.f),activeRoughness=WaterReflectionRoughness3D(.055f,.20f,.12f,.2f,0.f),foamRoughness=WaterReflectionRoughness3D(.055f,.20f,.12f,.2f,1.f);
 const bool roughnessSpatial=calmRoughness>=.05499f&&foamRoughness<=.20001f&&calmRoughness<activeRoughness&&activeRoughness<foamRoughness;
 const bool zeroWarp=Length(WaterDomainWarpPosition3D({1,2},{-.96f,.28f},{.1f,.2f,.3f},0.f,.2f)-Vec2{1,2})<1e-7f;
 const float derivativeMean=derivativeErrorSum/derivativeSamples,warpDerivativeMean=warpDerivativeErrorSum/warpSamples;
 std::cout<<"derivativeError="<<derivativeError<<'/'<<derivativeMean<<" warpDerivativeError="<<warpDerivativeError<<'/'<<warpDerivativeMean<<" minJacobian="<<minimumJacobian<<" maxSlope="<<maxSlope<<" maxHeight="<<maxHeight<<" singleConcentration="<<directionConcentration<<" productionConcentration="<<warpedConcentration<<" legacy="<<legacyDirectionConcentration<<" roughness="<<calmRoughness<<'/'<<activeRoughness<<'/'<<foamRoughness<<" preValid="<<valid<<" zero="<<Length(zero)<<'/'<<zeroWarp<<" roughnessSpatial="<<roughnessSpatial<<'\n';valid=valid&&derivativeMean<.002f&&warpDerivativeMean<.004f&&minimumJacobian>.35f&&maxSlope>.1f&&maxSlope<.14f&&maxHeight<=.12f&&directionConcentration<.92f&&warpedConcentration<.75f&&legacyDirectionConcentration>.95f&&Length(zero)==0&&zeroWarp&&roughnessSpatial;
#else
 valid=false;
#endif
 return valid?0:1;
}
