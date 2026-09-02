#include "Aevrynd/SkinnedMaterialSidecar3D.h"

#include "Aevrynd/Serialization.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <fstream>
#include <initializer_list>
#include <limits>
#include <system_error>
#include <unordered_map>
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

using TextureIntegrityByPath3D = std::unordered_map<std::filesystem::path, MaterialTextureIntegrity3D>;

bool ParseTextureReference(
    const SerializationObject& parent,
    std::string_view slot,
    std::string_view field,
    const std::filesystem::path& canonicalRoot,
    std::filesystem::path& path,
    MaterialTextureIntegrity3D& integrity,
    TextureIntegrityByPath3D& knownIntegrity,
    SkinnedMaterialSidecarError3D& error) {
    SerializationObject reference;
    std::string relativeText;
    if (!parent.Read(field, reference) ||
        !HasOnlyFields(reference, {"path", "expectedBytes", "expectedSha256"}) ||
        !reference.Read("path", relativeText) ||
        !reference.Read("expectedBytes", integrity.expectedBytes) ||
        !reference.Read("expectedSha256", integrity.expectedSha256)) {
        error = {ErrorCode::InvalidSchema, 0u, "Material '" + std::string(slot) + "' field '" + std::string(field) + "' must contain only path, expectedBytes, and expectedSha256"};
        return false;
    }
    const bool validSha = integrity.expectedSha256.size() == 64u &&
        std::ranges::all_of(integrity.expectedSha256, [](unsigned char value) { return std::isxdigit(value) != 0; });
    if (!integrity.expectedBytes || !validSha) {
        error = {ErrorCode::InvalidValue, 0u, "Material '" + std::string(slot) + "' field '" + std::string(field) + "' has invalid sealed integrity"};
        return false;
    }
    std::ranges::transform(integrity.expectedSha256, integrity.expectedSha256.begin(),
        [](unsigned char value) { return static_cast<char>(std::tolower(value)); });
    if (!ResolveTexturePath(canonicalRoot, slot, field, relativeText, path, error)) return false;
    const auto [found, inserted] = knownIntegrity.emplace(path, integrity);
    if (!inserted && (found->second.expectedBytes != integrity.expectedBytes ||
                      found->second.expectedSha256 != integrity.expectedSha256)) {
        error = {ErrorCode::InvalidValue, 0u, "Material '" + std::string(slot) + "' field '" + std::string(field) + "' conflicts with sealed integrity already declared for the same texture path"};
        return false;
    }
    return true;
}

bool ParseTextureLayer(
    const SerializationObject& object,
    std::string_view slot,
    std::string_view prefix,
    const std::filesystem::path& canonicalRoot,
    MaterialTextureLayerDefinition3D& output,
    TextureIntegrityByPath3D& knownIntegrity,
    SkinnedMaterialSidecarError3D& error) {
    const bool hasRoughness=object.Contains("roughness"),hasOrm=object.Contains("orm");
    if (!HasOnlyFields(object, {"baseColor", "roughness", "orm", "normal"}) ||
        !object.Contains("baseColor") || hasRoughness==hasOrm || !object.Contains("normal")) {
        error = {ErrorCode::InvalidSchema, 0u, "Material '" + std::string(slot) + "' " + std::string(prefix) + " must contain baseColor, normal, and exactly one of roughness or orm"};
        return false;
    }
    const std::string_view surfaceField=hasOrm?"orm":"roughness";
    output.surfaceEncoding=hasOrm?MaterialSurfaceMapEncoding3D::PackedOrm:MaterialSurfaceMapEncoding3D::RoughnessR;
    return ParseTextureReference(object,slot,"baseColor",canonicalRoot,output.baseColorPath,output.integrity[0],knownIntegrity,error) &&
           ParseTextureReference(object,slot,surfaceField,canonicalRoot,output.roughnessPath,output.integrity[1],knownIntegrity,error) &&
           ParseTextureReference(object,slot,"normal",canonicalRoot,output.normalPath,output.integrity[2],knownIntegrity,error);
}

