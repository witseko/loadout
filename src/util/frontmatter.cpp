#include "util/frontmatter.hpp"

#include <algorithm>
#include <cctype>
#include <string>
#include <string_view>
#include <vector>

namespace loadout::util {

namespace {

std::string_view trim_sv(std::string_view sv) noexcept {
    while (!sv.empty() && (sv.front() == ' ' || sv.front() == '\t' || sv.front() == '\r' || sv.front() == '\n')) {
        sv.remove_prefix(1);
    }
    while (!sv.empty() && (sv.back() == ' ' || sv.back() == '\t' || sv.back() == '\r' || sv.back() == '\n')) {
        sv.remove_suffix(1);
    }
    return sv;
}

std::string_view strip_cr(std::string_view sv) noexcept {
    if (!sv.empty() && sv.back() == '\r') {
        sv.remove_suffix(1);
    }
    return sv;
}

size_t count_leading_spaces(std::string_view sv) noexcept {
    size_t n = 0;
    while (n < sv.size() && sv[n] == ' ') {
        ++n;
    }
    return n;
}

std::string unquote_yaml_scalar(std::string_view raw) {
    raw = trim_sv(raw);
    if (raw.empty()) return {};

    if (raw.size() >= 2 && raw.front() == '"' && raw.back() == '"') {
        std::string out;
        std::string_view inner = raw.substr(1, raw.size() - 2);
        out.reserve(inner.size());
        for (size_t i = 0; i < inner.size(); ++i) {
            if (inner[i] == '\\' && i + 1 < inner.size()) {
                char next = inner[++i];
                switch (next) {
                    case 'n':  out.push_back('\n'); break;
                    case 'r':  out.push_back('\r'); break;
                    case 't':  out.push_back('\t'); break;
                    case '"':  out.push_back('"'); break;
                    case '\\': out.push_back('\\'); break;
                    default:   out.push_back(next); break;
                }
            } else {
                out.push_back(inner[i]);
            }
        }
        return out;
    }

    if (raw.size() >= 2 && raw.front() == '\'' && raw.back() == '\'') {
        std::string out;
        std::string_view inner = raw.substr(1, raw.size() - 2);
        out.reserve(inner.size());
        for (size_t i = 0; i < inner.size(); ++i) {
            if (inner[i] == '\'' && i + 1 < inner.size() && inner[i + 1] == '\'') {
                out.push_back('\'');
                ++i;
            } else {
                out.push_back(inner[i]);
            }
        }
        return out;
    }

    // Strip unquoted inline comment (" #...").
    for (size_t i = 1; i < raw.size(); ++i) {
        if (raw[i] == '#' && (raw[i - 1] == ' ' || raw[i - 1] == '\t')) {
            raw = trim_sv(raw.substr(0, i));
            break;
        }
    }

    return std::string(raw);
}

std::vector<std::string> parse_inline_yaml_list(std::string_view raw) {
    std::vector<std::string> items;
    raw = trim_sv(raw);
    if (raw.size() < 2 || raw.front() != '[' || raw.back() != ']') {
        return items;
    }
    std::string_view inner = raw.substr(1, raw.size() - 2);
    size_t i = 0;
    while (i < inner.size()) {
        while (i < inner.size() && (inner[i] == ' ' || inner[i] == '\t' || inner[i] == ',')) {
            ++i;
        }
        if (i >= inner.size()) break;

        size_t start = i;
        if (inner[i] == '"' || inner[i] == '\'') {
            char quote = inner[i++];
            while (i < inner.size()) {
                if (quote == '"' && inner[i] == '\\' && i + 1 < inner.size()) {
                    i += 2;
                    continue;
                }
                if (quote == '\'' && inner[i] == '\'' && i + 1 < inner.size() && inner[i + 1] == '\'') {
                    i += 2;
                    continue;
                }
                if (inner[i] == quote) {
                    ++i;
                    break;
                }
                ++i;
            }
            std::string val = unquote_yaml_scalar(inner.substr(start, i - start));
            if (!val.empty()) items.push_back(std::move(val));
        } else {
            while (i < inner.size() && inner[i] != ',') {
                ++i;
            }
            std::string val = unquote_yaml_scalar(inner.substr(start, i - start));
            if (!val.empty()) items.push_back(std::move(val));
        }
    }
    return items;
}

struct LineSlice {
    size_t start = 0;   // byte offset in content
    size_t end = 0;     // byte offset before \n (or EOF)
    size_t next = 0;    // byte offset after \n (or EOF)
    std::string_view view; // line content without \r\n
};

std::vector<LineSlice> index_lines(std::string_view content) {
    std::vector<LineSlice> lines;
    size_t pos = 0;
    while (pos <= content.size()) {
        if (pos == content.size()) break;
        size_t nl = content.find('\n', pos);
        LineSlice ls;
        ls.start = pos;
        if (nl == std::string_view::npos) {
            ls.end = content.size();
            ls.next = content.size();
        } else {
            ls.end = nl;
            ls.next = nl + 1;
        }
        ls.view = strip_cr(content.substr(ls.start, ls.end - ls.start));
        lines.push_back(ls);
        pos = ls.next;
    }
    return lines;
}

bool is_block_scalar_indicator(std::string_view val, bool& folded, char& chomp) noexcept {
    val = trim_sv(val);
    if (val.empty()) return false;
    if (val[0] != '>' && val[0] != '|') return false;
    folded = (val[0] == '>');
    chomp = ' '; // clip default
    size_t idx = 1;
    while (idx < val.size()) {
        char c = val[idx];
        if (c == '-' || c == '+') {
            chomp = c;
            ++idx;
        } else if (c >= '1' && c <= '9') {
            ++idx;
        } else if (c == ' ' || c == '\t') {
            // Possible trailing comment.
            auto rest = trim_sv(val.substr(idx));
            return rest.empty() || rest.front() == '#';
        } else {
            return false;
        }
    }
    return true;
}

std::string collect_block_scalar(
    const std::vector<LineSlice>& lines,
    size_t& idx,
    size_t fm_end_line,
    bool folded,
    char chomp
) {
    std::vector<std::string_view> raw_lines;
    size_t base_indent = std::string::npos;

    while (idx + 1 < fm_end_line) {
        std::string_view next_line = lines[idx + 1].view;
        bool blank = trim_sv(next_line).empty();
        if (blank) {
            raw_lines.push_back("");
            ++idx;
            continue;
        }
        size_t indent = count_leading_spaces(next_line);
        if (indent == 0) {
            break;
        }
        if (base_indent == std::string::npos) {
            base_indent = indent;
        } else if (indent < base_indent) {
            base_indent = indent;
        }
        raw_lines.push_back(next_line);
        ++idx;
    }

    // Trim trailing blank lines unless keep ('+') chomping.
    if (chomp != '+') {
        while (!raw_lines.empty() && raw_lines.back().empty()) {
            raw_lines.pop_back();
        }
    }

    if (raw_lines.empty()) {
        return {};
    }
    if (base_indent == std::string::npos) {
        base_indent = 0;
    }

    std::string out;
    if (!folded) {
        for (size_t i = 0; i < raw_lines.size(); ++i) {
            if (i > 0) out.push_back('\n');
            if (!raw_lines[i].empty()) {
                size_t strip = std::min(base_indent, raw_lines[i].size());
                out.append(raw_lines[i].substr(strip));
            }
        }
    } else {
        bool prev_was_blank = false;
        for (size_t i = 0; i < raw_lines.size(); ++i) {
            if (raw_lines[i].empty()) {
                if (!out.empty()) {
                    out.push_back('\n');
                }
                prev_was_blank = true;
            } else {
                size_t strip = std::min(base_indent, raw_lines[i].size());
                std::string_view piece = raw_lines[i].substr(strip);
                if (!out.empty() && !prev_was_blank) {
                    out.push_back(' ');
                }
                out.append(piece);
                prev_was_blank = false;
            }
        }
    }

    return std::string(trim_sv(out));
}

struct FrontmatterBounds {
    bool found = false;
    size_t open_line = 0;
    size_t close_line = 0;
    size_t body_offset = 0;
};

FrontmatterBounds locate_frontmatter(const std::vector<LineSlice>& lines) {
    FrontmatterBounds b;
    if (lines.empty()) return b;

    std::string_view first = lines[0].view;
    // Allow UTF-8 BOM on first line.
    if (first.size() >= 3 &&
        static_cast<unsigned char>(first[0]) == 0xEF &&
        static_cast<unsigned char>(first[1]) == 0xBB &&
        static_cast<unsigned char>(first[2]) == 0xBF) {
        first.remove_prefix(3);
    }
    if (trim_sv(first) != "---") {
        return b;
    }

    for (size_t i = 1; i < lines.size(); ++i) {
        std::string_view trimmed = trim_sv(lines[i].view);
        if (trimmed == "---" || trimmed == "...") {
            b.found = true;
            b.open_line = 0;
            b.close_line = i;
            b.body_offset = lines[i].next;
            return b;
        }
    }
    return b;
}

std::string derive_description_from_body(std::string_view body, std::string_view folder_slug) {
    auto lines = index_lines(body);
    bool in_fence = false;
    bool in_html_comment = false;
    std::string paragraph;

    for (const auto& ls : lines) {
        std::string_view t = trim_sv(ls.view);

        if (t.starts_with("```") || t.starts_with("~~~")) {
            in_fence = !in_fence;
            if (!paragraph.empty()) break;
            continue;
        }
        if (in_fence) continue;

        if (in_html_comment) {
            if (t.find("-->") != std::string_view::npos) {
                in_html_comment = false;
            }
            continue;
        }
        if (t.starts_with("<!--")) {
            if (t.find("-->") == std::string_view::npos) {
                in_html_comment = true;
            }
            continue;
        }

        if (t.empty()) {
            if (!paragraph.empty()) break;
            continue;
        }

        // Skip headings, horizontal rules, images-only lines.
        if (t.front() == '#' || t == "---" || t == "***" || t == "___") {
            if (!paragraph.empty()) break;
            continue;
        }

        // Strip leading blockquote or list markers if paragraph begins with one.
        if (t.starts_with("> ")) {
            t = trim_sv(t.substr(2));
        }
        if (t.empty()) continue;

        if (!paragraph.empty()) {
            paragraph.push_back(' ');
        }
        paragraph.append(t);
        if (paragraph.size() >= 240) {
            break;
        }
    }

    // Collapse consecutive whitespace.
    std::string cleaned;
    cleaned.reserve(paragraph.size());
    bool last_space = false;
    for (char c : paragraph) {
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
            if (!last_space && !cleaned.empty()) {
                cleaned.push_back(' ');
                last_space = true;
            }
        } else {
            cleaned.push_back(c);
            last_space = false;
        }
    }
    while (!cleaned.empty() && cleaned.back() == ' ') {
        cleaned.pop_back();
    }

