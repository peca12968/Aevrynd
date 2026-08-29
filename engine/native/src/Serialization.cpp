#include <Aevrynd/Serialization.h>
#include <Aevrynd/Mesh3D.h>
#include <charconv>
#include <cmath>
#include <limits>
#include <map>
#include <utility>
#include <variant>
#include <vector>

namespace Aevrynd::SerializationDetail {
struct ObjectStorage;
struct Value {
    using Array = std::vector<Value>;
    using Object = std::shared_ptr<ObjectStorage>;
    std::variant<std::nullptr_t, bool, std::string, Array, Object> data;
    bool number{};
};
struct ObjectStorage { std::map<std::string, Value, std::less<>> fields; };

static Value CloneValue(const Value& source);
static std::shared_ptr<ObjectStorage> CloneObject(const std::shared_ptr<ObjectStorage>& source) {
    auto result = std::make_shared<ObjectStorage>();
    if (source) for (const auto& [name, value] : source->fields) result->fields.emplace(name, CloneValue(value));
    return result;
}
static Value CloneValue(const Value& source) {
    Value result{source};
    if (const auto* array = std::get_if<Value::Array>(&source.data)) {
        Value::Array copy; copy.reserve(array->size());
        for (const Value& value : *array) copy.push_back(CloneValue(value));
        result.data = std::move(copy);
    } else if (const auto* object = std::get_if<Value::Object>(&source.data)) result.data = CloneObject(*object);
    return result;
}

static void SetError(SerializationError* error, SerializationErrorCode code, std::size_t offset, std::string message) {
    if (error) *error = {code, offset, std::move(message)};
}

static bool ValidUtf8(std::string_view text) {
    for (std::size_t index = 0; index < text.size();) {
        const unsigned char first = static_cast<unsigned char>(text[index]);
        if (first < 0x80) { ++index; continue; }
        std::size_t count{}; std::uint32_t code{};
        if ((first & 0xe0) == 0xc0) { count = 2; code = first & 0x1f; if (code < 2) return false; }
        else if ((first & 0xf0) == 0xe0) { count = 3; code = first & 0x0f; }
        else if ((first & 0xf8) == 0xf0) { count = 4; code = first & 0x07; }
        else return false;
        if (index + count > text.size()) return false;
        for (std::size_t part = 1; part < count; ++part) {
            const unsigned char byte = static_cast<unsigned char>(text[index + part]);
            if ((byte & 0xc0) != 0x80) return false;
            code = (code << 6) | (byte & 0x3f);
        }
        if ((count == 3 && code < 0x800) || (count == 4 && code < 0x10000) || code > 0x10ffff || (code >= 0xd800 && code <= 0xdfff)) return false;
        index += count;
    }
    return true;
}

static void AppendEscaped(std::string& output, std::string_view text) {
    static constexpr char Hex[] = "0123456789abcdef";
    output.push_back('"');
    for (const unsigned char value : text) {
        switch (value) {
        case '"': output += "\\\""; break;
        case '\\': output += "\\\\"; break;
        case '\b': output += "\\b"; break;
        case '\f': output += "\\f"; break;
        case '\n': output += "\\n"; break;
        case '\r': output += "\\r"; break;
        case '\t': output += "\\t"; break;
        default:
            if (value < 0x20) { output += "\\u00"; output.push_back(Hex[value >> 4]); output.push_back(Hex[value & 15]); }
            else output.push_back(static_cast<char>(value));
        }
    }
    output.push_back('"');
}

class Parser {
public:
    Parser(std::string_view input, const SerializationLimits& limits, SerializationError* error)
        : input_(input), limits_(limits), error_(error) {}

