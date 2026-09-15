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

precise float AevryndDeterministicReciprocalSigned(float value)
{
    // Shader model 的 div/rcp 可能採用依實作而異的近似值。
    // 從 IEEE-754 表示建立牛頓迭代種子，使齊次座標重建在不同證據程序間維持位元穩定。
    const uint valueBits = asuint(value);
    const uint magnitudeBits = valueBits & 0x7fffffffu;
    if (magnitudeBits >= 0x7f800000u || magnitudeBits < asuint(1e-20)) return 0.0;
    const uint exponent = (magnitudeBits >> 23u) & 0xffu;
    const uint normalizedBits = 0x3f800000u | (magnitudeBits & 0x007fffffu);
    precise const float normalizedMagnitude = asfloat(normalizedBits);
    precise float estimate = asfloat(0x7ef311c3u - normalizedBits);
    estimate = estimate * (2.0 - normalizedMagnitude * estimate);
    estimate = estimate * (2.0 - normalizedMagnitude * estimate);
    estimate = estimate * (2.0 - normalizedMagnitude * estimate);
    estimate = estimate * (2.0 - normalizedMagnitude * estimate);
    // 分開套回倒數指數，避免種子減法在巨大但有限的輸入下發生回繞。
    // 指數 254 需要可表示的非正規化縮放步驟；若後端將其沖成零，會由下方重建的失敗關閉檢查攔截。
    precise const float exponentScale = exponent < 254u
        ? asfloat((254u - exponent) << 23u)
        : asfloat(0x00800000u) * 0.5;
    estimate *= exponentScale;
    precise const float result = (valueBits & 0x80000000u) != 0u ? -estimate : estimate;
    return result;
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
    precise float4 clip = float4(AevryndViewportUvToNdc(viewportUv), hardwareDepth, 1.0);
#if defined(AEVRYND_WORLD_RAY_LEGACY_CLIP_MATRIX)
    precise float4 homogeneous = mul(clip, inverseViewProjection);
#else
    precise float4 homogeneous = mul(inverseViewProjection, clip);
#endif
#if defined(AEVRYND_WORLD_RAY_FORCE_NEGATIVE_W_FIXTURE)
    homogeneous = -homogeneous;
#endif
    if (!all(isfinite(homogeneous)) || abs(homogeneous.w) <= 1e-6) {
        worldPosition = 0.0;
        return false;
    }
#if defined(AEVRYND_WORLD_RAY_LEGACY_ABS_W)
    worldPosition = homogeneous.xyz / abs(homogeneous.w);
#elif defined(AEVRYND_WORLD_RAY_DETERMINISTIC_NORMALIZE)
    precise const float inverseW = AevryndDeterministicReciprocalSigned(homogeneous.w);
    if (inverseW == 0.0 || !isfinite(inverseW)) {
        worldPosition = 0.0;
        return false;
    }
    worldPosition = homogeneous.xyz * inverseW;
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
