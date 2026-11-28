#pragma once

#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include "core/sources.hpp"
#include "core/types.hpp"
#include "util/json.hpp"

namespace singlet::core {

/// Stateful domain engine owning lockfile persistence, scanning, symlink wiring,
/// zero-data-loss consolidation, quarantine undo, profiles, and git worktree hydration.
class Engine {
public:
    explicit Engine(EngineConfig config = {});

    [[nodiscard]] const std::filesystem::path& home_dir() const { return home_dir_; }
    [[nodiscard]] std::filesystem::path canonical_skills_dir() const { return home_dir_ / ".agents" / "skills"; }
    [[nodiscard]] std::filesystem::path quarantine_root_dir() const { return home_dir_ / ".agents" / "quarantine"; }
    [[nodiscard]] std::filesystem::path lockfile_path() const { return home_dir_ / ".agents" / "singlet.lock.json"; }

    /// Runs a full filesystem scan and updates the cached catalog snapshot.
    CatalogSnapshot scan(const std::vector<std::filesystem::path>& extra_roots = {});
    /// Returns the cached catalog snapshot, scanning first if none is cached.
    [[nodiscard]] CatalogSnapshot get_catalog();

    /// Creates an atomic managed symlink or safely unlinks a verified managed symlink.
    [[nodiscard]] OperationResult toggle_skill(
        const std::string& slug,
        const std::string& target_id,
        bool enabled);

    /// Computes or applies zero-data-loss consolidation into canonical_skills_dir() with quarantine backup.
    [[nodiscard]] ConsolidateResult consolidate(
        bool dry_run,
        const std::vector<std::string>& promote_repo_paths = {});
    /// Resolves a drifted skill by making chosen_path canonical or forking it to keep_both_slug.
    [[nodiscard]] OperationResult resolve_drift(
        const std::string& slug,
        const std::string& chosen_path,
        const std::string& keep_both_slug = {});
    /// Restores the most recent quarantine batch byte-for-byte from its manifest.json.
    [[nodiscard]] OperationResult undo_last_quarantine();

    /// Installs every skill found at a GitHub URL into canonical_skills_dir() and records its source,
    /// then turns each new skill on in wire_targets. Never replaces an existing skill folder.
    [[nodiscard]] InstallResult install_from_github(
        const std::string& url,
        const std::vector<std::string>& wire_targets = {});
    /// Compares skills (all with a recorded source when slugs is empty) with the latest commit
    /// of their source. Read-only.
    [[nodiscard]] std::vector<SourceUpdateStatus> check_source_updates(const std::vector<std::string>& slugs);
    /// Updates skills from their sources. Skips skills with local edits unless force is set.
    /// Replaced folders go to one quarantine batch, so undo_last_quarantine() restores them.
    [[nodiscard]] std::vector<SourceUpdateStatus> update_from_sources(
        const std::vector<std::string>& slugs,
        bool force);
    /// Returns the recorded sources of installed skills, keyed by slug.
    [[nodiscard]] const std::map<std::string, SkillSource>& skill_sources() const { return skill_sources_; }

    /// Previews or writes Antigravity-compliant YAML frontmatter for one skill.
    [[nodiscard]] FrontmatterFixResult fix_frontmatter(const std::string& slug, bool apply);
    /// Previews or repairs all skills failing strict Antigravity frontmatter validation.
    [[nodiscard]] std::vector<FrontmatterFixResult> fix_all_frontmatter(bool apply);

    /// Lists all configured targets in deterministic display order.
    [[nodiscard]] std::vector<TargetInstance> list_targets();
    /// Registers a custom or sibling target and optionally clones wiring from clone_from_id.
    [[nodiscard]] OperationResult add_target(
        const TargetInstance& target,
        const std::string& clone_from_id = {});
    /// Unlinks managed symlinks in target_id and removes it from persisted targets.
    [[nodiscard]] OperationResult remove_target(const std::string& target_id);
    /// Copies active skill wiring from from_id to to_id.
    [[nodiscard]] OperationResult clone_target(const std::string& from_id, const std::string& to_id);

    /// Returns detailed metadata, markdown, and bounded text previews for all files under a skill.
    [[nodiscard]] SkillDetail get_skill_detail(const std::string& slug);
    /// Saves SKILL.md for slug using atomic write-and-rename.
    [[nodiscard]] OperationResult save_skill_markdown(const std::string& slug, const std::string& markdown);
    /// Saves a relative file inside a skill directory after canonical path-traversal validation.
    [[nodiscard]] OperationResult save_skill_file(
        const std::string& slug,
        const std::string& rel_path,
        const std::string& content);

    /// Returns all persisted wiring profiles.
    [[nodiscard]] std::vector<Profile> get_profiles() const;
    /// Returns the currently active profile name (empty if none selected).
    [[nodiscard]] std::string active_profile() const { return active_profile_; }
    /// Saves a named wiring profile to singlet.lock.json. When both wiring and skills are empty,
    /// snapshots the current live switchboard wiring.
    [[nodiscard]] OperationResult save_profile(
        const std::string& name,
        const std::map<std::string, std::map<std::string, bool>>& wiring = {},
        const std::vector<std::string>& skills = {});
    /// Applies a named profile without touching unmanaged physical copies, broken links, or plugin shadows.
    [[nodiscard]] OperationResult apply_profile(const std::string& name);
    /// Deletes a saved profile from singlet.lock.json.
    [[nodiscard]] OperationResult delete_profile(const std::string& name);

