#include <Aevrynd/CookedRenderManifest3D.h>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <bcrypt.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <charconv>
#include <cmath>
#include <fstream>
#include <limits>
#include <memory>
#include <ranges>
#include <set>
#include <unordered_set>
#include <utility>

namespace Aevrynd {
namespace {

enum class JsonType : std::uint8_t { Null, Boolean, Number, String, Array, Object };
struct JsonValue {
    JsonType type{JsonType::Null};
    bool boolean{};
    std::uint64_t number{};
    double floatingNumber{};
    bool unsignedInteger{};
    std::string string;
    std::vector<JsonValue> array;
    std::vector<std::pair<std::string, JsonValue>> object;
};

void SetError(CookedRenderManifestError3D *error,
              CookedRenderManifestErrorCode3D code, std::size_t offset,
              std::string message) {
    if (error)
        *error = {code, offset, std::move(message)};
}

class JsonParser {
public:
    JsonParser(std::string_view text, std::size_t maximumStringBytes)
        : text_(text), maximumStringBytes_(maximumStringBytes) {}

    bool Parse(JsonValue &output, CookedRenderManifestError3D *error) {
        if (!Value(output, 0, error))
            return false;
        Space();
        if (position_ != text_.size()) {
            SetError(error, CookedRenderManifestErrorCode3D::InvalidJson,
                     position_, "Trailing JSON data");
            return false;
        }
        return true;
    }

private:
    void Space() {
        while (position_ < text_.size() &&
               (text_[position_] == ' ' || text_[position_] == '\t' ||
                text_[position_] == '\r' || text_[position_] == '\n'))
            ++position_;
    }
    bool String(std::string &output, CookedRenderManifestError3D *error) {
        if (position_ >= text_.size() || text_[position_++] != '"')
            return false;
        while (position_ < text_.size()) {
            const unsigned char value =
                static_cast<unsigned char>(text_[position_++]);
            if (value == '"')
                return true;
            if (value < 0x20 || value >= 0x80) {
                SetError(error, CookedRenderManifestErrorCode3D::InvalidJson,
                         position_ - 1,
                         "Manifest strings must be printable ASCII");
                return false;
            }
            if (value != '\\') {
                if (output.size() >= maximumStringBytes_) {
                    SetError(error,
                             CookedRenderManifestErrorCode3D::InputLimitExceeded,
                             position_ - 1, "Manifest string exceeds limit");
                    return false;
                }
                output.push_back(static_cast<char>(value));
                continue;
            }
            if (position_ >= text_.size())
                return false;
            const char escaped = text_[position_++];
            if (escaped == '"' || escaped == '\\' || escaped == '/')
                output.push_back(escaped);
            else {
                SetError(error, CookedRenderManifestErrorCode3D::InvalidJson,
                         position_ - 1, "Unsupported JSON escape");
                return false;
            }
        }
        SetError(error, CookedRenderManifestErrorCode3D::InvalidJson, position_,
                 "Unterminated JSON string");
        return false;
    }
    bool Value(JsonValue &output, unsigned depth,
               CookedRenderManifestError3D *error) {
        if (depth > 16) {
            SetError(error,
                     CookedRenderManifestErrorCode3D::InputLimitExceeded,
                     position_, "JSON nesting exceeds limit");
            return false;
        }
        Space();
        if (position_ >= text_.size())
            return false;
        if (text_[position_] == '"') {
            output.type = JsonType::String;
            return String(output.string, error);
        }
        if (text_.substr(position_, 4) == "true") {
            position_ += 4;
            output.type = JsonType::Boolean;
            output.boolean = true;
            return true;
        }
        if (text_.substr(position_, 5) == "false") {
            position_ += 5;
            output.type = JsonType::Boolean;
            return true;
        }
        if (text_.substr(position_, 4) == "null") {
            position_ += 4;
            return true;
        }
        if (text_[position_] == '-' ||
            (text_[position_] >= '0' && text_[position_] <= '9')) {
            const std::size_t start = position_;
            const bool negative = text_[position_] == '-';
            if (negative)
                ++position_;
            if (position_ >= text_.size() || text_[position_] < '0' ||
                text_[position_] > '9')
                return false;
            if (text_[position_] == '0')
                ++position_;
            else
                while (position_ < text_.size() && text_[position_] >= '0' &&
                       text_[position_] <= '9')
                    ++position_;
            bool fractional{};
            if (position_ < text_.size() && text_[position_] == '.') {
                fractional = true;
                ++position_;
                const std::size_t fractionStart = position_;
                while (position_ < text_.size() && text_[position_] >= '0' &&
                       text_[position_] <= '9')
                    ++position_;
                if (fractionStart == position_)
                    return false;
            }
            bool exponent{};
            if (position_ < text_.size() &&
                (text_[position_] == 'e' || text_[position_] == 'E')) {
                exponent = true;
                ++position_;
                if (position_ < text_.size() &&
                    (text_[position_] == '+' || text_[position_] == '-'))
                    ++position_;
                const std::size_t exponentStart = position_;
                while (position_ < text_.size() && text_[position_] >= '0' &&
                       text_[position_] <= '9')
                    ++position_;
                if (exponentStart == position_)
                    return false;
            }
            output.type = JsonType::Number;
            const auto floatingConverted = std::from_chars(
                text_.data() + start, text_.data() + position_,
                output.floatingNumber, std::chars_format::general);
            if (floatingConverted.ec != std::errc{} ||
                floatingConverted.ptr != text_.data() + position_ ||
                !std::isfinite(output.floatingNumber))
                return false;
            if (!negative && !fractional && !exponent) {
                const auto converted = std::from_chars(
                    text_.data() + start, text_.data() + position_,
                    output.number);
                if (converted.ec != std::errc{} ||
                    converted.ptr != text_.data() + position_)
                    return false;
                output.unsignedInteger = true;
            }
            return true;
        }
        if (text_[position_] == '[') {
            ++position_;
            output.type = JsonType::Array;
            Space();
            if (position_ < text_.size() && text_[position_] == ']') {
                ++position_;
                return true;
            }
            for (;;) {
                output.array.emplace_back();
                if (!Value(output.array.back(), depth + 1, error))
                    return false;
                Space();
                if (position_ >= text_.size())
                    return false;
                const char separator = text_[position_++];
                if (separator == ']')
                    return true;
                if (separator != ',')
                    return false;
            }
        }
        if (text_[position_] == '{') {
            ++position_;
            output.type = JsonType::Object;
            Space();
            std::unordered_set<std::string> keys;
            if (position_ < text_.size() && text_[position_] == '}') {
                ++position_;
                return true;
            }
            for (;;) {
                Space();
                std::string key;
                if (!String(key, error) || !keys.insert(key).second) {
                    SetError(error,
                             CookedRenderManifestErrorCode3D::DuplicateEntry,
                             position_, "Duplicate or invalid JSON object key");
                    return false;
                }
                Space();
                if (position_ >= text_.size() || text_[position_++] != ':')
                    return false;
                output.object.emplace_back(std::move(key), JsonValue{});
                if (!Value(output.object.back().second, depth + 1, error))
                    return false;
                Space();
                if (position_ >= text_.size())
                    return false;
                const char separator = text_[position_++];
                if (separator == '}')
                    return true;
                if (separator != ',')
                    return false;
            }
        }
        SetError(error, CookedRenderManifestErrorCode3D::InvalidJson,
                 position_, "Invalid JSON token");
        return false;
    }

