#include <Aevrynd/Renderer.h>
#include <chrono>
#include <cmath>
#include <iostream>
#include <limits>

using namespace Aevrynd;

int main(){
    SkinSubsurfaceSettings3D settings{};
    const Vec3 flat=EvaluateSkinSubsurfaceDiffusion3D(-.18f,.9f,.05f,settings),curved=EvaluateSkinSubsurfaceDiffusion3D(-.18f,.9f,.8f,settings),front=EvaluateSkinSubsurfaceDiffusion3D(1.f,.9f,.8f,settings);
    if(!(flat.x>flat.y&&flat.y>flat.z)||!(curved.x>flat.x)||front.x!=0.f||front.y!=0.f||front.z!=0.f){std::cerr<<"Skin diffusion profile failed\n";return 1;}
    settings.enabled=false;if(Length(EvaluateSkinSubsurfaceDiffusion3D(-.2f,1.f,1.f,settings))!=0.f){std::cerr<<"Disabled skin diffusion was not zero\n";return 2;}
    settings={};settings.strength=std::numeric_limits<float>::quiet_NaN();settings.wrap=std::numeric_limits<float>::infinity();settings.curvatureScale=-std::numeric_limits<float>::infinity();settings.transmission=std::numeric_limits<float>::quiet_NaN();const auto normalized=NormalizeSkinSubsurfaceSettings3D(settings);
    if(!std::isfinite(normalized.strength)||!std::isfinite(normalized.wrap)||!std::isfinite(normalized.curvatureScale)||!std::isfinite(normalized.transmission)||Length(EvaluateSkinSubsurfaceDiffusion3D(std::numeric_limits<float>::infinity(),0.f,0.f,normalized))!=0.f){std::cerr<<"Invalid skin diffusion input was not bounded\n";return 3;}
    DrawList draw{DrawListCapacityProfile::Minimal2D};draw.SetSkinSubsurface(settings);if(draw.SkinSubsurface()!=normalized){std::cerr<<"Skin diffusion draw API failed\n";return 4;}
    constexpr int Iterations=1000000;volatile float sink{};const auto start=std::chrono::steady_clock::now();for(int index=0;index<Iterations;++index){const Vec3 value=EvaluateSkinSubsurfaceDiffusion3D(float(index%201)/100.f-1.f,float(index%101)/100.f,float(index%89)/88.f,{});sink=sink+value.x+value.y+value.z;}const auto cpuUs=std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now()-start).count();
    std::cout<<"SkinSubsurface3D PASS evaluations="<<Iterations<<" cpuUs="<<cpuUs<<" settingsBytes="<<sizeof(SkinSubsurfaceSettings3D)<<" persistentCpuBytes=0 persistentGpuBytes=0\n";return sink<0.f?5:0;
}
