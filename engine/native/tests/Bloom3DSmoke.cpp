#include <Aevrynd/Renderer.h>
#include <cmath>
#include <iostream>
#include <limits>
using namespace Aevrynd;
int main(){
 BloomSettings3D settings{};if(!settings.enabled||BloomSoftThreshold3D(.5f,settings)!=0.f||BloomSoftThreshold3D(2.f,settings)<=0.f){std::cerr<<"Bloom threshold response failed\n";return 1;}
 settings.enabled=false;if(BloomSoftThreshold3D(8.f,settings)!=0.f){std::cerr<<"Disabled bloom must be zero cost\n";return 2;}
 settings={true,std::numeric_limits<float>::quiet_NaN(),std::numeric_limits<float>::infinity(),-2.f,99.f};settings=NormalizeBloomSettings3D(settings);
 if(settings.threshold!=1.f||settings.softKnee!=.5f||settings.intensity!=0.f||settings.radiusPixels!=16.f){std::cerr<<"Bloom normalization failed\n";return 3;}
 DrawList draw;if(draw.ExposureCompensation()!=0.f){std::cerr<<"Zero EV default parity failed\n";return 4;}draw.SetExposureCompensation(std::numeric_limits<float>::infinity());if(draw.ExposureCompensation()!=0.f){std::cerr<<"Non-finite exposure did not fail closed\n";return 5;}draw.SetExposureCompensation(99.f);draw.SetBloom({true,1.2f,.4f,.2f,3.f});if(draw.ExposureCompensation()!=10.f||std::abs(draw.Bloom().intensity-.2f)>.0001f){std::cerr<<"Bloom API state failed\n";return 6;}
 std::cout<<"Aevrynd bloom smoke passed\n";return 0;
}