    std::string_view text_;
    std::size_t maximumStringBytes_{};
    std::size_t position_{};
};

const JsonValue *Field(const JsonValue &object, std::string_view name) {
    if (object.type != JsonType::Object)
        return nullptr;
    const auto found = std::ranges::find_if(
        object.object, [&](const auto &item) { return item.first == name; });
    return found == object.object.end() ? nullptr : &found->second;
}

bool StringField(const JsonValue &object, std::string_view name,
                 std::string &output) {
    const JsonValue *value = Field(object, name);
    if (!value || value->type != JsonType::String)
        return false;
    output = value->string;
    return true;
}

bool NumberField(const JsonValue &object, std::string_view name,
                 std::uint64_t &output) {
    const JsonValue *value = Field(object, name);
    if (!value || value->type != JsonType::Number || !value->unsignedInteger)
        return false;
    output = value->number;
    return true;
}

bool ValidIdentifier(std::string_view value) {
    return !value.empty() && value.size() <= 128 &&
           std::ranges::all_of(value, [](unsigned char item) {
               return std::isalnum(item) || item == '-' || item == '_' ||
                      item == '.' || item == ':';
           });
}

bool NormalizePath(std::string_view input, std::string &output) {
    if (input.empty() || input.size() > 4096 || input.front() == '/' ||
        input.find('\\') != input.npos || input.find(':') != input.npos)
        return false;
    // 呼叫端允許輸入與輸出指向同一字串；先保留來源以避免指派後 string_view 失效。
    const std::string original{input};
    std::filesystem::path path{original};
    for (const auto &component : path)
        if (component == "." || component == ".." || component.empty())
            return false;
    output = path.generic_string();
    return !output.empty() && output == original;
}

bool NormalizeSha(std::string &value) {
    if (value.size() != 64)
        return false;
    for (char &item : value) {
        if (item >= 'A' && item <= 'F')
            item = static_cast<char>(item - 'A' + 'a');
        if (!((item >= '0' && item <= '9') ||
              (item >= 'a' && item <= 'f')))
            return false;
    }
    return true;
}

struct HandleCloser {
    void operator()(void *handle) const noexcept {
        if (handle && handle != INVALID_HANDLE_VALUE)
            CloseHandle(handle);
    }
};
using UniqueHandle = std::unique_ptr<void, HandleCloser>;

UniqueHandle OpenPinned(const std::filesystem::path &path, bool directory) {
    const DWORD flags = FILE_FLAG_OPEN_REPARSE_POINT |
                        (directory ? FILE_FLAG_BACKUP_SEMANTICS : 0);
    const DWORD sharing = directory
                              ? FILE_SHARE_READ | FILE_SHARE_WRITE |
                                    FILE_SHARE_DELETE
                              : FILE_SHARE_READ;
    const HANDLE handle = CreateFileW(path.c_str(), GENERIC_READ,
                                      sharing, nullptr, OPEN_EXISTING,
                                      flags, nullptr);
    return handle == INVALID_HANDLE_VALUE ? UniqueHandle{} : UniqueHandle{handle};
}

bool IsPlainHandle(HANDLE handle, bool directory) {
    FILE_ATTRIBUTE_TAG_INFO info{};
    if (!GetFileInformationByHandleEx(handle, FileAttributeTagInfo, &info,
                                      sizeof(info)) ||
        (info.FileAttributes & FILE_ATTRIBUTE_REPARSE_POINT))
        return false;
    return directory ==
           bool(info.FileAttributes & FILE_ATTRIBUTE_DIRECTORY);
}

bool FinalPath(HANDLE handle, std::wstring &output) {
    const DWORD needed = GetFinalPathNameByHandleW(handle, nullptr, 0,
                                                    FILE_NAME_NORMALIZED);
    if (!needed)
        return false;
    output.resize(needed);
    const DWORD written = GetFinalPathNameByHandleW(
        handle, output.data(), needed, FILE_NAME_NORMALIZED);
    if (!written || written >= needed)
        return false;
    output.resize(written);
    if (output.starts_with(L"\\\\?\\"))
        output.erase(0, 4);
    return true;
}

bool IsWithinFinalPath(std::wstring_view root, std::wstring_view candidate) {
    if (candidate.size() <= root.size() ||
        CompareStringOrdinal(root.data(), static_cast<int>(root.size()),
                             candidate.data(), static_cast<int>(root.size()),
                             TRUE) != CSTR_EQUAL)
        return false;
    return candidate[root.size()] == L'\\' || candidate[root.size()] == L'/';
}

bool HashHandle(HANDLE file, std::string &output) {
    BCRYPT_ALG_HANDLE algorithm{};
    BCRYPT_HASH_HANDLE hash{};
    DWORD objectBytes{}, copied{};
    std::vector<UCHAR> object;
    std::array<UCHAR, 32> digest{};
    if (BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, nullptr,
                                    0) < 0)
        return false;
    bool passed = BCryptGetProperty(
                      algorithm, BCRYPT_OBJECT_LENGTH,
                      reinterpret_cast<PUCHAR>(&objectBytes), sizeof(objectBytes),
                      &copied, 0) >= 0;
    if (passed)
        object.resize(objectBytes);
    passed = passed && BCryptCreateHash(algorithm, &hash, object.data(),
                                        objectBytes, nullptr, 0, 0) >= 0;
    // 大檔串流雜湊緩衝放 heap，避免測試/工具預設 1 MiB stack 被吃滿。
    std::vector<UCHAR> buffer(1024 * 1024);
    LARGE_INTEGER beginning{};
    passed = passed && SetFilePointerEx(file, beginning, nullptr, FILE_BEGIN);
    for (;;) {
        DWORD count{};
        if (!passed || !ReadFile(file, buffer.data(),
                                 static_cast<DWORD>(buffer.size()), &count,
                                 nullptr)) {
            passed = false;
            break;
        }
        if (!count)
            break;
        if (passed)
            passed = BCryptHashData(
                         hash, buffer.data(), count, 0) >= 0;
    }
    passed = passed && BCryptFinishHash(hash, digest.data(),
                              static_cast<ULONG>(digest.size()), 0) >= 0;
    if (hash)
        BCryptDestroyHash(hash);
    BCryptCloseAlgorithmProvider(algorithm, 0);
    if (!passed)
        return false;
    constexpr char Hex[] = "0123456789abcdef";
    output.resize(64);
    for (std::size_t index = 0; index < digest.size(); ++index) {
        output[index * 2] = Hex[digest[index] >> 4];
        output[index * 2 + 1] = Hex[digest[index] & 15];
    }
    return true;
}

bool HashMemorySha256(std::string_view bytes, std::string &output) {
    BCRYPT_ALG_HANDLE algorithm{};
    BCRYPT_HASH_HANDLE hash{};
    DWORD objectBytes{}, copied{};
    std::vector<UCHAR> object;
    std::array<UCHAR, 32> digest{};
    if (bytes.size() > ULONG_MAX ||
        BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM,
                                    nullptr, 0) < 0)
        return false;
    bool passed = BCryptGetProperty(
                      algorithm, BCRYPT_OBJECT_LENGTH,
                      reinterpret_cast<PUCHAR>(&objectBytes),
                      sizeof(objectBytes), &copied, 0) >= 0;
    if (passed)
        object.resize(objectBytes);
    passed = passed && BCryptCreateHash(algorithm, &hash, object.data(),
                                        objectBytes, nullptr, 0, 0) >= 0;
    passed = passed && BCryptHashData(
                           hash,
                           const_cast<PUCHAR>(
                               reinterpret_cast<const UCHAR *>(bytes.data())),
                           static_cast<ULONG>(bytes.size()), 0) >= 0;
    passed = passed && BCryptFinishHash(hash, digest.data(),
                                        static_cast<ULONG>(digest.size()),
                                        0) >= 0;
    if (hash)
        BCryptDestroyHash(hash);
    BCryptCloseAlgorithmProvider(algorithm, 0);
    if (!passed)
        return false;
    constexpr char Hex[] = "0123456789abcdef";
    output.resize(64);
    for (std::size_t index = 0; index < digest.size(); ++index) {
        output[index * 2] = Hex[digest[index] >> 4];
        output[index * 2 + 1] = Hex[digest[index] & 15];
    }
    return true;
}

