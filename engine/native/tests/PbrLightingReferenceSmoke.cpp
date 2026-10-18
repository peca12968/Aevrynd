#include <Aevrynd/PbrLightingReference.h>
#include <bit>
#include <cstdint>
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>
#include <string>

using namespace Aevrynd;

namespace {
double BoundedObjectFillRadianceReference(double radiance,double intensity,double peakRadiance) {
    const double authored=(std::max)(0.0,radiance*intensity);
    const double peak=(std::max)(0.0,peakRadiance*intensity);
    const double coupledScale=(std::min)(.24,.12/(std::max)(peak,.000001));
    return authored*coupledScale;
}

double ProductionObjectFillReference(double normalView,double baseColor,double roughness,
                                     double metallic,double occlusion,double waterMask,
                                     double terrainMask,double foliageMask,bool environmentEnabled,
                                     double radiance,double intensity,double peakRadiance) {
    const auto unit=[](double value){return std::clamp(value,0.0,1.0);};
    const double boundedRadiance=BoundedObjectFillRadianceReference(radiance,intensity,peakRadiance);
    const double viewShape=.35+.65*unit(normalView);
    const double aoVisibility=.35+.65*unit(occlusion);
    const double metal=unit(metallic),view=unit(normalView),color=unit(baseColor);
    const double reflectance=.04+(color-.04)*metal;
    const double grazing=(std::max)(1.0-unit(roughness),reflectance);
    const double fresnel=reflectance+(grazing-reflectance)*std::pow(1.0-view,5.0);
    const double eligibility=(1.0-unit(waterMask))*(1.0-unit(terrainMask))
                            *(1.0-unit(foliageMask))*(environmentEnabled?1.0:0.0);
    return boundedRadiance*viewShape*aoVisibility*eligibility
          *(1.0-fresnel)*(1.0-metal)*color;
}

bool HasProductionObjectFillContract() {
    const auto shaderPath=std::filesystem::path(__FILE__).parent_path().parent_path()/"shaders"/"World.hlsl";
    std::ifstream input(shaderPath,std::ios::binary);
    const std::string source{std::istreambuf_iterator<char>{input},std::istreambuf_iterator<char>{}};
    const auto diffuseBegin=source.find("float3 EvaluateEnvironmentDiffuse(");
    const auto diffuseEnd=diffuseBegin==std::string::npos?std::string::npos:source.find("float3 BoxProjectedEnvironmentDirection",diffuseBegin);
    const auto fillBegin=source.find("float3 EvaluateProductionObjectFill(");
    const auto fillEnd=fillBegin==std::string::npos?std::string::npos:source.find("float3 EvaluateEnvironmentLighting(",fillBegin);
    if(diffuseBegin==std::string::npos||diffuseEnd==std::string::npos||fillBegin==std::string::npos||fillEnd==std::string::npos)return false;
    const auto diffuse=source.substr(diffuseBegin,diffuseEnd-diffuseBegin);
    const auto fill=source.substr(fillBegin,fillEnd-fillBegin);
    return diffuse.find("environmentObjectFill")==std::string::npos
        && fill.find("viewShape")!=std::string::npos
        && fill.find("aoVisibility")!=std::string::npos
        && fill.find("waterMask")!=std::string::npos
        && fill.find("boundedRadiance")!=std::string::npos;
}
}

