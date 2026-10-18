#include "MistwatchPickupFeedback.h"
#include <algorithm>
#include <array>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {
unsigned checks{},cases{},exhaustive{};std::size_t maximumBytes{};
void Need(bool ok,const char* why){++checks;if(!ok)throw std::runtime_error(why);}
void Exact(unsigned sb,unsigned sa,unsigned mb,unsigned ma,std::string_view expected){
    const auto result=Mistwatch::PickupFeedback::Build(sb,sa,mb,ma);
    Need(result&&*result==expected,"NAMED_PICKUP_OR_AFTER_PROGRESS_MISMATCH");++cases;
}
}
int main(){try{
    // 明確的單一物品與非零既有進度案例；不以函式自己的名稱或計數表當測試答案。
    Exact(0,1,0,0,"取得：補給箱1；補給1/3、物資0/3");
    Exact(0,2,0,0,"取得：補給箱2；補給1/3、物資0/3");
    Exact(0,4,0,0,"取得：補給箱3；補給1/3、物資0/3");
    Exact(0,0,0,1,"取得：低棚工具；補給0/3、物資1/3");
    Exact(0,0,0,2,"取得：東南木料；補給0/3、物資1/3");
    Exact(0,0,0,4,"取得：西岸木料；補給0/3、物資1/3");
    Exact(5,7,4,4,"取得：補給箱2；補給3/3、物資1/3");
    Exact(7,7,2,3,"取得：低棚工具；補給3/3、物資2/3");
    Exact(1,7,0,5,"取得：補給箱2、補給箱3、低棚工具、西岸木料；補給3/3、物資2/3");
    Exact(0,7,0,7,"取得：補給箱1、補給箱2、補給箱3、低棚工具、東南木料、西岸木料；補給3/3、物資3/3");
    // 以八種新增集合的獨立完整文案表作 oracle；四個 0..7 mask 總共 4096 組。
    constexpr std::array<std::string_view,8> supplyNames{"","補給箱1","補給箱2","補給箱1、補給箱2",
        "補給箱3","補給箱1、補給箱3","補給箱2、補給箱3","補給箱1、補給箱2、補給箱3"};
    constexpr std::array<std::string_view,8> materialNames{"","低棚工具","東南木料","低棚工具、東南木料",
        "西岸木料","低棚工具、西岸木料","東南木料、西岸木料","低棚工具、東南木料、西岸木料"};
    constexpr std::array<std::string_view,8> progress{"0/3","1/3","1/3","2/3","1/3","2/3","2/3","3/3"};
    for(unsigned sb=0;sb<8;++sb)for(unsigned sa=0;sa<8;++sa)
    for(unsigned mb=0;mb<8;++mb)for(unsigned ma=0;ma<8;++ma){
        ++exhaustive;const auto result=Mistwatch::PickupFeedback::Build(sb,sa,mb,ma);
        bool removed=false;for(unsigned bit=1;bit<=4;bit<<=1)
            if(((sb&bit)&&!(sa&bit))||((mb&bit)&&!(ma&bit)))removed=true;
        const bool expected=!removed&&(sb!=sa||mb!=ma);
        Need(bool(result)==expected,"EXHAUSTIVE_INVALID_REMOVED_OR_NOOP_SUCCESS");
        if(!result)continue;
        std::string text="取得：";const auto sn=supplyNames[sa^sb],mn=materialNames[ma^mb];
        text+=sn;if(!sn.empty()&&!mn.empty())text+="、";text+=mn;
        text+="；補給";text+=progress[sa];text+="、物資";text+=progress[ma];
        Need(*result==text,"EXHAUSTIVE_NAMES_OR_ACTUAL_AFTER_COUNTS");
        Need(result->size()<=Mistwatch::PickupFeedback::MaximumTextBytes,"HUD_UTF8_TEXT_BOUND_EXCEEDED");
        Need(result->find("修橋")==std::string::npos&&result->find("完成")==std::string::npos&&
             result->find("可通行")==std::string::npos,"PICKUP_FALSE_READINESS_CLAIM");
        maximumBytes=(std::max)(maximumBytes,result->size());
    }
    Need(exhaustive==4096,"EXHAUSTIVE_COVERAGE_MISSING");
    Need(maximumBytes==Mistwatch::PickupFeedback::MaximumTextBytes,"MAXIMUM_TEXT_NOT_REACHED");
    for(const unsigned invalid:{8u,9u,16u,1u<<31,(std::numeric_limits<unsigned>::max)()})for(unsigned slot=0;slot<4;++slot){
        std::array<unsigned,4> masks{0,1,0,1};masks[slot]=invalid;
        Need(!Mistwatch::PickupFeedback::Build(masks[0],masks[1],masks[2],masks[3]),"OUT_OF_RANGE_MASK_SUCCESS");++cases;
    }
    Need(!Mistwatch::PickupFeedback::Build(7,3,0,1),"REMOVED_SUPPLY_PLUS_ADDED_MATERIAL_SUCCESS");
    Need(!Mistwatch::PickupFeedback::Build(0,1,7,3),"REMOVED_MATERIAL_PLUS_ADDED_SUPPLY_SUCCESS");
    Need(!Mistwatch::PickupFeedback::Build(7,7,7,7),"NO_NEW_PICKUP_SUCCESS");
    std::cout<<"PICKUP_FEEDBACK_CPU_PASS cases="<<cases<<" exhaustive="<<exhaustive<<" checks="<<checks
             <<" max_utf8_bytes="<<maximumBytes<<" engine=false GPU=false\n";return 0;
}catch(const std::exception& e){std::cerr<<"PICKUP_FEEDBACK_FIRST_DIVERGENCE reason="<<e.what()<<'\n';return 1;}}
