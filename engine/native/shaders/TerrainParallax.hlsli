// 有界 XZ 地表視差：只移動貼圖座標，不寫深度、不變碰撞與幾何。
// 宏關閉時不保留額外取樣；Scene 的尾端 ABI 仍由 World.hlsl 一律宣告。
#if !defined(AEVRYND_WORLD_TERRAIN_PARALLAX_DISABLED)
float TerrainParallaxWeight(float3 worldPosition, float3 geometricNormal)
{
    const float4 p = terrainParallaxParameters;
    const float4 v = terrainParallaxView;
    if (any(p != p) || any(v != v) || any(abs(p) > 65504.0) || any(abs(v) > 65504.0)
        || any(worldPosition != worldPosition) || any(geometricNormal != geometricNormal)
        || v.z < 0.5 || p.x <= 0.0 || p.x > 0.02 || p.y < 0.0 || p.y > 100.0 || p.z <= p.y || p.z > 120.0
        || p.w < 0.9 || p.w > 1.0 || v.x < 0.05 || v.x > 0.8 || v.y < 0.001 || v.y > 0.1)
        return 0.0;
    const float3 toCamera = cameraFogStart.xyz - worldPosition;
    const float distanceSquared = dot(toCamera, toCamera);
    if (distanceSquared <= 0.0000000001 || distanceSquared != distanceSquared || distanceSquared > 1e30
        || geometricNormal.y < p.w)
        return 0.0;
    const float distanceToCamera = sqrt(distanceSquared);
    const float viewCosine = toCamera.y / distanceToCamera;
    // 近平面的掠射角先淡出，不能靠極大 reciprocal 製造遠距拉伸。
    const float grazing = smoothstep(v.x, min(1.0, v.x + 0.15), viewCosine);
    const float nearWeight = 1.0 - smoothstep(p.y, p.z, distanceToCamera);
    const float slopeWeight = p.w < 1.0 ? smoothstep(p.w, min(1.0, p.w + 0.02), geometricNormal.y) : 1.0;
    return nearWeight * grazing * slopeWeight;
}

float2 TerrainParallaxOffset(Texture2D heightSource, float3 worldPosition,
    float3 samplePosition, float3 sampleDx, float3 sampleDy,
    float3 worldDx, float3 worldDy, float textureScale, float effectWeight)
{
#if defined(AEVRYND_WORLD_TERRAIN_PARALLAX_ZERO_OFFSET)
    // 只供同一 SampleGrad 路徑的零位移 oracle；預設不定義，不宣稱高度查詢通過。
    return float2(0.0, 0.0);
#else
    if (effectWeight <= 0.0 || effectWeight != effectWeight || textureScale <= 0.0 || textureScale != textureScale
        || any(samplePosition != samplePosition) || any(sampleDx != sampleDx) || any(sampleDy != sampleDy)
        || any(worldDx != worldDx) || any(worldDy != worldDy)
        || any(abs(samplePosition) > 1e30) || any(abs(sampleDx) > 1e15) || any(abs(sampleDy) > 1e15)
        || any(abs(worldDx) > 1e15) || any(abs(worldDy) > 1e15))
        return float2(0.0, 0.0);
    const float determinant = worldDx.x * worldDy.z - worldDx.z * worldDy.x;
    const float derivativeScale = max(dot(worldDx.xz, worldDx.xz) * dot(worldDy.xz, worldDy.xz), 0.00000000000000000001);
    if (determinant * determinant <= derivativeScale * 0.00000001)
        return float2(0.0, 0.0);
    const float3 toCamera = cameraFogStart.xyz - worldPosition;
    if (toCamera.y <= 0.0)
        return float2(0.0, 0.0);
    // depth 的單位是公尺；先解 world XZ 的局部座標，再乘實際 layer／貼圖尺度。
    // 此 Jacobian 包含來源的連續 warp、頻率，以及第二域的旋轉與 1.173 尺度。
    const float2 worldRay = -toCamera.xz / toCamera.y;
    const float2 screenCoefficients = float2(
        worldRay.x * worldDy.z - worldRay.y * worldDy.x,
        worldDx.x * worldRay.y - worldDx.z * worldRay.x) / determinant;
    float2 fullRay = (sampleDx.xz * screenCoefficients.x + sampleDy.xz * screenCoefficients.y)
                   * textureScale * terrainParallaxParameters.x * effectWeight;
    const float rayLength = length(fullRay);
    if (rayLength != rayLength || rayLength <= 0.00000001 || rayLength > 65504.0)
        return float2(0.0, 0.0);
    fullRay *= min(1.0, terrainParallaxView.y / rayLength);
    const float2 originalUv = samplePosition.xz * textureScale;
    const float2 gradientDx = sampleDx.xz * textureScale;
    const float2 gradientDy = sampleDy.xz * textureScale;
    float lower = 0.0;
    float upper = 1.0;
    bool intersectionFound = false;
    // 八次含起點／終點的有界查詢，加兩次二分細化；不使用隱式 loop derivatives。
    [unroll] for (uint layer = 0u; layer < 8u; ++layer)
    {
        const float depthFraction = float(layer) / 7.0;
        const float height = saturate(heightSource.SampleGrad(materialSampler,
            originalUv + fullRay * depthFraction, gradientDx, gradientDy).a);
        if (!intersectionFound && depthFraction >= 1.0 - height)
        {
            if (layer == 0u) return float2(0.0, 0.0);
            lower = float(layer - 1u) / 7.0;
            upper = depthFraction;
            intersectionFound = true;
        }
    }
    if (!intersectionFound) return float2(0.0, 0.0);
    [unroll] for (uint refinement = 0u; refinement < 2u; ++refinement)
    {
        const float middle = (lower + upper) * 0.5;
        const float height = saturate(heightSource.SampleGrad(materialSampler,
            originalUv + fullRay * middle, gradientDx, gradientDy).a);
        if (middle >= 1.0 - height) upper = middle;
        else lower = middle;
    }
    return fullRay * ((lower + upper) * 0.5);
#endif
}

