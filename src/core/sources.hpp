#pragma once

/// Remote skill sources: GitHub URL parsing, shallow git fetches, and skill discovery in a checkout.

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "util/json.hpp"

namespace singlet::core {

/// Public GitHub host used for installs and updates.
inline constexpr std::string_view DEFAULT_GIT_HOST_URL = "https://github.com";

/// A GitHub location parsed from a URL such as https://github.com/owner/repo/tree/main/skills.
struct GithubLocation {
    std::string repo;  ///< "owner/name"
    std::string ref;   ///< Branch or tag; empty means the default branch.
    std::string path;  ///< Folder inside the repository; empty means the repository root.
};

/// Where an installed skill came from, persisted in singlet.lock.json under "sources".
struct SkillSource {
    std::string repo;       ///< "owner/name"
    std::string ref;        ///< Branch or tag; empty means the default branch.
    std::string path;       ///< Skill folder inside the repository; empty means the repository root.
    std::string commit;     ///< Commit the installed files came from.
    std::string tree_hash;  ///< Tree hash of the files as installed, used to detect local edits.
    std::string updated_at; ///< ISO 8601 UTC time of the last install or update.
};

/// One skill folder found by an install.
struct InstalledSkill {
    std::string slug;
    std::string repo_path;  ///< Folder inside the repository.
    bool installed = false;
    std::string message;    ///< Why it was skipped, when installed is false.
    std::vector<std::string> wired_targets;  ///< Targets the skill was turned on in.
    std::vector<std::string> wire_errors;    ///< "<target>: <error>" for targets that failed.
};

/// Result of installing skills from a GitHub URL.
struct InstallResult {
    bool ok = false;
    std::string error;
    std::string repo;
    std::string commit;
    std::vector<InstalledSkill> skills;
};

/// State of one installed skill compared with its source.
enum class SourceState {
    UpToDate,
    UpdateAvailable,
    LocalChanges,  ///< Files differ from what was installed; update needs force.
    Updated,
    Failed,
};

/// Check or update result for one skill with a recorded source.
struct SourceUpdateStatus {
    std::string slug;
    std::string repo;
    std::string ref;
    std::string installed_commit;
    std::string latest_commit;
    SourceState state = SourceState::UpToDate;
    std::string message;
};

/// Result of a shallow clone.
struct FetchResult {
    bool ok = false;
    std::string error;
    std::string commit;
};

/// Parses a GitHub repository, tree, or blob URL. Accepts the forms
/// `[https://]github.com/<owner>/<repo>[.git]` and `.../tree|blob/<ref>[/<path>]`.
/// A blob URL that ends in SKILL.md points at its folder. The ref is one path segment,
/// so branch names that contain '/' are not supported. Returns std::nullopt and sets error on bad input.
[[nodiscard]] std::optional<GithubLocation> parse_github_url(std::string_view url, std::string& error);

/// Shallow-clones repo at ref (default branch when empty) from host_url into dest, which must not exist.
/// Runs `git` without a shell, without terminal prompts, and only over the scheme of host_url.
[[nodiscard]] FetchResult clone_repo(
    std::string_view host_url,
    const std::string& repo,
    const std::string& ref,
    const std::filesystem::path& dest);

/// Returns the commit that ref (default branch when empty) points to on the remote, using `git ls-remote`.
[[nodiscard]] FetchResult remote_commit(std::string_view host_url, const std::string& repo, const std::string& ref);

/// Lists skill folders (folders with SKILL.md) under checkout/subpath, as paths relative to checkout.
/// If subpath itself is a skill, returns only it. Skips .git, node_modules, and symlinks. Sorted.
[[nodiscard]] std::vector<std::string> find_skill_paths(
    const std::filesystem::path& checkout,
    const std::string& subpath);

/// Returns the slug for a skill at repo_path: its folder name, or the repository name for the root.
[[nodiscard]] std::string slug_for_skill_path(const std::string& repo, const std::string& repo_path);

/// Copies regular files and folders from src to dst (which must not exist). Skips symlinks and .git,
/// so a skill can never point outside its folder. Returns false and sets err on failure.
[[nodiscard]] bool copy_skill_tree(
    const std::filesystem::path& src,
    const std::filesystem::path& dst,
    std::string& err);

/// Returns the lowercase name of a SourceState, for example "update-available".
[[nodiscard]] std::string_view source_state_to_string(SourceState state);

/// Serializes a SkillSource for the lockfile.
[[nodiscard]] singlet::util::JsonValue to_skill_source_json(const SkillSource& source);
/// Reads a SkillSource from lockfile JSON; returns std::nullopt if repo is missing.
[[nodiscard]] std::optional<SkillSource> skill_source_from_json(const singlet::util::JsonValue& value);

}  // namespace singlet::core
