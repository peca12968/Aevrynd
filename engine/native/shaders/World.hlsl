#include "OcclusionDither.hlsli"

cbuffer Scene : register(b0)
{
    row_major float4x4 viewProjection;
    float4 lightDirectionAmbient;
    float4 lightColorIntensity;
    float4 fogColor;
    float4 cameraFogStart;
    float4 fogEndEnabled;
    float4 forwardPlusParameters;
    float4 forwardPlusReserved[5];
    float4 colorPipeline;
    row_major float4x4 inverseViewProjection;
    float4 worldViewport;
    row_major float4x4 previousViewProjection;
    float4 motionParameters;
    row_major float4x4 currentUnjitteredViewProjection;
    float4 temporalJitterPixels;
    float4 heightFogParameters;
    float4 aerialExtinctionScatteringHeight;
    float4 aerialDistancePhase;
    float4 aerialSkyRadianceSunScale;
    float4 environmentDiffuseScaleIntensity;
    float4 environmentSpecularScaleIntensity;
    float4 environmentAnalyticIblWeights;
    uint4 environmentPhaseReceipt;
    float4 phaseSkyZenith;
    float4 phaseSkyHorizon;
    float4 phaseSkyGround;
    float4 phaseAnalyticDiffuse;
    float4 phaseSunDisk;
    float4 environmentObjectFill;
};

struct PointLightGpu3D
{
    float3 position;
    float radius;
    float3 color;
    float intensity;
};
StructuredBuffer<PointLightGpu3D> forwardPlusPointLights : register(t20);
StructuredBuffer<uint4> forwardPlusTileGrid : register(t21);
StructuredBuffer<uint> forwardPlusTileIndices : register(t22);

float3 ApplyVelocityWind(float3 position,float packedWeight,uint materialClass,float time)
{
    const float weight=(materialClass==2u?1.0:0.0)*step(0.0,packedWeight)*(1.0-saturate(packedWeight));
    const float primary=sin(time*1.7+position.x*0.43+position.z*0.31);
    const float detail=sin(time*3.1-position.x*0.19+position.z*0.67);
    return position+float3(primary+detail*0.35,abs(detail)*0.08,detail*0.55)*weight*0.13;
}
// HLOD source/proxy 必須共用完整的像素雜湊輸入；一般 LOD 仍混入物件 transform seed。
uint StaticLodSeed(uint textureData,uint objectSeed){return((textureData>>13)&7u)^(((textureData&0x80000000u)!=0u)?0u:objectSeed);}

struct MotionInstanceInput
{
    float3 position : POSITION;
    float4 normal : NORMAL;
    float4 transformRow0 : INSTANCE0;
    float4 transformRow1 : INSTANCE1;
    float4 transformRow2 : INSTANCE2;
    float4 transformRow3 : INSTANCE3;
    uint textureData : TEXTURE;
    float4 previousRow0 : PREVIOUS0;
    float4 previousRow1 : PREVIOUS1;
    float4 previousRow2 : PREVIOUS2;
    float4 previousRow3 : PREVIOUS3;
};
struct MotionWorldInput { float3 position : POSITION; };

struct MotionVertexOutput
{
    float4 position : SV_POSITION;
    float4 currentClip : TEXCOORD0;
    float4 previousClip : TEXCOORD1;
    nointerpolation float lodCoverage : TEXCOORD2;
    nointerpolation uint lodSeed : TEXCOORD3;
    nointerpolation uint lodInvert : TEXCOORD4;
};

void ClipMotionLod(float4 position, float coverage, uint seed, uint invert)
{
    if (coverage >= 0.999) return;
    const uint2 pixel = uint2(position.xy);
    uint noise = pixel.x * 1973u + pixel.y * 9277u + seed * 26699u + 911u;
    noise = (noise << 13u) ^ noise;
    noise = noise * (noise * noise * 15731u + 789221u) + 1376312589u;
    const float threshold = float(noise & 255u) / 255.0;
    clip(invert != 0u ? threshold - coverage - 0.0000001 : coverage - threshold);
}

MotionVertexOutput VelocityWorldVertexMain(MotionWorldInput input)
{
    MotionVertexOutput output = (MotionVertexOutput)0;
    const float4 worldPosition = float4(input.position, 1.0);
    output.position = mul(viewProjection, worldPosition);
    output.currentClip = mul(currentUnjitteredViewProjection, worldPosition);
    output.previousClip = mul(previousViewProjection, worldPosition);
    output.lodCoverage = 1.0;
    output.lodSeed = 0u;
    output.lodInvert = 0u;
    return output;
}

MotionVertexOutput VelocityInstancedVertexMain(MotionInstanceInput input)
{
    const float4 localPosition = float4(input.position, 1.0);
    float4 currentWorld = float4(dot(input.transformRow0, localPosition), dot(input.transformRow1, localPosition), dot(input.transformRow2, localPosition), dot(input.transformRow3, localPosition));
    float4 previousWorld = float4(dot(input.previousRow0, localPosition), dot(input.previousRow1, localPosition), dot(input.previousRow2, localPosition), dot(input.previousRow3, localPosition));
    const uint materialClass=(input.textureData>>2)&3u;currentWorld.xyz=ApplyVelocityWind(currentWorld.xyz,input.normal.w,materialClass,fogEndEnabled.w);previousWorld.xyz=ApplyVelocityWind(previousWorld.xyz,input.normal.w,materialClass,motionParameters.w);
    MotionVertexOutput output = (MotionVertexOutput)0;
    output.position = mul(viewProjection, currentWorld);
    output.currentClip = mul(currentUnjitteredViewProjection, currentWorld);
    output.previousClip = mul(previousViewProjection, previousWorld);
    output.lodCoverage = float((input.textureData >> 4) & 255u) / 255.0;
    output.lodInvert = (input.textureData >> 12) & 1u;
    const uint objectSeed = asuint(input.transformRow0.w) * 73856093u ^ asuint(input.transformRow1.w) * 19349663u ^ asuint(input.transformRow2.w) * 83492791u;
    output.lodSeed = StaticLodSeed(input.textureData, objectSeed);
    return output;
}

float2 VelocityPixelMain(MotionVertexOutput input) : SV_TARGET
{
    ClipMotionLod(input.position, input.lodCoverage, input.lodSeed, input.lodInvert);
    if (motionParameters.x < 0.5 || input.currentClip.w <= 1e-6 || input.previousClip.w <= 1e-6)
        return float2(0.0,0.0);
    const float2 currentNdc = input.currentClip.xy / input.currentClip.w;
    const float2 previousNdc = input.previousClip.xy / input.previousClip.w;
    return (currentNdc - previousNdc) * float2(motionParameters.y, -motionParameters.z);
}
cbuffer ShadowScene : register(b1) { row_major float4x4 lightViewProjection[4]; float4 shadowCascadeSplits; float4 shadowParameters; float4 shadowTransition; float4 shadowSoftness; float4 contactShadowParameters; };
cbuffer DrawConstants : register(b2) { uint shadowCascadeIndex; uint drawConstantReserved; };

Texture2DArray<float> shadowTexture : register(t3);
SamplerComparisonState shadowSampler : register(s1);

Texture2D baseColorTexture : register(t0);
Texture2D roughnessTexture : register(t1);
Texture2D normalTexture : register(t2);
Texture2D soilBaseColorTexture : register(t4);
Texture2D soilRoughnessTexture : register(t5);
Texture2D soilNormalTexture : register(t6);
Texture2D rockBaseColorTexture : register(t7);
Texture2D rockRoughnessTexture : register(t8);
Texture2D rockNormalTexture : register(t9);
Texture2D<float2> ambientOcclusionTexture : register(t11);
Texture2D<float> sceneDepthTexture : register(t12);
Texture2D<float4> opaqueSceneColorTexture : register(t14);
TextureCube<float3> environmentSpecularTexture : register(t16);
TextureCube<float3> environmentIrradianceTexture : register(t17);
Texture2D<float2> environmentBrdfTexture : register(t18);
StructuredBuffer<float4> localReflectionProbeData : register(t19);
cbuffer EnvironmentConstants : register(b3)
{
    float environmentRotationRadians;
    float environmentSpecularMaximumMip;
    float environmentEnabled;
    float environmentReserved;
};
cbuffer WaterOpticsConstants : register(b4)
{
    float waterRefractionStrength;
    float waterRefractionDepthScale;
    float waterMaximumUvOffset;
    float waterSceneColorEnabled;
    uint waterSurfaceEnabled;
    uint3 waterSurfacePacked0;
    uint4 waterSurfacePacked1;
};
float3 DecodeWaterTriple(uint packed,float3 minimumValue,float3 maximumValue){return lerp(minimumValue,maximumValue,float3(packed&1023u,(packed>>10u)&1023u,(packed>>20u)&1023u)/1023.0);}
uint WaterSurfacePacked(uint index){if(index==0u)return waterSurfacePacked0.x;if(index==1u)return waterSurfacePacked0.y;if(index==2u)return waterSurfacePacked0.z;if(index==3u)return waterSurfacePacked1.x;if(index==4u)return waterSurfacePacked1.y;if(index==5u)return waterSurfacePacked1.z;return waterSurfacePacked1.w;}
cbuffer TerrainDistanceMaterialConstants : register(b5)
{
    float terrainDetailFadeStart;
    float terrainDetailFadeEnd;
    float terrainMacroVariationScale;
    float terrainMacroVariationStrength;
    float terrainNormalDetailStrength;
    float terrainFarRoughnessBoost;
    float terrainDistanceMaterialEnabled;
    uint terrainDecorrelationPacked;
};
float DecodeTerrainDecorrelation(uint shift, float minimumValue, float maximumValue)
{
    return lerp(minimumValue, maximumValue, float((terrainDecorrelationPacked >> shift) & 31u) / 31.0);
}
bool TerrainDecorrelationEnabled()
{
    return (terrainDecorrelationPacked & (1u << 30u)) != 0u;
}
bool TerrainMesoPartitionEnabled()
{
    return (terrainDecorrelationPacked & (1u << 31u)) != 0u;
}
float3 TerrainMaterialDepthContrast(float mesoSignal, float3 shapedAuthoredWeights)
{
    // 程序分區需要在細節貼圖淡出後仍有可讀明度跨度；明確 authored 區域
    // 則改由步道、落葉與濕岸的有界光譜目標接管。只重用既有訊號，沒有
    // 新取樣、相機、時間或霧依賴。
    if (any(shapedAuthoredWeights != shapedAuthoredWeights) ||
        any(abs(shapedAuthoredWeights) > 65504.0))
        return 1.0;
    float3 authoredWeights = saturate(shapedAuthoredWeights);
    const float authoredWeightTotal = authoredWeights.x + authoredWeights.y + authoredWeights.z;
    // smoothstep 的固定邊界展開成飽和座標與 Hermite 多項式，避免通用
    // intrinsic；neutral 是絕大多數 terrain pixel，直接跳過三組 RGB 混合。
    const float mesoCoordinate = saturate((mesoSignal - 0.18) * 1.5625);
    const float shapedMeso = mesoCoordinate * mesoCoordinate * (3.0 - 2.0 * mesoCoordinate);
    const float proceduralScale = 0.84 + 0.30 * shapedMeso;
    [branch] if (authoredWeightTotal <= 0.000001)
        return proceduralScale.xxx;
    const float authoredAmount = saturate(authoredWeightTotal);
    // 對 production 契約的 sum<=1 權重，normalizedTarget*total 嚴格等於
    // rawWeight*target；直接展開可移除 reciprocal 與三次 normalize multiply。
    const float3 authoredContrast = authoredWeights.x * float3(1.12, 1.13, 1.18)
                                  + authoredWeights.y * float3(1.02, 0.88, 0.72)
                                  + authoredWeights.z * float3(0.70, 0.76, 0.82);
    return clamp(proceduralScale.xxx * (1.0 - authoredAmount) + authoredContrast,
                 0.70.xxx, 1.18.xxx);
}
float3 TerrainDecorrelatedWorldPosition(float3 worldPosition, float warpScale, float warpStrength)
{
    // 只使用連續世界座標；不含格線、相機或時間，因此跨分區及視角移動時不會跳接或游動。
    const float3 phase = float3(worldPosition.x * 1.731 + worldPosition.z * 0.917,
                                worldPosition.y * 1.423 + worldPosition.x * 0.611,
                                worldPosition.z * 1.277 - worldPosition.y * 0.743) * warpScale;
    const float3 broad = sin(phase + float3(0.0, 1.7, -2.1));
    const float3 detail = sin(phase.yzx * 2.17 + float3(2.3, -1.1, 0.7));
    return worldPosition + (broad * 0.72 + detail * 0.28) * warpStrength;
}
float3 ApplyTerrainChromaVariation(float3 albedo, float noiseSample, float strength)
{
    const float amount = (saturate(noiseSample) * 2.0 - 1.0) * strength;
    const float3 shifted = max(0.0, albedo * (1.0 + amount * float3(0.92, -0.28, 0.54)));
    const float3 luminanceWeights = float3(0.2126, 0.7152, 0.0722);
    const float before = dot(albedo, luminanceWeights);
    const float after = dot(shifted, luminanceWeights);
    // 色度擾動後恢復原本線性亮度，避免以提亮地形偽造細節或破壞曝光排序。
    return before > 0.0 && after > 0.000001 ? shifted * (before / after) : albedo;
}
float3 TerrainForestFloorSpectralVariation(float mesoSignal, float macroSignal,
                                           float grassWeight, float soilWeight,
                                           float authoredCalibration)
{
    // Reuse the continuous world-space macro/meso communities to separate
    // cool moss pockets from warm leaf litter.  This adds no texture/noise
    // sample and normalizes the multiplier to unit Rec.709 luminance, keeping
    // exposure plus authored path/wet-bank energy unchanged.
    if (mesoSignal != mesoSignal || macroSignal != macroSignal ||
        grassWeight != grassWeight || soilWeight != soilWeight ||
        authoredCalibration != authoredCalibration)
        return 1.0.xxx;
    const float habitat = saturate(mesoSignal * 0.68 + macroSignal * 0.32);
    const float shapedHabitat = habitat * habitat * (3.0 - 2.0 * habitat);
    const float3 leafLitter = float3(1.12, 0.96, 0.78);
    const float3 moss = float3(0.78, 1.12, 0.84);
    const float naturalFloor = saturate(grassWeight + soilWeight) *
                               (1.0 - saturate(authoredCalibration));
    const float3 spectral = lerp(1.0.xxx, lerp(leafLitter, moss, shapedHabitat),
                                 naturalFloor * 0.62);
    const float spectralLuma = dot(spectral, float3(0.2126, 0.7152, 0.0722));
    return spectralLuma > 0.000001 ? spectral / spectralLuma : 1.0.xxx;
}
cbuffer VolumetricCloudConstants : register(b6)
{
    float cloudBaseAltitude;
    float cloudThickness;
    float cloudCoverage;
    float cloudDensityScale;
    float cloudErosion;
    float cloudWindSpeed;
    float cloudSampleCount;
    float volumetricCloudEnabled;
};
cbuffer VolumetricCloudShadowConstants : register(b7)
{
    float cloudShadowStrength;
    float cloudShadowEnabled;
    float2 cloudShadowReserved;
};
cbuffer DirectionalLightShaftConstants : register(b8)
{
    float directionalLightShaftIntensity;
    float directionalLightShaftAnisotropy;
    float directionalLightShaftMaximumDistance;
    float directionalLightShaftSampleCount;
};
cbuffer SkinSubsurfaceConstants : register(b9)
{
    float skinSubsurfaceStrength;
    float skinSubsurfaceWrap;
    float skinSubsurfaceCurvatureScale;
    float skinSubsurfaceTransmission;
    float screenSpaceReflectionIntensity;
    float screenSpaceReflectionMaximumDistance;
    float screenSpaceReflectionThickness;
    float screenSpaceReflectionMaximumRoughness;
    float screenSpaceReflectionSampleCount;
    float screenSpaceReflectionSceneAvailable;
    float2 screenSpaceReflectionReserved;
};
struct SkinPaletteJoint
{
    row_major float4x4 transform;
    float4 normalRow0;
    float4 normalRow1;
    float4 normalRow2;
};
StructuredBuffer<SkinPaletteJoint> skinPalette : register(t13);
SamplerState materialSampler : register(s0);
SamplerState screenPointClampSampler : register(s2);

struct MotionMaterialInstanceInput
{
    float3 position : POSITION;
    float4 normal : NORMAL;
    float4 color : COLOR;
    float2 uv : TEXCOORD0;
    float4 transformRow0 : INSTANCE0;
    float4 transformRow1 : INSTANCE1;
    float4 transformRow2 : INSTANCE2;
    float4 transformRow3 : INSTANCE3;
    uint textureData : TEXTURE;
    float4 normalRow1 : NROW1;
    float4 normalRow2 : NROW2;
    float4 tint : TINT;
    float4 previousRow0 : PREVIOUS0;
    float4 previousRow1 : PREVIOUS1;
    float4 previousRow2 : PREVIOUS2;
    float4 previousRow3 : PREVIOUS3;
};

struct MotionMaterialSkinnedInput
{
    float3 position : POSITION;
    float4 normal : NORMAL;
    float4 color : COLOR;
    float2 uv : TEXCOORD0;
    uint4 blendIndices : BLENDINDICES;
    float4 blendWeights : BLENDWEIGHT;
    float4 transformRow0 : INSTANCE0;
    float4 transformRow1 : INSTANCE1;
    float4 transformRow2 : INSTANCE2;
    float4 transformRow3 : INSTANCE3;
    uint textureData : TEXTURE;
    float4 tint : TINT;
    uint skinPaletteBase : SKINBASE;
    float4 previousRow0 : PREVIOUS0;
    float4 previousRow1 : PREVIOUS1;
    float4 previousRow2 : PREVIOUS2;
    float4 previousRow3 : PREVIOUS3;
    uint previousPaletteBase : PREVIOUSPALETTE;
};

struct MotionMaterialVertexOutput
{
    float4 position : SV_POSITION;
    float4 currentClip : TEXCOORD0;
    float4 previousClip : TEXCOORD1;
    float2 uv : TEXCOORD2;
    nointerpolation float alphaCutoff : TEXCOORD3;
    nointerpolation float alpha : TEXCOORD4;
    nointerpolation float lodCoverage : TEXCOORD5;
    nointerpolation uint lodSeed : TEXCOORD6;
    nointerpolation uint lodInvert : TEXCOORD7;
};

MotionMaterialVertexOutput VelocityMaskedInstancedVertexMain(MotionMaterialInstanceInput input)
{
    const float4 localPosition=float4(input.position,1.0);
    float4 currentWorld=float4(dot(input.transformRow0,localPosition),dot(input.transformRow1,localPosition),dot(input.transformRow2,localPosition),dot(input.transformRow3,localPosition));
    float4 previousWorld=float4(dot(input.previousRow0,localPosition),dot(input.previousRow1,localPosition),dot(input.previousRow2,localPosition),dot(input.previousRow3,localPosition));
    const uint materialClass=(input.textureData>>2)&3u;currentWorld.xyz=ApplyVelocityWind(currentWorld.xyz,input.normal.w,materialClass,fogEndEnabled.w);previousWorld.xyz=ApplyVelocityWind(previousWorld.xyz,input.normal.w,materialClass,motionParameters.w);
    MotionMaterialVertexOutput output;
    output.position=mul(viewProjection,currentWorld);
    output.currentClip=mul(currentUnjitteredViewProjection,currentWorld);
    output.previousClip=mul(previousViewProjection,previousWorld);
    output.uv=input.uv;
    output.alphaCutoff=saturate(input.normalRow1.w);
    output.alpha=input.color.a*input.tint.a;
    output.lodCoverage=float((input.textureData>>4)&255u)/255.0;
    output.lodInvert=(input.textureData>>12)&1u;
    const uint objectSeed=asuint(input.transformRow0.w)*73856093u^asuint(input.transformRow1.w)*19349663u^asuint(input.transformRow2.w)*83492791u;
    output.lodSeed=StaticLodSeed(input.textureData,objectSeed);
    return output;
}

float2 VelocityMaskedPixelMain(MotionMaterialVertexOutput input) : SV_TARGET
{
    // 速度根簽章將基色描述符映射至 t2，因此此處以同一資源槽裁切透明遮罩。
    clip(input.alpha*normalTexture.Sample(materialSampler,input.uv).a-input.alphaCutoff);
    ClipMotionLod(input.position,input.lodCoverage,input.lodSeed,input.lodInvert);
    if(motionParameters.x<0.5||input.currentClip.w<=1e-6||input.previousClip.w<=1e-6)return float2(0.0,0.0);
    const float2 currentNdc=input.currentClip.xy/input.currentClip.w;
    const float2 previousNdc=input.previousClip.xy/input.previousClip.w;
    return(currentNdc-previousNdc)*float2(motionParameters.y,-motionParameters.z);
}

struct MotionSkinnedVertexInput
{
    float3 position : POSITION;
    float4 normal : NORMAL;
    uint4 blendIndices : BLENDINDICES;
    float4 blendWeights : BLENDWEIGHT;
    float4 transformRow0 : INSTANCE0;
    float4 transformRow1 : INSTANCE1;
    float4 transformRow2 : INSTANCE2;
    float4 transformRow3 : INSTANCE3;
    uint textureData : TEXTURE;
    uint skinPaletteBase : SKINBASE;
    float4 previousRow0 : PREVIOUS0;
    float4 previousRow1 : PREVIOUS1;
    float4 previousRow2 : PREVIOUS2;
    float4 previousRow3 : PREVIOUS3;
    uint previousPaletteBase : PREVIOUSPALETTE;
};

float3 SkinMotionPosition(float3 position, uint4 indices, float4 blendWeights, uint paletteBase)
{
    const float weightSum = dot(blendWeights, float4(1.0, 1.0, 1.0, 1.0));
    const float4 weights = blendWeights / max(weightSum, 1e-6);
    const float4 sourcePosition = float4(position, 1.0);
    float3 result = 0.0;
    [unroll] for (uint influence = 0; influence < 4; ++influence)
    {
        const SkinPaletteJoint joint = skinPalette[(paletteBase & 4095u) + indices[influence]];
        result += float3(dot(joint.transform[0], sourcePosition),
                         dot(joint.transform[1], sourcePosition),
                         dot(joint.transform[2], sourcePosition)) * weights[influence];
    }
    return result;
}

MotionMaterialVertexOutput VelocityMaskedSkinnedVertexMain(MotionMaterialSkinnedInput input)
{
    const float4 currentLocal=float4(SkinMotionPosition(input.position,input.blendIndices,input.blendWeights,input.skinPaletteBase),1.0);
    const float4 previousLocal=float4(SkinMotionPosition(input.position,input.blendIndices,input.blendWeights,input.previousPaletteBase),1.0);
    float4 currentWorld=float4(dot(input.transformRow0,currentLocal),dot(input.transformRow1,currentLocal),dot(input.transformRow2,currentLocal),dot(input.transformRow3,currentLocal));
    float4 previousWorld=float4(dot(input.previousRow0,previousLocal),dot(input.previousRow1,previousLocal),dot(input.previousRow2,previousLocal),dot(input.previousRow3,previousLocal));
    const uint materialClass=(input.textureData>>2)&3u;currentWorld.xyz=ApplyVelocityWind(currentWorld.xyz,input.normal.w,materialClass,fogEndEnabled.w);previousWorld.xyz=ApplyVelocityWind(previousWorld.xyz,input.normal.w,materialClass,motionParameters.w);
    MotionMaterialVertexOutput output;
    output.position=mul(viewProjection,currentWorld);
    output.currentClip=mul(currentUnjitteredViewProjection,currentWorld);
    output.previousClip=mul(previousViewProjection,previousWorld);
    output.uv=input.uv;
    output.alphaCutoff=float((input.skinPaletteBase>>12)&255u)/255.0;
    output.alpha=input.color.a*input.tint.a;
    output.lodCoverage=float((input.textureData>>4)&255u)/255.0;
    output.lodInvert=(input.textureData>>12)&1u;
    const uint objectSeed=asuint(input.transformRow0.w)*73856093u^asuint(input.transformRow1.w)*19349663u^asuint(input.transformRow2.w)*83492791u;
    output.lodSeed=((input.textureData>>13)&7u)^objectSeed;
    return output;
}

MotionVertexOutput VelocitySkinnedVertexMain(MotionSkinnedVertexInput input)
{
    const float4 currentLocal = float4(SkinMotionPosition(input.position, input.blendIndices, input.blendWeights, input.skinPaletteBase), 1.0);
    const float4 previousLocal = float4(SkinMotionPosition(input.position, input.blendIndices, input.blendWeights, input.previousPaletteBase), 1.0);
    float4 currentWorld = float4(dot(input.transformRow0, currentLocal), dot(input.transformRow1, currentLocal), dot(input.transformRow2, currentLocal), dot(input.transformRow3, currentLocal));
    float4 previousWorld = float4(dot(input.previousRow0, previousLocal), dot(input.previousRow1, previousLocal), dot(input.previousRow2, previousLocal), dot(input.previousRow3, previousLocal));
    const uint materialClass=(input.textureData>>2)&3u;currentWorld.xyz=ApplyVelocityWind(currentWorld.xyz,input.normal.w,materialClass,fogEndEnabled.w);previousWorld.xyz=ApplyVelocityWind(previousWorld.xyz,input.normal.w,materialClass,motionParameters.w);
    MotionVertexOutput output = (MotionVertexOutput)0;
    output.position = mul(viewProjection, currentWorld);
    output.currentClip = mul(currentUnjitteredViewProjection, currentWorld);
    output.previousClip = mul(previousViewProjection, previousWorld);
    output.lodCoverage = float((input.textureData >> 4) & 255u) / 255.0;
    output.lodInvert = (input.textureData >> 12) & 1u;
    const uint objectSeed = asuint(input.transformRow0.w) * 73856093u ^ asuint(input.transformRow1.w) * 19349663u ^ asuint(input.transformRow2.w) * 83492791u;
    output.lodSeed = ((input.textureData >> 13) & 7u) ^ objectSeed;
    return output;
}

struct WorldVertexInput
{
    float3 position : POSITION;
    float4 normal : NORMAL;
    float4 color : COLOR;
    float2 uv : TEXCOORD0;
};

struct InstancedVertexInput
{
    float3 position : POSITION;
    float4 normal : NORMAL;
    float4 color : COLOR;
    float2 uv : TEXCOORD0;
    float4 transformRow0 : INSTANCE0;
    float4 transformRow1 : INSTANCE1;
    float4 transformRow2 : INSTANCE2;
    float4 transformRow3 : INSTANCE3;
    float4 normalRow0 : NROW0;
    float4 normalRow1 : NROW1;
    float4 normalRow2 : NROW2;
    float4 tint : TINT;
    float4 material : MATERIAL;
    uint textureData : TEXTURE;
    float occlusionOpacity : OCCLUSION;
    uint2 occlusionSeeds : OCCLUSIONSEED;
};

struct SkinnedVertexInput
{
    float3 position : POSITION;
    float4 normal : NORMAL;
    float4 color : COLOR;
    float2 uv : TEXCOORD0;
    float4 tangent : TANGENT;
    float4 transformRow0 : INSTANCE0;
    float4 transformRow1 : INSTANCE1;
    float4 transformRow2 : INSTANCE2;
    float4 transformRow3 : INSTANCE3;
    float4 normalRow0 : NROW0;
    float4 normalRow1 : NROW1;
    float4 normalRow2 : NROW2;
    float4 tint : TINT;
    float4 material : MATERIAL;
    uint textureData : TEXTURE;
    float occlusionOpacity : OCCLUSION;
    uint2 occlusionSeeds : OCCLUSIONSEED;
    uint skinPaletteBase : SKINBASE;
    uint4 blendIndices : BLENDINDICES;
    float4 blendWeights : BLENDWEIGHT;
};