float4 TerrainParallaxTriplanar(Texture2D source, float3 samplePosition,
    float3 normal, float scale, float3 sampleDx, float3 sampleDy, float2 xzUvOffset)
{
    const float3 blend = BlendWeights(normal);
    // 只有 XZ 投影位移；另外兩軸保持原來的位置與原來的 quad 導數。
    return source.SampleGrad(materialSampler, samplePosition.zy * scale, sampleDx.zy * scale, sampleDy.zy * scale) * blend.x
         + source.SampleGrad(materialSampler, samplePosition.xz * scale + xzUvOffset, sampleDx.xz * scale, sampleDy.xz * scale) * blend.y
         + source.SampleGrad(materialSampler, samplePosition.xy * scale, sampleDx.xy * scale, sampleDy.xy * scale) * blend.z;
}

float3 TerrainParallaxNormal(Texture2D source, float3 samplePosition,
    float3 geometricNormal, float scale, float3 sampleDx, float3 sampleDy, float2 xzUvOffset)
{
    const float3 blend = BlendWeights(geometricNormal);
    const float3 axisSign = float3(geometricNormal.x < 0.0 ? -1.0 : 1.0,
                                  geometricNormal.y < 0.0 ? -1.0 : 1.0,
                                  geometricNormal.z < 0.0 ? -1.0 : 1.0);
    const float3 x = DecodeBc5Normal(source.SampleGrad(materialSampler, samplePosition.zy * scale, sampleDx.zy * scale, sampleDy.zy * scale).rg);
    const float3 y = DecodeBc5Normal(source.SampleGrad(materialSampler, samplePosition.xz * scale + xzUvOffset, sampleDx.xz * scale, sampleDy.xz * scale).rg);
    const float3 z = DecodeBc5Normal(source.SampleGrad(materialSampler, samplePosition.xy * scale, sampleDx.xy * scale, sampleDy.xy * scale).rg);
    const float3 mappedX = ReorientTriplanarNormal(float3(axisSign.x * x.z, x.y, x.x), float3(axisSign.x, 0.0, 0.0), geometricNormal);
    const float3 mappedY = ReorientTriplanarNormal(float3(y.x, axisSign.y * y.z, y.y), float3(0.0, axisSign.y, 0.0), geometricNormal);
    const float3 mappedZ = ReorientTriplanarNormal(float3(z.x, z.y, axisSign.z * z.z), float3(0.0, 0.0, axisSign.z), geometricNormal);
    const float3 combined = mappedX * blend.x + mappedY * blend.y + mappedZ * blend.z;
    return dot(combined, combined) > 0.0000000001 ? normalize(combined) : geometricNormal;
}
#endif
