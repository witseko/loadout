#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "util/frontmatter.hpp"

namespace singlet::core {

/// Adapter rule set governing frontmatter strictness and link conventions.
enum class AdapterKind {
    Antigravity,
    ClaudeCode,
    Codex,
    OpenCode,
    ZCode,
    Cursor,
    Windsurf,
    CustomLenient,
    CustomStrict,
};

/// Mechanism used to materialize skills inside a target instance.
enum class LinkMode {
    SymlinkDir,
    AntigravitySkillsJson,
};

/// Filesystem classification of a single discovered skill entry.
enum class EntryKind {
    Canonical,
    ManagedSymlink,
    ExternalSymlink,
    DanglingSymlink,
    PhysicalCopy,
    PluginShadow,
};

/// Returns the canonical lowercase kebab-case identifier for an adapter kind.
[[nodiscard]] std::string_view adapter_to_string(AdapterKind kind);
/// Parses an adapter kind from a case-insensitive name or alias.
[[nodiscard]] std::optional<AdapterKind> adapter_from_string(std::string_view str);
/// Returns true if the adapter drops skills without strict Antigravity YAML frontmatter.
[[nodiscard]] bool adapter_requires_strict_frontmatter(AdapterKind kind);
/// Returns the default UI vendor group label for an adapter kind.
[[nodiscard]] std::string_view adapter_default_group(AdapterKind kind);

/// Returns the serialized identifier for a link mode.
[[nodiscard]] std::string_view link_mode_to_string(LinkMode mode);
/// Parses a link mode from a string identifier.
[[nodiscard]] std::optional<LinkMode> link_mode_from_string(std::string_view str);

/// Returns the serialized kebab-case identifier for an entry kind.
[[nodiscard]] std::string_view entry_kind_to_string(EntryKind kind);

/// A concrete agent installation or account profile on disk.
struct TargetInstance {
    std::string id;
    std::string display_name;
    std::string group;
    AdapterKind adapter = AdapterKind::CustomLenient;
    std::filesystem::path skills_dir;
    std::optional<std::filesystem::path> config_root;
    std::optional<std::string> env_hint;
    LinkMode link_mode = LinkMode::SymlinkDir;
    bool auto_discovered = false;
    bool configured = true;
};

/// One discovered instance of a skill on disk (canonical, symlink, copy, repo, or plugin).
struct SkillOccurrence {
    std::string slug;
    std::filesystem::path path;
    std::filesystem::path real_path;
    std::string target_id_or_repo;
    EntryKind entry_kind = EntryKind::PhysicalCopy;
    bool is_repo_local = false;
    std::filesystem::path repo_root;
    std::string tree_hash;
    size_t file_count = 0;
    uint64_t total_bytes = 0;
    int64_t mtime_epoch = 0;
    singlet::util::FrontmatterInfo frontmatter;
    std::string symlink_target;
};

/// Per-target wiring state for a single skill in the switchboard matrix.
struct TargetSkillState {
    bool enabled = false;
    bool has_entry = false;
    EntryKind entry_kind = EntryKind::ManagedSymlink;
    std::string path;
    bool incompatible = false;
};

/// Aggregated catalog record for a unique skill slug across all targets and repositories.
struct CanonicalSkill {
    std::string slug;
    std::string name;
    std::string description;
    std::vector<std::string> tags;
    std::filesystem::path canonical_path;
    bool is_canonical = false;
    std::string tree_hash;
    size_t file_count = 0;
    uint64_t total_bytes = 0;
    size_t approx_tokens = 0;
    bool antigravity_valid = true;
    std::vector<std::string> frontmatter_issues;
    bool has_scripts = false;
    bool has_references = false;
    bool is_repo_local = false;
    std::string repo_root;
    bool has_duplicates = false;
    bool has_drift = false;
    bool has_physical_copy = false;
    bool has_plugin_shadow = false;
    std::map<std::string, TargetSkillState> target_states;
    std::vector<SkillOccurrence> occurrences;
};

/// Exact byte-identical physical duplicates sharing one Merkle tree hash.
struct DuplicateGroup {
    std::string slug;
    std::string tree_hash;
    uint64_t redundant_bytes = 0;
    std::vector<SkillOccurrence> occurrences;
};

/// One distinct content variant in a drifted skill group.
struct DriftedVariant {
    SkillOccurrence occurrence;
    std::string skill_md_preview;
    std::string unified_diff_from_primary;
    std::vector<std::string> files;
};

/// Multiple physical copies of the same slug whose tree hashes diverge.
struct DriftedGroup {
    std::string slug;
    std::vector<DriftedVariant> variants;
    std::string unified_diff;
};

/// One planned or executed filesystem step during consolidation.
struct ConsolidateAction {
    std::string slug;
    std::string kind;  // "promote", "quarantine_and_link", "relink", "keep_repo_local"
    std::filesystem::path source_path;
    std::filesystem::path dest_path;
    std::filesystem::path quarantine_path;
    bool is_repo_local = false;
    std::string target_id;
    std::string reason;
};

/// Dry-run or applied plan summarizing all consolidation operations.
struct ConsolidatePlan {
    bool dry_run = true;
    std::vector<ConsolidateAction> actions;
    size_t skills_promoted = 0;
    size_t copies_quarantined = 0;
    size_t symlinks_created = 0;
    uint64_t bytes_deduplicated = 0;
};

/// Result of running consolidation, including quarantine batch metadata when applied.
struct ConsolidateResult {
    bool ok = true;
    std::string error;
    std::string quarantine_batch_id;
    std::filesystem::path quarantine_dir;
    ConsolidatePlan plan;
};

/// Generic status result for mutating engine operations.
struct OperationResult {
    bool ok = true;
    std::string error;
    std::string message;
};

/// Preview or applied result of repairing a skill's YAML frontmatter.
struct FrontmatterFixResult {
    bool ok = true;
    std::string error;
    std::string slug;
    std::filesystem::path file_path;
    bool changed = false;
    bool applied = false;
    std::string before;
    std::string after;
    std::string diff;
};

/// File metadata and bounded UTF-8 text preview for one file inside a skill directory.
struct SkillFileEntry {
    std::string path;
    uint64_t size = 0;
    bool is_text = true;
    std::string content;
};

/// Detailed inspector payload for a single skill, including its multi-file tree.
struct SkillDetail {
    bool found = false;
    std::string error;
    CanonicalSkill skill;
    std::string markdown;
    std::vector<std::string> files;
    std::vector<SkillFileEntry> file_entries;
};

/// Named wiring preset persisted in singlet.lock.json.
struct Profile {
    std::string name;
    std::vector<std::string> skills;
    std::map<std::string, std::map<std::string, bool>> wiring;
};

/// Result of hydrating skill symlinks and git exclude rules into a worktree.
struct WorktreeHydrateResult {
    bool ok = true;
    std::string error;
    std::string matched_target_id;
    std::vector<std::string> linked_skills;
    std::vector<std::string> excluded_entries;
    std::filesystem::path exclude_file;
};

/// Aggregate counts displayed in headers, CLI tables, and the scan wizard.
struct CatalogStats {
    size_t total_skills = 0;
    size_t canonical_skills = 0;
    size_t total_targets = 0;
    size_t total_repos = 0;
    size_t exact_duplicates = 0;
    uint64_t duplicate_bytes = 0;
    size_t drifted_groups = 0;
    size_t broken_links = 0;
    size_t frontmatter_issues = 0;
    size_t unmanaged_copies = 0;
    size_t project_local_skills = 0;
};

/// Immutable point-in-time snapshot of all discovered targets, skills, issues, and profiles.
struct CatalogSnapshot {
    CatalogStats stats;
    std::filesystem::path canonical_root;
    double last_scan_ms = 0.0;
    bool can_undo = false;
    std::string last_quarantine_batch;
    std::string active_profile;
    std::vector<Profile> profiles;
    std::vector<TargetInstance> unconfigured_siblings;
    std::vector<TargetInstance> targets;
    std::vector<CanonicalSkill> skills;
    std::vector<DuplicateGroup> duplicate_groups;
    std::vector<DriftedGroup> drifted_groups;
    std::vector<SkillOccurrence> broken_links;
    std::vector<std::filesystem::path> scan_roots;
};

/// Configuration injected into Engine so tests run in isolated home directories.
struct EngineConfig {
    std::filesystem::path home_dir{};
    std::vector<std::filesystem::path> scan_roots{};
    size_t max_scan_depth = 6;
    size_t worker_threads = 0;
};

/// Resolves the effective home directory: explicit override, SINGLET_HOME env var, then HOME.
[[nodiscard]] std::filesystem::path resolve_home_dir(const std::filesystem::path& explicit_home = {});
/// Expands a leading '~' relative to home_dir and normalizes lexically.
[[nodiscard]] std::filesystem::path expand_home_path(std::string_view raw_path, const std::filesystem::path& home_dir);
/// Contracts a leading home_dir prefix to '~' for portable display and lockfile storage.
[[nodiscard]] std::string contract_home_path(const std::filesystem::path& path, const std::filesystem::path& home_dir);

}  // namespace singlet::core
