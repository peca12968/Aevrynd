#pragma once
#include "MistwatchWoodlandCanopy.h"

namespace Mistwatch::WoodlandCanopy {
// run9 的真 CPU 作者與獨立全幾何驗證；此配方不是 native／下游／美術資格。
inline constexpr std::uint64_t FixedInputIdentity=10428373636874817235ull;
inline constexpr std::uint64_t FixedOutputIdentity=883808528841219062ull;
inline constexpr std::uint64_t FixedWoodFingerprint=10097770619612325227ull;
inline constexpr std::size_t FixedWoodParts=12863;
// run10 獨立串流核對的十二組界線；不能只驗扁平木件總數。
inline constexpr std::uint64_t FixedGroupedWoodFingerprint=7162537194293152961ull;
inline constexpr std::array<std::size_t,12> FixedGroupParts{{1127,938,1133,1137,996,1140,1053,1093,1041,1094,1046,1065}};
inline constexpr unsigned FixedFirCount=95,FixedPropCount=33,FixedSaplingCount=36,FixedMiddleCount=20;
inline constexpr unsigned FixedBodyCount=209,FixedVegetationCount=229,FixedCompleteBefore=13092,FixedCompleteAfter=13093;
// 遊戲只重播固定的 source-ID／原位元退讓表，不搜尋作者落點、不刪除硬設施。
inline Recipe FixedRecipe(){
    return {FixedInputIdentity,{
        {EntranceRole::Sapling,1u,0u,0u,{3244585500u,1064949519u,1111184894u,0u,1079026879u,0u,1049442772u,1049442772u,1049442772u},{3244585500u,1075738052u,1111184894u,1037649223u,1069547520u,1037649223u},WithdrawalReason::FullEnvelopeOccupied},
        {EntranceRole::Sapling,2u,2u,0u,{1096804572u,1067248081u,1113051033u,0u,1074269237u,0u,1049149779u,1049149779u,1049149779u},{1096804572u,1076786408u,1113051033u,1037198464u,1069547520u,1037198464u},WithdrawalReason::FullEnvelopeOccupied},
        {EntranceRole::Sapling,4u,3u,0u,{3245179668u,1067319823u,1116666870u,0u,1083564343u,0u,1051706851u,1051706851u,1051706851u},{3245179668u,1076822280u,1116666870u,1040659907u,1069547520u,1040659907u},WithdrawalReason::FullEnvelopeOccupied},
        {EntranceRole::Sapling,40u,2u,0u,{1098792236u,1065354705u,1111861680u,0u,1071139550u,0u,1051173602u,1051173602u,1051173602u},{1098792236u,1075839720u,1111861680u,1040249715u,1069547520u,1040249715u},WithdrawalReason::FullEnvelopeOccupied},
        {EntranceRole::Middle,6u,2u,0u,{1095570536u,1060418859u,1111855247u,0u,1078035000u,0u,1060429262u,1060429262u,1060429262u},{1095402775u,1068812668u,1111892184u,1047092883u,1060429262u,1048874245u},WithdrawalReason::FullEnvelopeOccupied},
        {EntranceRole::Middle,18u,0u,0u,{3241859816u,1051234008u,1110626175u,0u,1082248903u,0u,1061477920u,1061477920u,1061477920u},{3241815253u,1066177222u,1110685977u,1051528434u,1061477920u,1051292286u},WithdrawalReason::FullEnvelopeOccupied},
        {EntranceRole::Middle,21u,3u,0u,{3246701838u,1069140225u,1116944795u,0u,1027158270u,0u,1060349321u,1060349321u,1060349321u},{3246631555u,1074384355u,1116923310u,1045924191u,1060349321u,1048174980u},WithdrawalReason::FullEnvelopeOccupied}
    },{}};
}
}
