#include "core/targets.hpp"

#include <dirent.h>
#include <sys/stat.h>

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <set>
#include <sstream>

#include "util/json.hpp"

namespace singlet::core {

namespace fs = std::filesystem;

namespace {

std::string to_lower_ascii(std::string_view sv) {
    std::string out;
    out.reserve(sv.size());
    for (char ch : sv) {
        auto c = static_cast<unsigned char>(ch);
        out.push_back(static_cast<char>(std::tolower(c)));
    }
    return out;
}

std::string sanitize_id_suffix(std::string_view raw) {
    std::string out;
    bool prev_dash = false;
    for (char ch : raw) {
        auto c = static_cast<unsigned char>(ch);
        if (std::isalnum(c)) {
            out.push_back(static_cast<char>(std::tolower(c)));
            prev_dash = false;
        } else if ((c == '-' || c == '_' || c == '.' || c == ' ') && !out.empty() && !prev_dash) {
            out.push_back('-');
            prev_dash = true;
        }
    }
    while (!out.empty() && out.back() == '-') {
        out.pop_back();
    }
    return out;
}

std::string title_case_suffix(std::string_view raw) {
    std::string out;
    bool cap_next = true;
    for (char ch : raw) {
        auto c = static_cast<unsigned char>(ch);
        if (c == '-' || c == '_' || c == '.') {
            if (!out.empty() && out.back() != ' ') {
                out.push_back(' ');
            }
            cap_next = true;
        } else if (std::isalnum(c)) {
            out.push_back(cap_next ? static_cast<char>(std::toupper(c)) : static_cast<char>(std::tolower(c)));
            cap_next = false;
        }
    }
    while (!out.empty() && out.back() == ' ') {
        out.pop_back();
    }
    return out.empty() ? "Extra" : out;
}

bool is_dir_or_link_to_dir(const fs::path& p) {
    struct stat st {};
    if (::stat(p.c_str(), &st) != 0) {
        return false;
    }
    return S_ISDIR(st.st_mode);
}

bool path_exists_any(const fs::path& p) {
    struct stat st {};
    return ::lstat(p.c_str(), &st) == 0;
}

bool has_any_marker(const fs::path& dir, const std::vector<std::string_view>& markers) {
    for (auto m : markers) {
        if (path_exists_any(dir / m)) {
            return true;
        }
    }
    return false;
}

std::string read_file_text(const fs::path& p) {
    std::ifstream in(p, std::ios::in | std::ios::binary);
    if (!in) {
        return {};
    }
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

int group_rank(std::string_view group) {
    if (group == "Google") return 0;
    if (group == "Claude") return 1;
    if (group == "OpenAI") return 2;
    if (group == "OSS") return 3;
    if (group == "Z.ai") return 4;
    if (group == "Cursor") return 5;
    if (group == "Windsurf") return 6;
    return 7;
}

bool target_less(const TargetInstance& a, const TargetInstance& b) {
    int ra = group_rank(a.group);
    int rb = group_rank(b.group);
    if (ra != rb) return ra < rb;
    bool a_main = a.id.ends_with("-main") || a.id == "antigravity" || a.id == "codex" ||
                  a.id == "opencode" || a.id == "zcode" || a.id == "cursor" || a.id == "windsurf";
    bool b_main = b.id.ends_with("-main") || b.id == "antigravity" || b.id == "codex" ||
                  b.id == "opencode" || b.id == "zcode" || b.id == "cursor" || b.id == "windsurf";
    if (a_main != b_main) return a_main;
    return a.id < b.id;
}

void collect_plugin_skill_dirs(const fs::path& plugins_dir, std::vector<fs::path>& out) {
    DIR* dp = ::opendir(plugins_dir.c_str());
    if (!dp) return;
    while (struct dirent* ent = ::readdir(dp)) {
        std::string_view name(ent->d_name);
        if (name == "." || name == "..") continue;
        fs::path plugin_dir = plugins_dir / name;
        if (!is_dir_or_link_to_dir(plugin_dir)) continue;
        fs::path skills = plugin_dir / "skills";
        if (is_dir_or_link_to_dir(skills)) {
            out.push_back(skills);
        }
    }
    ::closedir(dp);
}

void extract_string_paths(
    const singlet::util::JsonValue& val,
    const fs::path& base_dir,
    const fs::path& home_dir,
    std::vector<fs::path>& out) {
    auto add_one = [&](const std::string& raw) {
        if (raw.empty()) return;
        fs::path p = expand_home_path(raw, home_dir);
        if (p.is_relative()) {
            p = (base_dir / p).lexically_normal();
        }
        out.push_back(p);
    };
    if (val.is_string()) {
        add_one(val.as_string());
    } else if (val.is_array()) {
        for (const auto& item : val.as_array()) {
            if (item.is_string()) {
                add_one(item.as_string());
            }
        }
    } else if (val.is_object()) {
        for (const char* k : {"paths", "dirs", "directories", "roots"}) {
            if (const auto* child = val.find(k)) {
                extract_string_paths(*child, base_dir, home_dir, out);
            }
        }
    }
}

}  // namespace

std::string_view adapter_to_string(AdapterKind kind) {
    switch (kind) {
        case AdapterKind::Antigravity: return "antigravity";
        case AdapterKind::ClaudeCode: return "claude-code";
        case AdapterKind::Codex: return "codex";
        case AdapterKind::OpenCode: return "opencode";
        case AdapterKind::ZCode: return "zcode";
        case AdapterKind::Cursor: return "cursor";
        case AdapterKind::Windsurf: return "windsurf";
        case AdapterKind::CustomLenient: return "custom-lenient";
        case AdapterKind::CustomStrict: return "custom-strict";
    }
    return "custom-lenient";
}

std::optional<AdapterKind> adapter_from_string(std::string_view str) {
    std::string s = to_lower_ascii(str);
    if (s == "antigravity" || s == "gemini" || s == "gemini-antigravity") return AdapterKind::Antigravity;
    if (s == "claude-code" || s == "claude_code" || s == "claude" || s == "claudecode") return AdapterKind::ClaudeCode;
    if (s == "codex" || s == "openai-codex") return AdapterKind::Codex;
    if (s == "opencode" || s == "open-code") return AdapterKind::OpenCode;
    if (s == "zcode" || s == "z-code" || s == "zai") return AdapterKind::ZCode;
    if (s == "cursor") return AdapterKind::Cursor;
    if (s == "windsurf" || s == "codeium") return AdapterKind::Windsurf;
    if (s == "custom-lenient" || s == "custom_lenient" || s == "custom" || s == "lenient") return AdapterKind::CustomLenient;
    if (s == "custom-strict" || s == "custom_strict" || s == "strict") return AdapterKind::CustomStrict;
    return std::nullopt;
}

bool adapter_requires_strict_frontmatter(AdapterKind kind) {
    return kind == AdapterKind::Antigravity || kind == AdapterKind::CustomStrict;
}

std::string_view adapter_default_group(AdapterKind kind) {
    switch (kind) {
        case AdapterKind::Antigravity: return "Google";
        case AdapterKind::ClaudeCode: return "Claude";
        case AdapterKind::Codex: return "OpenAI";
        case AdapterKind::OpenCode: return "OSS";
        case AdapterKind::ZCode: return "Z.ai";
        case AdapterKind::Cursor: return "Cursor";
        case AdapterKind::Windsurf: return "Windsurf";
        case AdapterKind::CustomLenient:
        case AdapterKind::CustomStrict: return "Custom";
    }
    return "Custom";
}

std::string_view link_mode_to_string(LinkMode mode) {
    switch (mode) {
        case LinkMode::SymlinkDir: return "symlink-dir";
        case LinkMode::AntigravitySkillsJson: return "antigravity-skills-json";
    }
    return "symlink-dir";
}

std::optional<LinkMode> link_mode_from_string(std::string_view str) {
    std::string s = to_lower_ascii(str);
    if (s == "symlink-dir" || s == "symlink_dir" || s == "symlink") return LinkMode::SymlinkDir;
    if (s == "antigravity-skills-json" || s == "antigravity_skills_json" || s == "skills-json") {
        return LinkMode::AntigravitySkillsJson;
    }
    return std::nullopt;
}

std::string_view entry_kind_to_string(EntryKind kind) {
    switch (kind) {
        case EntryKind::Canonical: return "canonical";
        case EntryKind::ManagedSymlink: return "managed-symlink";
        case EntryKind::ExternalSymlink: return "external-symlink";
        case EntryKind::DanglingSymlink: return "dangling-symlink";
        case EntryKind::PhysicalCopy: return "physical-copy";
        case EntryKind::PluginShadow: return "plugin-shadow";
    }
    return "physical-copy";
}

fs::path resolve_home_dir(const fs::path& explicit_home) {
    if (!explicit_home.empty()) {
        std::error_code ec;
        auto abs = fs::absolute(explicit_home, ec);
        return ec ? explicit_home : abs.lexically_normal();
    }
    if (const char* singlet_home = std::getenv("SINGLET_HOME"); singlet_home && *singlet_home != '\0') {
        std::error_code ec;
        auto abs = fs::absolute(fs::path(singlet_home), ec);
        return ec ? fs::path(singlet_home) : abs.lexically_normal();
    }
    if (const char* home = std::getenv("HOME"); home && *home != '\0') {
        return fs::path(home).lexically_normal();
    }
    return fs::current_path();
}

fs::path expand_home_path(std::string_view raw_path, const fs::path& home_dir) {
    if (raw_path.empty()) return {};
    if (raw_path == "~") return home_dir;
    if (raw_path.size() >= 2 && raw_path[0] == '~' && (raw_path[1] == '/' || raw_path[1] == '\\')) {
        return (home_dir / raw_path.substr(2)).lexically_normal();
    }
    return fs::path(raw_path).lexically_normal();
}

std::string contract_home_path(const fs::path& path, const fs::path& home_dir) {
    std::string p_str = path.lexically_normal().string();
    std::string h_str = home_dir.lexically_normal().string();
    while (h_str.size() > 1 && h_str.back() == '/') {
        h_str.pop_back();
    }
    if (!h_str.empty() && p_str == h_str) {
        return "~";
    }
    if (!h_str.empty() && p_str.starts_with(h_str + "/")) {
        return "~" + p_str.substr(h_str.size());
    }
    return p_str;
}

std::vector<fs::path> parse_opencode_custom_skill_paths(
    const fs::path& opencode_config_dir,
    const fs::path& home_dir) {
    std::vector<fs::path> result;
    std::set<std::string> seen;

    for (const char* fname : {"opencode.jsonc", "opencode.json"}) {
        fs::path cfg = opencode_config_dir / fname;
        if (!path_exists_any(cfg)) continue;
        std::string text = read_file_text(cfg);
        if (text.empty()) continue;

        auto parsed = singlet::util::parse_jsonc(text);
        if (!parsed || !parsed->is_object()) continue;

        std::vector<fs::path> raw_paths;
        for (const char* key : {"skills", "skillPaths", "skill_paths", "skillDirs", "skill_dirs"}) {
            if (const auto* v = parsed->find(key)) {
                extract_string_paths(*v, opencode_config_dir, home_dir, raw_paths);
            }
        }
        for (const auto& p : raw_paths) {
            std::string key = p.lexically_normal().string();
            if (seen.insert(key).second) {
                result.push_back(p.lexically_normal());
            }
        }
    }
    return result;
}

DiscoveredTargets discover_targets(const fs::path& home_dir) {
    DiscoveredTargets out;
    std::set<std::string> seen_ids;

    auto add_target_unique = [&](TargetInstance t) {
        if (seen_ids.insert(t.id).second) {
            out.targets.push_back(std::move(t));
        }
    };

    // 1. Scan home_dir for ~/.claude*, ~/.codex*, ~/.gemini*, ~/.cursor*, ~/.zcode*
    if (DIR* dp = ::opendir(home_dir.c_str())) {
        while (struct dirent* ent = ::readdir(dp)) {
            std::string name(ent->d_name);
            if (name == "." || name == "..") continue;
            if (name.empty() || name[0] != '.') continue;

            fs::path entry_path = home_dir / name;
            if (!is_dir_or_link_to_dir(entry_path)) continue;

            // Claude Code: ~/.claude, ~/.claude-second, ~/.claude-work, etc.
            if (name == ".claude" || name.starts_with(".claude-") || name.starts_with(".claude_")) {
                if (!has_any_marker(entry_path, {"skills", "settings.json", ".credentials.json", "commands", "plugins", "CLAUDE.md"})) {
                    continue;
                }
                TargetInstance t;
                t.adapter = AdapterKind::ClaudeCode;
                t.group = "Claude";
                t.skills_dir = entry_path / "skills";
                t.config_root = entry_path;
                t.link_mode = LinkMode::SymlinkDir;
                t.auto_discovered = true;
                t.configured = is_dir_or_link_to_dir(t.skills_dir);

                if (name == ".claude") {
                    t.id = "claude-main";
                    t.display_name = "Claude (Main)";
                } else {
                    std::string suffix_raw = name.substr(7);
                    std::string suffix = sanitize_id_suffix(suffix_raw);
                    if (suffix.empty()) suffix = "extra";
                    t.id = "claude-" + suffix;
                    t.display_name = "Claude (" + title_case_suffix(suffix_raw) + ")";
                    t.env_hint = "CLAUDE_CONFIG_DIR=" + contract_home_path(entry_path, home_dir);
                }
                collect_plugin_skill_dirs(entry_path / "plugins", out.plugin_skill_roots);
                add_target_unique(std::move(t));
                continue;
            }

            // Codex: ~/.codex, ~/.codex-work, etc.
            if (name == ".codex" || name.starts_with(".codex-") || name.starts_with(".codex_")) {
                if (!has_any_marker(entry_path, {"skills", "config.toml", "auth.json", "prompts"})) {
                    continue;
                }
                TargetInstance t;
                t.adapter = AdapterKind::Codex;
                t.group = "OpenAI";
                t.skills_dir = entry_path / "skills";
                t.config_root = entry_path;
                t.link_mode = LinkMode::SymlinkDir;
                t.auto_discovered = true;
                t.configured = is_dir_or_link_to_dir(t.skills_dir);

                if (name == ".codex") {
                    t.id = "codex";
                    t.display_name = "Codex";
                } else {
                    std::string suffix_raw = name.substr(6);
                    std::string suffix = sanitize_id_suffix(suffix_raw);
                    if (suffix.empty()) suffix = "extra";
                    t.id = "codex-" + suffix;
                    t.display_name = "Codex (" + title_case_suffix(suffix_raw) + ")";
                    t.env_hint = "CODEX_HOME=" + contract_home_path(entry_path, home_dir);
                }
                add_target_unique(std::move(t));
                continue;
            }

            // Gemini / Antigravity: ~/.gemini, ~/.gemini-*
            if (name == ".gemini" || name.starts_with(".gemini-") || name.starts_with(".gemini_")) {
                if (!has_any_marker(entry_path, {"config", "antigravity", "settings.json", "skills"})) {
                    continue;
                }
                TargetInstance t;
                t.adapter = AdapterKind::Antigravity;
                t.group = "Google";
                t.skills_dir = entry_path / "config" / "skills";
                t.config_root = entry_path;
                t.link_mode = LinkMode::SymlinkDir;
                t.auto_discovered = true;
                t.configured = is_dir_or_link_to_dir(t.skills_dir);

                if (name == ".gemini") {
                    t.id = "antigravity";
                    t.display_name = "Antigravity";
                } else {
                    std::string suffix_raw = name.substr(7);
                    std::string suffix = sanitize_id_suffix(suffix_raw);
                    if (suffix.empty()) suffix = "extra";
                    t.id = "antigravity-" + suffix;
                    t.display_name = "Antigravity (" + title_case_suffix(suffix_raw) + ")";
                    t.env_hint = "GEMINI_CONFIG_DIR=" + contract_home_path(entry_path, home_dir);
                }
                collect_plugin_skill_dirs(entry_path / "config" / "plugins", out.plugin_skill_roots);
                collect_plugin_skill_dirs(entry_path / "plugins", out.plugin_skill_roots);
                add_target_unique(std::move(t));
                continue;
            }

            // Cursor: ~/.cursor, ~/.cursor-*
            if (name == ".cursor" || name.starts_with(".cursor-") || name.starts_with(".cursor_")) {
                if (!has_any_marker(entry_path, {"skills", "rules", "mcp.json", "extensions"})) {
                    continue;
                }
                TargetInstance t;
                t.adapter = AdapterKind::Cursor;
                t.group = "Cursor";
                t.skills_dir = entry_path / "skills";
                t.config_root = entry_path;
                t.link_mode = LinkMode::SymlinkDir;
                t.auto_discovered = true;
                t.configured = is_dir_or_link_to_dir(t.skills_dir);

                if (name == ".cursor") {
                    t.id = "cursor";
                    t.display_name = "Cursor";
                } else {
                    std::string suffix_raw = name.substr(7);
                    std::string suffix = sanitize_id_suffix(suffix_raw);
                    if (suffix.empty()) suffix = "extra";
                    t.id = "cursor-" + suffix;
                    t.display_name = "Cursor (" + title_case_suffix(suffix_raw) + ")";
                }
                add_target_unique(std::move(t));
                continue;
            }

            // ZCode: ~/.zcode, ~/.zcode-*
            if (name == ".zcode" || name.starts_with(".zcode-") || name.starts_with(".zcode_")) {
                if (!has_any_marker(entry_path, {"skills", "config.json", "settings.json"})) {
                    continue;
                }
                TargetInstance t;
                t.adapter = AdapterKind::ZCode;
                t.group = "Z.ai";
                t.skills_dir = entry_path / "skills";
                t.config_root = entry_path;
                t.link_mode = LinkMode::SymlinkDir;
                t.auto_discovered = true;
                t.configured = is_dir_or_link_to_dir(t.skills_dir);

                if (name == ".zcode") {
                    t.id = "zcode";
                    t.display_name = "ZCode";
                } else {
                    std::string suffix_raw = name.substr(6);
                    std::string suffix = sanitize_id_suffix(suffix_raw);
                    if (suffix.empty()) suffix = "extra";
                    t.id = "zcode-" + suffix;
                    t.display_name = "ZCode (" + title_case_suffix(suffix_raw) + ")";
                }
                add_target_unique(std::move(t));
                continue;
            }
        }
        ::closedir(dp);
    }

    // 2. Scan ~/.config for opencode*
    fs::path dot_config = home_dir / ".config";
    if (DIR* dp = ::opendir(dot_config.c_str())) {
        while (struct dirent* ent = ::readdir(dp)) {
            std::string name(ent->d_name);
            if (name == "." || name == "..") continue;
            if (name != "opencode" && !name.starts_with("opencode-") && !name.starts_with("opencode_")) {
                continue;
            }
            fs::path entry_path = dot_config / name;
            if (!is_dir_or_link_to_dir(entry_path)) continue;
            if (!has_any_marker(entry_path, {"skills", "opencode.json", "opencode.jsonc", "config.json"})) {
                continue;
            }

            TargetInstance t;
            t.adapter = AdapterKind::OpenCode;
            t.group = "OSS";
            t.skills_dir = entry_path / "skills";
            t.config_root = entry_path;
            t.link_mode = LinkMode::SymlinkDir;
            t.auto_discovered = true;
            t.configured = is_dir_or_link_to_dir(t.skills_dir);

            if (name == "opencode") {
                t.id = "opencode";
                t.display_name = "OpenCode";
            } else {
                std::string suffix_raw = name.substr(8);
                std::string suffix = sanitize_id_suffix(suffix_raw);
                if (suffix.empty()) suffix = "extra";
                t.id = "opencode-" + suffix;
                t.display_name = "OpenCode (" + title_case_suffix(suffix_raw) + ")";
                t.env_hint = "OPENCODE_CONFIG_DIR=" + contract_home_path(entry_path, home_dir);
            }

            auto custom_paths = parse_opencode_custom_skill_paths(entry_path, home_dir);
            for (const auto& cp : custom_paths) {
                if (cp != t.skills_dir && is_dir_or_link_to_dir(cp)) {
                    out.opencode_extra_roots.push_back(cp);
                }
            }
            if (!custom_paths.empty() && !t.configured) {
                t.configured = true;
            }
            add_target_unique(std::move(t));
        }
        ::closedir(dp);
    }

    // 3. Scan ~/.codeium for windsurf*
    fs::path dot_codeium = home_dir / ".codeium";
    if (DIR* dp = ::opendir(dot_codeium.c_str())) {
        while (struct dirent* ent = ::readdir(dp)) {
            std::string name(ent->d_name);
            if (name == "." || name == "..") continue;
            if (name != "windsurf" && !name.starts_with("windsurf-") && !name.starts_with("windsurf_")) {
                continue;
            }
            fs::path entry_path = dot_codeium / name;
            if (!is_dir_or_link_to_dir(entry_path)) continue;
            if (!has_any_marker(entry_path, {"skills", "memories", "mcp_config.json", "cascade"})) {
                continue;
            }

            TargetInstance t;
            t.adapter = AdapterKind::Windsurf;
            t.group = "Windsurf";
            t.skills_dir = entry_path / "skills";
            t.config_root = entry_path;
            t.link_mode = LinkMode::SymlinkDir;
            t.auto_discovered = true;
            t.configured = is_dir_or_link_to_dir(t.skills_dir);

            if (name == "windsurf") {
                t.id = "windsurf";
                t.display_name = "Windsurf";
            } else {
                std::string suffix_raw = name.substr(8);
                std::string suffix = sanitize_id_suffix(suffix_raw);
                if (suffix.empty()) suffix = "extra";
                t.id = "windsurf-" + suffix;
                t.display_name = "Windsurf (" + title_case_suffix(suffix_raw) + ")";
            }
            add_target_unique(std::move(t));
        }
        ::closedir(dp);
    }

    std::sort(out.targets.begin(), out.targets.end(), target_less);
    return out;
}

void merge_targets(
    const std::vector<TargetInstance>& lockfile_targets,
    const std::vector<TargetInstance>& discovered,
    std::vector<TargetInstance>& out_targets,
    std::vector<TargetInstance>& out_unconfigured_siblings) {
    out_targets.clear();
    out_unconfigured_siblings.clear();

    std::set<std::string> seen_ids;
    std::set<std::string> seen_paths;

    for (auto t : lockfile_targets) {
        t.configured = true;
        seen_ids.insert(t.id);
        seen_paths.insert(t.skills_dir.lexically_normal().string());
        out_targets.push_back(std::move(t));
    }

    for (const auto& d : discovered) {
        std::string norm_path = d.skills_dir.lexically_normal().string();
        if (seen_ids.contains(d.id) || seen_paths.contains(norm_path)) {
            continue;
        }
        if (d.configured) {
            seen_ids.insert(d.id);
            seen_paths.insert(norm_path);
            out_targets.push_back(d);
        } else {
            out_unconfigured_siblings.push_back(d);
        }
    }

    std::sort(out_targets.begin(), out_targets.end(), target_less);
    std::sort(out_unconfigured_siblings.begin(), out_unconfigured_siblings.end(), target_less);
}

}  // namespace singlet::core
