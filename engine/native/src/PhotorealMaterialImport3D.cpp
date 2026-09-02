#include <Aevrynd/PhotorealMaterialImport3D.h>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <bcrypt.h>

#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <fstream>
#include <limits>
#include <set>
#include <unordered_set>

#pragma comment(lib, "bcrypt.lib")

namespace Aevrynd {
namespace {

enum class JsonType : std::uint8_t { Null, Boolean, Number, String, Array, Object };
struct JsonValue {
    JsonType type{JsonType::Null};
    bool boolean{};
    double number{};
    std::string numberLexeme;
    std::uint64_t unsignedInteger{};
    bool isUnsignedInteger{};
    std::string string;
    std::vector<JsonValue> array;
    std::vector<std::pair<std::string, JsonValue>> object;
};

void SetError(PhotorealMaterialImportError3D* error, PhotorealMaterialImportErrorCode3D code,
    std::size_t offset, std::string message) {
    if (error) *error = {code, offset, std::move(message)};
}

class JsonParser {
public:
    explicit JsonParser(std::string_view text) : text_(text) {}
    bool Parse(JsonValue& output, PhotorealMaterialImportError3D* error) {
        if (!Value(output, 0, error)) return false;
        Space();
        if (position_ != text_.size()) {
            SetError(error, PhotorealMaterialImportErrorCode3D::InvalidJson, position_, "Trailing JSON data");
            return false;
        }
        return true;
    }
private:
    void Space() { while (position_ < text_.size() && (text_[position_] == ' ' || text_[position_] == '\t' || text_[position_] == '\r' || text_[position_] == '\n')) ++position_; }
    bool String(std::string& output, PhotorealMaterialImportError3D* error) {
        if (position_ >= text_.size() || text_[position_++] != '"') return false;
        while (position_ < text_.size()) {
            unsigned char value = static_cast<unsigned char>(text_[position_++]);
            if (value == '"') return true;
            if (value < 0x20 || value >= 0x80) {
                SetError(error, PhotorealMaterialImportErrorCode3D::InvalidJson, position_ - 1, "Sidecar strings must be printable ASCII");
                return false;
            }
            if (value != '\\') { output.push_back(static_cast<char>(value)); continue; }
            if (position_ >= text_.size()) return false;
            const char escaped = text_[position_++];
            switch (escaped) {
            case '"': case '\\': case '/': output.push_back(escaped); break;
            case 'b': output.push_back('\b'); break; case 'f': output.push_back('\f'); break;
            case 'n': output.push_back('\n'); break; case 'r': output.push_back('\r'); break; case 't': output.push_back('\t'); break;
            default:
                SetError(error, PhotorealMaterialImportErrorCode3D::InvalidJson, position_ - 1, "Unsupported JSON escape");
                return false;
            }
        }
        SetError(error, PhotorealMaterialImportErrorCode3D::InvalidJson, position_, "Unterminated JSON string");
        return false;
    }
    bool Value(JsonValue& output, unsigned depth, PhotorealMaterialImportError3D* error) {
        if (depth > 16) { SetError(error, PhotorealMaterialImportErrorCode3D::InputLimitExceeded, position_, "JSON nesting exceeds limit"); return false; }
        Space();
        if (position_ >= text_.size()) return false;
        if (text_[position_] == '"') { output.type = JsonType::String; return String(output.string, error); }
        if (text_.substr(position_, 4) == "true") { position_ += 4; output.type = JsonType::Boolean; output.boolean = true; return true; }
        if (text_.substr(position_, 5) == "false") { position_ += 5; output.type = JsonType::Boolean; return true; }
        if (text_.substr(position_, 4) == "null") { position_ += 4; return true; }
        if (text_[position_] == '-' || (text_[position_] >= '0' && text_[position_] <= '9')) {
            const std::size_t start = position_;
            if (text_[position_] == '-') ++position_;
            if (position_ >= text_.size() || text_[position_] < '0' || text_[position_] > '9') return false;
            if (text_[position_] == '0') ++position_;
            else while (position_ < text_.size() && text_[position_] >= '0' && text_[position_] <= '9') ++position_;
            if (position_ < text_.size() && text_[position_] == '.') {
                ++position_;
                const std::size_t fraction = position_;
                while (position_ < text_.size() && text_[position_] >= '0' && text_[position_] <= '9') ++position_;
                if (position_ == fraction) return false;
            }
            if (position_ < text_.size() && (text_[position_] == 'e' || text_[position_] == 'E')) {
                ++position_;
                if (position_ < text_.size() && (text_[position_] == '+' || text_[position_] == '-')) ++position_;
                const std::size_t exponent = position_;
                while (position_ < text_.size() && text_[position_] >= '0' && text_[position_] <= '9') ++position_;
                if (position_ == exponent) return false;
            }
            output.numberLexeme.assign(text_.substr(start, position_ - start));
            auto converted = std::from_chars(text_.data() + start, text_.data() + position_, output.number);
            if (converted.ec != std::errc{} || converted.ptr != text_.data() + position_ || !std::isfinite(output.number)) return false;
            if (output.numberLexeme.find_first_of("-.eE") == std::string::npos) {
                const auto integer = std::from_chars(output.numberLexeme.data(), output.numberLexeme.data() + output.numberLexeme.size(), output.unsignedInteger);
                output.isUnsignedInteger = integer.ec == std::errc{} && integer.ptr == output.numberLexeme.data() + output.numberLexeme.size();
            }
            output.type = JsonType::Number; return true;
        }
        if (text_[position_] == '[') {
            ++position_; output.type = JsonType::Array; Space();
            if (position_ < text_.size() && text_[position_] == ']') { ++position_; return true; }
            while (true) {
                output.array.emplace_back(); if (!Value(output.array.back(), depth + 1, error)) return false; Space();
                if (position_ >= text_.size()) return false; const char separator = text_[position_++];
                if (separator == ']') return true; if (separator != ',') return false;
            }
        }
        if (text_[position_] == '{') {
            ++position_; output.type = JsonType::Object; Space(); std::unordered_set<std::string> keys;
            if (position_ < text_.size() && text_[position_] == '}') { ++position_; return true; }
            while (true) {
                Space(); std::string key; if (!String(key, error) || !keys.insert(key).second) {
                    SetError(error, PhotorealMaterialImportErrorCode3D::InvalidJson, position_, "Invalid or duplicate JSON object key"); return false;
                }
                Space(); if (position_ >= text_.size() || text_[position_++] != ':') return false;
                output.object.emplace_back(std::move(key), JsonValue{}); if (!Value(output.object.back().second, depth + 1, error)) return false; Space();
                if (position_ >= text_.size()) return false; const char separator = text_[position_++];
                if (separator == '}') return true; if (separator != ',') return false;
            }
        }
        SetError(error, PhotorealMaterialImportErrorCode3D::InvalidJson, position_, "Invalid JSON token"); return false;
    }
    std::string_view text_;
    std::size_t position_{};
};

const JsonValue* Field(const JsonValue& object, std::string_view name) {
    if (object.type != JsonType::Object) return nullptr;
    const auto found = std::find_if(object.object.begin(), object.object.end(), [&](const auto& field) { return field.first == name; });
    return found == object.object.end() ? nullptr : &found->second;
}
bool StringField(const JsonValue& object, std::string_view name, std::string& output) {
    const JsonValue* value = Field(object, name); if (!value || value->type != JsonType::String) return false; output = value->string; return true;
}
bool BooleanField(const JsonValue& object, std::string_view name, bool& output) {
    const JsonValue* value = Field(object, name); if (!value || value->type != JsonType::Boolean) return false; output = value->boolean; return true;
}

std::string Lower(std::string value) { std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); }); return value; }
bool ValidName(std::string_view value) { return !value.empty() && value.size() <= 128 && std::all_of(value.begin(), value.end(), [](unsigned char c) { return std::isalnum(c) || c == '-' || c == '_'; }); }
bool AcceptedPath(std::string_view value) {
    if (value.empty() || value.size() > 4096 || value.front() == '/' || value.find('\\') != value.npos || value.find(':') != value.npos) return false;
    std::size_t start{}; while (start < value.size()) { const auto end = value.find('/', start); const auto part = value.substr(start, end == value.npos ? value.size() - start : end - start); if (part.empty() || part == "." || part == "..") return false; if (end == value.npos) break; start = end + 1; }
    return true;
}

