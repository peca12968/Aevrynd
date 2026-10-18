#pragma once
#include <Aevrynd/Renderer.h>
#include <stdexcept>
#include <string_view>

namespace Mistwatch::ForestLighting {
inline constexpr std::string_view CandidateName="forest-ambient-contrast-v1";
inline bool Resolve(bool capture,bool replay,std::wstring_view name){
    if(name.empty()||name==L"original")return false;
    if(name!=L"forest-ambient-contrast-v1")throw std::invalid_argument("FOREST_LIGHTING_PROFILE");
    if(capture==replay)throw std::invalid_argument("FOREST_LIGHTING_REQUIRED_CONTEXT");
    return true;
}
// 可玩入口採用已比較的補光版本；舊診斷 API 與單變量 original 對照保持原義。
// IBL 的顯式組合仍交給呼叫者的既有隔離閘門，不把兩種光照實驗悄悄混成新預設。
inline bool ResolvePlayable(bool capture,bool replay,bool independentIblCandidate,std::wstring_view requested){
    if(capture&&replay)throw std::invalid_argument("FOREST_LIGHTING_PLAYABLE_CONTEXT_CONFLICT");
    if(!capture&&!replay){
        if(independentIblCandidate)throw std::invalid_argument("FOREST_LIGHTING_PLAYABLE_IBL_CONTEXT");
        // 一般玩家不繼承過去留在環境中的診斷字串，包含 original 與不合法的名字。
        return true;
    }
    if(capture)return Resolve(true,false,requested);
    // 一般 replay 與正常遊玩一致；只有獨立 IBL 的空請求保留原 .55 補光作對照。
    if(requested.empty())return !independentIblCandidate;
    return Resolve(false,true,requested);
}
// 單變量光照候選：只減少全場補光，不改太陽、曝光、材質、天空或可玩世界。
// 這不是物理照明或美術合格的證明；必須由真提交與同鏡頭畫面驗收。
inline Aevrynd::DirectionalLight3D Light(bool candidate){
    return {{.4f,-.85f,.3f},candidate?.28f:.55f,{1.f,.95f,.85f,1.f},1.5f};
}
}