void SkinLocalVertex(SkinnedVertexInput input, out float3 position, out float3 normal,
                     out float3 tangent, out float3 bitangent)
{
    const float weightSum = dot(input.blendWeights, float4(1.0, 1.0, 1.0, 1.0));
    const float4 weights = input.blendWeights / max(weightSum, 1e-6);
    position = 0.0;
    normal = 0.0;
    tangent = 0.0;
    bitangent = 0.0;
    const float3 sourceNormal = input.normal.xyz;
    const float tangentLengthSquared = dot(input.tangent.xyz, input.tangent.xyz);
    const float3 sourceTangent = tangentLengthSquared > 1e-8 && tangentLengthSquared < 1e8
        ? input.tangent.xyz * rsqrt(tangentLengthSquared) : float3(0.0, 0.0, 0.0);
    const float3 sourceBitangent = cross(sourceNormal, sourceTangent) * (input.tangent.w < 0.0 ? -1.0 : 1.0);
    [unroll] for (uint influence = 0; influence < 4; ++influence)
    {
        const SkinPaletteJoint joint = skinPalette[(input.skinPaletteBase & 4095u) + input.blendIndices[influence]];
        const float4 sourcePosition = float4(input.position, 1.0);
        position += float3(dot(joint.transform[0], sourcePosition),
                           dot(joint.transform[1], sourcePosition),
                           dot(joint.transform[2], sourcePosition)) * weights[influence];
        normal += float3(dot(joint.normalRow0.xyz, input.normal.xyz),
                         dot(joint.normalRow1.xyz, input.normal.xyz),
                         dot(joint.normalRow2.xyz, input.normal.xyz)) * weights[influence];
        tangent += float3(dot(joint.normalRow0.xyz, sourceTangent),
                          dot(joint.normalRow1.xyz, sourceTangent),
                          dot(joint.normalRow2.xyz, sourceTangent)) * weights[influence];
        bitangent += float3(dot(joint.normalRow0.xyz, sourceBitangent),
                            dot(joint.normalRow1.xyz, sourceBitangent),
                            dot(joint.normalRow2.xyz, sourceBitangent)) * weights[influence];
    }
    const float normalLengthSquared = dot(normal, normal);
    normal = normalLengthSquared > 1e-10 && normalLengthSquared < 1e20
        ? normal * rsqrt(normalLengthSquared) : float3(0.0, 1.0, 0.0);
    tangent -= normal * dot(normal, tangent);
    const float skinnedTangentLengthSquared = dot(tangent, tangent);
    if (skinnedTangentLengthSquared > 1e-10 && skinnedTangentLengthSquared < 1e20)
        tangent *= rsqrt(skinnedTangentLengthSquared);
    else
        tangent = 0.0;
}

struct WorldPixelInput
{
    float4 position : SV_POSITION;
    float4 color : COLOR;
    float3 worldPosition : TEXCOORD0;
    float4 normal : TEXCOORD1;
    float4 material : TEXCOORD2;
    float4 textureData : TEXCOORD3;
    float2 uv : TEXCOORD4;
    nointerpolation uint materialClass : TEXCOORD5;
    float4 shadowPosition : TEXCOORD6;
    float4 farShadowPosition : TEXCOORD7;
    nointerpolation float lodCoverage : TEXCOORD8;
    nointerpolation uint lodSeed : TEXCOORD9;
    nointerpolation uint lodInvert : TEXCOORD10;
    float4 tangent : TEXCOORD11;
    nointerpolation uint characterShadingModel : TEXCOORD12;
    nointerpolation float alphaCutoff : TEXCOORD13;
    nointerpolation uint alphaMasked : TEXCOORD14;
    nointerpolation float normalScale : TEXCOORD15;
    nointerpolation float occlusionOpacity : TEXCOORD16;
    nointerpolation uint2 occlusionSeeds : TEXCOORD17;
    float3 occlusionCoordinate : TEXCOORD18;
    nointerpolation uint objectToken : OBJECTID;
    nointerpolation uint alphaMode : TEXCOORD19;
};

struct ShadowVertexOutput { float4 position : SV_POSITION; float2 uv : TEXCOORD0; nointerpolation float alphaCutoff : TEXCOORD1; nointerpolation uint alphaMasked : TEXCOORD2; float alpha : TEXCOORD3; nointerpolation float lodCoverage : TEXCOORD4; nointerpolation uint lodSeed : TEXCOORD5; nointerpolation uint lodInvert : TEXCOORD6; };
float3 ApplyWind(float3 position, float packedWeight, uint materialClass);
float WaterVertexDisplacementHeight3D(float2 worldXZ, float time, float waveScale, float waveStrength, float waveSpeed);

ShadowVertexOutput ShadowWorldVertexMain(WorldVertexInput input)
{
    ShadowVertexOutput output = (ShadowVertexOutput)0;
    output.position = mul(lightViewProjection[shadowCascadeIndex], float4(input.position, 1.0));
    return output;
}

struct ShadowPackedInstancedVertexInput
{
    float3 position : POSITION;
    float4 normal : NORMAL;
    float4 color : COLOR;
    float2 uv : TEXCOORD0;
    float4 transformRow0 : INSTANCE0;
    float4 transformRow1 : INSTANCE1;
    float4 transformRow2 : INSTANCE2;
    float4 transformRow3 : INSTANCE3;
    float4 shadowData : SHADOWDATA0;
};

ShadowVertexOutput ShadowPackedInstancedVertexMain(ShadowPackedInstancedVertexInput input)
{
    const float4 localPosition = float4(input.position, 1.0);
    float4 worldPosition = float4(dot(input.transformRow0, localPosition),
                                  dot(input.transformRow1, localPosition),
                                  dot(input.transformRow2, localPosition),
                                  dot(input.transformRow3, localPosition));
    const uint textureData = asuint(input.shadowData.w);
    worldPosition.xyz = ApplyWind(worldPosition.xyz, input.normal.w, (textureData >> 2) & 3u);
    ShadowVertexOutput output = (ShadowVertexOutput)0;
    output.position = mul(lightViewProjection[shadowCascadeIndex], worldPosition);
    output.uv = input.uv * max(0.001, float((textureData >> 16) & 32767) * (16.0 / 32767.0));
    output.alphaCutoff = saturate(input.shadowData.x);
    output.alphaMasked = input.shadowData.y > 0.5 ? 1u : 0u;
    output.alpha = input.color.a * input.shadowData.z;
    output.lodCoverage = float((textureData >> 4) & 255u) / 255.0;
    output.lodInvert = (textureData >> 12) & 1u;
    const uint objectSeed = asuint(input.transformRow0.w) * 73856093u ^ asuint(input.transformRow1.w) * 19349663u ^ asuint(input.transformRow2.w) * 83492791u;
    output.lodSeed = StaticLodSeed(textureData, objectSeed);
    return output;
}

ShadowVertexOutput ShadowInstancedVertexMain(InstancedVertexInput input)
{
    const float4 localPosition = float4(input.position, 1.0);
    float4 worldPosition = float4(dot(input.transformRow0, localPosition), dot(input.transformRow1, localPosition), dot(input.transformRow2, localPosition), dot(input.transformRow3, localPosition));
    worldPosition.xyz = ApplyWind(worldPosition.xyz, input.normal.w, (input.textureData >> 2) & 3u);
    ShadowVertexOutput output = (ShadowVertexOutput)0;
    output.position = mul(lightViewProjection[shadowCascadeIndex], worldPosition);
    output.uv = input.uv * max(0.001, float((input.textureData >> 16) & 32767) * (16.0 / 32767.0));
    output.alphaCutoff = saturate(input.normalRow1.w); output.alphaMasked = input.normalRow2.w > 0.5 ? 1u : 0u;
    output.alpha = input.color.a * input.tint.a; output.lodCoverage = float((input.textureData >> 4) & 255u) / 255.0;
    output.lodInvert = (input.textureData >> 12) & 1u;
    const uint objectSeed = asuint(input.transformRow0.w) * 73856093u ^ asuint(input.transformRow1.w) * 19349663u ^ asuint(input.transformRow2.w) * 83492791u;
    output.lodSeed = StaticLodSeed(input.textureData, objectSeed);
    return output;
}

ShadowVertexOutput ShadowSkinnedVertexMain(SkinnedVertexInput input)
{
    float3 skinnedPosition;
    float3 skinnedNormal;
    float3 skinnedTangent;
    float3 skinnedBitangent;
    SkinLocalVertex(input, skinnedPosition, skinnedNormal, skinnedTangent, skinnedBitangent);
    const float4 localPosition = float4(skinnedPosition, 1.0);
    float4 worldPosition = float4(dot(input.transformRow0, localPosition), dot(input.transformRow1, localPosition), dot(input.transformRow2, localPosition), dot(input.transformRow3, localPosition));
    worldPosition.xyz = ApplyWind(worldPosition.xyz, input.normal.w, (input.textureData >> 2) & 3u);
    ShadowVertexOutput output = (ShadowVertexOutput)0;
    output.position = mul(lightViewProjection[shadowCascadeIndex], worldPosition);
    output.uv=input.uv*max(0.001,float((input.textureData>>16)&255u)*(16.0/255.0));
    output.alphaCutoff=float((input.skinPaletteBase>>12)&255u)/255.0;
    output.alphaMasked=(input.skinPaletteBase>>20)&1u;
    output.alpha=input.color.a*input.tint.a;
    output.lodCoverage=float((input.textureData>>4)&255u)/255.0;
    output.lodInvert=(input.textureData>>12)&1u;
    const uint objectSeed=asuint(input.transformRow0.w)*73856093u^asuint(input.transformRow1.w)*19349663u^asuint(input.transformRow2.w)*83492791u;
    output.lodSeed=((input.textureData>>13)&7u)^objectSeed;
    return output;
}

void ShadowSkinnedPixelMain(ShadowVertexOutput input)
{
    if(input.alphaMasked!=0)clip(input.alpha*baseColorTexture.Sample(materialSampler,input.uv).a-input.alphaCutoff);
    if(input.lodCoverage<0.999){const uint2 pixel=uint2(input.position.xy);uint noise=pixel.x*1973u+pixel.y*9277u+input.lodSeed*26699u+911u;noise=(noise<<13u)^noise;noise=noise*(noise*noise*15731u+789221u)+1376312589u;const float threshold=float(noise&255u)/255.0;clip(input.lodInvert!=0?threshold-input.lodCoverage-0.0000001:input.lodCoverage-threshold);}
}

float3 ApplyWind(float3 position, float packedWeight, uint materialClass)
{
    // 只有 foliage 材質會使用頂點 normal.w 的風權重，避免一般模型與角色被意外扭曲。
    const float weight = (materialClass == 2u ? 1.0 : 0.0) * step(0.0, packedWeight) * (1.0 - saturate(packedWeight));
    const float primary = sin(fogEndEnabled.w * 1.7 + position.x * 0.43 + position.z * 0.31);
    const float detail = sin(fogEndEnabled.w * 3.1 - position.x * 0.19 + position.z * 0.67);
    return position + float3(primary + detail * 0.35, abs(detail) * 0.08, detail * 0.55) * weight * 0.13;
}

WorldPixelInput WorldVertexMain(WorldVertexInput input)
{
    WorldPixelInput output = (WorldPixelInput)0;
    const float3 animatedPosition = input.position;
    output.position = mul(viewProjection, float4(animatedPosition, 1.0));
    output.color = input.color;
    output.worldPosition = animatedPosition;
    output.normal = float4(input.normal.xyz, input.normal.w < 0.0 ? 0.0 : 1.0);
    output.material = float4(0.72, 0.0, 0.0, 1.0);
    output.textureData = float4(0.0, 0.25, 0.25, 0.0);
    output.uv = input.uv;
    output.materialClass = 0;
    output.lodCoverage = 1.0;
    output.lodSeed = 0;
    output.lodInvert = 0;
    output.tangent = 0.0;
    output.characterShadingModel = 0;
    output.alphaCutoff = 0.5;
    output.alphaMasked = 0;
    output.alphaMode = 0;
    output.normalScale = 1.0;
    output.occlusionOpacity = 1.0;
    output.occlusionCoordinate = input.position;
    output.shadowPosition = mul(lightViewProjection[0], float4(animatedPosition, 1.0));
    output.farShadowPosition = mul(lightViewProjection[1], float4(animatedPosition, 1.0));
    return output;
}

WorldPixelInput InstancedVertexMain(InstancedVertexInput input)
{
    const float4 localPosition = float4(input.position, 1.0);
    float4 worldPosition = float4(dot(input.transformRow0, localPosition), dot(input.transformRow1, localPosition), dot(input.transformRow2, localPosition), dot(input.transformRow3, localPosition));
    worldPosition.xyz = ApplyWind(worldPosition.xyz, input.normal.w, (input.textureData >> 2) & 3u);
    const float3 worldNormal = float3(dot(input.normalRow0.xyz, input.normal.xyz),
                                      dot(input.normalRow1.xyz, input.normal.xyz),
                                      dot(input.normalRow2.xyz, input.normal.xyz));
    WorldPixelInput output = (WorldPixelInput)0;
    output.position = mul(viewProjection, worldPosition);
    output.color = float4(input.color.rgb * input.tint.rgb, input.color.a * input.tint.a);
    output.worldPosition = worldPosition.xyz;
    output.normal = float4(normalize(worldNormal), input.normal.w < 0.0 ? 0.0 : 1.0);
    output.material = input.material;
    const float textureScale=max(0.001,float((input.textureData>>16)&32767)*(16.0/32767.0));
    output.textureData = float4(input.textureData & 1,textureScale,textureScale,(input.textureData >> 1) & 1);
    output.uv = input.uv;
    output.materialClass = (input.textureData >> 2) & 3;
    output.lodCoverage = float((input.textureData >> 4) & 255) / 255.0;
    output.lodInvert = (input.textureData >> 12) & 1;
    // 物件平移量擴充有限的封裝 seed，讓密集植被不會同步顯示相同的螢幕孔點。
    const uint objectSeed = asuint(input.transformRow0.w) * 73856093u ^ asuint(input.transformRow1.w) * 19349663u ^ asuint(input.transformRow2.w) * 83492791u;
    output.lodSeed = StaticLodSeed(input.textureData, objectSeed);
    output.tangent = 0.0;
    output.characterShadingModel = 0;
    // normal matrix 其餘兩列的 w 承載 MASK 參數，維持既有 instance stride。
    output.alphaCutoff = saturate(input.normalRow1.w);
    output.alphaMode = (uint)round(input.normalRow2.w);
    output.alphaMasked = output.alphaMode == 1u ? 1u : 0u;
    // normal matrix 第一列的 w 原本未使用，直接承載逐材質強度而不擴充 instance stride。
    output.normalScale = input.normalRow0.w;
    output.occlusionOpacity=input.occlusionOpacity;
    output.occlusionSeeds=input.occlusionSeeds;
    output.occlusionCoordinate=input.position;
    output.shadowPosition = mul(lightViewProjection[0], worldPosition);
    output.farShadowPosition = mul(lightViewProjection[1], worldPosition);
    return output;
}

// Water owns a dedicated vertex entry and root signature.  The shared instanced
// entry above must remain independent of b4 so non-water geometry cannot inherit
// displacement; object-ID, AO and shadow retain their existing dedicated entries.
WorldPixelInput InstancedWaterVertexMain(InstancedVertexInput input)
{
    WorldPixelInput output = InstancedVertexMain(input);
    [branch] if (waterSurfaceEnabled != 0u)
    {
        const float3 broadWave = DecodeWaterTriple(WaterSurfacePacked(3), float3(.001, 0.0, -8.0), float3(8.0, .3, 8.0));
        // Production water meshes author a 0..1 shoreline handoff in normal.w.
        // The shared vertex path normalizes normal.w to a surface flag, so carry
        // the authored value through color.a only for enabled water.  The opaque
        // single-composite target does not otherwise consume water vertex alpha.
        const float waterBoundaryWeight = saturate(input.normal.w);
        output.worldPosition.y += WaterVertexDisplacementHeight3D(output.worldPosition.xz, fogEndEnabled.w, broadWave.x, broadWave.y, broadWave.z)
                                * waterBoundaryWeight;
        output.color.a = waterBoundaryWeight;
        output.position = mul(viewProjection, float4(output.worldPosition, 1.0));
        output.shadowPosition = mul(lightViewProjection[0], float4(output.worldPosition, 1.0));
        output.farShadowPosition = mul(lightViewProjection[1], float4(output.worldPosition, 1.0));
    }
    return output;
}

WorldPixelInput SkinnedVertexMain(SkinnedVertexInput input)
{
    float3 skinnedPosition;
    float3 skinnedNormal;
    float3 skinnedTangent;
    float3 skinnedBitangent;
    SkinLocalVertex(input, skinnedPosition, skinnedNormal, skinnedTangent, skinnedBitangent);
    const float4 localPosition = float4(skinnedPosition, 1.0);
    float4 worldPosition = float4(dot(input.transformRow0, localPosition),
                                  dot(input.transformRow1, localPosition),
                                  dot(input.transformRow2, localPosition),
                                  dot(input.transformRow3, localPosition));
    worldPosition.xyz = ApplyWind(worldPosition.xyz, input.normal.w, (input.textureData >> 2) & 3u);
    const float3 worldNormal = float3(dot(input.normalRow0.xyz, skinnedNormal),
                                      dot(input.normalRow1.xyz, skinnedNormal),
                                      dot(input.normalRow2.xyz, skinnedNormal));
    const float worldNormalLengthSquared = dot(worldNormal, worldNormal);
    const bool validWorldNormal = worldNormalLengthSquared > 1e-10 && worldNormalLengthSquared < 1e20;
    float3 normalizedWorldNormal = validWorldNormal
        ? worldNormal * rsqrt(worldNormalLengthSquared) : skinnedNormal;
    float3 worldTangent = float3(dot(input.normalRow0.xyz, skinnedTangent),
                                 dot(input.normalRow1.xyz, skinnedTangent),
                                 dot(input.normalRow2.xyz, skinnedTangent));
    const float3 worldBitangent = float3(dot(input.normalRow0.xyz, skinnedBitangent),
                                         dot(input.normalRow1.xyz, skinnedBitangent),
                                         dot(input.normalRow2.xyz, skinnedBitangent));
    worldTangent -= normalizedWorldNormal * dot(normalizedWorldNormal, worldTangent);
    const float worldTangentLengthSquared = dot(worldTangent, worldTangent);
    const float worldBitangentLengthSquared = dot(worldBitangent, worldBitangent);
    const bool validWorldBasis = worldTangentLengthSquared > 1e-10 && worldTangentLengthSquared < 1e20
                              && worldBitangentLengthSquared > 1e-10 && worldBitangentLengthSquared < 1e20;
    if (validWorldBasis) worldTangent *= rsqrt(worldTangentLengthSquared);
    WorldPixelInput output = (WorldPixelInput)0;
    output.position = mul(viewProjection, worldPosition);
    output.color = float4(input.color.rgb * input.tint.rgb, input.color.a * input.tint.a);
    output.worldPosition = worldPosition.xyz;
    output.normal = float4(normalizedWorldNormal, input.normal.w < 0.0 ? 0.0 : 1.0);
    // 由實際變形後的 bitangent 回推 handedness，可涵蓋負縮放與混合蒙皮；奇異基底交由像素端退回導數路徑。
    output.tangent = validWorldBasis
        ? float4(worldTangent, dot(cross(normalizedWorldNormal, worldTangent), worldBitangent) < 0.0 ? -1.0 : 1.0)
        : float4(0.0, 0.0, 0.0, 0.0);
    output.characterShadingModel = (input.textureData >> 13) & 7;
    output.alphaCutoff=float((input.skinPaletteBase>>12)&255u)/255.0;
    output.alphaMode=(uint)round(input.normalRow2.w);
    output.alphaMasked=output.alphaMode==1u?1u:0u;
    // 10 位元二補數 Q5 定點數可在不擴充實例資料的情況下保存 glTF normalTexture.scale。
    const uint encodedNormalScale=(input.skinPaletteBase>>22)&1023u;
    const int signedNormalScale=encodedNormalScale>=512u?int(encodedNormalScale)-1024:int(encodedNormalScale);
    output.normalScale=float(signedNormalScale)/32.0;
    output.material = input.material;
    output.textureData = float4(input.textureData & 1,
                                max(0.001,float((input.textureData>>16)&255u)*(16.0/255.0)),
                                max(0.001,float((input.textureData>>24)&255u)*(16.0/255.0)),
                                (input.textureData >> 1) & 1);
    output.uv = input.uv;
    output.materialClass = (input.textureData >> 2) & 3;
    output.lodCoverage = float((input.textureData >> 4) & 255) / 255.0;
    output.lodInvert = (input.textureData >> 12) & 1;
    const uint objectSeed = asuint(input.transformRow0.w) * 73856093u ^ asuint(input.transformRow1.w) * 19349663u ^ asuint(input.transformRow2.w) * 83492791u;
    output.lodSeed = ((input.textureData >> 13) & 7) ^ objectSeed;
    output.occlusionOpacity=input.occlusionOpacity;
    output.occlusionSeeds=input.occlusionSeeds;
    output.occlusionCoordinate=skinnedPosition;
    output.shadowPosition = mul(lightViewProjection[0], worldPosition);
    output.farShadowPosition = mul(lightViewProjection[1], worldPosition);
    return output;
}

WorldPixelInput ObjectIdInstancedVertexMain(InstancedVertexInput input, uint objectToken : OBJECTID)
{
    WorldPixelInput output=InstancedVertexMain(input);
    output.objectToken=objectToken;
    return output;
}

WorldPixelInput ObjectIdSkinnedVertexMain(SkinnedVertexInput input, uint objectToken : OBJECTID)
{
    WorldPixelInput output=SkinnedVertexMain(input);
    output.objectToken=objectToken;
    return output;
}

float3 BlendWeights(float3 normal)
{
    const float3 blend = pow(abs(normal), 4.0);
    return blend / max(0.001, blend.x + blend.y + blend.z);
}

float PackedArmRoughness(float4 surfaceSample)
{
    // 正式路徑固定讀 G；legacy 分支只會被測試版 bytecode 啟用，用來證明 GPU oracle 能抓回歸。
#if defined(AEVRYND_WORLD_LEGACY_ARM_ALPHA)
    return surfaceSample.a;
#else
    return surfaceSample.g;
#endif
}

float TerrainBiomeHeight(float vertexAlpha)
{
    // height-disabled 只存在於測試 bytecode，用來證明 GPU oracle 確實依賴正式 alpha 高度契約。
#if defined(AEVRYND_WORLD_TERRAIN_HEIGHT_DISABLED)
    return 0.0;
#else
    return saturate(vertexAlpha);
#endif
}

float3 DecodeAuthoredTerrainBiomeWeights(float2 encoded)
{
    // Terrain 的 signed UV 契約：+U=步道、-U=河岸濕土、+V=林下落葉。
    // 非有限值直接 fail closed 成 neutral；其餘輸入夾限並在惡意總和超過一
    // 時正規化。正式 authored vertex 互斥，光柵插值則可形成連續雙區過渡。
    if (encoded.x != encoded.x || encoded.y != encoded.y ||
        abs(encoded.x) > 65504.0 || abs(encoded.y) > 65504.0)
        return 0.0;
    float3 weights = saturate(float3(max(encoded.x, 0.0),
                                     max(encoded.y, 0.0),
                                     max(-encoded.x, 0.0)));
    const float total = weights.x + weights.y + weights.z;
    if (total > 1.0)
        weights /= total;
    return weights;
}

float TerrainAuthoredBiomeLumaScale(float3 shapedWeights)
{
    // 亮度校準只取決於已驗證的 authored 材質權重。惡意／非有限輸入
    // fail-neutral；超額總和先正規化，因此輸出嚴格限制於 wet .62 到
    // path 1.05，不會改變 neutral 或以無界曝光掩蓋材質錯誤。
    if (any(shapedWeights != shapedWeights) || any(abs(shapedWeights) > 65504.0))
        return 1.0;
    float3 authoredWeights = saturate(shapedWeights);
    const float authoredWeightTotal = authoredWeights.x + authoredWeights.y + authoredWeights.z;
    if (authoredWeightTotal <= 0.000001)
        return 1.0;
    authoredWeights /= authoredWeightTotal;
    return clamp(dot(authoredWeights, float3(1.05, 1.05, 0.62)), 0.62, 1.16);
}

float RayleighPhase(float cosine)
{
    return 3.0 * (1.0 + cosine * cosine) / (16.0 * 3.14159265);
}

float MiePhase(float cosine, float anisotropy)
{
    const float squared = anisotropy * anisotropy;
    const float denominator = max(0.0001, 1.0 + squared - 2.0 * anisotropy * cosine);
    return 3.0 * (1.0 - squared) * (1.0 + cosine * cosine) / (8.0 * 3.14159265 * (2.0 + squared) * pow(denominator, 1.5));
}

float3 EvaluatePhaseCoherentSkyRadiance(float3 rayDirection)
{
    // 相位 profile 已同時驅動森林表面的環境漫射；天空／反射若退回固定
    // 日間藍橙色，遠景輪廓便會像貼在天空上。這裡只塑形入射天空，
    // 不改 aerial extinction，避免用全畫面霧化偽造深度。
    const float vertical = clamp(rayDirection.y, -1.0, 1.0);
    const float hemisphere = pow(saturate(abs(vertical)), max(0.05, phaseSkyHorizon.a));
    const float3 poleRadiance = vertical >= 0.0 ? phaseSkyZenith.rgb : phaseSkyGround.rgb;
    return max(0.0, lerp(phaseSkyHorizon.rgb, poleRadiance, hemisphere) * phaseSkyZenith.a);
}

float3 EvaluateAnalyticSkyRadiance(float3 rayDirection, float3 sunDirection, float roughness)
{
    const float blur = saturate(roughness) * saturate(roughness);
    const float3 blurredDirection = normalize(lerp(rayDirection, float3(0.0, 1.0, 0.0), blur * 0.28));
    const float sunCosine = clamp(dot(blurredDirection, sunDirection), -1.0, 1.0);
    const float3 atmosphereBase = EvaluatePhaseCoherentSkyRadiance(blurredDirection);
    const float rayleighShape = 0.88 + RayleighPhase(sunCosine) * 1.35;
    const float sunDisk = pow(saturate(sunCosine), lerp(1400.0, 12.0, blur));
    const float sunGlow = pow(saturate(sunCosine), lerp(22.0, 4.0, blur));
    const float sunAmplitude = phaseSunDisk.a * lerp(1.0, 0.014, blur);
    const float forwardScatter = MiePhase(sunCosine, 0.76) * lerp(0.035, 0.012, blur);
    return clamp(atmosphereBase * rayleighShape
               + phaseSunDisk.rgb * (sunDisk * sunAmplitude
               + sunGlow * phaseSunDisk.a * lerp(0.020, 0.005, blur)
               + forwardScatter), 0.0, 16.0);
}

float3 EvaluateAtmosphere(float3 rayDirection, float3 sunDirection)
{
    return EvaluateAnalyticSkyRadiance(rayDirection, sunDirection, 0.0);
}

float3 EvaluateAnalyticSkyDiffuse(float3 normal, float3 sunDirection)
{
    const float upperHemisphere = saturate(normal.y * 0.5 + 0.5);
    const float horizon = pow(saturate(1.0 - abs(normal.y)), phaseSkyHorizon.a);
    const float3 hemisphereColor = lerp(phaseSkyGround.rgb,phaseSkyZenith.rgb,upperHemisphere);
    const float sunWrap = pow(saturate(dot(normal, sunDirection) * 0.5 + 0.5), 4.0);
    return max(0.0,(lerp(hemisphereColor,phaseSkyHorizon.rgb,horizon*.42)+phaseSunDisk.rgb*sunWrap*.018)*phaseAnalyticDiffuse.rgb*phaseAnalyticDiffuse.a);
}

float3 EvaluateAnalyticSkySpecular(float3 reflectionDirection, float3 sunDirection, float roughness)
{
    return EvaluateAnalyticSkyRadiance(reflectionDirection, sunDirection, clamp(roughness, 0.045, 1.0));
}

float3 RotateEnvironmentDirection(float3 direction)
{
    const float sineValue = sin(environmentRotationRadians);
    const float cosineValue = cos(environmentRotationRadians);
    return float3(direction.x * cosineValue - direction.z * sineValue,
                  direction.y,
                  direction.x * sineValue + direction.z * cosineValue);
}

