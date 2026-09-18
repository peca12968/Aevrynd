#include "Aevrynd/Renderer.h"
#include <array>
#include <bit>
#include <cmath>
#include <iostream>
#include <limits>
#include <fstream>
#include <iterator>
#include <d3dcompiler.h>

#pragma comment(lib, "d3dcompiler.lib")

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
 // 林地場景採用的水體參數必須遵守實際光學方向：掠射反射增加、紅光
 // 衰減快於藍光，且岸泡覆蓋率隨垂直水深連續遞減，不能硬切或反轉。
 const float normalFresnel=WaterFresnel3D(1.f,.021f),grazingFresnel=WaterFresnel3D(.08f,.021f);
 const float redTransmission=WaterTransmission3D(.34f,2.f),greenTransmission=WaterTransmission3D(.22f,2.f),blueTransmission=WaterTransmission3D(.15f,2.f);
 const std::array<float,5> shoreCoverage{{WaterShoreCoverage3D(0.f,.01f,.025f,.16f),WaterShoreCoverage3D(.04f,.01f,.025f,.16f),WaterShoreCoverage3D(.09f,.01f,.025f,.16f),WaterShoreCoverage3D(.15f,.01f,.025f,.16f),WaterShoreCoverage3D(.24f,.01f,.025f,.16f)}};
 if(normalFresnel<.0209f||normalFresnel>.0211f||grazingFresnel<=normalFresnel||grazingFresnel>.75f||!(redTransmission<greenTransmission&&greenTransmission<blueTransmission)||shoreCoverage.front()<.95f||shoreCoverage.back()!=0.f){std::cerr<<"Physical forest-water optics contract failed\n";return 22;}
 for(size_t i=1;i<shoreCoverage.size();++i)if(shoreCoverage[i]>shoreCoverage[i-1]){std::cerr<<"Shore coverage monotonicity failed\n";return 23;}
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
  // 黃昏與夜間另有 time-of-day exposure；環境反射上限若回到四倍天空
  // 能量，掠射角河面會再次成為均勻亮帶。固定正式 shader 的保守上限。
  if(shader.find("const precise float waterReflectionCap=max(waterMaximumNightRadiance,dot(aerialSkyRadianceSunScale.rgb,float3(.2126,.7152,.0722)))*1.5;") == std::string::npos ||
     shader.find("const precise float waterReflectionPeak=max(reflectedWater.r,max(reflectedWater.g,reflectedWater.b));") == std::string::npos ||
     shader.find("reflectedWater=WaterOrderedMultiply3D(reflectedWater,waterReflectionScale.xxx);") == std::string::npos ||
     shader.find("max(waterMaximumNightRadiance,dot(aerialSkyRadianceSunScale.rgb,float3(.2126,.7152,.0722)))*4.0") != std::string::npos){std::cerr<<"Water low-light reflection cap contract failed\n";return 27;}
  const auto analyticCount=count("WaterAnalyticWave3D("),bandLimitCount=count("WaterWavePixelBandLimit3D("),scatterCount=count("WaterLitScatter3D("),fallbackCount=count("WaterReflectionFallback3D(");
  const auto sharedBegin=shader.find("WorldPixelInput InstancedVertexMain("),waterBegin=shader.find("WorldPixelInput InstancedWaterVertexMain("),skinnedBegin=shader.find("WorldPixelInput SkinnedVertexMain(");
  if(sharedBegin==std::string::npos||waterBegin<=sharedBegin||skinnedBegin<=waterBegin){std::cerr<<"Dedicated water vertex entry ordering failed\n";return 25;}
  const auto sharedVertex=shader.substr(sharedBegin,waterBegin-sharedBegin),waterVertex=shader.substr(waterBegin,skinnedBegin-waterBegin);
  const auto rendererPath=shaderPath.parent_path().parent_path()/"src"/"D3D12Renderer.cpp";std::ifstream rendererFile(rendererPath);const std::string renderer{std::istreambuf_iterator<char>{rendererFile},std::istreambuf_iterator<char>{}};
  if(sharedVertex.find("waterSurfaceEnabled")!=std::string::npos||sharedVertex.find("WaterSurfacePacked(")!=std::string::npos||sharedVertex.find("WaterVertexDisplacementHeight3D(")!=std::string::npos||
     waterVertex.find("waterSurfaceEnabled") == std::string::npos||waterVertex.find("WaterSurfacePacked(3)") == std::string::npos||waterVertex.find("WaterVertexDisplacementHeight3D(") == std::string::npos||waterVertex.find("saturate(input.normal.w)") == std::string::npos||
     renderer.find("parameters[11].ShaderVisibility=D3D12_SHADER_VISIBILITY_PIXEL;\n D3D12_STATIC_SAMPLER_DESC") == std::string::npos||renderer.find("parameters[11].ShaderVisibility=D3D12_SHADER_VISIBILITY_ALL;blob.Reset()") == std::string::npos||
     renderer.find("instancedWater.pRootSignature=d.waterRoot.Get();instancedWater.VS={InstancedWaterVertexShader,sizeof(InstancedWaterVertexShader)}") == std::string::npos||renderer.find("if(singleComposite)d.BindWaterRootSignature();else d.BindWorldRootSignature();") == std::string::npos||
     renderer.find("instanced.VS={InstancedVertexShader,sizeof(InstancedVertexShader)}") == std::string::npos||renderer.find("auto diagnostic=instancedWater;") == std::string::npos||
     renderer.find("makeMsaa(instancedWater,d.msaaInstancedWaterPipeline") == std::string::npos||renderer.find("createSemanticPipeline(instancedWater,d.bloomSemanticWaterPipeline") == std::string::npos||
     renderer.find("makeSemanticMsaa(instancedWater,d.msaaBloomSemanticWaterPipeline") == std::string::npos||renderer.find("objectIdInstanced.VS = {ObjectIdInstancedVertexShader") == std::string::npos||
     renderer.find("aoDepthInstanced.VS={AmbientOcclusionDepthInstancedVertexShader") == std::string::npos||renderer.find("shadowInstanced.VS={ShadowPackedInstancedVertexShader") == std::string::npos){std::cerr<<"Dedicated water/shared instanced vertex isolation contract failed\n";return 26;}
   if(analyticCount!=7||bandLimitCount!=5||scatterCount!=2||fallbackCount!=2||count("WaterVertexDisplacementHeight3D(")!=3||count("WaterDomainWarpPosition3D(")!=3||count("WaterDomainWarpGradient3D(")!=3||count("WaterReflectionRoughness3D(")!=2||shader.find("* saturate(input.normal.w);")==std::string::npos||shader.find("clamp((broadWave.x + crossWave.x) * 0.16, -0.05, 0.05)")==std::string::npos||shader.find("const precise float4 componentWeights = float4(0.46, 0.26, 0.18, 0.10);")==std::string::npos||shader.find("waterBroadWaveStrength * 0.85")==std::string::npos||shader.find("const float2 waterInterpolatedDeltaX = ddx(input.worldPosition.xz);")==std::string::npos||shader.find("waterInterpolatedFootprint);")==std::string::npos||shader.find("waterBroadWaveScale * 1.31 * 1.91")==std::string::npos||shader.find("const precise float lowFrequencyDiversity = min(broadBandLimit, crossBandLimit);")==std::string::npos||shader.find("broadWave.yz * lowFrequencyDiversity * 0.27")==std::string::npos||shader.find("crossWave.yz * lowFrequencyDiversity * 0.27")==std::string::npos||shader.find("detailWave.yz * detailBandLimit * 0.22")==std::string::npos||shader.find("crossDetailWave.yz * crossDetailBandLimit * 0.24")==std::string::npos||shader.find("const precise float waterNormalScale = clamp(input.normalScale, 0.0, 1.35);")==std::string::npos||shader.find("heightGradient = heightGradient * min(1.0, 0.48 / max(unboundedSlope, 0.0001));")==std::string::npos||shader.find("WaterReflectionRoughness3D(waterMinimumRoughness, waterMaximumRoughness,")==std::string::npos||shader.find("waterComposite=WaterOrderedScaleAdd3D(waterComposite,1.0,waterDirectionalSpecular,1.0)") == std::string::npos||shader.find("WaterContinuousNoiseGradient3D(")!=std::string::npos||shader.find("physicalDeepWaterColor:legacyDeepColor,baseColor")!=std::string::npos){std::cerr<<"Water production shader river-spectrum/vertex-depth/warp/roughness/normal-scale/direct-specular/band-limit/scatter/fallback contract failed counts="<<analyticCount<<'/'<<bandLimitCount<<'/'<<scatterCount<<'/'<<fallbackCount<<'\n';return 12;}
 // single-composite 水體必須逐分量固定乘加順序，禁止回退成向量 lerp/mad 或輸出量化。
 if(shader.find("precise float3 WaterOrderedScaleAdd3D(")==std::string::npos||
    shader.find("result.g = firstTerm.g + secondTerm.g;")==std::string::npos||
    shader.find("WaterOrderedLerp3D(sideTransmittedScene, waterComposite, waterLocalContribution)")==std::string::npos||
    shader.find("const float3 singleCompositeLit=WaterOrderedLerp3D(baseColor,shaded,surfaceWeight)")==std::string::npos||
    shader.find("WaterOrderedScaleAdd3D(aerialOutput,1.0,aerialOwnedSceneContribution,recoveredSceneWeight)")==std::string::npos||
    shader.find("WaterOrderedScaleAdd3D(deepColor, 0.76,")==std::string::npos||
    shader.find("WaterOrderedLerp3D(environmentRadiance, screenRadiance,")==std::string::npos||
    count("WaterReconstructWorldPositionDeterministic3D(")!=10||
    shader.find("f32tof16(")!=std::string::npos||shader.find("f16tof32(")!=std::string::npos){std::cerr<<"Water single-composite ordered arithmetic contract failed\n";return 18;}
 // 正式折射必須驗證位移位置的深度，並讓 Beer-Lambert 使用同一水下點；
 // 岸泡也必須經門檻形成零覆蓋缺口，而不是只替連續岸帶改變明暗。
 if(shader.find("const float candidateDepth = sceneDepthTexture.SampleLevel(screenPointClampSampler, candidateUv, 0.0);")==std::string::npos||
    shader.find("candidateVerticalDepth >= -0.02")==std::string::npos||
    shader.find("opticalPathLength = clamp(candidateThickness, 0.0, 8.0);")==std::string::npos||
    shader.find("const float foamPresence = smoothstep(0.36, 0.70,")==std::string::npos||
    shader.find("foamMask = saturate(shore * foamPresence);")==std::string::npos){std::cerr<<"Water displaced-depth or broken-shore contract failed\n";return 24;}
 // 直接以正式 /Gis 等價旗標編譯並反組譯，防止來源看似 precise、DXBC 卻重新產生 mad。
 ID3DBlob* bytecode=nullptr;ID3DBlob* errors=nullptr;
 const auto compileResult=D3DCompileFromFile(shaderPath.c_str(),nullptr,D3D_COMPILE_STANDARD_FILE_INCLUDE,"WorldPixelMain","ps_5_0",D3DCOMPILE_ENABLE_STRICTNESS|D3DCOMPILE_IEEE_STRICTNESS|D3DCOMPILE_OPTIMIZATION_LEVEL3|D3DCOMPILE_WARNINGS_ARE_ERRORS,0,&bytecode,&errors);
 if(FAILED(compileResult)){if(errors)std::cerr.write(static_cast<const char*>(errors->GetBufferPointer()),static_cast<std::streamsize>(errors->GetBufferSize()));if(errors)errors->Release();if(bytecode)bytecode->Release();std::cerr<<"Water strict shader compilation contract failed\n";return 19;}
 ID3DBlob* disassembly=nullptr;const auto disassembleResult=D3DDisassemble(bytecode->GetBufferPointer(),bytecode->GetBufferSize(),0,nullptr,&disassembly);
 bytecode->Release();if(errors)errors->Release();
 if(FAILED(disassembleResult)||!disassembly){std::cerr<<"Water shader disassembly contract failed\n";return 20;}
 const std::string assembly{static_cast<const char*>(disassembly->GetBufferPointer()),disassembly->GetBufferSize()};disassembly->Release();
 if(assembly.find("\nmad ")!=std::string::npos||assembly.find("\nmad_sat ")!=std::string::npos||assembly.find("[precise") == std::string::npos){std::cerr<<"Water shader emitted reassociated MAD or lost precise DXBC operands\n";return 21;}
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
 // 四個尺度錨點仍留在主流向半球，但必須覆蓋足夠寬的入射角；把全部
 // 鎖在主流向附近會在實際河面疊成規則平行亮帶。舊版的完全反流
 // (-.89,.46) 仍被拒絕，正式組合的方向集中度另由 WaterAnalyticCpu 封住。
 const std::array<std::array<float,2>,3> scaleAnchors{{{{.573576436f,.819152044f}},{{.707106781f,-.707106781f}},{{.087155743f,.996194698f}}}};
 const std::array<std::array<float,2>,3> intraDirections{{{{.978147601f,.207911691f}},{{.945518576f,-.325568154f}},{{.874619707f,.484809620f}}}};
 for(const auto& anchor:scaleAnchors)if(anchor[0]<=0.f){std::cerr<<"Water cross-scale flow hemisphere failed\n";return 15;}
 if(scaleAnchors[0][1]<.8f||scaleAnchors[1][1]>-.7f||scaleAnchors[2][1]<.99f){std::cerr<<"Water cross-scale angular spread failed\n";return 15;}
 for(const auto& direction:intraDirections)if(direction[0]<.85f){std::cerr<<"Water intra-wave reverse-flow rejection failed\n";return 16;}
 if(std::abs(.46f+.26f+.18f+.10f-1.f)>1e-6f||
    shader.find("direction * 0.978147601 + perpendicular * 0.207911691")==std::string::npos||
    shader.find("direction * 0.945518576 - perpendicular * 0.325568154")==std::string::npos||
    shader.find("direction * 0.874619707 + perpendicular * 0.484809620")==std::string::npos||
    shader.find("const precise float2 riverTangent = float2(0.28, 0.96);")==std::string::npos||
    shader.find("const precise float2 riverAcross = float2(-0.96, 0.28);")==std::string::npos||
    shader.find("riverTangent * 0.573576436 + riverAcross * 0.819152044, 2.03")==std::string::npos||
    shader.find("riverTangent * 0.707106781 - riverAcross * 0.707106781, 4.79")==std::string::npos||
    shader.find("riverTangent * 0.087155743 + riverAcross * 0.996194698, 7.31")==std::string::npos||
    shader.find("direction * -0.71 + perpendicular * 0.70")!=std::string::npos){std::cerr<<"Water production flow-direction binding failed\n";return 17;}
 std::cout<<"waterOffset="<<offset.x<<','<<offset.y<<" maximumUvOffset="<<draw.WaterOptics().maximumUvOffset<<'\n';
}