bool Sha256(std::span<const std::byte> bytes, std::string& output) {
    BCRYPT_ALG_HANDLE algorithm{}; BCRYPT_HASH_HANDLE hash{}; DWORD objectBytes{}, copied{}; std::vector<UCHAR> object; std::array<UCHAR, 32> digest{};
    if (BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0) < 0) return false;
    const bool property = BCryptGetProperty(algorithm, BCRYPT_OBJECT_LENGTH, reinterpret_cast<PUCHAR>(&objectBytes), sizeof(objectBytes), &copied, 0) >= 0;
    if (property) object.resize(objectBytes);
    const bool created = property && BCryptCreateHash(algorithm, &hash, object.data(), objectBytes, nullptr, 0, 0) >= 0;
    bool passed = created;
    for (std::size_t offset = 0; passed && offset < bytes.size();) {
        const ULONG amount = static_cast<ULONG>((std::min)(bytes.size() - offset, std::size_t((std::numeric_limits<ULONG>::max)())));
        passed = BCryptHashData(hash, reinterpret_cast<PUCHAR>(const_cast<std::byte*>(bytes.data() + offset)), amount, 0) >= 0; offset += amount;
    }
    passed = passed && BCryptFinishHash(hash, digest.data(), static_cast<ULONG>(digest.size()), 0) >= 0;
    if (hash) BCryptDestroyHash(hash); BCryptCloseAlgorithmProvider(algorithm, 0); if (!passed) return false;
    constexpr char Hex[] = "0123456789abcdef"; output.resize(64); for (std::size_t i = 0; i < digest.size(); ++i) { output[i * 2] = Hex[digest[i] >> 4]; output[i * 2 + 1] = Hex[digest[i] & 15]; } return true;
}