    bool Parse(Value& output) {
        SkipSpace();
        if (!ParseValue(output, 0)) return false;
        SkipSpace();
        if (position_ != input_.size()) return Fail(SerializationErrorCode::InvalidSyntax, "Unexpected trailing data");
        return true;
    }

private:
    bool ParseValue(Value& output, std::size_t depth) {
        if (depth > limits_.maxDepth) return Fail(SerializationErrorCode::LimitExceeded, "Maximum nesting depth exceeded");
        SkipSpace(); if (position_ >= input_.size()) return Fail(SerializationErrorCode::InvalidSyntax, "Unexpected end of input");
        const char token = input_[position_];
        if (token == '{') { Value::Object object; if (!ParseObject(object, depth + 1)) return false; output.data = std::move(object); return true; }
        if (token == '[') { Value::Array array; if (!ParseArray(array, depth + 1)) return false; output.data = std::move(array); return true; }
        if (token == '"') { std::string value; if (!ParseString(value)) return false; output.data = std::move(value); return true; }
        if (token == 't' && Consume("true")) { output.data = true; return true; }
        if (token == 'f' && Consume("false")) { output.data = false; return true; }
        if (token == 'n' && Consume("null")) { output.data = nullptr; return true; }
        std::string number;
        if (!ParseNumber(number)) return false;
        output.data = std::move(number); output.number = true; return true;
    }

    bool ParseObject(Value::Object& output, std::size_t depth) {
        ++position_; SkipSpace(); auto object = std::make_shared<ObjectStorage>();
        if (Take('}')) { output = std::move(object); return true; }
        for (;;) {
            std::string name; if (!ParseString(name)) return false;
            if (++fieldCount_ > limits_.maxFields) return Fail(SerializationErrorCode::LimitExceeded, "Maximum field count exceeded");
            SkipSpace(); if (!Take(':')) return Fail(SerializationErrorCode::InvalidSyntax, "Expected ':' after field name");
            Value value; if (!ParseValue(value, depth)) return false;
            if (!object->fields.emplace(std::move(name), std::move(value)).second) return Fail(SerializationErrorCode::DuplicateField, "Duplicate object field");
            SkipSpace(); if (Take('}')) break;
            if (!Take(',')) return Fail(SerializationErrorCode::InvalidSyntax, "Expected ',' or '}'");
            SkipSpace();
        }
        output = std::move(object); return true;
    }

    bool ParseArray(Value::Array& output, std::size_t depth) {
        ++position_; SkipSpace(); if (Take(']')) return true;
        for (;;) {
            Value value; if (!ParseValue(value, depth)) return false; output.push_back(std::move(value));
            if (output.size() > limits_.maxFields) return Fail(SerializationErrorCode::LimitExceeded, "Maximum array element count exceeded");
            SkipSpace(); if (Take(']')) return true;
            if (!Take(',')) return Fail(SerializationErrorCode::InvalidSyntax, "Expected ',' or ']'");
        }
    }

    bool ParseString(std::string& output) {
        SkipSpace(); if (!Take('"')) return Fail(SerializationErrorCode::InvalidSyntax, "Expected string");
        while (position_ < input_.size()) {
            const unsigned char value = static_cast<unsigned char>(input_[position_++]);
            if (value == '"') {
                if (output.size() > limits_.maxStringBytes) return Fail(SerializationErrorCode::LimitExceeded, "Maximum string size exceeded");
                if (!ValidUtf8(output)) return Fail(SerializationErrorCode::InvalidUtf8, "String is not valid UTF-8");
                return true;
            }
            if (value < 0x20) return Fail(SerializationErrorCode::InvalidSyntax, "Unescaped control character");
            if (value != '\\') output.push_back(static_cast<char>(value));
            else {
                if (position_ >= input_.size()) return Fail(SerializationErrorCode::InvalidSyntax, "Incomplete escape sequence");
                const char escaped = input_[position_++];
                switch (escaped) {
                case '"': output.push_back('"'); break; case '\\': output.push_back('\\'); break; case '/': output.push_back('/'); break;
                case 'b': output.push_back('\b'); break; case 'f': output.push_back('\f'); break; case 'n': output.push_back('\n'); break;
                case 'r': output.push_back('\r'); break; case 't': output.push_back('\t'); break;
                case 'u': { std::uint32_t code{}; if (!ParseHex(code)) return false; if (code >= 0xd800 && code <= 0xdbff) { if (!Consume("\\u")) return Fail(SerializationErrorCode::InvalidUtf8, "Missing low surrogate"); std::uint32_t low{}; if (!ParseHex(low) || low < 0xdc00 || low > 0xdfff) return Fail(SerializationErrorCode::InvalidUtf8, "Invalid low surrogate"); code = 0x10000 + ((code - 0xd800) << 10) + (low - 0xdc00); } else if (code >= 0xdc00 && code <= 0xdfff) return Fail(SerializationErrorCode::InvalidUtf8, "Unexpected low surrogate"); AppendUtf8(output, code); break; }
                default: return Fail(SerializationErrorCode::InvalidSyntax, "Invalid escape sequence");
                }
            }
            if (output.size() > limits_.maxStringBytes) return Fail(SerializationErrorCode::LimitExceeded, "Maximum string size exceeded");
        }
        return Fail(SerializationErrorCode::InvalidSyntax, "Unterminated string");
    }

