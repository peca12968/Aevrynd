#include <Aevrynd/Renderer.h>
#include <chrono>
#include <cmath>
#include <iostream>
#include <limits>

using namespace Aevrynd;

int main(){
    ScreenSpaceReflectionSettings3D settings{};
    float previous{};for(uint32_t step=0;step<settings.sampleCount;++step){const float distance=ScreenSpaceReflectionStepDistance3D(step,settings);if(distance<=previous||distance>settings.maximumDistance){std::cerr<<"SSR step bounds failed\n";return 1;}previous=distance;}
    const float confidence=ScreenSpaceReflectionConfidence3D(.08f,.9f,.06f,settings);if(confidence<=0.f||confidence>1.f||ScreenSpaceReflectionConfidence3D(settings.thickness+.01f,1.f,0.f,settings)!=0.f||ScreenSpaceReflectionConfidence3D(0.f,1.f,settings.maximumRoughness,settings)!=0.f){std::cerr<<"SSR confidence bounds failed\n";return 2;}
    settings.enabled=false;if(ScreenSpaceReflectionStepDistance3D(0,settings)!=0.f||ScreenSpaceReflectionConfidence3D(0.f,1.f,0.f,settings)!=0.f){std::cerr<<"Disabled SSR was not zero\n";return 3;}
    settings={};settings.intensity=std::numeric_limits<float>::quiet_NaN();settings.maximumDistance=std::numeric_limits<float>::infinity();settings.thickness=-std::numeric_limits<float>::infinity();settings.maximumRoughness=std::numeric_limits<float>::quiet_NaN();settings.sampleCount=UINT32_MAX;const auto normalized=NormalizeScreenSpaceReflectionSettings3D(settings);if(!std::isfinite(normalized.intensity)||!std::isfinite(normalized.maximumDistance)||!std::isfinite(normalized.thickness)||!std::isfinite(normalized.maximumRoughness)||normalized.sampleCount!=16||ScreenSpaceReflectionConfidence3D(std::numeric_limits<float>::quiet_NaN(),1.f,0.f,normalized)!=0.f){std::cerr<<"Invalid SSR input was not bounded\n";return 4;}
    DrawList draw{DrawListCapacityProfile::Minimal2D};draw.SetScreenSpaceReflections(settings);if(draw.ScreenSpaceReflections()!=normalized){std::cerr<<"SSR draw API failed\n";return 5;}
    constexpr int Iterations=1000000;volatile float sink{};const auto start=std::chrono::steady_clock::now();for(int index=0;index<Iterations;++index)sink=sink+ScreenSpaceReflectionConfidence3D(float(index%70)*.01f,float(index%101)*.01f,float(index%29)*.01f,{});const auto cpuUs=std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now()-start).count();
    std::cout<<"ScreenSpaceReflection3D PASS evaluations="<<Iterations<<" cpuUs="<<cpuUs<<" settingsBytes="<<sizeof(ScreenSpaceReflectionSettings3D)<<" persistentCpuBytes=0 persistentGpuBytes=0\n";return sink<0.f?6:0;
}
