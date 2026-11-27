#include "util/json.hpp"

#include <charconv>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <tuple>

namespace singlet::util {

namespace {

const std::string kEmptyString{};
const JsonValue::Array kEmptyArray{};
const JsonValue::Object kEmptyObject{};

void append_utf8(std::string& out, uint32_t cp) {
    if (cp <= 0x7Fu) {
        out.push_back(static_cast<char>(cp));
    } else if (cp <= 0x7FFu) {
        out.push_back(static_cast<char>(0xC0u | (cp >> 6)));
        out.push_back(static_cast<char>(0x80u | (cp & 0x3Fu)));
    } else if (cp <= 0xFFFFu) {
        out.push_back(static_cast<char>(0xE0u | (cp >> 12)));
        out.push_back(static_cast<char>(0x80u | ((cp >> 6) & 0x3Fu)));
        out.push_back(static_cast<char>(0x80u | (cp & 0x3Fu)));
    } else if (cp <= 0x10FFFFu) {
        out.push_back(static_cast<char>(0xF0u | (cp >> 18)));
        out.push_back(static_cast<char>(0x80u | ((cp >> 12) & 0x3Fu)));
        out.push_back(static_cast<char>(0x80u | ((cp >> 6) & 0x3Fu)));
        out.push_back(static_cast<char>(0x80u | (cp & 0x3Fu)));
    }
}

int hex_digit(char c) noexcept {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return 10 + (c - 'a');
    if (c >= 'A' && c <= 'F') return 10 + (c - 'A');
    return -1;
}

class JsonParser {
public:
    JsonParser(std::string_view input, bool jsonc) noexcept
        : input_(input), jsonc_(jsonc) {}

    JsonParseResult parse() {
        // Skip optional UTF-8 BOM.
        if (input_.size() >= 3 &&
            static_cast<unsigned char>(input_[0]) == 0xEF &&
            static_cast<unsigned char>(input_[1]) == 0xBB &&
            static_cast<unsigned char>(input_[2]) == 0xBF) {
            pos_ = 3;
        }

        if (!skip_ws()) {
            return make_error();
        }
        if (eof()) {
            return fail("empty JSON input");
        }

        auto val = parse_value(0);
        if (!val) {
            return make_error();
        }

        if (!skip_ws()) {
            return make_error();
        }
        if (!eof()) {
            return fail("unexpected trailing characters after JSON value");
        }

        JsonParseResult res;
        res.value = std::move(*val);
        return res;
    }

private:
    static constexpr int kMaxDepth = 512;

    bool eof() const noexcept { return pos_ >= input_.size(); }
    char peek() const noexcept { return pos_ < input_.size() ? input_[pos_] : '\0'; }
    char get() noexcept { return pos_ < input_.size() ? input_[pos_++] : '\0'; }

    JsonParseResult fail(std::string msg) {
        if (error_.empty()) {
            error_ = std::move(msg);
            error_offset_ = pos_;
        }
        return make_error();
    }

    std::nullopt_t set_error(std::string msg) {
        if (error_.empty()) {
            error_ = std::move(msg);
            error_offset_ = pos_;
        }
        return std::nullopt;
    }

    JsonParseResult make_error() const {
        JsonParseResult res;
        res.error = error_;
        res.error_offset = error_offset_;
        return res;
    }

    bool skip_ws() {
        while (!eof()) {
            char c = peek();
            if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
                ++pos_;
                continue;
            }
            if (jsonc_ && c == '/' && pos_ + 1 < input_.size()) {
                char next = input_[pos_ + 1];
                if (next == '/') {
                    pos_ += 2;
                    while (!eof() && peek() != '\n') {
                        ++pos_;
                    }
                    continue;
                }
                if (next == '*') {
                    size_t start = pos_;
                    pos_ += 2;
                    bool closed = false;
                    while (pos_ + 1 < input_.size()) {
                        if (input_[pos_] == '*' && input_[pos_ + 1] == '/') {
                            pos_ += 2;
                            closed = true;
                            break;
                        }
                        ++pos_;
                    }
                    if (!closed) {
                        error_ = "unterminated block comment";
                        error_offset_ = start;
                        return false;
                    }
                    continue;
                }
            }
            break;
        }
        return true;
    }

