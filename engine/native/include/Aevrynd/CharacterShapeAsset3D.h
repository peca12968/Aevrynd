#pragma once
#include "CharacterShape3D.h"
#include <filesystem>
namespace Aevrynd {
// ACMS v1：全部小端序，無結構填充。magic[4]、version u32、baseline SHA256[32]、
// vertex/index/target count各u32、indices u32[]；每target為名稱UTF8長度u16、
// 名稱bytes（1..128）、min/max float32、每頂點xyz float32。禁止尾隨資料。
// 切線需於變形後重建；此載入器只驗證資料，不證明美術或動畫品質。
struct CharacterShapeAsset3D {std::vector<CharacterShapeDelta3D> targets;};
bool LoadCharacterShapeAsset3D(std::span<const std::byte> source,std::span<const std::byte> baselineAsset,
    CharacterShapeAsset3D& output,std::string* error=nullptr);
bool LoadCharacterShapeAssetFile3D(const std::filesystem::path& path,std::span<const std::byte> baselineAsset,
    CharacterShapeAsset3D& output,std::string* error=nullptr);
}
