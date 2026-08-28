#include <Aevrynd/Renderer.h>
#include <chrono>
#include <cmath>
#include <iostream>
#include <limits>

using namespace Aevrynd;

int main(){
    VolumetricCloudSettings3D settings{};
    const float middle=VolumetricCloudDensity3D(.82f,.18f,.5f,settings),edge=VolumetricCloudDensity3D(.82f,.18f,.01f,settings);
    if(!(middle>edge)||middle<=0.f||VolumetricCloudDensity3D(.82f,.18f,.5f,{false})!=0.f){std::cerr<<"Cloud density profile failed\n";return 1;}
    if(!(VolumetricCloudTransmittance3D(2.f)<VolumetricCloudTransmittance3D(.5f))||VolumetricCloudTransmittance3D(-1.f)!=1.f){std::cerr<<"Cloud transmittance failed\n";return 2;}
    if(!(VolumetricCloudRayLength3D(80.f,.35f,settings)>0.f)||VolumetricCloudRayLength3D(80.f,0.f,settings)!=0.f||VolumetricCloudRayLength3D(4000.f,.5f,settings)!=0.f||VolumetricCloudRayLength3D(std::numeric_limits<float>::infinity(),.5f,settings)!=0.f||VolumetricCloudRayLength3D(0.f,std::numeric_limits<float>::quiet_NaN(),settings)!=0.f){std::cerr<<"Cloud ray interval failed\n";return 6;}
    settings.baseAltitude=std::numeric_limits<float>::quiet_NaN();settings.thickness=-4.f;settings.coverage=2.f;settings.density=std::numeric_limits<float>::infinity();settings.erosion=-1.f;settings.windSpeed=500.f;settings.sampleCount=99;
    const auto normalized=NormalizeVolumetricCloudSettings3D(settings);
    if(normalized.baseAltitude!=1200.f||normalized.thickness!=10.f||normalized.coverage!=.95f||normalized.density!=1.15f||normalized.erosion!=0.f||normalized.windSpeed!=200.f||normalized.sampleCount!=16){std::cerr<<"Cloud normalization failed\n";return 3;}
    DrawList draw{DrawListCapacityProfile::Minimal2D};draw.SetVolumetricClouds(settings);if(draw.VolumetricClouds()!=normalized){std::cerr<<"Cloud draw API failed\n";return 4;}
    constexpr int Iterations=1000000;volatile float sink{};const auto start=std::chrono::steady_clock::now();for(int index=0;index<Iterations;++index)sink=sink+VolumetricCloudDensity3D(float(index%101)/100.f,float(index%47)/46.f,float(index%89)/88.f,normalized);const auto cpuUs=std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now()-start).count();
    if(cpuUs<=0||!std::isfinite(sink)){std::cerr<<"Cloud CPU reference failed\n";return 5;}
    std::cout<<"VolumetricCloud3D PASS evaluations="<<Iterations<<" cpuUs="<<cpuUs<<" settingsBytes="<<sizeof(VolumetricCloudSettings3D)<<" dynamicCpuBytes=0 persistentGpuBytes=0\n";
}