bool VerifyFile(const std::filesystem::path &root,
                const CookedRenderFile3D &file,
                const CookedRenderManifestLimits3D &limits,
                CookedRenderManifestError3D *error,
                std::shared_ptr<void> &pin) {
    std::error_code ec;
    const auto absoluteRoot = std::filesystem::absolute(root, ec).lexically_normal();
    if (ec || GetFileAttributesW(absoluteRoot.c_str()) == INVALID_FILE_ATTRIBUTES ||
        (GetFileAttributesW(absoluteRoot.c_str()) & FILE_ATTRIBUTE_REPARSE_POINT)) {
        SetError(error, CookedRenderManifestErrorCode3D::InvalidPath, 0,
                 "Asset root is missing or is a reparse point");
        return false;
    }
    auto lexical = absoluteRoot;
    for (const auto &component : std::filesystem::path(file.path)) {
        lexical /= component;
        const DWORD attributes = GetFileAttributesW(lexical.c_str());
        if (attributes == INVALID_FILE_ATTRIBUTES) {
            SetError(error, CookedRenderManifestErrorCode3D::FileMissing, 0,
                     "Cooked render file is missing: " + file.path);
            return false;
        }
        if (attributes & FILE_ATTRIBUTE_REPARSE_POINT) {
            SetError(error, CookedRenderManifestErrorCode3D::InvalidPath, 0,
                     "Cooked render path contains a reparse point");
            return false;
        }
    }
    auto rootHandle = OpenPinned(absoluteRoot, true);
    auto fileHandle = OpenPinned(lexical, false);
    std::wstring finalRoot, finalFile;
    if (!rootHandle || !fileHandle ||
        !IsPlainHandle(rootHandle.get(), true) ||
        !IsPlainHandle(fileHandle.get(), false) ||
        !FinalPath(rootHandle.get(), finalRoot) ||
        !FinalPath(fileHandle.get(), finalFile) ||
        !IsWithinFinalPath(finalRoot, finalFile)) {
        SetError(error, CookedRenderManifestErrorCode3D::InvalidPath, 0,
                 "Cooked render path escaped the asset root");
        return false;
    }
    LARGE_INTEGER bytes{};
    if (!GetFileSizeEx(fileHandle.get(), &bytes) || bytes.QuadPart <= 0 ||
        static_cast<std::uint64_t>(bytes.QuadPart) != file.bytes ||
        static_cast<std::uint64_t>(bytes.QuadPart) > limits.maximumFileBytes) {
        SetError(error, CookedRenderManifestErrorCode3D::FileSizeMismatch, 0,
                 "Cooked render file size mismatch: " + file.path);
        return false;
    }
    std::string actual;
    if (!HashHandle(fileHandle.get(), actual)) {
        SetError(error, CookedRenderManifestErrorCode3D::IoFailure, 0,
                 "Unable to hash cooked render file");
        return false;
    }
    if (actual != file.sha256) {
        SetError(error, CookedRenderManifestErrorCode3D::FileHashMismatch, 0,
                 "Cooked render file hash mismatch: " + file.path);
        return false;
    }
    try {
        pin = std::shared_ptr<void>{fileHandle.release(), HandleCloser{}};
    } catch (...) {
        SetError(error, CookedRenderManifestErrorCode3D::IoFailure, 0,
                 "Unable to retain cooked render file lease");
        return false;
    }
    return true;
}

