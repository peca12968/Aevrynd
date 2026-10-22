#include "OcclusionDither.hlsli"
#include "OcclusionWindow.hlsli"
#include "OcclusionNormal.hlsli"

Texture2D<float> sceneDepth : register(t0);
Texture2D<float2> ambientOcclusion : register(t1);
Texture2D baseColorTexture : register(t2);
struct SkinPaletteJoint
{
    row_major float4x4 transform;
    float4 normalRow0;
    float4 normalRow1;
    float4 normalRow2;
};
StructuredBuffer<SkinPaletteJoint> skinPalette : register(t13);
SamplerState pointClampSampler : register(s0);
SamplerState materialSampler : register(s1);

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
    float4 terrainParallaxParameters;
    float4 terrainParallaxView;
    float4 occlusionWindowEllipse;
    float4 occlusionWindowParameters;
};

cbuffer AmbientOcclusionConstants : register(b1)
{
    row_major float4x4 inverseProjection;
    float4 resolutionParameters;
    float4 occlusionParameters;
    float4 filterParameters;
    float4 viewportParameters;
    float4 contactShadowParameters;
};
// AO 深度通道必須沿用主著色器的 HLOD 共用 seed，否則會產生遮蔽孔洞。
uint StaticLodSeed(uint textureData,uint objectSeed){return((textureData>>13)&7u)^(((textureData&0x80000000u)!=0u)?0u:objectSeed);}

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

void SkinLocalVertex(SkinnedVertexInput input, out float3 position, out float3 normal)
{
    const float weightSum = dot(input.blendWeights, float4(1.0, 1.0, 1.0, 1.0));
    const float4 weights = input.blendWeights / max(weightSum, 1e-6);
    position = 0.0;
    normal = 0.0;
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
    }
    normal = normalize(normal);
}

struct DepthVertexOutput
{
    float4 position : SV_POSITION;
    nointerpolation float lodCoverage : TEXCOORD0;
    nointerpolation uint lodSeed : TEXCOORD1;
    nointerpolation uint lodInvert : TEXCOORD2;
    float2 uv : TEXCOORD3;
    nointerpolation float alphaCutoff : TEXCOORD4;
    nointerpolation uint alphaMasked : TEXCOORD5;
    float alpha : TEXCOORD6;
    nointerpolation float occlusionOpacity : TEXCOORD7;
    nointerpolation uint2 occlusionSeeds : TEXCOORD8;
    float3 occlusionCoordinate : TEXCOORD9;
    nointerpolation uint objectToken : OBJECTID;
};

float3 ApplyDepthWind(float3 position, float packedWeight, uint materialClass)
{
    // 深度通道必須與主色彩通道使用相同 foliage 判斷，避免一般模型寫入錯誤 AO 輪廓。
    const float weight = (materialClass == 2u ? 1.0 : 0.0) * step(0.0, packedWeight) * (1.0 - saturate(packedWeight));
    const float primary = sin(fogEndEnabled.w * 1.7 + position.x * 0.43 + position.z * 0.31);
    const float detail = sin(fogEndEnabled.w * 3.1 - position.x * 0.19 + position.z * 0.67);
    return position + float3(primary + detail * 0.35, abs(detail) * 0.08, detail * 0.55) * weight * 0.13;
}

DepthVertexOutput DepthWorldVertexMain(WorldVertexInput input)
{
    DepthVertexOutput output = (DepthVertexOutput)0;
    output.position = mul(viewProjection, float4(input.position, 1.0));
    output.lodCoverage = 1.0;
    output.lodSeed = 0;
    output.lodInvert = 0;
    output.occlusionOpacity = 1.0;
    output.occlusionCoordinate=input.position;
    return output;
}

DepthVertexOutput DepthInstancedVertexMain(InstancedVertexInput input, uint objectToken : OBJECTID)
{
    const float4 localPosition = float4(input.position, 1.0);
    float4 worldPosition = float4(dot(input.transformRow0, localPosition),
                                  dot(input.transformRow1, localPosition),
                                  dot(input.transformRow2, localPosition),
                                  dot(input.transformRow3, localPosition));
    worldPosition.xyz = ApplyDepthWind(worldPosition.xyz, input.normal.w, (input.textureData >> 2) & 3u);

    DepthVertexOutput output = (DepthVertexOutput)0;
    output.position = mul(viewProjection, worldPosition);
    output.lodCoverage = float((input.textureData >> 4) & 255) / 255.0;
    output.lodInvert = (input.textureData >> 12) & 1;
    // 平移量必須採用與主著色器相同的雜湊，深度與色彩通道才會保留相同孔點。
    const uint objectSeed = asuint(input.transformRow0.w) * 73856093u ^ asuint(input.transformRow1.w) * 19349663u ^ asuint(input.transformRow2.w) * 83492791u;
    output.lodSeed = StaticLodSeed(input.textureData, objectSeed);
    output.uv=input.uv*max(0.001,float((input.textureData>>16)&32767u)*(16.0/32767.0));
    output.alphaCutoff = saturate(input.normalRow1.w);
    output.alphaMasked = input.normalRow2.w > 0.5 ? 1u : 0u;
    output.alpha = input.color.a * input.tint.a;
    output.occlusionOpacity=input.occlusionOpacity;
    output.occlusionSeeds=input.occlusionSeeds;
    output.occlusionCoordinate=input.position;
    output.objectToken=objectToken;
    return output;
}