float3 EvaluateEnvironmentDiffuse(float3 normal, float3 sunDirection)
{
    const float3 analyticLighting = EvaluateAnalyticSkyDiffuse(normal, sunDirection);
    float3 lighting = analyticLighting;
    [branch] if (environmentEnabled >= 0.5)
    {
        const float3 iblLighting = environmentIrradianceTexture.SampleLevel(materialSampler,
                                   RotateEnvironmentDirection(normal), 0.0);
        lighting = analyticLighting * environmentAnalyticIblWeights.x
                 + iblLighting * environmentAnalyticIblWeights.y;
    }
    const float3 blendedEnvironment=max(float3(0.0,0.0,0.0),lighting)
         * environmentDiffuseScaleIntensity.rgb * environmentDiffuseScaleIntensity.a;
    // 中文：物件專用的 typed 解析底光位於 IBL 混合後，不修改天空、曝光或各相位的混合權重。
    const float3 objectFill=environmentObjectFill.rgb*environmentObjectFill.a;
    return blendedEnvironment+objectFill*step(0.5,environmentEnabled);
}

float3 BoxProjectedEnvironmentDirection(float3 worldPosition,float3 reflectionDirection)
{
    if(environmentReserved<.5)return reflectionDirection;
    const uint base=(uint(environmentReserved)-1u)*3u;
    const float3 boxMinimum=localReflectionProbeData[base].xyz;
    const float3 boxMaximum=localReflectionProbeData[base+1u].xyz;
    const float3 capturePosition=localReflectionProbeData[base+2u].xyz;
    const float3 safeMagnitude=max(abs(reflectionDirection),.000001);
    const float3 safeDirection=lerp(-safeMagnitude,safeMagnitude,step(0.0,reflectionDirection));
    const float3 distances=lerp((boxMinimum-worldPosition)/safeDirection,(boxMaximum-worldPosition)/safeDirection,step(0.0,reflectionDirection));
    const float hitDistance=max(0.0,min(distances.x,min(distances.y,distances.z)));
    const float3 corrected=worldPosition+reflectionDirection*hitDistance-capturePosition;
    return dot(corrected,corrected)>.000001?normalize(corrected):reflectionDirection;
}

float3 EvaluateEnvironmentSpecular(float3 worldPosition,float3 reflectionDirection,float3 sunDirection,
                                   float roughness)
{
    const float3 analyticLighting = EvaluateAnalyticSkySpecular(reflectionDirection, sunDirection, roughness);
    float3 lighting = analyticLighting;
    [branch] if (environmentEnabled >= 0.5)
    {
        const float3 iblLighting = environmentSpecularTexture.SampleLevel(materialSampler,
                                   RotateEnvironmentDirection(BoxProjectedEnvironmentDirection(worldPosition,reflectionDirection)),
                                   saturate(roughness) * environmentSpecularMaximumMip);
        lighting = analyticLighting * environmentAnalyticIblWeights.z
                 + iblLighting * environmentAnalyticIblWeights.w;
    }
    return max(float3(0.0, 0.0, 0.0), lighting)
         * environmentSpecularScaleIntensity.rgb * environmentSpecularScaleIntensity.a;
}

uint CloudLatticeHash(int2 coordinate, uint seed)
{
    uint value = asuint(coordinate.x) * 0x8da6b343u
               ^ asuint(coordinate.y) * 0xd8163841u
               ^ seed;
    value ^= value >> 16u;
    value *= 0x7feb352du;
    value ^= value >> 15u;
    value *= 0x846ca68bu;
    value ^= value >> 16u;
    return value;
}

float CloudHash(int2 coordinate, uint seed = 0xcb1ab31fu)
{
    // 取最高 24 位，避免浮點乘加次序或 FMA 差異改變格點亂數。
    return float(CloudLatticeHash(coordinate, seed) >> 8u) * (1.0 / 16777216.0);
}

float CloudNoise(float2 coordinate)
{
    const int2 cell = int2(floor(coordinate));
    const float2 fraction = frac(coordinate);
    const float2 smoothFraction = fraction * fraction * (3.0 - 2.0 * fraction);
    const float lower = lerp(CloudHash(cell), CloudHash(cell + int2(1, 0)), smoothFraction.x);
    const float upper = lerp(CloudHash(cell + int2(0, 1)), CloudHash(cell + int2(1, 1)), smoothFraction.x);
    return lerp(lower, upper, smoothFraction.y);
}

float TerrainMesoPartitionSignal(float2 worldPosition, float broadNoise)
{
    // 兩個旋轉、非整比尺度的平滑值雜訊形成連續中尺度群落。單一尺度會在
    // 32--256px 的地表觀察距離留下可辨識週期；第二尺度打散該週期，但仍只
    // 依賴世界座標，沒有相機、時間或未平滑的格線雜湊。
    const float2 rotated = float2(worldPosition.x * 0.8 - worldPosition.y * 0.6,
                                  worldPosition.x * 0.6 + worldPosition.y * 0.8);
    const float middle = CloudNoise(worldPosition * 0.14 + float2(1.0, 0.7));
    const float secondary = CloudNoise(rotated * 0.38 + float2(13.7, -5.9));
    const float combined = middle * 0.62 + secondary * 0.30 + broadNoise * 0.08;
    // 混合兩個頻帶會自然收窄統計範圍；以固定世界空間對比恢復濕／乾群落
    // 的原有可讀差異，避免夜景被平均成同一種粗糙度與亮度。
    return saturate(0.5 + (combined - 0.5) * 1.45);
}

float3 TerrainGrassSecondarySamplePosition(float3 primaryPosition)
{
    // 第二域同時旋轉、非整比縮放及平移。只替換單一域的固定旋轉仍保留原本
    // 的貼圖週期；保留主域再與此域混合，才會破壞可見的重複對齊。
    const float cosine = 0.829037573;
    const float sine = 0.559192903;
    const float scale = 1.173;
    const float3 rotated = float3(primaryPosition.x * cosine - primaryPosition.z * sine,
                                  primaryPosition.y,
                                  primaryPosition.x * sine + primaryPosition.z * cosine);
    return rotated * scale + float3(31.73, -11.29, 47.91);
}

float TerrainGrassDualDomainBlend(float2 worldPosition)
{
    // 僅使用連續、低頻世界座標值雜訊；沒有相機、時間或格線跳接。
    const float2 blendPosition = worldPosition * 0.19 + float2(37.11, -19.73);
    const float signal = TerrainMesoPartitionSignal(blendPosition, CloudNoise(blendPosition * 0.021));
    const float smoothSignal = signal * signal * (3.0 - 2.0 * signal);
    return lerp(0.28, 0.62, smoothSignal);
}

float3 BlendTerrainGrassAlbedo(float3 primary, float3 secondary, float blend)
{
    const float3 luminanceWeights = float3(0.2126, 0.7152, 0.0722);
    const float clampedBlend = saturate(blend);
    const float3 mixed = lerp(primary, secondary, clampedBlend);
    const float targetLuma = lerp(dot(primary, luminanceWeights), dot(secondary, luminanceWeights), clampedBlend);
    const float mixedLuma = dot(mixed, luminanceWeights);
    return targetLuma > 0.0 && mixedLuma > 0.000001 ? mixed * (targetLuma / mixedLuma) : mixed;
}

float TerrainWetShoreBlend(float authoredWeight, float breakupSignal)
{
    // signed authored UV 決定河岸範圍；世界空間連續雜訊只破碎過渡帶。
    // 乘上 4*w*(1-w) 令純乾地／純濕土端點仍精確為 0/1，不會讓濕土
    // 穿越美術指定的邊界，也不依賴相機、畫面像素或時間。
    const float weight = saturate(authoredWeight);
    const float edge = saturate(weight + (saturate(breakupSignal) - 0.5) * 0.24
                                      * (4.0 * weight * (1.0 - weight)));
    return edge * edge * (3.0 - 2.0 * edge);
}

float WaterFoamCoverage3D(float2 worldXZ, float time)
{
    // 固定兩次連續值雜訊，採不同旋轉與非整比尺度。舊 1.17/2.03
    // cycles/m 在高空視角會連成數公尺長的規則亮塊，像沿岸鋪設的石板；
    // 提高世界空間頻率只縮小泡沫島。岸線覆蓋固定於世界座標，避免遠景
    // 高對比邊界因時間位移產生單像素閃爍；水面法線與反射動畫仍由其他項保留。
    const float2 first = float2(worldXZ.x * 0.8 - worldXZ.y * 0.6, worldXZ.x * 0.6 + worldXZ.y * 0.8) * 9.17;
    const float2 second = float2(worldXZ.x * 0.384615 - worldXZ.y * 0.923077, worldXZ.x * 0.923077 + worldXZ.y * 0.384615) * 15.73 + float2(17.3, 9.7);
    return smoothstep(0.30, 0.72, CloudNoise(first) * 0.58 + CloudNoise(second) * 0.42);
}

float CloudFbm(float2 coordinate)
{
    float value = 0.0;
    float amplitude = 0.52;
    [unroll] for (int octave = 0; octave < 5; ++octave)
    {
        value += CloudNoise(coordinate) * amplitude;
        coordinate = mul(float2x2(1.62, 1.17, -1.17, 1.62), coordinate) + 7.13;
        amplitude *= 0.5;
    }
    return value;
}

float3 ApplyProceduralClouds(float3 atmosphere, float3 rayDirection, float3 sunDirection)
{
    const float horizonFade = smoothstep(0.025, 0.205, rayDirection.y);
    const float projectionHeight = max(0.12, rayDirection.y + 0.18);
    const float2 wind = float2(0.72, 0.34) * fogEndEnabled.w * 0.012;
    const float2 cloudPoint = rayDirection.xz / projectionHeight * 1.35 + wind;
    const float broad = CloudFbm(cloudPoint);
    const float erosion = CloudFbm(cloudPoint * 2.17 - wind * 1.6);
    const float density = smoothstep(0.54, 0.72, broad - erosion * 0.16) * horizonFade;
    const float sunAmount = pow(saturate(dot(rayDirection, sunDirection) * 0.5 + 0.5), 3.0);
    const float edge = saturate(density * (1.0 - density) * 4.0);
    const float3 cloudShade = lerp(float3(0.34, 0.39, 0.46), float3(1.05, 0.91, 0.72), sunAmount);
    const float3 silverLining = float3(1.0, 0.78, 0.52) * edge * pow(saturate(dot(rayDirection, sunDirection)), 8.0) * 0.9;
    return lerp(atmosphere, cloudShade + silverLining, density * 0.78);
}

float CloudVolumeNoise(float3 position)
{
    float2 coordinate=position.xz*0.00042+position.y*float2(0.000071,-0.000053);
    float value=0.0;
    float amplitude=0.58;
    [unroll] for(int octave=0;octave<3;++octave)
    {
        value+=CloudNoise(coordinate)*amplitude;
        coordinate=mul(float2x2(1.71,1.09,-1.09,1.71),coordinate)+5.37;
        amplitude*=0.48;
    }
    return value;
}

float SampleCloudVolumeDensity(float3 position)
{
    const float normalizedHeight=saturate((position.y-cloudBaseAltitude)/max(10.0,cloudThickness));
    const float lower=saturate(normalizedHeight/0.16);
    const float upper=saturate((1.0-normalizedHeight)/0.28);
    const float profile=lower*upper;
    const float2 wind=float2(0.72,0.34)*fogEndEnabled.w*cloudWindSpeed;
    const float3 moved=float3(position.x+wind.x,position.y,position.z+wind.y);
    const float broad=CloudVolumeNoise(moved);
    const float erosion=CloudVolumeNoise(moved*2.13+float3(31.7,11.3,-19.1));
    return smoothstep(cloudCoverage,cloudCoverage+0.18,broad-cloudErosion*erosion)*profile*cloudDensityScale;
}

float EvaluateCloudShadowTransmittance(float3 worldPosition,float3 sunDirection)
{
    const float targetHeight=cloudBaseAltitude+cloudThickness*0.5;
    const float distance=(targetHeight-worldPosition.y)/sunDirection.y;
    const float density=SampleCloudVolumeDensity(worldPosition+sunDirection*distance);
    return lerp(1.0,exp(-density*1.35),cloudShadowStrength);
}

float3 ApplyVolumetricClouds(float3 atmosphere,float3 rayDirection,float3 sunDirection,float2 pixelPosition)
{
    const float entry=max(0.0,(cloudBaseAltitude-cameraFogStart.y)/rayDirection.y);
    const float exit=max(entry,(cloudBaseAltitude+cloudThickness-cameraFogStart.y)/rayDirection.y);
    const int samples=clamp((int)cloudSampleCount,4,16);
    const float stepLength=(exit-entry)/float(samples);
    const float jitter=CloudHash(int2(floor(pixelPosition))+int2(17,43));
    const float horizonFade=smoothstep(0.02,0.14,rayDirection.y);
    float transmittance=1.0;
    float3 scattering=0.0;
    [loop] for(int sampleIndex=0;sampleIndex<16;++sampleIndex)
    {
        if(sampleIndex>=samples||transmittance<0.015)break;
        const float distance=entry+(float(sampleIndex)+jitter)*stepLength;
        const float3 position=cameraFogStart.xyz+rayDirection*distance;
        const float density=SampleCloudVolumeDensity(position);
        if(density<=0.001)continue;
        const float lightDensity=SampleCloudVolumeDensity(position+sunDirection*380.0);
        const float lightTransmission=exp(-lightDensity*0.82);
        const float forward=MiePhase(saturate(dot(rayDirection,sunDirection)),0.62);
        const float3 shade=lerp(float3(0.32,0.38,0.46),float3(1.06,0.91,0.72),lightTransmission)*(.72+forward*1.8);
        const float opacity=1.0-exp(-density*0.18*horizonFade);
        scattering+=transmittance*opacity*shade;
        transmittance*=1.0-opacity;
    }
    return atmosphere*transmittance+scattering;
}

float DirectionalShadowBilinearCompare(float2 uv, uint cascadeIndex, float comparisonDepth)
{
    uint width = 0, height = 0, layers = 0, levels = 0;
    shadowTexture.GetDimensions(0, width, height, layers, levels);
    const float2 texelPosition = uv * float2(width, height) - 0.5;
    const int2 baseTexel = int2(floor(texelPosition));
    const float2 fraction = frac(texelPosition);
    // 讀取原始深度並明確執行比較。部分驅動程式可能讓 GatherCmp 在相同程序間產生一個 FP16 ULP 的差異；
    // 原始 Gather 可讓 12 點 PCSS 取樣範圍保持決定性。Gather 使用點取樣夾制取樣器，
    // 因此套用 LESS_EQUAL 語意前，逐一恢復比較取樣器的白色邊界。
    float4 depths = shadowTexture.Gather(
        screenPointClampSampler, float3(uv, cascadeIndex));
    if (baseTexel.x < 0 || baseTexel.x >= int(width) || baseTexel.y < 0 || baseTexel.y >= int(height)) depths.w = 1.0;
    if (baseTexel.x + 1 < 0 || baseTexel.x + 1 >= int(width) || baseTexel.y < 0 || baseTexel.y >= int(height)) depths.z = 1.0;
    if (baseTexel.x < 0 || baseTexel.x >= int(width) || baseTexel.y + 1 < 0 || baseTexel.y + 1 >= int(height)) depths.x = 1.0;
    if (baseTexel.x + 1 < 0 || baseTexel.x + 1 >= int(width) || baseTexel.y + 1 < 0 || baseTexel.y + 1 >= int(height)) depths.y = 1.0;
    const float4 compared = step(comparisonDepth.xxxx, depths);
    const float top = lerp(compared.w, compared.z, fraction.x);
    const float bottom = lerp(compared.x, compared.y, fraction.x);
    return lerp(top, bottom, fraction.y);
}

float DirectionalShadow(float4 shadowPosition, uint cascadeIndex, float3 worldPosition, float3 normal, float3 lightDirection)
{
    const float3 projected = shadowPosition.xyz / max(0.0001, shadowPosition.w);
    const float2 uv = float2(projected.x * 0.5 + 0.5, -projected.y * 0.5 + 0.5);
    if (shadowParameters.x < 0.5 || projected.z <= 0.0 || projected.z >= 1.0 || any(uv < 0.0) || any(uv > 1.0)) return 1.0;
    const float receiverCosine = saturate(dot(normal, lightDirection));
    const float receiverSlope = sqrt(max(0.0, 1.0 - receiverCosine * receiverCosine)) / max(0.2, receiverCosine);
    const float bias = min(0.006, shadowParameters.z + shadowParameters.y * receiverSlope);
    const uint rotationIndex = min(3u, (uint)(CloudHash(int2(floor(worldPosition.xz * 2.0)), 0xc2b2ae35u ^ (cascadeIndex * 0x9e3779b9u)) * 4.0));
    const float swapAxes = float(rotationIndex & 1u);
    const float2 rotationSigns = float2((rotationIndex == 1u || rotationIndex == 2u) ? -1.0 : 1.0, rotationIndex >= 2u ? -1.0 : 1.0);
    static const float2 poisson[12] = {
        float2(-0.326, -0.406), float2(-0.840, -0.074), float2(-0.696, 0.457), float2(-0.203, 0.621),
        float2(0.962, -0.195), float2(0.473, -0.480), float2(0.519, 0.768), float2(0.185, -0.893),
        float2(0.507, 0.064), float2(0.896, 0.412), float2(-0.322, -0.933), float2(-0.792, -0.598)
    };
    float filterRadiusTexels = 1.45;
    if (shadowSoftness.x > 0.5)
    {
        float blockerDepthSum = 0.0;
        float blockerCount = 0.0;
        [unroll] for (int blockerIndex = 0; blockerIndex < 8; ++blockerIndex)
        {
            const float2 blockerOffset = lerp(poisson[blockerIndex], poisson[blockerIndex].yx, swapAxes) * rotationSigns * shadowParameters.w * shadowSoftness.y;
            const float2 blockerUv = uv + blockerOffset;
            const float sampledDepth = any(blockerUv < 0.0) || any(blockerUv > 1.0) ? 1.0 : shadowTexture.SampleLevel(screenPointClampSampler, float3(blockerUv, cascadeIndex), 0.0);
            if (sampledDepth < projected.z - bias)
            {
                blockerDepthSum += sampledDepth;
                blockerCount += 1.0;
            }
        }
        if (blockerCount > 0.0)
        {
            const float averageBlockerDepth = blockerDepthSum / blockerCount;
            const float penumbraRatio = max(0.0, projected.z - bias - averageBlockerDepth) / max(0.0001, averageBlockerDepth);
            filterRadiusTexels = clamp(penumbraRatio * shadowSoftness.z, 1.45, shadowSoftness.w);
        }
    }
    float visibility = 0.0;
    [unroll] for (int sampleIndex = 0; sampleIndex < 12; ++sampleIndex)
    {
        const float2 offset = lerp(poisson[sampleIndex], poisson[sampleIndex].yx, swapAxes) * rotationSigns * shadowParameters.w * filterRadiusTexels;
        // 原始深度 Gather 與明確比較保留線性比較範圍，同時讓擷取證據不依賴特定實作的比較過濾。
        visibility += DirectionalShadowBilinearCompare(uv + offset, cascadeIndex, projected.z - bias);
    }
    return visibility / 12.0;
}

float CascadedDirectionalShadow(WorldPixelInput input, float3 normal, float3 lightDirection)
{
    if (shadowParameters.x < 0.5) return 1.0;
    const uint cascadeCount=max(1u,min(4u,(uint)shadowParameters.x));
    float3 viewDepthAxis=viewProjection[3].xyz;if(length(viewDepthAxis)<1e-6)viewDepthAxis=viewProjection[2].xyz;const float cameraDistance=abs(dot(input.worldPosition-cameraFogStart.xyz,normalize(viewDepthAxis)));
    uint cascade=0u;[unroll]for(uint index=0u;index<3u;++index)if(index+1u<cascadeCount&&cameraDistance>=shadowCascadeSplits[index])cascade=index+1u;
    float visibility=DirectionalShadow(mul(lightViewProjection[cascade],float4(input.worldPosition,1.0)),cascade,input.worldPosition,normal,lightDirection);
    if(cascade+1u<cascadeCount){const float previous=cascade?shadowCascadeSplits[cascade-1u]:0.0;const float width=max(.001,(shadowCascadeSplits[cascade]-previous)*shadowTransition.x);const float blend=smoothstep(shadowCascadeSplits[cascade]-width,shadowCascadeSplits[cascade],cameraDistance);if(blend>0.0)visibility=lerp(visibility,DirectionalShadow(mul(lightViewProjection[cascade+1u],float4(input.worldPosition,1.0)),cascade+1u,input.worldPosition,normal,lightDirection),blend);}
    return visibility;
}

float DirectionalLightShaftCascadeShadow(float3 worldPosition, uint cascadeIndex)
{
    // 光柱每步保留一次雙線性陰影查詢；比較與插值由 shader 明確執行，
    // 避免硬體 comparison filtering 在跨程序擷取時產生 1 ULP 漂移。
    const float4 shadowPosition = mul(lightViewProjection[cascadeIndex], float4(worldPosition, 1.0));
    if (shadowPosition.w <= 0.0001) return 1.0;
    const float3 projected = shadowPosition.xyz / shadowPosition.w;
    const float2 uv = float2(projected.x * 0.5 + 0.5, -projected.y * 0.5 + 0.5);
    if (projected.z <= 0.0 || projected.z >= 1.0 || any(uv < 0.0) || any(uv > 1.0)) return 1.0;
    return DirectionalShadowBilinearCompare(
        uv, cascadeIndex, projected.z - max(0.0007, shadowParameters.z));
}

float DirectionalLightShaftShadow(float3 worldPosition)
{
    if (shadowParameters.x < 0.5) return 1.0;
    const uint cascadeCount=max(1u,min(4u,(uint)shadowParameters.x));float3 viewDepthAxis=viewProjection[3].xyz;if(length(viewDepthAxis)<1e-6)viewDepthAxis=viewProjection[2].xyz;const float cameraDistance=abs(dot(worldPosition-cameraFogStart.xyz,normalize(viewDepthAxis)));uint cascade=0u;[unroll]for(uint index=0u;index<3u;++index)if(index+1u<cascadeCount&&cameraDistance>=shadowCascadeSplits[index])cascade=index+1u;return DirectionalLightShaftCascadeShadow(worldPosition,cascade);
}

float3 EvaluateDirectionalLightShafts(float3 worldPosition, float3 sunDirection)
{
    const float3 rayVector = worldPosition - cameraFogStart.xyz;
    const float rayLengthSquared = dot(rayVector, rayVector);
    const bool validRay = rayLengthSquared > 0.0001 && rayLengthSquared <= 1.0e12;
    const float3 safeRayVector = validRay ? rayVector : float3(0.0, 0.0, 1.0);
    const float rayLength = validRay ? sqrt(rayLengthSquared) : 1.0;
    const float pathLength = validRay ? min(rayLength, directionalLightShaftMaximumDistance) : 0.0;
    const float3 rayDirection = safeRayVector / rayLength;
    const uint sampleCount = (uint)clamp(directionalLightShaftSampleCount, 2.0, 8.0);
    const float stepLength = max(0.0, pathLength) / float(sampleCount);
    float transmittance = 1.0;
    float integratedScattering = 0.0;
    const bool enabled = directionalLightShaftIntensity > 0.0 &&
                         heightFogParameters.x > 0.0 && fogEndEnabled.y >= 0.5 && pathLength > 0.001;
    [branch] if (enabled)
    {
        [loop] for (uint sampleIndex = 0u; sampleIndex < sampleCount; ++sampleIndex)
        {
            // 固定中點步進不含時間抖動，因此同一相機與時間的 capture 可逐位元重現。
            const float distanceAlongRay = (float(sampleIndex) + 0.5) * stepLength;
            const float3 samplePosition = cameraFogStart.xyz + rayDirection * distanceAlongRay;
            const float density = heightFogParameters.x * exp(clamp(-heightFogParameters.y * (samplePosition.y - heightFogParameters.z), -20.0, 20.0));
            const float stepOpacity = 1.0 - exp(-min(density * stepLength, 80.0));
            integratedScattering += transmittance * stepOpacity * DirectionalLightShaftShadow(samplePosition);
            transmittance *= 1.0 - stepOpacity;
            if (transmittance < 0.005) break;
        }
    }
    float cloudVisibility = 1.0;
    [branch] if (enabled && volumetricCloudEnabled >= 0.5 && cloudShadowEnabled >= 0.5 && cloudShadowStrength > 0.0 &&
                 cloudDensityScale > 0.0 && sunDirection.y > 0.02)
        cloudVisibility = EvaluateCloudShadowTransmittance(cameraFogStart.xyz + rayDirection * (pathLength * 0.5), sunDirection);
    const float phase = MiePhase(clamp(dot(rayDirection, sunDirection), -1.0, 1.0), directionalLightShaftAnisotropy);
    const float scattering = min(3.0,
        max(0.0, integratedScattering * cloudVisibility * phase * directionalLightShaftIntensity));
    return enabled ? max(0.0, lightColorIntensity.rgb * lightColorIntensity.w * scattering) : float3(0.0, 0.0, 0.0);
}

float4 Triplanar(Texture2D source, float3 worldPosition, float3 normal, float scale)
{
    const float3 blend = BlendWeights(normal);
    return source.Sample(materialSampler, worldPosition.zy * scale) * blend.x
         + source.Sample(materialSampler, worldPosition.xz * scale) * blend.y
         + source.Sample(materialSampler, worldPosition.xy * scale) * blend.z;
}

float3 DecodeBc5Normal(float2 encoded)
{
    const float2 xy = encoded * 2.0 - 1.0;
    return float3(xy, sqrt(saturate(1.0 - dot(xy, xy))));
}

float3 TriplanarNormalFrom(Texture2D source, float3 worldPosition, float3 geometricNormal, float scale)
{
    const float3 blend = BlendWeights(geometricNormal);
    const float3 axisSign = sign(geometricNormal);
    const float3 x = DecodeBc5Normal(source.Sample(materialSampler, worldPosition.zy * scale).rg);
    const float3 y = DecodeBc5Normal(source.Sample(materialSampler, worldPosition.xz * scale).rg);
    const float3 z = DecodeBc5Normal(source.Sample(materialSampler, worldPosition.xy * scale).rg);
    const float3 mappedX = float3(axisSign.x * x.z, x.y, x.x);
    const float3 mappedY = float3(y.x, axisSign.y * y.z, y.y);
    const float3 mappedZ = float3(z.x, z.y, axisSign.z * z.z);
    return normalize(mappedX * blend.x + mappedY * blend.y + mappedZ * blend.z);
}

float3 TriplanarNormal(float3 worldPosition, float3 geometricNormal, float scale)
{
    return TriplanarNormalFrom(normalTexture, worldPosition, geometricNormal, scale);
}

float3 TerrainTopDownNormal(Texture2D source, float3 worldPosition, float3 geometricNormal, float scale)
{
    const float3 sampled = DecodeBc5Normal(source.Sample(materialSampler, worldPosition.xz * scale).rg);
    const float3 mapped = float3(sampled.x, sign(geometricNormal.y) * sampled.z, sampled.y);
    return normalize(mapped);
}

float3 UvNormal(float3 worldPosition, float3 geometricNormal, float2 uv)
{
    const float3 positionX = ddx(worldPosition);
    const float3 positionY = ddy(worldPosition);
    const float2 uvX = ddx(uv);
    const float2 uvY = ddy(uv);
    const float3 tangentRaw = positionX * uvY.y - positionY * uvX.y;
    const float3 bitangentRaw = -positionX * uvY.x + positionY * uvX.x;
    const float tangentLengthSquared = dot(tangentRaw, tangentRaw);
    const float bitangentLengthSquared = dot(bitangentRaw, bitangentRaw);
    const float3 basisCross = cross(tangentRaw, bitangentRaw);
    const float basisAreaSquared = dot(basisCross, basisCross);
    // NaN 會讓比較式為 false；極小與極大的導數都退回幾何法線，避免退化 UV 污染整個像素。
    if (!(tangentLengthSquared > 1e-10 && tangentLengthSquared < 1e20
       && bitangentLengthSquared > 1e-10 && bitangentLengthSquared < 1e20
       && basisAreaSquared > 1e-16 && basisAreaSquared < 1e30))
        return geometricNormal;
    const float3 tangent = tangentRaw * rsqrt(tangentLengthSquared);
    const float3 bitangent = bitangentRaw * rsqrt(bitangentLengthSquared);
    const float3 mapped = DecodeBc5Normal(normalTexture.Sample(materialSampler, uv).rg);
    const float3 mappedWorld = mapped.x * tangent + mapped.y * bitangent + mapped.z * geometricNormal;
    const float mappedLengthSquared = dot(mappedWorld, mappedWorld);
    if (!(mappedLengthSquared > 1e-10 && mappedLengthSquared < 1e20))
        return geometricNormal;
    return mappedWorld * rsqrt(mappedLengthSquared);
}

