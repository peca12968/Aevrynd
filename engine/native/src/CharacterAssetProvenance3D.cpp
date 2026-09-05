#include <Aevrynd/CharacterAssetProvenance3D.h>
#include <Aevrynd/SkinnedModelAsset.h>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <winternl.h>
#include <bcrypt.h>

#include <algorithm>
#include <array>
#include <charconv>
#include <cstring>
#include <cwchar>
#include <limits>
#include <set>
#include <string_view>
#include <tuple>
#include <unordered_set>
#include <utility>

namespace Aevrynd {
namespace {

enum class JsonKind3D : std::uint8_t { Null, Boolean, Number, String, Array, Object };
struct JsonValue3D {
    JsonKind3D kind{JsonKind3D::Null};
    bool boolean{};
    std::uint64_t number{};
    bool unsignedInteger{};
    std::string string;
    std::vector<JsonValue3D> array;
    std::vector<std::pair<std::string, JsonValue3D>> object;
};

void SetError(CharacterAssetProvenanceError3D* error,
              CharacterAssetProvenanceErrorCode3D code, std::size_t offset,
              std::string message) {
    if (error) *error = {code, offset, std::move(message)};
}

class JsonParser3D {
public:
    JsonParser3D(std::string_view text, std::size_t maximumStringBytes,
                 std::size_t maximumEntries)
        : text_(text), maximumStringBytes_(maximumStringBytes),
          maximumEntries_(maximumEntries) {}

