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

Texture2D baseColorTexture : register(t0);
Texture2D roughnessTexture : register(t1);
Texture2D normalTexture : register(t2);
SamplerState materialSampler : register(s0);

struct WorldVertexInput
{
    float3 position : POSITION;
    float4 normal : NORMAL;
    float4 color : COLOR;
};

struct InstancedVertexInput
{
    float3 position : POSITION;
    float4 normal : NORMAL;
    float4 color : COLOR;
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
    float2 textureData : TEXCOORD3;
};

WorldPixelInput WorldVertexMain(WorldVertexInput input)
{
    WorldPixelInput output;
    output.position = mul(viewProjection, float4(input.position, 1.0));
    output.color = input.color;
    output.worldPosition = input.position;
    output.normal = input.normal;
    output.material = float4(0.72, 0.0, 0.0, 1.0);
    output.textureData = float2(0.0, 0.25);
    return output;
}

WorldPixelInput InstancedVertexMain(InstancedVertexInput input)
{
    const float4 localPosition = float4(input.position, 1.0);
    const float4 worldPosition = float4(dot(input.transformRow0, localPosition),
                                        dot(input.transformRow1, localPosition),
                                        dot(input.transformRow2, localPosition),
                                        dot(input.transformRow3, localPosition));
    const float3 worldNormal = float3(dot(input.normalRow0.xyz, input.normal.xyz),
                                      dot(input.normalRow1.xyz, input.normal.xyz),
                                      dot(input.normalRow2.xyz, input.normal.xyz));
    WorldPixelInput output;
    output.position = mul(viewProjection, worldPosition);
    output.color = float4(input.color.rgb * input.tint.rgb, input.color.a * input.tint.a);
    output.worldPosition = worldPosition.xyz;
    output.normal = float4(normalize(worldNormal), input.normal.w);
    output.material = input.material;
    output.textureData = float2(input.textureData & 1,
                                max(0.001, float((input.textureData >> 16) & 65535) * (16.0 / 65535.0)));
    return output;
}

float3 BlendWeights(float3 normal)
{
    const float3 blend = pow(abs(normal), 4.0);
    return blend / max(0.001, blend.x + blend.y + blend.z);
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

float4 WorldPixelMain(WorldPixelInput input) : SV_TARGET
{
    float3 normal = normalize(input.normal.xyz);
    float roughness = saturate(input.material.x);
    const float metallic = saturate(input.material.y);
    const float emissive = saturate(input.material.z);
    const float occlusion = saturate(input.material.w);
    float3 baseColor = input.color.rgb;
    if (input.textureData.x > 0.5)
    {
        baseColor *= Triplanar(baseColorTexture, input.worldPosition, normal, input.textureData.y).rgb;
        roughness = saturate(roughness * Triplanar(roughnessTexture, input.worldPosition, normal, input.textureData.y).r);
        normal = normalize(lerp(normal, TriplanarNormal(input.worldPosition, normal, input.textureData.y), 0.78));
    }

    const float waterMask = saturate((0.32 - roughness) * 5.0) * step(0.62, normal.y) * saturate((1.0 - input.color.a) * 6.0);
    const float2 ripple = float2(sin(input.worldPosition.z * 0.82 + input.worldPosition.x * 0.17),
                                 cos(input.worldPosition.x * 0.68 - input.worldPosition.z * 0.23));
    normal = normalize(normal + float3(ripple.x, 0.0, ripple.y) * waterMask * 0.075);

    const float3 lightDirection = -normalize(lightDirectionAmbient.xyz);
    const float3 viewDirection = normalize(cameraFogStart.xyz - input.worldPosition);
    const float3 halfDirection = normalize(lightDirection + viewDirection);
    const float diffuse = saturate((dot(normal, lightDirection) + 0.18) / 1.18);
    const float specular = pow(saturate(dot(normal, halfDirection)), lerp(128.0, 4.0, roughness)) * (1.0 - roughness * 0.65);
    const float3 diffuseColor = baseColor * (1.0 - metallic * 0.72);
    const float3 specularColor = lerp(float3(0.04, 0.04, 0.04), baseColor, metallic);
    const float skyVisibility = saturate(normal.y * 0.5 + 0.5);
    const float3 skyBounce = lerp(float3(0.22, 0.17, 0.12), float3(0.38, 0.52, 0.68), skyVisibility);
    const float backScatter = pow(saturate(dot(-lightDirection, viewDirection)), 3.0) * saturate(1.0 - roughness);
    float3 shaded = diffuseColor * (lightDirectionAmbient.w * occlusion * skyBounce * 2.8 + lightColorIntensity.rgb * lightColorIntensity.w * diffuse)
                  + specularColor * lightColorIntensity.rgb * lightColorIntensity.w * specular
                  + baseColor * emissive * 2.0
                  + lightColorIntensity.rgb * backScatter * 0.08;

    [unroll]
    for (uint pointIndex = 0; pointIndex < 3; ++pointIndex)
    {
        if (pointIndex >= (uint)fogEndEnabled.z)
            break;
        const float3 delta = pointPositionRadius[pointIndex].xyz - input.worldPosition;
        const float lightDistance = max(0.001, length(delta));
        const float attenuation = saturate(1.0 - lightDistance / pointPositionRadius[pointIndex].w);
        const float3 localDirection = delta / lightDistance;
        const float3 localHalf = normalize(localDirection + viewDirection);
        const float localDiffuse = saturate(dot(normal, localDirection));
        const float localSpecular = pow(saturate(dot(normal, localHalf)), lerp(128.0, 4.0, roughness)) * (1.0 - roughness * 0.65);
        shaded += (diffuseColor * localDiffuse + specularColor * localSpecular)
                * pointColorIntensity[pointIndex].rgb * pointColorIntensity[pointIndex].w
                * attenuation * attenuation;
    }

    const float fresnel = pow(1.0 - saturate(dot(normal, viewDirection)), 5.0);
    shaded = lerp(shaded, float3(0.24, 0.48, 0.62) + fresnel * 0.7, waterMask * (0.18 + fresnel * 0.46));
    const float3 lit = lerp(baseColor, shaded, saturate(input.normal.w));
    const float fogAmount = saturate((distance(input.worldPosition, cameraFogStart.xyz) - cameraFogStart.w)
                          / max(0.001, fogEndEnabled.x - cameraFogStart.w)) * fogEndEnabled.y * fogColor.a;
    return float4(lerp(lit, fogColor.rgb, fogAmount), input.color.a);
}
