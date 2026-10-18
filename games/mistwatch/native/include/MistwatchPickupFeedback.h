#pragma once
#include <array>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>

namespace Mistwatch::PickupFeedback {
// 此為文案的 UTF-8 位元組上限，不冒充實際字型寬度；正式 HUD 仍須量測。
inline constexpr std::size_t MaximumTextBytes=sizeof("取得：補給箱1、補給箱2、補給箱3、低棚工具、東南木料、西岸木料；補給3/3、物資3/3")-1;

// 僅敘述同一次物資變化中真正增加的項目，不宣稱橋梁、路線或下一任務已可完成。
// 非法、撤銷或沒有新拾取的輸入都不產生成功訊息；不修改任務或呼叫任何遊戲服務。
inline std::optional<std::string> Build(unsigned suppliesBefore,unsigned suppliesAfter,
                                      unsigned materialsBefore,unsigned materialsAfter) {
    if(suppliesBefore>7||suppliesAfter>7||materialsBefore>7||materialsAfter>7||
       (suppliesBefore&suppliesAfter)!=suppliesBefore||(materialsBefore&materialsAfter)!=materialsBefore)
        return std::nullopt;
    const unsigned suppliesAdded=suppliesAfter&~suppliesBefore,materialsAdded=materialsAfter&~materialsBefore;
    if(!suppliesAdded&&!materialsAdded)return std::nullopt;
    constexpr std::array<std::string_view,3> supplies{"補給箱1","補給箱2","補給箱3"};
    constexpr std::array<std::string_view,3> materials{"低棚工具","東南木料","西岸木料"};
    constexpr std::array<unsigned,8> counts{0,1,1,2,1,2,2,3};
    std::string text="取得：";bool separator=false;
    const auto append=[&](unsigned bits,const auto& names){
        for(unsigned i=0;i<3;++i)if(bits&(1u<<i)){
            if(separator)text+="、";text+=names[i];separator=true;
        }
    };
    append(suppliesAdded,supplies);append(materialsAdded,materials);
    text+="；補給";text+=std::to_string(counts[suppliesAfter]);text+="/3、物資";
    text+=std::to_string(counts[materialsAfter]);text+="/3";
    return text;
}
}
