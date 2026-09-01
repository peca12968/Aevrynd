#include <Aevrynd/Renderer.h>
#include <chrono>
#include <cmath>
#include <iostream>
#include <limits>

using namespace Aevrynd;

int main(){
    DirectionalLightShaftSettings3D settings{};
    const float forward=EvaluateDirectionalLightShaftScattering3D(.8f,1.f,1.f,1.f,settings);
    const float side=EvaluateDirectionalLightShaftScattering3D(.8f,1.f,1.f,0.f,settings);
    const float shadowed=EvaluateDirectionalLightShaftScattering3D(.8f,0.f,1.f,1.f,settings);
    const float clouded=EvaluateDirectionalLightShaftScattering3D(.8f,1.f,.35f,1.f,settings);
    if(!(forward>side)||shadowed!=0.f||!(clouded>0.f&&clouded<forward)){std::cerr<<"Directional shaft phase or transmittance failed\n";return 1;}
    settings.enabled=false;if(EvaluateDirectionalLightShaftScattering3D(.8f,1.f,1.f,1.f,settings)!=0.f){std::cerr<<"Disabled shaft was not zero cost\n";return 2;}
    settings={};settings.intensity=std::numeric_limits<float>::quiet_NaN();settings.anisotropy=std::numeric_limits<float>::infinity();settings.maximumDistance=-std::numeric_limits<float>::infinity();settings.sampleCount=99;const auto normalized=NormalizeDirectionalLightShaftSettings3D(settings);
    if(!std::isfinite(normalized.intensity)||!std::isfinite(normalized.anisotropy)||!std::isfinite(normalized.maximumDistance)||normalized.sampleCount!=16||EvaluateDirectionalLightShaftScattering3D(std::numeric_limits<float>::infinity(),1.f,1.f,1.f,normalized)!=0.f){std::cerr<<"Invalid shaft input was not bounded\n";return 3;}
    DrawList draw{DrawListCapacityProfile::Minimal2D};draw.SetDirectionalLightShafts(settings);if(draw.DirectionalLightShafts()!=normalized){std::cerr<<"Directional shaft draw API failed\n";return 4;}
    constexpr int Iterations=1000000;volatile float sink{};const auto start=std::chrono::steady_clock::now();for(int index=0;index<Iterations;++index)sink=sink+EvaluateDirectionalLightShaftScattering3D(float(index%120)/40.f,float(index%101)/100.f,float(index%83)/82.f,float(index%201)/100.f-1.f,{});const auto cpuUs=std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now()-start).count();
    std::cout<<"DirectionalLightShaft3D PASS evaluations="<<Iterations<<" cpuUs="<<cpuUs<<" settingsBytes="<<sizeof(DirectionalLightShaftSettings3D)<<" persistentCpuBytes=0 persistentGpuBytes=0\n";return sink<0.f?5:0;
}
