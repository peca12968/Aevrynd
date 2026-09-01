#ifndef AEVRYND_WORLD_RAY_RECONSTRUCTION_HLSLI
#define AEVRYND_WORLD_RAY_RECONSTRUCTION_HLSLI

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
    rayDirection = normalize(farWorld - rayOrigin);
#if defined(AEVRYND_WORLD_RAY_HEMISPHERE_FLIP)
    rayDirection.y = -rayDirection.y;
#endif
    return nearValid && farValid && all(isfinite(rayDirection));
}

#endif