bool ReadVerified(const std::filesystem::path& root, std::string_view relative, std::size_t maximumBytes,
    const PhotorealRuntimeAssetReceipt3D& runtime, std::vector<std::byte>& bytes, std::string& hash,
    PhotorealMaterialImportError3D* error) {
    if (!AcceptedPath(relative)) { SetError(error, PhotorealMaterialImportErrorCode3D::UnsafePath, 0, "Texture path is unsafe"); return false; }
    std::error_code ec; const auto absoluteRoot = std::filesystem::absolute(root, ec).lexically_normal(); if (ec) return false;
    auto lexical = absoluteRoot; const DWORD rootAttributes = GetFileAttributesW(absoluteRoot.c_str());
    if (rootAttributes == INVALID_FILE_ATTRIBUTES || (rootAttributes & FILE_ATTRIBUTE_REPARSE_POINT)) { SetError(error, PhotorealMaterialImportErrorCode3D::UnsafePath, 0, "Sidecar root is a reparse point"); return false; }
    for (const auto& component : std::filesystem::path(relative)) {
        lexical /= component; const DWORD attributes = GetFileAttributesW(lexical.c_str());
        if (attributes == INVALID_FILE_ATTRIBUTES) { SetError(error, PhotorealMaterialImportErrorCode3D::FileMissing, 0, "Texture path is missing"); return false; }
        if (attributes & FILE_ATTRIBUTE_REPARSE_POINT) { SetError(error, PhotorealMaterialImportErrorCode3D::UnsafePath, 0, "Texture path contains a reparse point"); return false; }
    }
    const auto canonicalRoot = std::filesystem::canonical(absoluteRoot, ec); if (ec) return false;
    const auto canonical = std::filesystem::canonical(root / std::filesystem::path(relative), ec);
    if (ec || canonical == canonicalRoot || std::mismatch(canonicalRoot.begin(), canonicalRoot.end(), canonical.begin(), canonical.end()).first != canonicalRoot.end()) {
        SetError(error, PhotorealMaterialImportErrorCode3D::UnsafePath, 0, "Texture path escaped sidecar directory"); return false;
    }
    const auto size = std::filesystem::file_size(canonical, ec); if (ec || !size || size > maximumBytes) { SetError(error, PhotorealMaterialImportErrorCode3D::InputLimitExceeded, 0, "Texture file size is invalid"); return false; }
    bytes.resize(static_cast<std::size_t>(size)); std::ifstream stream(canonical, std::ios::binary); stream.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    if (!stream || !Sha256(bytes, hash) || std::find(runtime.acceptedSha256.begin(), runtime.acceptedSha256.end(), hash) == runtime.acceptedSha256.end()) {
        SetError(error, PhotorealMaterialImportErrorCode3D::FileHashMismatch, 0, "Texture file hash is absent from the runtime receipt"); return false;
    }
    return true;
}