    bool ParseNumber(std::string& output) {
        const std::size_t start = position_; if (Take('-') && position_ >= input_.size()) return Fail(SerializationErrorCode::InvalidSyntax, "Incomplete number");
        if (Take('0')) { if (position_ < input_.size() && input_[position_] >= '0' && input_[position_] <= '9') return Fail(SerializationErrorCode::InvalidSyntax, "Leading zero in number"); }
        else { const std::size_t digits = position_; while (position_ < input_.size() && input_[position_] >= '0' && input_[position_] <= '9') ++position_; if (position_ == digits) return Fail(SerializationErrorCode::InvalidSyntax, "Expected value"); }
        if (Take('.')) { const std::size_t digits = position_; while (position_ < input_.size() && input_[position_] >= '0' && input_[position_] <= '9') ++position_; if (position_ == digits) return Fail(SerializationErrorCode::InvalidSyntax, "Missing fraction digits"); }
        if (position_ < input_.size() && (input_[position_] == 'e' || input_[position_] == 'E')) { ++position_; if (position_ < input_.size() && (input_[position_] == '+' || input_[position_] == '-')) ++position_; const std::size_t digits = position_; while (position_ < input_.size() && input_[position_] >= '0' && input_[position_] <= '9') ++position_; if (position_ == digits) return Fail(SerializationErrorCode::InvalidSyntax, "Missing exponent digits"); }
        output.assign(input_.substr(start, position_ - start)); return true;
    }

    bool ParseHex(std::uint32_t& output) {
        if (position_ + 4 > input_.size()) return Fail(SerializationErrorCode::InvalidSyntax, "Incomplete Unicode escape");
        output = 0; for (int index = 0; index < 4; ++index) { const char value = input_[position_++]; output <<= 4; if (value >= '0' && value <= '9') output |= value - '0'; else if (value >= 'a' && value <= 'f') output |= value - 'a' + 10; else if (value >= 'A' && value <= 'F') output |= value - 'A' + 10; else return Fail(SerializationErrorCode::InvalidSyntax, "Invalid Unicode escape"); } return true;
    }
    static void AppendUtf8(std::string& output, std::uint32_t code) { if (code < 0x80) output.push_back(static_cast<char>(code)); else if (code < 0x800) { output.push_back(static_cast<char>(0xc0 | code >> 6)); output.push_back(static_cast<char>(0x80 | (code & 63))); } else if (code < 0x10000) { output.push_back(static_cast<char>(0xe0 | code >> 12)); output.push_back(static_cast<char>(0x80 | ((code >> 6) & 63))); output.push_back(static_cast<char>(0x80 | (code & 63))); } else { output.push_back(static_cast<char>(0xf0 | code >> 18)); output.push_back(static_cast<char>(0x80 | ((code >> 12) & 63))); output.push_back(static_cast<char>(0x80 | ((code >> 6) & 63))); output.push_back(static_cast<char>(0x80 | (code & 63))); } }
    void SkipSpace() { while (position_ < input_.size() && (input_[position_] == ' ' || input_[position_] == '\t' || input_[position_] == '\r' || input_[position_] == '\n')) ++position_; }
    bool Take(char value) { if (position_ < input_.size() && input_[position_] == value) { ++position_; return true; } return false; }
    bool Consume(std::string_view value) { if (input_.substr(position_, value.size()) != value) return false; position_ += value.size(); return true; }
    bool Fail(SerializationErrorCode code, std::string message) { SetError(error_, code, position_, std::move(message)); return false; }

