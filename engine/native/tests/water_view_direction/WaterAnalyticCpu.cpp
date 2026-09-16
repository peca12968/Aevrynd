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
 auto detail=WaterAnalyticWave3D(warped,time,detailScale,detailStrength,.43f,tangent*.707106781f-across*.707106781f,4.79f);
 const Vec2 gradient=WaterDomainWarpGradient3D({detail.y,detail.z},across,broad,broadStrength,distance);
 detail.y=gradient.x;detail.z=gradient.y;return detail;
}
static float ProductionBandLimit(float footprint,float scale){
 const float cycles=std::max(0.f,scale)*footprint;
 const float blend=saturate((cycles-.06f)/.16f);
 const float smoothed=blend*blend*(3.f-2.f*blend);
 return 1.f-smoothed;
}
static Vec2 ProductionGradient(Vec2 p,float time,float footprint=0.f){
 const Vec2 tangent{.28f,.96f},across{-.96f,.28f};
 const auto broad=WaterAnalyticWave3D(p,time,.14f,.16f,.18f,tangent,.31f);
 const auto cross=WaterAnalyticWave3D(p,time,.14f*1.31f,.16f*.85f,.18f*.73f,tangent*.573576436f+across*.819152044f,2.03f);
 const auto detail=WarpedDetail(p,time,.14f,.16f,.92f,.055f);
 const float distance=WaterDomainWarpDistance3D(.14f*1.31f,.92f*.71f);
 const Vec2 warped=WaterDomainWarpPosition3D(p,across,cross,.16f*.85f,distance);
 auto crossDetail=WaterAnalyticWave3D(warped,time,.92f*.71f,.055f*.58f,.43f*1.27f,tangent*.087155743f+across*.996194698f,7.31f);
 const Vec2 crossGradient=WaterDomainWarpGradient3D({crossDetail.y,crossDetail.z},across,cross,.16f*.85f,distance);
 const float broadBand=ProductionBandLimit(footprint,.14f*1.91f);
 const float crossBand=ProductionBandLimit(footprint,.14f*1.31f*1.91f);
 const float detailBand=ProductionBandLimit(footprint,.92f*1.91f);
 const float crossDetailBand=ProductionBandLimit(footprint,.92f*.71f*1.91f);
 const float lowFrequencyDiversity=std::min(broadBand,crossBand);
 return Vec2{broad.y,broad.z}*(.27f*lowFrequencyDiversity)
       +Vec2{cross.y,cross.z}*(.27f*lowFrequencyDiversity)
       +Vec2{detail.y,detail.z}*(.22f*detailBand)
       +crossGradient*(.24f*crossDetailBand);
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
 float maximumVertexDisplacement=0,maximumHostileVertexDisplacement=0,vertexTemporalDelta=0;bool shorelinePinned=true;
  for(int x=-32;x<=32;++x)for(int z=-32;z<=32;++z){const Vec2 point{x*.37f,z*.29f};const float first=WaterVertexDisplacementHeight3D(point,.7f,.14f,.16f,.18f),second=WaterVertexDisplacementHeight3D(point,1.1f,.14f,.16f,.18f);maximumVertexDisplacement=std::max(maximumVertexDisplacement,std::max(std::abs(first),std::abs(second)));vertexTemporalDelta=std::max(vertexTemporalDelta,std::abs(first-second));}
  for(float time:{-8.f,0.f,8.f})for(int x=-12;x<=12;++x)for(int z=-12;z<=12;++z){const float hostile=WaterVertexDisplacementHeight3D({x*7.13f,z*5.71f},time,8.f,.3f,8.f);maximumHostileVertexDisplacement=std::max(maximumHostileVertexDisplacement,std::abs(hostile));shorelinePinned=shorelinePinned&&hostile*saturate(-1.f)==0.f&&hostile*saturate(0.f)==0.f&&std::abs(hostile*saturate(2.f))<=.050001f;}
 float warpDerivativeError=0,warpDerivativeErrorSum=0,minimumJacobian=1.f,maximumProductionSlope=0,warpedXX=0,warpedXZ=0,warpedZZ=0;unsigned warpSamples=0;
 for(int x=-24;x<=24;++x)for(int z=-24;z<=24;++z){const Vec2 p{x*.23f,z*.19f};const auto v=WarpedDetail(p,.7f,.14f,.16f,.92f,.055f);const float eps=.01f;const float dx=(WarpedDetail({p.x+eps,p.y},.7f,.14f,.16f,.92f,.055f).x-WarpedDetail({p.x-eps,p.y},.7f,.14f,.16f,.92f,.055f).x)/(2*eps);const float dz=(WarpedDetail({p.x,p.y+eps},.7f,.14f,.16f,.92f,.055f).x-WarpedDetail({p.x,p.y-eps},.7f,.14f,.16f,.92f,.055f).x)/(2*eps);const float error=std::max(std::abs(dx-v.y),std::abs(dz-v.z));warpDerivativeError=std::max(warpDerivativeError,error);warpDerivativeErrorSum+=error;++warpSamples;const Vec2 production=ProductionGradient(p,.7f);maximumProductionSlope=std::max(maximumProductionSlope,std::hypot(production.x,production.y));warpedXX+=production.x*production.x;warpedXZ+=production.x*production.y;warpedZZ+=production.y*production.y;const auto driver=WaterAnalyticWave3D(p,.7f,.14f,.16f,.18f,{.28f,.96f},.31f);const float warpDistance=WaterDomainWarpDistance3D(.14f,.92f),inverseStrength=1.f/.16f;const float determinant=1.f+warpDistance*inverseStrength*(-.96f*driver.y+.28f*driver.z);minimumJacobian=std::min(minimumJacobian,determinant);}
 const float warpedEnergy=warpedXX+warpedZZ;
 const float warpedConcentration=std::sqrt((warpedXX-warpedZZ)*(warpedXX-warpedZZ)+4.f*warpedXZ*warpedXZ)/warpedEnergy;
 float previousRms=1e9f,maximumFilteredConcentration=0.f;bool footprintMonotonic=true,terminalDiversity=true;
 for(int footprintIndex=0;footprintIndex<=30;++footprintIndex){const float footprint=footprintIndex*.05f;double energy=0,xx=0,xz=0,zz=0;unsigned samples=0;for(float time:{.0f,.7f,1.9f})for(int x=-12;x<=12;++x)for(int z=-12;z<=12;++z){const Vec2 g=ProductionGradient({x*.29f,z*.23f},time,footprint);const float e=g.x*g.x+g.y*g.y;energy+=e;xx+=g.x*g.x;xz+=g.x*g.y;zz+=g.y*g.y;++samples;}const float rms=std::sqrt(float(energy/samples));const float tensorEnergy=float(xx+zz);const float concentration=tensorEnergy>1e-12f?std::sqrt(float((xx-zz)*(xx-zz)+4.0*xz*xz))/tensorEnergy:0.f;if(rms>=.01f)maximumFilteredConcentration=std::max(maximumFilteredConcentration,concentration);if(rms>previousRms+1e-5f)footprintMonotonic=false;previousRms=rms;const float crossBand=ProductionBandLimit(footprint,.14f*1.31f*1.91f),broadBand=ProductionBandLimit(footprint,.14f*1.91f);if(crossBand==0.f&&broadBand!=0.f){const float shared=std::min(broadBand,crossBand);terminalDiversity=terminalDiversity&&shared==0.f;}}
 const float calmRoughness=WaterReflectionRoughness3D(.055f,.20f,0.f,0.f,0.f),activeRoughness=WaterReflectionRoughness3D(.055f,.20f,.12f,.2f,0.f),foamRoughness=WaterReflectionRoughness3D(.055f,.20f,.12f,.2f,1.f);
 const bool roughnessSpatial=calmRoughness>=.05499f&&foamRoughness<=.20001f&&calmRoughness<activeRoughness&&activeRoughness<foamRoughness;
 const bool zeroWarp=Length(WaterDomainWarpPosition3D({1,2},{-.96f,.28f},{.1f,.2f,.3f},0.f,.2f)-Vec2{1,2})<1e-7f;
 const float derivativeMean=derivativeErrorSum/derivativeSamples,warpDerivativeMean=warpDerivativeErrorSum/warpSamples;
 std::cout<<"derivativeError="<<derivativeError<<'/'<<derivativeMean<<" warpDerivativeError="<<warpDerivativeError<<'/'<<warpDerivativeMean<<" minJacobian="<<minimumJacobian<<" maxSlope="<<maxSlope<<" maxProductionSlope="<<maximumProductionSlope<<" maxHeight="<<maxHeight<<" vertexDisplacement="<<maximumVertexDisplacement<<'/'<<maximumHostileVertexDisplacement<<" shorelinePinned="<<shorelinePinned<<" temporalDelta="<<vertexTemporalDelta<<" singleConcentration="<<directionConcentration<<" productionConcentration="<<warpedConcentration<<" filteredConcentration="<<maximumFilteredConcentration<<" footprintMonotonic="<<footprintMonotonic<<" terminalDiversity="<<terminalDiversity<<" legacy="<<legacyDirectionConcentration<<" roughness="<<calmRoughness<<'/'<<activeRoughness<<'/'<<foamRoughness<<" preValid="<<valid<<" zero="<<Length(zero)<<'/'<<zeroWarp<<" roughnessSpatial="<<roughnessSpatial<<'\n';valid=valid&&derivativeMean<.002f&&warpDerivativeMean<.004f&&minimumJacobian>.35f&&maxSlope>.1f&&maxSlope<.14f&&maximumProductionSlope>.08f&&maximumProductionSlope<.30f&&maxHeight<=.12f&&maximumVertexDisplacement>=.03f&&maximumVertexDisplacement<=.050001f&&maximumHostileVertexDisplacement<=.050001f&&shorelinePinned&&vertexTemporalDelta>.001f&&directionConcentration<.92f&&warpedConcentration<=.60f&&maximumFilteredConcentration<=.70f&&footprintMonotonic&&terminalDiversity&&legacyDirectionConcentration>.95f&&Length(zero)==0&&zeroWarp&&roughnessSpatial;
#else
 valid=false;
#endif
 return valid?0:1;
}