void HashBytes(std::uint64_t &hash, const void *data, std::size_t bytes) {
    const auto *values = static_cast<const unsigned char *>(data);
    for (std::size_t index = 0; index < bytes; ++index) {
        hash ^= values[index];
        hash *= 1099511628211ull;
    }
}

void HashString(std::uint64_t &hash, std::string_view value) {
    HashBytes(hash, value.data(), value.size());
    const unsigned char separator = 0xff;
    HashBytes(hash, &separator, 1);
}

} // namespace

MaterialAssetDefinition3D CookedRenderMaterial3D::Definition(
    std::uint32_t maximumResolution) const {
    MaterialAssetDefinition3D result{};
    result.name = id;
    result.textures.baseColorPath = baseColorPath;
    result.textures.roughnessPath = surfacePath;
    result.textures.normalPath = normalPath;
    result.textures.maximumResolution = maximumResolution;
    result.textures.surfaceEncoding = surfaceEncoding;
    result.textures.alphaMode = alphaMode;
    result.textures.alphaCutoff = alphaCutoff;
    result.textures.integrity[0] = {baseColorBytes, baseColorSha256};
    result.textures.integrity[1] = {surfaceBytes, surfaceSha256};
    result.textures.integrity[2] = {normalBytes, normalSha256};
    result.material.useUv = true;
    result.material.textureScale = 1.f;
    result.material.alphaMode = alphaMode;
    result.material.alphaCutoff = alphaCutoff;
    result.material.doubleSided = doubleSided;
    result.material.materialClass = materialClass;
    return result;
}

