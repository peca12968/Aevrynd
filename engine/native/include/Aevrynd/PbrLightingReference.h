#pragma once

#include <algorithm>
#include <cmath>

namespace Aevrynd {

struct PbrEnergyWeights {
    double diffuse{};
    double specular{};
    double specularResponse{};
};
struct PbrEnvironmentWeights {
    double diffuse{};
    double specular{};
    double total{};
};

inline double PbrClampedRoughness(double roughness){return(std::clamp)(std::isfinite(roughness)?roughness:1.0,.045,1.0);}

// 與 shader 相同的分母下限是 anti-firefly 穩定化策略，不是未修改的解析 GGX。
inline double PbrDistributionGgxStabilized(double normalHalf,double roughness){const double alpha=PbrClampedRoughness(roughness)*PbrClampedRoughness(roughness),alphaSquared=alpha*alpha,denominator=normalHalf*normalHalf*(alphaSquared-1.0)+1.0;return alphaSquared/(std::max)(3.141592653589793*denominator*denominator,.000001);}
inline double PbrGeometrySchlickGgx(double normalDirection,double roughness){const double value=PbrClampedRoughness(roughness)+1.0,k=value*value*.125;return normalDirection/(std::max)(normalDirection*(1.0-k)+k,.0001);}
inline double PbrFresnelSchlick(double cosine,double reflectance){return reflectance+(1.0-reflectance)*std::pow(1.0-(std::clamp)(cosine,0.0,1.0),5.0);}
inline double PbrFresnelSchlickRoughness(double cosine,double reflectance,double roughness){reflectance=(std::clamp)(reflectance,0.0,1.0);roughness=PbrClampedRoughness(roughness);const double grazing=(std::max)(1.0-roughness,reflectance);return reflectance+(grazing-reflectance)*std::pow(1.0-(std::clamp)(cosine,0.0,1.0),5.0);}
inline PbrEnvironmentWeights PbrReferenceEnvironment(double baseColor,double metallic,double roughness,double normalView,double skyDiffuse,double skySpecular,double ambient=1.0,double occlusion=1.0){baseColor=(std::clamp)(std::isfinite(baseColor)?baseColor:0.0,0.0,1.0);metallic=(std::clamp)(std::isfinite(metallic)?metallic:0.0,0.0,1.0);roughness=PbrClampedRoughness(roughness);normalView=(std::clamp)(std::isfinite(normalView)?normalView:0.0,0.0,1.0);skyDiffuse=(std::clamp)(std::isfinite(skyDiffuse)?skyDiffuse:0.0,0.0,4.0);skySpecular=(std::clamp)(std::isfinite(skySpecular)?skySpecular:0.0,0.0,16.0);ambient=(std::clamp)(std::isfinite(ambient)?ambient:0.0,0.0,2.0);occlusion=(std::clamp)(std::isfinite(occlusion)?occlusion:0.0,0.0,1.0);const double reflectance=.04*(1.0-metallic)+baseColor*metallic,fresnel=PbrFresnelSchlickRoughness(normalView,reflectance,roughness),diffuse=(1.0-fresnel)*(1.0-metallic)*baseColor*skyDiffuse,specular=fresnel*skySpecular*(.35+.65*(1.0-roughness));return{diffuse*ambient*occlusion,specular*ambient*occlusion,(diffuse+specular)*ambient*occlusion};}
inline double PbrWaterReflectance(double refractiveIndex=1.333){if(!std::isfinite(refractiveIndex)||refractiveIndex<=0)return 0;const double ratio=(refractiveIndex-1.0)/(refractiveIndex+1.0);return(std::clamp)(ratio*ratio,0.0,1.0);}
inline double PbrWaterFresnel(double normalView,double refractiveIndex=1.333){return PbrFresnelSchlick(normalView,PbrWaterReflectance(refractiveIndex));}
inline double PbrCorneaReflectance(double refractiveIndex=1.376){return PbrWaterReflectance(refractiveIndex);}
inline double PbrCorneaFresnel(double cosine,double refractiveIndex=1.376){return PbrFresnelSchlick(cosine,PbrCorneaReflectance(refractiveIndex));}
inline double PbrCorneaRoughness(double materialRoughness){materialRoughness=std::isfinite(materialRoughness)?materialRoughness:1.0;return(std::clamp)(materialRoughness*.12,.025,.08);}
inline double PbrBeerLambertTransmittance(double absorption,double pathLength){absorption=(std::clamp)(std::isfinite(absorption)?absorption:0.0,0.0,2.0);pathLength=(std::clamp)(std::isfinite(pathLength)?pathLength:0.0,0.0,8.0);return std::exp(-absorption*pathLength);}

// 此參考函式驗證 shader 使用的能量分配，不取代 GPU 的向量化實作。
inline PbrEnergyWeights PbrReferenceEnergy(double baseColor,double metallic,double roughness,double normalView,double normalLight,double normalHalf,double viewHalf){
    baseColor=(std::clamp)(baseColor,0.0,1.0);metallic=(std::clamp)(metallic,0.0,1.0);roughness=PbrClampedRoughness(roughness);normalView=(std::clamp)(normalView,0.0,1.0);normalLight=(std::clamp)(normalLight,0.0,1.0);normalHalf=(std::clamp)(normalHalf,0.0,1.0);viewHalf=(std::clamp)(viewHalf,0.0,1.0);const double reflectance=.04*(1.0-metallic)+baseColor*metallic,fresnel=PbrFresnelSchlick(viewHalf,reflectance),diffuseWeight=(1.0-fresnel)*(1.0-metallic),distribution=PbrDistributionGgxStabilized(normalHalf,roughness),geometry=PbrGeometrySchlickGgx(normalView,roughness)*PbrGeometrySchlickGgx(normalLight,roughness),specularResponse=distribution*geometry*fresnel/(std::max)(4.0*normalView*normalLight,.0001);return{diffuseWeight,fresnel,specularResponse};
}

inline double PbrReferenceEvaluateLight(double baseColor,double metallic,double roughness,double normalView,double normalLight,double normalHalf,double viewHalf,double radiance){if(!std::isfinite(radiance)||radiance<0||normalView<=0||normalLight<=0)return 0;const PbrEnergyWeights energy=PbrReferenceEnergy(baseColor,metallic,roughness,normalView,normalLight,normalHalf,viewHalf);baseColor=(std::clamp)(baseColor,0.0,1.0);normalLight=(std::clamp)(normalLight,0.0,1.0);return(energy.diffuse*baseColor/3.141592653589793+energy.specularResponse)*radiance*normalLight;}
// 此純量參考與 shader 相同：角膜 Fresnel 先扣除基底能量，再疊加受控 GGX clearcoat。
inline double PbrReferenceEyeLight(double baseColor,double materialRoughness,double baseNormalView,double baseNormalLight,double baseNormalHalf,double viewHalf,double corneaNormalView,double corneaNormalLight,double corneaNormalHalf,double radiance){if(!std::isfinite(radiance)||radiance<0)return 0;const double base=PbrReferenceEvaluateLight(baseColor,0,materialRoughness,baseNormalView,baseNormalLight,baseNormalHalf,viewHalf,radiance);corneaNormalView=(std::clamp)(std::isfinite(corneaNormalView)?corneaNormalView:0.0,0.0,1.0);corneaNormalLight=(std::clamp)(std::isfinite(corneaNormalLight)?corneaNormalLight:0.0,0.0,1.0);corneaNormalHalf=(std::clamp)(std::isfinite(corneaNormalHalf)?corneaNormalHalf:0.0,0.0,1.0);if(corneaNormalView<=0||corneaNormalLight<=0)return base;const double fresnel=PbrCorneaFresnel(viewHalf),roughness=PbrCorneaRoughness(materialRoughness),distribution=PbrDistributionGgxStabilized(corneaNormalHalf,roughness),geometry=PbrGeometrySchlickGgx(corneaNormalView,roughness)*PbrGeometrySchlickGgx(corneaNormalLight,roughness),clearcoat=distribution*geometry*fresnel/(std::max)(4.0*corneaNormalView*corneaNormalLight,.0001)*radiance*corneaNormalLight;return base*(1.0-fresnel)+clearcoat;}
inline double PbrReferenceSkinSupplement(double baseColor,double roughness,double normalView,double normalLight,double normalHalf,double viewHalf,double backView,double radiance){if(!std::isfinite(radiance)||radiance<0)return 0;baseColor=(std::clamp)(std::isfinite(baseColor)?baseColor:0.0,0.0,1.0);roughness=PbrClampedRoughness(roughness);normalLight=(std::clamp)(std::isfinite(normalLight)?normalLight:0.0,-1.0,1.0);backView=(std::clamp)(std::isfinite(backView)?backView:0.0,0.0,1.0);const double wrapped=(std::clamp)((normalLight+.25)/1.25,0.0,1.0),wrapExtra=(std::max)(0.0,wrapped-(std::max)(0.0,normalLight)),backScatter=std::pow(backView,4.0)*(std::max)(0.0,-normalLight),narrow=PbrReferenceEvaluateLight(0,0,(std::max)(.09,roughness*.45),normalView,(std::max)(0.0,normalLight),normalHalf,viewHalf,radiance)*.18,warm=baseColor*.48*radiance*(wrapExtra*.20+backScatter*.08);return narrow+warm;}
// 以兩個切線高光正弦值重現 shader 的固定成本髮絲能量上限。
inline double PbrReferenceHairSupplement(double baseColor,double roughness,double normalLight,double strandHalf,double shiftedStrandHalf,double radiance){if(!std::isfinite(radiance)||radiance<0)return 0;baseColor=(std::clamp)(std::isfinite(baseColor)?baseColor:0.0,0.0,1.0);roughness=PbrClampedRoughness(roughness);normalLight=(std::clamp)(std::isfinite(normalLight)?normalLight:0.0,0.0,1.0);strandHalf=(std::clamp)(std::isfinite(strandHalf)?strandHalf:1.0,-1.0,1.0);shiftedStrandHalf=(std::clamp)(std::isfinite(shiftedStrandHalf)?shiftedStrandHalf:1.0,-1.0,1.0);const double smoothness=1.0-roughness,primarySine=std::sqrt((std::max)(0.0,1.0-strandHalf*strandHalf)),secondarySine=std::sqrt((std::max)(0.0,1.0-shiftedStrandHalf*shiftedStrandHalf)),primary=std::pow(primarySine,18.0+(72.0-18.0)*smoothness),secondary=std::pow(secondarySine,36.0+(112.0-36.0)*smoothness),reflectance=.04*(1.0-.18)+baseColor*.18,supplement=radiance*reflectance*normalLight*(primary*.12+secondary*.06);return(std::clamp)(supplement,0.0,radiance*.24);}

inline double PbrPointLightAttenuation(double distance,double radius){if(!std::isfinite(distance)||!std::isfinite(radius)||distance<0||radius<=0)return 0;const double distanceSquared=distance*distance,safeRadius=(std::max)(radius,.001),normalized=distanceSquared/(safeRadius*safeRadius),window=(std::clamp)(1.0-normalized*normalized,0.0,1.0);return window*window/(std::max)(distanceSquared+1.0,.0001);}

inline double PbrAcesToneMap(double color){color=(std::max)(color,0.0)*1.1;return(std::clamp)((color*(2.51*color+.03))/(color*(2.43*color+.59)+.14),0.0,1.0);}
inline double PbrLinearToSrgb(double linearColor){if(!std::isfinite(linearColor))return 0;linearColor=(std::clamp)(linearColor,0.0,1.0);return linearColor<=.0031308?linearColor*12.92:1.055*std::pow(linearColor,1.0/2.4)-.055;}
inline double PbrSrgbToLinear(double srgbColor){if(!std::isfinite(srgbColor))return 0;srgbColor=(std::clamp)(srgbColor,0.0,1.0);return srgbColor<=.04045?srgbColor/12.92:std::pow((srgbColor+.055)/1.055,2.4);}
inline double PbrLinearBlendToSrgb(double sourceLinear,double destinationSrgb,double sourceAlpha){sourceAlpha=(std::clamp)(std::isfinite(sourceAlpha)?sourceAlpha:0.0,0.0,1.0);const double blended=(std::max)(0.0,sourceLinear)*sourceAlpha+PbrSrgbToLinear(destinationSrgb)*(1.0-sourceAlpha);return PbrLinearToSrgb(blended);}
inline double PbrToneMapToSrgb(double color){return PbrLinearToSrgb(PbrAcesToneMap(color));}
inline double PbrExposureScale(double compensationEv){compensationEv=(std::clamp)(std::isfinite(compensationEv)?compensationEv:0.0,-10.0,10.0);return std::exp2(compensationEv);}
inline double PbrToneMapToSrgb(double color,double compensationEv){return PbrToneMapToSrgb(color*PbrExposureScale(compensationEv));}
inline double PbrApplyExposure(double color,double compensationEv){return(std::max)(std::isfinite(color)?color:0.0,0.0)*PbrExposureScale(compensationEv);}
inline double PbrBlendHdr(double source,double destination,double sourceAlpha){sourceAlpha=(std::clamp)(std::isfinite(sourceAlpha)?sourceAlpha:0.0,0.0,1.0);return(std::max)(std::isfinite(source)?source:0.0,0.0)*sourceAlpha+(std::max)(std::isfinite(destination)?destination:0.0,0.0)*(1.0-sourceAlpha);}
inline double PbrCompositeHdrToSrgb(double color){return PbrToneMapToSrgb(color);}

}
