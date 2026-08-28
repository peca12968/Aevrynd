Texture2D<float4> hdrSceneColor : register(t10);
SamplerState hdrSampler : register(s0);
cbuffer Composite : register(b0)
{
    float4 sourceRect;
    float4 targetRect;
    float4 antiAliasing;
};

struct PostProcessVertexOutput
{
    float4 position : SV_POSITION;
};

PostProcessVertexOutput PostProcessVertexMain(uint vertexId : SV_VertexID)
{
    PostProcessVertexOutput output;
    const float2 position = vertexId == 0 ? float2(-1.0, -1.0) : vertexId == 1 ? float2(-1.0, 3.0) : float2(3.0, -1.0);
    output.position = float4(position, 0.0, 1.0);
    return output;
}

float3 AcesToneMap(float3 color)
{
    color = max(color, 0.0) * 1.1;
    return saturate((color * (2.51 * color + 0.03)) / (color * (2.43 * color + 0.59) + 0.14));
}

float3 LinearToSrgb(float3 color)
{
    color = saturate(color);
    const float3 low = color * 12.92;
    const float3 high = 1.055 * pow(color, 1.0 / 2.4) - 0.055;
    return lerp(high, low, step(color, 0.0031308));
}

float LogLuminance(float3 color)
{
    return log2(1.0 + dot(max(color, 0.0), float3(0.2126, 0.7152, 0.0722)));
}

float3 EdgeAdaptiveHdr(float2 uv, float3 center)
{
    if (antiAliasing.z < 0.5)
        return center;
    const float2 texel = antiAliasing.xy;
    const float3 north = hdrSceneColor.SampleLevel(hdrSampler, uv - float2(0.0, texel.y), 0.0).rgb;
    const float3 south = hdrSceneColor.SampleLevel(hdrSampler, uv + float2(0.0, texel.y), 0.0).rgb;
    const float3 west = hdrSceneColor.SampleLevel(hdrSampler, uv - float2(texel.x, 0.0), 0.0).rgb;
    const float3 east = hdrSceneColor.SampleLevel(hdrSampler, uv + float2(texel.x, 0.0), 0.0).rgb;
    const float lumaCenter = LogLuminance(center);
    const float lumaNorth = LogLuminance(north), lumaSouth = LogLuminance(south);
    const float lumaWest = LogLuminance(west), lumaEast = LogLuminance(east);
    const float minimumLuma = min(lumaCenter, min(min(lumaNorth, lumaSouth), min(lumaWest, lumaEast)));
    const float maximumLuma = max(lumaCenter, max(max(lumaNorth, lumaSouth), max(lumaWest, lumaEast)));
    const float range = maximumLuma - minimumLuma;
    if (range < max(0.025, maximumLuma * antiAliasing.w))
        return center;
    const float horizontalGradient = abs(lumaNorth + lumaSouth - 2.0 * lumaCenter);
    const float verticalGradient = abs(lumaWest + lumaEast - 2.0 * lumaCenter);
    const float3 edgeAverage = horizontalGradient >= verticalGradient ? (north + south) * 0.5 : (west + east) * 0.5;
    const float blend = saturate(range / max(maximumLuma, 0.0001) * 0.65);
    return lerp(center, edgeAverage, blend);
}

float4 PostProcessPixelMain(PostProcessVertexOutput input) : SV_TARGET
{
    // 場景先在 FP16 線性空間完成透明混合；此處只執行一次顯示轉換。
    uint sourceWidth, sourceHeight;
    hdrSceneColor.GetDimensions(sourceWidth, sourceHeight);
    const float2 normalizedPosition = saturate((input.position.xy - targetRect.xy) / max(targetRect.zw, 1.0));
    const float2 sourcePosition = sourceRect.xy + normalizedPosition * sourceRect.zw;
    const float2 sourceUv = sourcePosition / float2(sourceWidth, sourceHeight);
    const float3 hdrCenter = hdrSceneColor.SampleLevel(hdrSampler, sourceUv, 0.0).rgb;
    const float3 hdr = EdgeAdaptiveHdr(sourceUv, hdrCenter);
    return float4(LinearToSrgb(AcesToneMap(hdr)), 1.0);
}
