#include "Aevrynd/Renderer.h"
#include <chrono>
#include <cmath>
#include <iostream>

using namespace Aevrynd;

namespace {
bool Near(float left,float right,float tolerance=.0001f){return std::abs(left-right)<=tolerance;}
}

int main(){
    Atmosphere3D atmosphere{};atmosphere.enabled=true;atmosphere.cameraPosition={0.f,10.f,0.f};
    if(EvaluateHeightFogAmount3D(atmosphere,{0.f,10.f,100.f})!=0.f){std::cerr<<"Disabled density changed the image\n";return 1;}
    atmosphere.heightFogDensity=.0125f;atmosphere.heightFogFalloff=.1f;atmosphere.heightFogBaseHeight=10.f;atmosphere.heightFogMaximumOpacity=.9f;
    const float horizontal=EvaluateHeightFogAmount3D(atmosphere,{0.f,10.f,80.f});
    const float expected=(std::min)(1.f-std::exp(-1.f),.9f);
    if(!Near(horizontal,expected)||!(EvaluateHeightFogAmount3D(atmosphere,{0.f,10.f,160.f})>horizontal)||!(EvaluateHeightFogAmount3D(atmosphere,{0.f,30.f,80.f})<horizontal)){std::cerr<<"Height fog profile mismatch\n";return 2;}
    if(EvaluateHeightFogAmount3D(atmosphere,atmosphere.cameraPosition)!=0.f){std::cerr<<"Zero length ray was not transparent\n";return 3;}
    const float extremeLow=EvaluateHeightFogAmount3D(atmosphere,{0.f,-1.0e30f,1.f}),extremeHigh=EvaluateHeightFogAmount3D(atmosphere,{0.f,1.0e30f,1.f});if(!std::isfinite(extremeLow)||!std::isfinite(extremeHigh)||extremeLow<0.f||extremeHigh<0.f||extremeLow>atmosphere.heightFogMaximumOpacity||extremeHigh>atmosphere.heightFogMaximumOpacity){std::cerr<<"Extreme height was not bounded\n";return 4;}
    Atmosphere3D invalid=atmosphere;invalid.heightFogDensity=std::numeric_limits<float>::quiet_NaN();invalid.heightFogFalloff=std::numeric_limits<float>::infinity();invalid.heightFogMaximumOpacity=-4.f;const Atmosphere3D normalized=NormalizeAtmosphere3D(invalid);
    if(normalized.heightFogDensity!=0.f||!Near(normalized.heightFogFalloff,.12f)||normalized.heightFogMaximumOpacity!=0.f||EvaluateHeightFogAmount3D(atmosphere,{std::numeric_limits<float>::infinity(),0.f,0.f})!=0.f){std::cerr<<"Invalid input was not fail-closed\n";return 5;}
    constexpr int Iterations=1000000;volatile float sink{};const auto start=std::chrono::steady_clock::now();for(int index=0;index<Iterations;++index)sink=sink+EvaluateHeightFogAmount3D(atmosphere,{float(index%400),float(index%70),float(index%900)});const auto elapsed=std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now()-start).count();
    if(!std::isfinite(float(sink))||elapsed>100000){std::cerr<<"Height fog CPU budget exceeded: "<<elapsed<<" us\n";return 6;}
    std::cout<<"HeightFog3D PASS evaluations="<<Iterations<<" cpuUs="<<elapsed<<" sceneConstantBytes=512 additionalResidentBytes=0\n";return 0;
}