    if (cleaned.size() > 240) {
        size_t cut = cleaned.rfind(' ', 237);
        if (cut == std::string::npos || cut < 80) {
            cut = 237;
        }
        cleaned.resize(cut);
        cleaned += "...";
    }

    if (cleaned.empty()) {
        if (!folder_slug.empty()) {
            return std::string("Skill ") + std::string(folder_slug);
        }
        return "Custom agent skill";
    }
    return cleaned;
}

std::string format_yaml_scalar(std::string_view value) {
    bool needs_quotes = value.empty();
    if (!needs_quotes) {
        if (value.front() == ' ' || value.back() == ' ' ||
            value.front() == '-' || value.front() == '?' ||
            value.front() == ':' || value.front() == '!' ||
            value.front() == '&' || value.front() == '*' ||
            value.front() == '[' || value.front() == ']' ||
            value.front() == '{' || value.front() == '}' ||
            value.front() == ',' || value.front() == '#' ||
            value.front() == '|' || value.front() == '>' ||
            value.front() == '\'' || value.front() == '"' ||
            value.front() == '%' || value.front() == '@' ||
            value.front() == '`') {
            needs_quotes = true;
        }
        if (!needs_quotes) {
            for (size_t i = 0; i < value.size(); ++i) {
                char c = value[i];
                if (c == '\n' || c == '\r' || c == '\t' || c == '"' || c == '\\') {
                    needs_quotes = true;
                    break;
                }
                if (c == ':' && (i + 1 == value.size() || value[i + 1] == ' ')) {
                    needs_quotes = true;
                    break;
                }
                if (c == '#' && i > 0 && value[i - 1] == ' ') {
                    needs_quotes = true;
                    break;
                }
            }
        }
        if (!needs_quotes) {
            if (value == "true" || value == "false" || value == "null" || value == "~" ||
                value == "yes" || value == "no") {
                needs_quotes = true;
            }
        }
    }

    if (!needs_quotes) {
        return std::string(value);
    }

    std::string out;
    out.reserve(value.size() + 4);
    out.push_back('"');
    for (char c : value) {
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:   out.push_back(c); break;
        }
    }
    out.push_back('"');
    return out;
}

