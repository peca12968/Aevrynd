#pragma once

#include <algorithm>
#include <cmath>

namespace Aevrynd {

struct PbrEnergyWeights {
    double diffuse{};
    double specular{};
    double specularResponse{};
};

inline double PbrClampedRoughness(double roughness){return(std::clamp)(roughness,.045,1.0);}

// 與 shader 相同的分母下限是 anti-firefly 穩定化策略，不是未修改的解析 GGX。
inline double PbrDistributionGgxStabilized(double normalHalf,double roughness){const double alpha=PbrClampedRoughness(roughness)*PbrClampedRoughness(roughness),alphaSquared=alpha*alpha,denominator=normalHalf*normalHalf*(alphaSquared-1.0)+1.0;return alphaSquared/(std::max)(3.141592653589793*denominator*denominator,.000001);}
inline double PbrGeometrySchlickGgx(double normalDirection,double roughness){const double value=PbrClampedRoughness(roughness)+1.0,k=value*value*.125;return normalDirection/(std::max)(normalDirection*(1.0-k)+k,.0001);}
inline double PbrFresnelSchlick(double cosine,double reflectance){return reflectance+(1.0-reflectance)*std::pow(1.0-(std::clamp)(cosine,0.0,1.0),5.0);}

// 此參考函式驗證 shader 使用的能量分配，不取代 GPU 的向量化實作。
inline PbrEnergyWeights PbrReferenceEnergy(double baseColor,double metallic,double roughness,double normalView,double normalLight,double normalHalf,double viewHalf){
    baseColor=(std::clamp)(baseColor,0.0,1.0);metallic=(std::clamp)(metallic,0.0,1.0);roughness=PbrClampedRoughness(roughness);normalView=(std::clamp)(normalView,0.0,1.0);normalLight=(std::clamp)(normalLight,0.0,1.0);normalHalf=(std::clamp)(normalHalf,0.0,1.0);viewHalf=(std::clamp)(viewHalf,0.0,1.0);const double reflectance=.04*(1.0-metallic)+baseColor*metallic,fresnel=PbrFresnelSchlick(viewHalf,reflectance),diffuseWeight=(1.0-fresnel)*(1.0-metallic),distribution=PbrDistributionGgxStabilized(normalHalf,roughness),geometry=PbrGeometrySchlickGgx(normalView,roughness)*PbrGeometrySchlickGgx(normalLight,roughness),specularResponse=distribution*geometry*fresnel/(std::max)(4.0*normalView*normalLight,.0001);return{diffuseWeight,fresnel,specularResponse};
}

inline double PbrReferenceEvaluateLight(double baseColor,double metallic,double roughness,double normalView,double normalLight,double normalHalf,double viewHalf,double radiance){if(!std::isfinite(radiance)||radiance<0||normalView<=0||normalLight<=0)return 0;const PbrEnergyWeights energy=PbrReferenceEnergy(baseColor,metallic,roughness,normalView,normalLight,normalHalf,viewHalf);baseColor=(std::clamp)(baseColor,0.0,1.0);normalLight=(std::clamp)(normalLight,0.0,1.0);return(energy.diffuse*baseColor/3.141592653589793+energy.specularResponse)*radiance*normalLight;}

inline double PbrPointLightAttenuation(double distance,double radius){if(!std::isfinite(distance)||!std::isfinite(radius)||distance<0||radius<=0)return 0;const double distanceSquared=distance*distance,safeRadius=(std::max)(radius,.001),normalized=distanceSquared/(safeRadius*safeRadius),window=(std::clamp)(1.0-normalized*normalized,0.0,1.0);return window*window/(std::max)(distanceSquared+1.0,.0001);}

inline double PbrAcesToneMap(double color){color=(std::max)(color,0.0)*1.1;return(std::clamp)((color*(2.51*color+.03))/(color*(2.43*color+.59)+.14),0.0,1.0);}
inline double PbrLinearToSrgb(double linearColor){if(!std::isfinite(linearColor))return 0;linearColor=(std::clamp)(linearColor,0.0,1.0);return linearColor<=.0031308?linearColor*12.92:1.055*std::pow(linearColor,1.0/2.4)-.055;}
inline double PbrToneMapToSrgb(double color){return PbrLinearToSrgb(PbrAcesToneMap(color));}

}
