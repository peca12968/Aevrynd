cbuffer Scene : register(b0)
{
    row_major float4x4 viewProjection;
    float4 lightDirectionAmbient;
    float4 lightColorIntensity;
    float4 fogColor;
    float4 cameraFogStart;
    float4 fogEndEnabled;
    float4 pointPositionRadius[3];
    float4 pointColorIntensity[3];
    float4 colorPipeline;
    row_major float4x4 inverseViewProjection;
    float4 worldViewport;
    row_major float4x4 previousViewProjection;
    float4 motionParameters;
    row_major float4x4 currentUnjitteredViewProjection;
    float4 temporalJitterPixels;
};

struct MotionInstanceInput
{
    float3 position : POSITION;
    float4 transformRow0 : INSTANCE0;
    float4 transformRow1 : INSTANCE1;
    float4 transformRow2 : INSTANCE2;
    float4 transformRow3 : INSTANCE3;
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
};

MotionVertexOutput VelocityWorldVertexMain(MotionWorldInput input)
{
    MotionVertexOutput output;
    const float4 worldPosition = float4(input.position, 1.0);
    output.position = mul(viewProjection, worldPosition);
    output.currentClip = mul(currentUnjitteredViewProjection, worldPosition);
    output.previousClip = mul(previousViewProjection, worldPosition);
    return output;
}

MotionVertexOutput VelocityInstancedVertexMain(MotionInstanceInput input)
{
    const float4 localPosition = float4(input.position, 1.0);
    const float4 currentWorld = float4(dot(input.transformRow0, localPosition), dot(input.transformRow1, localPosition), dot(input.transformRow2, localPosition), dot(input.transformRow3, localPosition));
    const float4 previousWorld = float4(dot(input.previousRow0, localPosition), dot(input.previousRow1, localPosition), dot(input.previousRow2, localPosition), dot(input.previousRow3, localPosition));
    MotionVertexOutput output;
    output.position = mul(viewProjection, currentWorld);
    output.currentClip = mul(currentUnjitteredViewProjection, currentWorld);
    output.previousClip = mul(previousViewProjection, previousWorld);
    return output;
}

float2 VelocityPixelMain(MotionVertexOutput input) : SV_TARGET
{
    if (motionParameters.x < 0.5 || input.currentClip.w <= 1e-6 || input.previousClip.w <= 1e-6)
        return 0.0;
    const float2 currentNdc = input.currentClip.xy / input.currentClip.w;
    const float2 previousNdc = input.previousClip.xy / input.previousClip.w;
    return (currentNdc - previousNdc) * float2(motionParameters.y, -motionParameters.z);
}
cbuffer ShadowScene : register(b1) { row_major float4x4 lightViewProjection[2]; float4 shadowParameters; float4 shadowTransition; };
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
Texture2D<float> ambientOcclusionTexture : register(t11);
Texture2D<float> sceneDepthTexture : register(t12);
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