namespace {
bool TerrainGeometricLightCases(){
    constexpr double pi=3.141592653589793;
    unsigned positiveCases{},grazingCases{},blockedCases{};
    // 幾何可見且 Ns·V>0：必直接沿用舊函式的 double bits，非近似相等。
    for(double base:{0.0,.3,1.0})for(double metal:{0.0,.5,1.0})
    for(double roughness:{.045,.5,1.0})for(double view:{.01,.8,1.0}){
        const double original=PbrReferenceEvaluateLight(base,metal,roughness,view,.6,.7,.8,3);
        const double candidate=PbrReferenceEvaluateTerrainLight(base,metal,roughness,
            .6,1,view,.6,.7,.8,3);
        if(std::bit_cast<std::uint64_t>(original)!=std::bit_cast<std::uint64_t>(candidate))return false;
        ++positiveCases;
    }
    // 原函式仍拒 shading 背視；新分支的 oracle 為原 Fresnel diffuse，不含 GGX。
    for(double base:{0.0,.3,1.0})for(double metal:{0.0,.5,1.0})
    for(double view:{-1.0,-.28,0.0})for(double light:{.01,.6,1.0}){
        const double reflectance=.04*(1-metal)+base*metal;
        const double diffuse=(1-PbrFresnelSchlick(.8,reflectance))*(1-metal)*base;
        const double expected=diffuse*(1/pi)*3*light;
        const double value=PbrReferenceEvaluateTerrainLight(base,metal,.045,.6,1,
            view,light,.7,.8,3);
        const double rough=PbrReferenceEvaluateTerrainLight(base,metal,1,.6,1,
            view,light,.7,.8,3);
        const double bound=base*(1/pi)*3*light;
        const double tolerance=16*std::numeric_limits<double>::epsilon()*(std::max)(1.0,bound);
        if(PbrReferenceEvaluateLight(base,metal,.045,view,light,.7,.8,3)!=0
            ||!std::isfinite(value)||value<0||std::abs(value-expected)>tolerance
            ||value>bound+tolerance||std::bit_cast<std::uint64_t>(value)!=std::bit_cast<std::uint64_t>(rough)
            ||(metal==1&&value!=0)||(base>0&&metal<1&&!(value>0)))return false;
        ++grazingCases;
    }
    // Ng 背面不得漏光，即使 shading normal 同時對光與觀察者可見。
    for(double blocked:{-1.0,0.0})for(double view:{-.28,0.0,.8}){
        if(PbrReferenceEvaluateTerrainLight(.7,0,.5,blocked,1,view,.6,.7,.8,3)!=0
            ||PbrReferenceEvaluateTerrainLight(.7,0,.5,.6,blocked,view,.6,.7,.8,3)!=0)return false;
        ++blockedCases;
    }
    for(double light:{-1.0,0.0})if(PbrReferenceEvaluateTerrainLight(.7,0,.5,.6,1,
        -.28,light,.7,.8,3)!=0)return false;
    // 真單位向量 Ng=(0,0,1)、Ns=(-.8,0,.6)、V=(.8,0,.6)、L=Ng。
    const double halfLength=std::sqrt(.8*.8+1.6*1.6);
    const double shadingView=-.8*.8+.6*.6;
    const double normalHalf=(-.8*.8+.6*1.6)/halfLength;
    const double viewHalf=(.8*.8+.6*1.6)/halfLength;
    const double witness=PbrReferenceEvaluateTerrainLight(.7,0,.5,.6,1,
        shadingView,.6,normalHalf,viewHalf,3);
    const double expected=(1-PbrFresnelSchlick(viewHalf,.04))*.7*(1/pi)*3*.6;
    if(!(shadingView<0)||!(witness>0)||std::abs(witness-expected)>16*std::numeric_limits<double>::epsilon()
        ||PbrReferenceEvaluateLight(.7,0,.5,shadingView,.6,normalHalf,viewHalf,3)!=0)return false;
    // 原 reference 輻射量防護亦保留，沒有新 brightness floor。
    for(double radiance:{-1.0,std::numeric_limits<double>::infinity()})
        if(PbrReferenceEvaluateTerrainLight(.7,0,.5,.6,1,-.28,.6,.7,.8,radiance)!=0)return false;
    std::cout<<"terrainGeometricLight positive="<<positiveCases<<" grazing="<<grazingCases
        <<" blocked="<<blockedCases<<" witness="<<witness<<" oldEarlyOutPreserved=1\n";
    return true;
}
}

