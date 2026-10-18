#pragma once
#include "MistwatchCanopyVolumeClearance.h"
#include <stdexcept>
#include <vector>

namespace Mistwatch::FernColonyDetail {
// 此 owner 只代表建構當刻的完整木件；不接受外部 cache 或新來源身分來冒用舊資料。
// CreateFernColonies 每次入口自行建立，生命週期只涵蓋該次不可變作者輸入。
class ImmutableCanopyWoodBounds final {
    const std::vector<CanopyVolume::Detail::Bounds> bounds_;
    explicit ImmutableCanopyWoodBounds(std::vector<CanopyVolume::Detail::Bounds>&& values):bounds_(std::move(values)){}
public:
    ImmutableCanopyWoodBounds(const ImmutableCanopyWoodBounds&)=delete;
    ImmutableCanopyWoodBounds& operator=(const ImmutableCanopyWoodBounds&)=delete;
    [[nodiscard]] static ImmutableCanopyWoodBounds Create(std::span<const Aevrynd::BoxCollider3D> source) {
        if(source.size()>10000)throw std::invalid_argument("Invalid canopy wood parts for fern colonies");
        std::vector<CanopyVolume::Detail::Bounds> values;values.reserve(source.size());
        // 所有木件一次完整驗證，不能以遠離植物或零人口跳過壞資料。
        for(const auto& part:source){CanopyVolume::Detail::Bounds value;
            if(!CanopyVolume::Detail::WoodBounds(part,value))throw std::invalid_argument("Invalid canopy wood parts for fern colonies");
            values.push_back(value);
        }
        return ImmutableCanopyWoodBounds(std::move(values));
    }
    [[nodiscard]] std::size_t Size()const noexcept{return bounds_.size();}
    [[nodiscard]] bool Clears(const CanopyVolume::PlantVolume& plant,float touchMargin=.03f)const noexcept {
        CanopyVolume::Detail::Bounds plantBounds;
        if(!CanopyVolume::Detail::PlantBounds(plant,touchMargin,plantBounds))return false;
        const std::span<const CanopyVolume::Detail::Bounds> borrowed(bounds_);
        for(const auto& woodBounds:borrowed){
            bool separated=false;
            for(unsigned axis=0;axis<3;++axis)separated|=plantBounds[axis].maximum<woodBounds[axis].minimum||
                woodBounds[axis].maximum<plantBounds[axis].minimum;
            if(!separated)return false;
        }
        return true;
    }
};
}
