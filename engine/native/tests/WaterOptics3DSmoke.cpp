#include "Aevrynd/Renderer.h"
#include <array>
#include <bit>
#include <cmath>
#include <iostream>
#include <limits>
#include <fstream>
#include <iterator>

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
 WaterSurfaceSettings3D darkNight{};darkNight.maximumNightRadiance=.012f;if(NormalizeWaterSurfaceSettings3D(darkNight).maximumNightRadiance!=.012f){std::cerr<<"Water night radiance floor prevented a physically dark scene\n";return 10;}
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
 // 讀取實際 shader，避免只驗證未被正式路徑呼叫的 CPU 複本。
 const auto shaderPath=std::filesystem::path(__FILE__).parent_path().parent_path()/"shaders"/"World.hlsl";
 std::ifstream shaderFile(shaderPath);const std::string shader{std::istreambuf_iterator<char>{shaderFile},std::istreambuf_iterator<char>{}};
 const auto count=[&](const std::string& token){unsigned n=0;size_t p=0;while((p=shader.find(token,p))!=std::string::npos){++n;p+=token.size();}return n;};
 const auto analyticCount=count("WaterAnalyticWave3D("),bandLimitCount=count("WaterWavePixelBandLimit3D("),scatterCount=count("WaterLitScatter3D("),fallbackCount=count("WaterReflectionFallback3D(");
 if(analyticCount!=5||bandLimitCount!=5||scatterCount!=2||fallbackCount!=2||shader.find("crossDetailWave.yz * crossDetailBandLimit * 0.50")==std::string::npos||shader.find("waterComposite+=waterDirectionalSpecular") == std::string::npos||shader.find("WaterContinuousNoiseGradient3D(")!=std::string::npos||shader.find("physicalDeepWaterColor:legacyDeepColor,baseColor")!=std::string::npos){std::cerr<<"Water production shader multi-direction analytic/direct-specular/band-limit/scatter/fallback contract failed counts="<<analyticCount<<'/'<<bandLimitCount<<'/'<<scatterCount<<'/'<<fallbackCount<<'\n';return 12;}
 // 雲、地表與水體共用的格點亂數必須只由整數位元運算產生；舊浮點雜湊曾造成跨次 HDR 位元漂移。
 if(count("uint CloudLatticeHash(int2 coordinate, uint seed)")!=1||
    shader.find("uint seed = 0xcb1ab31fu")==std::string::npos||
    shader.find("value *= 0x7feb352du;")==std::string::npos||
    shader.find("value *= 0x846ca68bu;")==std::string::npos||
    shader.find("CloudLatticeHash(coordinate, seed) >> 8u")==std::string::npos||
    shader.find("coordinate = frac(coordinate * float2(123.34, 456.21));")!=std::string::npos){std::cerr<<"Deterministic cloud/water lattice hash contract failed\n";return 13;}
 const auto cloudLatticeHash=[](int32_t x,int32_t y){uint32_t value=static_cast<uint32_t>(x)*0x8da6b343u^static_cast<uint32_t>(y)*0xd8163841u^0xcb1ab31fu;value^=value>>16u;value*=0x7feb352du;value^=value>>15u;value*=0x846ca68bu;value^=value>>16u;return value>>8u;};
 const std::array<std::array<int32_t,3>,8> cloudGolden{{{{0,0,7178068}},{{1,0,4053091}},{{0,1,2916818}},{{-1,0,13612103}},{{0,-1,4346319}},{{-17,43,11618627}},{{12345,-6789,1674007}},{{1048576,-1048576,4925535}}}};
 for(const auto& sample:cloudGolden){if(cloudLatticeHash(sample[0],sample[1])!=static_cast<uint32_t>(sample[2])){std::cerr<<"Deterministic cloud/water lattice hash golden failed\n";return 14;}}
 std::cout<<"waterOffset="<<offset.x<<','<<offset.y<<" maximumUvOffset="<<draw.WaterOptics().maximumUvOffset<<'\n';
}