    std::string_view input_; const SerializationLimits& limits_; SerializationError* error_{}; std::size_t position_{}, fieldCount_{};
};

static const Value* Find(const std::shared_ptr<ObjectStorage>& storage, std::string_view name) {
    if (!storage) return nullptr; const auto found = storage->fields.find(name); return found == storage->fields.end() ? nullptr : &found->second;
}
static Value Number(std::string value) { Value result; result.data = std::move(value); result.number = true; return result; }
static Value Array(std::initializer_list<float> values) { Value::Array array; array.reserve(values.size()); for (float value : values) { char buffer[64]; const auto converted = std::to_chars(buffer, buffer + sizeof(buffer), value, std::chars_format::general, std::numeric_limits<float>::max_digits10); array.push_back(Number(std::string(buffer, converted.ptr))); } Value result; result.data = std::move(array); return result; }
static bool ValidName(std::string_view name) { return !name.empty() && ValidUtf8(name); }

template<class Number> static bool ParseInteger(const Value& value, Number& output) {
    if (!value.number) return false; const auto* text = std::get_if<std::string>(&value.data); if (!text || text->find_first_of(".eE") != std::string::npos) return false;
    Number temporary{}; const auto parsed = std::from_chars(text->data(), text->data() + text->size(), temporary); if (parsed.ec != std::errc{} || parsed.ptr != text->data() + text->size()) return false; output = temporary; return true;
}
template<class Number> static bool ParseFloat(const Value& value, Number& output) {
    if (!value.number) return false; const auto* text = std::get_if<std::string>(&value.data); if (!text) return false;
    Number temporary{}; const auto parsed = std::from_chars(text->data(), text->data() + text->size(), temporary, std::chars_format::general); if (parsed.ec != std::errc{} || parsed.ptr != text->data() + text->size() || !std::isfinite(temporary)) return false; output = temporary; return true;
}
static bool ReadFloatArray(const Value& value, float* output, std::size_t count) {
    const auto* array = std::get_if<Value::Array>(&value.data); if (!array || array->size() != count) return false;
    std::vector<float> temporary(count); for (std::size_t index = 0; index < count; ++index) if (!ParseFloat((*array)[index], temporary[index])) return false;
    for (std::size_t index = 0; index < count; ++index) output[index] = temporary[index]; return true;
}

static bool EncodeValue(const Value& value, std::string& output, const SerializationLimits& limits, std::size_t depth, std::size_t& fields, SerializationError* error);
static bool EncodeObject(const ObjectStorage& object, std::string& output, const SerializationLimits& limits, std::size_t depth, std::size_t& fields, SerializationError* error) {
    if (depth > limits.maxDepth) { SetError(error, SerializationErrorCode::LimitExceeded, output.size(), "Maximum nesting depth exceeded"); return false; }
    output.push_back('{'); bool first = true;
    for (const auto& [name, value] : object.fields) {
        if (++fields > limits.maxFields || name.size() > limits.maxStringBytes) { SetError(error, SerializationErrorCode::LimitExceeded, output.size(), "Object field limit exceeded"); return false; }
        if (!ValidUtf8(name)) { SetError(error, SerializationErrorCode::InvalidUtf8, output.size(), "Object field name is not valid UTF-8"); return false; }
        if (!first) output.push_back(','); first = false; AppendEscaped(output, name); output.push_back(':'); if (!EncodeValue(value, output, limits, depth + 1, fields, error)) return false;
        if (output.size() > limits.maxDocumentBytes) { SetError(error, SerializationErrorCode::DocumentTooLarge, output.size(), "Document exceeds size limit"); return false; }
    }
    output.push_back('}'); return true;
}
static bool EncodeValue(const Value& value, std::string& output, const SerializationLimits& limits, std::size_t depth, std::size_t& fields, SerializationError* error) {
    if (depth > limits.maxDepth) { SetError(error, SerializationErrorCode::LimitExceeded, output.size(), "Maximum nesting depth exceeded"); return false; }
    if (value.number) { output += std::get<std::string>(value.data); return true; }
    if (std::holds_alternative<std::nullptr_t>(value.data)) output += "null";
    else if (const auto* boolean = std::get_if<bool>(&value.data)) output += *boolean ? "true" : "false";
    else if (const auto* string = std::get_if<std::string>(&value.data)) { if (string->size() > limits.maxStringBytes) { SetError(error, SerializationErrorCode::LimitExceeded, output.size(), "String exceeds limit"); return false; } if (!ValidUtf8(*string)) { SetError(error, SerializationErrorCode::InvalidUtf8, output.size(), "String is not valid UTF-8"); return false; } AppendEscaped(output, *string); }
    else if (const auto* array = std::get_if<Value::Array>(&value.data)) { if (array->size() > limits.maxFields) { SetError(error, SerializationErrorCode::LimitExceeded, output.size(), "Array exceeds limit"); return false; } output.push_back('['); for (std::size_t index = 0; index < array->size(); ++index) { if (index) output.push_back(','); if (!EncodeValue((*array)[index], output, limits, depth + 1, fields, error)) return false; } output.push_back(']'); }
    else return EncodeObject(**std::get_if<Value::Object>(&value.data), output, limits, depth, fields, error);
    return true;
}
}

