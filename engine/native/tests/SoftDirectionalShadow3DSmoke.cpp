#include <Aevrynd/Renderer.h>
#include <cmath>
#include <iostream>
#include <limits>

using namespace Aevrynd;

int main(){
    SoftDirectionalShadowSettings3D defaults{};
    if(!defaults.enabled||defaults.blockerSearchRadiusTexels!=24.f||defaults.lightSourceRadiusTexels!=128.f||defaults.maximumPenumbraRadiusTexels!=32.f){std::cerr<<"Soft shadow defaults failed\n";return 1;}
    SoftDirectionalShadowSettings3D invalid{true,std::numeric_limits<float>::quiet_NaN(),std::numeric_limits<float>::infinity(),-8.f};
    invalid=NormalizeSoftDirectionalShadowSettings3D(invalid);
    if(invalid.blockerSearchRadiusTexels!=24.f||invalid.lightSourceRadiusTexels!=128.f||invalid.maximumPenumbraRadiusTexels!=1.45f){std::cerr<<"Soft shadow normalization failed\n";return 2;}
    const float contact=DirectionalShadowPenumbraRadius3D(.5001f,.5f,defaults),separated=DirectionalShadowPenumbraRadius3D(.55f,.5f,defaults),maximum=DirectionalShadowPenumbraRadius3D(.95f,.1f,defaults);
    if(contact!=1.45f||!(separated>contact&&separated<maximum)||maximum!=32.f){std::cerr<<"Contact-hardening response failed "<<contact<<'/'<<separated<<'/'<<maximum<<'\n';return 3;}
    defaults.enabled=false;
    if(DirectionalShadowPenumbraRadius3D(.9f,.1f,defaults)!=1.45f){std::cerr<<"Disabled soft shadow changed PCF radius\n";return 4;}
    DrawList draw;
    draw.SetSoftDirectionalShadows({true,99.f,-4.f,99.f});
    const auto state=draw.SoftDirectionalShadows();
    if(state.blockerSearchRadiusTexels!=24.f||state.lightSourceRadiusTexels!=1.f||state.maximumPenumbraRadiusTexels!=32.f){std::cerr<<"Draw-list soft shadow state failed\n";return 5;}
    std::cout<<"Aevrynd soft directional shadow smoke passed contact="<<contact<<" separated="<<separated<<" maximum="<<maximum<<'\n';
    return 0;
}
