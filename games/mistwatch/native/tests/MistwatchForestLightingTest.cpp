#include "MistwatchForestLighting.h"
#include <array>
#include <bit>
#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

namespace {
using namespace Aevrynd;
namespace F=Mistwatch::ForestLighting;
unsigned checks{},negatives{};
void Need(bool value,const char* why){++checks;if(!value)throw std::runtime_error(why);}
bool Bits(float a,float b){return std::bit_cast<std::uint32_t>(a)==std::bit_cast<std::uint32_t>(b);}
template<std::size_t N>void Same(const std::array<float,N>& actual,const std::array<float,N>& expected,const char* why){
    for(std::size_t i=0;i<N;++i)Need(Bits(actual[i],expected[i]),why);
}
std::array<float,9> Fields(const DirectionalLight3D& l){
    return {l.direction.x,l.direction.y,l.direction.z,l.ambient,l.color.r,l.color.g,l.color.b,l.color.a,l.intensity};
}
std::array<float,22> Fields(const PhaseRadianceProfile3D& p){
    return {p.skyZenith.x,p.skyZenith.y,p.skyZenith.z,p.skyIntensity,
        p.skyHorizon.x,p.skyHorizon.y,p.skyHorizon.z,p.horizonExponent,
        p.groundBounce.x,p.groundBounce.y,p.groundBounce.z,p.sunAngularRadius,
        p.analyticDiffuse.x,p.analyticDiffuse.y,p.analyticDiffuse.z,p.analyticDiffuseIntensity,
        p.sunDisk.x,p.sunDisk.y,p.sunDisk.z,p.sunDiskIntensity,p.lunarDetailStrength,p.stellarDetailStrength};
}
template<class Work>void Reject(Work work,std::string_view reason){
    bool rejected=false;
    try{work();}catch(const std::invalid_argument& e){
        Need(std::string_view(e.what())==reason,"wrong rejection reason");rejected=true;
    }
    Need(rejected,"invalid profile or context accepted");++negatives;
}
void ProfileCases(){
    const std::wstring candidate=L"forest-ambient-contrast-v1";
    Need(F::CandidateName=="forest-ambient-contrast-v1","candidate name changed");
    for(bool capture:{false,true})for(bool replay:{false,true}){
        Need(!F::Resolve(capture,replay,L""),"blank must retain original in all contexts");
        Need(!F::Resolve(capture,replay,L"original"),"original must remain available in all contexts");
        if(capture!=replay)Need(F::Resolve(capture,replay,candidate),"candidate needs exactly one diagnostic context");
        else Reject([&]{(void)F::Resolve(capture,replay,candidate);},"FOREST_LIGHTING_REQUIRED_CONTEXT");
    }
    // 精確名字與完整長度才合法；不修剪、不忽略大小寫、不接受前綴或控制字。
    std::vector<std::wstring> invalid{
        L"unknown",L"Original",L"ORIGINAL",L" original",L"original ",L"original\n",
        L"forest-ambient-contrast",L"forest-ambient-contrast-v",L"forest-ambient-contrast-v2",
        L"FOREST-AMBIENT-CONTRAST-V1",L"Forest-ambient-contrast-v1",
        L"forest-ambient-contrast-v1-extra",L"xforest-ambient-contrast-v1",
        L" forest-ambient-contrast-v1",L"forest-ambient-contrast-v1 ",
        L"forest-ambient-contrast-v1\t",L"forest-ambient-contrast-v1\n",
        L"forest-ambient-contrast-v1\r",L"\t",L"\n",L"\r",L" ",L"\x1f",
        L"forest-ambient-contrast-v1\u00a0",L"forest-ambient-contrast-v1\u200b",
        std::wstring(65536,L'x'),candidate+std::wstring(65536,L'x'),
        std::wstring(L"original\0suffix",15),candidate+std::wstring(1,L'\0')};
    for(const auto& name:invalid)for(bool capture:{false,true})for(bool replay:{false,true})
        Reject([&]{(void)F::Resolve(capture,replay,std::wstring_view(name.data(),name.size()));},"FOREST_LIGHTING_PROFILE");
}
void LightCases(){
    // 獨立保留原 OnRender 定義，避免把被測函式自己當預期值。
    const DirectionalLight3D legacy{{.4f,-.85f,.3f},.55f,{1.f,.95f,.85f,1.f},1.5f};
    auto expectedCandidate=legacy;expectedCandidate.ambient=.28f;
    const auto original=F::Light(false),candidate=F::Light(true);
    Same(Fields(original),Fields(legacy),"original light bit mismatch");
    Same(Fields(candidate),Fields(expectedCandidate),"candidate changed a non-ambient field");
    const auto oldFields=Fields(original),newFields=Fields(candidate);
    unsigned different{};for(std::size_t i=0;i<oldFields.size();++i){
        if(!Bits(oldFields[i],newFields[i])){++different;Need(i==3,"only ambient may change");}
    }
    Need(different==1,"single-variable candidate must change exactly ambient");
    for(bool capture:{false,true})for(bool replay:{false,true}){
        Same(Fields(F::Light(F::Resolve(capture,replay,L""))),Fields(legacy),"blank light diverges");
        Same(Fields(F::Light(F::Resolve(capture,replay,L"original"))),Fields(legacy),"original light diverges");
    }
    Same(Fields(F::Light(F::Resolve(true,false,L"forest-ambient-contrast-v1"))),Fields(expectedCandidate),"capture light diverges");
    Same(Fields(F::Light(F::Resolve(false,true,L"forest-ambient-contrast-v1"))),Fields(expectedCandidate),"replay light diverges");
}
void DrawCases(){
    DrawList original,candidate;
    const float exposure=.375f;
    PhaseRadianceProfile3D phase;
    phase.skyZenith={.09f,.14f,.23f};phase.skyIntensity=.875f;
    phase.skyHorizon={.33f,.27f,.19f};phase.horizonExponent=.65f;
    phase.groundBounce={.04f,.035f,.025f};phase.sunAngularRadius=.0125f;
    phase.analyticDiffuse={.24f,.25f,.26f};phase.analyticDiffuseIntensity=.725f;
    phase.sunDisk={.95f,.83f,.64f};phase.sunDiskIntensity=5.5f;
    phase.phaseFingerprint=0x464f524553544142ull;phase.lunarDetailStrength=.25f;phase.stellarDetailStrength=.375f;
    for(auto* draw:{&original,&candidate}){
        draw->SetExposureCompensation(exposure);
        Need(draw->SetPhaseRadianceProfile(phase),"sentinel phase rejected");
        draw->SetVolumetricAtmospherePhase(VolumetricAtmospherePhase3D::Night);
    }
    const auto baselinePhase=original.PhaseRadianceProfile();
    const auto baselineExposure=original.ExposureCompensation();
    const auto baselineTimeOfDay=original.TimeOfDayExposure();
    const auto baselineBlend=original.EnvironmentLightingBlend();
    original.SetDirectionalLight(F::Light(false));candidate.SetDirectionalLight(F::Light(true));
    const auto old=original.DirectionalLight(),now=candidate.DirectionalLight();
    Need(Bits(old.ambient,.55f)&&Bits(now.ambient,.28f),"DrawList lost submitted ambient");
    const auto oldFields=Fields(old),newFields=Fields(now);
    for(std::size_t i=0;i<oldFields.size();++i)if(i!=3)Need(Bits(oldFields[i],newFields[i]),"normalized sun or direct light changed");
    const auto expectedDirection=Normalize(Vec3{.4f,-.85f,.3f});
    Same(std::array<float,3>{old.direction.x,old.direction.y,old.direction.z},
        std::array<float,3>{expectedDirection.x,expectedDirection.y,expectedDirection.z},"DrawList sun normalization diverges");
    for(const auto* draw:{&original,&candidate}){
        Need(Bits(draw->ExposureCompensation(),baselineExposure)&&Bits(baselineExposure,exposure),"exposure changed by lighting candidate");
        Same(Fields(draw->PhaseRadianceProfile()),Fields(baselinePhase),"phase radiance changed by lighting candidate");
        Need(draw->PhaseRadianceProfile().phaseFingerprint==baselinePhase.phaseFingerprint,"phase identity changed");
        Need(draw->VolumetricAtmospherePhase()==VolumetricAtmospherePhase3D::Night,"phase selection changed");
        Need(draw->TimeOfDayExposure()==baselineTimeOfDay,"time-of-day exposure changed");
        Need(draw->EnvironmentLightingBlend()==baselineBlend,"environment blend changed");
    }
}
std::vector<std::wstring> PlayableNames(){
    const std::wstring candidate=L"forest-ambient-contrast-v1";
    return {L"",L"original",candidate,
        L"unknown",L"Original",L"ORIGINAL",L" original",L"original ",L"original\n",
        L"forest-ambient-contrast",L"forest-ambient-contrast-v",L"forest-ambient-contrast-v2",
        L"FOREST-AMBIENT-CONTRAST-V1",L"Forest-ambient-contrast-v1",
        L"forest-ambient-contrast-v1-extra",L"xforest-ambient-contrast-v1",
        L" forest-ambient-contrast-v1",L"forest-ambient-contrast-v1 ",
        L"forest-ambient-contrast-v1\t",L"forest-ambient-contrast-v1\n",
        L"forest-ambient-contrast-v1\r",L"\t",L"\n",L"\r",L" ",L"\x1f",
        L"forest-ambient-contrast-v1\u00a0",L"forest-ambient-contrast-v1\u200b",
        std::wstring(65536,L'x'),candidate+std::wstring(65536,L'x'),
        std::wstring(L"original\0suffix",15),candidate+std::wstring(1,L'\0'),
        std::wstring(1,L'\0'),std::wstring(L"\0original",9),
        std::wstring(L"forest-ambient")+std::wstring(1,L'\0')+L"-contrast-v1"};
}
void PlayableProfileCases(){
    const auto names=PlayableNames();
    // 八種布林情境都必須有確切結果；context 錯誤優先於名字，不能因空字串繞過。
    for(bool capture:{false,true})for(bool replay:{false,true})for(bool ibl:{false,true}){
        for(std::size_t i=0;i<names.size();++i){
            const auto& name=names[i];const std::wstring_view requested(name.data(),name.size());
            if(capture&&replay){
                Reject([&]{(void)F::ResolvePlayable(capture,replay,ibl,requested);},
                    "FOREST_LIGHTING_PLAYABLE_CONTEXT_CONFLICT");
            }else if(!capture&&!replay&&ibl){
                Reject([&]{(void)F::ResolvePlayable(capture,replay,ibl,requested);},
                    "FOREST_LIGHTING_PLAYABLE_IBL_CONTEXT");
            }else if(!capture&&!replay){
                Need(F::ResolvePlayable(capture,replay,ibl,requested),"playable normal inherited diagnostic environment");
            }else if(i>=3){
                Reject([&]{(void)F::ResolvePlayable(capture,replay,ibl,requested);},"FOREST_LIGHTING_PROFILE");
            }else{
                // 獨立列出預期，不以新 ResolvePlayable 自己作 oracle。
                const bool expected=capture?(i==2):(i==0?!ibl:i==2);
                Need(F::ResolvePlayable(capture,replay,ibl,requested)==expected,"playable exact profile decision");
                if(capture||i!=0)
                    Need(F::ResolvePlayable(capture,replay,ibl,requested)==F::Resolve(capture,replay,requested),
                        "playable explicit diagnostic changed legacy API");
            }
        }
    }
    Need(F::ResolvePlayable(false,false,false,L"")==F::ResolvePlayable(false,true,false,L""),
        "normal and default replay lighting differ");
    Need(!F::ResolvePlayable(true,false,true,L"")&&!F::ResolvePlayable(false,true,true,L""),
        "IBL empty request lost original isolation");
    Need(!F::ResolvePlayable(false,true,false,L"original")&&!F::ResolvePlayable(false,true,true,L"original"),
        "replay explicit original no longer available");
    // 顯式要求補光候選的意義保留；IBL 同時要求時由既有 Main guard 拒絕混光。
    Need(F::ResolvePlayable(true,false,true,L"forest-ambient-contrast-v1")&&
        F::ResolvePlayable(false,true,true,L"forest-ambient-contrast-v1"),"explicit candidate meaning changed");
}
void PlayableLightAndDrawCases(){
    // 使用獨立 literal 檢查新入口，只准 ambient .55/.28 有差，並保留夜間狀態。
    const DirectionalLight3D legacy{{.4f,-.85f,.3f},.55f,{1.f,.95f,.85f,1.f},1.5f};
    auto contrast=legacy;contrast.ambient=.28f;
    const auto names=PlayableNames();
    for(bool capture:{false,true})for(bool replay:{false,true})for(bool ibl:{false,true}){
        if((capture&&replay)||(!capture&&!replay&&ibl))continue;
        for(std::size_t i=0;i<names.size();++i){
            if((capture||replay)&&i>=3)continue;
            const auto& name=names[i];const std::wstring_view requested(name.data(),name.size());
            const bool expected=(!capture&&!replay)?true:capture?(i==2):(i==0?!ibl:i==2);
            const auto selected=F::Light(F::ResolvePlayable(capture,replay,ibl,requested));
            const auto expectedLight=expected?contrast:legacy;
            Same(Fields(selected),Fields(expectedLight),"playable Light changed literal fields");
            DrawList draw;
            const float exposure=.375f;draw.SetExposureCompensation(exposure);
            PhaseRadianceProfile3D phase;
            phase.skyZenith={.09f,.14f,.23f};phase.skyIntensity=.875f;
            phase.skyHorizon={.33f,.27f,.19f};phase.horizonExponent=.65f;
            phase.groundBounce={.04f,.035f,.025f};phase.sunAngularRadius=.0125f;
            phase.analyticDiffuse={.24f,.25f,.26f};phase.analyticDiffuseIntensity=.725f;
            phase.sunDisk={.95f,.83f,.64f};phase.sunDiskIntensity=5.5f;
            phase.phaseFingerprint=0x464f524553544142ull;phase.lunarDetailStrength=.25f;phase.stellarDetailStrength=.375f;
            Need(draw.SetPhaseRadianceProfile(phase),"playable sentinel phase rejected");
            draw.SetVolumetricAtmospherePhase(VolumetricAtmospherePhase3D::Night);
            const auto baselinePhase=draw.PhaseRadianceProfile();
            const auto baselineTimeOfDay=draw.TimeOfDayExposure();
            const auto baselineBlend=draw.EnvironmentLightingBlend();
            draw.SetDirectionalLight(selected);
            const auto submitted=draw.DirectionalLight();
            const auto direction=Normalize(Vec3{.4f,-.85f,.3f});
            Same(Fields(submitted),std::array<float,9>{direction.x,direction.y,direction.z,
                expected?.28f:.55f,1.f,.95f,.85f,1.f,1.5f},"playable DrawList direct light changed");
            Need(Bits(draw.ExposureCompensation(),exposure),"playable exposure mutated");
            Same(Fields(draw.PhaseRadianceProfile()),Fields(baselinePhase),"playable radiance mutated");
            Need(draw.PhaseRadianceProfile().phaseFingerprint==baselinePhase.phaseFingerprint,
                "playable phase fingerprint mutated");
            Need(draw.VolumetricAtmospherePhase()==VolumetricAtmospherePhase3D::Night,"playable night phase mutated");
            Need(draw.TimeOfDayExposure()==baselineTimeOfDay,"playable time-of-day mutated");
            Need(draw.EnvironmentLightingBlend()==baselineBlend,"playable environment blend mutated");
        }
    }
}
}
int main(){try{
    ProfileCases();LightCases();DrawCases();PlayableProfileCases();PlayableLightAndDrawCases();
    std::cout<<"FOREST_LIGHTING_CPU_CONTRACT_OK checks="<<checks<<" negatives="<<negatives
        <<" shader_verified=0 physical_lighting_verified=0 art_accepted=0\n";
    return 0;
}catch(const std::exception& e){std::cerr<<"FOREST_LIGHTING_CPU_CONTRACT_FAIL: "<<e.what()<<'\n';return 1;}}