DepthVertexOutput DepthSkinnedVertexMain(SkinnedVertexInput input, uint objectToken : OBJECTID)
{
    float3 skinnedPosition;
    float3 skinnedNormal;
    SkinLocalVertex(input, skinnedPosition, skinnedNormal);
    const float4 localPosition = float4(skinnedPosition, 1.0);
    float4 worldPosition = float4(dot(input.transformRow0, localPosition),
                                  dot(input.transformRow1, localPosition),
                                  dot(input.transformRow2, localPosition),
                                  dot(input.transformRow3, localPosition));
    worldPosition.xyz = ApplyDepthWind(worldPosition.xyz, input.normal.w, (input.textureData >> 2) & 3u);

    DepthVertexOutput output = (DepthVertexOutput)0;
    output.position = mul(viewProjection, worldPosition);
    output.lodCoverage = float((input.textureData >> 4) & 255) / 255.0;
    output.lodInvert = (input.textureData >> 12) & 1;
    const uint objectSeed = asuint(input.transformRow0.w) * 73856093u ^ asuint(input.transformRow1.w) * 19349663u ^ asuint(input.transformRow2.w) * 83492791u;
    output.lodSeed = ((input.textureData >> 13) & 7) ^ objectSeed;
    output.uv=input.uv*max(0.001,float((input.textureData>>16)&255u)*(16.0/255.0));
    output.alphaCutoff=float((input.skinPaletteBase>>12)&255u)/255.0;
    output.alphaMasked=(input.skinPaletteBase>>20)&1u;
    output.alpha=input.color.a*input.tint.a;
    output.occlusionOpacity=input.occlusionOpacity;
    output.occlusionSeeds=input.occlusionSeeds;
    output.occlusionCoordinate=skinnedPosition;
    output.objectToken=objectToken;
    return output;
}

void DepthPixelMain(DepthVertexOutput input)
{
    if(input.alphaMasked!=0)clip(input.alpha*baseColorTexture.Sample(materialSampler,input.uv).a-input.alphaCutoff);
    // 固定螢幕雜訊完整重現不透明通道的 LOD 互補淡化。
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
    ClipOcclusionOpacity(input.occlusionCoordinate,ScreenWindowOcclusionOpacity((input.position.xy*colorPipeline.zw-worldViewport.xy)/max(worldViewport.zw,float2(0.000001,0.000001)),input.position.z,input.occlusionOpacity,occlusionWindowEllipse,occlusionWindowParameters),input.occlusionSeeds);
}

uint DepthObjectIdPixelMain(DepthVertexOutput input) : SV_TARGET
{
    // 與 AO/TAA 深度預通道共用完全相同的頂點輸出與裁切規則；物件識別
    // 因此不再以另一套 skinned vertex bytecode 重算輪廓深度。
    DepthPixelMain(input);
    return input.objectToken;
}

struct AmbientOcclusionVertexOutput
{
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD0;
};

AmbientOcclusionVertexOutput AmbientOcclusionVertexMain(uint vertexId : SV_VertexID)
{
    AmbientOcclusionVertexOutput output;
    const float2 uv = float2((vertexId << 1) & 2, vertexId & 2);
    output.position = float4(uv * float2(2.0, -2.0) + float2(-1.0, 1.0), 0.0, 1.0);
    output.uv = uv;
    return output;
}

float2 ViewportUv(float2 uv)
{
    return (uv - viewportParameters.xy) / max(viewportParameters.zw, 1e-6);
}

float3 ReconstructViewPosition(float2 uv, float depth)
{
    const float2 viewportUv = ViewportUv(uv);
    const float4 clipPosition = float4(viewportUv * float2(2.0, -2.0) + float2(-1.0, 1.0), depth, 1.0);
    const float4 viewPosition = mul(inverseProjection, clipPosition);
    return viewPosition.xyz / max(abs(viewPosition.w), 1e-6);
}