float3 UvOrVertexTangentNormal(float3 geometricNormal,float4 vertexTangent,
                               float2 uv,float3 derivativeFallback)
{
    const float tangentLengthSquared = dot(vertexTangent.xyz, vertexTangent.xyz);
    if (!(tangentLengthSquared > 1e-10 && tangentLengthSquared < 1e20
       && abs(vertexTangent.w) > 0.5 && abs(vertexTangent.w) < 1.5))
        return derivativeFallback;
    float3 tangent = vertexTangent.xyz * rsqrt(tangentLengthSquared);
    tangent -= geometricNormal * dot(geometricNormal, tangent);
    const float orthogonalLengthSquared = dot(tangent, tangent);
    if (!(orthogonalLengthSquared > 1e-10 && orthogonalLengthSquared < 1e20))
        return derivativeFallback;
    tangent *= rsqrt(orthogonalLengthSquared);
    const float3 bitangent = cross(geometricNormal, tangent) * (vertexTangent.w < 0.0 ? -1.0 : 1.0);
    const float3 mapped = DecodeBc5Normal(normalTexture.Sample(materialSampler, uv).rg);
    const float3 mappedWorld = mapped.x * tangent + mapped.y * bitangent + mapped.z * geometricNormal;
    const float mappedLengthSquared = dot(mappedWorld, mappedWorld);
    return mappedLengthSquared > 1e-10 && mappedLengthSquared < 1e20
        ? mappedWorld * rsqrt(mappedLengthSquared) : geometricNormal;
}

// 使用穩定化 GGX；分母下限犧牲極窄尖峰，避免低 roughness 產生 firefly。
float DistributionGgxStabilized(float3 normal, float3 halfDirection, float roughness)
{
    const float alpha = roughness * roughness;
    const float alphaSquared = alpha * alpha;
    const float normalHalf = saturate(dot(normal, halfDirection));
    const float denominator = normalHalf * normalHalf * (alphaSquared - 1.0) + 1.0;
    return alphaSquared / max(3.14159265 * denominator * denominator, 0.000001);
}

float GeometrySchlickGgx(float normalDirection, float roughness)
{
    const float value = roughness + 1.0;
    const float k = value * value * 0.125;
    return normalDirection / max(normalDirection * (1.0 - k) + k, 0.0001);
}

float GeometrySmith(float3 normal, float3 viewDirection, float3 lightDirection, float roughness)
{
    return GeometrySchlickGgx(saturate(dot(normal, viewDirection)), roughness)
         * GeometrySchlickGgx(saturate(dot(normal, lightDirection)), roughness);
}

float3 FresnelSchlick(float cosine, float3 reflectance)
{
    const float factor = pow(1.0 - saturate(cosine), 5.0);
    return reflectance + (1.0 - reflectance) * factor;
}

float3 FresnelSchlickRoughness(float cosine, float3 reflectance, float roughness)
{
    const float3 grazing = max(1.0 - roughness, reflectance);
    return reflectance + (grazing - reflectance) * pow(1.0 - saturate(cosine), 5.0);
}

float3 EvaluateEnvironmentLighting(float3 worldPosition,float3 normal, float3 viewDirection, float3 baseColor,
                                   float roughness, float metallic, float occlusion,
                                   float ambientIntensity, float3 sunDirection)
{
    const float3 reflectance = lerp(float3(0.04, 0.04, 0.04), baseColor, metallic);
    const float3 fresnel = FresnelSchlickRoughness(saturate(dot(normal, viewDirection)), reflectance, roughness);
    const float3 diffuseWeight = (1.0 - fresnel) * (1.0 - metallic);
    const float3 skyDiffuse = EvaluateEnvironmentDiffuse(normal, sunDirection);
    const float3 reflectionDirection = reflect(-viewDirection, normal);
    const float3 skySpecular = EvaluateEnvironmentSpecular(worldPosition,reflectionDirection, sunDirection, roughness);
    const float2 analyticBrdf=float2(0.35 + 0.65 * (1.0 - roughness), 0.0);
    const float2 sampledBrdf=environmentEnabled > 0.5
        ? environmentBrdfTexture.SampleLevel(screenPointClampSampler,
              float2(saturate(dot(normal, viewDirection)), saturate(roughness)), 0.0)
        : float2(0.0,0.0);
    const float2 brdf=sampledBrdf.x+sampledBrdf.y>0.00001?sampledBrdf:analyticBrdf;
    return (diffuseWeight * baseColor * skyDiffuse + skySpecular * (fresnel * brdf.x + brdf.y))
         * clamp(ambientIntensity, 0.0, 2.0) * saturate(occlusion);
}

float3 EvaluatePbrLight(float3 normal, float3 viewDirection, float3 lightDirection,
                        float3 radiance, float3 baseColor, float roughness, float metallic)
{
    const float normalLight = saturate(dot(normal, lightDirection));
    const float normalView = saturate(dot(normal, viewDirection));
    if (normalLight <= 0.0 || normalView <= 0.0)
        return float3(0.0,0.0,0.0);
    const float3 halfVector = lightDirection + viewDirection;
    const float3 halfDirection = halfVector * rsqrt(max(dot(halfVector, halfVector), 0.000001));
    const float3 reflectance = lerp(float3(0.04, 0.04, 0.04), baseColor, metallic);
    const float3 fresnel = FresnelSchlick(saturate(dot(halfDirection, viewDirection)), reflectance);
    const float distribution = DistributionGgxStabilized(normal, halfDirection, roughness);
    const float geometry = GeometrySmith(normal, viewDirection, lightDirection, roughness);
    const float3 specular = distribution * geometry * fresnel
                          / max(4.0 * normalView * normalLight, 0.0001);
    const float3 diffuseWeight = (1.0 - fresnel) * (1.0 - metallic);
    return (diffuseWeight * baseColor * (1.0 / 3.14159265) + specular) * radiance * normalLight;
}

float FoliagePhysicalTransmission(float normalLight,float viewLight)
{
    const float backLight=saturate(-normalLight);
    const float physicalForwardScatter=saturate(-viewLight);
    return backLight*backLight*(0.45+0.55*physicalForwardScatter);
}

float3 FoliageTwoSidedShadingNormal(float3 normal,float3 viewDirection,bool isFrontFace)
{
    // 葉片背面先進入與正面相同的觀察半球；只在背面或近掠射角時，
    // 保守向視線半球收斂，避免薄面法線在葉冠中形成突兀的黑白翻轉。
    // 正面且 facing >= 0.32 時 blend 嚴格為零，不改變一般正面材質回應。
    const float3 orientedNormal=isFrontFace?normal:-normal;
    const float rawFacing=dot(orientedNormal,viewDirection);
    const float3 hemisphereTangent=orientedNormal-viewDirection*rawFacing;
    const float3 hemisphereNormal=rawFacing<0.0?normalize(hemisphereTangent+viewDirection*0.02):orientedNormal;
    const float facing=saturate(dot(hemisphereNormal,viewDirection));
    const float grazingBlend=(1.0-smoothstep(0.08,0.32,facing))*0.12;
    const float backFaceBlend=isFrontFace?0.0:0.18;
    const float convergence=max(grazingBlend,backFaceBlend);
    const float3 viewHemisphereNormal=normalize(hemisphereNormal+viewDirection*0.18);
    return normalize(lerp(hemisphereNormal,viewHemisphereNormal,convergence));
}

float FoliageIndirectOcclusion(float combinedOcclusion,float foliageMask)
{
    // 薄葉片不應像封閉實體般承受完整 AO；僅 foliage 提升最深的間接遮蔽，
    // 在原始 AO 與平方根響應間各保留一半，抑制真實林冠中的黑色椒鹽雜點。
    // 非 foliage 走嚴格 identity，直射光、陰影與 alpha coverage 也不受影響。
    if(foliageMask<=0.5)return combinedOcclusion;
    combinedOcclusion=saturate(combinedOcclusion);
    return lerp(combinedOcclusion,sqrt(combinedOcclusion),0.50);
}

float3 FoliageNeutralizedTransmissionColor(float3 color,float transmissionWeight)
{
    color=max(color,0.0);
    const float luminance=dot(color,float3(0.2126,0.7152,0.0722));
    return lerp(color,luminance.xxx,0.08*saturate(transmissionWeight));
}

float3 FoliageNaturalDiffuseAlbedo(float3 color)
{
    // 葉片與草片是介電材質；在 sRGB 貼圖轉成線性空間後，仍不應以接近 1.0
    // 的單一通道反照率進入直射光與透射光。先以亮度等值灰階收斂過高彩度，
    // 再限制線性反照率峰值；中低彩度與暗部保留原樣，避免把所有植被洗成灰色。
    color=max(color,0.0);
    const float luminance=dot(color,float3(0.2126,0.7152,0.0722));
    const float maximumChannel=max(color.r,max(color.g,color.b));
    const float minimumChannel=min(color.r,min(color.g,color.b));
    const float chroma=maximumChannel-minimumChannel;
    const float chromaNeutralization=0.24*smoothstep(0.08,0.34,chroma);
    const float3 balancedColor=lerp(color,luminance.xxx,chromaNeutralization);
    const float balancedPeak=max(balancedColor.r,max(balancedColor.g,balancedColor.b));
    const float reflectanceScale=min(1.0,0.58/max(balancedPeak,0.000001));
    return balancedColor*reflectanceScale;
}

float CorneaFresnel(float cosine)
{
    // 人眼角膜折射率約 1.376，對空氣的法線入射反射率約為 2.5%。
    const float corneaReflectance = 0.0250428;
    return corneaReflectance + (1.0 - corneaReflectance) * pow(1.0 - saturate(cosine), 5.0);
}

float CorneaRoughness(float materialRoughness)
{
    // 角膜層維持有限寬度，避免次像素眼球產生過尖且不穩定的高光。
    return clamp(materialRoughness * 0.12, 0.025, 0.08);
}

float3 EvaluateEyeLight(float3 baseNormal, float3 corneaNormal, float3 viewDirection,
                        float3 lightDirection, float3 radiance, float3 baseColor,
                        float materialRoughness)
{
    const float3 baseLayer = EvaluatePbrLight(baseNormal, viewDirection, lightDirection,
                                               radiance, baseColor, materialRoughness, 0.0);
    const float normalLight = saturate(dot(corneaNormal, lightDirection));
    const float normalView = saturate(dot(corneaNormal, viewDirection));
    if (normalLight <= 0.0 || normalView <= 0.0)
        return baseLayer;
    const float3 halfVector = lightDirection + viewDirection;
    const float halfLengthSquared = dot(halfVector, halfVector);
    if (halfLengthSquared < 0.000001 || halfLengthSquared > 1000000.0)
        return baseLayer;
    const float3 halfDirection = halfVector * rsqrt(halfLengthSquared);
    const float fresnel = CorneaFresnel(dot(halfDirection, viewDirection));
    const float roughness = CorneaRoughness(materialRoughness);
    const float distribution = DistributionGgxStabilized(corneaNormal, halfDirection, roughness);
    const float geometry = GeometrySmith(corneaNormal, viewDirection, lightDirection, roughness);
    const float3 clearcoat = distribution * geometry * fresnel
                           / max(4.0 * normalView * normalLight, 0.0001)
                           * radiance * normalLight;
    // 角膜先取走 Fresnel 能量，再讓剩餘能量進入虹膜／鞏膜基底。
    return baseLayer * (1.0 - fresnel) + clearcoat;
}

float3 EvaluateEyeEnvironment(float3 worldPosition,float3 baseNormal, float3 corneaNormal, float3 viewDirection,
                              float3 baseColor, float materialRoughness, float occlusion,
                              float ambientIntensity, float3 sunDirection)
{
    const float3 baseLayer = EvaluateEnvironmentLighting(worldPosition,baseNormal, viewDirection, baseColor,
                                                          materialRoughness, 0.0, occlusion,
                                                          ambientIntensity, sunDirection);
    const float fresnel = CorneaFresnel(dot(corneaNormal, viewDirection));
    const float3 reflectionDirection = reflect(-viewDirection, corneaNormal);
    const float3 clearcoat = EvaluateEnvironmentSpecular(worldPosition,reflectionDirection, sunDirection,
                                                         CorneaRoughness(materialRoughness))
                           * fresnel * clamp(ambientIntensity, 0.0, 2.0) * saturate(occlusion);
    return baseLayer * (1.0 - fresnel) + clearcoat;
}

float3 EvaluateSkinSupplement(float3 normal,float3 viewDirection,float3 lightDirection,
                              float3 radiance,float3 baseColor,float roughness,float curvature)
{
    const float normalLight = dot(normal,lightDirection);
    const float narrowRoughness = max(0.09,roughness * 0.45);
    const float3 narrowSpecular = EvaluatePbrLight(normal,viewDirection,lightDirection,
                                                    radiance,0.0,narrowRoughness,0.0) * 0.18;
    if(skinSubsurfaceStrength<=0.0)return narrowSpecular;
    const float direct=saturate(normalLight);
    const float red=max(0.0,saturate((normalLight+skinSubsurfaceWrap)/(1.0+skinSubsurfaceWrap))-direct);
    const float green=max(0.0,saturate((normalLight+skinSubsurfaceWrap*.62)/(1.0+skinSubsurfaceWrap*.62))-direct);
    const float blue=max(0.0,saturate((normalLight+skinSubsurfaceWrap*.34)/(1.0+skinSubsurfaceWrap*.34))-direct);
    const float curveWeight=.35+.65*saturate(curvature*skinSubsurfaceCurvatureScale);
    const float back=saturate(-normalLight),forward=pow(saturate(dot(-lightDirection,viewDirection)),4.0);
    const float transmission=back*back*forward*skinSubsurfaceTransmission;
    const float3 diffusion=min(float3(.3,.3,.3),float3(red*curveWeight+transmission,green*curveWeight+transmission*.42,blue*curveWeight+transmission*.16)*skinSubsurfaceStrength);
    // 擴散補光逐通道限制在入射光 30%，避免與基礎 Lambert 疊加後超出皮膚可用能量。
    const float3 warmScatter=min(max(radiance,0.0)*.3,baseColor*radiance*diffusion);
    return narrowSpecular+warmScatter;
}

float3 EvaluateHairSupplement(float3 normal,float3 tangent,float3 viewDirection,float3 lightDirection,
                               float3 radiance,float3 baseColor,float roughness)
{
    // 髮絲切線無效時保留標準 PBR；避免退化骨架或奇異縮放把 NaN 傳入 HDR。
    const float tangentLengthSquared=dot(tangent,tangent);
    const float halfLengthSquared=dot(viewDirection+lightDirection,viewDirection+lightDirection);
    if(tangentLengthSquared<0.000001||tangentLengthSquared>1000000.0||halfLengthSquared<0.000001||halfLengthSquared>1000000.0)return float3(0.0,0.0,0.0);
    const float3 projectedStrand=tangent-normal*dot(normal,tangent);
    const float strandLengthSquared=dot(projectedStrand,projectedStrand);
    if(strandLengthSquared<0.000001||strandLengthSquared>1000000.0)return float3(0.0,0.0,0.0);
    const float3 strand=projectedStrand*rsqrt(strandLengthSquared);
    const float3 halfDirection=(viewDirection+lightDirection)*rsqrt(halfLengthSquared);
    const float normalLight=saturate(dot(normal,lightDirection));
    const float primarySine=sqrt(saturate(1.0-dot(strand,halfDirection)*dot(strand,halfDirection)));
    const float3 shiftedStrand=normalize(strand+normal*0.18);
    const float secondarySine=sqrt(saturate(1.0-dot(shiftedStrand,halfDirection)*dot(shiftedStrand,halfDirection)));
    const float smoothness=1.0-saturate(roughness);
    const float primary=pow(primarySine,lerp(18.0,72.0,smoothness));
    const float secondary=pow(secondarySine,lerp(36.0,112.0,smoothness));
    const float3 fiberReflectance=lerp(0.04,baseColor,0.18);
    const float3 supplement=radiance*fiberReflectance*normalLight*(primary*0.12+secondary*0.06);
    // 雙高光的額外能量逐通道限制在入射光 24%，讓 Default/Skin 的基準能量不受影響。
    return min(max(supplement,0.0),max(radiance,0.0)*0.24);
}

float3 EvaluateClothSupplement(float3 normal,float3 viewDirection,float3 lightDirection,
                               float3 radiance,float3 baseColor,float roughness)
{
    const float3 halfVector=viewDirection+lightDirection;
    const float halfLengthSquared=dot(halfVector,halfVector);
    if(halfLengthSquared<0.000001||halfLengthSquared>1000000.0)return float3(0.0,0.0,0.0);
    const float3 halfDirection=halfVector*rsqrt(halfLengthSquared);
    const float normalLight=saturate(dot(normal,lightDirection));
    const float normalView=saturate(dot(normal,viewDirection));
    const float normalHalf=saturate(dot(normal,halfDirection));
    // 以有界 Charlie-like 纖維高光模擬布料絨毛，不增加貼圖、pass 或 instance stride。
    const float sheenPower=lerp(2.0,7.0,1.0-saturate(roughness));
    const float grazingSheen=pow(saturate(1.0-normalHalf),sheenPower);
    const float viewFuzz=pow(saturate(1.0-normalView),4.0);
    const float3 fiberColor=sqrt(max(baseColor,0.0));
    const float3 supplement=radiance*fiberColor*normalLight*(grazingSheen*0.14+viewFuzz*0.035);
    return min(max(supplement,0.0),max(radiance,0.0)*0.18);
}

float CharacterNormalDetailStrength(float skinMask,float hairMask,float eyeMask,float clothMask)
{
    // 皮膚與角膜只保留低幅細節，避免壓縮法線在近距離形成塑膠凹凸；髮絲仍保留較強方向性。
    return lerp(0.78,0.56,skinMask)+hairMask*(0.68-0.78)+eyeMask*(0.32-0.78)+clothMask*(0.92-0.78);
}

float3 ScaleNormalDetail(float3 geometricNormal,float3 detailNormal,float signedScale)
{
    // 保留取樣法線的 Z 分量並只縮放切線偏移，因此 scale=1 可精確還原原細節，負值則反轉 X/Y。
    const float axialDetail=dot(geometricNormal,detailNormal);
    const float3 tangentialDetail=detailNormal-geometricNormal*axialDetail;
    return normalize(geometricNormal*axialDetail+tangentialDetail*signedScale);
}

float PointLightAttenuation(float distanceSquared, float radius)
{
    const float safeRadius = max(radius, 0.001);
    const float normalizedDistanceSquared = distanceSquared / (safeRadius * safeRadius);
    const float rangeWindow = saturate(1.0 - normalizedDistanceSquared * normalizedDistanceSquared);
    // 半徑只控制平滑截止；分母的軟化項避免光源中心產生無限亮度。
    return rangeWindow * rangeWindow / max(distanceSquared + 1.0, 0.0001);
}

float3 ReconstructWorldPosition(float2 screenUv, float depth)
{
    const float2 viewportUv = (screenUv - worldViewport.xy) / max(worldViewport.zw, float2(0.000001, 0.000001));
    const float4 clipPosition = float4(viewportUv * float2(2.0, -2.0) + float2(-1.0, 1.0), depth, 1.0);
    const float4 worldPosition = mul(inverseViewProjection, clipPosition);
    return worldPosition.xyz / max(abs(worldPosition.w), 0.000001);
}

float3 WaterReconstructWorldPositionDeterministic3D(float2 screenUv, float depth)
{
    const precise float2 viewportUv = (screenUv - worldViewport.xy) / max(worldViewport.zw, float2(0.000001, 0.000001));
    const precise float4 clipPosition = float4(viewportUv * float2(2.0, -2.0) + float2(-1.0, 1.0), depth, 1.0);
    precise float4 worldPosition;
    worldPosition.x = inverseViewProjection[0][0] * clipPosition.x;
    worldPosition.x = worldPosition.x + inverseViewProjection[0][1] * clipPosition.y;
    worldPosition.x = worldPosition.x + inverseViewProjection[0][2] * clipPosition.z;
    worldPosition.x = worldPosition.x + inverseViewProjection[0][3] * clipPosition.w;
    worldPosition.y = inverseViewProjection[1][0] * clipPosition.x;
    worldPosition.y = worldPosition.y + inverseViewProjection[1][1] * clipPosition.y;
    worldPosition.y = worldPosition.y + inverseViewProjection[1][2] * clipPosition.z;
    worldPosition.y = worldPosition.y + inverseViewProjection[1][3] * clipPosition.w;
    worldPosition.z = inverseViewProjection[2][0] * clipPosition.x;
    worldPosition.z = worldPosition.z + inverseViewProjection[2][1] * clipPosition.y;
    worldPosition.z = worldPosition.z + inverseViewProjection[2][2] * clipPosition.z;
    worldPosition.z = worldPosition.z + inverseViewProjection[2][3] * clipPosition.w;
    worldPosition.w = inverseViewProjection[3][0] * clipPosition.x;
    worldPosition.w = worldPosition.w + inverseViewProjection[3][1] * clipPosition.y;
    worldPosition.w = worldPosition.w + inverseViewProjection[3][2] * clipPosition.z;
    worldPosition.w = worldPosition.w + inverseViewProjection[3][3] * clipPosition.w;
    const precise float inverseW = 1.0 / max(abs(worldPosition.w), 0.000001);
    return worldPosition.xyz * inverseW;
}

float2 WaterDeterministicSinCos3D(float phase)
{
    // Shader intrinsic sin/cos is not required to return bit-identical results across
    // independently created pipelines. Reduce to [-pi/2, pi/2] and evaluate both
    // values with one explicitly ordered polynomial so wave height and derivative
    // cannot disagree about the phase.
    const precise float twoPi = 6.2831853071795864769;
    const precise float inverseTwoPi = 0.15915494309189533577;
    const precise float halfPi = 1.5707963267948966192;
    const precise float pi = 3.1415926535897932385;
    precise float reduced = phase - floor(phase * inverseTwoPi + 0.5) * twoPi;
    precise float cosineSign = 1.0;
    if (reduced > halfPi)
    {
        reduced = pi - reduced;
        cosineSign = -1.0;
    }
    else if (reduced < -halfPi)
    {
        reduced = -pi - reduced;
        cosineSign = -1.0;
    }
    const precise float squared = reduced * reduced;
    precise float sinePolynomial = -0.0000000250521083854417;
    sinePolynomial = 0.0000027557319223985893 + squared * sinePolynomial;
    sinePolynomial = -0.00019841269841269841 + squared * sinePolynomial;
    sinePolynomial = 0.0083333333333333332 + squared * sinePolynomial;
    sinePolynomial = -0.16666666666666667 + squared * sinePolynomial;
    const precise float sine = reduced * (1.0 + squared * sinePolynomial);
    // This is the explicit derivative of the sine polynomial above.  Reusing
    // the sine coefficients as a cosine series makes the reported height
    // gradient diverge from the height field even though both share a phase.
    precise float cosinePolynomial = -0.0000002755731922398589;
    cosinePolynomial = 0.000024801587301587301 + squared * cosinePolynomial;
    cosinePolynomial = -0.0013888888888888889 + squared * cosinePolynomial;
    cosinePolynomial = 0.041666666666666667 + squared * cosinePolynomial;
    cosinePolynomial = -0.5 + squared * cosinePolynomial;
    const precise float cosine = cosineSign * (1.0 + squared * cosinePolynomial);
    return float2(sine, cosine);
}

float3 WaterDeterministicNormalize3D(float3 value)
{
    precise float lengthSquared = value.x * value.x;
    lengthSquared = lengthSquared + value.y * value.y;
    lengthSquared = lengthSquared + value.z * value.z;
    const precise float inverseLength = 1.0 / sqrt(max(lengthSquared, 0.00000001));
    return value * inverseLength;
}

float WaterWavePixelBandLimit3D(float worldFootprint, float waveScale)
{
    // 以實際世界座標像素足跡做解析式 band-limit。每週期少於約 16px 時
    // 逐步衰減，少於約 4.5px 時完全移除，避免斜視水面把波峰變成規則閃爍條帶。
    const precise float cyclesPerPixel = max(0.0, waveScale) * worldFootprint;
    const precise float blend = saturate((cyclesPerPixel - 0.06) / 0.16);
    const precise float smoothed = blend * blend * (3.0 - 2.0 * blend);
    return 1.0 - smoothed;
}

float WaterWaveDirectionalFootprint3D(float2 reconstructedDeltaX,
                                      float2 reconstructedDeltaY,
                                      float2 interpolatedDeltaX,
                                      float2 interpolatedDeltaY,
                                      float2 waveDirection)
{
    // 掠射水面的 pixel footprint 高度各向異性；不能先取最長軸再套給所有
    // 波向。沿實際波向投影兩組既有 footprint，仍取較大值維持保守濾波，
    // 但與長軸近乎正交且仍可解析的分量不再被一併清零。
    precise float footprint = abs(dot(reconstructedDeltaX, waveDirection));
    footprint = max(footprint, abs(dot(reconstructedDeltaY, waveDirection)));
    footprint = max(footprint, abs(dot(interpolatedDeltaX, waveDirection)));
    footprint = max(footprint, abs(dot(interpolatedDeltaY, waveDirection)));
    return footprint;
}

float WaterWaveDirectionalPixelBandLimit3D(float2 reconstructedDeltaX,
                                           float2 reconstructedDeltaY,
                                           float2 interpolatedDeltaX,
                                           float2 interpolatedDeltaY,
                                           float waveScale,
                                           float2 waveDirection)
{
    return WaterWavePixelBandLimit3D(
        WaterWaveDirectionalFootprint3D(reconstructedDeltaX, reconstructedDeltaY,
                                        interpolatedDeltaX, interpolatedDeltaY,
                                        waveDirection),
        waveScale);
}

float4 WaterWaveDirectionalBandLimits3D(float2 reconstructedDeltaX,
                                        float2 reconstructedDeltaY,
                                        float2 interpolatedDeltaX,
                                        float2 interpolatedDeltaY,
                                        float waveScale,
                                        float2 direction)
{
    const precise float2 perpendicular = float2(-direction.y, direction.x);
    const precise float2 direction1 = direction * 0.978147601 + perpendicular * 0.207911691;
    const precise float2 direction2 = direction * 0.945518576 - perpendicular * 0.325568154;
    const precise float2 direction3 = direction * 0.874619707 + perpendicular * 0.484809620;
    return float4(
        WaterWaveDirectionalPixelBandLimit3D(reconstructedDeltaX, reconstructedDeltaY,
                                             interpolatedDeltaX, interpolatedDeltaY,
                                             waveScale * 0.73, direction),
        WaterWaveDirectionalPixelBandLimit3D(reconstructedDeltaX, reconstructedDeltaY,
                                             interpolatedDeltaX, interpolatedDeltaY,
                                             waveScale, direction1),
        WaterWaveDirectionalPixelBandLimit3D(reconstructedDeltaX, reconstructedDeltaY,
                                             interpolatedDeltaX, interpolatedDeltaY,
                                             waveScale * 1.37, direction2),
        WaterWaveDirectionalPixelBandLimit3D(reconstructedDeltaX, reconstructedDeltaY,
                                             interpolatedDeltaX, interpolatedDeltaY,
                                             waveScale * 1.91, direction3));
}