    std::optional<JsonValue> parse_value(int depth) {
        if (depth > kMaxDepth) {
            return set_error("maximum nesting depth exceeded");
        }
        if (!skip_ws()) {
            return std::nullopt;
        }
        if (eof()) {
            return set_error("unexpected end of input");
        }

        char c = peek();
        if (c == 'n') return parse_null();
        if (c == 't' || c == 'f') return parse_bool();
        if (c == '"') {
            auto s = parse_string();
            if (!s) return std::nullopt;
            return JsonValue(std::move(*s));
        }
        if (c == '[') return parse_array(depth + 1);
        if (c == '{') return parse_object(depth + 1);
        if (c == '-' || (c >= '0' && c <= '9')) return parse_number();

        return set_error(std::string("unexpected character '") + c + "'");
    }

    std::optional<JsonValue> parse_null() {
        if (input_.substr(pos_, 4) == "null") {
            pos_ += 4;
            return JsonValue(nullptr);
        }
        return set_error("invalid literal (expected 'null')");
    }

    std::optional<JsonValue> parse_bool() {
        if (input_.substr(pos_, 4) == "true") {
            pos_ += 4;
            return JsonValue(true);
        }
        if (input_.substr(pos_, 5) == "false") {
            pos_ += 5;
            return JsonValue(false);
        }
        return set_error("invalid boolean literal");
    }

    std::optional<uint32_t> parse_hex4() {
        if (pos_ + 4 > input_.size()) {
            set_error("incomplete \\uXXXX escape sequence");
            return std::nullopt;
        }
        uint32_t value = 0;
        for (int i = 0; i < 4; ++i) {
            int d = hex_digit(input_[pos_ + static_cast<size_t>(i)]);
            if (d < 0) {
                set_error("invalid hex digit in \\uXXXX escape sequence");
                return std::nullopt;
            }
            value = (value << 4) | static_cast<uint32_t>(d);
        }
        pos_ += 4;
        return value;
    }

    std::optional<std::string> parse_string() {
        if (get() != '"') {
            return set_error("expected '\"'");
        }

        std::string out;
        while (!eof()) {
            char c = get();
            if (c == '"') {
                return out;
            }
            if (static_cast<unsigned char>(c) < 0x20u) {
                return set_error("unescaped control character in string");
            }
            if (c == '\\') {
                if (eof()) {
                    return set_error("unterminated escape sequence");
                }
                char esc = get();
                switch (esc) {
                    case '"':  out.push_back('"'); break;
                    case '\\': out.push_back('\\'); break;
                    case '/':  out.push_back('/'); break;
                    case 'b':  out.push_back('\b'); break;
                    case 'f':  out.push_back('\f'); break;
                    case 'n':  out.push_back('\n'); break;
                    case 'r':  out.push_back('\r'); break;
                    case 't':  out.push_back('\t'); break;
                    case 'u': {
                        auto u1 = parse_hex4();
                        if (!u1) return std::nullopt;
                        uint32_t cp = *u1;
                        if (cp >= 0xD800u && cp <= 0xDBFFu) {
                            if (pos_ + 2 > input_.size() || input_[pos_] != '\\' || input_[pos_ + 1] != 'u') {
                                return set_error("unpaired high UTF-16 surrogate");
                            }
                            pos_ += 2;
                            auto u2 = parse_hex4();
                            if (!u2) return std::nullopt;
                            if (*u2 < 0xDC00u || *u2 > 0xDFFFu) {
                                return set_error("invalid low UTF-16 surrogate");
                            }
                            cp = 0x10000u + (((cp - 0xD800u) << 10) | (*u2 - 0xDC00u));
                        } else if (cp >= 0xDC00u && cp <= 0xDFFFu) {
                            return set_error("unpaired low UTF-16 surrogate");
                        }
                        append_utf8(out, cp);
                        break;
                    }
                    default:
                        return set_error(std::string("invalid escape sequence '\\") + esc + "'");
                }
            } else {
                out.push_back(c);
            }
        }
        return set_error("unterminated string");
    }

