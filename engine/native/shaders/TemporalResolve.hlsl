Texture2D<float4> currentSceneColor : register(t10);
Texture2D<float4> previousSceneColor : register(t14);
Texture2D<float2> sceneVelocity : register(t15);
SamplerState linearClampSampler : register(s0);

cbuffer TemporalComposite : register(b0)
{
    float4 sourceRect;
    float4 targetRect;
    float4 temporalParameters;
};

struct TemporalVertexOutput { float4 position : SV_POSITION; };

TemporalVertexOutput TemporalResolveVertexMain(uint vertexId : SV_VertexID)
{
    TemporalVertexOutput output;
    const float2 position = vertexId == 0 ? float2(-1.0, -1.0) : vertexId == 1 ? float2(-1.0, 3.0) : float2(3.0, -1.0);
    output.position = float4(position, 0.0, 1.0);
    return output;
}

float TemporalLuminance(float3 color)
{
    return dot(max(color, 0.0), float3(0.2126, 0.7152, 0.0722));
}

float4 TemporalResolvePixelMain(TemporalVertexOutput input) : SV_TARGET
{
    uint width, height;
    currentSceneColor.GetDimensions(width, height);
    const float2 texel = temporalParameters.xy;
    const float2 normalizedPosition = saturate((input.position.xy - targetRect.xy) / max(targetRect.zw, 1.0));
    const float2 sourceUv = (sourceRect.xy + normalizedPosition * sourceRect.zw) / float2(width, height);
    const float3 center = currentSceneColor.SampleLevel(linearClampSampler, sourceUv, 0.0).rgb;
    if (temporalParameters.z < 0.5)
        return float4(center, 1.0);

    const float3 north = currentSceneColor.SampleLevel(linearClampSampler, sourceUv - float2(0.0, texel.y), 0.0).rgb;
    const float3 south = currentSceneColor.SampleLevel(linearClampSampler, sourceUv + float2(0.0, texel.y), 0.0).rgb;
    const float3 west = currentSceneColor.SampleLevel(linearClampSampler, sourceUv - float2(texel.x, 0.0), 0.0).rgb;
    const float3 east = currentSceneColor.SampleLevel(linearClampSampler, sourceUv + float2(texel.x, 0.0), 0.0).rgb;
    const float3 neighborhoodMinimum = min(center, min(min(north, south), min(west, east)));
    const float3 neighborhoodMaximum = max(center, max(max(north, south), max(west, east)));
    const float2 velocityPixels = sceneVelocity.SampleLevel(linearClampSampler, sourceUv, 0.0);
    const float2 previousUv = sourceUv - velocityPixels * texel;
    const float inside = all(previousUv >= 0.0) && all(previousUv <= 1.0) ? 1.0 : 0.0;
    float3 history = previousSceneColor.SampleLevel(linearClampSampler, saturate(previousUv), 0.0).rgb;
    history = clamp(history, neighborhoodMinimum, neighborhoodMaximum);
    const float luminanceDelta = abs(TemporalLuminance(center) - TemporalLuminance(history)) / max(TemporalLuminance(center), 0.05);
    const float confidence = saturate(1.0 - luminanceDelta * 1.5) * inside;
    return float4(lerp(center, history, temporalParameters.w * confidence), 1.0);
}