bool CookedRenderManifest3D::LoadFile(
    const std::filesystem::path &manifestPath,
    CookedRenderManifestError3D *error,
    const CookedRenderManifestLimits3D &limits) {
    if (error)
        *error = {};
    const auto parent = manifestPath.has_parent_path()
                            ? manifestPath.parent_path()
                            : std::filesystem::current_path();
    auto rootHandle = OpenPinned(parent, true);
    auto fileHandle = OpenPinned(manifestPath, false);
    std::wstring finalRoot, finalFile;
    LARGE_INTEGER bytes{};
    if (!rootHandle || !fileHandle ||
        !IsPlainHandle(rootHandle.get(), true) ||
        !IsPlainHandle(fileHandle.get(), false) ||
        !FinalPath(rootHandle.get(), finalRoot) ||
        !FinalPath(fileHandle.get(), finalFile) ||
        !IsWithinFinalPath(finalRoot, finalFile) ||
        !GetFileSizeEx(fileHandle.get(), &bytes) || bytes.QuadPart <= 0 ||
        static_cast<std::uint64_t>(bytes.QuadPart) > limits.maximumJsonBytes ||
        static_cast<std::uint64_t>(bytes.QuadPart) > SIZE_MAX) {
        SetError(error, CookedRenderManifestErrorCode3D::InputLimitExceeded, 0,
                 "Runtime render manifest size is invalid");
        return false;
    }
    std::string json(static_cast<std::size_t>(bytes.QuadPart), '\0');
    DWORD read{};
    if (!ReadFile(fileHandle.get(), json.data(), static_cast<DWORD>(json.size()),
                  &read, nullptr) || read != static_cast<DWORD>(json.size())) {
        SetError(error, CookedRenderManifestErrorCode3D::IoFailure, 0,
                 "Unable to read runtime render manifest");
        return false;
    }
    std::string manifestSha256;
    if (!HashMemorySha256(json, manifestSha256)) {
        SetError(error, CookedRenderManifestErrorCode3D::IoFailure, 0,
                 "Unable to hash runtime render manifest bytes");
        return false;
    }
    if (!LoadJson(json, parent, error, limits))
        return false;
    stats_.manifestSha256 = std::move(manifestSha256);
    stats_.validatedFiles = files_.size();
    stats_.validatedMaterials = materials_.size();
    stats_.validatedMeshParts = meshParts_.size();
    stats_.validatedReferencedFiles = stats_.referencedFiles;
    return true;
}