float WaterWaveResolvedWeight3D(float worldFootprint, float waveScale)
{
    // 與解析高度場相同的四分量頻率及能量權重。此值只描述保留下來的
    // 頻譜能量，正式高度與偏導仍在 WaterAnalyticWave3D 內逐分量一起衰減。
    const precise float4 componentWeights = float4(0.46, 0.26, 0.18, 0.10);
    precise float resolved = componentWeights.x * WaterWavePixelBandLimit3D(worldFootprint, waveScale * 0.73);
    resolved = resolved + componentWeights.y * WaterWavePixelBandLimit3D(worldFootprint, waveScale);
    resolved = resolved + componentWeights.z * WaterWavePixelBandLimit3D(worldFootprint, waveScale * 1.37);
    resolved = resolved + componentWeights.w * WaterWavePixelBandLimit3D(worldFootprint, waveScale * 1.91);
    return saturate(resolved);
}

float WaterWaveDirectionalResolvedWeight3D(float2 reconstructedDeltaX,
                                           float2 reconstructedDeltaY,
                                           float2 interpolatedDeltaX,
                                           float2 interpolatedDeltaY,
                                           float waveScale,
                                           float2 direction)
{
    const precise float4 componentWeights = float4(0.46, 0.26, 0.18, 0.10);
    const precise float4 bandLimits = WaterWaveDirectionalBandLimits3D(
        reconstructedDeltaX, reconstructedDeltaY, interpolatedDeltaX,
        interpolatedDeltaY, waveScale, direction);
    precise float resolved = componentWeights.x * bandLimits.x;
    resolved = resolved + componentWeights.y * bandLimits.y;
    resolved = resolved + componentWeights.z * bandLimits.z;
    resolved = resolved + componentWeights.w * bandLimits.w;
    return saturate(resolved);
}

float3 WaterAnalyticWaveComponents3D(float2 worldXZ, float time, float waveScale,
                                     float amplitude, float speed, float2 direction,
                                     float phaseOffset, float4 componentBandLimits)
{
    // 四個非整比、不同方向的解析波共同形成水面；高度與偏導使用完全相同
    // 的相位，因此法線仍是實際高度場的導數，而不是後製擾動。單一正弦波
    // 會在斜視河面形成規則百葉條紋，不能代表自然風浪。
    const precise float waveNumber = max(waveScale, 0.001) * 6.28318530718;
    const precise float boundedAmplitude = clamp(amplitude, 0.0, 0.3);
    const precise float2 perpendicular = float2(-direction.y, direction.x);
    // 以河流 tangent 為主要能量軸，僅加入受限橫向分量；舊版 ±35~47 度
    // 的等權展開會讓四組頻譜交會成規律菱形高光。
    const precise float2 direction1 = direction * 0.978147601 + perpendicular * 0.207911691;
    const precise float2 direction2 = direction * 0.945518576 - perpendicular * 0.325568154;
    const precise float2 direction3 = direction * 0.874619707 + perpendicular * 0.484809620;
    const precise float waveNumber0 = waveNumber * 0.73;
    const precise float waveNumber1 = waveNumber;
    const precise float waveNumber2 = waveNumber * 1.37;
    const precise float waveNumber3 = waveNumber * 1.91;
    precise float projected0 = direction.x * worldXZ.x;
    projected0 = projected0 + direction.y * worldXZ.y;
    precise float projected1 = direction1.x * worldXZ.x;
    projected1 = projected1 + direction1.y * worldXZ.y;
    precise float projected2 = direction2.x * worldXZ.x;
    projected2 = projected2 + direction2.y * worldXZ.y;
    precise float projected3 = direction3.x * worldXZ.x;
    projected3 = projected3 + direction3.y * worldXZ.y;
    precise float phase0 = waveNumber0 * projected0;
    phase0 = phase0 - (speed * 0.83) * time;
    phase0 = phase0 + phaseOffset;
    precise float phase1 = waveNumber1 * projected1;
    phase1 = phase1 - speed * time;
    phase1 = phase1 + 1.7 + phaseOffset * 1.37;
    precise float phase2 = waveNumber2 * projected2;
    phase2 = phase2 - (speed * 1.19) * time;
    phase2 = phase2 + 4.1 + phaseOffset * 0.73;
    precise float phase3 = waveNumber3 * projected3;
    phase3 = phase3 - (speed * 1.43) * time;
    phase3 = phase3 + 2.4 + phaseOffset * 1.91;
    const precise float2 wave0 = WaterDeterministicSinCos3D(phase0);
    const precise float2 wave1 = WaterDeterministicSinCos3D(phase1);
    const precise float2 wave2 = WaterDeterministicSinCos3D(phase2);
    const precise float2 wave3 = WaterDeterministicSinCos3D(phase3);
    // 非等權能譜讓順流長波承擔主要形狀，高頻只提供細碎法線；總權重為 1，
    // 高度仍嚴格受 amplitude 上限約束。
    const precise float4 componentWeights = float4(0.46, 0.26, 0.18, 0.10);
    // 每個頻率分量依自己的 cycles-per-pixel 退場。舊路徑以最高 1.91x
    // 頻率得到單一 group 權重，斜視時會連仍可解析的 0.73x/1.0x 長波一起
    // 清零。高度與偏導共用完全相同的解析權重，避免法線脫離實際高度場。
    const precise float resolvedWeight0 = componentWeights.x * componentBandLimits.x;
    const precise float resolvedWeight1 = componentWeights.y * componentBandLimits.y;
    const precise float resolvedWeight2 = componentWeights.z * componentBandLimits.z;
    const precise float resolvedWeight3 = componentWeights.w * componentBandLimits.w;
    const precise float slope0 = boundedAmplitude * resolvedWeight0 * waveNumber0 * wave0.y;
    const precise float slope1 = boundedAmplitude * resolvedWeight1 * waveNumber1 * wave1.y;
    const precise float slope2 = boundedAmplitude * resolvedWeight2 * waveNumber2 * wave2.y;
    const precise float slope3 = boundedAmplitude * resolvedWeight3 * waveNumber3 * wave3.y;
    precise float height = wave0.x * resolvedWeight0 + wave1.x * resolvedWeight1;
    height = height + wave2.x * resolvedWeight2 + wave3.x * resolvedWeight3;
    height = boundedAmplitude * height;
    precise float2 gradient = direction * slope0;
    gradient = gradient + direction1 * slope1;
    gradient = gradient + direction2 * slope2;
    gradient = gradient + direction3 * slope3;
    return float3(height, gradient.x, gradient.y);
}

float3 WaterAnalyticWave3D(float2 worldXZ, float time, float waveScale,
                           float amplitude, float speed, float2 direction,
                           float phaseOffset, float worldFootprint)
{
    const precise float4 bandLimits = float4(
        WaterWavePixelBandLimit3D(worldFootprint, waveScale * 0.73),
        WaterWavePixelBandLimit3D(worldFootprint, waveScale),
        WaterWavePixelBandLimit3D(worldFootprint, waveScale * 1.37),
        WaterWavePixelBandLimit3D(worldFootprint, waveScale * 1.91));
    return WaterAnalyticWaveComponents3D(worldXZ, time, waveScale, amplitude,
                                         speed, direction, phaseOffset, bandLimits);
}

float3 WaterAnalyticDirectionalWave3D(float2 worldXZ, float time,
                                      float waveScale, float amplitude,
                                      float speed, float2 direction,
                                      float phaseOffset,
                                      float2 reconstructedDeltaX,
                                      float2 reconstructedDeltaY,
                                      float2 interpolatedDeltaX,
                                      float2 interpolatedDeltaY)
{
    const precise float4 bandLimits = WaterWaveDirectionalBandLimits3D(
        reconstructedDeltaX, reconstructedDeltaY, interpolatedDeltaX,
        interpolatedDeltaY, waveScale, direction);
    return WaterAnalyticWaveComponents3D(worldXZ, time, waveScale, amplitude,
                                         speed, direction, phaseOffset, bandLimits);
}

float WaterVertexDisplacementHeight3D(float2 worldXZ, float time, float waveScale, float waveStrength, float waveSpeed)
{
    // 頂點只重用兩組可解析低頻波；細波仍由像素法線承擔。0.16 的幾何比例
    // 在正式 .16/.136m 波強度下將最壞位移限制為約 4.7cm，再以 5cm 硬上限
    // 保護 hostile preset。此路徑不取樣貼圖，也不增加 draw 或 pass。
    const precise float2 riverTangent = float2(0.28, 0.96);
    const precise float2 riverAcross = float2(-0.96, 0.28);
    // footprint=0 保留原本的頂點位移頻譜；只有像素法線依畫面解析度退場。
    const precise float3 broadWave = WaterAnalyticWave3D(worldXZ, time, waveScale, waveStrength, waveSpeed, riverTangent, 0.31, 0.0);
    const precise float3 crossWave = WaterAnalyticWave3D(worldXZ, time, waveScale * 1.31, waveStrength * 0.85, waveSpeed * 0.73, riverTangent * 0.573576436 + riverAcross * 0.819152044, 2.03, 0.0);
    return clamp((broadWave.x + crossWave.x) * 0.16, -0.05, 0.05);
}

float WaterDomainWarpDistance3D(float broadWaveScale, float detailWaveScale)
{
    // 驅動項取自已設定的寬波，因此同時以公尺與波長比例限制位移；
    // 寬波界線也把 Jacobian 擾動限制在約 2*pi*.10。
    return min(0.32, min(0.18 / max(detailWaveScale, 0.001),
                         0.10 / max(broadWaveScale, 0.001)));
}

float2 WaterDomainWarpPosition3D(float2 worldXZ, float2 acrossDirection,
                                 float3 driverWave, float driverStrength,
                                 float warpDistance)
{
    const float inverseStrength = driverStrength > 0.0001 ? 1.0 / driverStrength : 0.0;
    return worldXZ + acrossDirection * (driverWave.x * inverseStrength * warpDistance);
}

float2 WaterDomainWarpGradient3D(float2 warpedGradient, float2 acrossDirection,
                                 float3 driverWave, float driverStrength,
                                 float warpDistance)
{
    // q(p)=p+a*h(p)，且 J(q)^T*g = g + grad(h)*dot(a,g)；如此擾動後的
    // 法線仍是同一個扭曲高度場的導數。
    const float inverseStrength = driverStrength > 0.0001 ? 1.0 / driverStrength : 0.0;
    const float2 driverGradient = float2(driverWave.y, driverWave.z) * inverseStrength;
    return warpedGradient + driverGradient * (warpDistance * dot(acrossDirection, warpedGradient));
}

float WaterDepthSampleValidity3D(float verticalDepth, float measuredThickness)
{
    // distance() alone cannot distinguish a submerged receiver from opaque geometry
    // reconstructed above the water plane. Ordered finite bounds reject NaN and both
    // infinities without relying on driver-specific isfinite lowering. The small
    // negative tolerance preserves quantized/co-planar shore samples; every other
    // invalid reconstruction fails deep through waterDepthValid == 0.
    const bool finiteReasonableVerticalDepth = verticalDepth >= -0.02
                                              && verticalDepth < 100000.0;
    const bool finiteReasonableThickness = measuredThickness >= 0.0
                                         && measuredThickness < 100000.0;
    return finiteReasonableVerticalDepth && finiteReasonableThickness ? 1.0 : 0.0;
}

float WaterDepthSlice3D(float shoreDepth, float depthValid)
{
    // 只使用已重建的垂直水深建立低頻視覺切片；深度無效時回到深水中性值，
    // 避免天空或輪廓 depth 被誤當成淺岸。這裡不取樣、不依賴相機距離或曝光。
    const float boundedDepth = clamp(shoreDepth, 0.0, 8.0);
    const float depthSlice = smoothstep(0.06, 1.35, boundedDepth);
    return lerp(1.0, depthSlice, saturate(depthValid));
}

float WaterReflectionRoughness3D(float minimumRoughness, float maximumRoughness,
                                 float resolvedSlope, float unresolvedDetail,
                                 float foamCoverage, float depthSlice)
{
    // 已解析坡度已由 waterReflectionNormal 改變反射方向，不能再把同一坡度
    // 當成微面粗糙度展寬一次。粗糙度只承接未解析頻譜、泡沫及淺岸擾動；
    // resolvedSlope 保留在介面中供診斷比對，不參與 lobe 寬度。
    const float weight = saturate(0.12 + unresolvedDetail * 0.30
                                        + foamCoverage * 0.70
                                        + (1.0 - saturate(depthSlice)) * 0.32);
    return lerp(minimumRoughness, maximumRoughness, weight);
}

precise float3 WaterOrderedScaleAdd3D(precise float3 first, precise float firstScale,
                                      precise float3 second, precise float secondScale)
{
    // single-composite 會把結果直接寫入 FP16 HDR。逐分量分離乘法與加法，
    // 避免向量 lerp/mad 在不同 wave 排程下改變最後一個 half-ULP。
    precise float3 firstTerm;
    firstTerm.r = first.r * firstScale;
    firstTerm.g = first.g * firstScale;
    firstTerm.b = first.b * firstScale;
    precise float3 secondTerm;
    secondTerm.r = second.r * secondScale;
    secondTerm.g = second.g * secondScale;
    secondTerm.b = second.b * secondScale;
    precise float3 result;
    result.r = firstTerm.r + secondTerm.r;
    result.g = firstTerm.g + secondTerm.g;
    result.b = firstTerm.b + secondTerm.b;
    return result;
}

precise float3 WaterOrderedMultiply3D(precise float3 first, precise float3 second)
{
    precise float3 result;
    result.r = first.r * second.r;
    result.g = first.g * second.g;
    result.b = first.b * second.b;
    return result;
}

precise float3 WaterOrderedLerp3D(precise float3 first, precise float3 second,
                                  precise float weight)
{
    precise const float inverseWeight = 1.0 - weight;
    return WaterOrderedScaleAdd3D(first, inverseWeight, second, weight);
}

precise float3 WaterLitScatter3D(precise float3 deepColor, precise float3 surfaceColor,
                                 precise float3 illuminance)
{
    // 混合散射反照率後乘入射光；黑暗時不可殘留固定青色。
    const precise float3 scatterAlbedo = WaterOrderedScaleAdd3D(deepColor, 0.76,
                                                                 surfaceColor, 0.24);
    return WaterOrderedMultiply3D(scatterAlbedo, illuminance);
}

precise float3 WaterReflectionFallback3D(precise float3 environmentRadiance,
                                         precise float3 screenRadiance,
                                         precise float confidence)
{
    // SSR miss 與離屏保留環境反射；有效命中依可信度平滑取代。
    return WaterOrderedLerp3D(environmentRadiance, screenRadiance,
                              clamp(confidence, 0.0, 1.0));
}

float4 EvaluateWaterScreenSpaceReflection(float3 worldPosition,float3 normal,float3 viewDirection,float roughness)
{
    if(screenSpaceReflectionIntensity<=0.0||screenSpaceReflectionSceneAvailable<0.5||roughness>=screenSpaceReflectionMaximumRoughness)return float4(0.0,0.0,0.0,0.0);
    const float3 reflectionDirection=normalize(reflect(-viewDirection,normal));
    const float3 rayOrigin=worldPosition+normal*.035;
    const float minimumDistance=min(.18,screenSpaceReflectionMaximumDistance*.02);
    const float2 viewportMaximum=worldViewport.xy+worldViewport.zw;
    float previousRayDistance=0.0;
    float previousDepthGap=0.0;
    bool previousValid=false;
    [loop] for(uint stepIndex=0;stepIndex<16u;++stepIndex)
    {
        if(stepIndex>=(uint)screenSpaceReflectionSampleCount)break;
        const float fraction=float(stepIndex+1u)/screenSpaceReflectionSampleCount;
        const float rayDistance=lerp(minimumDistance,screenSpaceReflectionMaximumDistance,fraction*fraction);
        const float3 rayPosition=rayOrigin+reflectionDirection*rayDistance;
        const float4 clipPosition=mul(viewProjection,float4(rayPosition,1.0));
        if(clipPosition.w<=0.000001)break;
        const float2 viewportUv=clipPosition.xy/clipPosition.w*float2(.5,-.5)+.5;
        const float2 screenUv=worldViewport.xy+viewportUv*worldViewport.zw;
        if(any(screenUv<=worldViewport.xy)||any(screenUv>=viewportMaximum))break;
        const float sampledDepth=sceneDepthTexture.SampleLevel(screenPointClampSampler,screenUv,0.0);
        if(sampledDepth<=0.0||sampledDepth>=.999999)continue;
        const precise float3 sampledWorldPosition=WaterReconstructWorldPositionDeterministic3D(screenUv,sampledDepth);
        const float rayCameraDistance=distance(cameraFogStart.xyz,rayPosition);
        const float sceneCameraDistance=distance(cameraFogStart.xyz,sampledWorldPosition);
        const float depthGap=rayCameraDistance-sceneCameraDistance;
        float2 hitUv=screenUv;
        float hitDepthGap=depthGap;
        // Quadratic SSR steps become several metres apart near the end of the
        // ray. Once the ray crosses from in front of scene depth to behind it,
        // refine that real bracket before applying the original thickness
        // test; otherwise valid cottage/tree reflections are skipped by a
        // large overshoot. The fixed iteration count remains deterministic.
        if(depthGap>=0.0&&previousValid&&previousDepthGap<0.0)
        {
            float lowerDistance=previousRayDistance;
            float upperDistance=rayDistance;
            [unroll] for(uint refinement=0u;refinement<3u;++refinement)
            {
                const float midpointDistance=(lowerDistance+upperDistance)*.5;
                const float3 midpointPosition=rayOrigin+reflectionDirection*midpointDistance;
                const float4 midpointClip=mul(viewProjection,float4(midpointPosition,1.0));
                if(midpointClip.w<=0.000001)break;
                const float2 midpointViewportUv=midpointClip.xy/midpointClip.w*float2(.5,-.5)+.5;
                const float2 midpointUv=worldViewport.xy+midpointViewportUv*worldViewport.zw;
                if(any(midpointUv<=worldViewport.xy)||any(midpointUv>=viewportMaximum))break;
                const float midpointDepth=sceneDepthTexture.SampleLevel(screenPointClampSampler,midpointUv,0.0);
                if(midpointDepth<=0.0||midpointDepth>=.999999)
                {
                    lowerDistance=midpointDistance;
                    continue;
                }
                const precise float3 midpointWorldPosition=WaterReconstructWorldPositionDeterministic3D(midpointUv,midpointDepth);
                const float midpointGap=distance(cameraFogStart.xyz,midpointPosition)-distance(cameraFogStart.xyz,midpointWorldPosition);
                if(midpointGap>=0.0)
                {
                    upperDistance=midpointDistance;
                    hitUv=midpointUv;
                    hitDepthGap=midpointGap;
                }
                else
                {
                    lowerDistance=midpointDistance;
                }
            }
        }
        if(hitDepthGap>=0.0&&hitDepthGap<=screenSpaceReflectionThickness)
        {
            const float2 hitViewportUv=(hitUv-worldViewport.xy)/worldViewport.zw;
            const float2 edgeDistance=min(hitViewportUv,1.0-hitViewportUv);
            const float edgeFade=saturate(min(edgeDistance.x,edgeDistance.y)*12.0);
            const float roughnessFade=1.0-saturate(roughness/screenSpaceReflectionMaximumRoughness);
            const float hitConfidence=(1.0-hitDepthGap/screenSpaceReflectionThickness)*edgeFade*roughnessFade*screenSpaceReflectionIntensity;
            // SSR 僅補上畫面內可見幾何；低信心與 miss 由既有 IBL 無縫接手。
            return float4(opaqueSceneColorTexture.SampleLevel(screenPointClampSampler,hitUv,0.0).rgb,saturate(hitConfidence));
        }
        if(depthGap<0.0){previousRayDistance=rayDistance;previousDepthGap=depthGap;previousValid=true;}
    }
    return float4(0.0,0.0,0.0,0.0);
}

float MaskedAlphaToCoverage(float materialAlpha, float alphaCutoff)
{
    // 只在門檻附近保留窄漸層，讓 MSAA alpha-to-coverage 產生穩定的子像素覆蓋率。
    const float transitionWidth = max(fwidth(materialAlpha) * 0.5, 1.0 / 255.0);
    return smoothstep(alphaCutoff - transitionWidth, alphaCutoff + transitionWidth, materialAlpha);
}

void ClipWorldLod(WorldPixelInput input)
{
    // 固定的螢幕空間雜訊讓兩級網格互補淡化，不依賴時間所以不會逐幀閃爍。
    if (input.lodCoverage < 0.999)
    {
        const uint2 pixel = uint2(input.position.xy);
        uint noise = pixel.x * 1973u + pixel.y * 9277u + input.lodSeed * 26699u + 911u;
        noise = (noise << 13u) ^ noise;
        noise = noise * (noise * noise * 15731u + 789221u) + 1376312589u;
        const float threshold = float(noise & 255u) / 255.0;
        const float signedCoverage = input.lodInvert != 0 ? threshold - input.lodCoverage - 0.0000001 : input.lodCoverage - threshold;
        clip(signedCoverage);
    }
}

float3 WaterSurfaceViewDirection(float3 worldPosition, float3 cameraPosition, float4x4 cameraMatrix)
{
    // 正交投影的深度列提供固定視軸；不以有限相機位置製造隨像素改變的 Fresnel。
    // 裸 identity 僅是低階繪圖／測試的 clip-space 約定，沒有可推回的相機姿態；
    // 此時沿用顯式 camera position，避免把任意 XY 平面誤判成 100% 掠射反射。
    const bool identityClip = abs(cameraMatrix[0][0] - 1.0) + abs(cameraMatrix[1][1] - 1.0)
                            + abs(cameraMatrix[2][2] - 1.0) + abs(cameraMatrix[3][3] - 1.0)
                            + abs(cameraMatrix[0][1]) + abs(cameraMatrix[0][2]) + abs(cameraMatrix[0][3])
                            + abs(cameraMatrix[1][0]) + abs(cameraMatrix[1][2]) + abs(cameraMatrix[1][3])
                            + abs(cameraMatrix[2][0]) + abs(cameraMatrix[2][1]) + abs(cameraMatrix[2][3])
                            + abs(cameraMatrix[3][0]) + abs(cameraMatrix[3][1]) + abs(cameraMatrix[3][2]) < 0.00001;
    if (identityClip) return normalize(cameraPosition - worldPosition);
    const bool orthographic = abs(cameraMatrix[3][0]) + abs(cameraMatrix[3][1]) + abs(cameraMatrix[3][2]) < 0.00001
                           && abs(cameraMatrix[3][3] - 1.0) < 0.00001;
    if (orthographic) return normalize(float3(-cameraMatrix[2][0], -cameraMatrix[2][1], -cameraMatrix[2][2]));
    return normalize(cameraPosition - worldPosition);
}