int main(){
    if(PbrClampedRoughness(0)!=.045||PbrClampedRoughness(2)!=1||PbrClampedRoughness(std::numeric_limits<double>::quiet_NaN())!=1||PbrClampedRoughness(std::numeric_limits<double>::infinity())!=1)return 1;
    for(double roughness:{0.0,.045,.2,.5,1.0})for(double metallic:{0.0,.5,1.0}){const PbrEnergyWeights energy=PbrReferenceEnergy(.7,metallic,roughness,.8,.7,.9,.85);if(!std::isfinite(energy.diffuse)||!std::isfinite(energy.specular)||!std::isfinite(energy.specularResponse)||energy.diffuse<0||energy.specular<0||energy.specularResponse<0||energy.diffuse+energy.specular>1.0000001)return 2;}
    const PbrEnergyWeights dielectric=PbrReferenceEnergy(.7,0,.5,1,1,1,1),metal=PbrReferenceEnergy(.7,1,.5,1,1,1,1);if(!(metal.diffuse<dielectric.diffuse)||metal.diffuse!=0)return 3;
    const double sharp=PbrReferenceEnergy(.7,0,.1,1,1,1,1).specularResponse,rough=PbrReferenceEnergy(.7,0,.8,1,1,1,1).specularResponse;if(!(sharp>rough))return 4;
    double previous=PbrPointLightAttenuation(0,10);for(double distance:{1.0,2.0,4.0,8.0,10.0,12.0}){const double attenuation=PbrPointLightAttenuation(distance,10);if(!std::isfinite(attenuation)||attenuation>previous+.0000001)return 5;previous=attenuation;}if(PbrPointLightAttenuation(10,10)!=0||PbrPointLightAttenuation(12,10)!=0)return 6;
    double mappedPrevious=0;for(double value:{0.0,.1,1.0,4.0,64.0}){const double mapped=PbrAcesToneMap(value);if(!std::isfinite(mapped)||mapped<0||mapped>1||mapped<mappedPrevious)return 7;mappedPrevious=mapped;}
    const PbrEnergyWeights grazing=PbrReferenceEnergy(1,0,.045,.00001,.00001,.5,.5);if(!std::isfinite(grazing.specularResponse))return 8;
    double srgbPrevious=0;for(double linear:{0.0,.001,.0031308,.018,.18,.5,1.0}){const double srgb=PbrLinearToSrgb(linear);if(!std::isfinite(srgb)||srgb<srgbPrevious||srgb<0||srgb>1){std::cerr<<"sRGB monotonic or finite check failed linear="<<linear<<" value="<<srgb<<'\n';return 9;}srgbPrevious=srgb;}const double srgbZero=PbrLinearToSrgb(0),srgbOne=PbrLinearToSrgb(1),srgbMid=PbrLinearToSrgb(.18),srgbHalf=PbrLinearToSrgb(.5);if(std::abs(srgbZero)>1e-12||std::abs(srgbOne-1)>1e-12||std::abs(srgbMid-.4613561295)>.0000001||std::abs(srgbHalf-.7353569831)>.0000001){std::cerr<<"sRGB reference mismatch zero="<<srgbZero<<" one="<<srgbOne<<" mid="<<srgbMid<<" half="<<srgbHalf<<'\n';return 10;}const double srgbBelow=PbrLinearToSrgb(-1),srgbAbove=PbrLinearToSrgb(2),srgbInfinite=PbrLinearToSrgb(std::numeric_limits<double>::infinity());if(std::abs(srgbBelow)>1e-12||std::abs(srgbAbove-1)>1e-12||std::abs(srgbInfinite)>1e-12){std::cerr<<"sRGB clamp check failed below="<<srgbBelow<<" above="<<srgbAbove<<" infinite="<<srgbInfinite<<'\n';return 11;}
    const double ggx045=PbrDistributionGgxStabilized(1,.045),ggx01=PbrDistributionGgxStabilized(1,.1);if(std::abs(ggx045-4.100625)>.0000001||std::abs(ggx01-100.0)>.000001){std::cerr<<"Stabilized GGX golden mismatch low="<<ggx045<<" high="<<ggx01<<'\n';return 12;}const double evaluated=PbrReferenceEvaluateLight(.7,.2,.5,.8,.7,.9,.85,3);if(std::abs(evaluated-.3561374190812342)>.000000001||PbrReferenceEvaluateLight(.7,.2,.5,.8,0,.9,.85,3)!=0||PbrReferenceEvaluateLight(.7,.2,.5,0,.7,.9,.85,3)!=0){std::cerr<<"Scalar PBR light golden or early-out failed value="<<evaluated<<'\n';return 13;}
    const double centerSmall=PbrPointLightAttenuation(0,2),centerLarge=PbrPointLightAttenuation(0,20),distanceSmall=PbrPointLightAttenuation(4,5),distanceLarge=PbrPointLightAttenuation(4,10);const double smallWindow=1-std::pow(4.0/5.0,4),largeWindow=1-std::pow(4.0/10.0,4);if(std::abs(centerSmall-1)>1e-12||std::abs(centerLarge-1)>1e-12||std::abs(distanceSmall*17-smallWindow*smallWindow)>1e-12||std::abs(distanceLarge*17-largeWindow*largeWindow)>1e-12||!(distanceLarge>distanceSmall)){std::cerr<<"Point radius/window semantics failed center="<<centerSmall<<','<<centerLarge<<" distance="<<distanceSmall<<','<<distanceLarge<<'\n';return 14;}
    const double combined18=PbrToneMapToSrgb(.18),combined1=PbrToneMapToSrgb(1),combined4=PbrToneMapToSrgb(4);if(std::abs(combined18-.5805850495887156)>1e-10||std::abs(combined1-.9178695857177466)>1e-10||std::abs(combined4-.9906239237950901)>1e-10){std::cerr<<"ACES to sRGB golden mismatch values="<<combined18<<','<<combined1<<','<<combined4<<'\n';return 15;}
    for(double linear:{0.0,.0031308,.18,.5,1.0})if(std::abs(PbrSrgbToLinear(PbrLinearToSrgb(linear))-linear)>1e-9){std::cerr<<"sRGB round trip mismatch linear="<<linear<<'\n';return 16;}
    const double linearHalf=PbrLinearBlendToSrgb(1,0,.5),gammaHalf=.5;if(std::abs(linearHalf-.7353569830524495)>1e-9||std::abs(linearHalf-gammaHalf)<.2||std::abs(PbrLinearBlendToSrgb(1,0,0))>1e-12||std::abs(PbrLinearBlendToSrgb(1,0,1)-1)>1e-12){std::cerr<<"Linear display blend mismatch value="<<linearHalf<<'\n';return 17;}
    if(std::abs(PbrExposureScale(1)-2)>1e-12||std::abs(PbrExposureScale(-1)-.5)>1e-12||std::abs(PbrExposureScale(std::numeric_limits<double>::infinity())-1)>1e-12||PbrExposureScale(20)!=1024||PbrExposureScale(-20)!=1.0/1024){std::cerr<<"Exposure scale clamp mismatch\n";return 18;}double exposurePrevious=0;for(double ev:{-10.0,-2.0,0.0,2.0,10.0}){const double exposed=PbrToneMapToSrgb(.18,ev);if(!std::isfinite(exposed)||exposed<exposurePrevious){std::cerr<<"Exposure monotonic mismatch ev="<<ev<<" value="<<exposed<<'\n';return 19;}exposurePrevious=exposed;}if(std::abs(PbrToneMapToSrgb(.18,0)-combined18)>1e-12){std::cerr<<"Zero EV baseline mismatch\n";return 20;}
    const double exposedHdr=PbrApplyExposure(4,1),blendedHdr=PbrBlendHdr(exposedHdr,.5,.25),composited=PbrCompositeHdrToSrgb(blendedHdr),incorrectOrder=PbrBlendHdr(PbrCompositeHdrToSrgb(exposedHdr),PbrCompositeHdrToSrgb(.5),.25);if(exposedHdr!=8||blendedHdr<=1||!std::isfinite(composited)||composited<=incorrectOrder+.01||PbrBlendHdr(4,2,0)!=2||PbrBlendHdr(4,2,1)!=4){std::cerr<<"HDR composition ordering mismatch exposed="<<exposedHdr<<" blended="<<blendedHdr<<" composite="<<composited<<" incorrect="<<incorrectOrder<<'\n';return 21;}
    const PbrEnvironmentWeights dielectricEnvironment=PbrReferenceEnvironment(.7,0,.25,.72,.9,2.4),metalEnvironment=PbrReferenceEnvironment(.7,1,.25,.72,.9,2.4),roughEnvironment=PbrReferenceEnvironment(.7,1,.85,.72,.9,2.4),invalidEnvironment=PbrReferenceEnvironment(.7,1,std::numeric_limits<double>::quiet_NaN(),.72,.9,2.4);if(!std::isfinite(dielectricEnvironment.total)||!std::isfinite(metalEnvironment.total)||!std::isfinite(invalidEnvironment.total)||dielectricEnvironment.diffuse<=0||metalEnvironment.diffuse!=0||metalEnvironment.specular<=roughEnvironment.specular||dielectricEnvironment.total<0||metalEnvironment.total<0){std::cerr<<"Analytic environment energy mismatch\n";return 22;}
    const double waterF0=PbrWaterReflectance(),waterNormal=PbrWaterFresnel(1),waterHalf=PbrWaterFresnel(.5),waterGrazing=PbrWaterFresnel(0);if(std::abs(waterF0-.0203731878419714)>1e-12||std::abs(waterNormal-waterF0)>1e-12||!(waterNormal<waterHalf&&waterHalf<waterGrazing)||std::abs(waterGrazing-1)>1e-12){std::cerr<<"Water Fresnel reference mismatch f0="<<waterF0<<'\n';return 23;}
    const double clearWater=PbrBeerLambertTransmittance(0,8),shallowWater=PbrBeerLambertTransmittance(.2,1),deepWater=PbrBeerLambertTransmittance(.2,5),redWater=PbrBeerLambertTransmittance(.42,5),greenWater=PbrBeerLambertTransmittance(.16,5),blueWater=PbrBeerLambertTransmittance(.08,5);if(clearWater!=1||!(deepWater<shallowWater)||std::abs(deepWater-.36787944117144233)>1e-12||!(redWater<greenWater&&greenWater<blueWater)){std::cerr<<"Beer-Lambert water absorption mismatch\n";return 24;}
    const double skinFront=PbrReferenceSkinSupplement(.72,.45,.8,.7,.9,.85,.1,1),skinBack=PbrReferenceSkinSupplement(.72,.45,.8,-.6,.9,.85,.9,1),skinInvalid=PbrReferenceSkinSupplement(.72,std::numeric_limits<double>::quiet_NaN(),.8,.7,.9,.85,.1,1);if(!std::isfinite(skinFront)||!std::isfinite(skinBack)||!std::isfinite(skinInvalid)||skinFront<0||skinBack<=0||skinFront>1||skinBack>1){std::cerr<<"Skin lighting reference is not finite or bounded\n";return 25;}
    const double hairSharp=PbrReferenceHairSupplement(.22,.18,.8,0,.18,3),hairOffAxis=PbrReferenceHairSupplement(.22,.18,.8,.8,.8,3),hairInvalid=PbrReferenceHairSupplement(.22,std::numeric_limits<double>::quiet_NaN(),.8,std::numeric_limits<double>::infinity(),.18,3),hairPeak=PbrReferenceHairSupplement(1,.045,1,0,0,10);if(!std::isfinite(hairSharp)||!std::isfinite(hairOffAxis)||!std::isfinite(hairInvalid)||!std::isfinite(hairPeak)||hairSharp<0||hairOffAxis<0||hairInvalid<0||hairPeak>2.4000001||hairSharp<=hairOffAxis){std::cerr<<"Hair lighting reference is not finite, bounded, or anisotropic\n";return 26;}
    const double corneaF0=PbrCorneaReflectance(),corneaNormal=PbrCorneaFresnel(1),corneaHalf=PbrCorneaFresnel(.5),corneaGrazing=PbrCorneaFresnel(0);if(std::abs(corneaF0-.0250427960865671)>1e-12||std::abs(corneaNormal-corneaF0)>1e-12||!(corneaNormal<corneaHalf&&corneaHalf<corneaGrazing)||std::abs(corneaGrazing-1)>1e-12||PbrCorneaRoughness(0)!=.025||PbrCorneaRoughness(1)!=.08||PbrCorneaRoughness(std::numeric_limits<double>::quiet_NaN())!=.08){std::cerr<<"Cornea Fresnel or roughness reference mismatch f0="<<corneaF0<<'\n';return 27;}
    const double eyeFront=PbrReferenceEyeLight(.42,.4,.9,.8,.92,.9,.9,.8,.92,2),eyeOffAxis=PbrReferenceEyeLight(.42,.4,.9,.8,.65,.65,.9,.8,.65,2),eyeInvalid=PbrReferenceEyeLight(.42,std::numeric_limits<double>::quiet_NaN(),.9,.8,.92,.9,.9,.8,.92,2),eyeNoCornea=PbrReferenceEyeLight(.42,.4,.9,.8,.92,.9,0,.8,.92,2);if(!std::isfinite(eyeFront)||!std::isfinite(eyeOffAxis)||!std::isfinite(eyeInvalid)||!std::isfinite(eyeNoCornea)||eyeFront<0||eyeOffAxis<0||eyeNoCornea<0||eyeFront<=eyeOffAxis){std::cerr<<"Eye clearcoat reference is not finite, bounded, or view dependent values="<<eyeFront<<','<<eyeOffAxis<<','<<eyeInvalid<<'\n';return 28;}
    const double eyeBase=PbrReferenceEvaluateLight(.42,0,.4,.9,.8,.92,.9,2),expectedNoCornea=eyeBase;if(std::abs(eyeNoCornea-expectedNoCornea)>1e-12||!std::isfinite(eyeBase*(1.0-corneaHalf))||eyeBase*(1.0-corneaHalf)>eyeBase){std::cerr<<"Eye base-layer energy conservation failed base="<<eyeBase<<" fallback="<<eyeNoCornea<<'\n';return 29;}
    const auto objectFill=[](double normalView,double baseColor=.65,double roughness=.45,double metallic=0,double occlusion=1,double water=0,double terrain=0,double foliage=0,bool environmentEnabled=true,double radiance=.72,double intensity=.58,double peakRadiance=1.0){return ProductionObjectFillReference(normalView,baseColor,roughness,metallic,occlusion,water,terrain,foliage,environmentEnabled,radiance,intensity,peakRadiance);};
    const double fillFront=objectFill(1),fillSide=objectFill(0),fillOccluded=objectFill(1,.65,.45,0,0),fillMetal=objectFill(1,.65,.45,1),fillWater=objectFill(1,.65,.45,0,1,1),fillTerrain=objectFill(1,.65,.45,0,1,0,1),fillFoliage=objectFill(1,.65,.45,0,1,0,0,1),fillEnvironmentNull=objectFill(1,.65,.45,0,1,0,0,0,false),fillDarkBase=objectFill(1,.2),fillBrightBase=objectFill(1,.8),fillSmoothGrazing=objectFill(.35,.65,.1),fillRoughGrazing=objectFill(.35,.65,.9),fillBeforeCap=BoundedObjectFillRadianceReference(.25,.8,.25),fillAfterCap=BoundedObjectFillRadianceReference(16,8,16),fillCoupledRed=BoundedObjectFillRadianceReference(.72,.58,1.0),fillCoupledBlue=BoundedObjectFillRadianceReference(1.0,.58,1.0);
    if(!(fillFront>fillSide&&fillSide>0&&fillOccluded>0&&fillOccluded<fillFront&&fillDarkBase<fillBrightBase&&fillSmoothGrazing<fillRoughGrazing&&fillCoupledRed<fillCoupledBlue)||fillMetal!=0||fillWater!=0||fillTerrain!=0||fillFoliage!=0||fillEnvironmentNull!=0||std::abs(fillBeforeCap-.048)>1e-12||std::abs(fillAfterCap-.12)>1e-12||std::abs(fillCoupledBlue-.12)>1e-12){std::cerr<<"Production object fill CPU contract failed front="<<fillFront<<" side="<<fillSide<<" occluded="<<fillOccluded<<" metal="<<fillMetal<<" water="<<fillWater<<" terrain="<<fillTerrain<<" foliage="<<fillFoliage<<" null="<<fillEnvironmentNull<<" base="<<fillDarkBase<<'/'<<fillBrightBase<<" roughness="<<fillSmoothGrazing<<'/'<<fillRoughGrazing<<" bounded="<<fillBeforeCap<<'/'<<fillAfterCap<<" coupled="<<fillCoupledRed<<'/'<<fillCoupledBlue<<'\n';return 30;}if(!HasProductionObjectFillContract()){std::cerr<<"Production object fill shader source contract failed\n";return 31;}
    if(!TerrainGeometricLightCases()){std::cerr<<"Terrain geometric/shading-normal CPU reference failed\n";return 32;}
    std::cout<<"sharp="<<sharp<<" rough="<<rough<<" attenuation4="<<PbrPointLightAttenuation(4,10)<<" aces4="<<PbrAcesToneMap(4)<<" srgb18="<<PbrLinearToSrgb(.18)<<" linearBlend50="<<linearHalf<<" hdrBlend="<<blendedHdr<<" environment="<<metalEnvironment.total<<" waterF0="<<waterF0<<" skin="<<skinFront<<" hair="<<hairSharp<<" corneaF0="<<corneaF0<<" eye="<<eyeFront<<" objectFillFront="<<fillFront<<'\n';return 0;
}