struct MotionSkinnedVertexInput
{
    float3 position : POSITION;
    uint4 blendIndices : BLENDINDICES;
    float4 blendWeights : BLENDWEIGHT;
    float4 transformRow0 : INSTANCE0;
    float4 transformRow1 : INSTANCE1;
    float4 transformRow2 : INSTANCE2;
    float4 transformRow3 : INSTANCE3;
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

MotionVertexOutput VelocitySkinnedVertexMain(MotionSkinnedVertexInput input)
{
    const float4 currentLocal = float4(SkinMotionPosition(input.position, input.blendIndices, input.blendWeights, input.skinPaletteBase), 1.0);
    const float4 previousLocal = float4(SkinMotionPosition(input.position, input.blendIndices, input.blendWeights, input.previousPaletteBase), 1.0);
    const float4 currentWorld = float4(dot(input.transformRow0, currentLocal), dot(input.transformRow1, currentLocal), dot(input.transformRow2, currentLocal), dot(input.transformRow3, currentLocal));
    const float4 previousWorld = float4(dot(input.previousRow0, previousLocal), dot(input.previousRow1, previousLocal), dot(input.previousRow2, previousLocal), dot(input.previousRow3, previousLocal));
    MotionVertexOutput output;
    output.position = mul(viewProjection, currentWorld);
    output.currentClip = mul(currentUnjitteredViewProjection, currentWorld);
    output.previousClip = mul(previousViewProjection, previousWorld);
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
    float3 textureData : TEXCOORD3;
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
};

struct ShadowVertexOutput { float4 position : SV_POSITION; float2 uv : TEXCOORD0; nointerpolation float alphaCutoff : TEXCOORD1; nointerpolation uint alphaMasked : TEXCOORD2; float alpha : TEXCOORD3; };
float3 ApplyWind(float3 position, float packedWeight);

ShadowVertexOutput ShadowWorldVertexMain(WorldVertexInput input)
{
    ShadowVertexOutput output = (ShadowVertexOutput)0;
    output.position = mul(lightViewProjection[shadowCascadeIndex], float4(ApplyWind(input.position, input.normal.w), 1.0));
    return output;
}

ShadowVertexOutput ShadowInstancedVertexMain(InstancedVertexInput input)
{
    const float4 localPosition = float4(input.position, 1.0);
    float4 worldPosition = float4(dot(input.transformRow0, localPosition),
                                  dot(input.transformRow1, localPosition),
                                  dot(input.transformRow2, localPosition),
                                  dot(input.transformRow3, localPosition));
    worldPosition.xyz = ApplyWind(worldPosition.xyz, input.normal.w);
    ShadowVertexOutput output = (ShadowVertexOutput)0;
    output.position = mul(lightViewProjection[shadowCascadeIndex], worldPosition);
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
    worldPosition.xyz = ApplyWind(worldPosition.xyz, input.normal.w);
    ShadowVertexOutput output = (ShadowVertexOutput)0;
    output.position = mul(lightViewProjection[shadowCascadeIndex], worldPosition);
    output.uv=input.uv;
    output.alphaCutoff=float((input.skinPaletteBase>>12)&255u)/255.0;
    output.alphaMasked=(input.skinPaletteBase>>20)&1u;
    output.alpha=input.color.a*input.tint.a;
    return output;
}

void ShadowSkinnedPixelMain(ShadowVertexOutput input)
{
    if(input.alphaMasked!=0)clip(input.alpha*baseColorTexture.Sample(materialSampler,input.uv).a-input.alphaCutoff);
}

float3 ApplyWind(float3 position, float packedWeight)
{
    const float weight = step(0.0, packedWeight) * (1.0 - saturate(packedWeight));
    const float primary = sin(fogEndEnabled.w * 1.7 + position.x * 0.43 + position.z * 0.31);
    const float detail = sin(fogEndEnabled.w * 3.1 - position.x * 0.19 + position.z * 0.67);
    return position + float3(primary + detail * 0.35, abs(detail) * 0.08, detail * 0.55) * weight * 0.13;
}

WorldPixelInput WorldVertexMain(WorldVertexInput input)
{
    WorldPixelInput output = (WorldPixelInput)0;
    const float3 animatedPosition = ApplyWind(input.position, input.normal.w);
    output.position = mul(viewProjection, float4(animatedPosition, 1.0));
    output.color = input.color;
    output.worldPosition = animatedPosition;
    output.normal = float4(input.normal.xyz, input.normal.w < 0.0 ? 0.0 : 1.0);
    output.material = float4(0.72, 0.0, 0.0, 1.0);
    output.textureData = float3(0.0, 0.25, 0.0);
    output.uv = input.uv;
    output.materialClass = 0;
    output.lodCoverage = 1.0;
    output.lodSeed = 0;
    output.lodInvert = 0;
    output.tangent = 0.0;
    output.characterShadingModel = 0;
    output.alphaCutoff = 0.5;
    output.alphaMasked = 0;
    output.normalScale = 1.0;
    output.shadowPosition = mul(lightViewProjection[0], float4(animatedPosition, 1.0));
    output.farShadowPosition = mul(lightViewProjection[1], float4(animatedPosition, 1.0));
    return output;
}

WorldPixelInput InstancedVertexMain(InstancedVertexInput input)
{
    const float4 localPosition = float4(input.position, 1.0);
    float4 worldPosition = float4(dot(input.transformRow0, localPosition), dot(input.transformRow1, localPosition), dot(input.transformRow2, localPosition), dot(input.transformRow3, localPosition));
    worldPosition.xyz = ApplyWind(worldPosition.xyz, input.normal.w);
    const float3 worldNormal = float3(dot(input.normalRow0.xyz, input.normal.xyz),
                                      dot(input.normalRow1.xyz, input.normal.xyz),
                                      dot(input.normalRow2.xyz, input.normal.xyz));
    WorldPixelInput output = (WorldPixelInput)0;
    output.position = mul(viewProjection, worldPosition);
    output.color = float4(input.color.rgb * input.tint.rgb, input.color.a * input.tint.a);
    output.worldPosition = worldPosition.xyz;
    output.normal = float4(normalize(worldNormal), input.normal.w < 0.0 ? 0.0 : 1.0);
    output.material = input.material;
    output.textureData = float3(input.textureData & 1,
                                max(0.001, float((input.textureData >> 16) & 65535) * (16.0 / 65535.0)),
                                (input.textureData >> 1) & 1);
    output.uv = input.uv;
    output.materialClass = (input.textureData >> 2) & 3;
    output.lodCoverage = float((input.textureData >> 4) & 255) / 255.0;
    output.lodInvert = (input.textureData >> 12) & 1;
    // 物件平移量擴充有限的封裝 seed，讓密集植被不會同步顯示相同的螢幕孔點。
    const uint objectSeed = asuint(input.transformRow0.w) * 73856093u ^ asuint(input.transformRow1.w) * 19349663u ^ asuint(input.transformRow2.w) * 83492791u;
    output.lodSeed = ((input.textureData >> 13) & 7) ^ objectSeed;
    output.tangent = 0.0;
    output.characterShadingModel = 0;
    output.alphaCutoff = 0.5;
    output.alphaMasked = 0;
    // normal matrix 第一列的 w 原本未使用，直接承載逐材質強度而不擴充 instance stride。
    output.normalScale = input.normalRow0.w;
    output.shadowPosition = mul(lightViewProjection[0], worldPosition);
    output.farShadowPosition = mul(lightViewProjection[1], worldPosition);
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
    worldPosition.xyz = ApplyWind(worldPosition.xyz, input.normal.w);
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
    output.alphaMasked=(input.skinPaletteBase>>20)&1u;
    // 10 位元二補數 Q5 定點數可在不擴充實例資料的情況下保存 glTF normalTexture.scale。
    const uint encodedNormalScale=(input.skinPaletteBase>>22)&1023u;
    const int signedNormalScale=encodedNormalScale>=512u?int(encodedNormalScale)-1024:int(encodedNormalScale);
    output.normalScale=float(signedNormalScale)/32.0;
    output.material = input.material;
    output.textureData = float3(input.textureData & 1,
                                max(0.001, float((input.textureData >> 16) & 65535) * (16.0 / 65535.0)),
                                (input.textureData >> 1) & 1);
    output.uv = input.uv;
    output.materialClass = (input.textureData >> 2) & 3;
    output.lodCoverage = float((input.textureData >> 4) & 255) / 255.0;
    output.lodInvert = (input.textureData >> 12) & 1;
    const uint objectSeed = asuint(input.transformRow0.w) * 73856093u ^ asuint(input.transformRow1.w) * 19349663u ^ asuint(input.transformRow2.w) * 83492791u;
    output.lodSeed = ((input.textureData >> 13) & 7) ^ objectSeed;
    output.shadowPosition = mul(lightViewProjection[0], worldPosition);
    output.farShadowPosition = mul(lightViewProjection[1], worldPosition);
    return output;
}

float3 BlendWeights(float3 normal)
{
    const float3 blend = pow(abs(normal), 4.0);
    return blend / max(0.001, blend.x + blend.y + blend.z);
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

float3 EvaluateAnalyticSkyRadiance(float3 rayDirection, float3 sunDirection, float roughness)
{
    const float blur = saturate(roughness) * saturate(roughness);
    const float3 blurredDirection = normalize(lerp(rayDirection, float3(0.0, 1.0, 0.0), blur * 0.28));
    const float height = clamp(blurredDirection.y, -0.08, 1.0);
    const float opticalDepth = min(8.0, 1.0 / max(0.09, height + 0.13));
    const float sunCosine = clamp(dot(blurredDirection, sunDirection), -1.0, 1.0);
    const float3 rayleighColor = float3(0.23, 0.48, 1.0) * RayleighPhase(sunCosine) * (0.75 + opticalDepth * 0.16);
    const float3 mieColor = float3(1.0, 0.68, 0.38) * MiePhase(sunCosine, 0.76) * (0.045 + opticalDepth * 0.018);
    const float horizon = pow(saturate(1.0 - abs(height)), 4.0);
    const float sunsetAmount = 1.0 - saturate(max(0.0, sunDirection.y) * 2.2);
    const float3 horizonColor = lerp(float3(0.26, 0.38, 0.52), float3(0.72, 0.49, 0.30), sunsetAmount);
    const float sunDisk = pow(saturate(sunCosine), lerp(1400.0, 12.0, blur));
    const float sunGlow = pow(saturate(sunCosine), lerp(22.0, 4.0, blur));
    const float sunAmplitude = lerp(7.0, 0.10, blur);
    return clamp(rayleighColor + mieColor + horizonColor * horizon * 0.42
               + float3(1.0, 0.78, 0.48) * (sunDisk * sunAmplitude + sunGlow * lerp(0.16, 0.04, blur)), 0.0, 16.0);
}

float3 EvaluateAtmosphere(float3 rayDirection, float3 sunDirection)
{
    return EvaluateAnalyticSkyRadiance(rayDirection, sunDirection, 0.0);
}

float3 EvaluateAnalyticSkyDiffuse(float3 normal, float3 sunDirection)
{
    const float upperHemisphere = saturate(normal.y * 0.5 + 0.5);
    const float horizon = pow(saturate(1.0 - abs(normal.y)), 2.0);
    const float sunsetAmount = 1.0 - saturate(max(0.0, sunDirection.y) * 2.2);
    const float3 horizonColor = lerp(float3(0.26, 0.38, 0.52), float3(0.72, 0.49, 0.30), sunsetAmount);
    const float3 hemisphereColor = lerp(float3(0.055, 0.045, 0.035), float3(0.16, 0.36, 0.78), upperHemisphere);
    const float sunWrap = pow(saturate(dot(normal, sunDirection) * 0.5 + 0.5), 4.0);
    return clamp((hemisphereColor + horizonColor * horizon * 0.38 + float3(1.0, 0.68, 0.38) * sunWrap * 0.055) * 2.4, 0.0, 4.0);
}

float3 EvaluateAnalyticSkySpecular(float3 reflectionDirection, float3 sunDirection, float roughness)
{
    return EvaluateAnalyticSkyRadiance(reflectionDirection, sunDirection, clamp(roughness, 0.045, 1.0));
}

float CloudHash(float2 coordinate)
{
    coordinate = frac(coordinate * float2(123.34, 456.21));
    coordinate += dot(coordinate, coordinate + 45.32);
    return frac(coordinate.x * coordinate.y);
}

float CloudNoise(float2 coordinate)
{
    const float2 cell = floor(coordinate);
    const float2 fraction = frac(coordinate);
    const float2 smoothFraction = fraction * fraction * (3.0 - 2.0 * fraction);
    const float lower = lerp(CloudHash(cell), CloudHash(cell + float2(1.0, 0.0)), smoothFraction.x);
    const float upper = lerp(CloudHash(cell + float2(0.0, 1.0)), CloudHash(cell + 1.0), smoothFraction.x);
    return lerp(lower, upper, smoothFraction.y);
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

float DirectionalShadow(float4 shadowPosition, uint cascadeIndex, float3 worldPosition, float3 normal, float3 lightDirection)
{
    const float3 projected = shadowPosition.xyz / max(0.0001, shadowPosition.w);
    const float2 uv = float2(projected.x * 0.5 + 0.5, -projected.y * 0.5 + 0.5);
    if (shadowParameters.x < 0.5 || projected.z <= 0.0 || projected.z >= 1.0 || any(uv < 0.0) || any(uv > 1.0)) return 1.0;
    const float receiverCosine = saturate(dot(normal, lightDirection));
    const float receiverSlope = sqrt(max(0.0, 1.0 - receiverCosine * receiverCosine)) / max(0.2, receiverCosine);
    const float bias = min(0.006, shadowParameters.z + shadowParameters.y * receiverSlope);
    const uint rotationIndex = min(3u, (uint)(CloudHash(floor(worldPosition.xz * 2.0) * 0.173 + float(cascadeIndex) * 19.19) * 4.0));
    const float swapAxes = float(rotationIndex & 1u);
    const float2 rotationSigns = float2((rotationIndex == 1u || rotationIndex == 2u) ? -1.0 : 1.0, rotationIndex >= 2u ? -1.0 : 1.0);
    static const float2 poisson[12] = {
        float2(-0.326, -0.406), float2(-0.840, -0.074), float2(-0.696, 0.457), float2(-0.203, 0.621),
        float2(0.962, -0.195), float2(0.473, -0.480), float2(0.519, 0.768), float2(0.185, -0.893),
        float2(0.507, 0.064), float2(0.896, 0.412), float2(-0.322, -0.933), float2(-0.792, -0.598)
    };
    float visibility = 0.0;
    [unroll] for (int sampleIndex = 0; sampleIndex < 12; ++sampleIndex)
    {
        const float2 offset = lerp(poisson[sampleIndex], poisson[sampleIndex].yx, swapAxes) * rotationSigns * shadowParameters.w * 1.45;
        visibility += shadowTexture.SampleCmpLevelZero(shadowSampler, float3(uv + offset, cascadeIndex), projected.z - bias);
    }
    return visibility / 12.0;
}

float CascadedDirectionalShadow(WorldPixelInput input, float3 normal, float3 lightDirection)
{
    const float cameraDistance = distance(input.worldPosition, cameraFogStart.xyz);
    const float halfWidth = shadowTransition.x * 0.5;
    float visibility = 1.0;
    if (cameraDistance <= shadowParameters.x - halfWidth) visibility = DirectionalShadow(input.shadowPosition, 0, input.worldPosition, normal, lightDirection);
    else if (cameraDistance >= shadowParameters.x + halfWidth) visibility = DirectionalShadow(input.farShadowPosition, 1, input.worldPosition, normal, lightDirection);
    else
    {
        const float blend = smoothstep(shadowParameters.x - halfWidth, shadowParameters.x + halfWidth, cameraDistance);
        visibility = lerp(DirectionalShadow(input.shadowPosition, 0, input.worldPosition, normal, lightDirection), DirectionalShadow(input.farShadowPosition, 1, input.worldPosition, normal, lightDirection), blend);
    }
    return visibility;
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

float3 EvaluateEnvironmentLighting(float3 normal, float3 viewDirection, float3 baseColor,
                                   float roughness, float metallic, float occlusion,
                                   float ambientIntensity, float3 sunDirection)
{
    const float3 reflectance = lerp(float3(0.04, 0.04, 0.04), baseColor, metallic);
    const float3 fresnel = FresnelSchlickRoughness(saturate(dot(normal, viewDirection)), reflectance, roughness);
    const float3 diffuseWeight = (1.0 - fresnel) * (1.0 - metallic);
    const float3 skyDiffuse = EvaluateAnalyticSkyDiffuse(normal, sunDirection);
    const float3 reflectionDirection = reflect(-viewDirection, normal);
    const float3 skySpecular = EvaluateAnalyticSkySpecular(reflectionDirection, sunDirection, roughness);
    const float specularEnergy = 0.35 + 0.65 * (1.0 - roughness);
    return (diffuseWeight * baseColor * skyDiffuse + fresnel * skySpecular * specularEnergy)
         * clamp(ambientIntensity, 0.0, 2.0) * saturate(occlusion);
}

float3 EvaluatePbrLight(float3 normal, float3 viewDirection, float3 lightDirection,
                        float3 radiance, float3 baseColor, float roughness, float metallic)
{
    const float normalLight = saturate(dot(normal, lightDirection));
    const float normalView = saturate(dot(normal, viewDirection));
    if (normalLight <= 0.0 || normalView <= 0.0)
        return 0.0;
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

float3 EvaluateEyeEnvironment(float3 baseNormal, float3 corneaNormal, float3 viewDirection,
                              float3 baseColor, float materialRoughness, float occlusion,
                              float ambientIntensity, float3 sunDirection)
{
    const float3 baseLayer = EvaluateEnvironmentLighting(baseNormal, viewDirection, baseColor,
                                                          materialRoughness, 0.0, occlusion,
                                                          ambientIntensity, sunDirection);
    const float fresnel = CorneaFresnel(dot(corneaNormal, viewDirection));
    const float3 reflectionDirection = reflect(-viewDirection, corneaNormal);
    const float3 clearcoat = EvaluateAnalyticSkySpecular(reflectionDirection, sunDirection,
                                                         CorneaRoughness(materialRoughness))
                           * fresnel * clamp(ambientIntensity, 0.0, 2.0) * saturate(occlusion);
    return baseLayer * (1.0 - fresnel) + clearcoat;
}

float3 EvaluateSkinSupplement(float3 normal,float3 viewDirection,float3 lightDirection,
                              float3 radiance,float3 baseColor,float roughness)
{
    const float normalLight = dot(normal,lightDirection);
    const float wrapped = saturate((normalLight + 0.25) / 1.25);
    const float wrapExtra = max(0.0,wrapped - saturate(normalLight));
    const float backScatter = pow(saturate(dot(-lightDirection,viewDirection)),4.0)
                            * saturate(-normalLight);
    const float narrowRoughness = max(0.09,roughness * 0.45);
    const float3 narrowSpecular = EvaluatePbrLight(normal,viewDirection,lightDirection,
                                                    radiance,0.0,narrowRoughness,0.0) * 0.18;
    const float3 warmScatter = baseColor * float3(1.0,0.48,0.32)
                             * radiance * (wrapExtra * 0.20 + backScatter * 0.08);
    return narrowSpecular + warmScatter;
}

float3 EvaluateHairSupplement(float3 normal,float3 tangent,float3 viewDirection,float3 lightDirection,
                               float3 radiance,float3 baseColor,float roughness)
{
    // 髮絲切線無效時保留標準 PBR；避免退化骨架或奇異縮放把 NaN 傳入 HDR。
    const float tangentLengthSquared=dot(tangent,tangent);
    const float halfLengthSquared=dot(viewDirection+lightDirection,viewDirection+lightDirection);
    if(tangentLengthSquared<0.000001||tangentLengthSquared>1000000.0||halfLengthSquared<0.000001||halfLengthSquared>1000000.0)return 0.0;
    const float3 projectedStrand=tangent-normal*dot(normal,tangent);
    const float strandLengthSquared=dot(projectedStrand,projectedStrand);
    if(strandLengthSquared<0.000001||strandLengthSquared>1000000.0)return 0.0;
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

float CharacterNormalDetailStrength(float skinMask,float hairMask,float eyeMask)
{
    // 皮膚與角膜只保留低幅細節，避免壓縮法線在近距離形成塑膠凹凸；髮絲仍保留較強方向性。
    return lerp(0.78,0.56,skinMask)+hairMask*(0.68-0.78)+eyeMask*(0.32-0.78);
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

float MaskedAlphaToCoverage(float materialAlpha, float alphaCutoff)
{
    // 只在門檻附近保留窄漸層，讓 MSAA alpha-to-coverage 產生穩定的子像素覆蓋率。
    const float transitionWidth = max(fwidth(materialAlpha) * 0.5, 1.0 / 255.0);
    return smoothstep(alphaCutoff - transitionWidth, alphaCutoff + transitionWidth, materialAlpha);
}

float4 EvaluateWorldPixel(WorldPixelInput input, bool isFrontFace, bool alphaToCoverage)
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
    const float3 geometricNormal = normalize(input.normal.xyz);
    float3 normal = geometricNormal;
    float roughness = clamp(input.material.x, 0.045, 1.0);
    const float skinMask = input.characterShadingModel == 1 ? 1.0 : 0.0;
    const float hairMask = input.characterShadingModel == 2 ? 1.0 : 0.0;
    const float eyeMask = input.characterShadingModel == 3 ? 1.0 : 0.0;
    const float normalDetailStrength=CharacterNormalDetailStrength(skinMask,hairMask,eyeMask)*input.normalScale;
    const float metallic = lerp(saturate(input.material.y),0.0,saturate(skinMask+hairMask+eyeMask));
    const float emissive = saturate(input.material.z);
    const float occlusion = saturate(input.material.w);
    float3 baseColor = input.color.rgb;
    float materialAlpha = input.color.a;
    if (input.textureData.x > 0.5 && input.materialClass != 3)
    {
        if (input.textureData.z > 0.5)
        {
            const float4 sampledBaseColor=baseColorTexture.Sample(materialSampler,input.uv);
            materialAlpha *= sampledBaseColor.a;
            if(input.alphaMasked!=0&&!alphaToCoverage)clip(materialAlpha-input.alphaCutoff);
            baseColor *= sampledBaseColor.rgb;
            roughness = clamp(roughness * roughnessTexture.Sample(materialSampler, input.uv).r, 0.045, 1.0);
            // 導數必須在一致控制流中計算；有可靠頂點 tangent 時僅將它保留為奇異變形的安全退路。
            const float3 derivativeNormal = UvNormal(input.worldPosition, normal, input.uv);
            normal = ScaleNormalDetail(normal,UvOrVertexTangentNormal(normal,input.tangent,input.uv,derivativeNormal),normalDetailStrength);
        }
        else
        {
            baseColor *= Triplanar(baseColorTexture, input.worldPosition, normal, input.textureData.y).rgb;
            roughness = clamp(roughness * Triplanar(roughnessTexture, input.worldPosition, normal, input.textureData.y).r, 0.045, 1.0);
            normal = ScaleNormalDetail(normal,TriplanarNormal(input.worldPosition,normal,input.textureData.y),normalDetailStrength);
        }
    }

    const float terrainMask = input.materialClass == 3 ? 1.0 : 0.0;
    [branch] if (terrainMask > 0.5)
    {
        const float macroNoise = CloudNoise(input.worldPosition.xz * 0.021);
        const float microNoise = CloudHash(floor(input.worldPosition.xz * 3.2));
        const float terrainSlope = 1.0 - saturate(abs(geometricNormal.y));
        const float terrainHeight = saturate(input.color.a);
        const float rockWeight = smoothstep(0.38, 0.78, terrainSlope + max(0.0, terrainHeight - 0.62) * 0.35);
        const float soilWeight = (1.0 - rockWeight) * smoothstep(0.18, 0.62, terrainSlope + (1.0 - macroNoise) * 0.18);
        const float grassWeight = max(0.0, 1.0 - rockWeight - soilWeight);
        if (input.textureData.x > 0.5)
        {
            const float2 terrainUv = input.worldPosition.xz * input.textureData.y;
            const float3 grassColor = baseColorTexture.Sample(materialSampler, terrainUv).rgb;
            const float3 soilColor = soilBaseColorTexture.Sample(materialSampler, terrainUv).rgb;
            const float3 rockColor = Triplanar(rockBaseColorTexture, input.worldPosition, geometricNormal, input.textureData.y).rgb;
            const float grassRoughness = roughnessTexture.Sample(materialSampler, terrainUv).r;
            const float soilRoughness = soilRoughnessTexture.Sample(materialSampler, terrainUv).r;
            const float rockRoughness = Triplanar(rockRoughnessTexture, input.worldPosition, geometricNormal, input.textureData.y).r;
            const float3 grassNormal = TerrainTopDownNormal(normalTexture, input.worldPosition, geometricNormal, input.textureData.y);
            const float3 soilNormal = TerrainTopDownNormal(soilNormalTexture, input.worldPosition, geometricNormal, input.textureData.y);
            const float3 rockNormal = TriplanarNormalFrom(rockNormalTexture, input.worldPosition, geometricNormal, input.textureData.y);
            baseColor *= (grassColor * grassWeight + soilColor * soilWeight + rockColor * rockWeight) * lerp(0.94, 1.06, microNoise);
            roughness = clamp(roughness * (grassRoughness * grassWeight + soilRoughness * soilWeight + rockRoughness * rockWeight), 0.045, 1.0);
            normal = normalize(grassNormal * grassWeight + soilNormal * soilWeight + rockNormal * rockWeight);
        }
        else
        {
            const float3 terrainTint = grassWeight * float3(0.72, 0.91, 0.63) + soilWeight * float3(0.82, 0.67, 0.48) + rockWeight * float3(0.76, 0.78, 0.80);
            baseColor *= terrainTint * lerp(0.94, 1.06, microNoise);
            roughness = clamp(roughness * (grassWeight * 0.86 + soilWeight * 1.03 + rockWeight * 0.74), 0.045, 1.0);
        }
    }

    const float foliageMask = input.materialClass == 2 ? 1.0 : 0.0;
    if (foliageMask > 0.5 && !isFrontFace) normal = -normal;

    const float skyMask = 1.0 - saturate(input.normal.w);
    const float3 skyRay = normalize(input.worldPosition - cameraFogStart.xyz);
    const float3 skyLightDirection = -normalize(lightDirectionAmbient.xyz);
    [branch] if (skyMask > 0.5)
    {
        baseColor = ApplyProceduralClouds(EvaluateAtmosphere(skyRay, skyLightDirection), skyRay, skyLightDirection);
        // 天空不需要執行材質、陰影與點光源路徑，直接輸出曝光後的線性 HDR。
        const float skyAlpha = alphaToCoverage && input.alphaMasked != 0
                             ? MaskedAlphaToCoverage(materialAlpha, input.alphaCutoff)
                             : input.color.a;
        return float4(max(0.0, baseColor * colorPipeline.x), skyAlpha);
    }

    // 材質類別明確決定水面；法線門檻僅避免垂直側面產生水平水波。
    const float waterMask = input.materialClass == 1 ? step(0.35, normal.y) : 0.0;
    float foamMask = waterMask * smoothstep(0.30, 0.50, roughness);
    float waterThickness = 1.6;
    float waterDepthValid = 0.0;
    float waterOutputAlpha = input.color.a;
    [branch] if (waterMask > 0.5 && worldViewport.z > 0.0 && worldViewport.w > 0.0)
    {
        const float2 screenUv = input.position.xy * colorPipeline.zw;
        const float2 viewportMaximum = worldViewport.xy + worldViewport.zw;
        const float opaqueDepth = sceneDepthTexture.SampleLevel(screenPointClampSampler, screenUv, 0.0);
        const bool insideViewport = all(screenUv >= worldViewport.xy) && all(screenUv <= viewportMaximum);
        if (insideViewport && opaqueDepth > 0.0 && opaqueDepth < 0.999999)
        {
            const float3 bottomWorldPosition = ReconstructWorldPosition(screenUv, opaqueDepth);
            const float measuredThickness = distance(bottomWorldPosition, input.worldPosition);
            if (measuredThickness >= 0.0 && measuredThickness < 100000.0)
            {
                waterDepthValid = 1.0;
                waterThickness = clamp(measuredThickness, 0.0, 8.0);
                // 真實河床深度取代人工岸線網格；近岸泡沫在淺水處自然形成並快速破碎。
                const float shore = 1.0 - smoothstep(0.06, 0.72, waterThickness);
                const float shoreNoise = lerp(0.62, 1.0, CloudNoise(input.worldPosition.xz * 0.58 + fogEndEnabled.ww * float2(0.035, -0.021)));
                foamMask = saturate(shore * shoreNoise);
                waterOutputAlpha = lerp(0.34, 0.91, smoothstep(0.04, 4.5, waterThickness));
            }
        }
    }
    [branch] if (waterMask > 0.5)
    {
        const float2 broadRipple = float2(sin(input.worldPosition.z * 0.38 + input.worldPosition.x * 0.11 + fogEndEnabled.w * 0.72),
                                          cos(input.worldPosition.x * 0.31 - input.worldPosition.z * 0.16 - fogEndEnabled.w * 0.61));
        const float2 detailRipple = float2(sin(input.worldPosition.z * 1.73 - input.worldPosition.x * 0.64 + fogEndEnabled.w * 1.91),
                                           cos(input.worldPosition.x * 1.42 + input.worldPosition.z * 0.57 - fogEndEnabled.w * 1.67));
        const float2 waterRipple = broadRipple * 0.68 + detailRipple * 0.32;
        normal = normalize(normal + float3(waterRipple.x, 0.0, waterRipple.y) * lerp(0.085, 0.035, foamMask));
    }

    const float3 lightDirection = -normalize(lightDirectionAmbient.xyz);
    const float3 viewDirection = normalize(cameraFogStart.xyz - input.worldPosition);
    roughness = lerp(roughness, max(roughness, 0.72), foliageMask * pow(1.0 - saturate(dot(normal, viewDirection)), 2.0));
    // SSAO 僅調制間接環境光，避免把太陽、點光、自發光與水面反射一起壓暗。
    const float screenOcclusion = colorPipeline.y > 0.5 && waterMask < 0.5
                                ? ambientOcclusionTexture.SampleLevel(materialSampler, input.position.xy * colorPipeline.zw, 0.0)
                                : 1.0;
    const float3 environmentLighting = eyeMask > 0.5
        ? EvaluateEyeEnvironment(normal, geometricNormal, viewDirection, baseColor, roughness,
                                 occlusion * screenOcclusion, lightDirectionAmbient.w, skyLightDirection)
        : EvaluateEnvironmentLighting(normal, viewDirection, baseColor, roughness, metallic,
                                      occlusion * screenOcclusion, lightDirectionAmbient.w, skyLightDirection);
    const float3 directionalLighting = eyeMask > 0.5
        ? EvaluateEyeLight(normal, geometricNormal, viewDirection, lightDirection,
                           lightColorIntensity.rgb * lightColorIntensity.w, baseColor, roughness)
        : EvaluatePbrLight(normal, viewDirection, lightDirection,
                           lightColorIntensity.rgb * lightColorIntensity.w, baseColor, roughness, metallic);
    // 基礎 PBR 與角色補光共用同一次級聯陰影，避免 Skin/Hair 每像素重複取樣陰影圖。
    const float directionalShadow=CascadedDirectionalShadow(input,eyeMask>0.5?geometricNormal:normal,lightDirection);
    float3 shaded = environmentLighting
                  + directionalShadow * directionalLighting
                  + baseColor * emissive * 2.0;
    [branch] if(skinMask > 0.5){
        shaded+=directionalShadow*EvaluateSkinSupplement(normal,viewDirection,lightDirection,
                    lightColorIntensity.rgb*lightColorIntensity.w,baseColor,roughness);
    }
    [branch] if(hairMask>0.5){
        shaded+=directionalShadow*EvaluateHairSupplement(normal,input.tangent.xyz,viewDirection,lightDirection,
                    lightColorIntensity.rgb*lightColorIntensity.w,baseColor,roughness);
    }
    [branch] if (foliageMask > 0.5)
    {
        const float foliageWrappedDiffuse = saturate((dot(normal, lightDirection) + 0.35) / 1.35);
        const float foliageBackLight = saturate(dot(-normal, lightDirection));
        const float foliageForwardScatter = saturate(dot(viewDirection, lightDirection));
        const float foliageTransmission = foliageBackLight * foliageBackLight * (0.45 + 0.55 * foliageForwardScatter);
        const float foliageShadow = CascadedDirectionalShadow(input, normal, lightDirection);
        shaded += baseColor * lightColorIntensity.rgb * lightColorIntensity.w * (foliageWrappedDiffuse * 0.22 + foliageTransmission * 0.48) * foliageShadow;
    }

    [unroll]
    for (uint pointIndex = 0; pointIndex < 3; ++pointIndex)
    {
        if (pointIndex >= (uint)fogEndEnabled.z)
            break;
        const float3 delta = pointPositionRadius[pointIndex].xyz - input.worldPosition;
        const float distanceSquared = max(0.0001, dot(delta, delta));
        const float lightDistance = sqrt(distanceSquared);
        const float attenuation = PointLightAttenuation(distanceSquared, pointPositionRadius[pointIndex].w);
        const float3 localDirection = delta / lightDistance;
        const float3 pointRadiance = pointColorIntensity[pointIndex].rgb * pointColorIntensity[pointIndex].w * attenuation;
        shaded += eyeMask > 0.5
                ? EvaluateEyeLight(normal, geometricNormal, viewDirection, localDirection, pointRadiance, baseColor, roughness)
                : EvaluatePbrLight(normal, viewDirection, localDirection, pointRadiance, baseColor, roughness, metallic);
        if(skinMask>0.5)shaded+=EvaluateSkinSupplement(normal,viewDirection,localDirection,
                    pointColorIntensity[pointIndex].rgb*pointColorIntensity[pointIndex].w*attenuation,
                    baseColor,roughness);
        if(hairMask>0.5)shaded+=EvaluateHairSupplement(normal,input.tangent.xyz,viewDirection,localDirection,
                    pointColorIntensity[pointIndex].rgb*pointColorIntensity[pointIndex].w*attenuation,
                    baseColor,roughness);
    }

    [branch] if (waterMask > 0.5)
    {
        const float waterViewCosine = saturate(dot(normal, viewDirection));
        const float waterReflectance = 0.0203732;
        const float fresnel = waterReflectance + (1.0 - waterReflectance) * pow(1.0 - waterViewCosine, 5.0);
        const float waterPathLength = min(8.0, waterThickness / max(0.16, waterViewCosine));
        const float3 waterTransmission = exp(-float3(0.42, 0.16, 0.08) * waterPathLength);
        const float3 deepWaterColor = lerp(float3(0.012, 0.055, 0.070), baseColor, 0.24);
        const float3 absorbedWater = shaded * waterTransmission + deepWaterColor * (1.0 - waterTransmission);
        const float waterReflectionRoughness = clamp(roughness, 0.045, 0.18);
        const float3 reflectedWater = EvaluateAnalyticSkySpecular(reflect(-viewDirection, normal), skyLightDirection, waterReflectionRoughness)
                                    * lerp(1.0, 0.55, waterReflectionRoughness);
        float3 waterComposite = lerp(absorbedWater, reflectedWater, fresnel);
        [branch] if (foamMask > 0.01)
        {
            // 岸線泡沫使用有界程序破碎圖樣，避免整條河岸成為均勻發光帶。
            const float2 foamFlow = input.worldPosition.xz * 0.42 + float2(fogEndEnabled.w * 0.07, -fogEndEnabled.w * 0.04);
            const float foamNoise = CloudNoise(foamFlow);
            const float foamWave = 0.5 + 0.5 * sin(input.worldPosition.z * 1.35 + input.worldPosition.x * 0.31 + fogEndEnabled.w * 1.8);
            const float foamCoverage = smoothstep(0.34, 0.76, foamNoise * 0.72 + foamWave * 0.28);
            const float3 foamColor = baseColor * lerp(0.58, 0.96, foamCoverage)
                                   + EvaluateAnalyticSkyDiffuse(normal, skyLightDirection) * 0.055;
            waterComposite = lerp(waterComposite, foamColor, foamMask * lerp(0.42, 0.88, foamCoverage));
        }
        shaded = waterComposite;
    }
    const float3 lit = lerp(baseColor, shaded, saturate(input.normal.w));
    const float cameraDistance = distance(input.worldPosition, cameraFogStart.xyz);
    const float distanceFog = saturate((cameraDistance - cameraFogStart.w) / max(0.001, fogEndEnabled.x - cameraFogStart.w));
    const float horizonFog = pow(saturate(1.0 - abs(normalize(input.worldPosition - cameraFogStart.xyz).y)), 4.0) * saturate(cameraDistance / max(1.0, fogEndEnabled.x));
    const float fogAmount = saturate(max(distanceFog, horizonFog * 0.72)) * fogEndEnabled.y * fogColor.a * saturate(input.normal.w);
    // 世界著色器只輸出曝光後的線性 HDR；ACES 與 sRGB 由全畫面後處理統一執行。
    float outputAlpha = input.materialClass == 3 ? 1.0 : lerp(input.color.a, waterOutputAlpha, waterMask * waterDepthValid);
    if(alphaToCoverage && input.alphaMasked != 0)
        outputAlpha = MaskedAlphaToCoverage(materialAlpha, input.alphaCutoff);
    return float4(max(0.0, lerp(lit, fogColor.rgb, fogAmount) * colorPipeline.x), outputAlpha);
}

float4 WorldPixelMain(WorldPixelInput input, bool isFrontFace : SV_IsFrontFace) : SV_TARGET
{
    return EvaluateWorldPixel(input, isFrontFace, false);
}

float4 WorldMaskedPixelMain(WorldPixelInput input, bool isFrontFace : SV_IsFrontFace) : SV_TARGET
{
    return EvaluateWorldPixel(input, isFrontFace, true);
}