float4 EvaluateWorldPixel(WorldPixelInput input, bool isFrontFace, bool alphaToCoverage)
{
    ClipWorldLod(input);
    const float3 geometricNormal = input.materialClass == 1
        ? WaterDeterministicNormalize3D(input.normal.xyz)
        : normalize(input.normal.xyz);
    float3 normal = geometricNormal;
    float roughness = clamp(input.material.x, 0.045, 1.0);
    const float skinMask = input.characterShadingModel == 1 ? 1.0 : 0.0;
    const float hairMask = input.characterShadingModel == 2 ? 1.0 : 0.0;
    const float eyeMask = input.characterShadingModel == 3 ? 1.0 : 0.0;
    const float clothMask = input.characterShadingModel == 4 ? 1.0 : 0.0;
    const float normalDetailStrength=CharacterNormalDetailStrength(skinMask,hairMask,eyeMask,clothMask)*input.normalScale;
    float metallic = lerp(saturate(input.material.y),0.0,saturate(skinMask+hairMask+eyeMask+clothMask));
    const float emissive = saturate(input.material.z);
    float occlusion = saturate(input.material.w);
    float3 baseColor = input.color.rgb;
    float materialAlpha = input.color.a;
    if (input.textureData.x > 0.5 && input.materialClass != 3)
    {
        if (input.textureData.w > 0.5)
        {
            const float2 baseUv=input.uv*input.textureData.y;
            const float2 detailUv=input.uv*input.textureData.z;
            const float4 sampledBaseColor=baseColorTexture.Sample(materialSampler,baseUv);
            materialAlpha *= sampledBaseColor.a;
            if(input.alphaMasked!=0&&!alphaToCoverage)clip(materialAlpha-input.alphaCutoff);
            baseColor *= sampledBaseColor.rgb;
            const float4 surfaceSample=roughnessTexture.Sample(materialSampler,detailUv);
            occlusion*=surfaceSample.r;
            // Packed ARM 固定使用 R/G/B=AO/Roughness/Metallic；A 僅保留給資產容器，不得改變 PBR。
            roughness=clamp(roughness*PackedArmRoughness(surfaceSample),0.045,1.0);
            metallic*=surfaceSample.b;
            // 導數必須在一致控制流中計算；有可靠頂點 tangent 時僅將它保留為奇異變形的安全退路。
            const float3 derivativeNormal = UvNormal(input.worldPosition, normal, detailUv);
            normal = ScaleNormalDetail(normal,UvOrVertexTangentNormal(normal,input.tangent,detailUv,derivativeNormal),normalDetailStrength);
        }
        else
        {
            baseColor *= Triplanar(baseColorTexture, input.worldPosition, normal, input.textureData.y).rgb;
            const float4 surfaceSample=Triplanar(roughnessTexture,input.worldPosition,normal,input.textureData.z);
            occlusion*=surfaceSample.r;
            roughness=clamp(roughness*PackedArmRoughness(surfaceSample),0.045,1.0);
            metallic*=surfaceSample.b;
            normal = ScaleNormalDetail(normal,TriplanarNormal(input.worldPosition,normal,input.textureData.z),normalDetailStrength);
        }
    }
    // MASK 的材質 alpha 已先裁切；遮擋淡化再套用同一個穩定圖樣。
    ClipOcclusionOpacity(input.occlusionCoordinate,input.occlusionOpacity,input.occlusionSeeds);

    const float terrainMask = input.materialClass == 3 ? 1.0 : 0.0;
    float terrainDetailWeight = 1.0;
    [branch] if (terrainMask > 0.5)
    {
        const float macroNoise = CloudNoise(input.worldPosition.xz * 0.021);
        // 連續值雜訊取代整格跳變的 hash；維持確定性，同時避免近地表出現
        // 約 31cm 的方格色塊。
        const float microNoise = CloudNoise(input.worldPosition.xz * 3.2);
        const float terrainCameraDistance = distance(input.worldPosition, cameraFogStart.xyz);
        float distanceMacroVariation = 1.0;
        float3 grassSamplePosition = input.worldPosition;
        float3 soilSamplePosition = input.worldPosition;
        float3 rockSamplePosition = input.worldPosition;
        float terrainChromaNoise = 0.5;
        const bool terrainDecorrelationEnabled = TerrainDecorrelationEnabled();
        // 關閉時維持舊路徑且不計算額外噪聲；純 2D 不進入 terrain branch，因此沒有像素成本。
        [branch] if (terrainDistanceMaterialEnabled > 0.5)
        {
            terrainDetailWeight = 1.0 - smoothstep(terrainDetailFadeStart, terrainDetailFadeEnd, terrainCameraDistance);
            const float distanceMacroNoise = CloudNoise(input.worldPosition.xz * terrainMacroVariationScale);
            distanceMacroVariation = lerp(1.0 - terrainMacroVariationStrength, 1.0 + terrainMacroVariationStrength, distanceMacroNoise);
            [branch] if (terrainDecorrelationEnabled)
            {
                const float warpScale = DecodeTerrainDecorrelation(0u, 0.0001, 0.05);
                const float warpStrength = DecodeTerrainDecorrelation(5u, 0.0, 8.0);
                const float3 decorrelatedPosition = TerrainDecorrelatedWorldPosition(input.worldPosition, warpScale, warpStrength);
                grassSamplePosition = decorrelatedPosition * DecodeTerrainDecorrelation(10u, 0.5, 1.75);
                soilSamplePosition = decorrelatedPosition * DecodeTerrainDecorrelation(15u, 0.5, 1.75);
                rockSamplePosition = decorrelatedPosition * DecodeTerrainDecorrelation(20u, 0.5, 1.75);
                terrainChromaNoise = CloudNoise(input.worldPosition.zx * (terrainMacroVariationScale * 0.73) + float2(19.31, -7.17));
            }
        }
        const float terrainSlope = 1.0 - saturate(abs(geometricNormal.y));
        const float terrainHeight = TerrainBiomeHeight(input.color.a);
        float rockWeight = smoothstep(0.38, 0.78, terrainSlope + max(0.0, terrainHeight - 0.62) * 0.35);
        const float originalRockWeight = rockWeight;
        const float originalSoilWeight = (1.0 - rockWeight) * smoothstep(0.18, 0.62, terrainSlope + (1.0 - macroNoise) * 0.18);
        const float originalGrassWeight = max(0.0, 1.0 - rockWeight - originalSoilWeight);
        float soilWeight = originalSoilWeight;
        float grassWeight = originalGrassWeight;
        float terrainMesoSignal = 0.5;
        [branch] if (terrainDistanceMaterialEnabled > 0.5 && TerrainMesoPartitionEnabled())
        {
            // 岩石仍完全由坡度／高度決定；只在剩餘權重內重新分配草地與土壤。
            const float nonRockWeight = max(0.0, 1.0 - rockWeight);
            const float originalSoilFraction = nonRockWeight > 0.000001 ? saturate(soilWeight / nonRockWeight) : 0.0;
            terrainMesoSignal = TerrainMesoPartitionSignal(input.worldPosition.xz, macroNoise);
            const float targetSoilFraction = smoothstep(0.43, 0.70, 1.0 - terrainMesoSignal);
            soilWeight = nonRockWeight * lerp(originalSoilFraction, targetSoilFraction, 0.62);
            grassWeight = nonRockWeight - soilWeight;
        }
        // 保留三個 authored 權重直到 PBR 合成完成；後段只能用這些相同權重
        // 做有界的區域反應，不能重新解碼 UV 或增加任何材質取樣。
        float authoredPathWeight = 0.0;
        float authoredLeafLitterWeight = 0.0;
        float authoredWetSoilWeight = 0.0;
        float showcaseTerrainAuthoredBiomeCalibration = 0.0;
        float showcaseTerrainAuthoredBiomeLumaScale = 1.0;
        const bool showcaseTerrainProductionBinding =
            input.textureData.w > 0.5 && input.normalScale == -15.5;
        // Showcase 的第三個實體槽是 stony_dirt_path，不是可全域散佈的
        // geology/rock layer。程序式坡度仍可決定 forest 與 Ground108 的
        // 比例，但在沒有 authored +U path 權重時，第三槽必須精確為零。
        // 這也讓中性高地與陡坡不再間接取樣道路紋理。
        [branch] if (showcaseTerrainProductionBinding)
        {
            soilWeight += rockWeight;
            rockWeight = 0.0;
        }
        // useUv 是既有 generic bit；再要求 Renderer.h 宣告、可精確表示的
        // -15.5 marker，避免任何歷史 Terrain UV 或一般負 normalScale 誤啟用。
        [branch] if (showcaseTerrainProductionBinding)
        {
            const float3 authoredBiomeWeights = DecodeAuthoredTerrainBiomeWeights(input.uv);
            const float authoredBiomeWeight = authoredBiomeWeights.x + authoredBiomeWeights.y + authoredBiomeWeights.z;
            // neutral 必須完全繞過重配，讓 useUv 開啟但 UV=(0,0) 時逐位元沿用
            // 原 procedural slope/height/noise 路徑。
            [branch] if (authoredBiomeWeight > 0.0)
            {
                const float2 wetBreakupCoordinate = float2(
                    input.worldPosition.x * 0.8 - input.worldPosition.z * 0.6,
                    input.worldPosition.x * 0.6 + input.worldPosition.z * 0.8);
                const float wetBreakup = CloudNoise(input.worldPosition.xz * 0.31 + float2(7.1, -3.7)) * 0.65
                                       + CloudNoise(wetBreakupCoordinate * 0.83 + float2(-11.3, 5.9)) * 0.35;
                authoredPathWeight = authoredBiomeWeights.x;
                authoredLeafLitterWeight = authoredBiomeWeights.y;
                authoredWetSoilWeight = TerrainWetShoreBlend(authoredBiomeWeights.z, wetBreakup);
                const float3 shapedAuthoredBiomeWeights = float3(
                    authoredPathWeight, authoredLeafLitterWeight, authoredWetSoilWeight);
                const float shapedAuthoredBiomeWeight = shapedAuthoredBiomeWeights.x
                                                      + shapedAuthoredBiomeWeights.y
                                                      + shapedAuthoredBiomeWeights.z;
                // materialWeights 已按 authored 強度混合；校準再次使用 sqrt
                // 會放大低權重外緣，形成高空可見遮罩。線性強度保持連續且
                // 只施加一次區域影響。
                showcaseTerrainAuthoredBiomeCalibration = saturate(shapedAuthoredBiomeWeight);
                showcaseTerrainAuthoredBiomeLumaScale =
                    TerrainAuthoredBiomeLumaScale(shapedAuthoredBiomeWeights);
                // 實際綁定順序固定為 forest / Ground108 / stony path。
                // path 因此主導第三層；leaf litter 以森林層為主；wet soil
                // 使用 Ground108。三者仍共用既有 9/3 次 PBR sample。
                const float3 pathTarget = float3(0.04, 0.18, 0.78);
                const float3 leafTarget = float3(0.86, 0.12, 0.02);
                const float3 wetSoilTarget = float3(0.04, 0.92, 0.04);
                const float3 authoredLayerTarget =
                    (shapedAuthoredBiomeWeights.x * pathTarget +
                     shapedAuthoredBiomeWeights.y * leafTarget +
                     shapedAuthoredBiomeWeights.z * wetSoilTarget) /
                    max(0.000001, shapedAuthoredBiomeWeight);
                float3 materialWeights = saturate(lerp(
                    float3(grassWeight, soilWeight, rockWeight),
                    authoredLayerTarget, saturate(shapedAuthoredBiomeWeight)));
                materialWeights /= max(0.000001,
                    materialWeights.x + materialWeights.y + materialWeights.z);
                grassWeight = materialWeights.x;
                soilWeight = materialWeights.y;
                rockWeight = materialWeights.z;
            }
        }
        const float3 terrainMaterialDepthContrast = TerrainMesoPartitionEnabled()
            ? TerrainMaterialDepthContrast(terrainMesoSignal,
                float3(authoredPathWeight, authoredLeafLitterWeight, authoredWetSoilWeight))
            : 1.0.xxx;
        // Keep one secondary material domain per pixel.  The showcase path and
        // wet bank are authored as rock- and soil-dominant layers, so the old
        // grass-only route left the two hero transitions visibly tiled.  A
        // narrow zero-secondary handoff band prevents a hard domain switch at
        // authored edges, while detail weight removes the extra samples at the
        // far-material cutoff.
        uint terrainSecondaryDomainLayer = 3u;
        float terrainSecondaryDomainBlend = 0.0;
        [branch] if (terrainDecorrelationEnabled && TerrainMesoPartitionEnabled())
        {
            const float authoredSecondaryWeight = max(authoredPathWeight, authoredWetSoilWeight);
            const float authoredSecondaryActivation = smoothstep(0.20, 0.55, authoredSecondaryWeight);
            const float grassSecondaryActivation = smoothstep(0.45, 0.72, grassWeight)
                                                   * (1.0 - smoothstep(0.05, 0.20, authoredSecondaryWeight));
            float selectedActivation = grassSecondaryActivation;
            const float authoredSecondaryDominance = authoredPathWeight - authoredWetSoilWeight;
            if (authoredSecondaryDominance > 0.04 && authoredSecondaryActivation > 0.0)
            {
                terrainSecondaryDomainLayer = 2u;
                selectedActivation = authoredSecondaryActivation;
            }
            else if (authoredSecondaryDominance < -0.04 && authoredSecondaryActivation > 0.0)
            {
                terrainSecondaryDomainLayer = 1u;
                selectedActivation = authoredSecondaryActivation;
            }
            else if (grassSecondaryActivation > 0.0)
                terrainSecondaryDomainLayer = 0u;
            terrainSecondaryDomainBlend = TerrainGrassDualDomainBlend(input.worldPosition.xz)
                                        * selectedActivation * terrainDetailWeight;
        }
        if (input.textureData.x > 0.5)
        {
            // 三層都使用同一個世界空間三平面投影；只讓 rock 使用 triplanar 會讓草地與土壤在陡坡拉伸。
            float3 grassColor = Triplanar(baseColorTexture, grassSamplePosition, geometricNormal, input.textureData.y).rgb;
            float3 soilColor = Triplanar(soilBaseColorTexture, soilSamplePosition, geometricNormal, input.textureData.y).rgb;
            float3 rockColor = Triplanar(rockBaseColorTexture, rockSamplePosition, geometricNormal, input.textureData.y).rgb;
            // The selected secondary domain is reused by base color, ARM and
            // normal.  This keeps the existing worst case rather than stacking
            // grass, path and wet-bank secondary samples in the same pixel.
            [branch] if (terrainSecondaryDomainBlend > 0.001)
            {
                if (terrainSecondaryDomainLayer == 0u)
                {
                    const float3 secondaryGrassColor = Triplanar(baseColorTexture, TerrainGrassSecondarySamplePosition(grassSamplePosition), geometricNormal, input.textureData.y).rgb;
                    grassColor = BlendTerrainGrassAlbedo(grassColor, secondaryGrassColor, terrainSecondaryDomainBlend);
                }
                else if (terrainSecondaryDomainLayer == 1u)
                {
                    const float3 secondarySoilColor = Triplanar(soilBaseColorTexture, TerrainGrassSecondarySamplePosition(soilSamplePosition), geometricNormal, input.textureData.y).rgb;
                    soilColor = BlendTerrainGrassAlbedo(soilColor, secondarySoilColor, terrainSecondaryDomainBlend);
                }
                else if (terrainSecondaryDomainLayer == 2u)
                {
                    const float3 secondaryRockColor = Triplanar(rockBaseColorTexture, TerrainGrassSecondarySamplePosition(rockSamplePosition), geometricNormal, input.textureData.y).rgb;
                    rockColor = BlendTerrainGrassAlbedo(rockColor, secondaryRockColor, terrainSecondaryDomainBlend);
                }
            }
            // forest_ground_04 保留可讀的線性反照率，但不再把綠色通道推到
            // 大片撞上 .82 上限。三通道接近等能量，讓苔蘚色主要來自真實
            // 貼圖與 authored biome，而不是以高增益製造螢光草坪。
            const float3 terrainGrassReflectanceNormalization =
                lerp(1.0.xxx, float3(1.72, 1.58, 1.68), showcaseTerrainAuthoredBiomeCalibration);
            const float3 calibratedGrassColor = grassColor * terrainGrassReflectanceNormalization;
            float4 terrainSurface = 1.0;
            float3 terrainNormal = geometricNormal;
            // 遠距離分支省略六次細節貼圖取樣；近景維持完整表面與法線，轉場以解析度無關權重淡化。
            [branch] if (terrainDetailWeight > 0.001)
            {
                // surface map 固定為 R=AO、G=roughness、B=metallic；A 不參與材質能量。
                float4 grassSurface=Triplanar(roughnessTexture,grassSamplePosition,geometricNormal,input.textureData.z);
                float4 soilSurface=Triplanar(soilRoughnessTexture,soilSamplePosition,geometricNormal,input.textureData.z);
                float4 rockSurface=Triplanar(rockRoughnessTexture,rockSamplePosition,geometricNormal,input.textureData.z);
                [branch] if (terrainSecondaryDomainBlend > 0.001)
                {
                    if (terrainSecondaryDomainLayer == 0u)
                    {
                        const float4 secondaryGrassSurface = Triplanar(roughnessTexture,
                            TerrainGrassSecondarySamplePosition(grassSamplePosition), geometricNormal, input.textureData.z);
                        grassSurface = lerp(grassSurface, secondaryGrassSurface, terrainSecondaryDomainBlend);
                    }
                    else if (terrainSecondaryDomainLayer == 1u)
                    {
                        const float4 secondarySoilSurface = Triplanar(soilRoughnessTexture,
                            TerrainGrassSecondarySamplePosition(soilSamplePosition), geometricNormal, input.textureData.z);
                        soilSurface = lerp(soilSurface, secondarySoilSurface, terrainSecondaryDomainBlend);
                    }
                    else if (terrainSecondaryDomainLayer == 2u)
                    {
                        const float4 secondaryRockSurface = Triplanar(rockRoughnessTexture,
                            TerrainGrassSecondarySamplePosition(rockSamplePosition), geometricNormal, input.textureData.z);
                        rockSurface = lerp(rockSurface, secondaryRockSurface, terrainSecondaryDomainBlend);
                    }
                }
                // forest_ground_04 的 packed roughness 平均值為 .9128；Showcase
                // terrain 的基礎 roughness 尚會乘 .78。只將 forest layer 輕微
                // 收斂到 packed mean 約 .90（最終約 .702），保留 AO、metallic
                // 以及 Ground108/path 的原始表面資料。界限避免極端 texel 形成
                // 鏡面閃點或重新退化為沒有微表面層次的平坦地表。
                const float calibratedGrassRoughness = clamp(grassSurface.g * 0.986, 0.68, 0.98);
                grassSurface.g = lerp(grassSurface.g, calibratedGrassRoughness,
                                      showcaseTerrainAuthoredBiomeCalibration);
                const float4 sampledSurface=grassSurface*grassWeight+soilSurface*soilWeight+rockSurface*rockWeight;
                terrainSurface=lerp(1.0,sampledSurface,terrainDetailWeight);
                float3 grassNormal = TriplanarNormalFrom(normalTexture, grassSamplePosition, geometricNormal, input.textureData.z);
                float3 soilNormal = TriplanarNormalFrom(soilNormalTexture, soilSamplePosition, geometricNormal, input.textureData.z);
                float3 rockNormal = TriplanarNormalFrom(rockNormalTexture, rockSamplePosition, geometricNormal, input.textureData.z);
                [branch] if (terrainSecondaryDomainBlend > 0.001)
                {
                    if (terrainSecondaryDomainLayer == 0u)
                    {
                        const float3 secondaryGrassNormal = TriplanarNormalFrom(normalTexture,
                            TerrainGrassSecondarySamplePosition(grassSamplePosition), geometricNormal, input.textureData.z);
                        grassNormal = normalize(lerp(grassNormal, secondaryGrassNormal, terrainSecondaryDomainBlend));
                    }
                    else if (terrainSecondaryDomainLayer == 1u)
                    {
                        const float3 secondarySoilNormal = TriplanarNormalFrom(soilNormalTexture,
                            TerrainGrassSecondarySamplePosition(soilSamplePosition), geometricNormal, input.textureData.z);
                        soilNormal = normalize(lerp(soilNormal, secondarySoilNormal, terrainSecondaryDomainBlend));
                    }
                    else if (terrainSecondaryDomainLayer == 2u)
                    {
                        const float3 secondaryRockNormal = TriplanarNormalFrom(rockNormalTexture,
                            TerrainGrassSecondarySamplePosition(rockSamplePosition), geometricNormal, input.textureData.z);
                        rockNormal = normalize(lerp(rockNormal, secondaryRockNormal, terrainSecondaryDomainBlend));
                    }
                }
                const float3 sampledNormal=normalize(grassNormal*grassWeight+soilNormal*soilWeight+rockNormal*rockWeight);
                terrainNormal=normalize(lerp(geometricNormal,sampledNormal,saturate(terrainDetailWeight*terrainNormalDetailStrength)));
            }
            // 微變化隨細節權重淡出，宏觀變化保留到遠景，避免遠距閃爍又維持近中遠色彩連續。
            const float microVariation=lerp(1.0,lerp(0.92,1.08,microNoise),terrainDetailWeight);
            // 原始曝光基準也必須遵守相同角色；否則即使 rockWeight 已歸零，
            // stony path 仍會透過 luma reference 汙染未標記的坡地。
            const float3 originalTerrainAlbedo = calibratedGrassColor * originalGrassWeight + soilColor * (originalSoilWeight + originalRockWeight);
            float3 terrainAlbedo = calibratedGrassColor * grassWeight + soilColor * soilWeight + rockColor * rockWeight;
            [branch] if (TerrainMesoPartitionEnabled())
            {
                // 真實材質權重可改變，但先守住原本曝光，再以克制的色相區分苔蘚與落葉土。
                // 舊色偏會在日景把地面推成螢光綠／橘；新的色偏保留群落辨識，同時更接近林下漫射色。
                const float3 biomeTint = grassWeight * float3(0.88, 1.05, 0.78) + soilWeight * float3(1.16, 0.91, 0.72) + rockWeight;
                terrainAlbedo *= biomeTint;
                const float originalLuma = dot(originalTerrainAlbedo, float3(0.2126, 0.7152, 0.0722));
                const float partitionLuma = dot(terrainAlbedo, float3(0.2126, 0.7152, 0.0722));
                // 程序式 meso 分區仍守住原曝光；明確 authored 材質則保留自身物理亮度，
                // 避免將草、土路與岩面強制拉成同一明度而壓平層次。
                const float referenceLuma = lerp(originalLuma,
                    partitionLuma * showcaseTerrainAuthoredBiomeLumaScale,
                    showcaseTerrainAuthoredBiomeCalibration);
                if (originalLuma > 0.0 && partitionLuma > 0.000001)
                    terrainAlbedo *= referenceLuma / partitionLuma;
                terrainAlbedo *= TerrainForestFloorSpectralVariation(
                    terrainMesoSignal, macroNoise, grassWeight, soilWeight,
                    showcaseTerrainAuthoredBiomeCalibration);
                // 中尺度亮度變化代表樹冠遮蔽、苔蘚與落葉堆積；範圍仍有界且不依賴相機或時間。
                // 讓遠景在細節貼圖淡出後仍保有可讀層次，避免大片同亮度地表形成規律重複。
                terrainAlbedo *= lerp(0.90, 1.08, terrainMesoSignal);
            }
            [branch] if (terrainDecorrelationEnabled)
                terrainAlbedo = ApplyTerrainChromaVariation(terrainAlbedo, terrainChromaNoise,
                    DecodeTerrainDecorrelation(25u, 0.0, 0.2));
            // 最終只做小幅冷色域平衡，抵銷日光下的黃綠偏移；不依賴時間、
            // 光源或相機，也不覆蓋各 layer 的真實反照率。
            const float3 terrainProductionSpectralTrim =
                lerp(1.0.xxx, float3(0.94, 0.96, 1.04), showcaseTerrainAuthoredBiomeCalibration);
            // 最終介電質輸出仍保留 .82 上限。
            baseColor = min(baseColor * terrainAlbedo * terrainProductionSpectralTrim * microVariation *
                            distanceMacroVariation, 0.82.xxx);
            occlusion*=terrainSurface.r;
            roughness=clamp(roughness*PackedArmRoughness(terrainSurface)*(1.0+terrainFarRoughnessBoost*(1.0-terrainDetailWeight)),0.045,1.0);
            metallic*=terrainSurface.b;
            normal = terrainNormal;
        }
        else
        {
            const float3 originalTerrainTint = originalGrassWeight * float3(0.72, 0.91, 0.63) + (originalSoilWeight + originalRockWeight) * float3(0.82, 0.67, 0.48);
            float3 terrainTint = grassWeight * float3(0.76, 0.92, 0.64) + soilWeight * float3(0.90, 0.70, 0.50) + rockWeight * float3(0.76, 0.78, 0.80);
            [branch] if (TerrainMesoPartitionEnabled())
            {
                const float originalLuma = dot(originalTerrainTint, float3(0.2126, 0.7152, 0.0722));
                const float partitionLuma = dot(terrainTint, float3(0.2126, 0.7152, 0.0722));
                const float referenceLuma = lerp(originalLuma,
                    partitionLuma * showcaseTerrainAuthoredBiomeLumaScale,
                    showcaseTerrainAuthoredBiomeCalibration);
                if (originalLuma > 0.0 && partitionLuma > 0.000001)
                    terrainTint *= referenceLuma / partitionLuma;
                terrainTint *= lerp(0.84, 1.16, terrainMesoSignal);
            }
            const float3 decorrelatedTerrainTint = terrainDecorrelationEnabled
                ? ApplyTerrainChromaVariation(terrainTint, terrainChromaNoise,
                    DecodeTerrainDecorrelation(25u, 0.0, 0.2))
                : terrainTint;
            baseColor *= decorrelatedTerrainTint * lerp(0.92, 1.08, microNoise) * distanceMacroVariation;
            roughness = clamp(roughness * (grassWeight * 0.86 + soilWeight * 1.03 + rockWeight * 0.74) * (1.0+terrainFarRoughnessBoost*(1.0-terrainDetailWeight)), 0.045, 1.0);
        }
        [branch] if (authoredWetSoilWeight > 0.0)
        {
            // 河岸濕土只做受限的暗冷能量與微表面反應，不增加 sample、
            // 不冒充積水；roughness 下限避免形成黑色鏡面。
            // Ground108 原圖偏暖；濕岸以冷色吸收校正靠近水體色域，
            // 並沿 authored weight 平滑退回遠岸原材質。
            const float3 riverWetSoilAlbedo = float3(0.62, 0.72, 0.74);
            baseColor *= lerp(1.0.xxx, riverWetSoilAlbedo, authoredWetSoilWeight);
            roughness = clamp(roughness * lerp(1.0, 0.90, authoredWetSoilWeight),
                              0.24, 1.0);
        }
        // 最後套用一次即可同時涵蓋貼圖與 fallback 路徑；介電質 .82 上限已在
        // 各路徑內處理，contrast 後再次夾限，避免 path 增益越過該能量界線。
        baseColor *= terrainMaterialDepthContrast;
        baseColor = min(baseColor, 0.82.xxx);
    }

    const float foliageMask = input.materialClass == 2 ? 1.0 : 0.0;
    [branch] if (foliageMask > 0.5)
        baseColor=FoliageNaturalDiffuseAlbedo(baseColor);

    const float skyMask = 1.0 - saturate(input.normal.w);
    const float3 skyRay = normalize(input.worldPosition - cameraFogStart.xyz);
    const float3 skyLightDirection = -normalize(lightDirectionAmbient.xyz);
    // 水波 band-limit 不依賴 quad helper lane。以目前裝置深度明確重建相鄰像素的
    // 世界位置，讓三角形邊界、裁切與不同程序的 wave 配置仍得到相同像素足跡。
    const float2 waterPixelUv = input.position.xy * colorPipeline.zw;
    const float2 waterViewportMaximum = worldViewport.xy + worldViewport.zw;
    const precise float3 waterWorldCenter = WaterReconstructWorldPositionDeterministic3D(waterPixelUv, input.position.z);
    const precise float3 waterWorldNeighbourX = WaterReconstructWorldPositionDeterministic3D(
        min(waterPixelUv + float2(colorPipeline.z, 0.0), waterViewportMaximum), input.position.z);
    const precise float3 waterWorldNeighbourY = WaterReconstructWorldPositionDeterministic3D(
        min(waterPixelUv + float2(0.0, colorPipeline.w), waterViewportMaximum), input.position.z);
    const precise float2 waterWorldDeltaX = waterWorldNeighbourX.xz - waterWorldCenter.xz;
    const precise float2 waterWorldDeltaY = waterWorldNeighbourY.xz - waterWorldCenter.xz;
    precise float waterWorldLengthSquaredX = waterWorldDeltaX.x * waterWorldDeltaX.x;
    waterWorldLengthSquaredX = waterWorldLengthSquaredX + waterWorldDeltaX.y * waterWorldDeltaX.y;
    precise float waterWorldLengthSquaredY = waterWorldDeltaY.x * waterWorldDeltaY.x;
    waterWorldLengthSquaredY = waterWorldLengthSquaredY + waterWorldDeltaY.y * waterWorldDeltaY.y;
    // 固定 depth 重建的鄰點並不一定落在同一個透視水面上，斜視時會低估
    // 真正的世界空間像素跨度。插值 world position 的 quad derivative 才是
    // 當前三角形表面上的實際 footprint；取兩者較大值只會更保守地濾除
    // 不可解析波，不會重新引入高頻。
    const float2 waterInterpolatedDeltaX = ddx(input.worldPosition.xz);
    const float2 waterInterpolatedDeltaY = ddy(input.worldPosition.xz);
    const float waterInterpolatedFootprint = max(length(waterInterpolatedDeltaX), length(waterInterpolatedDeltaY));
    const precise float waterWorldFootprint = max(max(sqrt(waterWorldLengthSquaredX), sqrt(waterWorldLengthSquaredY)),
                                                   waterInterpolatedFootprint);
    float4 evaluatedOutput=float4(0.0,0.0,0.0,0.0);
    if (skyMask > 0.5)
    {
        float3 skyColor=EvaluateAtmosphere(skyRay,skyLightDirection);
        [branch] if(volumetricCloudEnabled>=0.5&&cloudDensityScale>0.0&&skyRay.y>0.001&&cameraFogStart.y<cloudBaseAltitude+cloudThickness)
            skyColor=ApplyVolumetricClouds(skyColor,skyRay,skyLightDirection,input.position.xy);
        // 天空不需要執行材質、陰影與點光源路徑，直接輸出曝光後的線性 HDR。
        const float skyAlpha = alphaToCoverage && input.alphaMasked != 0
                             ? MaskedAlphaToCoverage(materialAlpha, input.alphaCutoff)
                             : input.color.a;
        evaluatedOutput=float4(max(0.0, skyColor), skyAlpha);
    }
    else
    {
    const float3 waterAbsorption=DecodeWaterTriple(WaterSurfacePacked(0),0.0,4.0);
    const float3 waterDeepColor=DecodeWaterTriple(WaterSurfacePacked(1),0.0,4.0);
    const float3 waterReflectanceRoughness=DecodeWaterTriple(WaterSurfacePacked(2),float3(.005,.02,.02),float3(.12,.6,1.0));
    const float3 waterBroadWave=DecodeWaterTriple(WaterSurfacePacked(3),float3(.001,0.0,-8.0),float3(8.0,.3,8.0));
    const float3 waterDetailWave=DecodeWaterTriple(WaterSurfacePacked(4),float3(.001,0.0,-16.0),float3(16.0,.2,16.0));
    const float3 waterFoam=DecodeWaterTriple(WaterSurfacePacked(5),float3(0.0,.001,0.0),float3(4.0,8.0,1.0));
    const float3 waterAlphaNight=DecodeWaterTriple(WaterSurfacePacked(6),float3(0.0,0.0,.005),float3(1.0,1.0,8.0));
    // 10-bit欄位各自解碼後再次建立跨欄不變式，避免量化令smoothstep邊界倒置或相等。
    const float waterReflectance=waterSurfaceEnabled!=0u?waterReflectanceRoughness.x:.0203732;
    const float waterMinimumRoughness=waterSurfaceEnabled!=0u?waterReflectanceRoughness.y:.045;
    const float waterMaximumRoughness=waterSurfaceEnabled!=0u?max(waterMinimumRoughness+.001,waterReflectanceRoughness.z):.18;
    const float waterBroadWaveScale=waterBroadWave.x,waterBroadWaveStrength=waterBroadWave.y,waterBroadWaveSpeed=waterBroadWave.z;
    const float waterDetailWaveScale=waterDetailWave.x,waterDetailWaveStrength=waterDetailWave.y,waterDetailWaveSpeed=waterDetailWave.z;
    const float waterFoamStartDepth=waterSurfaceEnabled!=0u?waterFoam.x:.06;
    const float waterFoamEndDepth=waterSurfaceEnabled!=0u?max(waterFoamStartDepth+.001,waterFoam.y):.72;
    const float waterFoamStrength=waterSurfaceEnabled!=0u?waterFoam.z:1.0;
    const float waterShallowAlpha=waterSurfaceEnabled!=0u?waterAlphaNight.x:.34;
    const float waterDeepAlpha=waterSurfaceEnabled!=0u?max(waterShallowAlpha,waterAlphaNight.y):.91;
    const float waterMaximumNightRadiance=waterSurfaceEnabled!=0u?waterAlphaNight.z:8.0;

    // 水材質的所有片段都必須遵守其管線合成契約；朝上門檻只限制水平水波與岸泡。
    const float waterMaterialMask = input.materialClass == 1 ? 1.0 : 0.0;
#if AEVRYND_WATER_AERIAL_DIAGNOSTIC == 19
    float2 waterDiagnosticHeightGradient = 0.0;
#endif
    const float waterMask = waterMaterialMask * step(0.35, normal.y);
    const float waterCompositeMask = max(waterMask, waterMaterialMask * float(waterSurfaceEnabled != 0u));
    float foamMask = waterSurfaceEnabled != 0u ? 0.0 : waterMask * smoothstep(0.30, 0.50, roughness);
    float waterFoamCoverage = 0.0;
    float waterResolvedSlope = 0.0;
    float waterUnresolvedDetail = 0.0;
    float2 waterLowFrequencyGradient = 0.0;
    const float3 waterReflectionBaseNormal = normal;
    float waterThickness = 1.6;
    float opticalPathLength = 1.6;
    float shoreDepth = 1.6;
    float waterDepthValid = 0.0;
#if AEVRYND_WATER_AERIAL_DIAGNOSTIC >= 10
    // 因果診斷：保存雜訊之前的岸泡與導數，不更動正式計算。
    float waterDiagnosticShore = 0.0;
    float waterDiagnosticDerivative = 0.0;
    // D 必須在岸泡分支外估算，避免把分支遮罩誤判為雜訊不連續。
    const float2 diagnosticFoamFlow = input.worldPosition.xz * 0.42 + float2(fogEndEnabled.w * 0.07, -fogEndEnabled.w * 0.04);
    const float diagnosticFoamWave = 0.5 + 0.5 * sin(input.worldPosition.z * 1.35 + input.worldPosition.x * 0.31 + fogEndEnabled.w * 1.8);
    const float waterDiagnosticFoamCoverage = smoothstep(0.34, 0.76, CloudNoise(diagnosticFoamFlow) * 0.72 + diagnosticFoamWave * 0.28);
#endif
    // Enabled water uses color.a for the interpolated contour weight; optical
    // coverage comes from the packed shallow/deep water settings.
    const float waterBoundaryWeight = waterSurfaceEnabled != 0u && waterMaterialMask > 0.5
        ? saturate(input.color.a) : 1.0;
    float waterOutputAlpha = waterSurfaceEnabled != 0u && waterMaterialMask > 0.5
        ? waterDeepAlpha : input.color.a;
    const float2 waterScreenUv = input.position.xy * colorPipeline.zw;
    [branch] if (waterMask > 0.5 && worldViewport.z > 0.0 && worldViewport.w > 0.0)
    {
        const float2 screenUv = waterScreenUv;
        const float2 viewportMaximum = worldViewport.xy + worldViewport.zw;
        const float opaqueDepth = sceneDepthTexture.SampleLevel(screenPointClampSampler, screenUv, 0.0);
        const bool insideViewport = all(screenUv >= worldViewport.xy) && all(screenUv <= viewportMaximum);
        if (insideViewport && opaqueDepth > 0.0 && opaqueDepth < 0.999999)
        {
            const precise float3 bottomWorldPosition = WaterReconstructWorldPositionDeterministic3D(screenUv, opaqueDepth);
            const float measuredThickness = distance(bottomWorldPosition, input.worldPosition);
            const float verticalDepth = input.worldPosition.y - bottomWorldPosition.y;
            if (WaterDepthSampleValidity3D(verticalDepth, measuredThickness) > 0.5)
            {
                waterDepthValid = 1.0;
                waterThickness = clamp(measuredThickness, 0.0, 8.0);
                opticalPathLength = waterThickness;
                shoreDepth = waterSurfaceEnabled != 0u
                    ? clamp(max(0.0, verticalDepth), 0.0, 8.0)
                    : waterThickness;
                // 新路徑用垂直岸深避免斜視放大，導數寬度讓岸線跨解析度保持連續；停用時維持舊公式。
                // 深度斷層不可把導數放大成整片泡沫，只保留窄帶像素抗鋸齒。
                const float shoreDerivativeMaximum = max(0.002, min(0.08, (waterFoamEndDepth - waterFoamStartDepth) * 0.25));
                // Use explicit neighbouring depth samples instead of fwidth inside the
                // depth-valid branch. Invalid neighbours fall back to the current shore
                // depth, keeping silhouettes deterministic without widening foam.
                const float2 neighbourUvX = min(screenUv + float2(colorPipeline.z, 0.0), viewportMaximum);
                const float2 neighbourUvY = min(screenUv + float2(0.0, colorPipeline.w), viewportMaximum);
                const float neighbourOpaqueDepthX = sceneDepthTexture.SampleLevel(screenPointClampSampler, neighbourUvX, 0.0);
                const float neighbourOpaqueDepthY = sceneDepthTexture.SampleLevel(screenPointClampSampler, neighbourUvY, 0.0);
                float neighbourShoreDepthX = shoreDepth;
                float neighbourShoreDepthY = shoreDepth;
                if (neighbourOpaqueDepthX > 0.0 && neighbourOpaqueDepthX < 0.999999)
                {
                    const precise float3 neighbourBottomX = WaterReconstructWorldPositionDeterministic3D(neighbourUvX, neighbourOpaqueDepthX);
                    const float neighbourThicknessX = distance(neighbourBottomX, input.worldPosition);
                    const float neighbourVerticalDepthX = input.worldPosition.y - neighbourBottomX.y;
                    if (WaterDepthSampleValidity3D(neighbourVerticalDepthX, neighbourThicknessX) > 0.5)
                        neighbourShoreDepthX = clamp(max(0.0, neighbourVerticalDepthX), 0.0, 8.0);
                }
                if (neighbourOpaqueDepthY > 0.0 && neighbourOpaqueDepthY < 0.999999)
                {
                    const precise float3 neighbourBottomY = WaterReconstructWorldPositionDeterministic3D(neighbourUvY, neighbourOpaqueDepthY);
                    const float neighbourThicknessY = distance(neighbourBottomY, input.worldPosition);
                    const float neighbourVerticalDepthY = input.worldPosition.y - neighbourBottomY.y;
                    if (WaterDepthSampleValidity3D(neighbourVerticalDepthY, neighbourThicknessY) > 0.5)
                        neighbourShoreDepthY = clamp(max(0.0, neighbourVerticalDepthY), 0.0, 8.0);
                }
                const float shoreFiniteDifference = max(abs(neighbourShoreDepthX - shoreDepth), abs(neighbourShoreDepthY - shoreDepth));
                const float shoreDerivative = clamp(shoreFiniteDifference * 1.5, 0.002, shoreDerivativeMaximum);
                const float shore = waterSurfaceEnabled != 0u
                    ? 1.0 - smoothstep(waterFoamStartDepth - shoreDerivative, waterFoamEndDepth + shoreDerivative, shoreDepth)
                    : 1.0 - smoothstep(waterFoamStartDepth, waterFoamEndDepth, waterThickness);
#if AEVRYND_WATER_AERIAL_DIAGNOSTIC >= 10
                waterDiagnosticShore = shore;
                waterDiagnosticDerivative = shoreDerivative / max(0.002, shoreDerivativeMaximum);
#endif
                if(waterSurfaceEnabled != 0u)
                {
                    waterFoamCoverage = WaterFoamCoverage3D(input.worldPosition.xz, fogEndEnabled.w);
                    // 乘上 [0.62,1] 雜訊只會改變連續帶的明暗；對連續場設門檻，
                    // 讓淺岸在世界空間形成真正缺口，同時保持深度方向單調。
                    const float foamPresence = smoothstep(0.36, 0.70,
                                                          waterFoamCoverage + shore * 0.16);
                    foamMask = saturate(shore * foamPresence);
                }
                else
                {
                    const float shoreNoise = lerp(0.62, 1.0, CloudNoise(input.worldPosition.xz * 0.58 + fogEndEnabled.ww * float2(0.035, -0.021)));
                    foamMask = saturate(shore * shoreNoise);
                }
                waterOutputAlpha=lerp(waterShallowAlpha,waterDeepAlpha,smoothstep(waterSurfaceEnabled!=0u?waterFoamStartDepth:.04,4.5,waterSurfaceEnabled!=0u?shoreDepth:waterThickness));
            }
        }
    }
    [branch] if (waterMask > 0.5)
    {
        if(waterSurfaceEnabled==0u)
        {
            // disabled必須逐常數重播b5f9f59，不讓新設定的packed值影響legacy輸出。
            const float2 broadRipple=float2(sin(input.worldPosition.z*.38+input.worldPosition.x*.11+fogEndEnabled.w*.72),cos(input.worldPosition.x*.31-input.worldPosition.z*.16-fogEndEnabled.w*.61));
            const float2 detailRipple=float2(sin(input.worldPosition.z*1.73-input.worldPosition.x*.64+fogEndEnabled.w*1.91),cos(input.worldPosition.x*1.42+input.worldPosition.z*.57-fogEndEnabled.w*1.67));
            const float2 waterRipple=broadRipple*.68+detailRipple*.32;
            normal=normalize(normal+float3(waterRipple.x,0.0,waterRipple.y)*lerp(.085,.035,foamMask));
        }
        else
        {
            // 場景河道的主軸約為 +Z；以固定正交 frame 組合順流／橫向波，
            // 每一頻譜使用不同相位原點，避免四組重播相同相位格。
            const precise float2 riverTangent = float2(0.28, 0.96);
            const precise float2 riverAcross = float2(-0.96, 0.28);
            const precise float2 crossBroadDirection = riverTangent * 0.342020143
                                                      + riverAcross * 0.939692621;
            const precise float2 macroDirection = riverTangent * 0.906307787
                                                 + riverAcross * 0.422618262;
            const precise float2 crossMacroDirection = riverTangent * 0.573576436
                                                      - riverAcross * 0.819152044;
            const precise float broadResolvedWeight = WaterWaveDirectionalResolvedWeight3D(
                waterWorldDeltaX, waterWorldDeltaY,
                waterInterpolatedDeltaX, waterInterpolatedDeltaY,
                waterBroadWaveScale, riverTangent);
            const precise float crossResolvedWeight = WaterWaveDirectionalResolvedWeight3D(
                waterWorldDeltaX, waterWorldDeltaY,
                waterInterpolatedDeltaX, waterInterpolatedDeltaY,
                waterBroadWaveScale * 1.31, crossBroadDirection);
            const precise float pairedBroadResolvedWeight = min(broadResolvedWeight, crossResolvedWeight);
            const precise float broadDiversityScale = broadResolvedWeight > 0.0001
                ? pairedBroadResolvedWeight / broadResolvedWeight : 0.0;
            const precise float crossDiversityScale = crossResolvedWeight > 0.0001
                ? pairedBroadResolvedWeight / crossResolvedWeight : 0.0;
            precise float3 broadWave = WaterAnalyticDirectionalWave3D(
                input.worldPosition.xz, fogEndEnabled.w, waterBroadWaveScale,
                waterBroadWaveStrength, waterBroadWaveSpeed, riverTangent, 0.31,
                waterWorldDeltaX, waterWorldDeltaY,
                waterInterpolatedDeltaX, waterInterpolatedDeltaY);
            // 次要頻譜不可只在主流方向附近擺動，否則多組解析波仍會疊成
            // 橫跨河寬的規則平行亮帶。保留順流長波作為主要輪廓，其餘三組
            // 分散到 +70、-45 與 +85 度，讓低頻分量逐級退場時仍保有兩個
            // 足夠分離的方向，形成有方向但不呈瓦楞狀的河面。
            precise float3 crossWave = WaterAnalyticDirectionalWave3D(
                input.worldPosition.xz, fogEndEnabled.w, waterBroadWaveScale * 1.31,
                waterBroadWaveStrength * 0.85, waterBroadWaveSpeed * 0.73,
                crossBroadDirection, 2.03,
                waterWorldDeltaX, waterWorldDeltaY,
                waterInterpolatedDeltaX, waterInterpolatedDeltaY);
            // 暮色斜視鏡頭每像素跨越一公尺以上；即使寬波逐方向 band-limit，
            // 0.14 cycle/m 的主要頻譜也只剩很小斜率。兩組 15~40m 波長的
            // 河面 swell 提供仍可解析的大尺度法線，不使用螢幕雜訊或復活
            // 已過 Nyquist 的 detail 頻率。
            const precise float macroWaveScale = waterBroadWaveScale * 0.36;
            const precise float crossMacroWaveScale = macroWaveScale * 1.23;
            const precise float macroResolvedWeight = WaterWaveDirectionalResolvedWeight3D(
                waterWorldDeltaX, waterWorldDeltaY,
                waterInterpolatedDeltaX, waterInterpolatedDeltaY,
                macroWaveScale, macroDirection);
            const precise float crossMacroResolvedWeight = WaterWaveDirectionalResolvedWeight3D(
                waterWorldDeltaX, waterWorldDeltaY,
                waterInterpolatedDeltaX, waterInterpolatedDeltaY,
                crossMacroWaveScale, crossMacroDirection);
            const precise float pairedMacroResolvedWeight = min(macroResolvedWeight, crossMacroResolvedWeight);
            const precise float macroDiversityScale = macroResolvedWeight > 0.0001
                ? pairedMacroResolvedWeight / macroResolvedWeight : 0.0;
            const precise float crossMacroDiversityScale = crossMacroResolvedWeight > 0.0001
                ? pairedMacroResolvedWeight / crossMacroResolvedWeight : 0.0;
            precise float3 macroWave = WaterAnalyticDirectionalWave3D(
                input.worldPosition.xz, fogEndEnabled.w, macroWaveScale,
                waterBroadWaveStrength, waterBroadWaveSpeed * 0.55,
                macroDirection, 1.13,
                waterWorldDeltaX, waterWorldDeltaY,
                waterInterpolatedDeltaX, waterInterpolatedDeltaY);
            precise float3 crossMacroWave = WaterAnalyticDirectionalWave3D(
                input.worldPosition.xz, fogEndEnabled.w, crossMacroWaveScale,
                waterBroadWaveStrength * 0.82, waterBroadWaveSpeed * 0.41,
                crossMacroDirection, 5.17,
                waterWorldDeltaX, waterWorldDeltaY,
                waterInterpolatedDeltaX, waterInterpolatedDeltaY);
            // 每組內部四個頻率仍獨立退場；此處只平衡兩組尚可解析的總能量，
            // 並同時縮放高度與偏導，避免中間 LOD 只剩單一方向形成高光條帶。
            broadWave = broadWave * broadDiversityScale;
            crossWave = crossWave * crossDiversityScale;
            macroWave = macroWave * macroDiversityScale;
            crossMacroWave = crossMacroWave * crossMacroDiversityScale;
            const precise float detailWarpDistance = WaterDomainWarpDistance3D(waterBroadWaveScale, waterDetailWaveScale);
            const precise float crossDetailWarpDistance = WaterDomainWarpDistance3D(waterBroadWaveScale * 1.31, waterDetailWaveScale * 0.71);
            const precise float2 detailPosition = WaterDomainWarpPosition3D(input.worldPosition.xz, riverAcross, broadWave, waterBroadWaveStrength, detailWarpDistance);
            const precise float2 crossDetailPosition = WaterDomainWarpPosition3D(input.worldPosition.xz, riverAcross, crossWave, waterBroadWaveStrength * 0.85, crossDetailWarpDistance);
            precise float3 detailWave = WaterAnalyticWave3D(detailPosition, fogEndEnabled.w, waterDetailWaveScale, waterDetailWaveStrength, waterDetailWaveSpeed, riverTangent * 0.707106781 - riverAcross * 0.707106781, 4.79, waterWorldFootprint);
            precise float3 crossDetailWave = WaterAnalyticWave3D(crossDetailPosition, fogEndEnabled.w, waterDetailWaveScale * 0.71, waterDetailWaveStrength * 0.58, waterDetailWaveSpeed * 1.27, riverTangent * 0.087155743 + riverAcross * 0.996194698, 7.31, waterWorldFootprint);
            detailWave.yz = WaterDomainWarpGradient3D(detailWave.yz, riverAcross, broadWave, waterBroadWaveStrength, detailWarpDistance);
            crossDetailWave.yz = WaterDomainWarpGradient3D(crossDetailWave.yz, riverAcross, crossWave, waterBroadWaveStrength * 0.85, crossDetailWarpDistance);
            // 僅用加權頻譜保留量估算 unresolved roughness；正式高度與偏導已在
            // WaterAnalyticWave3D 內逐分量 band-limit，不再用最高頻率清零整組波。
            const precise float detailResolvedWeight = WaterWaveResolvedWeight3D(waterWorldFootprint, waterDetailWaveScale);
            const precise float crossDetailResolvedWeight = WaterWaveResolvedWeight3D(waterWorldFootprint, waterDetailWaveScale * 0.71);
            const precise float pairedDetailResolvedWeight = min(detailResolvedWeight, crossDetailResolvedWeight);
            const precise float detailDiversityScale = detailResolvedWeight > 0.0001
                ? pairedDetailResolvedWeight / detailResolvedWeight : 0.0;
            const precise float crossDetailDiversityScale = crossDetailResolvedWeight > 0.0001
                ? pairedDetailResolvedWeight / crossDetailResolvedWeight : 0.0;
            detailWave = detailWave * detailDiversityScale;
            crossDetailWave = crossDetailWave * crossDetailDiversityScale;
            // 四組各含四個非共線分量的解析高度場分散主要方向。每組偏導
            // 與高度共用相位，遠景亦各自依像素世界足跡衰減；不採跨程序
            // 可能落在不同半精度邊界的格點雜訊梯度。
            // 遠景會先濾除兩組細波，因此兩組寬波必須保留相近能量；否則
            // 只剩單一順流正弦主宰高光，即使近景的總方向統計已分散仍會出現條帶。
            precise float2 heightGradient = broadWave.yz * 1.48;
            heightGradient = heightGradient + crossWave.yz * 1.48;
            heightGradient = heightGradient + macroWave.yz * 3.48;
            heightGradient = heightGradient + crossMacroWave.yz * 3.48;
            heightGradient = heightGradient + detailWave.yz * 0.22;
            heightGradient = heightGradient + crossDetailWave.yz * 0.24;
            // Material3D already carries the authored normal amplitude. The water path
            // previously ignored it, so distant river pixels used only half of the
            // intended analytic slope and collapsed into a flat translucent sheet.
            // Scale the real height derivative after per-band filtering; this keeps
            // sub-pixel waves suppressed and does not introduce screen-space noise.
            const precise float waterNormalScale = clamp(input.normalScale, 0.0, 2.0);
            heightGradient = heightGradient * waterNormalScale;
            waterLowFrequencyGradient = ((broadWave.yz + crossWave.yz) * 1.48
                                      + (macroWave.yz + crossMacroWave.yz) * 3.48)
                                      * waterNormalScale;
            const precise float lowFrequencySlope = length(waterLowFrequencyGradient);
            waterLowFrequencyGradient = waterLowFrequencyGradient
                                      * min(1.0, 0.48 / max(lowFrequencySlope, 0.0001));
            const precise float unboundedSlope = length(heightGradient);
            heightGradient = heightGradient * min(1.0, 0.48 / max(unboundedSlope, 0.0001));
            waterResolvedSlope = min(unboundedSlope, 0.48);
            // 解析度粗糙度能量必須包含新增的 macro swell。舊式重複 broad、
            // 完全漏掉 macro，讓已清楚出現在法線中的大波仍被當成未解析微面，
            // 在反射查詢中再次模糊。三個頻帶的歸一化權重總和固定為 1。
            waterUnresolvedDetail = saturate(1.0 - (
                pairedBroadResolvedWeight * 0.54
              + pairedMacroResolvedWeight * 0.36
              + pairedDetailResolvedWeight * 0.10));
#if AEVRYND_WATER_AERIAL_DIAGNOSTIC == 19
            waterDiagnosticHeightGradient = heightGradient;
#endif
            const precise float waveBlend = 1.0 + (0.45 - 1.0) * foamMask;
            precise float3 perturbedNormal = normal;
            perturbedNormal.x = perturbedNormal.x - heightGradient.x * waveBlend;
            perturbedNormal.z = perturbedNormal.z - heightGradient.y * waveBlend;
            normal = WaterDeterministicNormalize3D(perturbedNormal);
        }
    }

    float skinCurvature=0.0;
    // 材質法線完成後才估算螢幕空間曲率；Skin 為 nointerpolation 旗標，分支在三角形內一致。
    [branch] if(skinMask>0.5&&skinSubsurfaceStrength>0.0)skinCurvature=length(ddx(normal))+length(ddy(normal));
    const float3 lightDirection = -normalize(lightDirectionAmbient.xyz);
    const float3 viewDirection = waterMaterialMask > 0.5
        ? WaterSurfaceViewDirection(input.worldPosition, cameraFogStart.xyz, viewProjection)
        : normalize(cameraFogStart.xyz - input.worldPosition);
    [branch] if (foliageMask > 0.5)
        normal=FoliageTwoSidedShadingNormal(normal,viewDirection,isFrontFace);
    roughness = lerp(roughness, max(roughness, 0.72), foliageMask * pow(1.0 - saturate(dot(normal, viewDirection)), 2.0));
    // SSAO 僅調制間接環境光，避免把太陽、點光、自發光與水面反射一起壓暗。
    const float2 screenOcclusion = colorPipeline.y > 0.5 && waterMask < 0.5
                                ? ambientOcclusionTexture.SampleLevel(materialSampler, input.position.xy * colorPipeline.zw, 0.0)
                                : 1.0;
    const float indirectOcclusion=FoliageIndirectOcclusion(occlusion*screenOcclusion.x,foliageMask);
    const float3 environmentLighting = eyeMask > 0.5
        ? EvaluateEyeEnvironment(input.worldPosition,normal, geometricNormal, viewDirection, baseColor, roughness,
                                 indirectOcclusion, lightDirectionAmbient.w, skyLightDirection)
        : EvaluateEnvironmentLighting(input.worldPosition,normal, viewDirection, baseColor, roughness, metallic,
                                      indirectOcclusion, lightDirectionAmbient.w, skyLightDirection);
    const float3 directionalLighting = eyeMask > 0.5
        ? EvaluateEyeLight(normal, geometricNormal, viewDirection, lightDirection,
                           lightColorIntensity.rgb * lightColorIntensity.w, baseColor, roughness)
        : EvaluatePbrLight(normal, viewDirection, lightDirection,
                           lightColorIntensity.rgb * lightColorIntensity.w, baseColor, roughness, metallic);
    // 基礎 PBR 與角色補光共用同一次級聯陰影，避免 Skin/Hair 每像素重複取樣陰影圖。
    const float directionalShadow=CascadedDirectionalShadow(input,eyeMask>0.5?geometricNormal:normal,lightDirection);
    float cloudShadow=1.0;
    [branch] if(volumetricCloudEnabled>=0.5&&cloudShadowEnabled>=0.5&&cloudShadowStrength>0.0&&cloudDensityScale>0.0&&lightDirection.y>0.02&&input.worldPosition.y<cloudBaseAltitude+cloudThickness*0.5)
        cloudShadow=EvaluateCloudShadowTransmittance(input.worldPosition,lightDirection);
    const float directionalVisibility=directionalShadow*cloudShadow*screenOcclusion.y;
    float3 shaded = environmentLighting
                  + directionalVisibility * directionalLighting
                  + baseColor * emissive * 2.0;
    [branch] if(skinMask > 0.5){
        shaded+=directionalVisibility*EvaluateSkinSupplement(normal,viewDirection,lightDirection,
                    lightColorIntensity.rgb*lightColorIntensity.w,baseColor,roughness,skinCurvature);
    }
    [branch] if(hairMask>0.5){
        shaded+=directionalVisibility*EvaluateHairSupplement(normal,input.tangent.xyz,viewDirection,lightDirection,
                    lightColorIntensity.rgb*lightColorIntensity.w,baseColor,roughness);
    }
    [branch] if(clothMask>0.5){
        shaded+=directionalVisibility*EvaluateClothSupplement(normal,viewDirection,lightDirection,
                    lightColorIntensity.rgb*lightColorIntensity.w,baseColor,roughness);
    }
    [branch] if (foliageMask > 0.5)
    {
        const float foliageWrappedDiffuse = saturate((dot(normal, lightDirection) + 0.35) / 1.35);
        const float foliagePhysicalTransmission = FoliagePhysicalTransmission(dot(normal, lightDirection), dot(viewDirection, lightDirection));
        const float foliageTransmissionTerm = foliagePhysicalTransmission * 0.48;
        const float3 foliageTransmissionColor = FoliageNeutralizedTransmissionColor(baseColor, foliagePhysicalTransmission);
        const float foliageShadow = CascadedDirectionalShadow(input, normal, lightDirection);
        shaded += lightColorIntensity.rgb * lightColorIntensity.w * (baseColor * foliageWrappedDiffuse * 0.22 + foliageTransmissionColor * foliageTransmissionTerm) * foliageShadow*cloudShadow;
    }

    // 透明與水面保守遍歷全部 canonical lights；不透明 tile 溢位時也回退全部燈，絕不截斷。
    const uint canonicalPointLightCount=(uint)fogEndEnabled.z;
    uint pointLightStart=0u;
    uint pointLightIterationCount=canonicalPointLightCount;
    bool useForwardPlusTile=forwardPlusParameters.x>0.5&&waterMaterialMask<0.5&&input.alphaMode!=2u;
    if(useForwardPlusTile)
    {
        const uint2 localPixel=uint2(max(input.position.xy-forwardPlusParameters.zw,0.0));
        const uint2 tile=localPixel/16u;
        const uint tileIndex=tile.y*(uint)forwardPlusParameters.y+tile.x;
        const uint4 entry=forwardPlusTileGrid[tileIndex];
        pointLightStart=entry.x;
        pointLightIterationCount=entry.z!=0u?canonicalPointLightCount:entry.y;
    }
    [loop]
    for (uint pointIteration = 0; pointIteration < pointLightIterationCount; ++pointIteration)
    {
        const uint pointIndex=useForwardPlusTile&&pointLightIterationCount!=canonicalPointLightCount?forwardPlusTileIndices[pointLightStart+pointIteration]:pointIteration;
        const PointLightGpu3D pointLight=forwardPlusPointLights[pointIndex];
        const float3 delta = pointLight.position - input.worldPosition;
        const float distanceSquared = max(0.0001, dot(delta, delta));
        const float lightDistance = sqrt(distanceSquared);
        const float attenuation = PointLightAttenuation(distanceSquared, pointLight.radius);
        const float3 localDirection = delta / lightDistance;
        const float3 pointRadiance = pointLight.color * pointLight.intensity * attenuation;
        shaded += eyeMask > 0.5
                ? EvaluateEyeLight(normal, geometricNormal, viewDirection, localDirection, pointRadiance, baseColor, roughness)
                : EvaluatePbrLight(normal, viewDirection, localDirection, pointRadiance, baseColor, roughness, metallic);
        if(skinMask>0.5)shaded+=EvaluateSkinSupplement(normal,viewDirection,localDirection,
                    pointLight.color*pointLight.intensity*attenuation,
                    baseColor,roughness,skinCurvature);
        if(hairMask>0.5)shaded+=EvaluateHairSupplement(normal,input.tangent.xyz,viewDirection,localDirection,
                    pointLight.color*pointLight.intensity*attenuation,
                    baseColor,roughness);
        if(clothMask>0.5)shaded+=EvaluateClothSupplement(normal,viewDirection,localDirection,
                    pointLight.color*pointLight.intensity*attenuation,
                    baseColor,roughness);
    }

    float3 aerialOwnedSceneContribution = 0.0;
#if AEVRYND_WATER_AERIAL_DIAGNOSTIC >= 6
    // 分量暫存僅存在於診斷變體，不改正式水體數學。
    float3 waterDiagnosticAbsorbed = 0.0;
    float3 waterDiagnosticReflected = 0.0;
    float3 waterDiagnosticTransmission = 0.0;
    float waterDiagnosticFoam = 0.0;
    float3 waterDiagnosticEnvironmentReflection = 0.0;
    float4 waterDiagnosticScreenSpaceReflection = 0.0;
#endif
    [branch] if (waterCompositeMask > 0.5)
    {
        const float waterDepthSlice = waterSurfaceEnabled != 0u
            ? WaterDepthSlice3D(shoreDepth, waterDepthValid)
            : 1.0;
        float3 waterReflectionNormal = normal;
        if(waterSurfaceEnabled != 0u && waterMask > 0.5)
        {
            precise float3 lowFrequencyNormal = waterReflectionBaseNormal;
            lowFrequencyNormal.x = lowFrequencyNormal.x - waterLowFrequencyGradient.x;
            lowFrequencyNormal.z = lowFrequencyNormal.z - waterLowFrequencyGradient.y;
            lowFrequencyNormal = WaterDeterministicNormalize3D(lowFrequencyNormal);
            // 深水天空輪廓主要由可解析寬波驅動；近岸逐步回到完整法線並由
            // 同一 depth slice 提高粗糙度，避免 Fresnel 與天空查詢使用不同相位。
            const float lowFrequencyWeight = waterDepthSlice * (1.0 - foamMask) * 0.72;
            waterReflectionNormal = WaterDeterministicNormalize3D(
                lerp(normal, lowFrequencyNormal, lowFrequencyWeight));
        }
        const precise float waterViewCosine = saturate(dot(waterReflectionNormal, viewDirection));
        // 固定 Schlick Fresnel 的乘法順序。pow(x, 5) 可由驅動程式改寫成
        // 不同的近似序列，極小差異會在反射與半精度 HDR 封存時跨越位元邊界。
        const precise float fresnelBase = 1.0 - waterViewCosine;
        const precise float fresnelBaseSquared = fresnelBase * fresnelBase;
        const precise float fresnelBaseFourth = fresnelBaseSquared * fresnelBaseSquared;
        const precise float fresnelBaseFifth = fresnelBaseFourth * fresnelBase;
        precise float fresnel = (1.0 - waterReflectance) * fresnelBaseFifth;
        fresnel = waterReflectance + fresnel;
        const float3 legacyAbsorption=float3(.42,.16,.08);
        const float3 legacyDeepColor=float3(.012,.055,.070);
        float3 transmittedScene = shaded;
        [branch] if (waterSceneColorEnabled > 0.5 && (waterDepthValid > 0.5 || waterSurfaceEnabled != 0u))
        {
            // 折射位移受水深與最大 UV 偏移雙重限制，避免岸邊越界及遠景拉伸。
            const float refractionScale = waterDepthValid > 0.5 ? min(waterMaximumUvOffset,
                waterRefractionStrength * saturate(opticalPathLength * waterRefractionDepthScale)) : 0.0;
            const float2 viewportMinimum = worldViewport.xy + colorPipeline.zw * 1.5;
            const float2 viewportMaximum = worldViewport.xy + worldViewport.zw - colorPipeline.zw * 1.5;
            float2 refractedUv = waterScreenUv;
            if(refractionScale > 0.0)
            {
                const float2 candidateUv = clamp(waterScreenUv + normal.xz * refractionScale,
                                                  viewportMinimum, viewportMaximum);
                const float candidateDepth = sceneDepthTexture.SampleLevel(screenPointClampSampler, candidateUv, 0.0);
                if(candidateDepth > 0.0 && candidateDepth < 0.999999)
                {
                    const precise float3 candidateBottom = WaterReconstructWorldPositionDeterministic3D(candidateUv, candidateDepth);
                    const float candidateThickness = distance(candidateBottom, input.worldPosition);
                    const float candidateVerticalDepth = input.worldPosition.y - candidateBottom.y;
                    // 折射場景色與 Beer-Lambert 厚度必須指向同一個水下點；
                    // 天空、前景及岸面樣本直接拒絕，不能把它們吸收成虛構深水。
                    if(candidateThickness >= 0.0 && candidateThickness < 100000.0 && candidateVerticalDepth >= -0.02)
                    {
                        refractedUv = candidateUv;
                        opticalPathLength = clamp(candidateThickness, 0.0, 8.0);
                    }
                }
            }
            transmittedScene = opaqueSceneColorTexture.SampleLevel(screenPointClampSampler, refractedUv, 0.0).rgb;
            aerialOwnedSceneContribution = transmittedScene;
        }
        const float waterPathLength = waterSurfaceEnabled != 0u
            ? min(8.0, opticalPathLength)
            : min(8.0, waterThickness / max(0.16, waterViewCosine));
        const float3 waterTransmission=exp(-(waterSurfaceEnabled!=0u?waterAbsorption:legacyAbsorption)*waterPathLength);
        // 深水色是懸浮微粒的散射，不是自發光。用天空漫射與可見直射光
        // 驅動體積源項，夜間不再維持與白天相同的青色亮度。
        const float3 waterVolumeIlluminance = max(float3(0.0,0.0,0.0),
            EvaluateEnvironmentDiffuse(normal, skyLightDirection)
            + lightColorIntensity.rgb * lightColorIntensity.w
              * saturate(dot(normal, lightDirection)) * directionalVisibility);
        const float3 deepWaterColor = waterSurfaceEnabled != 0u
            ? WaterLitScatter3D(waterDeepColor, baseColor, waterVolumeIlluminance)
            : lerp(legacyDeepColor, baseColor, .24);
        precise float3 inverseWaterTransmission;
        inverseWaterTransmission.r = 1.0 - waterTransmission.r;
        inverseWaterTransmission.g = 1.0 - waterTransmission.g;
        inverseWaterTransmission.b = 1.0 - waterTransmission.b;
        precise const float3 transmittedWater = WaterOrderedMultiply3D(transmittedScene, waterTransmission);
        precise const float3 scatteredWater = WaterOrderedMultiply3D(deepWaterColor, inverseWaterTransmission);
        precise const float3 absorbedWater = WaterOrderedScaleAdd3D(transmittedWater, 1.0, scatteredWater, 1.0);
        const float waterReflectionRoughness = waterSurfaceEnabled != 0u
            ? WaterReflectionRoughness3D(waterMinimumRoughness, waterMaximumRoughness,
                                         waterResolvedSlope, waterUnresolvedDetail, foamMask,
                                         waterDepthSlice)
            : clamp(roughness, waterMinimumRoughness, waterMaximumRoughness);
        float3 reflectedWater = EvaluateEnvironmentSpecular(input.worldPosition,reflect(-viewDirection, waterReflectionNormal), skyLightDirection, waterReflectionRoughness)
                              * lerp(1.0, 0.55, waterReflectionRoughness);
        // 透明水面會以不透明場景取代先前的 shaded 值，因此基礎
        // directionalLighting 內的太陽高光不能沿用。以金屬路徑的零漫射
        // GGX 明確重建水的 F0 高光；EvaluatePbrLight 已計算 Fresnel，後續
        // 不可再乘一次 water fresnel。
        const float3 waterDirectionalSpecular = directionalVisibility * EvaluatePbrLight(
            waterReflectionNormal, viewDirection, lightDirection,
            lightColorIntensity.rgb * lightColorIntensity.w,
            waterReflectance.xxx, waterReflectionRoughness, 1.0);
        const float4 screenSpaceReflection=EvaluateWaterScreenSpaceReflection(input.worldPosition,waterReflectionNormal,viewDirection,waterReflectionRoughness);
#if AEVRYND_WATER_AERIAL_DIAGNOSTIC >= 14
        waterDiagnosticEnvironmentReflection = reflectedWater * fresnel;
        waterDiagnosticScreenSpaceReflection = screenSpaceReflection;
#endif
        reflectedWater=WaterReflectionFallback3D(reflectedWater,screenSpaceReflection.rgb,screenSpaceReflection.a);
        // 反射只取得 Fresnel 能量，其餘能量保留給透射，避免日夜切換時雙重增亮。
        if(waterSurfaceEnabled!=0u)
        {
            // 低光 phase 之後仍會經 time-of-day exposure；四倍天空亮度上限會把
            // 黃昏／夜間掠射角反射重新抬成均勻亮帶。保留淡水 Fresnel 與波面
            // 起伏，只把環境反射限制在可讀、非自發光的天空能量範圍。以峰值
            // 等比例縮放而不是逐通道 min，避免把高光裁成失去色彩與波紋的平帶。
            const precise float waterReflectionKnee=max(waterMaximumNightRadiance,dot(aerialSkyRadianceSunScale.rgb,float3(.2126,.7152,.0722)))*1.5;
            const precise float waterReflectionMaximum=waterReflectionKnee*1.65;
            const precise float waterReflectionPeak=max(reflectedWater.r,max(reflectedWater.g,reflectedWater.b));
            // C1 連續的有界 soft-knee：門檻以下 exact no-op；門檻以上仍單調
            // 保留亮暗差，漸近上限為 1.65 倍天空能量。用 excess/(excess+headroom)
            // 使 knee 右導數保持 1；舊 K/peak 形式在 knee 右側只剩 0.1，會把
            // 黃昏波面剛形成的反射對比立即壓平。
            const precise float waterReflectionHeadroom=max(
                waterReflectionMaximum-waterReflectionKnee,0.000001);
            const precise float waterReflectionExcess=max(
                0.0,waterReflectionPeak-waterReflectionKnee);
            const precise float waterReflectionCompressedPeak=waterReflectionPeak<=waterReflectionKnee
                ? waterReflectionPeak
                : waterReflectionKnee+waterReflectionHeadroom*waterReflectionExcess
                  /(waterReflectionExcess+waterReflectionHeadroom);
            const precise float waterReflectionScale=min(1.0,waterReflectionCompressedPeak/max(waterReflectionPeak,0.000001));
            reflectedWater=WaterOrderedMultiply3D(reflectedWater,waterReflectionScale.xxx);
        }
        precise const float inverseFresnel = 1.0 - fresnel;
        precise float3 waterComposite=waterSurfaceEnabled!=0u
            ? WaterOrderedScaleAdd3D(absorbedWater, inverseFresnel, reflectedWater, fresnel)
            : lerp(absorbedWater,reflectedWater,fresnel);
        if(waterSurfaceEnabled!=0u)
            waterComposite=WaterOrderedScaleAdd3D(waterComposite,1.0,waterDirectionalSpecular,1.0);
        aerialOwnedSceneContribution = WaterOrderedMultiply3D(aerialOwnedSceneContribution, waterTransmission);
        aerialOwnedSceneContribution = WaterOrderedScaleAdd3D(aerialOwnedSceneContribution, inverseFresnel,
                                                                float3(0.0,0.0,0.0), 0.0);
#if AEVRYND_WATER_AERIAL_DIAGNOSTIC >= 6
        waterDiagnosticAbsorbed = absorbedWater * (1.0 - fresnel);
        waterDiagnosticReflected = reflectedWater * fresnel;
        waterDiagnosticTransmission = waterTransmission;
#endif
        // 單次合成輸出必須保持 alpha=1，但 authored shallow/deep alpha 仍應
        // 在 production contour mesh 的過渡帶控制 RGB 覆蓋曲線；深水內部
        // 回到完整水材質，避免 deepAlpha 令整條河回滲明亮背景。側壁維持舊值。
        const float waterLocalContribution = waterSurfaceEnabled != 0u && waterMask > 0.5
            ? saturate(waterBoundaryWeight * lerp(waterOutputAlpha, 1.0, waterBoundaryWeight))
            : lerp(0.35, 1.0, waterMask);
        const float3 sideTransmittedScene = waterSceneColorEnabled > 0.5
            ? opaqueSceneColorTexture.SampleLevel(screenPointClampSampler, waterScreenUv, 0.0).rgb
            : transmittedScene;
        const float3 sideAerialOwnedScene = waterSceneColorEnabled > 0.5 ? sideTransmittedScene : aerialOwnedSceneContribution;
        waterComposite = waterSurfaceEnabled != 0u ? WaterOrderedLerp3D(sideTransmittedScene, waterComposite, waterLocalContribution) : waterComposite;
        aerialOwnedSceneContribution = waterSurfaceEnabled != 0u
            ? WaterOrderedLerp3D(sideAerialOwnedScene, aerialOwnedSceneContribution, waterLocalContribution)
            : aerialOwnedSceneContribution;
        [branch] if (foamMask > 0.01)
        {
            // 岸線泡沫使用有界程序破碎圖樣，避免整條河岸成為均勻發光帶。
            float foamCoverage = waterFoamCoverage;
            if(waterSurfaceEnabled == 0u)
            {
                const float2 foamFlow = input.worldPosition.xz * 0.42 + float2(fogEndEnabled.w * 0.07, -fogEndEnabled.w * 0.04);
                const float foamNoise = CloudNoise(foamFlow);
                const float foamWave = 0.5 + 0.5 * sin(input.worldPosition.z * 1.35 + input.worldPosition.x * 0.31 + fogEndEnabled.w * 1.8);
                foamCoverage = smoothstep(0.34, 0.76, foamNoise * 0.72 + foamWave * 0.28);
            }
            const float3 foamColor = baseColor * lerp(0.58, 0.96, foamCoverage)
                                   + EvaluateEnvironmentDiffuse(normal, skyLightDirection) * 0.055;
            const float foamBlend=foamMask*waterFoamStrength*lerp(.42,waterSurfaceEnabled!=0u?1.0:.88,foamCoverage)*(waterSurfaceEnabled!=0u?waterLocalContribution:1.0);
#if AEVRYND_WATER_AERIAL_DIAGNOSTIC >= 6
            waterDiagnosticFoam = foamBlend;
#endif
            waterComposite=WaterOrderedLerp3D(waterComposite,foamColor,foamBlend);
            aerialOwnedSceneContribution=WaterOrderedScaleAdd3D(aerialOwnedSceneContribution,1.0-foamBlend,
                                                                 float3(0.0,0.0,0.0),0.0);
        }
        shaded = waterComposite;
    }
    const float surfaceWeight = saturate(input.normal.w);
    const float3 lit = lerp(baseColor, shaded, surfaceWeight);
    const bool singleCompositeWater = screenSpaceReflectionReserved.x > 0.5 && waterMaterialMask > 0.5;
    if(singleCompositeWater)
        aerialOwnedSceneContribution=WaterOrderedScaleAdd3D(aerialOwnedSceneContribution,surfaceWeight,
                                                             float3(0.0,0.0,0.0),0.0);
    else
        aerialOwnedSceneContribution *= surfaceWeight;
    float aerialTransmittance = 1.0;
    float3 aerialInscattering = 0.0;
    [branch] if (fogEndEnabled.y > 0.5 && aerialExtinctionScatteringHeight.x > 0.0 && input.normal.w > 0.0)
    {
        const float3 cameraRay = input.worldPosition - cameraFogStart.xyz;
        const float cameraDistance = length(cameraRay);
        const float availableDistance = max(0.0, min(cameraDistance, aerialDistancePhase.z) -
                                                  aerialDistancePhase.x);
        const float onset = smoothstep(0.0, max(0.001, aerialDistancePhase.y), availableDistance);
        const float effectiveDistance = availableDistance * onset;
        [branch] if (effectiveDistance > 0.0 && cameraDistance > 0.0)
        {
            const float3 rayDirection = cameraRay / cameraDistance;
            const float startHeight = cameraFogStart.y + rayDirection.y * aerialDistancePhase.x;
            const float startDensity = exp(clamp(-aerialExtinctionScatteringHeight.z *
                                                  (startHeight - aerialExtinctionScatteringHeight.w), -20.0, 20.0));
            const float heightRate = aerialExtinctionScatteringHeight.z * rayDirection.y;
            float densityIntegral = abs(heightRate) < 0.00001
                ? startDensity * availableDistance
                : startDensity * (1.0 - exp(clamp(-heightRate * availableDistance, -20.0, 20.0))) / heightRate;
            densityIntegral *= onset;
            const float opticalDepth = min(80.0, max(0.0, aerialExtinctionScatteringHeight.x * densityIntegral));
            const float transmittance = exp(-opticalDepth);
            const float scatteredFraction = saturate(aerialExtinctionScatteringHeight.y /
                                                      aerialExtinctionScatteringHeight.x) * (1.0 - transmittance);
            const float viewSunCosine = dot(rayDirection, lightDirection);
            const float anisotropy = clamp(aerialDistancePhase.w, -0.8, 0.85);
            const float phaseDenominator = max(0.0001, 1.0 + anisotropy * anisotropy -
                                                2.0 * anisotropy * viewSunCosine);
            const float phaseRatio = (1.0 - anisotropy * anisotropy) /
                                     (phaseDenominator * sqrt(phaseDenominator));
            const float3 sourceRadiance = aerialSkyRadianceSunScale.rgb +
                lightColorIntensity.rgb * lightColorIntensity.w *
                aerialSkyRadianceSunScale.w * phaseRatio;
            aerialTransmittance = lerp(1.0, transmittance, surfaceWeight);
            aerialInscattering = max(0.0, sourceRadiance) * scatteredFraction * surfaceWeight;
        }
    }
    // 世界著色器只輸出未曝光線性 HDR；曝光、ACES 與 sRGB 由全畫面後處理統一執行。
    float outputAlpha = input.materialClass == 3 ? 1.0 : lerp(input.color.a, waterOutputAlpha, waterMask * waterDepthValid);
    if(singleCompositeWater)
        outputAlpha = 1.0;
    if(alphaToCoverage && input.alphaMasked != 0)
        outputAlpha = MaskedAlphaToCoverage(materialAlpha, input.alphaCutoff);
    // 光柱只由全螢幕 VolumetricAtmosphere pass 依 scene depth 合成；在每個表面再次積分會形成垂直白柱並雙重加色。
    float3 aerialOutput = lit * aerialTransmittance + aerialInscattering;
#if AEVRYND_WATER_AERIAL_DIAGNOSTIC == 1
    // 負向控制保留舊版整體再次套用 aerial 的輸出。
#elif AEVRYND_WATER_AERIAL_DIAGNOSTIC == 2
    aerialOutput = aerialOwnedSceneContribution;
#elif AEVRYND_WATER_AERIAL_DIAGNOSTIC == 3
    aerialOutput = max(0.0, lit - aerialOwnedSceneContribution);
#elif AEVRYND_WATER_AERIAL_DIAGNOSTIC == 4
    aerialOutput = aerialTransmittance.xxx;
#elif AEVRYND_WATER_AERIAL_DIAGNOSTIC == 5
    aerialOutput = aerialInscattering;
#elif AEVRYND_WATER_AERIAL_DIAGNOSTIC == 6
    aerialOutput = waterDiagnosticAbsorbed;
#elif AEVRYND_WATER_AERIAL_DIAGNOSTIC == 7
    aerialOutput = waterDiagnosticReflected;
#elif AEVRYND_WATER_AERIAL_DIAGNOSTIC == 8
    aerialOutput = waterDiagnosticTransmission;
#elif AEVRYND_WATER_AERIAL_DIAGNOSTIC == 9
    aerialOutput = waterDiagnosticFoam.xxx;
#elif AEVRYND_WATER_AERIAL_DIAGNOSTIC == 10
    aerialOutput = waterDepthValid.xxx;
#elif AEVRYND_WATER_AERIAL_DIAGNOSTIC == 11
    aerialOutput = waterDiagnosticShore.xxx;
#elif AEVRYND_WATER_AERIAL_DIAGNOSTIC == 12
    aerialOutput = waterDiagnosticDerivative.xxx;
#elif AEVRYND_WATER_AERIAL_DIAGNOSTIC == 13
    aerialOutput = waterDiagnosticFoamCoverage.xxx;
#elif AEVRYND_WATER_AERIAL_DIAGNOSTIC == 14
    aerialOutput = waterDiagnosticEnvironmentReflection;
#elif AEVRYND_WATER_AERIAL_DIAGNOSTIC == 15
    aerialOutput = waterDiagnosticScreenSpaceReflection.rgb * waterDiagnosticScreenSpaceReflection.a;
#elif AEVRYND_WATER_AERIAL_DIAGNOSTIC == 16
    aerialOutput = normal * 0.5 + 0.5;
#elif AEVRYND_WATER_AERIAL_DIAGNOSTIC == 17
    // 保留插值後世界座標的小數資訊；若此輸出漂移，分歧發生於波浪與光照之前。
    aerialOutput = frac(abs(input.worldPosition) * float3(0.03125, 0.0625, 0.03125));
#elif AEVRYND_WATER_AERIAL_DIAGNOSTIC == 18
    aerialOutput = geometricNormal * 0.5 + 0.5;
#elif AEVRYND_WATER_AERIAL_DIAGNOSTIC == 19
    aerialOutput = float3(waterDiagnosticHeightGradient * 0.25 + 0.5, 0.5);
#elif AEVRYND_WATER_AERIAL_DIAGNOSTIC == 20
    aerialOutput = foamMask.xxx;
#else
    [branch] if(singleCompositeWater)
    {
        // 單次水體合成取樣的是已完成 aerial 的 HDR；僅豁免該 snapshot 份額，水體本地吸收、反射與泡沫仍套用目前距離的 aerial。
        precise const float3 singleCompositeLit=WaterOrderedLerp3D(baseColor,shaded,surfaceWeight);
        aerialOutput=WaterOrderedScaleAdd3D(singleCompositeLit,aerialTransmittance,aerialInscattering,1.0);
        precise const float recoveredSceneWeight = 1.0 - aerialTransmittance;
        aerialOutput = WaterOrderedScaleAdd3D(aerialOutput,1.0,aerialOwnedSceneContribution,recoveredSceneWeight);
    }
    // 遠山的最終可見亮度包含 aerial in-scattering，因此必須在大氣合成後、
    // tone map 前收斂；否則霧會重新抬亮已壓低的 PBR 結果。只處理夜間遠距 terrain。
    [branch] if (terrainMask > 0.5 && phaseSkyZenith.w < 0.5 && terrainDetailWeight < 1.0)
    {
        const float lowSkyLinear = saturate((0.5 - phaseSkyZenith.w) / 0.22);
        const float lowSkyWeight = lowSkyLinear * lowSkyLinear * (3.0 - 2.0 * lowSkyLinear);
        const float distantNightWeight = saturate((1.0 - terrainDetailWeight) * lowSkyWeight * 4.0);
        const float terrainNightLuma = dot(aerialOutput, float3(0.2126, 0.7152, 0.0722));
        const float3 distantNightLighting = lerp(aerialOutput, terrainNightLuma.xxx, 0.30) * 0.68;
        aerialOutput = lerp(aerialOutput, distantNightLighting, distantNightWeight);
    }
#endif
    evaluatedOutput=float4(max(0.0, aerialOutput), outputAlpha);
    }
    return evaluatedOutput;
}

