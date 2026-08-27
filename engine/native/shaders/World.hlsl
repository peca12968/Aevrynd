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
};
cbuffer ShadowScene : register(b1) { row_major float4x4 lightViewProjection; float4 shadowParameters; };

Texture2D<float> shadowTexture : register(t3);
SamplerComparisonState shadowSampler : register(s1);

Texture2D baseColorTexture : register(t0);
Texture2D roughnessTexture : register(t1);
Texture2D normalTexture : register(t2);
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
};

struct ShadowVertexOutput { float4 position : SV_POSITION; };
float3 ApplyWind(float3 position, float packedWeight);

ShadowVertexOutput ShadowWorldVertexMain(WorldVertexInput input)
{
    ShadowVertexOutput output;
    output.position = mul(lightViewProjection, float4(ApplyWind(input.position, input.normal.w), 1.0));
    return output;
}

ShadowVertexOutput ShadowInstancedVertexMain(InstancedVertexInput input)
{
    const float4 localPosition = float4(input.position, 1.0);
    float4 worldPosition = float4(dot(input.transformRow0, localPosition), dot(input.transformRow1, localPosition), dot(input.transformRow2, localPosition), dot(input.transformRow3, localPosition));
    worldPosition.xyz = ApplyWind(worldPosition.xyz, input.normal.w);
    ShadowVertexOutput output;
    output.position = mul(lightViewProjection, worldPosition);
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
    WorldPixelInput output;
    const float3 animatedPosition = ApplyWind(input.position, input.normal.w);
    output.position = mul(viewProjection, float4(animatedPosition, 1.0));
    output.color = input.color;
    output.worldPosition = animatedPosition;
    output.normal = float4(input.normal.xyz, input.normal.w < 0.0 ? 0.0 : 1.0);
    output.material = float4(0.72, 0.0, 0.0, 1.0);
    output.textureData = float3(0.0, 0.25, 0.0);
    output.uv = input.uv;
    output.materialClass = 0;
    output.shadowPosition = mul(lightViewProjection, float4(animatedPosition, 1.0));
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
    WorldPixelInput output;
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
    output.shadowPosition = mul(lightViewProjection, worldPosition);
    return output;
}

float3 BlendWeights(float3 normal)
{
    const float3 blend = pow(abs(normal), 4.0);
    return blend / max(0.001, blend.x + blend.y + blend.z);
}

