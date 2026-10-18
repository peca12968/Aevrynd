#pragma once
#include "MistwatchObjectiveGuide.h"
#include <cmath>
#include <limits>

namespace Mistwatch {
enum class PickupKind { None, Supply, Timber };

// 僅保存一次提前按下的取物意圖；呼叫者須把本物件與世界候選一起提交。
class PickupIntent {
public:
    static constexpr double Duration=.12;
    static constexpr double MaximumApproachDistance=2.85;
    static constexpr unsigned NoIndex=3;

    void Clear() noexcept {kind_=PickupKind::None;index_=NoIndex;stage_=Stage::Supplies;remaining_=0;}
    bool Armed()const noexcept{return kind_!=PickupKind::None;}
    PickupKind Kind()const noexcept{return kind_;}
    unsigned Index()const noexcept{return index_;}
    double Remaining()const noexcept{return remaining_;}

    bool Arm(const Session& session,const ObjectiveGuide& guide) {
        // 新的按鍵意圖取代舊意圖；拒絕不能讓先前的目標稍後突然生效。
        Clear();
        PickupKind kind;unsigned index;
        if(!Identify(session,guide,kind,index)||guide.bearing!=ObjectiveBearing::Ahead||
           guide.distance<=2.5||guide.distance>MaximumApproachDistance)return false;
        kind_=kind;index_=index;stage_=session.CurrentStage();remaining_=Duration;return true;
    }

    bool Advance(Session& session,const ObjectiveGuide& guide,double dt) {
        if(!Armed())return false;
        PickupKind kind;unsigned index;
        if(!std::isfinite(dt)||dt<0||dt>.1||
           !Identify(session,guide,kind,index)||kind!=kind_||index!=index_||session.CurrentStage()!=stage_||
           (guide.bearing!=ObjectiveBearing::Ahead&&guide.bearing!=ObjectiveBearing::AtTarget)){
            Clear();return false;
        }
        // 零時間不是新的成功世界步；過期當步必須先清除，不能再取物。
        if(dt==0)return false;
        if(dt>=remaining_){Clear();return false;}
        remaining_-=dt;
        const bool changed=kind_==PickupKind::Supply?session.Collect(index_):session.CollectRepairMaterial(index_,false);
        if(changed)Clear();
        return changed;
    }
private:
    static bool Identify(const Session& session,const ObjectiveGuide& guide,PickupKind& kind,unsigned& index) {
        if(!session.Valid()||!session.Alive()||!guide.valid||!guide.hasTarget||guide.relayTrackingActive||
           !Session::Finite(guide.target)||!std::isfinite(guide.distance)||guide.distance<0||
           !std::isfinite(guide.relativeRadians)||std::abs(guide.relativeRadians)>3.14159265358979323846)return false;
        const auto stage=session.CurrentStage();
        if(stage!=Stage::Supplies&&stage!=Stage::RepairMaterials)return false;
        kind=stage==Stage::Supplies?PickupKind::Supply:PickupKind::Timber;
        const auto& points=stage==Stage::Supplies?Session::supplies:Session::repairSites;
        const unsigned completed=stage==Stage::Supplies?session.collected:session.repairMaterials;
        index=NoIndex;double closest=(std::numeric_limits<double>::infinity)();
        for(unsigned i=0;i<points.size();++i)if(!(completed&(1u<<i))){
            const double distance=std::hypot(double(points[i].x)-session.feet.x,double(points[i].z)-session.feet.z);
            if(distance<closest){closest=distance;index=i;}
        }
        // 保留正式最近目標的等距規則；不能跳過工具，暗中改指另一組木料。
        if(index>=points.size()||(kind==PickupKind::Timber&&index==0)||guide.index!=index)return false;
        const auto target=points[index];
        if(guide.target.x!=target.x||guide.target.y!=target.y||guide.target.z!=target.z||
           guide.distance!=closest||std::abs(double(session.feet.y)-target.y)>2)return false;
        constexpr double quarter=.7853981633974483;
        if(closest<=.001)return guide.bearing==ObjectiveBearing::AtTarget&&guide.relativeRadians==0;
        return guide.bearing==ObjectiveBearing::Ahead&&std::abs(guide.relativeRadians)<=quarter;
    }
    PickupKind kind_{PickupKind::None};
    unsigned index_{NoIndex};
    Stage stage_{Stage::Supplies};
    double remaining_{};
};
}