float4 WorldPixelMain(WorldPixelInput input, bool isFrontFace : SV_IsFrontFace) : SV_TARGET
{
    return EvaluateWorldPixel(input, isFrontFace, false);
}

float4 WorldMaskedPixelMain(WorldPixelInput input, bool isFrontFace : SV_IsFrontFace) : SV_TARGET
{
    return EvaluateWorldPixel(input, isFrontFace, true);
}

float BloomSemanticDeviceDepth(WorldPixelInput input)
{
    // FP16 的 1.0 專屬天空；幾何深度限制在前一個可表示值，避免遠景被誤判為天空。
    const bool sky = 1.0 - saturate(input.normal.w) > 0.5;
    return sky ? 1.0 : min(saturate(input.position.z), 0.99951171875);
}

float4 WorldBloomSemanticPixelMain(WorldPixelInput input, bool isFrontFace : SV_IsFrontFace) : SV_TARGET
{
    float4 output = EvaluateWorldPixel(input, isFrontFace, false);
    output.a = BloomSemanticDeviceDepth(input);
    return output;
}

float4 WorldBloomSemanticMasked1xPixelMain(WorldPixelInput input, bool isFrontFace : SV_IsFrontFace) : SV_TARGET
{
    // 1x MASK 必須走硬裁切；SV_Coverage 僅屬於 MSAA 路徑。
    float4 output = EvaluateWorldPixel(input, isFrontFace, false);
    output.a = BloomSemanticDeviceDepth(input);
    return output;
}

