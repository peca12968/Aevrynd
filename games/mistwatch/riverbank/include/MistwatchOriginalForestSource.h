#pragma once
#include "MistwatchMiddleLayer.h"
#include "MistwatchFallenLogProfile.h"

namespace Mistwatch {
// 僅重建原地形上的五族基礎來源；不是含 canopy、倒木重作者及完整林床的世界認證。
// 不接受外部向量冒充原來源，亦不透過仍依 ActiveEntryLayout 的 WorldSolids wrapper。
class OriginalForestSource {
public:
    static OriginalForestSource Create(const Aevrynd::HeightField3D& terrain) {
        ValidateFallenLogTerrain(terrain);
        OriginalForestSource source;
        source.firs_=CreateForestWoodMatureFirLayout(terrain,100,0x57464952u,OriginalEntryLayout);
        if(!source.firs_.reachedBudget||source.firs_.firs.size()!=100||source.firs_.roots.size()!=100||
            source.firs_.fingerprint!=FallenLogFirFingerprint)
            throw std::invalid_argument("ORIGINAL_SOURCE_FIR_IDENTITY");
        source.props_=ForestProps(terrain,source.firs_.roots,OriginalEntryLayout);
        ValidateFallenLogOriginalProps(source.props_);
        source.understory_=CreateUnderstory(terrain,source.firs_.roots,source.props_,OriginalEntryLayout);
        source.broadleaf_=CreateBroadleafTrees(terrain,source.firs_.roots,source.props_,source.understory_,OriginalEntryLayout);
        if(source.understory_.ferns.size()!=420||source.understory_.saplings.size()!=42||source.broadleaf_.size()!=12)
            throw std::invalid_argument("ORIGINAL_SOURCE_CORE_POPULATION");
        source.baseSolids_=OriginalFacilities();
        for(const auto& tree:source.firs_.firs)source.baseSolids_.push_back(tree.collider);
        for(const auto& prop:source.props_)source.baseSolids_.push_back(prop.collider);
        for(const auto& tree:source.understory_.saplings)source.baseSolids_.push_back(SaplingCollider(tree));
        for(const auto& tree:source.broadleaf_)source.baseSolids_.push_back(tree.trunk);
        if(source.baseSolids_.size()!=223)throw std::invalid_argument("ORIGINAL_SOURCE_BASE_SOLIDS");
        source.middle_=CreateMiddleLayer(terrain,source.firs_.roots,source.props_,source.understory_,
            source.broadleaf_,source.baseSolids_,24,OriginalFernRadii,OriginalEntryLayout);
        if(!source.middle_.reachedBudget||source.middle_.trees.size()!=24||
            MiddleLayerTransformFingerprint(source.middle_.trees)!=10337542786750772359ull)
            throw std::invalid_argument("ORIGINAL_SOURCE_MIDDLE_IDENTITY");
        return source;
    }
    const MatureFirLayout& Firs()const noexcept{return firs_;}
    const std::vector<ForestProp>& Props()const noexcept{return props_;}
    const ForestUnderstory& Understory()const noexcept{return understory_;}
    const std::vector<BroadleafTree>& Broadleaf()const noexcept{return broadleaf_;}
    const MiddleLayerResult& Middle()const noexcept{return middle_;}
    const std::vector<Aevrynd::BoxCollider3D>& BaseSolids()const noexcept{return baseSolids_;}
    std::vector<Aevrynd::BoxCollider3D> MappedSolids(bool repaired,FallenLogProfile profile)const {
        ValidateFallenLogRoute(FirLayoutSelector::ForestWood,profile);
        auto boxes=baseSolids_;
        if(boxes.size()!=223||props_.size()!=36||firs_.firs.size()!=100)
            throw std::invalid_argument("ORIGINAL_SOURCE_MAPPED_ROLE_ORDER");
        constexpr std::size_t PropBegin=133;
        for(std::size_t id=0;id<props_.size();++id)
            if(!FallenLogDetail::SameBox(boxes[PropBegin+id],props_[id].collider))
                throw std::invalid_argument("ORIGINAL_SOURCE_MAPPED_ORIGINAL_BOX");
        const auto mapped=ApplyFallenLogProfile(props_,FirLayoutSelector::ForestWood,profile);
        if(profile==FallenLogProfile::ScannedStraightV1)
            for(const auto& placement:FallenLogPlacements())boxes[PropBegin+placement.propId]=mapped[placement.propId].collider;
        if(repaired)boxes.insert(boxes.begin()+33,Aevrynd::BoxCollider3D{{0,-.2f,36},{2,.2f,4}});
        return boxes;
    }
    static constexpr std::array<float,4> OriginalFernRadii{.981686473f,.821101248f,.549620509f,.635080874f};
private:
    OriginalForestSource()=default;
    static std::vector<Aevrynd::BoxCollider3D> OriginalFacilities() {
        // 目前唯一會選配置的設施是工具棚。未來若移棚，拒絕沿用舊設施來源。
        if(!FallenLogDetail::Same(Session::repairSites[0],OriginalEntryLayout.repairSites[0]))
            throw std::invalid_argument("ORIGINAL_SOURCE_FACILITY_LAYOUT_DRIFT");
        auto boxes=WorkshopSolids();boxes.push_back(FuseCaseSolid());
        for(const auto& box:RangerHutSolids())boxes.push_back(box);
        for(const auto& box:RepairShelterSolids())boxes.push_back(box);
        boxes.push_back(GeneratorSolid());
        for(unsigned id=0;id<3;++id)for(const auto& box:RelaySolids(id))boxes.push_back(box);
        if(boxes.size()!=33)throw std::invalid_argument("ORIGINAL_SOURCE_FACILITY_POPULATION");
        return boxes;
    }
    MatureFirLayout firs_;
    std::vector<ForestProp> props_;
    ForestUnderstory understory_;
    std::vector<BroadleafTree> broadleaf_;
    MiddleLayerResult middle_;
    std::vector<Aevrynd::BoxCollider3D> baseSolids_;
};
}
