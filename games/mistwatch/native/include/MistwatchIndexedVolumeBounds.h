#pragma once
#include "MistwatchCanopyVolumeClearance.h"
#include <Aevrynd/StaticAabbIndex3D.h>
#include <memory>

namespace Mistwatch::CanopyVolume {
// 單一配置器呼叫內借用不變來源；建索引後的bounds為自有快照，不跨來源／執行緒重用。
// 舊lazy cache及其順序／計數契約不動；此入口只承諾相同完整布林判定。
class IndexedVolumeBounds final {
    const std::span<const Aevrynd::BoxCollider3D> source_;
    std::unique_ptr<Aevrynd::StaticAabbIndex3D> index_;
    bool invalid_{};
    std::size_t queries_{},boxesTested_{};
    static Aevrynd::ClosedAabb3D Convert(const Detail::Bounds& source)noexcept{
        Aevrynd::ClosedAabb3D output;
        for(unsigned a=0;a<3;++a){output.minimum[a]=source[a].minimum;output.maximum[a]=source[a].maximum;}
        return output;
    }
public:
    explicit IndexedVolumeBounds(std::span<const Aevrynd::BoxCollider3D> source)noexcept:source_(source){}
    IndexedVolumeBounds(const IndexedVolumeBounds&)=delete;
    IndexedVolumeBounds& operator=(const IndexedVolumeBounds&)=delete;
    [[nodiscard]] std::size_t IndexedParts()const noexcept{return index_?index_->Size():0;}
    [[nodiscard]] std::size_t Queries()const noexcept{return queries_;}
    [[nodiscard]] std::size_t BoxesTested()const noexcept{return boxesTested_;}
    [[nodiscard]] bool Clears(const PlantVolume& plant,float margin=.03f){
        Detail::Bounds bounds;
        if(!Detail::PlantBounds(plant,margin,bounds)||source_.size()>10000)return false;
        if(source_.empty())return true;
        if(invalid_)return false;
        if(!index_){
            std::vector<Aevrynd::ClosedAabb3D> prepared;prepared.reserve(source_.size());
            for(const auto& part:source_){Detail::Bounds wood;
                if(!Detail::WoodBounds(part,wood)){invalid_=true;return false;}
                prepared.push_back(Convert(wood));
            }
            auto candidate=std::make_unique<Aevrynd::StaticAabbIndex3D>(prepared);
            if(!candidate->Valid()){invalid_=true;return false;}index_=std::move(candidate);
        }
        const auto result=index_->Query(Convert(bounds));++queries_;boxesTested_+=result.boxesTested;
        return result.status==Aevrynd::StaticAabbQueryStatus3D::Disjoint;
    }
};
}