bool JpegSize(std::span<const std::byte> bytes, std::uint32_t& width, std::uint32_t& height) {
    if (bytes.size() < 4 || std::to_integer<unsigned char>(bytes[0]) != 0xff || std::to_integer<unsigned char>(bytes[1]) != 0xd8) return false;
    std::size_t offset = 2; while (offset + 4 <= bytes.size()) {
        if (std::to_integer<unsigned char>(bytes[offset++]) != 0xff) return false; while (offset < bytes.size() && std::to_integer<unsigned char>(bytes[offset]) == 0xff) ++offset; if (offset >= bytes.size()) return false;
        const unsigned marker = std::to_integer<unsigned char>(bytes[offset++]); if (marker == 0xd9 || marker == 0xda) return false;
        const unsigned length = (std::to_integer<unsigned char>(bytes[offset]) << 8) | std::to_integer<unsigned char>(bytes[offset + 1]); if (length < 2 || offset + length > bytes.size()) return false;
        if ((marker >= 0xc0 && marker <= 0xc3) || (marker >= 0xc5 && marker <= 0xc7) || (marker >= 0xc9 && marker <= 0xcb) || (marker >= 0xcd && marker <= 0xcf)) {
            if (length < 7) return false; height = (std::to_integer<unsigned char>(bytes[offset + 3]) << 8) | std::to_integer<unsigned char>(bytes[offset + 4]); width = (std::to_integer<unsigned char>(bytes[offset + 5]) << 8) | std::to_integer<unsigned char>(bytes[offset + 6]); return width && height;
        }
        offset += length;
    }
    return false;
}

TextureResolutionTier Tier(std::uint32_t maximum) { return maximum >= 4096 ? TextureResolutionTier::Hero : maximum >= 2048 ? TextureResolutionTier::Near : maximum >= 1024 ? TextureResolutionTier::General : TextureResolutionTier::Far; }

} // namespace

