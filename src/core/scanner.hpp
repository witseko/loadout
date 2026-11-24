#pragma once

#include <cstddef>
#include <filesystem>
#include <vector>

#include "core/types.hpp"

namespace loadout::core {

/// Inputs controlling directory traversal, pruning, and worker concurrency.
struct ScanOptions {
    std::filesystem::path home_dir{};
    std::filesystem::path canonical_root{};
    std::vector<TargetInstance> targets{};
    std::vector<std::filesystem::path> plugin_skill_roots{};
    std::vector<std::filesystem::path> extra_skill_roots{};
    std::vector<std::filesystem::path> repo_scan_roots{};
    size_t max_depth = 6;
    size_t worker_threads = 0;  // 0 = auto
};

/// Raw filesystem discovery result before catalog aggregation.
struct ScanResult {
    std::vector<SkillOccurrence> occurrences;
    std::vector<SkillOccurrence> broken_links;
    std::vector<std::filesystem::path> discovered_repos;
    double scan_duration_ms = 0.0;
};

/// Parallel work-stealing scanner using std::jthread and POSIX opendir/readdir d_type pruning.
/// Skips linked git worktrees (a `.git` file pointing into `.git/worktrees/`) unless one is a scan root.
/// Never enters node_modules, .git, target, .venv, build, dist, .next, Pods, or macOS TCC
/// directories (Library, Pictures, Music, Movies, .Trash, /Volumes).
/// Tracks visited (st_dev, st_ino) pairs to terminate circular symlinks immediately.
[[nodiscard]] ScanResult scan_filesystem(const ScanOptions& options);

}  // namespace loadout::core
