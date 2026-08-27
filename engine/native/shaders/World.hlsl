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
};
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
SamplerState materialSampler : register(s0);

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
};

struct ShadowVertexOutput { float4 position : SV_POSITION; };
float3 ApplyWind(float3 position, float packedWeight);

ShadowVertexOutput ShadowWorldVertexMain(WorldVertexInput input)
{
    ShadowVertexOutput output;
    output.position = mul(lightViewProjection[shadowCascadeIndex], float4(ApplyWind(input.position, input.normal.w), 1.0));
    return output;
}

ShadowVertexOutput ShadowInstancedVertexMain(InstancedVertexInput input)
{
    const float4 localPosition = float4(input.position, 1.0);
    float4 worldPosition = float4(dot(input.transformRow0, localPosition), dot(input.transformRow1, localPosition), dot(input.transformRow2, localPosition), dot(input.transformRow3, localPosition));
    worldPosition.xyz = ApplyWind(worldPosition.xyz, input.normal.w);
    ShadowVertexOutput output;
    output.position = mul(lightViewProjection[shadowCascadeIndex], worldPosition);
    return output;
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
    output.shadowPosition = mul(lightViewProjection[0], float4(animatedPosition, 1.0));
    output.farShadowPosition = mul(lightViewProjection[1], float4(animatedPosition, 1.0));
    return output;
}

WorldPixelInput InstancedVertexMain(InstancedVertexInput input)
{
    const float4 localPosition = float4(input.position, 1.0);
    float4 worldPosition = float4(dot(input.transformRow0, localPosition),
                                  dot(input.transformRow1, localPosition),
                                  dot(input.transformRow2, localPosition),
                                  dot(input.transformRow3, localPosition));
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

float PointLightAttenuation(float distanceSquared, float radius)
{
    const float safeRadius = max(radius, 0.001);
    const float normalizedDistanceSquared = distanceSquared / (safeRadius * safeRadius);
    const float rangeWindow = saturate(1.0 - normalizedDistanceSquared * normalizedDistanceSquared);
    // 半徑只控制平滑截止；分母的軟化項避免光源中心產生無限亮度。
    return rangeWindow * rangeWindow / max(distanceSquared + 1.0, 0.0001);
}

float4 WorldPixelMain(WorldPixelInput input, bool isFrontFace : SV_IsFrontFace) : SV_TARGET
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
    const float metallic = saturate(input.material.y);
    const float emissive = saturate(input.material.z);
    const float occlusion = saturate(input.material.w);
    float3 baseColor = input.color.rgb;
    if (input.textureData.x > 0.5 && input.materialClass != 3)
    {
        if (input.textureData.z > 0.5)
        {
            baseColor *= baseColorTexture.Sample(materialSampler, input.uv).rgb;
            roughness = clamp(roughness * roughnessTexture.Sample(materialSampler, input.uv).r, 0.045, 1.0);
            normal = normalize(lerp(normal, UvNormal(input.worldPosition, normal, input.uv), 0.78));
        }
        else
        {
            baseColor *= Triplanar(baseColorTexture, input.worldPosition, normal, input.textureData.y).rgb;
            roughness = clamp(roughness * Triplanar(roughnessTexture, input.worldPosition, normal, input.textureData.y).r, 0.045, 1.0);
            normal = normalize(lerp(normal, TriplanarNormal(input.worldPosition, normal, input.textureData.y), 0.78));
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
        return float4(max(0.0, baseColor * colorPipeline.x), input.color.a);
    }

    // 材質類別明確決定水面；法線門檻僅避免垂直側面產生水平水波。
    const float waterMask = input.materialClass == 1 ? step(0.35, normal.y) : 0.0;
    const float foamMask = waterMask * smoothstep(0.30, 0.50, roughness);
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
    float3 shaded = EvaluateEnvironmentLighting(normal, viewDirection, baseColor, roughness, metallic,
                                                occlusion * screenOcclusion, lightDirectionAmbient.w, skyLightDirection)
                  + CascadedDirectionalShadow(input, normal, lightDirection) * EvaluatePbrLight(normal, viewDirection, lightDirection,
                                     lightColorIntensity.rgb * lightColorIntensity.w,
                                     baseColor, roughness, metallic)
                  + baseColor * emissive * 2.0;
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
        shaded += EvaluatePbrLight(normal, viewDirection, localDirection,
                                   pointColorIntensity[pointIndex].rgb * pointColorIntensity[pointIndex].w * attenuation,
                                   baseColor, roughness, metallic);
    }

    [branch] if (waterMask > 0.5)
    {
        const float waterViewCosine = saturate(dot(normal, viewDirection));
        const float waterReflectance = 0.0203732;
        const float fresnel = waterReflectance + (1.0 - waterReflectance) * pow(1.0 - waterViewCosine, 5.0);
        const float waterPathLength = min(8.0, 1.6 / max(0.16, waterViewCosine));
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
    const float outputAlpha = input.materialClass == 3 ? 1.0 : input.color.a;
    return float4(max(0.0, lerp(lit, fogColor.rgb, fogAmount) * colorPipeline.x), outputAlpha);
}