float ViewDepth(float2 uv)
{
    const float2 viewportUv = ViewportUv(uv);
    if (any(viewportUv < 0.0) || any(viewportUv > 1.0))
        return 1e6;
    const float depth = sceneDepth.SampleLevel(pointClampSampler, saturate(uv), 0.0);
    return depth >= 0.999999 ? 1e6 : length(ReconstructViewPosition(uv, depth) - cameraFogStart.xyz);
}

float3 ReconstructViewNormal(float2 uv, float3 centerPosition)
{
    const float2 fullResolutionTexel = resolutionParameters.xy;
    const float3 left = ReconstructViewPosition(uv - float2(fullResolutionTexel.x, 0.0), sceneDepth.SampleLevel(pointClampSampler, uv - float2(fullResolutionTexel.x, 0.0), 0.0));
    const float3 right = ReconstructViewPosition(uv + float2(fullResolutionTexel.x, 0.0), sceneDepth.SampleLevel(pointClampSampler, uv + float2(fullResolutionTexel.x, 0.0), 0.0));
    const float3 top = ReconstructViewPosition(uv - float2(0.0, fullResolutionTexel.y), sceneDepth.SampleLevel(pointClampSampler, uv - float2(0.0, fullResolutionTexel.y), 0.0));
    const float3 bottom = ReconstructViewPosition(uv + float2(0.0, fullResolutionTexel.y), sceneDepth.SampleLevel(pointClampSampler, uv + float2(0.0, fullResolutionTexel.y), 0.0));
    const float centerDepth = length(centerPosition - cameraFogStart.xyz);
    const float leftDepth = length(left - cameraFogStart.xyz), rightDepth = length(right - cameraFogStart.xyz);
    const float topDepth = length(top - cameraFogStart.xyz), bottomDepth = length(bottom - cameraFogStart.xyz);
    const float3 horizontal = abs(leftDepth - centerDepth) < abs(rightDepth - centerDepth) ? centerPosition - left : right - centerPosition;
    const float3 vertical = abs(topDepth - centerDepth) < abs(bottomDepth - centerDepth) ? centerPosition - top : bottom - centerPosition;
    return ReconstructOcclusionSurfaceNormal(horizontal, vertical, cameraFogStart.xyz - centerPosition);
}