    std::optional<JsonValue> parse_number() {
        size_t start = pos_;
        bool is_float = false;

        if (peek() == '-') {
            ++pos_;
        }
        if (eof()) {
            return set_error("invalid number");
        }

        if (peek() == '0') {
            ++pos_;
            if (!eof() && peek() >= '0' && peek() <= '9') {
                return set_error("leading zeros are not allowed in JSON numbers");
            }
        } else if (peek() >= '1' && peek() <= '9') {
            while (!eof() && peek() >= '0' && peek() <= '9') {
                ++pos_;
            }
        } else {
            return set_error("expected digit in number");
        }

        if (!eof() && peek() == '.') {
            is_float = true;
            ++pos_;
            if (eof() || peek() < '0' || peek() > '9') {
                return set_error("expected digit after decimal point");
            }
            while (!eof() && peek() >= '0' && peek() <= '9') {
                ++pos_;
            }
        }

        if (!eof() && (peek() == 'e' || peek() == 'E')) {
            is_float = true;
            ++pos_;
            if (!eof() && (peek() == '+' || peek() == '-')) {
                ++pos_;
            }
            if (eof() || peek() < '0' || peek() > '9') {
                return set_error("expected digit in exponent");
            }
            while (!eof() && peek() >= '0' && peek() <= '9') {
                ++pos_;
            }
        }

        std::string_view token = input_.substr(start, pos_ - start);
        if (!is_float) {
            int64_t ival = 0;
            auto [ptr, ec] = std::from_chars(token.data(), token.data() + token.size(), ival);
            if (ec == std::errc{} && ptr == token.data() + token.size()) {
                return JsonValue(ival);
            }
        }

        std::string buf(token);
        char* endptr = nullptr;
        double dval = std::strtod(buf.c_str(), &endptr);
        if (endptr != buf.c_str() + buf.size() || !std::isfinite(dval)) {
            return set_error("number out of range");
        }
        return JsonValue(dval);
    }

    std::optional<JsonValue> parse_array(int depth) {
        ++pos_; // '['
        if (!skip_ws()) return std::nullopt;

        JsonValue::Array arr;
        if (!eof() && peek() == ']') {
            ++pos_;
            return JsonValue(std::move(arr));
        }

        while (true) {
            auto elem = parse_value(depth);
            if (!elem) return std::nullopt;
            arr.push_back(std::move(*elem));

            if (!skip_ws()) return std::nullopt;
            if (eof()) {
                return set_error("unterminated array");
            }
            char c = peek();
            if (c == ']') {
                ++pos_;
                return JsonValue(std::move(arr));
            }
            if (c == ',') {
                ++pos_;
                if (!skip_ws()) return std::nullopt;
                if (jsonc_ && !eof() && peek() == ']') {
                    ++pos_;
                    return JsonValue(std::move(arr));
                }
                continue;
            }
            return set_error("expected ',' or ']' in array");
        }
    }

    std::optional<JsonValue> parse_object(int depth) {
        ++pos_; // '{'
        if (!skip_ws()) return std::nullopt;

        JsonValue obj = JsonValue::object();
        if (!eof() && peek() == '}') {
            ++pos_;
            return obj;
        }

        while (true) {
            if (!skip_ws()) return std::nullopt;
            if (eof() || peek() != '"') {
                return set_error("expected string key in object");
            }
            auto key = parse_string();
            if (!key) return std::nullopt;

            if (!skip_ws()) return std::nullopt;
            if (eof() || peek() != ':') {
                return set_error("expected ':' after object key");
            }
            ++pos_;

            auto val = parse_value(depth);
            if (!val) return std::nullopt;
            obj.set(std::move(*key), std::move(*val));

            if (!skip_ws()) return std::nullopt;
            if (eof()) {
                return set_error("unterminated object");
            }
            char c = peek();
            if (c == '}') {
                ++pos_;
                return obj;
            }
            if (c == ',') {
                ++pos_;
                if (!skip_ws()) return std::nullopt;
                if (jsonc_ && !eof() && peek() == '}') {
                    ++pos_;
                    return obj;
                }
                continue;
            }
            return set_error("expected ',' or '}' in object");
        }
    }

    std::string_view input_;
    bool jsonc_ = false;
    size_t pos_ = 0;
    std::string error_;
    size_t error_offset_ = 0;
};

void escape_json_string(std::string& out, std::string_view s) {
    out.push_back('"');
    for (char ch : s) {
        const auto c = static_cast<unsigned char>(ch);
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\b': out += "\\b"; break;
            case '\f': out += "\\f"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (c < 0x20u) {
                    char buf[8];
                    std::snprintf(buf, sizeof(buf), "\\u%04x", static_cast<unsigned>(c));
                    out += buf;
                } else {
                    out.push_back(ch);
                }
                break;
        }
    }
    out.push_back('"');
}

void write_indent(std::string& out, int level, int indent_spaces) {
    out.append(static_cast<size_t>(level * indent_spaces), ' ');
}

