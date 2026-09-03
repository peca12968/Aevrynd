struct PointLightGpu3D
{
    float3 position;
    float radius;
    float3 color;
    float intensity;
};

cbuffer ForwardPlusConstants3D : register(b0)
{
    row_major float4x4 viewProjection;
    row_major float4x4 inverseViewProjection;
    uint2 renderSize;
    uint2 viewportOrigin;
    uint2 viewportSize;
    uint2 tileCount;
    uint lightCount;
    uint maximumLightsPerTile;
    uint perspectiveDepth;
    uint padding;
};

Texture2D<float> opaqueDepth : register(t0);
StructuredBuffer<PointLightGpu3D> pointLights : register(t1);
RWStructuredBuffer<uint4> tileGrid : register(u0);
RWStructuredBuffer<uint> tileIndices : register(u1);

groupshared uint minimumDepthBits;
groupshared uint maximumDepthBits;

float LinearViewDepth(uint2 pixel, float deviceDepth)
{
    // 直接從逆矩陣恢復齊次 w，避免把任意相機的 near/far 寫死。
    const float2 local=(float2(pixel)+0.5-float2(viewportOrigin))/float2(viewportSize);
    const float2 ndc=local*float2(2.0,-2.0)+float2(-1.0,1.0);
    const float4 worldH=mul(inverseViewProjection,float4(ndc,deviceDepth,1.0));
    return abs(1.0/worldH.w);
}

[numthreads(16,16,1)]
void ForwardPlusLightCullingMain(uint3 groupId : SV_GroupID, uint3 groupThreadId : SV_GroupThreadID)
{
    const uint lane = groupThreadId.y * 16u + groupThreadId.x;
    if (lane == 0u)
    {
        minimumDepthBits = asuint(3.402823466e+38f);
        maximumDepthBits = 0u;
    }
    GroupMemoryBarrierWithGroupSync();

    const uint2 pixel = viewportOrigin + groupId.xy * 16u + groupThreadId.xy;
    if (all(pixel < viewportOrigin + viewportSize) && all(pixel < renderSize))
    {
        const float depth = opaqueDepth.Load(int3(pixel, 0));
        if (depth > 0.0f && depth < 0.999999f)
        {
            const uint linearBits = asuint(LinearViewDepth(pixel,depth));
            InterlockedMin(minimumDepthBits, linearBits);
            InterlockedMax(maximumDepthBits, linearBits);
        }
    }
    GroupMemoryBarrierWithGroupSync();

    if (lane != 0u) return;
    const uint tile = groupId.y * tileCount.x + groupId.x;
    const uint baseIndex = tile * maximumLightsPerTile;
    uint stored = 0u;
    bool overflow = false;
    const bool hasOpaqueDepth = perspectiveDepth!=0u&&minimumDepthBits <= maximumDepthBits;
    const float minimumDepth = asfloat(minimumDepthBits);
    const float maximumDepth = asfloat(maximumDepthBits);

    [loop]
    for (uint index = 0u; index < lightCount; ++index)
    {
        const PointLightGpu3D light = pointLights[index];
        const float4 clip = mul(viewProjection, float4(light.position, 1.0f));
        const float radius = abs(light.radius);
        const float3 rowX = float3(viewProjection[0][0], viewProjection[0][1], viewProjection[0][2]);
        const float3 rowY = float3(viewProjection[1][0], viewProjection[1][1], viewProjection[1][2]);
        const float3 rowZ = float3(viewProjection[2][0], viewProjection[2][1], viewProjection[2][2]);
        const float3 rowW = float3(viewProjection[3][0], viewProjection[3][1], viewProjection[3][2]);
        const float4 support = radius * float4(length(rowX), length(rowY), length(rowZ), length(rowW));

        // 每個 clip 分量都是世界座標的線性 functional；row.xyz 的長度因此是
        // 該 functional 在世界空間球上的精確 support。非有限輸入不能安全剔除。
        bool conservativeIntersection = !all(isfinite(clip)) || !all(isfinite(support));
        if (!conservativeIntersection)
        {
            const float epsilon = 1e-5f * (1.0f + abs(clip.z) + abs(clip.w) + support.z + support.w);
            if (clip.w + support.w < -epsilon) continue; // 整球在 eye plane 後方。
            if (clip.z + support.z < -epsilon) continue; // 整球在 D3D near plane 外。
            const float farCenter = clip.w - clip.z;
            const float farRadius = radius * length(rowW - rowZ);
            if (farCenter + farRadius < -epsilon) continue; // 整球在 D3D far plane 外。
            if (hasOpaqueDepth &&
                (clip.w + support.w < minimumDepth - epsilon || clip.w - support.w > maximumDepth + epsilon)) continue;

            // 跨越 eye/near plane 時有界的透視 XY 投影不存在；保守加入所有 tile。
            conservativeIntersection = clip.w - support.w <= epsilon || clip.z - support.z <= epsilon;
            if (!conservativeIntersection)
            {
                const float denominatorMinimum = clip.w - support.w;
                const float denominatorMaximum = clip.w + support.w;
                const float4 projectedX = float4(
                    (clip.x - support.x) / denominatorMinimum,
                    (clip.x - support.x) / denominatorMaximum,
                    (clip.x + support.x) / denominatorMinimum,
                    (clip.x + support.x) / denominatorMaximum);
                const float4 projectedY = float4(
                    (clip.y - support.y) / denominatorMinimum,
                    (clip.y - support.y) / denominatorMaximum,
                    (clip.y + support.y) / denominatorMinimum,
                    (clip.y + support.y) / denominatorMaximum);
                const float2 sphereMinimum = float2(
                    min(min(projectedX.x, projectedX.y), min(projectedX.z, projectedX.w)),
                    min(min(projectedY.x, projectedY.y), min(projectedY.z, projectedY.w)));
                const float2 sphereMaximum = float2(
                    max(max(projectedX.x, projectedX.y), max(projectedX.z, projectedX.w)),
                    max(max(projectedY.x, projectedY.y), max(projectedY.z, projectedY.w)));
                const float2 tileA = (float2(groupId.xy * 16u) / float2(viewportSize)) * float2(2.0f, -2.0f) + float2(-1.0f, 1.0f);
                const float2 tileB = (float2(min(groupId.xy * 16u + 16u, viewportSize)) / float2(viewportSize)) * float2(2.0f, -2.0f) + float2(-1.0f, 1.0f);
                const float2 tileMinimum = min(tileA, tileB);
                const float2 tileMaximum = max(tileA, tileB);
                const float2 projectionPadding = float2(1e-5f, 1e-5f);
                if (any(sphereMaximum + projectionPadding < tileMinimum) ||
                    any(sphereMinimum - projectionPadding > tileMaximum)) continue;
            }
        }
        if (stored < maximumLightsPerTile) tileIndices[baseIndex + stored++] = index;
        else overflow = true;
    }
    // overflow 時像素著色器必須遍歷全部 canonical lights，不能只讀截斷清單。
    tileGrid[tile] = uint4(baseIndex, stored, overflow ? 1u : 0u, lightCount);
}