bool PhotorealMaterialImport3D::BuildPlan(const PhotorealRuntimeManifest3D& runtimeManifest, std::string_view assetId,
    const std::filesystem::path& sidecarPath, PhotorealMaterialImportPlan3D& output,
    PhotorealMaterialImportError3D* error, const PhotorealMaterialImportLimits3D& limits) {
    if (error) *error = {}; if (!limits.maximumJsonBytes || !limits.maximumMaterials || !limits.maximumSourceBytes || !limits.maximumTextureDimension) { SetError(error, PhotorealMaterialImportErrorCode3D::InputLimitExceeded, 0, "Import limits rejected input"); return false; }
    if (!runtimeManifest.Passed()) { SetError(error, PhotorealMaterialImportErrorCode3D::RuntimeManifestRejected, 0, "Runtime manifest did not pass production gates"); return false; }
    const auto* runtime = runtimeManifest.Find(assetId); if (!runtime) { SetError(error, PhotorealMaterialImportErrorCode3D::AssetNotFound, 0, "Runtime asset was not found"); return false; }
    if (!runtime->licensePassed || !runtime->memoryGatePassed || !runtime->hashesPassed || !runtime->channelsPassed || !runtime->lodReceiptPassed) { SetError(error, PhotorealMaterialImportErrorCode3D::LicenseRejected, 0, "Runtime asset receipts are incomplete"); return false; }
    std::vector<std::byte> sidecarBytes; std::string sidecarHash;
    if (!ReadVerified(sidecarPath.parent_path(), sidecarPath.filename().generic_string(), limits.maximumJsonBytes, *runtime, sidecarBytes, sidecarHash, error)) return false;
    const std::string_view sidecarJson(reinterpret_cast<const char*>(sidecarBytes.data()), sidecarBytes.size());
    JsonValue root; if (!JsonParser(sidecarJson).Parse(root, error) || root.type != JsonType::Object) return false;
    const JsonValue* schema = Field(root, "schemaVersion"); const JsonValue* materials = Field(root, "materials");
    const JsonValue* outputResolution = Field(root, "outputResolution"); const JsonValue* expectedMipCount = Field(root, "expectedMipCount");
    if (!schema || schema->type != JsonType::Number || !schema->isUnsignedInteger || schema->unsignedInteger != 1 ||
        !materials || materials->type != JsonType::Array || materials->array.empty() || materials->array.size() > limits.maximumMaterials ||
        !outputResolution || outputResolution->type != JsonType::Number || !outputResolution->isUnsignedInteger ||
        (outputResolution->unsignedInteger != 512 && outputResolution->unsignedInteger != 1024 && outputResolution->unsignedInteger != 2048 && outputResolution->unsignedInteger != 4096) ||
        !expectedMipCount || expectedMipCount->type != JsonType::Number || !expectedMipCount->isUnsignedInteger) { SetError(error, PhotorealMaterialImportErrorCode3D::InvalidSchema, 0, "Material sidecar schema is invalid"); return false; }
    std::uint64_t mipCount = 1; for (std::uint64_t size = outputResolution->unsignedInteger; size > 1; size >>= 1) ++mipCount;
    if (expectedMipCount->unsignedInteger != mipCount) { SetError(error, PhotorealMaterialImportErrorCode3D::InvalidSchema, 0, "Material output resolution or mip contract is invalid"); return false; }
    std::string binding; if (Field(root, "binding") && (!StringField(root, "binding", binding) || binding != "runtime-sidecar")) { SetError(error, PhotorealMaterialImportErrorCode3D::InvalidSchema, 0, "Material binding is not runtime-sidecar"); return false; }
    std::string convention = "OpenGL"; if (Field(root, "normalConvention") && !StringField(root, "normalConvention", convention)) return false; if (convention != "OpenGL") { SetError(error, PhotorealMaterialImportErrorCode3D::NormalConventionUnsupported, 0, "Only OpenGL source normals can be converted"); return false; }
    PhotorealMaterialImportPlan3D temporary; temporary.assetId = std::string(assetId); temporary.verifiedSourceBytes = sidecarBytes.size(); std::set<std::string> names;
    for (std::size_t index = 0; index < materials->array.size(); ++index) {
        const JsonValue& value = materials->array[index]; if (value.type != JsonType::Object) return false; std::string name, base, normal, arm, ambientOcclusion, roughness;
        const JsonValue* slot = Field(value, "slot");
        if (slot && (slot->type != JsonType::Number || !slot->isUnsignedInteger)) { SetError(error, PhotorealMaterialImportErrorCode3D::InvalidSchema, 0, "Material slot is invalid"); return false; }
        if (StringField(value, "name", name)) {
            if (!ValidName(name)) { SetError(error, PhotorealMaterialImportErrorCode3D::InvalidSchema, 0, "Material name is invalid"); return false; }
            name = Lower(name);
        } else {
            if (Field(value, "name")) { SetError(error, PhotorealMaterialImportErrorCode3D::InvalidSchema, 0, "Material name is invalid"); return false; }
            name = "slot-" + std::to_string(slot ? slot->unsignedInteger : index);
        }
        const bool hasNormalGl = Field(value, "normalGl");
        const bool hasPackedArm = Field(value, "packedArm");
        const bool hasPackedFields = hasPackedArm || Field(value, "arm");
        const bool normalValid = hasNormalGl ? StringField(value, "normalGl", normal) : StringField(value, "normal", normal);
        const bool packed = (hasPackedArm ? StringField(value, "packedArm", arm) : StringField(value, "arm", arm)) && !arm.empty();
        const JsonValue* metallic = Field(value, "metallicScalar");
        const bool separate = StringField(value, "ambientOcclusion", ambientOcclusion) && StringField(value, "roughness", roughness) &&
                              metallic && metallic->type == JsonType::Number && std::isfinite(metallic->number) && metallic->number >= 0.0 && metallic->number <= 1.0;
        const bool hasSeparateFields = Field(value, "ambientOcclusion") || Field(value, "roughness") || metallic;
        static const std::unordered_set<std::string> allowedKeys{"name", "slot", "baseColor", "normalGl", "normal", "packedArm", "arm", "ambientOcclusion", "roughness", "metallicScalar", "alphaMode", "alphaCutoff", "doubleSided", "materialClass", "alphaSource", "alphaBlackKeyThreshold", "alphaDilationPixels"};
        if (std::any_of(value.object.begin(), value.object.end(), [](const auto& field) { return !allowedKeys.contains(field.first); })) { SetError(error, PhotorealMaterialImportErrorCode3D::InvalidSchema, 0, "Material contains an unknown field"); return false; }
        if (!ValidName(name) || !names.insert(Lower(name)).second || !StringField(value, "baseColor", base) ||
            !normalValid || (hasPackedFields && !packed) || (hasSeparateFields && (!separate || packed)) || packed == separate) {
            SetError(error, PhotorealMaterialImportErrorCode3D::InvalidSchema, 0, "Material channels are incomplete, ambiguous, or duplicated"); return false;
        }
        {
            std::string alphaMode = "opaque", alphaSource, materialClass = "surface"; bool doubleSided{};
            if ((Field(value, "alphaMode") && !StringField(value, "alphaMode", alphaMode)) ||
                (Field(value, "materialClass") && !StringField(value, "materialClass", materialClass)) ||
                (Field(value, "doubleSided") && !BooleanField(value, "doubleSided", doubleSided))) {
                SetError(error, PhotorealMaterialImportErrorCode3D::InvalidSchema, 0, "Material alpha type contract is invalid"); return false;
            }
            const JsonValue* blackKeyThreshold = Field(value, "alphaBlackKeyThreshold");
            const JsonValue* alphaCutoff = Field(value, "alphaCutoff");
            const JsonValue* dilationPixels = Field(value, "alphaDilationPixels");
            if ((alphaCutoff && (alphaCutoff->type != JsonType::Number || alphaCutoff->number != 0.5)) ||
                (materialClass != "surface" && materialClass != "foliage")) {
                SetError(error, PhotorealMaterialImportErrorCode3D::InvalidSchema, 0, "Masked foliage alpha semantics are incomplete or invalid"); return false;
            }
            if (alphaMode == "mask") {
                if (!alphaCutoff || !StringField(value, "alphaSource", alphaSource) ||
                    (alphaSource != "embedded" && alphaSource != "black-key") ||
                    !dilationPixels || dilationPixels->type != JsonType::Number || !dilationPixels->isUnsignedInteger || dilationPixels->unsignedInteger > 32 ||
                    (alphaSource == "embedded" && blackKeyThreshold) ||
                    (alphaSource == "black-key" && (!blackKeyThreshold || blackKeyThreshold->type != JsonType::Number || blackKeyThreshold->number <= 0.0 || blackKeyThreshold->number > 0.25))) {
                    SetError(error, PhotorealMaterialImportErrorCode3D::InvalidSchema, 0, "MASK alpha source, cutoff, threshold, or dilation contract is invalid"); return false;
                }
            } else if (alphaMode != "opaque" || Field(value, "alphaSource") || blackKeyThreshold || dilationPixels) {
                SetError(error, PhotorealMaterialImportErrorCode3D::InvalidSchema, 0, "Opaque material contains MASK-only alpha semantics"); return false;
            }
            if (materialClass == "foliage" && (alphaMode != "mask" || !doubleSided)) {
                SetError(error, PhotorealMaterialImportErrorCode3D::InvalidSchema, 0, "Foliage must be a double-sided MASK material"); return false;
            }
        }
        if (separate) arm = ambientOcclusion;
        const auto armOperation = separate ? PhotorealTextureBakeOperation3D::PackAmbientOcclusionRoughnessMetallicScalar : PhotorealTextureBakeOperation3D::PreservePackedArm;
        const std::array<std::pair<std::string*, PhotorealTextureBakeOperation3D>, 3> sources{{{&base, PhotorealTextureBakeOperation3D::BaseColorSrgb}, {&normal, PhotorealTextureBakeOperation3D::InvertNormalGreenToDirectX}, {&arm, armOperation}}};
        std::array<std::string, 3> textureIds; std::uint32_t materialMaximum{};
        for (std::size_t channel = 0; channel < sources.size(); ++channel) {
            std::vector<std::byte> bytes; std::string hash; if (!ReadVerified(sidecarPath.parent_path(), *sources[channel].first, limits.maximumSourceBytes - temporary.verifiedSourceBytes, *runtime, bytes, hash, error)) return false;
            if (temporary.verifiedSourceBytes > limits.maximumSourceBytes - bytes.size()) return false; temporary.verifiedSourceBytes += bytes.size();
            std::uint32_t width{}, height{}; if (!JpegSize(bytes, width, height) || width > limits.maximumTextureDimension || height > limits.maximumTextureDimension) { SetError(error, PhotorealMaterialImportErrorCode3D::UnsupportedImage, 0, "Source texture is not a supported bounded JPEG"); return false; }
            if (channel && (width != temporary.textureBakes[temporary.textureBakes.size() - channel].width || height != temporary.textureBakes[temporary.textureBakes.size() - channel].height)) { SetError(error, PhotorealMaterialImportErrorCode3D::ChannelMismatch, 0, "Material channel dimensions do not match"); return false; }
            const std::string stem = std::string(assetId) + "-" + Lower(name) + (channel == 0 ? "-base" : channel == 1 ? "-normal" : "-arm"); textureIds[channel] = stem;
            const TextureUsage usage = channel == 0 ? TextureUsage::BaseColor : channel == 1 ? TextureUsage::Normal : TextureUsage::PackedOrm;
            const std::string outputPath = "cooked/materials/" + std::string(assetId) + "/" + Lower(name) + (channel == 0 ? "-base.dds" : channel == 1 ? "-normal.dds" : "-arm.dds");
            const std::string sourceUri = runtime->source == "poly-haven" ? "https://polyhaven.com/a/" + std::string(assetId) : "https://ambientcg.com/view?id=" + std::string(assetId);
            AssetManifestEntry entry; entry.id = stem; entry.path = outputPath; entry.source = {sourceUri, hash}; entry.texture = {usage, Tier((std::max)(width, height)), width, height, true}; temporary.manifest.entries.push_back(entry);
            temporary.textureBakes.push_back({*sources[channel].first, hash, {}, {}, outputPath, stem, usage, sources[channel].second, width, height, channel == 0 ? "BC7_UNORM_SRGB" : channel == 1 ? "BC5_UNORM" : "BC7_UNORM", 0.f, true}); materialMaximum = (std::max)(materialMaximum, (std::max)(width, height));
        }
        if (separate) {
            std::vector<std::byte> roughnessBytes; std::string roughnessHash;
            if (!ReadVerified(sidecarPath.parent_path(), roughness, limits.maximumSourceBytes - temporary.verifiedSourceBytes, *runtime, roughnessBytes, roughnessHash, error)) return false;
            if (temporary.verifiedSourceBytes > limits.maximumSourceBytes - roughnessBytes.size()) return false;
            temporary.verifiedSourceBytes += roughnessBytes.size();
            std::uint32_t width{}, height{};
            if (!JpegSize(roughnessBytes, width, height) || width != temporary.textureBakes.back().width || height != temporary.textureBakes.back().height) {
                SetError(error, PhotorealMaterialImportErrorCode3D::ChannelMismatch, 0, "Separate roughness dimensions do not match AO"); return false;
            }
            temporary.textureBakes.back().secondarySourcePath = roughness;
            temporary.textureBakes.back().secondarySourceSha256 = roughnessHash;
            temporary.textureBakes.back().metallicScalar = static_cast<float>(metallic->number);
        }
        const std::string materialSourceUri = runtime->source == "poly-haven" ? "https://polyhaven.com/a/" + std::string(assetId) : "https://ambientcg.com/view?id=" + std::string(assetId);
        AssetManifestEntry material; material.id = std::string(assetId) + "-" + Lower(name) + "-material"; material.path = "cooked/materials/" + std::string(assetId) + "/" + Lower(name) + ".material"; material.kind = AssetKind::Material; material.source = {materialSourceUri, sidecarHash}; material.material.layers[0] = {textureIds[0], textureIds[2], textureIds[1]}; material.material.resolutionTier = Tier(materialMaximum); material.material.textureScale = 1.f; material.material.useUv = true; temporary.manifest.entries.push_back(std::move(material));
    }
    AssetManifest normalized; AssetManifestError manifestError; if (!AssetManifestValidator::ValidateAndNormalize(temporary.manifest, normalized, &manifestError)) { SetError(error, PhotorealMaterialImportErrorCode3D::ManifestRejected, manifestError.entryIndex, manifestError.message); return false; }
    temporary.manifest = std::move(normalized); PbrMaterialCookError3D cookError; PbrMaterialCookOptions3D options; options.quality = MaterialQualityTier3D::Ultra; options.maximumResidentBytes = limits.maximumResidentBytes; options.maximumMaterials = limits.maximumMaterials;
    if (!PbrMaterialCooker3D::Cook(temporary.manifest, temporary.cookPlan, &cookError, options)) { SetError(error, PhotorealMaterialImportErrorCode3D::CookRejected, cookError.entryIndex, cookError.message); return false; }
    output = std::move(temporary); return true;
}

} // namespace Aevrynd