void serialize_json(const JsonValue& val, std::string& out, bool pretty, int indent_spaces, int depth) {
    switch (val.type()) {
        case JsonType::Null:
            out += "null";
            break;
        case JsonType::Bool:
            out += val.as_bool() ? "true" : "false";
            break;
        case JsonType::Int: {
            char buf[32];
            auto [ptr, ec] = std::to_chars(buf, buf + sizeof(buf), val.as_int());
            if (ec == std::errc{}) {
                out.append(buf, ptr);
            } else {
                out += "0";
            }
            break;
        }
        case JsonType::Double: {
            double d = val.as_double();
            if (!std::isfinite(d)) {
                out += "null";
            } else {
                char buf[64];
                int len = std::snprintf(buf, sizeof(buf), "%.17g", d);
                if (len > 0) {
                    std::string_view sv(buf, static_cast<size_t>(len));
                    out.append(sv);
                    if (sv.find('.') == std::string_view::npos &&
                        sv.find('e') == std::string_view::npos &&
                        sv.find('E') == std::string_view::npos) {
                        out += ".0";
                    }
                } else {
                    out += "0.0";
                }
            }
            break;
        }
        case JsonType::String:
            escape_json_string(out, val.as_string());
            break;
        case JsonType::Array: {
            const auto& arr = val.as_array();
            if (arr.empty()) {
                out += "[]";
                break;
            }
            out.push_back('[');
            for (size_t i = 0; i < arr.size(); ++i) {
                if (i > 0) out.push_back(',');
                if (pretty) {
                    out.push_back('\n');
                    write_indent(out, depth + 1, indent_spaces);
                }
                serialize_json(arr[i], out, pretty, indent_spaces, depth + 1);
            }
            if (pretty) {
                out.push_back('\n');
                write_indent(out, depth, indent_spaces);
            }
            out.push_back(']');
            break;
        }
        case JsonType::Object: {
            const auto& obj = val.as_object();
            if (obj.empty()) {
                out += "{}";
                break;
            }
            out.push_back('{');
            for (size_t i = 0; i < obj.size(); ++i) {
                if (i > 0) out.push_back(',');
                if (pretty) {
                    out.push_back('\n');
                    write_indent(out, depth + 1, indent_spaces);
                }
                escape_json_string(out, obj[i].first);
                out += pretty ? ": " : ":";
                serialize_json(obj[i].second, out, pretty, indent_spaces, depth + 1);
            }
            if (pretty) {
                out.push_back('\n');
                write_indent(out, depth, indent_spaces);
            }
            out.push_back('}');
            break;
        }
    }
}

} // namespace

JsonType JsonValue::type() const noexcept {
    switch (data_.index()) {
        case 0: return JsonType::Null;
        case 1: return JsonType::Bool;
        case 2: return JsonType::Int;
        case 3: return JsonType::Double;
        case 4: return JsonType::String;
        case 5: return JsonType::Array;
        case 6: return JsonType::Object;
        default: return JsonType::Null;
    }
}

bool JsonValue::as_bool(bool def) const noexcept {
    if (const auto* b = std::get_if<bool>(&data_)) return *b;
    return def;
}

int64_t JsonValue::as_int(int64_t def) const noexcept {
    if (const auto* i = std::get_if<int64_t>(&data_)) return *i;
    if (const auto* d = std::get_if<double>(&data_)) return static_cast<int64_t>(*d);
    return def;
}

double JsonValue::as_double(double def) const noexcept {
    if (const auto* d = std::get_if<double>(&data_)) return *d;
    if (const auto* i = std::get_if<int64_t>(&data_)) return static_cast<double>(*i);
    return def;
}

const std::string& JsonValue::as_string() const noexcept {
    if (const auto* s = std::get_if<std::string>(&data_)) return *s;
    return kEmptyString;
}

std::string JsonValue::as_string_or(std::string_view def) const {
    if (const auto* s = std::get_if<std::string>(&data_)) return *s;
    return std::string(def);
}

const JsonValue::Array& JsonValue::as_array() const noexcept {
    if (const auto* a = std::get_if<Array>(&data_)) return *a;
    return kEmptyArray;
}

JsonValue::Array& JsonValue::as_array() {
    if (is_null()) {
        data_ = Array{};
    }
    if (auto* a = std::get_if<Array>(&data_)) return *a;
    throw std::runtime_error("JsonValue is not an array");
}

const JsonValue::Object& JsonValue::as_object() const noexcept {
    if (const auto* o = std::get_if<Object>(&data_)) return *o;
    return kEmptyObject;
}

JsonValue::Object& JsonValue::as_object() {
    if (is_null()) {
        data_ = Object{};
    }
    if (auto* o = std::get_if<Object>(&data_)) return *o;
    throw std::runtime_error("JsonValue is not an object");
}

size_t JsonValue::size() const noexcept {
    if (const auto* a = std::get_if<Array>(&data_)) return a->size();
    if (const auto* o = std::get_if<Object>(&data_)) return o->size();
    if (const auto* s = std::get_if<std::string>(&data_)) return s->size();
    return 0;
}

