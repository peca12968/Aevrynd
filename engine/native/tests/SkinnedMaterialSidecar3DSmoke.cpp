#include "Aevrynd/SkinnedMaterialSidecar3D.h"

#include <array>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

namespace {

bool Expect(bool condition, const char* message) {
    if (!condition) std::cerr << message << '\n';
    return condition;
}

void WriteFile(const std::filesystem::path& path, std::string_view contents = "texture") {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream(path, std::ios::binary).write(contents.data(), static_cast<std::streamsize>(contents.size()));
}

std::string ValidDocument() {
    return R"({"format":"aevrynd.serialization","version":1,"root":{"materials":{"Skin":{"textures":{"baseColor":"skin/base.png","roughness":"skin/rough.png","normal":"skin/normal.png","maximumResolution":2048,"layer1":{"baseColor":"detail/base.png","roughness":"detail/rough.png","normal":"detail/normal.png"}},"pbr":{"tint":[0.82,0.61,0.52],"alpha":1,"roughness":0.46,"metallic":0.02,"emissive":0,"occlusion":0.95},"textureScale":1.5,"useUv":true,"castsShadow":true,"shadingModel":"skin"},"Hair":{"textures":{"baseColor":"hair/base.png","roughness":"hair/rough.png","normal":"hair/normal.png","maximumResolution":1024},"pbr":{"roughness":0.7,"metallic":0},"textureScale":0.75}}}})";
}

} // 匿名命名空間

int main() {
    using namespace Aevrynd;
    const std::filesystem::path root = std::filesystem::temp_directory_path() / "aevrynd-skinned-material-sidecar-smoke";
    std::error_code ignored;
    std::filesystem::remove_all(root, ignored);
    for (const char* path : {"skin/base.png", "skin/rough.png", "skin/normal.png", "detail/base.png", "detail/rough.png", "detail/normal.png", "hair/base.png", "hair/rough.png", "hair/normal.png"}) {
        WriteFile(root / path);
    }
    const std::array<std::string, 2> slots{"Skin", "Hair"};
    bool passed = true;

    const auto parsed = ParseSkinnedMaterialSidecar3D(ValidDocument(), slots, root);
    passed &= Expect(parsed && parsed.materials.size() == 2u, "valid sidecar did not parse");
    if (parsed && parsed.materials.size() == 2u) {
        passed &= Expect(parsed.materials[0].name == "Skin" && parsed.materials[1].name == "Hair", "AESKIN slot order was not preserved");
        passed &= Expect(parsed.materials[0].textures.maximumResolution == 2048u, "maximumResolution was not applied");
        passed &= Expect(!parsed.materials[0].textures.additionalLayers[0].baseColorPath.empty(), "additional texture layer was not resolved");
        passed &= Expect(std::abs(parsed.materials[0].material.textureScale - 1.5f) < 0.0001f, "textureScale was not applied");
        passed &= Expect(std::abs(parsed.materials[0].material.surface.roughness - 0.46f) < 0.0001f, "PBR override was not applied");
        passed &= Expect(parsed.materials[0].material.shadingModel == CharacterShadingModel3D::Skin && parsed.materials[1].material.shadingModel == CharacterShadingModel3D::Default, "character shading model was not applied or defaulted");
        passed &= Expect(parsed.materials[0].textures.baseColorPath.is_absolute(), "resolved texture path is not absolute");
    }

    const std::filesystem::path sidecar = root / "character.materials.json";
    WriteFile(sidecar, ValidDocument());
    passed &= Expect(LoadSkinnedMaterialSidecarFile3D(sidecar, slots, root).materials.size() == 2u, "file sidecar did not load");

    std::string missing = ValidDocument();
    const std::size_t missingPosition = missing.find("hair/normal.png");
    missing.replace(missingPosition, std::string("hair/normal.png").size(), "hair/missing.png");
    const auto missingResult = ParseSkinnedMaterialSidecar3D(missing, slots, root);
    passed &= Expect(!missingResult && missingResult.materials.empty() && missingResult.error.code == SkinnedMaterialSidecarErrorCode3D::TextureFileMissing,
        "missing texture was not an atomic failure");

    std::string traversal = ValidDocument();
    const std::size_t traversalPosition = traversal.find("skin/base.png");
    traversal.replace(traversalPosition, std::string("skin/base.png").size(), "../outside.png");
    const auto traversalResult = ParseSkinnedMaterialSidecar3D(traversal, slots, root);
    passed &= Expect(!traversalResult && traversalResult.materials.empty() && traversalResult.error.code == SkinnedMaterialSidecarErrorCode3D::PathOutsideAssetRoot,
        "asset-root traversal was not rejected");

    std::string invalidPbr = ValidDocument();
    const std::size_t pbrPosition = invalidPbr.find("\"roughness\":0.46");
    invalidPbr.replace(pbrPosition, std::string("\"roughness\":0.46").size(), "\"roughness\":1.5");
    passed &= Expect(!ParseSkinnedMaterialSidecar3D(invalidPbr, slots, root), "out-of-range PBR value was accepted");

    std::string unknownSlot = ValidDocument();
    const std::size_t hairPosition = unknownSlot.find("\"Hair\"");
    unknownSlot.replace(hairPosition, 6u, "\"Other\"");
    passed &= Expect(!ParseSkinnedMaterialSidecar3D(unknownSlot, slots, root), "slot mismatch was accepted");

    SkinnedMaterialSidecarLimits3D tinyLimits;
    tinyLimits.maximumDocumentBytes = 32u;
    const auto oversized = ParseSkinnedMaterialSidecar3D(ValidDocument(), slots, root, tinyLimits);
    passed &= Expect(!oversized && oversized.error.code == SkinnedMaterialSidecarErrorCode3D::InputTooLarge, "document size limit was not enforced");

    std::filesystem::remove_all(root, ignored);
    return passed ? 0 : 1;
}