bool CookedRenderManifest3D::LoadJson(
    std::string_view json, const std::filesystem::path &assetRoot,
    CookedRenderManifestError3D *error,
    const CookedRenderManifestLimits3D &limits) {
    if (error)
        *error = {};
    if (json.empty() || json.size() > limits.maximumJsonBytes ||
        !limits.maximumFiles || !limits.maximumMaterials ||
        !limits.maximumMeshParts || !limits.maximumStringBytes ||
        !limits.maximumFileBytes || !limits.maximumAggregateBytes) {
        SetError(error, CookedRenderManifestErrorCode3D::InputLimitExceeded, 0,
                 "Runtime render manifest limits are invalid");
        return false;
    }
    JsonValue root;
    if (!JsonParser(json, limits.maximumStringBytes).Parse(root, error) ||
        root.type != JsonType::Object) {
        if (error && !*error)
            SetError(error, CookedRenderManifestErrorCode3D::InvalidJson, 0,
                     "Runtime render manifest is not valid JSON");
        return false;
    }
    const JsonValue *schema = Field(root, "schemaVersion");
    const JsonValue *files = Field(root, "files");
    const JsonValue *materials = Field(root, "materials");
    const JsonValue *meshParts = Field(root, "meshParts");
    const JsonValue *ibl = Field(root, "ibl");
    if (!schema || schema->type != JsonType::Number ||
        !schema->unsignedInteger || schema->number != 1 ||
        !files || files->type != JsonType::Array || files->array.empty() ||
        files->array.size() > limits.maximumFiles || !materials ||
        materials->type != JsonType::Array || materials->array.empty() ||
        materials->array.size() > limits.maximumMaterials || !meshParts ||
        meshParts->type != JsonType::Array || meshParts->array.empty() ||
        meshParts->array.size() > limits.maximumMeshParts || !ibl ||
        ibl->type != JsonType::Object) {
        SetError(error, CookedRenderManifestErrorCode3D::InvalidSchema, 0,
                 "Runtime render manifest core schema is invalid");
        return false;
    }

    CookedRenderManifest3D candidate;
    std::error_code rootError;
    candidate.assetRoot_ =
        std::filesystem::absolute(assetRoot, rootError).lexically_normal();
    if (rootError) {
        SetError(error, CookedRenderManifestErrorCode3D::InvalidPath, 0,
                 "Unable to resolve the cooked render asset root");
        return false;
    }
    std::set<std::string, std::less<>> filePaths, materialIds, meshPartIds;
    std::uint64_t aggregate{};
    for (const JsonValue &value : files->array) {
        CookedRenderFile3D item;
        if (value.type != JsonType::Object ||
            !StringField(value, "path", item.path) ||
            !StringField(value, "sha256", item.sha256) ||
            !NumberField(value, "bytes", item.bytes) || !item.bytes ||
            item.bytes > limits.maximumFileBytes) {
            SetError(error, CookedRenderManifestErrorCode3D::InvalidSchema, 0,
                     "Cooked render file entry is invalid");
            return false;
        }
        if (!NormalizePath(item.path, item.path)) {
            SetError(error, CookedRenderManifestErrorCode3D::InvalidPath, 0,
                     "Cooked render file path is invalid");
            return false;
        }
        if (!NormalizeSha(item.sha256)) {
            SetError(error, CookedRenderManifestErrorCode3D::InvalidSha256, 0,
                     "Cooked render file SHA-256 is invalid");
            return false;
        }
        if (!filePaths.insert(item.path).second) {
            SetError(error, CookedRenderManifestErrorCode3D::DuplicateEntry, 0,
                     "Duplicate cooked render file path");
            return false;
        }
        if (aggregate > limits.maximumAggregateBytes - item.bytes) {
            SetError(error,
                     CookedRenderManifestErrorCode3D::InputLimitExceeded, 0,
                     "Cooked render files exceed aggregate limit");
            return false;
        }
        aggregate += item.bytes;
        candidate.files_.push_back(std::move(item));
    }

    std::set<std::string, std::less<>> referenced;
    const auto addReference = [&](const std::string &path) {
        if (referenced.insert(path).second)
            return true;
        SetError(error, CookedRenderManifestErrorCode3D::DuplicateEntry, 0,
                 "Cooked render payload is referenced more than once: " + path);
        return false;
    };
    for (const JsonValue &value : materials->array) {
        CookedRenderMaterial3D item;
        std::string encoding;
        std::string alphaMode = "opaque";
        std::string materialClass = "surface";
        if (value.type != JsonType::Object || !StringField(value, "id", item.id) ||
            !StringField(value, "assetId", item.assetId) ||
            !StringField(value, "slot", item.slot) ||
            !StringField(value, "baseColorPath", item.baseColorPath) ||
            !StringField(value, "surfacePath", item.surfacePath) ||
            !StringField(value, "normalPath", item.normalPath) ||
            !StringField(value, "surfaceEncoding", encoding) ||
            !ValidIdentifier(item.id) || !ValidIdentifier(item.assetId) ||
            !ValidIdentifier(item.slot) ||
            item.id != item.assetId + ":" + item.slot ||
            !NormalizePath(item.baseColorPath, item.baseColorPath) ||
            !NormalizePath(item.surfacePath, item.surfacePath) ||
            !NormalizePath(item.normalPath, item.normalPath) ||
            (encoding != "packed-orm" && encoding != "roughness-r")) {
            SetError(error, CookedRenderManifestErrorCode3D::InvalidSchema, 0,
                     "Cooked render material entry is invalid");
            return false;
        }
        if (const JsonValue *alpha = Field(value, "alphaMode")) {
            if (alpha->type != JsonType::String ||
                (alpha->string != "opaque" && alpha->string != "mask")) {
                SetError(error, CookedRenderManifestErrorCode3D::InvalidSchema,
                         0, "Cooked render material alpha mode is invalid");
                return false;
            }
            alphaMode = alpha->string;
        }
        if (const JsonValue *cutoff = Field(value, "alphaCutoff")) {
            if (cutoff->type != JsonType::Number ||
                !std::isfinite(cutoff->floatingNumber) ||
                cutoff->floatingNumber < 0.0 || cutoff->floatingNumber > 1.0) {
                SetError(error, CookedRenderManifestErrorCode3D::InvalidSchema,
                         0, "Cooked render material alpha cutoff is invalid");
                return false;
            }
            item.alphaCutoff = static_cast<float>(cutoff->floatingNumber);
        }
        if (const JsonValue *sided = Field(value, "doubleSided")) {
            if (sided->type != JsonType::Boolean) {
                SetError(error, CookedRenderManifestErrorCode3D::InvalidSchema,
                         0, "Cooked render material double-sided flag is invalid");
                return false;
            }
            item.doubleSided = sided->boolean;
        }
        if (const JsonValue *category = Field(value, "materialClass")) {
            if (category->type != JsonType::String ||
                (category->string != "surface" &&
                 category->string != "foliage")) {
                SetError(error, CookedRenderManifestErrorCode3D::InvalidSchema,
                         0, "Cooked render material class is invalid");
                return false;
            }
            materialClass = category->string;
        }
        if (!materialIds.insert(item.id).second) {
            SetError(error, CookedRenderManifestErrorCode3D::DuplicateEntry, 0,
                     "Duplicate cooked render material id");
            return false;
        }
        item.surfaceEncoding = encoding == "packed-orm"
                                   ? MaterialSurfaceMapEncoding3D::PackedOrm
                                   : MaterialSurfaceMapEncoding3D::RoughnessR;
        item.alphaMode = alphaMode == "mask" ? MaterialAlphaMode3D::Mask
                                              : MaterialAlphaMode3D::Opaque;
        item.materialClass = materialClass == "foliage"
                                 ? MaterialClass3D::Foliage
                                 : MaterialClass3D::Surface;
        if (item.materialClass == MaterialClass3D::Foliage &&
            (item.alphaMode != MaterialAlphaMode3D::Mask ||
             !item.doubleSided)) {
            SetError(error, CookedRenderManifestErrorCode3D::InvalidSchema, 0,
                     "Foliage material must be masked and double-sided");
            return false;
        }
        if (!addReference(item.baseColorPath) ||
            !addReference(item.surfacePath) ||
            !addReference(item.normalPath))
            return false;
        candidate.materials_.push_back(std::move(item));
    }

    for (const JsonValue &value : meshParts->array) {
        CookedRenderMeshPart3D item;
        const JsonValue *lods = Field(value, "lodPaths");
        if (value.type != JsonType::Object || !StringField(value, "id", item.id) ||
            !StringField(value, "assetId", item.assetId) ||
            !StringField(value, "materialId", item.materialId) ||
            !ValidIdentifier(item.id) || !ValidIdentifier(item.assetId) ||
            !ValidIdentifier(item.materialId) || !lods ||
            lods->type != JsonType::Array || lods->array.size() != 3) {
            SetError(error, CookedRenderManifestErrorCode3D::InvalidSchema, 0,
                     "Cooked render mesh part entry is invalid");
            return false;
        }
        if (!meshPartIds.insert(item.id).second) {
            SetError(error, CookedRenderManifestErrorCode3D::DuplicateEntry, 0,
                     "Duplicate cooked render mesh part id");
            return false;
        }
        if (!materialIds.contains(item.materialId)) {
            SetError(error, CookedRenderManifestErrorCode3D::ReferenceMissing, 0,
                     "Mesh part references a missing material");
            return false;
        }
        for (std::size_t index = 0; index < item.lodPaths.size(); ++index) {
            if (lods->array[index].type != JsonType::String ||
                !NormalizePath(lods->array[index].string, item.lodPaths[index])) {
                SetError(error, CookedRenderManifestErrorCode3D::InvalidPath, 0,
                         "Mesh LOD path is invalid");
                return false;
            }
            if (!addReference(item.lodPaths[index]))
                return false;
        }
        candidate.meshParts_.push_back(std::move(item));
    }

    if (!StringField(*ibl, "specularPath", candidate.ibl_.specularPath) ||
        !StringField(*ibl, "irradiancePath", candidate.ibl_.irradiancePath) ||
        !StringField(*ibl, "brdfLutPath", candidate.ibl_.brdfLutPath) ||
        !NormalizePath(candidate.ibl_.specularPath,
                       candidate.ibl_.specularPath) ||
        !NormalizePath(candidate.ibl_.irradiancePath,
                       candidate.ibl_.irradiancePath) ||
        !NormalizePath(candidate.ibl_.brdfLutPath,
                       candidate.ibl_.brdfLutPath)) {
        SetError(error, CookedRenderManifestErrorCode3D::InvalidSchema, 0,
                 "Cooked render IBL entry is invalid");
        return false;
    }
    if (!addReference(candidate.ibl_.specularPath) ||
        !addReference(candidate.ibl_.irradiancePath) ||
        !addReference(candidate.ibl_.brdfLutPath))
        return false;

    for (const std::string &path : referenced)
        if (!filePaths.contains(path)) {
            SetError(error, CookedRenderManifestErrorCode3D::ReferenceMissing, 0,
                     "Cooked render reference is absent from files: " + path);
            return false;
        }
    if (referenced.size() != filePaths.size()) {
        SetError(error, CookedRenderManifestErrorCode3D::UnreferencedFile, 0,
                 "Runtime render manifest contains unreferenced payload files");
        return false;
    }
    const auto resolveFile = [&](std::string_view path)
        -> const CookedRenderFile3D * {
        const auto found = std::ranges::find(candidate.files_, path,
                                             &CookedRenderFile3D::path);
        return found == candidate.files_.end() ? nullptr : &*found;
    };
    for (auto &material : candidate.materials_) {
        const auto *base = resolveFile(material.baseColorPath);
        const auto *surface = resolveFile(material.surfacePath);
        const auto *normal = resolveFile(material.normalPath);
        if (!base || !surface || !normal) {
            SetError(error, CookedRenderManifestErrorCode3D::ReferenceMissing,
                     0, "Material file receipt resolution failed");
            return false;
        }
        material.baseColorSha256 = base->sha256;
        material.surfaceSha256 = surface->sha256;
        material.normalSha256 = normal->sha256;
        material.baseColorBytes = base->bytes;
        material.surfaceBytes = surface->bytes;
        material.normalBytes = normal->bytes;
    }
    try {
        candidate.pinnedFiles_.resize(candidate.files_.size());
    } catch (...) {
        SetError(error, CookedRenderManifestErrorCode3D::InputLimitExceeded, 0,
                 "Unable to allocate cooked render file leases");
        return false;
    }
    for (std::size_t index = 0; index < candidate.files_.size(); ++index)
        if (!VerifyFile(candidate.assetRoot_, candidate.files_[index], limits,
                        error, candidate.pinnedFiles_[index]))
            return false;

    std::ranges::sort(candidate.files_, {}, &CookedRenderFile3D::path);
    std::ranges::sort(candidate.materials_, {}, &CookedRenderMaterial3D::id);
    std::ranges::sort(candidate.meshParts_, {}, &CookedRenderMeshPart3D::id);
    std::uint64_t fingerprint = 1469598103934665603ull;
    for (const auto &file : candidate.files_) {
        HashString(fingerprint, file.path);
        HashString(fingerprint, file.sha256);
        HashBytes(fingerprint, &file.bytes, sizeof(file.bytes));
    }
    for (const auto &material : candidate.materials_) {
        HashString(fingerprint, material.id);
        HashString(fingerprint, material.assetId);
        HashString(fingerprint, material.slot);
        HashString(fingerprint, material.baseColorPath);
        HashString(fingerprint, material.surfacePath);
        HashString(fingerprint, material.normalPath);
        HashString(fingerprint, material.baseColorSha256);
        HashString(fingerprint, material.surfaceSha256);
        HashString(fingerprint, material.normalSha256);
        HashBytes(fingerprint, &material.baseColorBytes,
                  sizeof(material.baseColorBytes));
        HashBytes(fingerprint, &material.surfaceBytes,
                  sizeof(material.surfaceBytes));
        HashBytes(fingerprint, &material.normalBytes,
                  sizeof(material.normalBytes));
        HashBytes(fingerprint, &material.surfaceEncoding,
                  sizeof(material.surfaceEncoding));
        HashBytes(fingerprint, &material.alphaMode, sizeof(material.alphaMode));
        HashBytes(fingerprint, &material.alphaCutoff,
                  sizeof(material.alphaCutoff));
        HashBytes(fingerprint, &material.doubleSided,
                  sizeof(material.doubleSided));
        HashBytes(fingerprint, &material.materialClass,
                  sizeof(material.materialClass));
    }
    for (const auto &part : candidate.meshParts_) {
        HashString(fingerprint, part.id);
        HashString(fingerprint, part.assetId);
        HashString(fingerprint, part.materialId);
        for (const auto &path : part.lodPaths)
            HashString(fingerprint, path);
    }
    HashString(fingerprint, candidate.ibl_.specularPath);
    HashString(fingerprint, candidate.ibl_.irradiancePath);
    HashString(fingerprint, candidate.ibl_.brdfLutPath);
    candidate.stats_ = {candidate.files_.size(), candidate.materials_.size(),
                        candidate.meshParts_.size(), referenced.size(), aggregate,
                        fingerprint ? fingerprint : 1};
    *this = std::move(candidate);
    return true;
}

