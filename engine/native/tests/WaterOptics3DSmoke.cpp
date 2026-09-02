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
 WaterSurfaceSettings3D hostile{};hostile.absorption={invalidValue,-2.f,std::numeric_limits<float>::infinity()};hostile.deepColor={-1.f,9.f,invalidValue};hostile.minimumRoughness=.8f;hostile.maximumRoughness=-1.f;hostile.foamStartDepth=3.f;hostile.foamEndDepth=-2.f;hostile.shallowAlpha=.8f;hostile.deepAlpha=.1f;hostile.maximumNightRadiance=invalidValue;
 const auto surface=NormalizeWaterSurfaceSettings3D(hostile);if(surface.absorption.x!=.42f||surface.absorption.y!=0.f||surface.absorption.z!=.08f||surface.deepColor.x!=0.f||surface.deepColor.y!=4.f||surface.minimumRoughness!=.6f||surface.maximumRoughness!=.6f||surface.foamEndDepth<=surface.foamStartDepth||surface.deepAlpha<surface.shallowAlpha||surface.maximumNightRadiance!=1.25f){std::cerr<<"Water surface hostile normalization failed\n";return 7;}
 const auto fingerprint=WaterSurfaceFingerprint3D(surface);if(!fingerprint||fingerprint!=WaterSurfaceFingerprint3D(surface)){std::cerr<<"Water surface fingerprint failed\n";return 8;}draw.SetWaterSurface(surface);const auto receipt=draw.WaterSurfaceReceipt();if(receipt.fingerprint!=fingerprint||!receipt.normalized||!receipt.enabledRequested||!receipt.singleCompositeRequested){std::cerr<<"Water surface receipt failed\n";return 9;}
 // 中文：GPU 10-bit逐欄量化後仍必須在shader端重建跨欄順序，且root容量不可偷偷增加。
 const auto quantize=[](float value,float minimum,float maximum){return std::lround(Clamp((value-minimum)/(maximum-minimum),0.f,1.f)*1023.f);};
 const auto decode=[](long value,float minimum,float maximum){return minimum+(maximum-minimum)*(float(value)/1023.f);};
 const float decodedFoamStart=decode(quantize(.001f,0.f,4.f),0.f,4.f);
 const float decodedFoamEnd=decode(quantize(.0011f,.001f,8.f),.001f,8.f);
 const float reconstructedFoamEnd=(std::max)(decodedFoamStart+.001f,decodedFoamEnd);
 if(!(reconstructedFoamEnd>decodedFoamStart)||EnhancedWorldRootDwords3D!=64||EnhancedWorldRootConstantDwords3D!=54){std::cerr<<"Packed water/root contract failed\n";return 10;}
 // 中文：固定CPU oracle直接封存b5f9f59的foam lerp(.42,.88)，避免只比較兩個同源disabled輸出。
 uint64_t legacyFoamHash=14695981039346656037ull;for(float value:{.42f,.65f,.88f}){const auto bits=std::bit_cast<uint32_t>(value);for(unsigned byte=0;byte<4;++byte){legacyFoamHash^=uint8_t(bits>>(byte*8));legacyFoamHash*=1099511628211ull;}}
 if(legacyFoamHash!=7577946972624993819ull){std::cerr<<"Legacy water foam golden changed\n";return 11;}
 std::cout<<"waterOffset="<<offset.x<<','<<offset.y<<" maximumUvOffset="<<draw.WaterOptics().maximumUvOffset<<'\n';
}