bool ParseTextures(
    const SerializationObject& object,
    std::string_view slot,
    const std::filesystem::path& canonicalRoot,
    const SkinnedMaterialSidecarLimits3D& limits,
    MaterialTextureSetDefinition3D& output,
    TextureIntegrityByPath3D& knownIntegrity,
    SkinnedMaterialSidecarError3D& error) {
    const bool hasRoughness=object.Contains("roughness"),hasOrm=object.Contains("orm");
    if (!HasOnlyFields(object, {"baseColor", "roughness", "orm", "normal", "maximumResolution", "layer1", "layer2"}) ||
        !object.Contains("baseColor") || hasRoughness==hasOrm || !object.Contains("normal")) {
        error = {ErrorCode::InvalidSchema, 0u, "Material '" + std::string(slot) + "' textures have missing or unknown fields"};
        return false;
    }
    const std::string_view surfaceField=hasOrm?"orm":"roughness";
    if (!object.Read("maximumResolution", output.maximumResolution)) {
        error = {ErrorCode::InvalidSchema, 0u, "Material '" + std::string(slot) + "' texture fields have invalid types"};
        return false;
    }
    if (output.maximumResolution == 0u || output.maximumResolution > limits.maximumTextureResolution) {
        error = {ErrorCode::InvalidValue, 0u, "Material '" + std::string(slot) + "' maximumResolution is outside the configured limit"};
        return false;
    }
    output.surfaceEncoding=hasOrm?MaterialSurfaceMapEncoding3D::PackedOrm:MaterialSurfaceMapEncoding3D::RoughnessR;
    if (!ParseTextureReference(object,slot,"baseColor",canonicalRoot,output.baseColorPath,output.integrity[0],knownIntegrity,error) ||
        !ParseTextureReference(object,slot,surfaceField,canonicalRoot,output.roughnessPath,output.integrity[1],knownIntegrity,error) ||
        !ParseTextureReference(object,slot,"normal",canonicalRoot,output.normalPath,output.integrity[2],knownIntegrity,error)) {
        return false;
    }
    for (std::size_t index = 0; index < output.additionalLayers.size(); ++index) {
        const std::string name = "layer" + std::to_string(index + 1u);
        if (!object.Contains(name)) {
            continue;
        }
        SerializationObject layer;
        if (!object.Read(name, layer) || !ParseTextureLayer(layer, slot, name, canonicalRoot, output.additionalLayers[index], knownIntegrity, error)) {
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
    TextureIntegrityByPath3D knownIntegrity;
    for (const std::string& slot : materialSlotNames) {
        if (!materials.Contains(slot)) {
            return Failure(ErrorCode::InvalidSchema, "Sidecar is missing AESKIN material slot '" + slot + "'");
        }
        SerializationObject definition;
        if (!materials.Read(slot, definition) || !HasOnlyFields(definition, {"textures", "pbr", "textureScale", "baseTextureScale", "detailTextureScale", "normalScale", "useUv", "castsShadow", "shadingModel", "alphaMode", "alphaCutoff", "doubleSided"}) ||
            !definition.Contains("textures")) {
            return Failure(ErrorCode::InvalidSchema, "Material '" + slot + "' definition is invalid");
        }
        MaterialAssetDefinition3D material;
        material.name = slot;
        material.material.useUv = true;
        if(definition.Contains("shadingModel")){
            std::string shadingModel;
            if(!definition.Read("shadingModel",shadingModel)||(shadingModel!="default"&&shadingModel!="skin"&&shadingModel!="hair"&&shadingModel!="eye"&&shadingModel!="cloth"))
                return Failure(ErrorCode::InvalidValue,"Material '"+slot+"' shadingModel must be 'default', 'skin', 'hair', 'eye', or 'cloth'");
            material.material.shadingModel=shadingModel=="skin"?CharacterShadingModel3D::Skin:shadingModel=="hair"?CharacterShadingModel3D::Hair:shadingModel=="eye"?CharacterShadingModel3D::Eye:shadingModel=="cloth"?CharacterShadingModel3D::Cloth:CharacterShadingModel3D::Default;
        }
        if(definition.Contains("alphaMode")){
            std::string alphaMode;
            if(!definition.Read("alphaMode",alphaMode)||(alphaMode!="opaque"&&alphaMode!="mask"&&alphaMode!="blend"))return Failure(ErrorCode::InvalidValue,"Material '"+slot+"' alphaMode must be 'opaque', 'mask', or 'blend'");
            material.material.alphaMode=alphaMode=="mask"?MaterialAlphaMode3D::Mask:alphaMode=="blend"?MaterialAlphaMode3D::Blend:MaterialAlphaMode3D::Opaque;
        }
        if(!definition.Read("alphaCutoff",material.material.alphaCutoff)||!definition.Read("doubleSided",material.material.doubleSided)||!std::isfinite(material.material.alphaCutoff)||material.material.alphaCutoff<0.f||material.material.alphaCutoff>1.f)return Failure(ErrorCode::InvalidValue,"Material '"+slot+"' alphaCutoff or doubleSided is invalid");
        if (!definition.Read("textureScale", material.material.textureScale) || !definition.Read("useUv", material.material.useUv) ||
            !definition.Read("castsShadow", material.material.castsShadow) || !std::isfinite(material.material.textureScale) ||
            material.material.textureScale <= 0.0f || material.material.textureScale > limits.maximumTextureScale) {
            return Failure(ErrorCode::InvalidValue, "Material '" + slot + "' textureScale or flags are invalid");
        }
        if((definition.Contains("baseTextureScale")&&(!definition.Read("baseTextureScale",material.material.baseTextureScale)||!std::isfinite(material.material.baseTextureScale)||material.material.baseTextureScale<=0.f||material.material.baseTextureScale>limits.maximumTextureScale))||
           (definition.Contains("detailTextureScale")&&(!definition.Read("detailTextureScale",material.material.detailTextureScale)||!std::isfinite(material.material.detailTextureScale)||material.material.detailTextureScale<=0.f||material.material.detailTextureScale>limits.maximumTextureScale)))
            return Failure(ErrorCode::InvalidValue,"Material '"+slot+"' baseTextureScale or detailTextureScale is invalid");
        if(definition.Contains("normalScale")&&(!definition.Read("normalScale",material.material.normalScale)||!std::isfinite(material.material.normalScale)||material.material.normalScale<-16.f||material.material.normalScale>15.96875f))
            return Failure(ErrorCode::InvalidValue,"Material '"+slot+"' normalScale must be finite and within the runtime range");
        SerializationObject textures;
        if (!definition.Read("textures", textures)) {
            return Failure(ErrorCode::InvalidSchema, "Material '" + slot + "' textures must be an object");
        }
        SkinnedMaterialSidecarError3D parseError;
        if (!ParseTextures(textures, slot, canonicalRoot, limits, material.textures, knownIntegrity, parseError)) {
            return Failure(parseError.code, std::move(parseError.message), parseError.offset);
        }
        material.textures.alphaMode=material.material.alphaMode;
        material.textures.alphaCutoff=material.material.alphaCutoff;
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