const CookedRenderFile3D *
CookedRenderManifest3D::FindFile(std::string_view path) const noexcept {
    const auto found = std::ranges::lower_bound(files_, path, {},
                                                &CookedRenderFile3D::path);
    return found != files_.end() && found->path == path ? &*found : nullptr;
}

const CookedRenderMaterial3D *
CookedRenderManifest3D::FindMaterial(std::string_view id) const noexcept {
    const auto found = std::ranges::lower_bound(materials_, id, {},
                                                &CookedRenderMaterial3D::id);
    return found != materials_.end() && found->id == id ? &*found : nullptr;
}

const CookedRenderMeshPart3D *
CookedRenderManifest3D::FindMeshPart(std::string_view id) const noexcept {
    const auto found = std::ranges::lower_bound(meshParts_, id, {},
                                                &CookedRenderMeshPart3D::id);
    return found != meshParts_.end() && found->id == id ? &*found : nullptr;
}

void CookedRenderManifest3D::Clear() noexcept {
    std::vector<std::shared_ptr<void>>{}.swap(pinnedFiles_);
    assetRoot_.clear();
    std::vector<CookedRenderFile3D>{}.swap(files_);
    std::vector<CookedRenderMaterial3D>{}.swap(materials_);
    std::vector<CookedRenderMeshPart3D>{}.swap(meshParts_);
    ibl_ = {};
    stats_ = {};
}

} // namespace Aevrynd
