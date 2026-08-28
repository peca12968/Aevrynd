#include "Aevrynd/Renderer.h"
#include <bit>
#include <cmath>
#include <iostream>
#include <limits>

using namespace Aevrynd;

int main(){
 WaterOpticsSettings3D defaults{};
 if(!defaults.screenSpaceRefraction||defaults.refractionStrength<=0||defaults.maximumUvOffset<=0){std::cerr<<"Water optics defaults failed\n";return 1;}
 WaterOpticsSettings3D invalid{};invalid.refractionStrength=std::numeric_limits<float>::quiet_NaN();invalid.depthScale=std::numeric_limits<float>::infinity();invalid.maximumUvOffset=-1.f;const auto normalized=NormalizeWaterOpticsSettings3D(invalid);
 if(!std::isfinite(normalized.refractionStrength)||normalized.depthScale!=.35f||normalized.maximumUvOffset!=0.f){std::cerr<<"Water optics normalization failed\n";return 2;}
 WaterOpticsSettings3D disabled{};disabled.screenSpaceRefraction=false;const Vec2 disabledOffset=WaterRefractionOffset3D({1,-1},8,disabled);if(std::bit_cast<uint32_t>(disabledOffset.x)!=0||std::bit_cast<uint32_t>(disabledOffset.y)!=0){std::cerr<<"Disabled refraction did not preserve bitwise zero\n";return 3;}
 const float invalidValue=std::numeric_limits<float>::quiet_NaN();if(Length(WaterRefractionOffset3D({invalidValue,1},1,defaults))!=0||Length(WaterRefractionOffset3D({1,1},std::numeric_limits<float>::infinity(),defaults))!=0){std::cerr<<"Invalid refraction input did not fall back\n";return 6;}
 WaterOpticsSettings3D bounded{};bounded.refractionStrength=.12f;bounded.depthScale=4.f;bounded.maximumUvOffset=.025f;const Vec2 offset=WaterRefractionOffset3D({4,-3},100,bounded);if(std::abs(offset.x)>.02501f||std::abs(offset.y)>.02501f){std::cerr<<"Water refraction bound failed\n";return 4;}
 DrawList draw;draw.SetWaterOptics(bounded);if(draw.WaterOptics().maximumUvOffset!=.025f){std::cerr<<"Draw-list water settings failed\n";return 5;}
 std::cout<<"waterOffset="<<offset.x<<','<<offset.y<<" maximumUvOffset="<<draw.WaterOptics().maximumUvOffset<<'\n';
}