float DirectionalShadow(float4 shadowPosition, float3 normal, float3 lightDirection)
{
    const float3 projected = shadowPosition.xyz / max(0.0001, shadowPosition.w);
    const float2 uv = float2(projected.x * 0.5 + 0.5, -projected.y * 0.5 + 0.5);
    if (shadowParameters.x < 0.5 || projected.z <= 0.0 || projected.z >= 1.0 || any(uv < 0.0) || any(uv > 1.0)) return 1.0;
    const float bias = max(shadowParameters.z, shadowParameters.y * (1.0 - saturate(dot(normal, lightDirection))));
    float visibility = 0.0;
    [unroll] for (int y = -1; y <= 1; ++y)
    [unroll] for (int x = -1; x <= 1; ++x)
        visibility += shadowTexture.SampleCmpLevelZero(shadowSampler, uv + float2(x, y) * shadowParameters.w, projected.z - bias);
    return visibility / 9.0;
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

float3 TriplanarNormal(float3 worldPosition, float3 geometricNormal, float scale)
{
    const float3 blend = BlendWeights(geometricNormal);
    const float3 axisSign = sign(geometricNormal);
    const float3 x = DecodeBc5Normal(normalTexture.Sample(materialSampler, worldPosition.zy * scale).rg);
    const float3 y = DecodeBc5Normal(normalTexture.Sample(materialSampler, worldPosition.xz * scale).rg);
    const float3 z = DecodeBc5Normal(normalTexture.Sample(materialSampler, worldPosition.xy * scale).rg);
    const float3 mappedX = float3(axisSign.x * x.z, x.y, x.x);
    const float3 mappedY = float3(y.x, axisSign.y * y.z, y.y);
    const float3 mappedZ = float3(z.x, z.y, axisSign.z * z.z);
    return normalize(mappedX * blend.x + mappedY * blend.y + mappedZ * blend.z);
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

float3 AcesToneMap(float3 color)
{
    color = max(color, 0.0) * 1.1;
    return saturate((color * (2.51 * color + 0.03)) / (color * (2.43 * color + 0.59) + 0.14));
}

float3 LinearToSrgb(float3 linearColor)
{
    linearColor = saturate(linearColor);
    const float3 low = linearColor * 12.92;
    const float3 high = 1.055 * pow(linearColor, 1.0 / 2.4) - 0.055;
    return float3(linearColor.x <= 0.0031308 ? low.x : high.x,
                  linearColor.y <= 0.0031308 ? low.y : high.y,
                  linearColor.z <= 0.0031308 ? low.z : high.z);
}

float4 WorldPixelMain(WorldPixelInput input) : SV_TARGET
{
    float3 normal = normalize(input.normal.xyz);
    float roughness = clamp(input.material.x, 0.045, 1.0);
    const float metallic = saturate(input.material.y);
    const float emissive = saturate(input.material.z);
    const float occlusion = saturate(input.material.w);
    float3 baseColor = input.color.rgb;
    if (input.textureData.x > 0.5)
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

    const float skyMask = 1.0 - saturate(input.normal.w);
    const float3 skyRay = normalize(input.worldPosition - cameraFogStart.xyz);
    const float3 skyLightDirection = -normalize(lightDirectionAmbient.xyz);
    const float sunDisk = pow(saturate(dot(skyRay, skyLightDirection)), 640.0);
    const float sunGlow = pow(saturate(dot(skyRay, skyLightDirection)), 18.0);
    const float horizonGlow = pow(saturate(1.0 - abs(skyRay.y)), 5.0);
    baseColor += skyMask * (float3(1.0, 0.76, 0.42) * sunDisk * 2.8
                         + float3(1.0, 0.52, 0.24) * sunGlow * 0.24
                         + float3(0.34, 0.20, 0.12) * horizonGlow * 0.16);

    // 材質類別明確決定水面；法線門檻僅避免垂直側面產生水平水波。
    const float waterMask = input.materialClass == 1 ? step(0.35, normal.y) : 0.0;
    const float2 ripple = float2(sin(input.worldPosition.z * 0.82 + input.worldPosition.x * 0.17 + fogEndEnabled.w * 1.35),
                                 cos(input.worldPosition.x * 0.68 - input.worldPosition.z * 0.23 - fogEndEnabled.w * 1.08));
    normal = normalize(normal + float3(ripple.x, 0.0, ripple.y) * waterMask * 0.075);

    const float3 lightDirection = -normalize(lightDirectionAmbient.xyz);
    const float3 viewDirection = normalize(cameraFogStart.xyz - input.worldPosition);
    const float3 reflectance = lerp(float3(0.04, 0.04, 0.04), baseColor, metallic);
    const float skyVisibility = saturate(normal.y * 0.5 + 0.5);
    const float3 skyBounce = lerp(float3(0.22, 0.17, 0.12), float3(0.38, 0.52, 0.68), skyVisibility);
    const float3 ambientFresnel = FresnelSchlickRoughness(saturate(dot(normal, viewDirection)), reflectance, roughness);
    const float3 ambientDiffuseWeight = (1.0 - ambientFresnel) * (1.0 - metallic);
    float3 shaded = ambientDiffuseWeight * baseColor * skyBounce * lightDirectionAmbient.w * occlusion * 2.8
                  + ambientFresnel * skyBounce * lightDirectionAmbient.w * occlusion * (0.3 + (1.0 - roughness) * 0.35)
                  + DirectionalShadow(input.shadowPosition, normal, lightDirection) * EvaluatePbrLight(normal, viewDirection, lightDirection,
                                     lightColorIntensity.rgb * lightColorIntensity.w,
                                     baseColor, roughness, metallic)
                  + baseColor * emissive * 2.0;

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

    const float fresnel = pow(1.0 - saturate(dot(normal, viewDirection)), 5.0);
    shaded = lerp(shaded, float3(0.24, 0.48, 0.62) + fresnel * 0.7, waterMask * (0.18 + fresnel * 0.46));
    const float3 lit = lerp(baseColor, shaded, saturate(input.normal.w));
    const float fogAmount = saturate((distance(input.worldPosition, cameraFogStart.xyz) - cameraFogStart.w)
                          / max(0.001, fogEndEnabled.x - cameraFogStart.w)) * fogEndEnabled.y * fogColor.a * saturate(input.normal.w);
    // World shader 的材質、天空、水與霧均在線性空間合成；UNORM RTV 前需明確轉為 sRGB。
    return float4(LinearToSrgb(AcesToneMap(lerp(lit, fogColor.rgb, fogAmount))), input.color.a);
}