    bool Parse(JsonValue3D& output, CharacterAssetProvenanceError3D* error) {
        if (!Value(output, 0u, error)) return false;
        Space();
        if (position_ != text_.size()) {
            SetError(error, CharacterAssetProvenanceErrorCode3D::InvalidJson,
                     position_, "Trailing JSON data");
            return false;
        }
        return true;
    }

private:
    void Space() {
        while (position_ < text_.size() &&
               (text_[position_] == ' ' || text_[position_] == '\t' ||
                text_[position_] == '\r' || text_[position_] == '\n')) ++position_;
    }
    static bool Hex(char value, std::uint32_t& digit) {
        if (value >= '0' && value <= '9') digit = static_cast<std::uint32_t>(value - '0');
        else if (value >= 'a' && value <= 'f') digit = static_cast<std::uint32_t>(value - 'a' + 10);
        else if (value >= 'A' && value <= 'F') digit = static_cast<std::uint32_t>(value - 'A' + 10);
        else return false;
        return true;
    }
    bool AppendUtf8(std::string& output, std::uint32_t codepoint,
                    CharacterAssetProvenanceError3D* error) {
        if (codepoint <= 0x7fu) output.push_back(static_cast<char>(codepoint));
        else if (codepoint <= 0x7ffu) {
            output.push_back(static_cast<char>(0xc0u | (codepoint >> 6u)));
            output.push_back(static_cast<char>(0x80u | (codepoint & 0x3fu)));
        } else if (codepoint <= 0xffffu) {
            output.push_back(static_cast<char>(0xe0u | (codepoint >> 12u)));
            output.push_back(static_cast<char>(0x80u | ((codepoint >> 6u) & 0x3fu)));
            output.push_back(static_cast<char>(0x80u | (codepoint & 0x3fu)));
        } else {
            output.push_back(static_cast<char>(0xf0u | (codepoint >> 18u)));
            output.push_back(static_cast<char>(0x80u | ((codepoint >> 12u) & 0x3fu)));
            output.push_back(static_cast<char>(0x80u | ((codepoint >> 6u) & 0x3fu)));
            output.push_back(static_cast<char>(0x80u | (codepoint & 0x3fu)));
        }
        if (output.size() > maximumStringBytes_) {
            SetError(error, CharacterAssetProvenanceErrorCode3D::InputLimitExceeded,
                     position_, "JSON string exceeds limit");
            return false;
        }
        return true;
    }
    bool String(std::string& output, CharacterAssetProvenanceError3D* error) {
        if (position_ >= text_.size() || text_[position_++] != '"') return false;
        output.clear();
        while (position_ < text_.size()) {
            const unsigned char current = static_cast<unsigned char>(text_[position_++]);
            if (current == '"') return true;
            if (current < 0x20u) {
                SetError(error, CharacterAssetProvenanceErrorCode3D::InvalidJson,
                         position_ - 1u, "Control character in JSON string");
                return false;
            }
            if (current != '\\') {
                output.push_back(static_cast<char>(current));
            } else {
                if (position_ >= text_.size()) break;
                const char escaped = text_[position_++];
                switch (escaped) {
                case '"': output.push_back('"'); break;
                case '\\': output.push_back('\\'); break;
                case '/': output.push_back('/'); break;
                case 'b': output.push_back('\b'); break;
                case 'f': output.push_back('\f'); break;
                case 'n': output.push_back('\n'); break;
                case 'r': output.push_back('\r'); break;
                case 't': output.push_back('\t'); break;
                case 'u': {
                    std::uint32_t codepoint{};
                    for (int index = 0; index < 4; ++index) {
                        std::uint32_t digit{};
                        if (position_ >= text_.size() || !Hex(text_[position_++], digit)) {
                            SetError(error, CharacterAssetProvenanceErrorCode3D::InvalidJson,
                                     position_, "Invalid Unicode escape");
                            return false;
                        }
                        codepoint = (codepoint << 4u) | digit;
                    }
                    if (codepoint >= 0xd800u && codepoint <= 0xdbffu) {
                        if (position_ + 6u > text_.size() || text_[position_] != '\\' ||
                            text_[position_ + 1u] != 'u') return false;
                        position_ += 2u;
                        std::uint32_t low{};
                        for (int index = 0; index < 4; ++index) {
                            std::uint32_t digit{};
                            if (!Hex(text_[position_++], digit)) return false;
                            low = (low << 4u) | digit;
                        }
                        if (low < 0xdc00u || low > 0xdfffu) return false;
                        codepoint = 0x10000u + ((codepoint - 0xd800u) << 10u) + (low - 0xdc00u);
                    } else if (codepoint >= 0xdc00u && codepoint <= 0xdfffu) return false;
                    if (!AppendUtf8(output, codepoint, error)) return false;
                    break;
                }
                default:
                    SetError(error, CharacterAssetProvenanceErrorCode3D::InvalidJson,
                             position_ - 1u, "Invalid JSON escape");
                    return false;
                }
            }
            if (output.size() > maximumStringBytes_) {
                SetError(error, CharacterAssetProvenanceErrorCode3D::InputLimitExceeded,
                         position_, "JSON string exceeds limit");
                return false;
            }
        }
        SetError(error, CharacterAssetProvenanceErrorCode3D::InvalidJson,
                 position_, "Unterminated JSON string");
        return false;
    }
    bool Value(JsonValue3D& output, std::size_t depth,
               CharacterAssetProvenanceError3D* error) {
        if (depth > 32u) {
            SetError(error, CharacterAssetProvenanceErrorCode3D::InputLimitExceeded,
                     position_, "JSON nesting exceeds limit");
            return false;
        }
        Space();
        if (position_ >= text_.size()) return false;
        const char value = text_[position_];
        if (value == '"') { output.kind = JsonKind3D::String; return String(output.string, error); }
        if (text_.substr(position_, 4u) == "true") { position_ += 4u; output.kind = JsonKind3D::Boolean; output.boolean = true; return true; }
        if (text_.substr(position_, 5u) == "false") { position_ += 5u; output.kind = JsonKind3D::Boolean; return true; }
        if (text_.substr(position_, 4u) == "null") { position_ += 4u; output.kind = JsonKind3D::Null; return true; }
        if (value == '-' || (value >= '0' && value <= '9')) {
            const std::size_t begin = position_;
            if (text_[position_] == '-') ++position_;
            if (position_ >= text_.size()) return false;
            if (text_[position_] == '0') ++position_;
            else if (text_[position_] >= '1' && text_[position_] <= '9') {
                while (position_ < text_.size() && text_[position_] >= '0' && text_[position_] <= '9')
                    ++position_;
            } else return false;
            bool integral = true;
            if (position_ < text_.size() && text_[position_] == '.') {
                integral = false; ++position_;
                const std::size_t digits = position_;
                while (position_ < text_.size() && text_[position_] >= '0' && text_[position_] <= '9')
                    ++position_;
                if (digits == position_) return false;
            }
            if (position_ < text_.size() && (text_[position_] == 'e' || text_[position_] == 'E')) {
                integral = false; ++position_;
                if (position_ < text_.size() && (text_[position_] == '+' || text_[position_] == '-'))
                    ++position_;
                const std::size_t digits = position_;
                while (position_ < text_.size() && text_[position_] >= '0' && text_[position_] <= '9')
                    ++position_;
                if (digits == position_) return false;
            }
            output.kind = JsonKind3D::Number;
            output.unsignedInteger = integral && text_[begin] != '-';
            if (output.unsignedInteger) {
                const auto converted = std::from_chars(text_.data() + begin,
                                                       text_.data() + position_, output.number);
                output.unsignedInteger = converted.ec == std::errc{} &&
                                         converted.ptr == text_.data() + position_;
            }
            return true;
        }
        if (value == '[') {
            ++position_; output.kind = JsonKind3D::Array; Space();
            if (position_ < text_.size() && text_[position_] == ']') { ++position_; return true; }
            while (true) {
                if (output.array.size() >= maximumEntries_) {
                    SetError(error, CharacterAssetProvenanceErrorCode3D::InputLimitExceeded,
                             position_, "JSON array exceeds limit"); return false;
                }
                output.array.emplace_back();
                if (!Value(output.array.back(), depth + 1u, error)) return false;
                Space(); if (position_ >= text_.size()) return false;
                const char separator = text_[position_++];
                if (separator == ']') return true;
                if (separator != ',') return false;
            }
        }
        if (value == '{') {
            ++position_; output.kind = JsonKind3D::Object; Space();
            if (position_ < text_.size() && text_[position_] == '}') { ++position_; return true; }
            std::unordered_set<std::string> keys;
            while (true) {
                if (output.object.size() >= maximumEntries_) {
                    SetError(error, CharacterAssetProvenanceErrorCode3D::InputLimitExceeded,
                             position_, "JSON object exceeds limit"); return false;
                }
                Space(); std::string key;
                if (!String(key, error)) return false;
                if (!keys.insert(key).second) {
                    SetError(error, CharacterAssetProvenanceErrorCode3D::InvalidJson,
                             position_, "Duplicate JSON key"); return false;
                }
                Space(); if (position_ >= text_.size() || text_[position_++] != ':') return false;
                output.object.emplace_back(std::move(key), JsonValue3D{});
                if (!Value(output.object.back().second, depth + 1u, error)) return false;
                Space(); if (position_ >= text_.size()) return false;
                const char separator = text_[position_++];
                if (separator == '}') return true;
                if (separator != ',') return false;
            }
        }
        SetError(error, CharacterAssetProvenanceErrorCode3D::InvalidJson,
                 position_, "Invalid JSON token");
        return false;
    }

