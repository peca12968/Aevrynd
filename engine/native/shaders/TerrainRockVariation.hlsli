// 世界固定的第二岩層取樣域；色彩、ARM、法線必須共用位置與投影權重。
// 正的等向尺度不改法線方向；回傳世界法線時使用旋轉的轉置。
float3 TerrainRockDomainRotate(float3 value)
{
    return float3(value.x * 0.829037573f - value.z * 0.559192903f,
                  value.y,
                  value.x * 0.559192903f + value.z * 0.829037573f);
}
float3 TerrainRockDomainInverse(float3 value)
{
    return float3(value.x * 0.829037573f + value.z * 0.559192903f,
                  value.y,
                 -value.x * 0.559192903f + value.z * 0.829037573f);
}
float3 TerrainRockSecondaryPosition(float3 position)
{
    return TerrainRockDomainRotate(position) * 1.173f + float3(31.73f, -11.29f, 47.91f);
}
