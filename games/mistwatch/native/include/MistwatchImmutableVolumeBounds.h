#pragma once
#include "MistwatchCanopyVolumeClearance.h"
#include <vector>

namespace Mistwatch::CanopyVolume {
// 僅借用單次配置器呼叫期間不變的來源；不得跨來源、跨執行緒或跨呼叫重用。
// 惰性記憶只省略同一木件的純bounds重算，保留原驗證、遍歷與短路順序。
class ImmutableVolumeBoundsCache final {
    enum class State : unsigned char {Uncomputed,Valid,Invalid};
    struct Entry {Detail::Bounds bounds{};State state{State::Uncomputed};};
    const std::span<const Aevrynd::BoxCollider3D> source_;
    std::vector<Entry> entries_;
    std::size_t computed_{};
public:
    explicit ImmutableVolumeBoundsCache(std::span<const Aevrynd::BoxCollider3D> source)noexcept:source_(source){}
    ImmutableVolumeBoundsCache(const ImmutableVolumeBoundsCache&)=delete;
    ImmutableVolumeBoundsCache& operator=(const ImmutableVolumeBoundsCache&)=delete;
    ImmutableVolumeBoundsCache(ImmutableVolumeBoundsCache&&)=delete;
    ImmutableVolumeBoundsCache& operator=(ImmutableVolumeBoundsCache&&)=delete;
    [[nodiscard]] std::size_t SourceParts()const noexcept{return source_.size();}
    [[nodiscard]] std::size_t StorageParts()const noexcept{return entries_.size();}
    [[nodiscard]] std::size_t ComputedParts()const noexcept{return computed_;}
    [[nodiscard]] bool Clears(const PlantVolume& plant,float touchMargin=.03f) {
        Detail::Bounds plantBounds;
        // 與原helper相同：先驗植物，再驗來源上限；零件尾端不能提前檢查。
        if(!Detail::PlantBounds(plant,touchMargin,plantBounds)||source_.size()>10000)return false;
        // 空來源、壞植物、超額來源及未被呼叫的分支完全不配置cache。
        // 配置失敗照實向外傳遞bad_alloc，不把記憶體不足偽裝成植物碰撞。
        if(entries_.empty()&&!source_.empty())entries_.resize(source_.size());
        for(std::size_t index=0;index<source_.size();++index){
            auto& entry=entries_[index];
            if(entry.state==State::Uncomputed){
                ++computed_;
                entry.state=Detail::WoodBounds(source_[index],entry.bounds)?State::Valid:State::Invalid;
            }
            if(entry.state==State::Invalid)return false;
            const auto& woodBounds=entry.bounds;
            bool separated=false;
            for(unsigned axis=0;axis<3;++axis)separated|=plantBounds[axis].maximum<woodBounds[axis].minimum||
                woodBounds[axis].maximum<plantBounds[axis].minimum;
            if(!separated)return false;
        }
        return true;
    }
};
}
