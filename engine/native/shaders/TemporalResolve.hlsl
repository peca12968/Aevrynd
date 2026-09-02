Texture2D<float4> currentSceneColor : register(t10);
Texture2D<float4> previousSceneColor : register(t14);
Texture2D<float2> sceneVelocity : register(t15);
Texture2D<float> currentSceneDepth : register(t16);
Texture2D<float> previousSceneDepth : register(t17);
SamplerState linearClampSampler : register(s0);

cbuffer TemporalComposite : register(b0)
{
    float4 sourceRect;
    float4 targetRect;
    float4 temporalParameters;
    float4 jitterPixels;
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

bool TemporalDepthCompatible(float centerDepth, float sampleDepth, bool legacyLooseSky)
{
    // 深度以 Load 讀取 D32，standard-Z 的 clear 值才是天空；任何小於 1 的值皆為幾何。
    const float skyThreshold = legacyLooseSky ? 0.9995 : 1.0;
    const bool centerSky = centerDepth >= skyThreshold;
    const bool sampleSky = sampleDepth >= skyThreshold;
    if (centerSky != sampleSky)
        return false;
    if (centerSky)
        return true;
    // 目前引擎使用 clear=1 的透視深度；以離 clear plane 的距離正規化，遠近景都有一致門檻。
    const float depthScale = max(max(1.0 - centerDepth, 1.0 - sampleDepth), 0.0001);
    return abs(centerDepth - sampleDepth) <= max(0.00001, depthScale * 0.035);
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

    const bool legacyCurrentDepth = temporalParameters.z >= 1.5 && temporalParameters.z < 2.5;
    const bool legacyLooseSky = temporalParameters.z >= 2.5;
    const int2 maximumPixel = int2(width, height) - 1;
    const int2 centerPixel = clamp(int2(sourceUv * float2(width, height)), int2(0, 0), maximumPixel);
    const float centerDepth = currentSceneDepth.Load(int3(centerPixel, 0));

    const float3 north = currentSceneColor.SampleLevel(linearClampSampler, sourceUv - float2(0.0, texel.y), 0.0).rgb;
    const float3 south = currentSceneColor.SampleLevel(linearClampSampler, sourceUv + float2(0.0, texel.y), 0.0).rgb;
    const float3 west = currentSceneColor.SampleLevel(linearClampSampler, sourceUv - float2(texel.x, 0.0), 0.0).rgb;
    const float3 east = currentSceneColor.SampleLevel(linearClampSampler, sourceUv + float2(texel.x, 0.0), 0.0).rgb;
    float3 neighborhoodMinimum = center;
    float3 neighborhoodMaximum = center;
    if (TemporalDepthCompatible(centerDepth, currentSceneDepth.Load(int3(clamp(centerPixel + int2(0, -1), int2(0, 0), maximumPixel), 0)), legacyLooseSky)) { neighborhoodMinimum = min(neighborhoodMinimum, north); neighborhoodMaximum = max(neighborhoodMaximum, north); }
    if (TemporalDepthCompatible(centerDepth, currentSceneDepth.Load(int3(clamp(centerPixel + int2(0, 1), int2(0, 0), maximumPixel), 0)), legacyLooseSky)) { neighborhoodMinimum = min(neighborhoodMinimum, south); neighborhoodMaximum = max(neighborhoodMaximum, south); }
    if (TemporalDepthCompatible(centerDepth, currentSceneDepth.Load(int3(clamp(centerPixel + int2(-1, 0), int2(0, 0), maximumPixel), 0)), legacyLooseSky)) { neighborhoodMinimum = min(neighborhoodMinimum, west); neighborhoodMaximum = max(neighborhoodMaximum, west); }
    if (TemporalDepthCompatible(centerDepth, currentSceneDepth.Load(int3(clamp(centerPixel + int2(1, 0), int2(0, 0), maximumPixel), 0)), legacyLooseSky)) { neighborhoodMinimum = min(neighborhoodMinimum, east); neighborhoodMaximum = max(neighborhoodMaximum, east); }
    const float2 velocityPixels = sceneVelocity.SampleLevel(linearClampSampler, sourceUv, 0.0);
    const float2 previousUv = sourceUv - velocityPixels * texel + (jitterPixels.zw - jitterPixels.xy) * texel;
    const float inside = all(previousUv >= 0.0) && all(previousUv <= 1.0) ? 1.0 : 0.0;
    float3 history = previousSceneColor.SampleLevel(linearClampSampler, saturate(previousUv), 0.0).rgb;
    history = clamp(history, neighborhoodMinimum, neighborhoodMaximum);
    const float luminanceDelta = abs(TemporalLuminance(center) - TemporalLuminance(history)) / max(TemporalLuminance(center), 0.05);
    const int2 previousPixel = clamp(int2(saturate(previousUv) * float2(width, height)), int2(0, 0), maximumPixel);
    // 生產路徑必須比較前幀深度；負向控制保留舊版錯誤的當幀重投影深度。
    const float reprojectedDepth = legacyCurrentDepth ? currentSceneDepth.Load(int3(previousPixel, 0)) : previousSceneDepth.Load(int3(previousPixel, 0));
    const float depthConfidence = TemporalDepthCompatible(centerDepth, reprojectedDepth, legacyLooseSky) ? 1.0 : 0.0;
    const float confidence = saturate(1.0 - luminanceDelta * 1.5) * inside * depthConfidence;
    return float4(lerp(center, history, temporalParameters.w * confidence), 1.0);
}