bool parse_top_level_key(std::string_view line, std::string_view& key, std::string_view& val) noexcept {
    if (line.empty() || line[0] == ' ' || line[0] == '\t' || line[0] == '#') {
        return false;
    }
    size_t colon = line.find(':');
    if (colon == std::string_view::npos || colon == 0) {
        return false;
    }
    // Must be followed by EOL or whitespace.
    if (colon + 1 < line.size() && line[colon + 1] != ' ' && line[colon + 1] != '\t') {
        return false;
    }
    key = trim_sv(line.substr(0, colon));
    val = (colon + 1 < line.size()) ? trim_sv(line.substr(colon + 1)) : std::string_view{};
    return !key.empty();
}

std::vector<std::string> parse_tags_field(
    std::string_view val,
    const std::vector<LineSlice>& lines,
    size_t& idx,
    size_t fm_end_line
) {
    if (!val.empty() && val.front() == '[') {
        return parse_inline_yaml_list(val);
    }

    std::vector<std::string> tags;
    if (!val.empty()) {
        size_t pos = 0;
        while (pos < val.size()) {
            size_t comma = val.find(',', pos);
            std::string_view tok = (comma == std::string_view::npos)
                ? val.substr(pos)
                : val.substr(pos, comma - pos);
            std::string item = unquote_yaml_scalar(tok);
            if (!item.empty()) {
                tags.push_back(std::move(item));
            }
            if (comma == std::string_view::npos) break;
            pos = comma + 1;
        }
        return tags;
    }

    while (idx + 1 < fm_end_line) {
        std::string_view next_line = lines[idx + 1].view;
        std::string_view trimmed = trim_sv(next_line);
        if (trimmed.empty() || trimmed.front() == '#') {
            ++idx;
            continue;
        }
        if (count_leading_spaces(next_line) == 0 && !trimmed.starts_with("- ")) {
            break;
        }
        if (!trimmed.starts_with("- ")) {
            break;
        }
        std::string item = unquote_yaml_scalar(trimmed.substr(2));
        if (!item.empty()) {
            tags.push_back(std::move(item));
        }
        ++idx;
    }
    return tags;
}

} // namespace

