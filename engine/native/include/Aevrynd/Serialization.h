#pragma once

#include "Math.h"
#include "WorldOrigin3D.h"
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>

namespace Aevrynd {
struct Transform3D;

struct SerializationLimits {
    std::size_t maxDocumentBytes{1024 * 1024};
    std::size_t maxStringBytes{64 * 1024};
    // 欄位上限包含固定的三個文件封裝欄位，深度從文件封裝的根值開始計算。
    std::size_t maxFields{4096};
    std::size_t maxDepth{16};
};

enum class SerializationErrorCode {
    None,
    EmptyInput,
    DocumentTooLarge,
    InvalidSyntax,
    InvalidUtf8,
    InvalidEnvelope,
    UnsupportedVersion,
    DuplicateField,
    LimitExceeded,
    TypeMismatch,
    NumericOverflow
};

struct SerializationError {
    SerializationErrorCode code{SerializationErrorCode::None};
    std::size_t offset{};
    std::string message;

    explicit operator bool() const { return code != SerializationErrorCode::None; }
};

namespace SerializationDetail { struct ObjectStorage; }

class SerializationObject {
public:
    SerializationObject();
    SerializationObject(const SerializationObject& other);
    SerializationObject& operator=(const SerializationObject& other);
    SerializationObject(SerializationObject&& other);
    SerializationObject& operator=(SerializationObject&& other);
    ~SerializationObject() = default;

    bool Write(std::string_view name, bool value);
    bool Write(std::string_view name, std::int32_t value);
    bool Write(std::string_view name, std::uint32_t value);
    bool Write(std::string_view name, std::int64_t value);
    bool Write(std::string_view name, std::uint64_t value);
    bool Write(std::string_view name, float value);
    bool Write(std::string_view name, double value);
    bool Write(std::string_view name, std::string_view value);
    bool Write(std::string_view name, const char* value) { return value && Write(name, std::string_view(value)); }
    bool Write(std::string_view name, Vec2 value);
    bool Write(std::string_view name, Vec3 value);
    bool Write(std::string_view name, const Transform3D& value);
    bool Write(std::string_view name,const GlobalPosition3D& value);
    bool Write(std::string_view name, const SerializationObject& value);

    // 缺少欄位時保留呼叫端既有值，讓新舊版本可安全共用預設值。
    bool Read(std::string_view name, bool& value) const;
    bool Read(std::string_view name, std::int32_t& value) const;
    bool Read(std::string_view name, std::uint32_t& value) const;
    bool Read(std::string_view name, std::int64_t& value) const;
    bool Read(std::string_view name, std::uint64_t& value) const;
    bool Read(std::string_view name, float& value) const;
    bool Read(std::string_view name, double& value) const;
    bool Read(std::string_view name, std::string& value) const;
    bool Read(std::string_view name, Vec2& value) const;
    bool Read(std::string_view name, Vec3& value) const;
    bool Read(std::string_view name, Transform3D& value) const;
    bool Read(std::string_view name,GlobalPosition3D& value)const;
    bool Read(std::string_view name, SerializationObject& value) const;

    bool Contains(std::string_view name) const;
    std::size_t FieldCount() const;
    void Clear();

private:
    explicit SerializationObject(std::shared_ptr<SerializationDetail::ObjectStorage> storage);
    std::shared_ptr<SerializationDetail::ObjectStorage> storage_;
    friend class SerializationDocument;
};

class SerializationDocument {
public:
    static constexpr std::uint32_t CurrentVersion = 1;

    std::uint32_t version{CurrentVersion};
    SerializationObject root;

    bool Encode(std::string& output, SerializationError* error = nullptr,
        const SerializationLimits& limits = {}) const;
    static bool Decode(std::string_view input, SerializationDocument& output,
        SerializationError* error = nullptr, const SerializationLimits& limits = {});
};
}
