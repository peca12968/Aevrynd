#ifndef AEVRYND_PHASE_SKY_DIFFUSE_INCLUDED
#define AEVRYND_PHASE_SKY_DIFFUSE_INCLUDED

#ifdef __cplusplus
namespace Aevrynd {
#endif

struct PhaseSkyDiffuseWeights
{
    float zenith;
    float horizon;
    float ground;
};

// 對現有軸對稱天空 H+(pole-H)|y|^k 做 Lambert 半球積分 E/pi。
// 保留 Legendre 0、1、2、4、6、8 階；奇數三階以上的 cosine 卷積為零。
// 不含太陽圓盤：直射光已有獨立陰影，不能再加入無陰影的 sun-wrap。
// 輸入契約：有限單位法線的 y，以及 NormalizePhaseRadianceProfile3D 的 k。
// 方法參考 Ramamoorthi/Hanrahan, An Efficient Representation for Irradiance
// Environment Maps (2001)；下列軸對稱 power-profile 積分為本引擎自行推導。
inline PhaseSkyDiffuseWeights EvaluatePhaseSkyDiffuseRawWeights(float normalY, float exponent)
{
    const float y = normalY < -1.0f ? -1.0f : (normalY > 1.0f ? 1.0f : normalY);
    const float k = exponent < 0.05f ? 0.05f : (exponent > 8.0f ? 8.0f : exponent);
    const float y2 = y * y;
    const float p2 = (3.0f * y2 - 1.0f) * 0.5f;
    const float p4 = ((35.0f * y2 - 30.0f) * y2 + 3.0f) * 0.125f;
    const float p6 = (((231.0f * y2 - 315.0f) * y2 + 105.0f) * y2 - 5.0f) * 0.0625f;
    const float p8 = ((((6435.0f * y2 - 12012.0f) * y2 + 6930.0f) * y2 - 1260.0f) * y2 + 35.0f) * 0.0078125f;
    const float i2 = k / ((k + 1.0f) * (k + 3.0f));
    const float i4 = i2 * (k - 2.0f) / (k + 5.0f);
    const float i6 = i4 * (k - 4.0f) / (k + 7.0f);
    const float i8 = i6 * (k - 6.0f) / (k + 9.0f);
    const float even = 0.625f * i2 * p2 - 0.1875f * i4 * p4 + 0.1015625f * i6 * p6 - 0.06640625f * i8 * p8;
    const float constant = 0.5f / (k + 1.0f);
    const float linearWeight = y / (k + 2.0f);
    const float upper = constant + linearWeight + even;
    const float lower = constant - linearWeight + even;
    const float middle = 1.0f - upper - lower;
    PhaseSkyDiffuseWeights result;
    result.zenith = upper;
    result.ground = lower;
    result.horizon = middle;
    return result;
}

inline PhaseSkyDiffuseWeights EvaluatePhaseSkyDiffuseWeights(float normalY, float exponent)
{
    const PhaseSkyDiffuseWeights raw = EvaluatePhaseSkyDiffuseRawWeights(normalY, exponent);
    // 截斷級數在極端單色天空可有小負瓣；投影回凸組合，保持黑色、
    // 常數天空、非負性及能量界限。誤差另與獨立半球數值積分比較。
    PhaseSkyDiffuseWeights result;
    result.zenith = raw.zenith > 0.0f ? raw.zenith : 0.0f;
    result.ground = raw.ground > 0.0f ? raw.ground : 0.0f;
    result.horizon = raw.horizon > 0.0f ? raw.horizon : 0.0f;
    const float total = result.zenith + result.horizon + result.ground;
    result.zenith /= total;
    result.horizon /= total;
    result.ground /= total;
    return result;
}

#ifdef __cplusplus
}
#endif
#endif