FrontmatterInfo parse_skill_markdown(std::string_view content, std::string_view folder_slug) {
    FrontmatterInfo info;
    auto lines = index_lines(content);
    auto bounds = locate_frontmatter(lines);

    info.has_frontmatter = bounds.found;
    if (bounds.found) {
        info.body = std::string(content.substr(bounds.body_offset));

        for (size_t i = bounds.open_line + 1; i < bounds.close_line; ++i) {
            std::string_view key;
            std::string_view val;
            if (!parse_top_level_key(lines[i].view, key, val)) {
                continue;
            }

            if (key == "name") {
                bool folded = false;
                char chomp = ' ';
                info.name = is_block_scalar_indicator(val, folded, chomp)
                    ? collect_block_scalar(lines, i, bounds.close_line, folded, chomp)
                    : unquote_yaml_scalar(val);
            } else if (key == "description") {
                bool folded = false;
                char chomp = ' ';
                info.description = is_block_scalar_indicator(val, folded, chomp)
                    ? collect_block_scalar(lines, i, bounds.close_line, folded, chomp)
                    : unquote_yaml_scalar(val);
            } else if (key == "tags") {
                info.tags = parse_tags_field(val, lines, i, bounds.close_line);
            }
        }
    } else {
        info.body = std::string(content);
    }

    // Estimate per-turn progressive-disclosure context tokens.
    if (content.empty()) {
        info.approx_tokens = 0;
    } else if (!info.description.empty()) {
        size_t meta_chars = info.name.size() + info.description.size();
        info.approx_tokens = 20 + (meta_chars + 3) / 4;
    } else {
        size_t sample_chars = folder_slug.size() + std::min<size_t>(content.size(), 600);
        info.approx_tokens = 20 + (sample_chars + 3) / 4;
    }

    // Validate Antigravity strict compatibility.
    if (!info.has_frontmatter) {
        info.issues.push_back("Missing YAML frontmatter block (---)");
    }
    if (info.name.empty()) {
        info.issues.push_back("Missing required 'name' field in frontmatter");
    } else if (!folder_slug.empty() && info.name != folder_slug) {
        info.issues.push_back(
            "Frontmatter 'name' ('" + info.name + "') does not match folder slug ('" +
            std::string(folder_slug) + "')"
        );
    }
    if (info.description.empty()) {
        info.issues.push_back("Missing required 'description' field in frontmatter");
    }

    info.antigravity_valid =
        info.has_frontmatter &&
        !info.name.empty() &&
        info.name == folder_slug &&
        !info.description.empty();

    return info;
}

