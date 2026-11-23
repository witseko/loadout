#pragma once

/// Zero-dependency RFC 8259 JSON and JSONC DOM, parser, and serializer.

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace loadout::util {

/// Discriminator for values stored inside JsonValue.
enum class JsonType {
    Null,
    Bool,
    Int,
    Double,
    String,
    Array,
    Object,
};

/// Value-semantic JSON DOM node with insertion-ordered object storage.
class JsonValue {
public:
    using Array = std::vector<JsonValue>;
    using Object = std::vector<std::pair<std::string, JsonValue>>;

    JsonValue() noexcept = default;
    JsonValue(std::nullptr_t) noexcept : data_(std::monostate{}) {}
    JsonValue(bool v) noexcept : data_(v) {}

    template <std::integral T>
        requires (!std::same_as<std::remove_cvref_t<T>, bool>)
    JsonValue(T v) noexcept : data_(static_cast<int64_t>(v)) {}

    JsonValue(double v) noexcept : data_(v) {}
    JsonValue(const char* v) : data_(v ? std::string(v) : std::string()) {}
    JsonValue(std::string v) noexcept : data_(std::move(v)) {}
    JsonValue(std::string_view v) : data_(std::string(v)) {}
    JsonValue(Array v) noexcept : data_(std::move(v)) {}
    JsonValue(Object v) noexcept : data_(std::move(v)) {}

    /// Constructs a JSON null value.
    [[nodiscard]] static JsonValue make_null() noexcept { return JsonValue(); }
    /// Constructs a JSON array value from the given elements.
    [[nodiscard]] static JsonValue array(Array items = {}) noexcept { return JsonValue(std::move(items)); }
    /// Constructs a JSON object value from the given key-value pairs.
    [[nodiscard]] static JsonValue object(Object entries = {}) noexcept { return JsonValue(std::move(entries)); }

    [[nodiscard]] JsonType type() const noexcept;
    [[nodiscard]] bool is_null() const noexcept { return std::holds_alternative<std::monostate>(data_); }
    [[nodiscard]] bool is_bool() const noexcept { return std::holds_alternative<bool>(data_); }
    [[nodiscard]] bool is_int() const noexcept { return std::holds_alternative<int64_t>(data_); }
    [[nodiscard]] bool is_double() const noexcept { return std::holds_alternative<double>(data_); }
    [[nodiscard]] bool is_number() const noexcept { return is_int() || is_double(); }
    [[nodiscard]] bool is_string() const noexcept { return std::holds_alternative<std::string>(data_); }
    [[nodiscard]] bool is_array() const noexcept { return std::holds_alternative<Array>(data_); }
    [[nodiscard]] bool is_object() const noexcept { return std::holds_alternative<Object>(data_); }

    [[nodiscard]] bool as_bool(bool def = false) const noexcept;
    [[nodiscard]] int64_t as_int(int64_t def = 0) const noexcept;
    [[nodiscard]] double as_double(double def = 0.0) const noexcept;
    [[nodiscard]] const std::string& as_string() const noexcept;
    [[nodiscard]] std::string as_string_or(std::string_view def) const;
    [[nodiscard]] const Array& as_array() const noexcept;
    Array& as_array();
    [[nodiscard]] const Object& as_object() const noexcept;
    Object& as_object();

    [[nodiscard]] size_t size() const noexcept;
    [[nodiscard]] bool empty() const noexcept;

    [[nodiscard]] bool contains(std::string_view key) const noexcept;
    [[nodiscard]] const JsonValue* find(std::string_view key) const noexcept;
    [[nodiscard]] JsonValue* find(std::string_view key) noexcept;

    [[nodiscard]] std::string get_str(std::string_view key, std::string_view def = "") const;
    [[nodiscard]] bool get_bool(std::string_view key, bool def = false) const noexcept;
    [[nodiscard]] int64_t get_int(std::string_view key, int64_t def = 0) const noexcept;
    [[nodiscard]] double get_double(std::string_view key, double def = 0.0) const noexcept;

    [[nodiscard]] const JsonValue& operator[](std::string_view key) const noexcept;
    JsonValue& operator[](std::string_view key);

    [[nodiscard]] const JsonValue& operator[](size_t index) const noexcept;
    JsonValue& operator[](size_t index);

    void push_back(JsonValue val);
    void set(std::string key, JsonValue val);
    bool erase(std::string_view key);

    [[nodiscard]] bool operator==(const JsonValue& other) const noexcept;

    /// Returns a shared immutable null JsonValue sentinel.
    [[nodiscard]] static const JsonValue& null_value() noexcept;

private:
    using Storage = std::variant<
        std::monostate,
        bool,
        int64_t,
        double,
        std::string,
        Array,
        Object
    >;

    Storage data_{std::monostate{}};
};

/// Result returned by parse_json_ex and parse_jsonc_ex with byte error location.
struct JsonParseResult {
    std::optional<JsonValue> value;
    std::string error;
    size_t error_offset = 0;

    [[nodiscard]] explicit operator bool() const noexcept { return value.has_value(); }
};

/// Parses strict RFC 8259 JSON and returns either the parsed value or an error message and byte offset.
[[nodiscard]] JsonParseResult parse_json_ex(std::string_view input);

/// Parses JSON with // comments, /* */ comments, and trailing commas, returning a value or error details.
[[nodiscard]] JsonParseResult parse_jsonc_ex(std::string_view input);

/// Parses strict RFC 8259 JSON and returns std::nullopt on syntax error.
[[nodiscard]] std::optional<JsonValue> parse_json(std::string_view input);

/// Parses JSONC (comments and trailing commas) and returns std::nullopt on syntax error.
[[nodiscard]] std::optional<JsonValue> parse_jsonc(std::string_view input);

/// Serializes a JsonValue to compact or pretty-printed RFC 8259 JSON text.
[[nodiscard]] std::string to_json(const JsonValue& val, bool pretty = false, int indent_spaces = 2);

} // namespace loadout::util
