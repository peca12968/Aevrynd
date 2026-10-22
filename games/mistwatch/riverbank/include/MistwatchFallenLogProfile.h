#pragma once
#include "MistwatchForestProps.h"
#include "MistwatchFirLayout.h"
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <stdexcept>
#include <string_view>
#include <utility>
#include <vector>

namespace Mistwatch {
// 明確內容選擇，不依環境、外部證據檔或執行期接地搜尋改變世界。
enum class FallenLogProfile : std::uint8_t { LegacyCylinder, ScannedStraightV1 };

struct FallenLogPlacement {
    unsigned propId{};
    Aevrynd::Vec3 authoredAnchor{};
    Aevrynd::Transform3D pose{};
    Aevrynd::BoxCollider3D collider{};
};

inline constexpr char FallenLogMeshName[]=
    "dead-tree-trunk-lod2-node0-mesh0-primitive0-materialnone-material-none.aemesh";
inline constexpr char FallenLogMeshSha256[]=
    "B7F6A05BF928787082DEA92A0051557496E45894A9C2B1A0AF1E51110ABD0D46";
inline constexpr std::size_t FallenLogMeshBytes=434012;
inline constexpr std::size_t FallenLogVertexCount=8956,FallenLogIndexCount=45807;
inline constexpr std::uint64_t FallenLogFirFingerprint=8236695966185867318ull;
inline constexpr std::uint64_t FallenLogTerrainFingerprint=11561441978197049099ull;
inline constexpr std::uint64_t FallenLogOriginalPropFingerprint=13639393489331150506ull;
inline constexpr std::array<unsigned,4> FallenLogPropIds{{0,8,34,35}};

namespace FallenLogDetail {
static_assert(sizeof(float)==sizeof(std::uint32_t)&&std::numeric_limits<float>::is_iec559);
using PropBits=std::array<std::uint32_t,16>;
inline std::uint32_t Bits(float value) noexcept {return std::bit_cast<std::uint32_t>(value);}
inline bool Same(Aevrynd::Vec3 a,Aevrynd::Vec3 b) noexcept {
    return Bits(a.x)==Bits(b.x)&&Bits(a.y)==Bits(b.y)&&Bits(a.z)==Bits(b.z);
}
inline bool SameBox(const Aevrynd::BoxCollider3D& a,const Aevrynd::BoxCollider3D& b) noexcept {
    return Same(a.center,b.center)&&Same(a.extents,b.extents);
}
inline PropBits Words(const ForestProp& p) noexcept {
    return {{p.mesh,Bits(p.transform.position.x),Bits(p.transform.position.y),Bits(p.transform.position.z),
        Bits(p.transform.rotation.x),Bits(p.transform.rotation.y),Bits(p.transform.rotation.z),
        Bits(p.transform.scale.x),Bits(p.transform.scale.y),Bits(p.transform.scale.z),
        Bits(p.collider.center.x),Bits(p.collider.center.y),Bits(p.collider.center.z),
        Bits(p.collider.extents.x),Bits(p.collider.extents.y),Bits(p.collider.extents.z)}};
}
// 保留整個原地物群逐欄位身分；不把可碰撞的 fingerprint 當無碰撞認證。
inline constexpr std::array<PropBits,36> OriginalPropWords{{
    {{0x00000003u,0xc0f00000u,0x3e4f6d47u,0x40e00000u,0x3fc90fdau,0x00000000u,0x00000000u,0x3f015038u,0x4080fb8eu,0x3f015038u,0xc0f00000u,0x3e4f6d47u,0x40e00000u,0x3e815038u,0x3e81503du,0x4000fb8eu}},
    {{0x00000001u,0xc0c00000u,0xbdf5c28fu,0x40000000u,0x00000000u,0x4084bc01u,0x00000000u,0x3f682871u,0x3f359b5bu,0x3f58ff6bu,0xc0c00000u,0x3d9969c9u,0x40000000u,0x3f06c278u,0x3e47962cu,0x3efc31d6u}},
    {{0x00000001u,0xc0d87448u,0xbdf5c28fu,0x42a4b9b0u,0x00000000u,0x4057fd12u,0x00000000u,0x4007ea6eu,0x401534eeu,0x40041b09u,0xc0d87448u,0x3f05423fu,0x42a4b9b0u,0x3f74971cu,0x3f23fa91u,0x3f9653cau}},
    {{0x00000002u,0x40f19590u,0xbdf5c28fu,0x428dc11au,0x00000000u,0x3fbb931cu,0x00000000u,0x3f98abd2u,0x3f8150a1u,0x3fa46012u,0x40f19590u,0x3e215af0u,0x428dc11au,0x3f2e9b2fu,0x3e8e1e1cu,0x3ef549cfu}},
    {{0x00000002u,0xc1081428u,0xbdf5c28fu,0x4231bf5eu,0x00000000u,0x403ddcd7u,0x00000000u,0x3f716fb9u,0x3f3dd6ceu,0x3f89f245u,0xc1081428u,0x3dab81f9u,0x4231bf5eu,0x3ed503abu,0x3e50a244u,0x3f166c84u}},
    {{0x00000000u,0xc10c6ef4u,0xbdf5c28fu,0x3a888000u,0x00000000u,0x40349085u,0x00000000u,0x400554c6u,0x3fc69506u,0x3fd5a3bfu,0xc10c6ef4u,0x3e9ccd6fu,0x3a888000u,0x3f72e534u,0x3eda3e13u,0x3f82843bu}},
    {{0x00000000u,0xc0f5ba30u,0xbdf5c28fu,0x429c3a1fu,0x00000000u,0x40879a05u,0x00000000u,0x40043b98u,0x3fcf3a18u,0x4008acacu,0xc0f5ba30u,0x3ea64d9bu,0x429c3a1fu,0x3fa3819bu,0x3ee3be3fu,0x3f901885u}},
    {{0x00000000u,0xc0df77b0u,0xbdf5c28fu,0x428d8eedu,0x00000000u,0x40b1e093u,0x00000000u,0x4011d3deu,0x401cdbf3u,0x40135cfdu,0xc0df77b0u,0x3f0dab37u,0x428d8eedu,0x3fad71fcu,0x3f2c6389u,0x3fb16ae5u}},
    {{0x00000003u,0x41131dd0u,0x3e5b02afu,0x3df65880u,0x3fc90fdau,0x3e0c812du,0x00000000u,0x3f071aecu,0x406462d2u,0x3f071aecu,0x41131dd0u,0x3e5b02afu,0x3df65880u,0x3f016545u,0x3e871af1u,0x3fe6dc04u}},
    {{0x00000001u,0x40cfe4c0u,0xbdf5c28fu,0xc052e78au,0x00000000u,0x3fe34243u,0x00000000u,0x3fb03915u,0x3f970925u,0x3fc63a11u,0x40cfe4c0u,0x3e5118feu,0xc052e78au,0x3f5af16bu,0x3ea5fd23u,0x3f2005a0u}},
    {{0x00000000u,0x40eeb0f0u,0xbdf5c28fu,0x4261265cu,0x00000000u,0x3f6dc0c0u,0x00000000u,0x3f9e1266u,0x3f5ea5d0u,0x3fb3127eu,0x40eeb0f0u,0x3df39f0du,0x4261265cu,0x3f5119bau,0x3e74b0ceu,0x3f4315f1u}},
    {{0x00000002u,0xc0fa80b8u,0xbdf5c28fu,0x4296bd3fu,0x00000000u,0x40763f64u,0x00000000u,0x3f8a6289u,0x3f6e91c0u,0x3f6b7042u,0xc0fa80b8u,0x3e0b4efeu,0x4296bd3fu,0x3f15739eu,0x3e831823u,0x3f17e239u}},
    {{0x00000000u,0xc113a810u,0xbdf5c28fu,0x405ad5b0u,0x00000000u,0x40a949a5u,0x00000000u,0x3f820859u,0x3f7a6054u,0x3f802910u,0xc113a810u,0x3e1848d2u,0x405ad5b0u,0x3f1ca3e0u,0x3e89950du,0x3f11967fu}},
    {{0x00000001u,0xc0f970c8u,0xbdf5c28fu,0x4285c978u,0x00000000u,0x408f66f2u,0x00000000u,0x3f901a2eu,0x3f98823fu,0x3f795850u,0xc0f970c8u,0x3e5455dcu,0x4285c978u,0x3f1042abu,0x3ea79b92u,0x3efbc8dau}},
    {{0x00000000u,0x40d30c70u,0xbdf5c28fu,0x418b8c47u,0x00000000u,0x400932ffu,0x00000000u,0x3fbde0e4u,0x3fb18946u,0x3fb24d5bu,0x40d30c70u,0x3e85ac47u,0x418b8c47u,0x3f5d4236u,0x3ec31cebu,0x3f4f6168u}},
    {{0x00000000u,0x40d34b30u,0xbdf5c28fu,0x426ae8a4u,0x00000000u,0x3f7829f3u,0x00000000u,0x3fb9c26au,0x3fa9cd1fu,0x3f969221u,0x40d34b30u,0x3e7a582au,0x426ae8a4u,0x3f45161du,0x3eba9cb9u,0x3f3f7b04u}},
    {{0x00000002u,0xc0ea801cu,0xbdf5c28fu,0xc0e1b5a4u,0x00000000u,0x40c4bebeu,0x00000000u,0x40165afeu,0x4008bae3u,0x400b4d33u,0xc0ea801cu,0x3eef17f6u,0xc0e1b5a4u,0x3f743f5eu,0x3f16444du,0x3f981186u}},
    {{0x00000002u,0x4102ca6cu,0xbdf5c28fu,0x40d074f6u,0x00000000u,0x40c49a38u,0x00000000u,0x3fbe3ba1u,0x3fa1b046u,0x3fac0a28u,0x4102ca6cu,0x3e68833eu,0x40d074f6u,0x3f1aa088u,0x3eb1b243u,0x3f3cb582u}},
    {{0x00000000u,0xc0c35bf0u,0xbdf5c28fu,0x4110b29cu,0x00000000u,0x404bd4f9u,0x00000000u,0x3f879483u,0x3f8608c5u,0x3f8aeaaau,0xc0c35bf0u,0x3e2bba70u,0x4110b29cu,0x3ec8031eu,0x3e934ddcu,0x3f0edb7au}},
    {{0x00000002u,0x40b70560u,0xbdf5c28fu,0xbf1a6db0u,0x00000000u,0x3f1dfdadu,0x00000000u,0x3f820635u,0x3f6d9a14u,0x3f904858u,0x40b70560u,0x3e0a3eceu,0xbf1a6db0u,0x3f1d1510u,0x3e82900bu,0x3f29e217u}},
    {{0x00000000u,0x410efeecu,0xbdf5c28fu,0x41ddadf4u,0x00000000u,0x4064b32cu,0x00000000u,0x3fcf6eb0u,0x3fb57c75u,0x3ff12604u,0x410efeecu,0x3e8a0393u,0x41ddadf4u,0x3f67a944u,0x3ec77437u,0x3f8ba0efu}},
    {{0x00000001u,0xc0bad280u,0xbdf5c28fu,0xc02e94e2u,0x00000000u,0x3fc26981u,0x00000000u,0x3f894395u,0x3f833108u,0x3f93f7e3u,0xc0bad280u,0x3e257adeu,0xc02e94e2u,0x3f18b706u,0x3e902e13u,0x3ecd9d57u}},
    {{0x00000001u,0xc10e4ae2u,0xbdf5c28fu,0x424b3582u,0x00000000u,0x408c02d0u,0x00000000u,0x3f729e08u,0x3f72afdau,0x3f508da0u,0xc10e4ae2u,0x3e0fd574u,0x424b3582u,0x3efc7ef7u,0x3e855b5eu,0x3ee3db34u}},
    {{0x00000002u,0x409e2730u,0xbdf5c28fu,0x3fbb4020u,0x00000000u,0x40534db0u,0x00000000u,0x3f670052u,0x3f75c3b0u,0x3f3970fbu,0x409e2730u,0x3e13374au,0x3fbb4020u,0x3ebbccceu,0x3e870c49u,0x3ed09d4du}},
    {{0x00000000u,0xc0bbdf04u,0xbdf5c28fu,0x4286c1bcu,0x00000000u,0x3eb61f7fu,0x00000000u,0x3f81980du,0x3f81954eu,0x3f9b228au,0xc0bbdf04u,0x3e21f1e2u,0x4286c1bcu,0x3f0a5396u,0x3e8e6995u,0x3f30bdecu}},
    {{0x00000000u,0xc11a0186u,0xbdf5c28fu,0x42a17da8u,0x00000000u,0x408c9afdu,0x00000000u,0x3f87dbc4u,0x3f901237u,0x3fa2d09du,0xc11a0186u,0x3e41ca1au,0x42a17da8u,0x3f3825aeu,0x3e9e55b1u,0x3f0c8400u}},
    {{0x00000000u,0xc0af1e2cu,0xbdf5c28fu,0x40ba8b70u,0x00000000u,0x4041fd6au,0x00000000u,0x3f7773c7u,0x3f4a704du,0x3f831ca0u,0xc0af1e2cu,0x3dc733a3u,0x40ba8b70u,0x3ec795c5u,0x3e5e7b19u,0x3f0bc7bau}},
    {{0x00000000u,0x40fccae0u,0xbdf5c28fu,0x424f53b2u,0x00000000u,0x40328bc5u,0x00000000u,0x3fe9f90du,0x3fe53845u,0x400c7005u,0x40fccae0u,0x3ebe7931u,0x424f53b2u,0x3f7934d0u,0x3efbe9d5u,0x3f9fcf83u}},
    {{0x00000002u,0x4114ff68u,0xbdf5c28fu,0x42925d71u,0x00000000u,0x40961659u,0x00000000u,0x3fbcf902u,0x3f926833u,0x3fa1cba3u,0x4114ff68u,0x3e46ec78u,0x42925d71u,0x3f24a95cu,0x3ea0e6e0u,0x3f06b002u}},
    {{0x00000002u,0x41035fc0u,0xbdf5c28fu,0x4243eba3u,0x00000000u,0x4095f45au,0x00000000u,0x3fd8fab9u,0x3fe06f55u,0x3fc1c931u,0x41035fc0u,0x3eb936fbu,0x4243eba3u,0x3f45ae13u,0x3ef6a79fu,0x3f1b9d81u}},
    {{0x00000001u,0xc105a690u,0xbdf5c28fu,0x42553e84u,0x00000000u,0x409c4469u,0x00000000u,0x3fbd4d7cu,0x3fbcd07bu,0x3faaa953u,0xc105a690u,0x3e921154u,0x42553e84u,0x3f3e85b9u,0x3ecf81f8u,0x3f1e7babu}},
    {{0x00000000u,0x411456fcu,0xbdf5c28fu,0x414a7870u,0x00000000u,0x404f5077u,0x00000000u,0x3f772085u,0x3f5ce046u,0x3f86c2c2u,0x411456fcu,0x3defba2bu,0x414a7870u,0x3ec4f5f0u,0x3e72be5du,0x3f0e7b4bu}},
    {{0x00000001u,0x40eafc80u,0xbdf5c28fu,0x42916e23u,0x00000000u,0x40018d85u,0x00000000u,0x3f98c350u,0x3f8f1b39u,0x3f768a9cu,0x40eafc80u,0x3e3fab36u,0x42916e23u,0x3f1d409bu,0x3e9d463fu,0x3f154beau}},
    {{0x00000000u,0xc11263d0u,0xbdf5c28fu,0x413d4524u,0x00000000u,0x3fd142cfu,0x00000000u,0x3faa0c48u,0x3fb0cd03u,0x3fd1b90au,0xc11263d0u,0x3e84dd61u,0x413d4524u,0x3f58d89eu,0x3ec24e05u,0x3f032f42u}},
    {{0x00000003u,0xc10ebf78u,0x3e4c83a5u,0x42a8a094u,0x3fc90fdau,0x404b1102u,0x00000000u,0x3effb6cfu,0x4067ab1cu,0x3effb6cfu,0xc10ebf78u,0x3e4c83a5u,0x42a8a094u,0x3e9cd03bu,0x3e7fb6d8u,0x3fe88e46u}},
    {{0x00000003u,0xc0dfa578u,0x3e4937c5u,0x42811758u,0x3fc90fdau,0x409a1202u,0x00000000u,0x3efc6aedu,0x40913c7bu,0x3efc6aedu,0xc0dfa578u,0x3e4937c5u,0x42811758u,0x40121689u,0x3e7c6af8u,0x3ef436b6u}}
}};
inline void Mix(std::uint64_t& hash,std::uint32_t value) noexcept {
    for(unsigned byte=0;byte<4;++byte){hash^=(value>>(byte*8))&255u;hash*=1099511628211ull;}
}
inline bool FiniteProp(const ForestProp& p) noexcept {
    return p.mesh<4&&Session::Finite(p.transform.position)&&Session::Finite(p.transform.rotation)&&
        Session::Finite(p.transform.scale)&&p.transform.scale.x>0&&p.transform.scale.y>0&&p.transform.scale.z>0&&
        Session::Finite(p.collider.center)&&Session::Finite(p.collider.extents)&&
        p.collider.extents.x>0&&p.collider.extents.y>0&&p.collider.extents.z>0;
}
inline bool ContainsEveryVertex(const Aevrynd::Mesh3D& mesh,const Aevrynd::Transform3D& pose,
    const Aevrynd::BoxCollider3D& box) {
    if(!Session::Finite(pose.position)||!Session::Finite(pose.rotation)||!Session::Finite(pose.scale)||
        pose.scale.x<=0||Bits(pose.scale.x)!=Bits(pose.scale.y)||Bits(pose.scale.x)!=Bits(pose.scale.z)||
        !Session::Finite(box.center)||!Session::Finite(box.extents)||
        box.extents.x<=0||box.extents.y<=0||box.extents.z<=0)return false;
    const auto matrix=pose.Matrix();
    for(const auto& vertex:mesh.vertices){
        const auto p=Aevrynd::TransformPoint(matrix,{vertex.x,vertex.y,vertex.z});
        if(!Session::Finite(p)||double(p.x)<double(box.center.x)-double(box.extents.x)||
            double(p.x)>double(box.center.x)+double(box.extents.x)||
            double(p.y)<double(box.center.y)-double(box.extents.y)||
            double(p.y)>double(box.center.y)+double(box.extents.y)||
            double(p.z)<double(box.center.z)-double(box.extents.z)||
            double(p.z)>double(box.center.z)+double(box.extents.z))return false;
    }
    return true;
}
}

// 直接取自已完整 CPU 驗證的實際 f32 pose/全網格保守 box。
// 接地證明仍由離線完整測試處理；常駐遊戲不依證據路徑或 SeatOne 認證自己。
inline constexpr std::array<FallenLogPlacement,4> ScannedStraightFallenLogPlacements{{
    {0u,{std::bit_cast<float>(0xc106b75au),std::bit_cast<float>(0x3e4f6d47u),std::bit_cast<float>(0xc132085cu)},
        {{std::bit_cast<float>(0xc1060985u),std::bit_cast<float>(0x3d1525b4u),std::bit_cast<float>(0xc131bcbau)},{std::bit_cast<float>(0x00000000u),std::bit_cast<float>(0xbfc90fdbu),std::bit_cast<float>(0x00000000u)},{std::bit_cast<float>(0x3fa9233du),std::bit_cast<float>(0x3fa9233du),std::bit_cast<float>(0x3fa9233du)}},
        {{std::bit_cast<float>(0xc106b75au),std::bit_cast<float>(0x3e43f30cu),std::bit_cast<float>(0xc132085cu)},{std::bit_cast<float>(0x3e3b8d40u),std::bit_cast<float>(0x3e43f30cu),std::bit_cast<float>(0x4000fb90u)}}},
    {8u,{std::bit_cast<float>(0x41131dd0u),std::bit_cast<float>(0x3e5b02afu),std::bit_cast<float>(0xbdb5e268u)},
        {{std::bit_cast<float>(0x4113bf6du),std::bit_cast<float>(0x3d040ba5u),std::bit_cast<float>(0xbd9f3eb2u)},{std::bit_cast<float>(0x00000000u),std::bit_cast<float>(0xbfb77fb5u),std::bit_cast<float>(0x00000000u)},{std::bit_cast<float>(0x3f95be63u),std::bit_cast<float>(0x3f95be63u),std::bit_cast<float>(0x3f95be63u)}},
        {{std::bit_cast<float>(0x4113651au),std::bit_cast<float>(0x3e2d7b2bu),std::bit_cast<float>(0xbdb45cf0u)},{std::bit_cast<float>(0x3eb8b240u),std::bit_cast<float>(0x3e2d7b2bu),std::bit_cast<float>(0x3fe29000u)}}},
    {34u,{std::bit_cast<float>(0xc0fd7048u),std::bit_cast<float>(0x3e4c83a5u),std::bit_cast<float>(0x42ae2f88u)},
        {{std::bit_cast<float>(0xc0feac9bu),std::bit_cast<float>(0x3d05f17au),std::bit_cast<float>(0x42ae27a8u)},{std::bit_cast<float>(0x00000000u),std::bit_cast<float>(0x3fcd1229u),std::bit_cast<float>(0x00000000u)},{std::bit_cast<float>(0x3f97e555u),std::bit_cast<float>(0x3f97e555u),std::bit_cast<float>(0x3f97e555u)}},
        {{std::bit_cast<float>(0xc0fd692cu),std::bit_cast<float>(0x3e2ff972u),std::bit_cast<float>(0x42ae2fb4u)},{std::bit_cast<float>(0x3e5290e0u),std::bit_cast<float>(0x3e2ff972u),std::bit_cast<float>(0x3fe790c0u)}}},
    {35u,{std::bit_cast<float>(0xc0dfb4e9u),std::bit_cast<float>(0x3e4937c5u),std::bit_cast<float>(0x4281c4c0u)},
        {{std::bit_cast<float>(0xc0e0365cu),std::bit_cast<float>(0x3d27f121u),std::bit_cast<float>(0x4281de2du)},{std::bit_cast<float>(0x00000000u),std::bit_cast<float>(0xc042839fu),std::bit_cast<float>(0x00000000u)},{std::bit_cast<float>(0x3fbe738cu),std::bit_cast<float>(0x3fbe738cu),std::bit_cast<float>(0x3fbe738cu)}},
        {{std::bit_cast<float>(0xc0dfb60fu),std::bit_cast<float>(0x3e5ca44bu),std::bit_cast<float>(0x4281cc31u)},{std::bit_cast<float>(0x40109812u),std::bit_cast<float>(0x3e5ca44bu),std::bit_cast<float>(0x3ec93400u)}}}
}};
inline constexpr const std::array<FallenLogPlacement,4>& FallenLogPlacements() noexcept {
    return ScannedStraightFallenLogPlacements;
}
inline const FallenLogPlacement& FallenLogPlacementForId(unsigned propId) {
    for(const auto& p:FallenLogPlacements())if(p.propId==propId)return p;
    throw std::invalid_argument("FALLEN_LOG_ID_NOT_ALLOWED");
}
inline void ValidateFallenLogRoute(FirLayoutSelector fir,FallenLogProfile profile) {
    if(fir!=FirLayoutSelector::Legacy&&fir!=FirLayoutSelector::ForestWood)
        throw std::invalid_argument("FALLEN_INVALID_FIR_SELECTOR");
    if(profile!=FallenLogProfile::LegacyCylinder&&profile!=FallenLogProfile::ScannedStraightV1)
        throw std::invalid_argument("FALLEN_INVALID_CONTENT_SELECTOR");
    if(profile==FallenLogProfile::ScannedStraightV1&&fir!=FirLayoutSelector::ForestWood)
        throw std::invalid_argument("FALLEN_SCAN_REQUIRES_FOREST_WOOD");
}
inline std::string_view FallenLogProfileId(FallenLogProfile profile) {
    switch(profile){
    case FallenLogProfile::LegacyCylinder:return "legacy-cylinder";
    case FallenLogProfile::ScannedStraightV1:return "scanned-straight-v1";
    }
    throw std::invalid_argument("FALLEN_INVALID_CONTENT_SELECTOR");
}
inline void ValidateFallenLogOriginalProps(std::span<const ForestProp> props) {
    if(props.size()!=FallenLogDetail::OriginalPropWords.size())
        throw std::invalid_argument("FALLEN_ORIGINAL_PROP_COUNT");
    unsigned logs{},rocks{};
    for(std::size_t id=0;id<props.size();++id){
        if(!FallenLogDetail::FiniteProp(props[id])||
            FallenLogDetail::Words(props[id])!=FallenLogDetail::OriginalPropWords[id])
            throw std::invalid_argument("FALLEN_ORIGINAL_PROP_WORDS");
        if(props[id].FallenLog()){
            if(std::find(FallenLogPropIds.begin(),FallenLogPropIds.end(),unsigned(id))==FallenLogPropIds.end())
                throw std::invalid_argument("FALLEN_ORIGINAL_LOG_ROLE");
            ++logs;
        }else ++rocks;
    }
    if(logs!=4||rocks!=32)throw std::invalid_argument("FALLEN_ORIGINAL_ROLE_QUOTAS");
}
inline void ValidateFallenLogTerrain(const Aevrynd::HeightField3D& terrain) {
    const auto& g=terrain.Settings();
    if(g.width!=201||g.depth!=241||FallenLogDetail::Bits(g.cellSize)!=FallenLogDetail::Bits(.5f)||
        !FallenLogDetail::Same(g.origin,{-50,0,-20})||terrain.SampleCount()!=201u*241u)
        throw std::invalid_argument("FALLEN_TERRAIN_GRID");
    std::uint64_t hash=14695981039346656037ull;
    for(unsigned z=0;z<g.depth;++z)for(unsigned x=0;x<g.width;++x){
        const float y=terrain.Height(x,z);
        if(!std::isfinite(y))throw std::invalid_argument("FALLEN_TERRAIN_NONFINITE");
        FallenLogDetail::Mix(hash,FallenLogDetail::Bits(y));
    }
    if(hash!=FallenLogTerrainFingerprint)throw std::invalid_argument("FALLEN_TERRAIN_IDENTITY");
}
inline void ValidateFallenLogSourceRoots(const Aevrynd::HeightField3D& terrain,
    std::span<const Aevrynd::Vec3> roots) {
    ValidateFallenLogTerrain(terrain);
    const auto canonical=CreateForestWoodMatureFirLayout(terrain);
    if(!canonical.reachedBudget||canonical.fingerprint!=FallenLogFirFingerprint||
        canonical.roots.size()!=100||roots.size()!=canonical.roots.size())
        throw std::invalid_argument("FALLEN_FIR_COHORT");
    for(std::size_t id=0;id<roots.size();++id)
        if(!FallenLogDetail::Same(roots[id],canonical.roots[id]))
            throw std::invalid_argument("FALLEN_FIR_ROOT_WORDS");
}
inline std::vector<ForestProp> ApplyFallenLogProfile(std::span<const ForestProp> original,
    FirLayoutSelector fir,FallenLogProfile profile) {
    ValidateFallenLogRoute(fir,profile);
    if(profile==FallenLogProfile::LegacyCylinder){
        if(original.size()>44)throw std::invalid_argument("FALLEN_LEGACY_PROP_DOMAIN");
        for(const auto& prop:original)if(!FallenLogDetail::FiniteProp(prop))
            throw std::invalid_argument("FALLEN_LEGACY_PROP_INVALID");
        return {original.begin(),original.end()};
    }
    ValidateFallenLogOriginalProps(original);
    // 先驗完全部輸入，再改私有候選；失敗不得留下半套世界。
    std::vector<ForestProp> result(original.begin(),original.end());
    for(const auto& placement:FallenLogPlacements()){
        result[placement.propId].transform=placement.pose;
        result[placement.propId].collider=placement.collider;
    }
    return result;
}
inline bool TryApplyFallenLogProfile(std::span<const ForestProp> original,
    FirLayoutSelector fir,FallenLogProfile profile,std::vector<ForestProp>& output) noexcept {
    try{
        auto candidate=ApplyFallenLogProfile(original,fir,profile);
        static_assert(noexcept(output.swap(candidate)));
        output.swap(candidate);
        return true;
    }catch(...){return false;}
}
// 舊二參數 ForestProps 完全保留，顯式內容 overload 才採用掃描。
inline std::vector<ForestProp> ForestProps(const Aevrynd::HeightField3D& terrain,
    std::span<const Aevrynd::Vec3> roots,FirLayoutSelector fir,FallenLogProfile profile) {
    ValidateFallenLogRoute(fir,profile);
    if(profile==FallenLogProfile::ScannedStraightV1)ValidateFallenLogSourceRoots(terrain,roots);
    const auto original=ForestProps(terrain,roots);
    return ApplyFallenLogProfile(original,fir,profile);
}
inline std::uint64_t FallenLogMeshPayloadFingerprint(const Aevrynd::Mesh3D& mesh) {
    std::uint64_t hash=14695981039346656037ull;
    for(const auto& v:mesh.vertices){
        for(const auto word:{FallenLogDetail::Bits(v.x),FallenLogDetail::Bits(v.y),FallenLogDetail::Bits(v.z),
            v.normal,v.color,FallenLogDetail::Bits(v.u),FallenLogDetail::Bits(v.v)})
            FallenLogDetail::Mix(hash,word);
    }
    for(const auto index:mesh.indices)FallenLogDetail::Mix(hash,index);
    return hash;
}
inline void ValidateFallenLogRawMesh(const Aevrynd::Mesh3D& mesh) {
    if(mesh.vertices.size()!=FallenLogVertexCount||mesh.indices.size()!=FallenLogIndexCount||!mesh.Valid())
        throw std::invalid_argument("FALLEN_RAW_MESH_TOPOLOGY");
    for(const auto& v:mesh.vertices)
        if(!std::isfinite(v.x)||!std::isfinite(v.y)||!std::isfinite(v.z)||!std::isfinite(v.u)||!std::isfinite(v.v))
            throw std::invalid_argument("FALLEN_RAW_MESH_NONFINITE");
    // 指紋僅辨識 CPU payload；檔案 SHA256 仍由正式資產完整性層獨立核對。
    if(FallenLogMeshPayloadFingerprint(mesh)!=12171149956384895771ull)
        throw std::invalid_argument("FALLEN_RAW_MESH_PAYLOAD");
}
inline void ValidateFallenLogVertexContainment(const Aevrynd::Mesh3D& mesh) {
    ValidateFallenLogRawMesh(mesh);
    for(const auto& placement:FallenLogPlacements())
        if(!FallenLogDetail::ContainsEveryVertex(mesh,placement.pose,placement.collider))
            throw std::invalid_argument("FALLEN_ALL_VERTEX_CONTAINMENT");
}
}