std::string fix_skill_frontmatter(std::string_view content, std::string_view folder_slug) {
    FrontmatterInfo parsed = parse_skill_markdown(content, folder_slug);

    // Detect line ending style.
    std::string_view eol = (content.find("\r\n") != std::string_view::npos) ? "\r\n" : "\n";

    std::string desired_name(folder_slug);
    std::string desired_desc = !parsed.description.empty()
        ? parsed.description
        : derive_description_from_body(parsed.body, folder_slug);

    if (!parsed.has_frontmatter) {
        // Strip UTF-8 BOM if present at beginning of body so frontmatter stays on line 1.
        std::string_view body_sv = content;
        if (body_sv.size() >= 3 &&
            static_cast<unsigned char>(body_sv[0]) == 0xEF &&
            static_cast<unsigned char>(body_sv[1]) == 0xBB &&
            static_cast<unsigned char>(body_sv[2]) == 0xBF) {
            body_sv.remove_prefix(3);
        }

        std::string out;
        out += "---";
        out.append(eol);
        out += "name: ";
        out += format_yaml_scalar(desired_name);
        out.append(eol);
        out += "description: ";
        out += format_yaml_scalar(desired_desc);
        out.append(eol);
        out += "---";
        out.append(eol);
        if (!body_sv.empty()) {
            if (!body_sv.starts_with("\n") && !body_sv.starts_with("\r\n")) {
                out.append(eol);
            }
            out.append(body_sv);
        }
        return out;
    }

    // Non-destructively update existing frontmatter lines.
    auto lines = index_lines(content);
    auto bounds = locate_frontmatter(lines);

    std::vector<std::string> rebuilt;
    bool saw_name = false;
    bool saw_valid_desc = false;

    for (size_t i = bounds.open_line + 1; i < bounds.close_line; ++i) {
        std::string_view key;
        std::string_view val;
        if (parse_top_level_key(lines[i].view, key, val)) {
            if (key == "name") {
                saw_name = true;
                rebuilt.push_back("name: " + format_yaml_scalar(desired_name));
                // Skip any continuation lines if old name was a block scalar.
                bool folded = false;
                char chomp = ' ';
                if (is_block_scalar_indicator(val, folded, chomp)) {
                    (void)collect_block_scalar(lines, i, bounds.close_line, folded, chomp);
                }
                continue;
            }
            if (key == "description") {
                if (!parsed.description.empty()) {
                    // Existing description is non-empty: keep original lines verbatim!
                    saw_valid_desc = true;
                    rebuilt.emplace_back(lines[i].view);
                    continue;
                }
                // Existing description was empty: replace it.
                saw_valid_desc = true;
                rebuilt.push_back("description: " + format_yaml_scalar(desired_desc));
                bool folded = false;
                char chomp = ' ';
                if (is_block_scalar_indicator(val, folded, chomp)) {
                    (void)collect_block_scalar(lines, i, bounds.close_line, folded, chomp);
                }
                continue;
            }
        }
        rebuilt.emplace_back(lines[i].view);
    }

    if (!saw_name) {
        rebuilt.insert(rebuilt.begin(), "name: " + format_yaml_scalar(desired_name));
    }
    if (!saw_valid_desc) {
        // Insert description right after name if possible, else at end.
        size_t insert_pos = rebuilt.size();
        for (size_t i = 0; i < rebuilt.size(); ++i) {
            if (rebuilt[i].starts_with("name:")) {
                insert_pos = i + 1;
                break;
            }
        }
        rebuilt.insert(
            rebuilt.begin() + static_cast<std::ptrdiff_t>(insert_pos),
            "description: " + format_yaml_scalar(desired_desc)
        );
    }

    std::string out;
    out += "---";
    out.append(eol);
    for (const auto& line : rebuilt) {
        out += line;
        out.append(eol);
    }
    out += "---";
    out.append(eol);
    out.append(content.substr(bounds.body_offset));
    return out;
}

} // namespace loadout::util