struct AmbientOcclusionOutput { float2 occlusion : SV_TARGET0; float2 contactDepth : SV_TARGET1; };
AmbientOcclusionOutput AmbientOcclusionTemporalPixelMain(AmbientOcclusionVertexOutput input)
{
    AmbientOcclusionOutput output;output.occlusion=1.0;output.contactDepth=float2(1.0,1.0);
    const float2 uv = saturate(input.uv);
    const float2 viewportUv = ViewportUv(uv);
    if (any(viewportUv < 0.0) || any(viewportUv > 1.0))
        return output;
    const float depth = sceneDepth.SampleLevel(pointClampSampler, uv, 0.0);
    if (depth >= 0.999999)
        return output;

    const float3 centerPosition = ReconstructViewPosition(uv, depth);
    if (!all(isfinite(centerPosition)))
        return output;

    const float radius = max(occlusionParameters.x, 1e-4);
    const float bias = max(occlusionParameters.y, 0.0);
    const float intensity = max(occlusionParameters.z, 0.0);
    const float power = max(occlusionParameters.w, 1e-3);
    const float projectionScale = max(filterParameters.x, 1.0);
    const float maximumRadiusPixels = max(filterParameters.y, 1.0);
    const float cameraDistance = length(centerPosition - cameraFogStart.xyz);
    const float radiusPixels = min(radius * projectionScale / max(cameraDistance, 1e-3), maximumRadiusPixels);
    const float2 sampleScale = resolutionParameters.zw * radiusPixels;
    const float3 normal = ReconstructViewNormal(uv, centerPosition);

    static const float2 directions[12] = {
        float2(0.105, 0.027), float2(-0.148, 0.136), float2(0.061, -0.287),
        float2(0.335, 0.231), float2(-0.432, -0.078), float2(0.386, -0.365),
        float2(-0.196, 0.572), float2(-0.525, -0.421), float2(0.712, 0.092),
        float2(0.197, 0.752), float2(-0.735, 0.337), float2(0.604, -0.627)
    };

    float occlusion = 0.0;
    float weightSum = 0.0;
    [unroll] for (uint sampleIndex = 0; sampleIndex < 12; ++sampleIndex)
    {
        const float2 sampleUv = saturate(uv + directions[sampleIndex] * sampleScale);
        const float sampleDepth = sceneDepth.SampleLevel(pointClampSampler, sampleUv, 0.0);
        if (sampleDepth >= 0.999999)
            continue;
        const float3 samplePosition = ReconstructViewPosition(sampleUv, sampleDepth);
        const float3 delta = samplePosition - centerPosition;
        const float distanceSquared = dot(delta, delta);
        if (!all(isfinite(samplePosition)) || distanceSquared <= 1e-8 || distanceSquared >= radius * radius)
            continue;
        const float inverseDistance = rsqrt(distanceSquared);
        const float directionWeight = saturate(dot(normal, delta * inverseDistance) - bias);
        const float rangeWeight = saturate(1.0 - distanceSquared / (radius * radius));
        occlusion += directionWeight * rangeWeight;
        weightSum += rangeWeight;
    }

    const float normalizedOcclusion = weightSum > 1e-5 ? occlusion / weightSum : 0.0;
    const float ambientVisibility=pow(saturate(1.0-normalizedOcclusion*intensity),power);
    float contactVisibility=1.0;const uint contactSteps=min(24u,(uint)contactShadowParameters.x);
    if(contactSteps>0u){const float maximumDistance=contactShadowParameters.y,thickness=contactShadowParameters.z,strength=contactShadowParameters.w;const float3 rayDirection=-normalize(lightDirectionAmbient.xyz);const float2 contactPixel=floor(uv/resolutionParameters.zw);const float spatialPhase=frac(dot(contactPixel,float2(0.754877666,0.569840296)));const float rayPhase=frac(spatialPhase+filterParameters.z);[loop]for(uint stepIndex=1u;stepIndex<=24u;++stepIndex){if(stepIndex>contactSteps)break;const float travel=maximumDistance*((float(stepIndex)-0.5+rayPhase)/float(contactSteps));const float3 rayPosition=centerPosition+normal*thickness+rayDirection*travel;const float4 rayClip=mul(viewProjection,float4(rayPosition,1.0));if(rayClip.w<=1e-5)break;const float2 rayUv=rayClip.xy/rayClip.w*float2(.5,-.5)+.5;const float2 rayViewport=ViewportUv(rayUv);if(any(rayViewport<0.0)||any(rayViewport>1.0))break;const float sampledDepth=sceneDepth.SampleLevel(pointClampSampler,rayUv,0.0);if(sampledDepth>=.999999)continue;const float rayDeviceDepth=rayClip.z/rayClip.w;if(sampledDepth+max(1e-5,thickness*1e-4)<rayDeviceDepth){contactVisibility=1.0-strength*(1.0-float(stepIndex-1u)/float(contactSteps));break;}}}
    output.occlusion=float2(ambientVisibility,saturate(contactVisibility));output.contactDepth=float2(saturate(contactVisibility),cameraDistance);return output;
}
float2 AmbientOcclusionPixelMain(AmbientOcclusionVertexOutput input) : SV_TARGET0
{
    return AmbientOcclusionTemporalPixelMain(input).occlusion;
}

float2 AmbientOcclusionBilateralBlurPixelMain(AmbientOcclusionVertexOutput input) : SV_TARGET
{
    const float2 uv = saturate(input.uv);
    const float centerDepth = ViewDepth(uv);
    if (centerDepth >= 1e5)
        return 1.0;

    static const float weights[3] = { 0.33421, 0.14650, 0.03645 };
    const float depthSharpness = max(filterParameters.x, 0.0);
    float2 centerOcclusion=ambientOcclusion.SampleLevel(pointClampSampler,uv,0.0);if(filterParameters.y>0.5)centerOcclusion.y=baseColorTexture.SampleLevel(pointClampSampler,uv,0.0).x;float2 result = centerOcclusion * weights[0];
    float weightSum = weights[0];
    [unroll] for (uint axis = 0; axis < 2; ++axis)
    {
        const float2 direction = axis == 0 ? float2(resolutionParameters.z, 0.0) : float2(0.0, resolutionParameters.w);
        [unroll] for (uint offset = 1; offset <= 2; ++offset)
        {
            [unroll] for (int sign = -1; sign <= 1; sign += 2)
            {
                const float2 sampleUv = saturate(uv + direction * float(sign * int(offset)));
                const float sampleDepth = ViewDepth(sampleUv);
                const float bilateralWeight = weights[offset] * exp2(-abs(sampleDepth - centerDepth) * depthSharpness);
                float2 sampleOcclusion=ambientOcclusion.SampleLevel(pointClampSampler,sampleUv,0.0);if(filterParameters.y>0.5)sampleOcclusion.y=baseColorTexture.SampleLevel(pointClampSampler,sampleUv,0.0).x;result += sampleOcclusion * bilateralWeight;
                weightSum += bilateralWeight;
            }
        }
    }
    return saturate(result / max(weightSum, 1e-5));
}
