#pragma once

#include <filesystem>
#include <vector>

#include "core/types.hpp"

namespace loadout::core {

/// Output of target auto-discovery, including plugin shadow and OpenCode custom skill roots.
struct DiscoveredTargets {
    std::vector<TargetInstance> targets;
    std::vector<std::filesystem::path> plugin_skill_roots;
    std::vector<std::filesystem::path> opencode_extra_roots;
};

/// Discovers default targets and multi-account siblings under home_dir and home_dir/.config:
/// ~/.claude*, ~/.codex*, ~/.gemini* (+ config/plugins/*/skills), ~/.cursor*,
/// ~/.config/opencode* (+ opencode.json / opencode.jsonc custom skill paths),
/// ~/.zcode*, and ~/.codeium/windsurf*.
[[nodiscard]] DiscoveredTargets discover_targets(const std::filesystem::path& home_dir);

/// Merges persisted targets from loadout.lock.json with freshly discovered targets.
/// Populates out_targets and out_unconfigured_siblings (siblings detected on disk
/// without a skills/ directory yet).
void merge_targets(
    const std::vector<TargetInstance>& lockfile_targets,
    const std::vector<TargetInstance>& discovered,
    std::vector<TargetInstance>& out_targets,
    std::vector<TargetInstance>& out_unconfigured_siblings);

/// Parses opencode.json and opencode.jsonc (with comments) for custom skill directory paths.
[[nodiscard]] std::vector<std::filesystem::path> parse_opencode_custom_skill_paths(
    const std::filesystem::path& opencode_config_dir,
    const std::filesystem::path& home_dir);

}  // namespace loadout::core
