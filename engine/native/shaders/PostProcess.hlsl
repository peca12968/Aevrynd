Texture2D<float4> hdrSceneColor : register(t10);
Texture2D<float> sceneSemanticDepth : register(t15);
SamplerState hdrSampler : register(s0);
cbuffer Composite : register(b0)
{
    float4 sourceRect;
    float4 targetRect;
    float4 antiAliasing;
    float4 bloomParameters;
    float4 timeOfDayExposure;
    float4 timeOfDayFilmic;
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

float3 ApplyTimeOfDayFilmic(float3 color,float sceneSemantic)
{
    if (timeOfDayExposure.w < 0.5)
        return color;
    const float luminance = dot(max(color, 0.0), float3(0.2126, 0.7152, 0.0722));
    // 陰影抬升按原色比例縮放，不加入灰色 ambient，保留材質色相與光照方向。
    const float lift = timeOfDayExposure.y * exp2(-luminance * 10.0) * timeOfDayExposure.z;
    const float3 lifted = color * ((luminance + lift) / max(luminance, 0.0005));
    const float3 filmic=lifted / (1.0 + max(0.0, timeOfDayFilmic.x - 0.5) * luminance * 0.08);
    // 天空維持legacy能量曲線；陰影抬升與肩部壓縮僅作用於有幾何深度的場景。
    return lerp(color,filmic,sceneSemantic);
}

float3 ApplySkyPreservingExposure(float3 color,float exposureEv,float sceneSemantic)
{
    if(timeOfDayExposure.w<.5)return color;
    // HDR天空與高光維持物理能量；曝光只逐步補償低亮度場景，不把天空與bloom一起乘亮。
    return color*lerp(1.0,exp2(exposureEv),sceneSemantic);
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

float3 BloomContribution(float2 uv)
{
    if (bloomParameters.w <= 0.0001)
        return 0.0;
    const float2 texel = antiAliasing.xy * bloomParameters.z;
    const float2 offsets[8] = { float2(-1,-1),float2(0,-1),float2(1,-1),float2(-1,0),float2(1,0),float2(-1,1),float2(0,1),float2(1,1) };
    float3 sum = 0.0;
    [unroll] for (uint index=0; index<8; ++index)
    {
        const float3 sampleColor=hdrSceneColor.SampleLevel(hdrSampler,uv+offsets[index]*texel,0).rgb;
        const float luminance=dot(max(sampleColor,0.0),float3(0.2126,0.7152,0.0722));
        const float knee=max(bloomParameters.x*bloomParameters.y,0.0001);
        const float soft=saturate((luminance-bloomParameters.x+knee)/(2.0*knee));
        const float excess=max(luminance-bloomParameters.x,0.0)+soft*soft*knee;
        sum+=sampleColor*(excess/max(luminance,0.0001));
    }
    return sum*(bloomParameters.w/8.0);
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
    if (range < max(0.025, maximumLuma * 0.085))
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
    // 曝光在色調映射前執行，保留 HDR 高光供泛光擷取。
    const float sceneSemantic=1.0-step(.995,sceneSemanticDepth.SampleLevel(hdrSampler,sourceUv,0.0));
    const float3 exposedScene=ApplySkyPreservingExposure(EdgeAdaptiveHdr(sourceUv,hdrCenter)*exp2(antiAliasing.w),timeOfDayExposure.x,sceneSemantic);
    const float3 hdr=ApplyTimeOfDayFilmic(exposedScene+BloomContribution(sourceUv),sceneSemantic);
    return float4(LinearToSrgb(AcesToneMap(hdr)), 1.0);
}

#if AEVRYND_INTERNAL_RENDERER_TEST_HOOKS
float4 PostProcessLinearExposurePixelMain(PostProcessVertexOutput input) : SV_TARGET
{
    uint sourceWidth, sourceHeight;
    hdrSceneColor.GetDimensions(sourceWidth, sourceHeight);
    const float2 normalizedPosition = saturate((input.position.xy - targetRect.xy) / max(targetRect.zw, 1.0));
    const float2 sourcePosition = sourceRect.xy + normalizedPosition * sourceRect.zw;
    const float2 sourceUv = sourcePosition / float2(sourceWidth, sourceHeight);
    // 測試專用輸出使用正式非零time-of-day常數與filmic路徑，只略過bloom、ACES及sRGB。
    const float3 source=max(0.0,hdrSceneColor.SampleLevel(hdrSampler,sourceUv,0.0).rgb)*exp2(antiAliasing.w);
    return float4(ApplyTimeOfDayFilmic(ApplySkyPreservingExposure(source,timeOfDayExposure.x,1.0),1.0),1.0);
}
#endif