    std::string_view text_;
    std::size_t maximumStringBytes_{};
    std::size_t maximumEntries_{};
    std::size_t position_{};
};

const JsonValue3D* Field(const JsonValue3D& object, std::string_view name) {
    if (object.kind != JsonKind3D::Object) return nullptr;
    for (const auto& [key, value] : object.object) if (key == name) return &value;
    return nullptr;
}
bool StringField(const JsonValue3D& object, std::string_view name, std::string& output) {
    const JsonValue3D* value = Field(object, name);
    if (!value || value->kind != JsonKind3D::String || value->string.empty()) return false;
    output = value->string; return true;
}
bool NumberField(const JsonValue3D& object, std::string_view name, std::uint64_t& output) {
    const JsonValue3D* value = Field(object, name);
    if (!value || value->kind != JsonKind3D::Number || !value->unsignedInteger) return false;
    output = value->number; return true;
}
bool ExactKeys(const JsonValue3D& value, std::initializer_list<std::string_view> keys) {
    if (value.kind != JsonKind3D::Object || value.object.size() != keys.size()) return false;
    for (std::string_view key : keys) if (!Field(value, key)) return false;
    return true;
}
bool Identifier(std::string_view value) {
    if (value.empty() || value.size() > 128u) return false;
    return std::ranges::all_of(value, [](char item) {
        return (item >= 'a' && item <= 'z') || (item >= 'A' && item <= 'Z') ||
               (item >= '0' && item <= '9') || item == '-' || item == '_' || item == '.';
    });
}
bool Sha256Text(std::string_view value) {
    return value.size() == 64u && std::ranges::all_of(value, [](char item) {
        return (item >= '0' && item <= '9') || (item >= 'a' && item <= 'f');
    });
}
bool SafeRelativePath(std::string_view text) {
    if (text.empty() || text.size() > 1024u || text.front() == '/' ||
        text.find('\\') != std::string_view::npos || text.find(':') != std::string_view::npos) return false;
    std::filesystem::path path(text);
    if (path.is_absolute() || path.has_root_name() || path.has_root_directory()) return false;
    for (const auto& part : path) if (part == "." || part == ".." || part.empty()) return false;
    return path.generic_string() == text;
}
bool SameInformation(const BY_HANDLE_FILE_INFORMATION& left,
                     const BY_HANDLE_FILE_INFORMATION& right) {
    return left.dwVolumeSerialNumber == right.dwVolumeSerialNumber &&
           left.nFileIndexHigh == right.nFileIndexHigh && left.nFileIndexLow == right.nFileIndexLow &&
           left.nFileSizeHigh == right.nFileSizeHigh && left.nFileSizeLow == right.nFileSizeLow &&
           left.ftLastWriteTime.dwHighDateTime == right.ftLastWriteTime.dwHighDateTime &&
           left.ftLastWriteTime.dwLowDateTime == right.ftLastWriteTime.dwLowDateTime;
}
bool FinalNormalizedPath(HANDLE handle, std::wstring& output) {
    const DWORD flags = FILE_NAME_NORMALIZED | VOLUME_NAME_DOS;
    const DWORD required = GetFinalPathNameByHandleW(handle, nullptr, 0u, flags);
    if (!required) return false;
    output.resize(required);
    const DWORD written = GetFinalPathNameByHandleW(handle, output.data(), required, flags);
    if (!written || written >= required) return false;
    output.resize(written);
    while (output.size() > 4u && (output.back() == L'\\' || output.back() == L'/')) output.pop_back();
    return true;
}
bool WithinPinnedRoot(std::wstring_view path, std::wstring_view root) {
    return path.size() > root.size() &&
           CompareStringOrdinal(path.data(), static_cast<int>(root.size()), root.data(),
                                static_cast<int>(root.size()), TRUE) == CSTR_EQUAL &&
           (path[root.size()] == L'\\' || path[root.size()] == L'/');
}
struct PinnedDirectory3D {
    HANDLE handle{INVALID_HANDLE_VALUE};
    std::wstring finalPath;
};
struct PinnedHandles3D {
    std::vector<HANDLE> values;
    ~PinnedHandles3D() {
        for (auto iterator = values.rbegin(); iterator != values.rend(); ++iterator) CloseHandle(*iterator);
    }
    HANDLE Keep(HANDLE value) { values.push_back(value); return value; }
};
struct PhysicalIdentity3D {
    DWORD volume{}, indexHigh{}, indexLow{};
    bool operator<(const PhysicalIdentity3D& right) const noexcept {
        return std::tie(volume, indexHigh, indexLow) <
               std::tie(right.volume, right.indexHigh, right.indexLow);
    }
};
using NtCreateFile3D = NTSTATUS(NTAPI*)(PHANDLE, ACCESS_MASK, POBJECT_ATTRIBUTES,
                                       PIO_STATUS_BLOCK, PLARGE_INTEGER, ULONG, ULONG,
                                       ULONG, ULONG, PVOID, ULONG);
NtCreateFile3D ResolveNtCreateFile() {
    const HMODULE module = GetModuleHandleW(L"ntdll.dll");
    return module ? reinterpret_cast<NtCreateFile3D>(
                        GetProcAddress(module, "NtCreateFile")) : nullptr;
}
bool NativeOpenRelative(HANDLE parent, std::wstring_view component, bool directory,
                        HANDLE& output, NTSTATUS* nativeStatus = nullptr) {
    static const NtCreateFile3D ntCreateFile = ResolveNtCreateFile();
    if (!ntCreateFile || component.empty() || component.size() > 255u ||
        component == L"." || component == L".." ||
        component.find_first_of(L"\\/") != std::wstring_view::npos) {
        if (nativeStatus) *nativeStatus = static_cast<NTSTATUS>(0xc000000du);
        return false;
    }
    UNICODE_STRING name{};
    name.Length = static_cast<USHORT>(component.size() * sizeof(wchar_t));
    name.MaximumLength = name.Length;
    name.Buffer = const_cast<PWSTR>(component.data());
    OBJECT_ATTRIBUTES attributes{};
    attributes.Length = sizeof(attributes);
    attributes.RootDirectory = parent;
    attributes.ObjectName = &name;
    attributes.Attributes = OBJ_CASE_INSENSITIVE;
    IO_STATUS_BLOCK statusBlock{};
    const ACCESS_MASK access = directory
                                   ? FILE_LIST_DIRECTORY | FILE_READ_ATTRIBUTES | SYNCHRONIZE
                                   : FILE_GENERIC_READ | SYNCHRONIZE;
    const ULONG options = FILE_SYNCHRONOUS_IO_NONALERT | FILE_OPEN_REPARSE_POINT |
                          (directory ? FILE_DIRECTORY_FILE : FILE_NON_DIRECTORY_FILE);
    output = INVALID_HANDLE_VALUE;
    const NTSTATUS status = ntCreateFile(&output, access, &attributes, &statusBlock, nullptr,
                                         FILE_ATTRIBUTE_NORMAL, FILE_SHARE_READ, FILE_OPEN,
                                         options, nullptr, 0u);
    if (nativeStatus) *nativeStatus = status;
    return status >= 0 && output != INVALID_HANDLE_VALUE;
}
bool PinRuntimeRoot(const std::filesystem::path& path, PinnedHandles3D& handles,
                    PinnedDirectory3D& output, CharacterAssetProvenanceError3D* error) {
    const HANDLE root = CreateFileW(path.c_str(), FILE_LIST_DIRECTORY | FILE_READ_ATTRIBUTES,
                                    FILE_SHARE_READ, nullptr, OPEN_EXISTING,
                                    FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT,
                                    nullptr);
    BY_HANDLE_FILE_INFORMATION information{};
    if (root == INVALID_HANDLE_VALUE || !GetFileInformationByHandle(root, &information) ||
        !(information.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ||
        (information.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) ||
        !FinalNormalizedPath(root, output.finalPath)) {
        if (root != INVALID_HANDLE_VALUE) CloseHandle(root);
        SetError(error, CharacterAssetProvenanceErrorCode3D::InvalidPath, 0u,
                 "Expected runtime root could not be pinned to a physical directory");
        return false;
    }
    output.handle = handles.Keep(root);
    return true;
}
bool PinRelativeDirectory(const PinnedDirectory3D& parent, std::wstring_view component,
                          std::wstring_view requiredRoot, PinnedHandles3D& handles,
                          PinnedDirectory3D& output, CharacterAssetProvenanceError3D* error) {
    HANDLE directory{};
    BY_HANDLE_FILE_INFORMATION information{};
    if (!NativeOpenRelative(parent.handle, component, true, directory) ||
        !GetFileInformationByHandle(directory, &information) ||
        !(information.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ||
        (information.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) ||
        !FinalNormalizedPath(directory, output.finalPath) ||
        !WithinPinnedRoot(output.finalPath, requiredRoot)) {
        if (directory && directory != INVALID_HANDLE_VALUE) CloseHandle(directory);
        SetError(error, CharacterAssetProvenanceErrorCode3D::InvalidPath, 0u,
                 "Relative directory is missing, reparsed, or escaped its pinned root");
        return false;
    }
    output.handle = handles.Keep(directory);
    return true;
}
bool ReadPinnedFile(HANDLE parent, const std::filesystem::path& relativePath,
                    std::uint64_t maximumBytes, std::wstring_view pinnedRoot,
                    PinnedHandles3D& handles, std::string* contents,
                    std::uint64_t& bytes, std::string& sha256,
                    PhysicalIdentity3D* identity,
                    CharacterAssetProvenanceError3D* error) {
    std::vector<std::wstring> components;
    for (const auto& component : relativePath) components.push_back(component.wstring());
    if (components.empty()) {
        SetError(error, CharacterAssetProvenanceErrorCode3D::InvalidPath, 0u,
                 "Relative file path is empty"); return false;
    }
    HANDLE current = parent;
    for (std::size_t index = 0; index + 1u < components.size(); ++index) {
        HANDLE directory{}; BY_HANDLE_FILE_INFORMATION information{}; std::wstring finalPath;
        if (!NativeOpenRelative(current, components[index], true, directory) ||
            !GetFileInformationByHandle(directory, &information) ||
            !(information.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ||
            (information.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) ||
            !FinalNormalizedPath(directory, finalPath) || !WithinPinnedRoot(finalPath, pinnedRoot)) {
            if (directory && directory != INVALID_HANDLE_VALUE) CloseHandle(directory);
            SetError(error, CharacterAssetProvenanceErrorCode3D::InvalidPath, 0u,
                     "Artifact parent is missing, reparsed, or escaped the pinned pack");
            return false;
        }
        current = handles.Keep(directory);
    }
    HANDLE file{}; NTSTATUS nativeStatus{};
    if (!NativeOpenRelative(current, components.back(), false, file, &nativeStatus)) {
        constexpr NTSTATUS SharingViolation = static_cast<NTSTATUS>(0xc0000043u);
        SetError(error, nativeStatus == SharingViolation
                            ? CharacterAssetProvenanceErrorCode3D::ConcurrentModification
                            : CharacterAssetProvenanceErrorCode3D::IoFailure, 0u,
                 "File could not be opened relative to its pinned parent"); return false;
    }
    handles.Keep(file);
    BY_HANDLE_FILE_INFORMATION before{}, after{};
    std::wstring finalPath;
    LARGE_INTEGER size{};
    if (!GetFileInformationByHandle(file, &before) ||
        (before.dwFileAttributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT)) ||
        !FinalNormalizedPath(file, finalPath) || !WithinPinnedRoot(finalPath, pinnedRoot)) {
        SetError(error, CharacterAssetProvenanceErrorCode3D::InvalidPath, 0u,
                 "File handle escaped the pinned manifest root"); return false;
    }
    if (!GetFileSizeEx(file, &size) || size.QuadPart < 0 ||
        static_cast<std::uint64_t>(size.QuadPart) > maximumBytes ||
        static_cast<std::uint64_t>(size.QuadPart) > (std::numeric_limits<std::size_t>::max)()) {
        SetError(error, CharacterAssetProvenanceErrorCode3D::InputLimitExceeded, 0u,
                 "File size is invalid or exceeds the validation limit"); return false;
    }
    bytes = static_cast<std::uint64_t>(size.QuadPart);
    if (identity) *identity = {before.dwVolumeSerialNumber, before.nFileIndexHigh,
                               before.nFileIndexLow};
    if (contents) contents->clear();
    if (contents) contents->reserve(static_cast<std::size_t>(bytes));
    BCRYPT_ALG_HANDLE algorithm{}; BCRYPT_HASH_HANDLE hash{};
    DWORD objectBytes{}, copied{}; std::vector<UCHAR> object; std::array<UCHAR, 32> digest{};
    bool passed = BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0) >= 0;
    if (passed) passed = BCryptGetProperty(algorithm, BCRYPT_OBJECT_LENGTH,
        reinterpret_cast<PUCHAR>(&objectBytes), sizeof(objectBytes), &copied, 0) >= 0;
    if (passed) object.resize(objectBytes);
    if (passed) passed = BCryptCreateHash(algorithm, &hash, object.data(), objectBytes, nullptr, 0, 0) >= 0;
    std::array<unsigned char, 64u * 1024u> chunk{}; std::uint64_t consumed{};
    while (passed) {
        DWORD count{};
        if (!ReadFile(file, chunk.data(), static_cast<DWORD>(chunk.size()), &count, nullptr)) { passed = false; break; }
        if (!count) break;
        consumed += count;
        if (contents) contents->append(reinterpret_cast<const char*>(chunk.data()), count);
        if (BCryptHashData(hash, chunk.data(), count, 0) < 0) passed = false;
    }
    if (passed) passed = consumed == bytes &&
        BCryptFinishHash(hash, digest.data(), static_cast<ULONG>(digest.size()), 0) >= 0 &&
        GetFileInformationByHandle(file, &after) && SameInformation(before, after);
    if (hash) BCryptDestroyHash(hash);
    if (algorithm) BCryptCloseAlgorithmProvider(algorithm, 0);
    if (!passed) {
        SetError(error, CharacterAssetProvenanceErrorCode3D::ConcurrentModification, 0u,
                 "File changed or could not be hashed while pinned"); return false;
    }
    constexpr char HexDigits[] = "0123456789abcdef";
    sha256.resize(64u);
    for (std::size_t index = 0; index < digest.size(); ++index) {
        sha256[index * 2u] = HexDigits[digest[index] >> 4u];
        sha256[index * 2u + 1u] = HexDigits[digest[index] & 15u];
    }
    return true;
}

struct GltfEvidence3D {
    std::string skeletonIdentity;
    std::vector<std::string> jointNames;
    std::vector<std::string> materialNames;
    std::vector<std::string> animationNames;
};
bool CollectGltfEvidence(std::string_view contents, std::size_t maximumStringBytes,
                         std::size_t maximumEntries, GltfEvidence3D& output,
                         CharacterAssetProvenanceError3D* error) {
    JsonValue3D root;
    if (!JsonParser3D(contents, maximumStringBytes, maximumEntries).Parse(root, error)) {
        if (error && !*error) SetError(error, CharacterAssetProvenanceErrorCode3D::InvalidJson,
                                      0u, "Source GLTF is malformed");
        return false;
    }
    const JsonValue3D* animations = Field(root, "animations");
    const JsonValue3D* skins = Field(root, "skins");
    const JsonValue3D* nodes = Field(root, "nodes");
    const JsonValue3D* materials = Field(root, "materials");
    if (!animations || animations->kind != JsonKind3D::Array ||
        !skins || skins->kind != JsonKind3D::Array || skins->array.size() != 1u ||
        !nodes || nodes->kind != JsonKind3D::Array ||
        !materials || materials->kind != JsonKind3D::Array) {
        SetError(error, CharacterAssetProvenanceErrorCode3D::InvalidSchema, 0u,
                 "Source GLTF semantic tables are missing or ambiguous"); return false;
    }
    std::unordered_set<std::string> unique;
    for (const JsonValue3D& animation : animations->array) {
        std::string name;
        if (!StringField(animation, "name", name) || !Identifier(name) ||
            !unique.insert(name).second) {
            SetError(error, CharacterAssetProvenanceErrorCode3D::InvalidSchema, 0u,
                     "Source GLTF animation identity is invalid or duplicated"); return false;
        }
        output.animationNames.push_back(std::move(name));
    }
    const JsonValue3D& skin = skins->array.front();
    const JsonValue3D* joints = Field(skin, "joints");
    if (!StringField(skin, "name", output.skeletonIdentity) || !Identifier(output.skeletonIdentity) ||
        !joints || joints->kind != JsonKind3D::Array || joints->array.empty()) {
        SetError(error, CharacterAssetProvenanceErrorCode3D::InvalidSchema, 0u,
                 "Source GLTF skeleton identity or joint table is invalid"); return false;
    }
    unique.clear();
    for (const JsonValue3D& joint : joints->array) {
        if (joint.kind != JsonKind3D::Number || !joint.unsignedInteger ||
            joint.number >= nodes->array.size()) {
            SetError(error, CharacterAssetProvenanceErrorCode3D::InvalidSchema, 0u,
                     "Source GLTF joint index is invalid"); return false;
        }
        std::string name;
        if (!StringField(nodes->array[static_cast<std::size_t>(joint.number)], "name", name) ||
            !Identifier(name) || !unique.insert(name).second) {
            SetError(error, CharacterAssetProvenanceErrorCode3D::InvalidSchema, 0u,
                     "Source GLTF joint identity is invalid or duplicated"); return false;
        }
        output.jointNames.push_back(std::move(name));
    }
    unique.clear();
    for (const JsonValue3D& material : materials->array) {
        std::string name;
        if (!StringField(material, "name", name) || !Identifier(name) ||
            !unique.insert(name).second) {
            SetError(error, CharacterAssetProvenanceErrorCode3D::InvalidSchema, 0u,
                     "Source GLTF material identity is invalid or duplicated"); return false;
        }
        output.materialNames.push_back(std::move(name));
    }
    return true;
}

bool LoadAeskinEvidence(std::string_view contents, SkinnedModelAssetData& output,
                        CharacterAssetProvenanceError3D* error) {
    if (contents.size() < 8u) {
        SetError(error, CharacterAssetProvenanceErrorCode3D::InvalidSchema, 0u,
                 "Cooked AESKIN header is truncated"); return false;
    }
    const auto u32 = [&](std::size_t offset) {
        const auto* bytes = reinterpret_cast<const unsigned char*>(contents.data() + offset);
        return std::uint32_t(bytes[0]) | (std::uint32_t(bytes[1]) << 8u) |
               (std::uint32_t(bytes[2]) << 16u) | (std::uint32_t(bytes[3]) << 24u);
    };
    if (u32(0u) != 0x4b534541u || u32(4u) != 2u) {
        SetError(error, CharacterAssetProvenanceErrorCode3D::InvalidSchema, 0u,
                 "Cooked artifact is not AESKIN v2"); return false;
    }
    auto loaded = LoadSkinnedModelAssetDetailed(std::span<const std::byte>{
        reinterpret_cast<const std::byte*>(contents.data()), contents.size()});
    if (!loaded) {
        SetError(error, CharacterAssetProvenanceErrorCode3D::InvalidSchema, 0u,
                 "Cooked AESKIN v2 clip table is invalid"); return false;
    }
    output = std::move(loaded.asset);
    return true;
}

bool StringArray(const JsonValue3D& parent, std::string_view field,
                 std::vector<std::string>& output, std::unordered_set<std::string>& identities,
                 CharacterAssetProvenanceError3D* error) {
    const JsonValue3D* array = Field(parent, field);
    if (!array || array->kind != JsonKind3D::Array || array->array.empty()) {
        SetError(error, CharacterAssetProvenanceErrorCode3D::InvalidSchema, 0u,
                 "Required identity collection is missing, empty, or has the wrong kind");
        return false;
    }
    for (const JsonValue3D& value : array->array) {
        if (value.kind != JsonKind3D::String || !Identifier(value.string) ||
            !identities.insert(value.string).second) {
            SetError(error, CharacterAssetProvenanceErrorCode3D::DuplicateIdentity, 0u,
                     "Collection identity is empty, invalid, or duplicated"); return false;
        }
        output.push_back(value.string);
    }
    return true;
}

} // namespace

bool ValidateCharacterAssetProvenance3D(
    const std::filesystem::path& expectedRuntimeRoot,
    const std::filesystem::path& manifestPath,
    const CharacterAssetProvenanceTrustAnchor3D& trustAnchor,
    CharacterAssetProvenanceReceipt3D& output,
    CharacterAssetProvenanceError3D* error, const CharacterAssetProvenanceLimits3D& limits) {
    if (error) *error = {};
    constexpr std::uint64_t HardMaximumArtifactBytes = 256ull * 1024ull * 1024ull;
    if (!limits.maximumManifestBytes || limits.maximumManifestBytes > 16u * 1024u * 1024u ||
        !limits.maximumStringBytes || !limits.maximumCollectionEntries || !limits.maximumArtifactBytes ||
        limits.maximumArtifactBytes > HardMaximumArtifactBytes) {
        SetError(error, CharacterAssetProvenanceErrorCode3D::InputLimitExceeded, 0u,
                 "Validation limits are invalid"); return false;
    }
    if (!trustAnchor.manifestBytes || !Sha256Text(trustAnchor.manifestSha256)) {
        SetError(error, CharacterAssetProvenanceErrorCode3D::InvalidSchema, 0u,
                 "Runtime manifest trust anchor is invalid"); return false;
    }
    CharacterAssetProvenanceReceipt3D candidate;
    std::error_code pathError;
    const auto absoluteRoot = std::filesystem::absolute(expectedRuntimeRoot, pathError).lexically_normal();
    if (pathError) {
        SetError(error, CharacterAssetProvenanceErrorCode3D::InvalidPath, 0u,
                 "Expected runtime root is not an absolute lexical anchor"); return false;
    }
    const auto absoluteManifest = std::filesystem::absolute(manifestPath, pathError).lexically_normal();
    if (pathError) {
        SetError(error, CharacterAssetProvenanceErrorCode3D::InvalidPath, 0u,
                 "Manifest path is not an absolute lexical path"); return false;
    }
    const auto relativeManifest = absoluteManifest.lexically_relative(absoluteRoot);
    std::vector<std::filesystem::path> manifestComponents;
    for (const auto& component : relativeManifest) manifestComponents.push_back(component);
    if (manifestComponents.size() != 4u || manifestComponents[0] != "assets" ||
        manifestComponents[1] != "model-packs" ||
        !Identifier(manifestComponents[2].generic_string()) ||
        manifestComponents[3].generic_string() !=
            manifestComponents[2].generic_string() + ".provenance.json") {
        SetError(error, CharacterAssetProvenanceErrorCode3D::InvalidPath, 0u,
                 "Manifest is not the exact asset pack under the expected runtime root"); return false;
    }
    const std::string packIdentity = manifestComponents[2].generic_string();
    PinnedHandles3D pinnedHandles;
    PinnedDirectory3D pinnedRoot, assetsDirectory, modelPacksDirectory, packDirectory;
    if (!PinRuntimeRoot(absoluteRoot, pinnedHandles, pinnedRoot, error)) return false;
    if (limits.barrierCallback)
        limits.barrierCallback(CharacterAssetProvenanceBarrier3D::RuntimeRootPinned,
                               limits.barrierContext);
    if (!PinRelativeDirectory(pinnedRoot, L"assets", pinnedRoot.finalPath, pinnedHandles,
                              assetsDirectory, error) ||
        !PinRelativeDirectory(assetsDirectory, L"model-packs", pinnedRoot.finalPath,
                              pinnedHandles, modelPacksDirectory, error)) return false;
    if (limits.barrierCallback)
        limits.barrierCallback(CharacterAssetProvenanceBarrier3D::ModelPacksPinned,
                               limits.barrierContext);
    const std::wstring packIdentityWide = manifestComponents[2].wstring();
    if (!PinRelativeDirectory(modelPacksDirectory, packIdentityWide, pinnedRoot.finalPath,
                              pinnedHandles, packDirectory, error)) return false;
    if (limits.barrierCallback)
        limits.barrierCallback(CharacterAssetProvenanceBarrier3D::AssetPackPinned,
                               limits.barrierContext);
    std::string json;
    if (!ReadPinnedFile(packDirectory.handle, manifestComponents[3], limits.maximumManifestBytes,
                        packDirectory.finalPath, pinnedHandles, &json, candidate.manifestBytes,
                        candidate.manifestSha256, nullptr, error)) return false;
    if (candidate.manifestBytes != trustAnchor.manifestBytes ||
        candidate.manifestSha256 != trustAnchor.manifestSha256) {
        SetError(error, CharacterAssetProvenanceErrorCode3D::IntegrityMismatch, 0u,
                 "Manifest bytes or SHA-256 do not match the runtime trust anchor"); return false;
    }
    JsonValue3D root;
    if (!JsonParser3D(json, limits.maximumStringBytes, limits.maximumCollectionEntries).Parse(root, error)) {
        if (error && !*error) SetError(error, CharacterAssetProvenanceErrorCode3D::InvalidJson, 0u, "Malformed JSON");
        return false;
    }
    if (!ExactKeys(root, {"schema", "version", "assetId", "canonicalRuntimePath", "generator",
                          "converter", "artifacts", "skeleton", "materials", "lods",
                          "animations", "runtimeFallbacks"})) {
        SetError(error, CharacterAssetProvenanceErrorCode3D::InvalidSchema, 0u,
                 "Root fields are missing or unexpected"); return false;
    }
    std::string schema; std::uint64_t version{};
    if (!StringField(root, "schema", schema) || schema != "aevrynd.character-asset-provenance" ||
        !NumberField(root, "version", version) || version != 1u ||
        !StringField(root, "assetId", candidate.assetId) || !Identifier(candidate.assetId) ||
        candidate.assetId != packIdentity ||
        !StringField(root, "canonicalRuntimePath", candidate.canonicalRuntimePath) ||
        !SafeRelativePath(candidate.canonicalRuntimePath)) {
        SetError(error, CharacterAssetProvenanceErrorCode3D::InvalidSchema, 0u,
                 "Manifest identity, version, or canonical runtime path is invalid"); return false;
    }
    const JsonValue3D* generator = Field(root, "generator");
    const JsonValue3D* converter = Field(root, "converter");
    if (!generator || !converter || !ExactKeys(*generator, {"id", "version", "options"}) ||
        !ExactKeys(*converter, {"id", "version", "options"}) ||
        !StringField(*generator, "id", candidate.generatorId) ||
        !StringField(*converter, "id", candidate.converterId) ||
        !SafeRelativePath(candidate.generatorId) || !SafeRelativePath(candidate.converterId)) {
        SetError(error, CharacterAssetProvenanceErrorCode3D::InvalidSchema, 0u,
                 "Generator or converter contract is invalid"); return false;
    }
    std::string componentVersion;
    if (!StringField(*generator, "version", componentVersion) ||
        !StringField(*converter, "version", componentVersion)) {
        SetError(error, CharacterAssetProvenanceErrorCode3D::InvalidSchema, 0u,
                 "Generator or converter version is missing"); return false;
    }
    for (const JsonValue3D* component : {generator, converter}) {
        const JsonValue3D* options = Field(*component, "options");
        if (!options || options->kind != JsonKind3D::Array || options->array.empty()) {
            SetError(error, CharacterAssetProvenanceErrorCode3D::InvalidSchema, 0u,
                     "Generator and converter options must be explicit"); return false;
        }
        std::unordered_set<std::string> unique;
        for (const JsonValue3D& option : options->array)
            if (option.kind != JsonKind3D::String || option.string.empty() || !unique.insert(option.string).second) {
                SetError(error, CharacterAssetProvenanceErrorCode3D::InvalidSchema, 0u,
                         "Generator or converter option is invalid"); return false;
            }
    }
    std::unordered_set<std::string> identities;
    const JsonValue3D* artifacts = Field(root, "artifacts");
    if (!artifacts || artifacts->kind != JsonKind3D::Array || artifacts->array.size() != 3u) {
        SetError(error, CharacterAssetProvenanceErrorCode3D::InvalidSchema, 0u,
                 "Exactly source GLTF, source BIN, and cooked AESKIN are required"); return false;
    }
    std::unordered_set<std::string> roles;
    std::set<PhysicalIdentity3D> physicalArtifacts;
    std::string sourceGltfContents, cookedAeskinContents;
    for (const JsonValue3D& value : artifacts->array) {
        CharacterAssetArtifactReceipt3D artifact; std::uint64_t expectedBytes{};
        if (!ExactKeys(value, {"identity", "role", "path", "bytes", "sha256"}) ||
            !StringField(value, "identity", artifact.identity) || !Identifier(artifact.identity) ||
            !identities.insert(artifact.identity).second || !StringField(value, "role", artifact.role) ||
            !roles.insert(artifact.role).second || !StringField(value, "path", artifact.path) ||
            !SafeRelativePath(artifact.path) || !NumberField(value, "bytes", expectedBytes) || !expectedBytes ||
            !StringField(value, "sha256", artifact.sha256) || !Sha256Text(artifact.sha256)) {
            SetError(error, CharacterAssetProvenanceErrorCode3D::InvalidSchema, 0u,
                     "Artifact identity, path, size, or hash is invalid"); return false;
        }
        std::string actualHash, artifactContents; std::uint64_t actualBytes{};
        std::string* retainedContents = artifact.role == "source-gltf" || artifact.role == "cooked-aeskin"
                                            ? &artifactContents : nullptr;
        PhysicalIdentity3D physicalIdentity;
        if (!ReadPinnedFile(packDirectory.handle, std::filesystem::path(artifact.path),
                            limits.maximumArtifactBytes, packDirectory.finalPath, pinnedHandles,
                            retainedContents, actualBytes, actualHash, &physicalIdentity, error)) return false;
        if (!physicalArtifacts.insert(physicalIdentity).second) {
            SetError(error, CharacterAssetProvenanceErrorCode3D::DuplicateIdentity, 0u,
                     "Artifact paths resolve to the same physical file"); return false;
        }
        if (actualBytes != expectedBytes || actualHash != artifact.sha256) {
            SetError(error, CharacterAssetProvenanceErrorCode3D::IntegrityMismatch, 0u,
                     "Artifact size or SHA-256 does not match manifest"); return false;
        }
        if (artifact.role == "source-gltf") sourceGltfContents = std::move(artifactContents);
        if (artifact.role == "cooked-aeskin") {
            cookedAeskinContents = std::move(artifactContents);
            if (limits.barrierCallback)
                limits.barrierCallback(CharacterAssetProvenanceBarrier3D::CookedAssetPinned,
                                       limits.barrierContext);
        }
        artifact.bytes = actualBytes;
        candidate.artifacts.push_back(std::move(artifact));
    }
    if (!roles.contains("source-gltf") || !roles.contains("source-bin") || !roles.contains("cooked-aeskin")) {
        SetError(error, CharacterAssetProvenanceErrorCode3D::InvalidSchema, 0u,
                 "Required artifact roles are incomplete"); return false;
    }
    const JsonValue3D* skeleton = Field(root, "skeleton"); std::uint64_t jointCount{};
    if (!skeleton || !ExactKeys(*skeleton, {"identity", "jointCount"}) ||
        !StringField(*skeleton, "identity", candidate.skeletonIdentity) ||
        !Identifier(candidate.skeletonIdentity) || !identities.insert(candidate.skeletonIdentity).second ||
        !NumberField(*skeleton, "jointCount", jointCount) || !jointCount || jointCount > 1024u) {
        SetError(error, CharacterAssetProvenanceErrorCode3D::InvalidSchema, 0u,
                 "Skeleton contract is invalid"); return false;
    }
    candidate.jointCount = static_cast<std::uint32_t>(jointCount);
    if (!StringArray(root, "materials", candidate.materials, identities, error)) return false;
    const JsonValue3D* lods = Field(root, "lods");
    if (!lods || lods->kind != JsonKind3D::Array || lods->array.empty()) {
        SetError(error, CharacterAssetProvenanceErrorCode3D::InvalidSchema, 0u,
                 "LOD collection is missing, empty, or has the wrong kind"); return false;
    }
    std::set<std::uint64_t> lodLevels;
    for (const JsonValue3D& value : lods->array) {
        std::string identity, source; std::uint64_t level{};
        if (!ExactKeys(value, {"identity", "level", "source"}) ||
            !StringField(value, "identity", identity) || !Identifier(identity) ||
            !identities.insert(identity).second || !NumberField(value, "level", level) ||
            !StringField(value, "source", source) || source != "cooked-aeskin" || level > 32u) {
            SetError(error, CharacterAssetProvenanceErrorCode3D::InvalidSchema, 0u, "LOD contract is invalid"); return false;
        }
        if (!lodLevels.insert(level).second) {
            SetError(error, CharacterAssetProvenanceErrorCode3D::DuplicateIdentity, 0u,
                     "LOD level is duplicated"); return false;
        }
        candidate.lods.push_back(identity);
    }
    std::uint64_t expectedLevel{};
    for (const std::uint64_t level : lodLevels) {
        if (level != expectedLevel++) {
            SetError(error, CharacterAssetProvenanceErrorCode3D::InvalidSchema, 0u,
                     "LOD levels must be contiguous from zero"); return false;
        }
    }
    const JsonValue3D* animations = Field(root, "animations");
    if (!animations || animations->kind != JsonKind3D::Array) {
        SetError(error, CharacterAssetProvenanceErrorCode3D::InvalidSchema, 0u,
                 "Animation collection is missing or has the wrong kind"); return false;
    }
    for (const JsonValue3D& value : animations->array) {
        std::string identity, source, clip;
        if (!ExactKeys(value, {"identity", "source", "clip"}) ||
            !StringField(value, "identity", identity) || !Identifier(identity) ||
            !identities.insert(identity).second || !StringField(value, "source", source) ||
            source != "authored" || !StringField(value, "clip", clip) || clip != identity) {
            SetError(error, CharacterAssetProvenanceErrorCode3D::InvalidSchema, 0u,
                     "Authored animation contract is invalid"); return false;
        }
        candidate.authoredAnimations.push_back(identity);
    }
    const JsonValue3D* fallbacks = Field(root, "runtimeFallbacks");
    if (!fallbacks || fallbacks->kind != JsonKind3D::Array || fallbacks->array.empty()) {
        SetError(error, CharacterAssetProvenanceErrorCode3D::InvalidSchema, 0u,
                 "Runtime fallback collection is missing, empty, or has the wrong kind"); return false;
    }
    for (const JsonValue3D& value : fallbacks->array) {
        std::string identity, kind; const JsonValue3D* authored = Field(value, "authored");
        if (!ExactKeys(value, {"identity", "kind", "authored"}) ||
            !StringField(value, "identity", identity) || !Identifier(identity) ||
            !identities.insert(identity).second || !StringField(value, "kind", kind) ||
            kind != "runtime-procedural" || !authored || authored->kind != JsonKind3D::Boolean || authored->boolean) {
            SetError(error, CharacterAssetProvenanceErrorCode3D::InvalidSchema, 0u,
                     "Runtime fallback must be explicit and non-authored"); return false;
        }
        candidate.runtimeFallbacks.push_back(identity);
    }
    const auto cooked = std::ranges::find_if(candidate.artifacts, [](const auto& artifact) {
        return artifact.role == "cooked-aeskin";
    });
    if (cooked == candidate.artifacts.end() ||
        candidate.canonicalRuntimePath != "assets/model-packs/" + candidate.assetId + '/' + cooked->path) {
        SetError(error, CharacterAssetProvenanceErrorCode3D::InvalidSchema, 0u,
                  "Cooked asset is not bound to canonical runtime path"); return false;
    }
    GltfEvidence3D gltfEvidence;
    SkinnedModelAssetData aeskinEvidence;
    if (!CollectGltfEvidence(sourceGltfContents, limits.maximumStringBytes,
                             limits.maximumCollectionEntries, gltfEvidence, error) ||
        !LoadAeskinEvidence(cookedAeskinContents, aeskinEvidence, error)) return false;
    std::vector<std::string> aeskinAnimations;
    for (const auto& clip : aeskinEvidence.clips) aeskinAnimations.push_back(clip.name);
    auto authored = candidate.authoredAnimations;
    std::ranges::sort(authored); std::ranges::sort(gltfEvidence.animationNames);
    std::ranges::sort(aeskinAnimations);
    if (authored != gltfEvidence.animationNames || authored != aeskinAnimations) {
        SetError(error, CharacterAssetProvenanceErrorCode3D::IntegrityMismatch, 0u,
                 "Authored animation set does not match sealed GLTF and AESKIN v2"); return false;
    }
    if (candidate.skeletonIdentity != gltfEvidence.skeletonIdentity ||
        candidate.jointCount != aeskinEvidence.model.skeleton.joints.size() ||
        gltfEvidence.jointNames != aeskinEvidence.jointNames) {
        SetError(error, CharacterAssetProvenanceErrorCode3D::IntegrityMismatch, 0u,
                 "Skeleton identity or joints do not match sealed GLTF and AESKIN v2"); return false;
    }
    if (candidate.materials != gltfEvidence.materialNames ||
        candidate.materials != aeskinEvidence.materialSlotNames) {
        SetError(error, CharacterAssetProvenanceErrorCode3D::IntegrityMismatch, 0u,
                 "Material identities do not match sealed GLTF and AESKIN v2"); return false;
    }
    if (candidate.lods.size() != 1u || lodLevels.size() != 1u || *lodLevels.begin() != 0u) {
        SetError(error, CharacterAssetProvenanceErrorCode3D::IntegrityMismatch, 0u,
                 "AESKIN v2 payload contains one non-LOD model and cannot prove extra LODs"); return false;
    }
    candidate.immutableCookedAeskin.resize(cookedAeskinContents.size());
    std::memcpy(candidate.immutableCookedAeskin.data(), cookedAeskinContents.data(),
                cookedAeskinContents.size());
    candidate.passed = true;
    output = std::move(candidate);
    return true;
}

} // namespace Aevrynd