namespace Aevrynd {
using namespace SerializationDetail;

SerializationObject::SerializationObject() : storage_(std::make_shared<ObjectStorage>()) {}
SerializationObject::SerializationObject(std::shared_ptr<ObjectStorage> storage) : storage_(std::move(storage)) {}
SerializationObject::SerializationObject(const SerializationObject& other) : storage_(CloneObject(other.storage_)) {}
SerializationObject& SerializationObject::operator=(const SerializationObject& other) { if (this != &other) storage_ = CloneObject(other.storage_); return *this; }
SerializationObject::SerializationObject(SerializationObject&& other) { auto replacement=std::make_shared<ObjectStorage>();storage_=std::move(other.storage_);if(!storage_)storage_=std::make_shared<ObjectStorage>();other.storage_=std::move(replacement); }
SerializationObject& SerializationObject::operator=(SerializationObject&& other) { if(this!=&other){auto replacement=std::make_shared<ObjectStorage>();storage_=std::move(other.storage_);if(!storage_)storage_=std::make_shared<ObjectStorage>();other.storage_=std::move(replacement);}return *this; }

bool SerializationObject::Write(std::string_view name, bool value) { if (!ValidName(name)) return false; Value stored; stored.data = value; storage_->fields[std::string(name)] = std::move(stored); return true; }
#define AEVRYND_WRITE_INTEGER(type) bool SerializationObject::Write(std::string_view name, type value) { if (!ValidName(name)) return false; char buffer[32]; const auto converted = std::to_chars(buffer, buffer + sizeof(buffer), value); if (converted.ec != std::errc{}) return false; storage_->fields[std::string(name)] = Number(std::string(buffer, converted.ptr)); return true; }
AEVRYND_WRITE_INTEGER(std::int32_t)
AEVRYND_WRITE_INTEGER(std::uint32_t)
AEVRYND_WRITE_INTEGER(std::int64_t)
AEVRYND_WRITE_INTEGER(std::uint64_t)
#undef AEVRYND_WRITE_INTEGER
bool SerializationObject::Write(std::string_view name, float value) { if (!ValidName(name) || !std::isfinite(value)) return false; char buffer[64]; const auto converted = std::to_chars(buffer, buffer + sizeof(buffer), value, std::chars_format::general, std::numeric_limits<float>::max_digits10);if(converted.ec!=std::errc{})return false; storage_->fields[std::string(name)] = Number(std::string(buffer, converted.ptr)); return true; }
bool SerializationObject::Write(std::string_view name, double value) { if (!ValidName(name) || !std::isfinite(value)) return false; char buffer[64]; const auto converted = std::to_chars(buffer, buffer + sizeof(buffer), value, std::chars_format::general, std::numeric_limits<double>::max_digits10);if(converted.ec!=std::errc{})return false; storage_->fields[std::string(name)] = Number(std::string(buffer, converted.ptr)); return true; }
bool SerializationObject::Write(std::string_view name, std::string_view value) { if (!ValidName(name) || !ValidUtf8(value)) return false; Value stored; stored.data = std::string(value); storage_->fields[std::string(name)] = std::move(stored); return true; }
bool SerializationObject::Write(std::string_view name, Vec2 value) { if (!ValidName(name) || !std::isfinite(value.x) || !std::isfinite(value.y)) return false; storage_->fields[std::string(name)] = Array({value.x, value.y}); return true; }
bool SerializationObject::Write(std::string_view name, Vec3 value) { if (!ValidName(name) || !std::isfinite(value.x) || !std::isfinite(value.y) || !std::isfinite(value.z)) return false; storage_->fields[std::string(name)] = Array({value.x, value.y, value.z}); return true; }
bool SerializationObject::Write(std::string_view name, const Transform3D& value) { SerializationObject transform; return transform.Write("position", value.position) && transform.Write("rotation", value.rotation) && transform.Write("scale", value.scale) && Write(name, transform); }
bool SerializationObject::Write(std::string_view name,const GlobalPosition3D& value){GlobalPosition3D normalized=value;if(!NormalizeGlobalPosition3D(normalized))return false;SerializationObject position;return position.Write("wireVersion",std::uint32_t{1})&&position.Write("chunkX",normalized.chunk.x)&&position.Write("chunkY",normalized.chunk.y)&&position.Write("chunkZ",normalized.chunk.z)&&position.Write("offset",normalized.offset)&&Write(name,position);}
bool SerializationObject::Write(std::string_view name, const SerializationObject& value) { if (!ValidName(name)) return false; Value stored; stored.data = CloneObject(value.storage_); storage_->fields[std::string(name)] = std::move(stored); return true; }

bool SerializationObject::Read(std::string_view name, bool& value) const { const Value* found = Find(storage_, name); if (!found) return true; const auto* typed = std::get_if<bool>(&found->data); if (!typed || found->number) return false; value = *typed; return true; }
#define AEVRYND_READ_INTEGER(type) bool SerializationObject::Read(std::string_view name, type& value) const { const Value* found = Find(storage_, name); if (!found) return true; type temporary{}; if (!ParseInteger(*found, temporary)) return false; value = temporary; return true; }
AEVRYND_READ_INTEGER(std::int32_t)
AEVRYND_READ_INTEGER(std::uint32_t)
AEVRYND_READ_INTEGER(std::int64_t)
AEVRYND_READ_INTEGER(std::uint64_t)
#undef AEVRYND_READ_INTEGER
bool SerializationObject::Read(std::string_view name, float& value) const { const Value* found = Find(storage_, name); if (!found) return true; float temporary{}; if (!ParseFloat(*found, temporary)) return false; value = temporary; return true; }
bool SerializationObject::Read(std::string_view name, double& value) const { const Value* found = Find(storage_, name); if (!found) return true; double temporary{}; if (!ParseFloat(*found, temporary)) return false; value = temporary; return true; }
bool SerializationObject::Read(std::string_view name, std::string& value) const { const Value* found = Find(storage_, name); if (!found) return true; const auto* typed = std::get_if<std::string>(&found->data); if (!typed || found->number) return false; value = *typed; return true; }
bool SerializationObject::Read(std::string_view name, Vec2& value) const { const Value* found = Find(storage_, name); if (!found) return true; float values[2]; if (!ReadFloatArray(*found, values, 2)) return false; value = {values[0], values[1]}; return true; }
bool SerializationObject::Read(std::string_view name, Vec3& value) const { const Value* found = Find(storage_, name); if (!found) return true; float values[3]; if (!ReadFloatArray(*found, values, 3)) return false; value = {values[0], values[1], values[2]}; return true; }
bool SerializationObject::Read(std::string_view name, Transform3D& value) const { const Value* found = Find(storage_, name); if (!found) return true; const auto* object = std::get_if<Value::Object>(&found->data); if (!object) return false; SerializationObject view(CloneObject(*object)); Transform3D temporary = value; if (!view.Read("position", temporary.position) || !view.Read("rotation", temporary.rotation) || !view.Read("scale", temporary.scale)) return false; value = temporary; return true; }
bool SerializationObject::Read(std::string_view name,GlobalPosition3D& value)const{const Value* found=Find(storage_,name);if(!found)return true;const auto* object=std::get_if<Value::Object>(&found->data);if(!object)return false;SerializationObject view(CloneObject(*object));std::uint32_t version{};GlobalPosition3D temporary=value;if(!view.Read("wireVersion",version)||version!=1||!view.Read("chunkX",temporary.chunk.x)||!view.Read("chunkY",temporary.chunk.y)||!view.Read("chunkZ",temporary.chunk.z)||!view.Read("offset",temporary.offset)||!NormalizeGlobalPosition3D(temporary))return false;value=temporary;return true;}
bool SerializationObject::Read(std::string_view name, SerializationObject& value) const { const Value* found = Find(storage_, name); if (!found) return true; const auto* object = std::get_if<Value::Object>(&found->data); if (!object) return false; value = SerializationObject(CloneObject(*object)); return true; }
bool SerializationObject::Contains(std::string_view name) const { return Find(storage_, name) != nullptr; }
std::size_t SerializationObject::FieldCount() const { return storage_ ? storage_->fields.size() : 0; }
void SerializationObject::Clear() { storage_ = std::make_shared<ObjectStorage>(); }

bool SerializationDocument::Encode(std::string& output, SerializationError* error, const SerializationLimits& limits) const {
    if (error) *error = {}; if (version == 0 || version > CurrentVersion) { SetError(error, SerializationErrorCode::UnsupportedVersion, 0, "Document version is not supported"); return false; }if(limits.maxFields<3){SetError(error,SerializationErrorCode::LimitExceeded,0,"Document envelope exceeds field limit");return false;} std::string temporary; temporary.reserve(256); temporary += "{\"format\":\"aevrynd.serialization\",\"version\":"; char versionBuffer[16]; const auto converted = std::to_chars(versionBuffer, versionBuffer + sizeof(versionBuffer), version);if(converted.ec!=std::errc{}){SetError(error,SerializationErrorCode::NumericOverflow,temporary.size(),"Version conversion failed");return false;} temporary.append(versionBuffer, converted.ptr); temporary += ",\"root\":"; std::size_t fields{3}; if (!root.storage_||!EncodeObject(*root.storage_, temporary, limits, 1, fields, error)) {if(!root.storage_)SetError(error,SerializationErrorCode::InvalidEnvelope,temporary.size(),"Root object is invalid");return false;} temporary.push_back('}'); if (temporary.size() > limits.maxDocumentBytes) { SetError(error, SerializationErrorCode::DocumentTooLarge, temporary.size(), "Document exceeds size limit"); return false; } output = std::move(temporary); return true;
}

bool SerializationDocument::Decode(std::string_view input, SerializationDocument& output, SerializationError* error, const SerializationLimits& limits) {
    if (error) *error = {}; if (input.empty()) { SetError(error, SerializationErrorCode::EmptyInput, 0, "Document is empty"); return false; } if (input.size() > limits.maxDocumentBytes) { SetError(error, SerializationErrorCode::DocumentTooLarge, input.size(), "Document exceeds size limit"); return false; }
    Value envelope; Parser parser(input, limits, error); if (!parser.Parse(envelope)) return false; const auto* object = std::get_if<Value::Object>(&envelope.data); if (!object) { SetError(error, SerializationErrorCode::InvalidEnvelope, 0, "Document envelope must be an object"); return false; }
    const Value* format = Find(*object, "format"), *version = Find(*object, "version"), *root = Find(*object, "root"); const auto* formatText = format ? std::get_if<std::string>(&format->data) : nullptr; const auto* rootObject = root ? std::get_if<Value::Object>(&root->data) : nullptr; std::uint32_t parsedVersion{};
    if (!formatText || format->number || *formatText != "aevrynd.serialization" || !version || !ParseInteger(*version, parsedVersion) || !rootObject) { SetError(error, SerializationErrorCode::InvalidEnvelope, 0, "Document envelope is invalid"); return false; }
    if (parsedVersion == 0 || parsedVersion > CurrentVersion) { SetError(error, SerializationErrorCode::UnsupportedVersion, 0, "Document version is not supported"); return false; }
    SerializationDocument temporary; temporary.version = parsedVersion; temporary.root = SerializationObject(CloneObject(*rootObject)); output = std::move(temporary); return true;
}
}