float4 WorldBloomSemanticNonMaskedMutationPixelMain(WorldPixelInput input, bool isFrontFace : SV_IsFrontFace) : SV_TARGET
{
    // 僅供負向 oracle 模擬舊錯誤：一般 semantic PSO 不承擔 MASK 契約。
    input.alphaMasked = 0u;
    float4 output = EvaluateWorldPixel(input, isFrontFace, false);
    output.a = BloomSemanticDeviceDepth(input);
    return output;
}

struct WorldBloomSemanticTransparentOutput
{
    float4 color : SV_TARGET0;
    float4 blend : SV_TARGET1;
};

WorldBloomSemanticTransparentOutput WorldBloomSemanticTransparentPixelMain(
    WorldPixelInput input, bool isFrontFace : SV_IsFrontFace)
{
    const float4 evaluated = EvaluateWorldPixel(input, isFrontFace, false);
    // 完全透明片段不得覆寫既有語意；非零透明度仍由 renderer 的排序決定最近可見表面。
    clip(evaluated.a - 0.0000152587890625);
    WorldBloomSemanticTransparentOutput output;
    output.color = float4(evaluated.rgb, BloomSemanticDeviceDepth(input));
    // 第二來源只承載舊透明度；PSO 以 SRC1_ALPHA 做 RGB 混合，Target0 alpha 才能保留語意深度。
    output.blend = evaluated.aaaa;
    return output;
}

uint BloomSemanticCoverageMask4(float opacity, uint2 pixel)
{
    const uint coveredSamples = (uint)round(saturate(opacity) * 4.0);
    const uint rotation = (pixel.x ^ (pixel.y * 3u)) & 3u;
    if (coveredSamples == 0u) return 0u;
    if (coveredSamples == 1u) return 1u << rotation;
    if (coveredSamples == 2u) return (rotation & 1u) != 0u ? 0xau : 0x5u;
    if (coveredSamples == 3u) return 0xfu ^ (1u << rotation);
    return 0xfu;
}

struct WorldBloomSemanticMaskedOutput
{
    float4 color : SV_TARGET0;
    uint coverage : SV_Coverage;
};

WorldBloomSemanticMaskedOutput WorldBloomSemanticMaskedPixelMain(
    WorldPixelInput input, bool isFrontFace : SV_IsFrontFace)
{
    const float4 evaluated = EvaluateWorldPixel(input, isFrontFace, true);
    WorldBloomSemanticMaskedOutput output;
    output.color = float4(evaluated.rgb, BloomSemanticDeviceDepth(input));
    // 正式管線固定為 4x MSAA；顯式 coverage 取代硬體 A2C，讓 Target0 alpha 可承載深度。
    output.coverage = BloomSemanticCoverageMask4(evaluated.a, uint2(input.position.xy));
    return output;
}

uint ObjectIdPixelMain(WorldPixelInput input) : SV_TARGET
{
    // ID pass 重用正式 LOD 點陣裁切；surface 四個 UNORM bytes 在此承載無損 frame token。
    ClipWorldLod(input);
    ClipOcclusionOpacity(input.occlusionCoordinate,input.occlusionOpacity,input.occlusionSeeds);
    return input.objectToken;
}

uint ObjectIdMaskedPixelMain(WorldPixelInput input) : SV_TARGET
{
    clip(input.color.a*baseColorTexture.Sample(materialSampler,input.uv*input.textureData.y).a-input.alphaCutoff);
    return ObjectIdPixelMain(input);
}

struct ObjectIdColorOutput { float4 color : SV_TARGET0; uint objectToken : SV_TARGET1; };
ObjectIdColorOutput ObjectIdColorPixelMain(WorldPixelInput input, bool isFrontFace : SV_IsFrontFace)
{
    ObjectIdColorOutput output;
    output.color=EvaluateWorldPixel(input,isFrontFace,false);
    output.objectToken=input.objectToken;
    return output;
}
ObjectIdColorOutput ObjectIdMaskedColorPixelMain(WorldPixelInput input, bool isFrontFace : SV_IsFrontFace)
{
    ObjectIdColorOutput output;
    output.color=EvaluateWorldPixel(input,isFrontFace,true);
    output.objectToken=input.objectToken;
    return output;
}
