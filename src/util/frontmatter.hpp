#pragma once

/// YAML frontmatter parser and non-destructive Antigravity compliance repair for SKILL.md files.

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace singlet::util {

/// Parsed metadata, token estimate, and Antigravity validation diagnostics for a SKILL.md document.
struct FrontmatterInfo {
    bool has_frontmatter = false;
    std::string name;
    std::string description;
    std::vector<std::string> tags;
    size_t approx_tokens = 0;
    bool antigravity_valid = false;
    std::vector<std::string> issues;
    std::string body;
};

/// Parses YAML frontmatter from SKILL.md content (quoted scalars, folded >, literal |, inline/block tags).
/// Sets antigravity_valid when has_frontmatter && !name.empty() && name == folder_slug && !description.empty().
[[nodiscard]] FrontmatterInfo parse_skill_markdown(std::string_view content, std::string_view folder_slug);

/// Non-destructively repairs or injects compliant YAML frontmatter with name == folder_slug
/// and a non-empty description while preserving all other frontmatter keys and markdown body.
[[nodiscard]] std::string fix_skill_frontmatter(std::string_view content, std::string_view folder_slug);

} // namespace singlet::util
