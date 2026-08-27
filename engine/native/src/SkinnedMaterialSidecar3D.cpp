#include "Aevrynd/SkinnedMaterialSidecar3D.h"

#include "Aevrynd/Serialization.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <initializer_list>
#include <limits>
#include <system_error>
#include <unordered_set>

namespace Aevrynd {
namespace {

using ErrorCode = SkinnedMaterialSidecarErrorCode3D;

SkinnedMaterialSidecarResult3D Failure(ErrorCode code, std::string message, std::size_t offset = 0u) {
    SkinnedMaterialSidecarResult3D result;
    result.error = {code, offset, std::move(message)};
    return result;
}

bool HasOnlyFields(const SerializationObject& object, std::initializer_list<std::string_view> names) {
    std::size_t known{};
    for (const std::string_view name : names) {
        known += object.Contains(name) ? 1u : 0u;
    }
    return known == object.FieldCount();
}

bool IsWithin(const std::filesystem::path& root, const std::filesystem::path& candidate) {
    auto rootIt = root.begin();
    auto candidateIt = candidate.begin();
    for (; rootIt != root.end(); ++rootIt, ++candidateIt) {
        if (candidateIt == candidate.end() || *rootIt != *candidateIt) {
            return false;
        }
    }
    return true;
}

bool ResolveTexturePath(
    const std::filesystem::path& canonicalRoot,
    std::string_view slot,
    std::string_view field,
    const std::string& relativeText,
    std::filesystem::path& output,
    SkinnedMaterialSidecarError3D& error) {
    const std::filesystem::path relative(relativeText);
    if (relativeText.empty() || relative.is_absolute() || relative.has_root_name() || relative.has_root_directory()) {
        error = {ErrorCode::InvalidValue, 0u, "Material '" + std::string(slot) + "' field '" + std::string(field) + "' must be a non-empty relative path"};
        return false;
    }

    std::error_code filesystemError;
    const std::filesystem::path resolved = std::filesystem::weakly_canonical(canonicalRoot / relative, filesystemError);
    if (filesystemError || !IsWithin(canonicalRoot, resolved)) {
        error = {ErrorCode::PathOutsideAssetRoot, 0u, "Material '" + std::string(slot) + "' field '" + std::string(field) + "' escapes the asset root"};
        return false;
    }
    if (!std::filesystem::is_regular_file(resolved, filesystemError) || filesystemError) {
        error = {ErrorCode::TextureFileMissing, 0u, "Material '" + std::string(slot) + "' texture does not exist: " + relativeText};
        return false;
    }
    output = resolved;
    return true;
}

bool ParseTextureLayer(
    const SerializationObject& object,
    std::string_view slot,
    std::string_view prefix,
    const std::filesystem::path& canonicalRoot,
    MaterialTextureLayerDefinition3D& output,
    SkinnedMaterialSidecarError3D& error) {
    if (!HasOnlyFields(object, {"baseColor", "roughness", "normal"}) ||
        !object.Contains("baseColor") || !object.Contains("roughness") || !object.Contains("normal")) {
        error = {ErrorCode::InvalidSchema, 0u, "Material '" + std::string(slot) + "' " + std::string(prefix) + " must contain only baseColor, roughness, and normal"};
        return false;
    }
    std::string baseColor;
    std::string roughness;
    std::string normal;
    if (!object.Read("baseColor", baseColor) || !object.Read("roughness", roughness) || !object.Read("normal", normal)) {
        error = {ErrorCode::InvalidSchema, 0u, "Material '" + std::string(slot) + "' " + std::string(prefix) + " texture paths must be strings"};
        return false;
    }
    return ResolveTexturePath(canonicalRoot, slot, std::string(prefix) + ".baseColor", baseColor, output.baseColorPath, error) &&
           ResolveTexturePath(canonicalRoot, slot, std::string(prefix) + ".roughness", roughness, output.roughnessPath, error) &&
           ResolveTexturePath(canonicalRoot, slot, std::string(prefix) + ".normal", normal, output.normalPath, error);
}

bool ParseTextures(
    const SerializationObject& object,
    std::string_view slot,
    const std::filesystem::path& canonicalRoot,
    const SkinnedMaterialSidecarLimits3D& limits,
    MaterialTextureSetDefinition3D& output,
    SkinnedMaterialSidecarError3D& error) {
    if (!HasOnlyFields(object, {"baseColor", "roughness", "normal", "maximumResolution", "layer1", "layer2"}) ||
        !object.Contains("baseColor") || !object.Contains("roughness") || !object.Contains("normal")) {
        error = {ErrorCode::InvalidSchema, 0u, "Material '" + std::string(slot) + "' textures have missing or unknown fields"};
        return false;
    }
    std::string baseColor;
    std::string roughness;
    std::string normal;
    if (!object.Read("baseColor", baseColor) || !object.Read("roughness", roughness) || !object.Read("normal", normal) ||
        !object.Read("maximumResolution", output.maximumResolution)) {
        error = {ErrorCode::InvalidSchema, 0u, "Material '" + std::string(slot) + "' texture fields have invalid types"};
        return false;
    }
    if (output.maximumResolution == 0u || output.maximumResolution > limits.maximumTextureResolution) {
        error = {ErrorCode::InvalidValue, 0u, "Material '" + std::string(slot) + "' maximumResolution is outside the configured limit"};
        return false;
    }
    if (!ResolveTexturePath(canonicalRoot, slot, "textures.baseColor", baseColor, output.baseColorPath, error) ||
        !ResolveTexturePath(canonicalRoot, slot, "textures.roughness", roughness, output.roughnessPath, error) ||
        !ResolveTexturePath(canonicalRoot, slot, "textures.normal", normal, output.normalPath, error)) {
        return false;
    }
    for (std::size_t index = 0; index < output.additionalLayers.size(); ++index) {
        const std::string name = "layer" + std::to_string(index + 1u);
        if (!object.Contains(name)) {
            continue;
        }
        SerializationObject layer;
        if (!object.Read(name, layer) || !ParseTextureLayer(layer, slot, name, canonicalRoot, output.additionalLayers[index], error)) {
            if (!error) error = {ErrorCode::InvalidSchema, 0u, "Material '" + std::string(slot) + "' " + name + " must be an object"};
            return false;
        }
    }
    return true;
}

bool ParsePbr(const SerializationObject& object, std::string_view slot, Material3D& output, SkinnedMaterialSidecarError3D& error) {
    if (!HasOnlyFields(object, {"tint", "alpha", "roughness", "metallic", "emissive", "occlusion"})) {
        error = {ErrorCode::InvalidSchema, 0u, "Material '" + std::string(slot) + "' pbr contains an unknown field"};
        return false;
    }
    Vec3 tint{output.tint.r, output.tint.g, output.tint.b};
    float alpha = output.tint.a;
    if (!object.Read("tint", tint) || !object.Read("alpha", alpha) ||
        !object.Read("roughness", output.surface.roughness) || !object.Read("metallic", output.surface.metallic) ||
        !object.Read("emissive", output.surface.emissive) || !object.Read("occlusion", output.surface.occlusion)) {
        error = {ErrorCode::InvalidSchema, 0u, "Material '" + std::string(slot) + "' pbr fields have invalid types"};
        return false;
    }
    const auto unit = [](float value) { return std::isfinite(value) && value >= 0.0f && value <= 1.0f; };
    if (!unit(tint.x) || !unit(tint.y) || !unit(tint.z) || !unit(alpha) || !unit(output.surface.roughness) ||
        !unit(output.surface.metallic) || !unit(output.surface.emissive) || !unit(output.surface.occlusion)) {
        error = {ErrorCode::InvalidValue, 0u, "Material '" + std::string(slot) + "' pbr values must be finite values from zero to one"};
        return false;
    }
    output.tint = {tint.x, tint.y, tint.z, alpha};
    return true;
}

} // 匿名命名空間

SkinnedMaterialSidecarResult3D ParseSkinnedMaterialSidecar3D(
    std::string_view document,
    std::span<const std::string> materialSlotNames,
    const std::filesystem::path& assetRoot,
    const SkinnedMaterialSidecarLimits3D& limits) {
    if (materialSlotNames.empty() || materialSlotNames.size() > limits.maximumMaterialSlots || limits.maximumDocumentBytes == 0u ||
        limits.maximumTextureResolution == 0u || !std::isfinite(limits.maximumTextureScale) || limits.maximumTextureScale <= 0.0f) {
        return Failure(ErrorCode::InvalidArgument, "Sidecar arguments or limits are invalid");
    }
    if (document.size() > limits.maximumDocumentBytes) {
        return Failure(ErrorCode::InputTooLarge, "Sidecar exceeds the configured document size limit", document.size());
    }
    std::unordered_set<std::string> uniqueNames;
    uniqueNames.reserve(materialSlotNames.size());
    for (const std::string& name : materialSlotNames) {
        if (name.empty() || !uniqueNames.insert(name).second) {
            return Failure(ErrorCode::InvalidArgument, "AESKIN material slot names must be non-empty and unique");
        }
    }

    std::error_code filesystemError;
    const std::filesystem::path canonicalRoot = std::filesystem::canonical(assetRoot, filesystemError);
    if (filesystemError || !std::filesystem::is_directory(canonicalRoot, filesystemError)) {
        return Failure(ErrorCode::InvalidArgument, "Asset root must be an existing directory");
    }

    SerializationLimits serializationLimits;
    serializationLimits.maxDocumentBytes = limits.maximumDocumentBytes;
    serializationLimits.maxFields = std::max<std::size_t>(serializationLimits.maxFields, materialSlotNames.size() * 24u + 16u);
    SerializationDocument parsed;
    SerializationError serializationError;
    if (!SerializationDocument::Decode(document, parsed, &serializationError, serializationLimits)) {
        return Failure(ErrorCode::InvalidDocument, "Sidecar JSON is invalid: " + serializationError.message, serializationError.offset);
    }
    if (!HasOnlyFields(parsed.root, {"materials"}) || !parsed.root.Contains("materials")) {
        return Failure(ErrorCode::InvalidSchema, "Sidecar root must contain only a materials object");
    }
    SerializationObject materials;
    if (!parsed.root.Read("materials", materials) || materials.FieldCount() != materialSlotNames.size()) {
        return Failure(ErrorCode::InvalidSchema, "Sidecar materials must exactly match the AESKIN material slots");
    }

    SkinnedMaterialSidecarResult3D result;
    result.materials.reserve(materialSlotNames.size());
    for (const std::string& slot : materialSlotNames) {
        if (!materials.Contains(slot)) {
            return Failure(ErrorCode::InvalidSchema, "Sidecar is missing AESKIN material slot '" + slot + "'");
        }
        SerializationObject definition;
        if (!materials.Read(slot, definition) || !HasOnlyFields(definition, {"textures", "pbr", "textureScale", "useUv", "castsShadow", "shadingModel"}) ||
            !definition.Contains("textures")) {
            return Failure(ErrorCode::InvalidSchema, "Material '" + slot + "' definition is invalid");
        }
        MaterialAssetDefinition3D material;
        material.name = slot;
        material.material.useUv = true;
        if(definition.Contains("shadingModel")){
            std::string shadingModel;
            if(!definition.Read("shadingModel",shadingModel)||(shadingModel!="default"&&shadingModel!="skin"&&shadingModel!="hair"))
                return Failure(ErrorCode::InvalidValue,"Material '"+slot+"' shadingModel must be 'default', 'skin', or 'hair'");
            material.material.shadingModel=shadingModel=="skin"?CharacterShadingModel3D::Skin:shadingModel=="hair"?CharacterShadingModel3D::Hair:CharacterShadingModel3D::Default;
        }
        if (!definition.Read("textureScale", material.material.textureScale) || !definition.Read("useUv", material.material.useUv) ||
            !definition.Read("castsShadow", material.material.castsShadow) || !std::isfinite(material.material.textureScale) ||
            material.material.textureScale <= 0.0f || material.material.textureScale > limits.maximumTextureScale) {
            return Failure(ErrorCode::InvalidValue, "Material '" + slot + "' textureScale or flags are invalid");
        }
        SerializationObject textures;
        if (!definition.Read("textures", textures)) {
            return Failure(ErrorCode::InvalidSchema, "Material '" + slot + "' textures must be an object");
        }
        SkinnedMaterialSidecarError3D parseError;
        if (!ParseTextures(textures, slot, canonicalRoot, limits, material.textures, parseError)) {
            return Failure(parseError.code, std::move(parseError.message), parseError.offset);
        }
        if (definition.Contains("pbr")) {
            SerializationObject pbr;
            if (!definition.Read("pbr", pbr) || !ParsePbr(pbr, slot, material.material, parseError)) {
                if (!parseError) parseError = {ErrorCode::InvalidSchema, 0u, "Material '" + slot + "' pbr must be an object"};
                return Failure(parseError.code, std::move(parseError.message), parseError.offset);
            }
        }
        result.materials.push_back(std::move(material));
    }
    return result;
}

SkinnedMaterialSidecarResult3D LoadSkinnedMaterialSidecarFile3D(
    const std::filesystem::path& sidecarPath,
    std::span<const std::string> materialSlotNames,
    const std::filesystem::path& assetRoot,
    const SkinnedMaterialSidecarLimits3D& limits) {
    std::error_code error;
    const std::uintmax_t size = std::filesystem::file_size(sidecarPath, error);
    if (error) return Failure(ErrorCode::FileReadFailed, "Could not determine sidecar file size");
    if (size > limits.maximumDocumentBytes || size > static_cast<std::uintmax_t>(std::numeric_limits<std::size_t>::max())) {
        return Failure(ErrorCode::InputTooLarge, "Sidecar file exceeds the configured document size limit", static_cast<std::size_t>(std::min<std::uintmax_t>(size, std::numeric_limits<std::size_t>::max())));
    }
    std::ifstream stream(sidecarPath, std::ios::binary);
    std::string document(static_cast<std::size_t>(size), '\0');
    if (!stream || (size != 0u && !stream.read(document.data(), static_cast<std::streamsize>(document.size())))) {
        return Failure(ErrorCode::FileReadFailed, "Could not read sidecar file");
    }
    return ParseSkinnedMaterialSidecar3D(document, materialSlotNames, assetRoot, limits);
}

} // 命名空間 Aevrynd
