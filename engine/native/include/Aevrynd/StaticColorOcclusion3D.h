#pragma once
#include "Renderer.h"
#include <type_traits>

namespace Aevrynd {
enum class StaticColorOcclusionStatus3D {Invalid,NotVisible,Applied,GenerationConflict,CapacityRejected};
struct StaticColorOcclusionResult3D {
    StaticColorOcclusionStatus3D status{StaticColorOcclusionStatus3D::Invalid};
    std::size_t matchedColor{},shadowCopies{},generationConflicts{};
};
// 只改不透明色彩/深度覆蓋。原來投影的實例原樣複製到ShadowOnly，保留原LOD與陰影dither。
// 同一幀重複呼叫不再複製已拆出的陰影；無效要求及reserve失敗不改任何實例欄位。
inline StaticColorOcclusionResult3D ApplyStaticColorOcclusion3D(std::vector<MeshInstance3D>& items,
    RenderObjectIdentity3D identity,float opacity)noexcept {
    StaticColorOcclusionResult3D result;
    if(!identity||!ValidOcclusionOpacity3D(opacity))return result;
    const auto count=items.size();std::size_t copies{};
    for(const auto& item:items){
        if(item.identity.id==identity.id&&item.identity.generation!=identity.generation)++result.generationConflicts;
        if(item.identity==identity&&item.layer==MeshRenderLayer3D::Opaque){
            ++result.matchedColor;if(opacity<1.f&&item.castsShadow)++copies;
        }
    }
    if(result.generationConflicts){result.status=StaticColorOcclusionStatus3D::GenerationConflict;return result;}
    if(!result.matchedColor){result.status=StaticColorOcclusionStatus3D::NotVisible;return result;}
    if(result.matchedColor>64||copies>items.max_size()-count){result.status=StaticColorOcclusionStatus3D::CapacityRejected;return result;}
    static_assert(std::is_nothrow_copy_constructible_v<MeshInstance3D>);
    try{items.reserve(count+copies);}catch(...){result.status=StaticColorOcclusionStatus3D::CapacityRejected;return result;}
    for(std::size_t i=0;i<count;++i){
        auto& item=items[i];if(item.identity!=identity||item.layer!=MeshRenderLayer3D::Opaque)continue;
        if(opacity<1.f&&item.castsShadow){
            auto shadow=item;shadow.layer=MeshRenderLayer3D::ShadowOnly;items.push_back(shadow);
            item.castsShadow=false;item.shadowCascadeMask=0;++result.shadowCopies;
        }
        item.occlusionOpacity=opacity;
    }
    result.status=StaticColorOcclusionStatus3D::Applied;return result;
}
}