bool JsonValue::empty() const noexcept {
    if (is_null()) return true;
    if (const auto* a = std::get_if<Array>(&data_)) return a->empty();
    if (const auto* o = std::get_if<Object>(&data_)) return o->empty();
    if (const auto* s = std::get_if<std::string>(&data_)) return s->empty();
    return false;
}

bool JsonValue::contains(std::string_view key) const noexcept {
    return find(key) != nullptr;
}

const JsonValue* JsonValue::find(std::string_view key) const noexcept {
    const auto* o = std::get_if<Object>(&data_);
    if (!o) return nullptr;
    for (const auto& [k, v] : *o) {
        if (k == key) return &v;
    }
    return nullptr;
}

JsonValue* JsonValue::find(std::string_view key) noexcept {
    auto* o = std::get_if<Object>(&data_);
    if (!o) return nullptr;
    for (auto& [k, v] : *o) {
        if (k == key) return &v;
    }
    return nullptr;
}

std::string JsonValue::get_str(std::string_view key, std::string_view def) const {
    if (const auto* v = find(key); v && v->is_string()) {
        return v->as_string();
    }
    return std::string(def);
}

bool JsonValue::get_bool(std::string_view key, bool def) const noexcept {
    if (const auto* v = find(key); v && v->is_bool()) {
        return v->as_bool();
    }
    return def;
}

int64_t JsonValue::get_int(std::string_view key, int64_t def) const noexcept {
    if (const auto* v = find(key); v && v->is_number()) {
        return v->as_int(def);
    }
    return def;
}

double JsonValue::get_double(std::string_view key, double def) const noexcept {
    if (const auto* v = find(key); v && v->is_number()) {
        return v->as_double(def);
    }
    return def;
}

const JsonValue& JsonValue::operator[](std::string_view key) const noexcept {
    if (const auto* v = find(key)) return *v;
    return null_value();
}

JsonValue& JsonValue::operator[](std::string_view key) {
    auto& obj = as_object();
    for (auto& [k, v] : obj) {
        if (k == key) return v;
    }
    obj.emplace_back(std::piecewise_construct, std::forward_as_tuple(key), std::forward_as_tuple());
    return obj.back().second;
}

const JsonValue& JsonValue::operator[](size_t index) const noexcept {
    const auto* a = std::get_if<Array>(&data_);
    if (!a || index >= a->size()) return null_value();
    return (*a)[index];
}

JsonValue& JsonValue::operator[](size_t index) {
    auto& arr = as_array();
    if (index >= arr.size()) {
        arr.resize(index + 1);
    }
    return arr[index];
}

void JsonValue::push_back(JsonValue val) {
    as_array().push_back(std::move(val));
}

void JsonValue::set(std::string key, JsonValue val) {
    auto& obj = as_object();
    for (auto& [k, v] : obj) {
        if (k == key) {
            v = std::move(val);
            return;
        }
    }
    obj.emplace_back(std::move(key), std::move(val));
}

bool JsonValue::erase(std::string_view key) {
    auto* o = std::get_if<Object>(&data_);
    if (!o) return false;
    for (auto it = o->begin(); it != o->end(); ++it) {
        if (it->first == key) {
            o->erase(it);
            return true;
        }
    }
    return false;
}

bool JsonValue::operator==(const JsonValue& other) const noexcept {
    if (is_number() && other.is_number()) {
        if (is_int() && other.is_int()) {
            return as_int() == other.as_int();
        }
        return as_double() == other.as_double();
    }
    if (data_.index() != other.data_.index()) {
        return false;
    }
    if (is_object()) {
        const auto& a = as_object();
        const auto& b = other.as_object();
        if (a.size() != b.size()) return false;
        for (const auto& [k, v] : a) {
            const auto* bv = other.find(k);
            if (!bv || !(*bv == v)) return false;
        }
        return true;
    }
    return data_ == other.data_;
}

const JsonValue& JsonValue::null_value() noexcept {
    static const JsonValue kNull{};
    return kNull;
}

JsonParseResult parse_json_ex(std::string_view input) {
    return JsonParser(input, false).parse();
}

JsonParseResult parse_jsonc_ex(std::string_view input) {
    return JsonParser(input, true).parse();
}

std::optional<JsonValue> parse_json(std::string_view input) {
    return parse_json_ex(input).value;
}

std::optional<JsonValue> parse_jsonc(std::string_view input) {
    return parse_jsonc_ex(input).value;
}

std::string to_json(const JsonValue& val, bool pretty, int indent_spaces) {
    std::string out;
    out.reserve(128);
    serialize_json(val, out, pretty, indent_spaces < 0 ? 0 : indent_spaces, 0);
    return out;
}

} // namespace singlet::util
