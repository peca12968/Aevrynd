Texture2D<float4> hdrSceneColor : register(t10);
SamplerState hdrSampler : register(s0);
cbuffer Composite : register(b0)
{
    float4 sourceRect;
    float4 targetRect;
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

float4 PostProcessPixelMain(PostProcessVertexOutput input) : SV_TARGET
{
    // 場景先在 FP16 線性空間完成透明混合；此處只執行一次顯示轉換。
    uint sourceWidth, sourceHeight;
    hdrSceneColor.GetDimensions(sourceWidth, sourceHeight);
    const float2 normalizedPosition = saturate((input.position.xy - targetRect.xy) / max(targetRect.zw, 1.0));
    const float2 sourcePosition = sourceRect.xy + normalizedPosition * sourceRect.zw;
    const float3 hdr = hdrSceneColor.SampleLevel(hdrSampler, sourcePosition / float2(sourceWidth, sourceHeight), 0.0).rgb;
    return float4(LinearToSrgb(AcesToneMap(hdr)), 1.0);
}