    /// Hydrates active target and repo-local skill symlinks into a git worktree and updates info/exclude.
    [[nodiscard]] WorktreeHydrateResult hydrate_worktree(
        const std::filesystem::path& worktree_path,
        const std::map<std::string, std::string>& env_map,
        const std::filesystem::path& repo_root = {},
        const std::string& provider_id = {});

    /// Reloads persisted targets, scan roots, profiles, and quarantine history from singlet.lock.json.
    bool load_lockfile();
    /// Atomically persists lockfile state to singlet.lock.json.
    bool save_lockfile() const;

private:
    struct QuarantineBatchRecord {
        std::string id;
        std::string timestamp;
        std::filesystem::path dir;
        size_t entry_count = 0;
    };

    EngineConfig config_;
    std::filesystem::path home_dir_;
    std::vector<std::filesystem::path> configured_scan_roots_;
    std::vector<TargetInstance> lockfile_targets_;
    std::vector<std::string> removed_target_ids_;
    std::map<std::string, Profile> profiles_;
    std::string active_profile_;
    std::vector<QuarantineBatchRecord> quarantine_history_;
    std::optional<CatalogSnapshot> cached_snapshot_;
    std::map<std::string, SkillSource> skill_sources_;

    /// A fresh clone of one source repository at one ref, shared by all skills from it.
    struct SourceCheckout {
        std::filesystem::path dir;
        FetchResult fetch;
    };
    /// Quarantine batch collected while updating skills from their sources.
    struct SourceUpdateBatch {
        std::string id;
        std::filesystem::path dir;
        singlet::util::JsonValue entries = singlet::util::JsonValue::array();
    };

    [[nodiscard]] std::optional<TargetInstance> find_target(const std::string& target_id) const;
    [[nodiscard]] std::optional<std::filesystem::path> find_skill_dir(const std::string& slug) const;
    [[nodiscard]] OperationResult write_atomic_symlink(
        const std::filesystem::path& link_path,
        const std::filesystem::path& target_path) const;
    [[nodiscard]] OperationResult remove_managed_symlink(const std::filesystem::path& link_path) const;
    [[nodiscard]] bool is_managed_symlink(const std::filesystem::path& link_path) const;
    void sync_antigravity_skills_json(const TargetInstance& target) const;
    [[nodiscard]] std::string git_host_url() const;
    [[nodiscard]] InstalledSkill install_one_skill(
        const GithubLocation& location,
        const std::string& commit,
        const std::filesystem::path& checkout,
        const std::string& repo_path);
    [[nodiscard]] std::vector<std::string> select_source_slugs(
        const std::vector<std::string>& slugs,
        std::vector<SourceUpdateStatus>& failures) const;
    [[nodiscard]] bool has_local_changes(const std::string& slug, const SkillSource& source) const;
    [[nodiscard]] SourceUpdateStatus update_one_source(
        const std::string& slug,
        bool force,
        const SourceCheckout& checkout,
        SourceUpdateBatch& batch);
    void record_quarantine_batch(const SourceUpdateBatch& batch);
    [[nodiscard]] OperationResult restore_replaced_skill(
        const singlet::util::JsonValue& entry,
        const std::filesystem::path& batch_dir);
};

/// Serializes a CatalogSnapshot matching the REST and JSON-RPC 2.0 contract.
[[nodiscard]] singlet::util::JsonValue to_catalog_json(const CatalogSnapshot& snap);
/// Serializes a TargetInstance to JSON.
[[nodiscard]] singlet::util::JsonValue to_target_json(const TargetInstance& target);
/// Serializes a SkillDetail (including multi-file `files: [{path, size, isText, content}]`) to JSON.
[[nodiscard]] singlet::util::JsonValue to_skill_detail_json(const SkillDetail& detail);
/// Serializes a ConsolidateResult to JSON.
[[nodiscard]] singlet::util::JsonValue to_consolidate_result_json(const ConsolidateResult& res);
/// Serializes a FrontmatterFixResult to JSON.
[[nodiscard]] singlet::util::JsonValue to_frontmatter_fix_json(const FrontmatterFixResult& res);
/// Serializes a WorktreeHydrateResult to JSON.
[[nodiscard]] singlet::util::JsonValue to_worktree_hydrate_json(const WorktreeHydrateResult& res);
/// Serializes an InstallResult to JSON.
[[nodiscard]] singlet::util::JsonValue to_install_result_json(const InstallResult& res);
/// Serializes source check or update results to a JSON array.
[[nodiscard]] singlet::util::JsonValue to_source_statuses_json(const std::vector<SourceUpdateStatus>& statuses);
/// Serializes an OperationResult to JSON.
[[nodiscard]] singlet::util::JsonValue to_operation_result_json(const OperationResult& res);
/// Serializes saved profiles and active profile name to JSON.
[[nodiscard]] singlet::util::JsonValue to_profiles_json(
    const std::vector<Profile>& profiles,
    std::string_view active_profile);

}  // namespace singlet::core
