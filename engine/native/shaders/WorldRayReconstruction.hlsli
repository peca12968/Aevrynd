#ifndef AEVRYND_WORLD_RAY_RECONSTRUCTION_HLSLI
#define AEVRYND_WORLD_RAY_RECONSTRUCTION_HLSLI

#if defined(AEVRYND_WORLD_RAY_DETERMINISTIC_NORMALIZE)
precise float AevryndDeterministicInverseSqrt(float value)
{
    // Shader-model rsq/normalize 可使用實作相依的近似。固定 IEEE-754
    // 初始值與四次 Newton 疊代，讓證據擷取不因 wave 排程漂移 1 ULP。
    precise const float inputValue=max(value,1e-20);
    precise float estimate=asfloat(0x5f375a86u-(asuint(inputValue)>>1u));
    precise const float halfInput=.5*inputValue;
    estimate=estimate*(1.5-halfInput*estimate*estimate);
    estimate=estimate*(1.5-halfInput*estimate*estimate);
    estimate=estimate*(1.5-halfInput*estimate*estimate);
    estimate=estimate*(1.5-halfInput*estimate*estimate);
    return estimate;
}

precise float3 AevryndDeterministicNormalize(float3 value)
{
    precise const float inverseLength=AevryndDeterministicInverseSqrt(dot(value,value));
    precise const float3 result=value*inverseLength;
    return result;
}
#endif

float2 AevryndViewportUvToNdc(float2 viewportUv)
{
    return float2(viewportUv.x * 2.0 - 1.0, 1.0 - viewportUv.y * 2.0);
}

bool AevryndReconstructWorldPosition(float2 viewportUv, float hardwareDepth,
                                     row_major float4x4 inverseViewProjection,
                                     out float3 worldPosition)
{
    float4 clip = float4(AevryndViewportUvToNdc(viewportUv), hardwareDepth, 1.0);
#if defined(AEVRYND_WORLD_RAY_LEGACY_CLIP_MATRIX)
    float4 homogeneous = mul(clip, inverseViewProjection);
#else
    float4 homogeneous = mul(inverseViewProjection, clip);
#endif
#if defined(AEVRYND_WORLD_RAY_FORCE_NEGATIVE_W_FIXTURE)
    homogeneous = -homogeneous;
#endif
    if (abs(homogeneous.w) <= 1e-6) {
        worldPosition = 0.0;
        return false;
    }
#if defined(AEVRYND_WORLD_RAY_LEGACY_ABS_W)
    worldPosition = homogeneous.xyz / abs(homogeneous.w);
#else
    // 齊次座標除法必須保留 w 符號，否則天空半球會在特定列翻轉。
    worldPosition = homogeneous.xyz / homogeneous.w;
#endif
    return all(isfinite(worldPosition));
}

bool AevryndReconstructWorldRay(float2 viewportUv,
                                row_major float4x4 inverseViewProjection,
                                float3 cameraPosition, bool orthographic,
                                out float3 rayOrigin, out float3 rayDirection)
{
    float3 nearWorld, farWorld;
    const bool nearValid = AevryndReconstructWorldPosition(
        viewportUv, 0.0, inverseViewProjection, nearWorld);
    const bool farValid = AevryndReconstructWorldPosition(
        viewportUv, 1.0, inverseViewProjection, farWorld);
    rayOrigin = orthographic ? nearWorld : cameraPosition;
#if defined(AEVRYND_WORLD_RAY_DETERMINISTIC_NORMALIZE)
    rayDirection = AevryndDeterministicNormalize(farWorld - rayOrigin);
#else
    rayDirection = normalize(farWorld - rayOrigin);
#endif
#if defined(AEVRYND_WORLD_RAY_HEMISPHERE_FLIP)
    rayDirection.y = -rayDirection.y;
#endif
    return nearValid && farValid && all(isfinite(rayDirection));
}

#endif
