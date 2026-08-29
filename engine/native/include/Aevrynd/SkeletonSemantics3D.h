#pragma once
#include "SkinnedModel3D.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>

namespace Aevrynd {
// 引擎支援的最小人形語意集合；值的順序是序列化與測試契約的一部分。
enum class HumanoidJoint3D : std::uint8_t {
    Root,Hips,Spine,Head,
    LeftUpperLeg,LeftLowerLeg,LeftFoot,RightUpperLeg,RightLowerLeg,RightFoot,
    LeftUpperArm,LeftLowerArm,LeftHand,RightUpperArm,RightLowerArm,RightHand,
    Count
};

// Resolve 的失敗原因；任何非 None 結果都保證 output 未被修改。
enum class SkeletonSemanticError3D : std::uint8_t {
    None,InvalidSkeleton,UnknownCompatibility,CompatibilityMismatch,NameCountMismatch,MissingJoint,DuplicateJoint,InvalidTopology
};

// 已解析的人形語意索引；compatibility 綁定名稱、拓樸與 inverse-bind profile。
struct HumanoidSkeletonMap3D {
    // InvalidJoint 只用於尚未成功解析的暫存值，不是可接受的 production mapping。
    static constexpr std::uint16_t InvalidJoint=0xffffu;
    // compatibility 必須等於呼叫端提供的 authoritative expected profile。
    SkeletonCompatibilityId3D compatibility{};
    // joints 依 HumanoidJoint3D 的固定順序保存唯一 skeleton joint index。
    std::array<std::uint16_t,std::size_t(HumanoidJoint3D::Count)> joints{};
    // Joint 回傳指定語意的 skeleton index；呼叫前必須先以 Valid 驗證整張 mapping。
    [[nodiscard]] std::size_t Joint(HumanoidJoint3D semantic)const noexcept{return joints[std::size_t(semantic)];}
    // Valid 嚴格核對 expected compatibility、索引唯一性、範圍及人形拓樸，不做名稱 fallback。
    [[nodiscard]] bool Valid(const Skeleton3D& skeleton,SkeletonCompatibilityId3D expected)const noexcept;
};

// jointNames、model.skeleton 與 inverse binds 必須共同雜湊成 expectedCompatibility；expected 必須由
// authoritative asset/profile 提供，不可取自待驗資料的自我宣告。名稱只接受明列 alias，namespace 與大小寫
// 會正規化，但不以索引或拓樸猜測遺漏語意。任何 compatibility、名稱或拓樸錯誤都 fail-closed，output 原子不變。
bool ResolveHumanoidSkeletonMap3D(std::span<const std::string> jointNames,const SkinnedModel3D& model,
    SkeletonCompatibilityId3D expectedCompatibility,HumanoidSkeletonMap3D& output,
    SkeletonSemanticError3D* error=nullptr)noexcept;
}
