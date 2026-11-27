#include "core/scanner.hpp"

#include <dirent.h>
#include <limits.h>
#include <sys/stat.h>
#include <unistd.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <fstream>
#include <mutex>
#include <set>
#include <sstream>
#include <thread>
#include <unordered_map>
#include <unordered_set>

#include "util/frontmatter.hpp"
#include "util/sha256.hpp"

namespace singlet::core {

namespace fs = std::filesystem;

namespace {

struct DevIno {
    dev_t dev;
    ino_t ino;
    bool operator==(const DevIno& o) const noexcept {
        return dev == o.dev && ino == o.ino;
    }
};

struct DevInoHash {
    size_t operator()(const DevIno& k) const noexcept {
        size_t h1 = std::hash<uint64_t>{}(static_cast<uint64_t>(k.dev));
        size_t h2 = std::hash<uint64_t>{}(static_cast<uint64_t>(k.ino));
        return h1 ^ (h2 + 0x9e3779b97f4a7c15ULL + (h1 << 6) + (h1 >> 2));
    }
};

bool is_pruned_dir_name(std::string_view name) {
    // Build/VCS heavy directories + macOS TCC-protected directories
    static const std::unordered_set<std::string_view> kPruned = {
        ".git",
        "node_modules",
        "target",
        ".venv",
        "venv",
        "build",
        "dist",
        ".next",
        ".nuxt",
        "Pods",
        "DerivedData",
        "__pycache__",
        ".cache",
        ".npm",
        ".cargo",
        ".rustup",
        ".gradle",
        ".idea",
        ".tox",
        "coverage",
        ".turbo",
        ".svelte-kit",
        "bower_components",
        // macOS TCC & system directories
        "Library",
        "Pictures",
        "Music",
        "Movies",
        ".Trash",
        "Volumes",
    };
    return kPruned.contains(name);
}

bool is_agent_dot_dir(std::string_view name) {
    return name == ".claude" || name == ".agents" || name == ".codex" ||
           name == ".opencode" || name == ".cursor" || name == ".gemini" ||
           name == ".zcode" || name == ".windsurf" || name == ".codeium" ||
           name.starts_with(".claude-") || name.starts_with(".codex-");
}

bool is_volumes_path(const fs::path& p) {
    std::string s = p.string();
    return s == "/Volumes" || s.starts_with("/Volumes/");
}

std::string read_symlink_raw(const fs::path& p) {
    std::string buf(PATH_MAX, '\0');
    ssize_t len = ::readlink(p.c_str(), buf.data(), buf.size() - 1);
    if (len < 0) return {};
    buf.resize(static_cast<size_t>(len));
    return buf;
}

fs::path safe_weakly_canonical(const fs::path& p) {
    std::error_code ec;
    auto c = fs::weakly_canonical(p, ec);
    return ec ? p.lexically_normal() : c;
}

bool is_inside_dir(const fs::path& candidate, const fs::path& parent) {
    std::string c = safe_weakly_canonical(candidate).string();
    std::string p = safe_weakly_canonical(parent).string();
    while (p.size() > 1 && p.back() == '/') p.pop_back();
    return !p.empty() && (c == p || c.starts_with(p + "/"));
}

fs::path find_skill_md_file(const fs::path& dir) {
    struct stat st {};
    fs::path upper = dir / "SKILL.md";
    if (::stat(upper.c_str(), &st) == 0 && S_ISREG(st.st_mode)) {
        return upper;
    }
    fs::path lower = dir / "skill.md";
    if (::stat(lower.c_str(), &st) == 0 && S_ISREG(st.st_mode)) {
        return lower;
    }
    return {};
}

std::string read_small_file(const fs::path& p) {
    std::ifstream in(p, std::ios::in | std::ios::binary);
    if (!in) return {};
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

/// True when `git_entry` is the `.git` file of a linked worktree (`gitdir: <repo>/.git/worktrees/<name>`).
/// Main checkouts have a `.git` directory, and submodules point at `.git/modules/<name>`.
bool is_linked_worktree_git_file(const fs::path& git_entry, unsigned char dtype) {
    if (dtype == DT_DIR) return false;
    struct stat lst {};
    if (::lstat(git_entry.c_str(), &lst) != 0 || !S_ISREG(lst.st_mode)) return false;
    std::string content = read_small_file(git_entry);
    return content.starts_with("gitdir:") && content.find("/worktrees/") != std::string::npos;
}

int64_t get_dir_mtime_epoch(const fs::path& dir, const fs::path& skill_md) {
    struct stat st {};
    int64_t best = 0;
    if (!skill_md.empty() && ::stat(skill_md.c_str(), &st) == 0) {
        best = static_cast<int64_t>(st.st_mtime);
    }
    if (::stat(dir.c_str(), &st) == 0) {
        best = std::max(best, static_cast<int64_t>(st.st_mtime));
    }
    return best;
}

struct CachedSkillMeta {
    singlet::util::TreeDigest digest;
    singlet::util::FrontmatterInfo frontmatter;
    int64_t mtime_epoch = 0;
    bool valid = false;
};

class SkillMetaCache {
public:
    CachedSkillMeta get_or_compute(const fs::path& real_dir, std::string_view slug) {
        std::string key = real_dir.string() + "\n" + std::string(slug);
        {
            std::lock_guard<std::mutex> lk(mu_);
            if (auto it = cache_.find(key); it != cache_.end()) {
                return it->second;
            }
        }

        CachedSkillMeta meta;
        fs::path skill_md = find_skill_md_file(real_dir);
        if (!skill_md.empty()) {
            meta.valid = true;
            std::string content = read_small_file(skill_md);
            meta.frontmatter = singlet::util::parse_skill_markdown(content, slug);
            meta.digest = singlet::util::hash_skill_tree(real_dir);
            meta.mtime_epoch = get_dir_mtime_epoch(real_dir, skill_md);
        }

        {
            std::lock_guard<std::mutex> lk(mu_);
            cache_.emplace(std::move(key), meta);
        }
        return meta;
    }

private:
    std::mutex mu_;
    std::unordered_map<std::string, CachedSkillMeta> cache_;
};

// Scans a skills container directory (e.g. ~/.claude/skills, ~/.agents/skills, <repo>/.claude/skills)
void scan_skills_container(
    const fs::path& container_dir,
    const fs::path& canonical_root,
    const std::string& owner_id,
    bool is_canonical_store,
    bool is_plugin_store,
    bool is_repo_local,
    const fs::path& repo_root,
    SkillMetaCache& meta_cache,
    std::mutex& out_mu,
    std::vector<SkillOccurrence>& out_occurrences,
    std::vector<SkillOccurrence>& out_broken_links) {
    DIR* dp = ::opendir(container_dir.c_str());
    if (!dp) return;

    std::vector<SkillOccurrence> local_occ;
    std::vector<SkillOccurrence> local_broken;

    while (struct dirent* ent = ::readdir(dp)) {
        std::string name(ent->d_name);
        if (name == "." || name == ".." || name == ".DS_Store" || name == "Thumbs.db") {
            continue;
        }
        if (name.starts_with(".singlet-tmp-")) {
            continue;
        }

        fs::path entry_path = container_dir / name;
        struct stat lst {};
        if (::lstat(entry_path.c_str(), &lst) != 0) {
            continue;
        }

        if (S_ISLNK(lst.st_mode)) {
            std::string raw_target = read_symlink_raw(entry_path);
            struct stat st {};
            if (::stat(entry_path.c_str(), &st) != 0 || !S_ISDIR(st.st_mode)) {
                SkillOccurrence broken;
                broken.slug = name;
                broken.path = entry_path;
                broken.real_path = entry_path;
                broken.target_id_or_repo = owner_id;
                broken.entry_kind = EntryKind::DanglingSymlink;
                broken.is_repo_local = is_repo_local;
                broken.repo_root = repo_root;
                broken.symlink_target = raw_target;
                broken.mtime_epoch = static_cast<int64_t>(lst.st_mtime);
                local_broken.push_back(std::move(broken));
                continue;
            }

            fs::path real_p = safe_weakly_canonical(entry_path);
            auto meta = meta_cache.get_or_compute(real_p, name);
            if (!meta.valid) {
                continue;
            }

            SkillOccurrence occ;
            occ.slug = name;
            occ.path = entry_path;
            occ.real_path = real_p;
            occ.target_id_or_repo = owner_id;
            occ.is_repo_local = is_repo_local;
            occ.repo_root = repo_root;
            occ.symlink_target = raw_target;
            if (is_plugin_store) {
                occ.entry_kind = EntryKind::PluginShadow;
            } else if (is_inside_dir(real_p, canonical_root)) {
                occ.entry_kind = is_canonical_store ? EntryKind::Canonical : EntryKind::ManagedSymlink;
            } else {
                occ.entry_kind = EntryKind::ExternalSymlink;
            }
            occ.tree_hash = meta.digest.hex_hash;
            occ.file_count = meta.digest.file_count;
            occ.total_bytes = meta.digest.total_bytes;
            occ.mtime_epoch = meta.mtime_epoch;
            occ.frontmatter = meta.frontmatter;
            local_occ.push_back(std::move(occ));
            continue;
        }

        if (S_ISDIR(lst.st_mode)) {
            fs::path real_p = safe_weakly_canonical(entry_path);
            auto meta = meta_cache.get_or_compute(real_p, name);
            if (!meta.valid) {
                continue;
            }

            SkillOccurrence occ;
            occ.slug = name;
            occ.path = entry_path;
            occ.real_path = real_p;
            occ.target_id_or_repo = owner_id;
            occ.is_repo_local = is_repo_local;
            occ.repo_root = repo_root;
            if (is_canonical_store) {
                occ.entry_kind = EntryKind::Canonical;
            } else if (is_plugin_store) {
                occ.entry_kind = EntryKind::PluginShadow;
            } else {
                occ.entry_kind = EntryKind::PhysicalCopy;
            }
            occ.tree_hash = meta.digest.hex_hash;
            occ.file_count = meta.digest.file_count;
            occ.total_bytes = meta.digest.total_bytes;
            occ.mtime_epoch = meta.mtime_epoch;
            occ.frontmatter = meta.frontmatter;
            local_occ.push_back(std::move(occ));
        }
    }
    ::closedir(dp);

    if (!local_occ.empty() || !local_broken.empty()) {
        std::lock_guard<std::mutex> lk(out_mu);
        for (auto& o : local_occ) out_occurrences.push_back(std::move(o));
        for (auto& b : local_broken) out_broken_links.push_back(std::move(b));
    }
}

struct CrawlTask {
    fs::path dir;
    size_t depth = 0;
    fs::path repo_root;
    bool is_scan_root = false;
};

}  // namespace

ScanResult scan_filesystem(const ScanOptions& options) {
    auto start_time = std::chrono::steady_clock::now();

    ScanResult result;
    SkillMetaCache meta_cache;
    std::mutex out_mu;

    // 1. Scan canonical store (~/.agents/skills)
    scan_skills_container(
        options.canonical_root,
        options.canonical_root,
        "canonical",
        /*is_canonical_store=*/true,
        /*is_plugin_store=*/false,
        /*is_repo_local=*/false,
        {},
        meta_cache,
        out_mu,
        result.occurrences,
        result.broken_links);

    // 2. Scan all target skill directories
    std::set<std::string> scanned_containers;
    scanned_containers.insert(safe_weakly_canonical(options.canonical_root).string());

    for (const auto& target : options.targets) {
        std::string key = target.skills_dir.lexically_normal().string();
        scanned_containers.insert(key);
        scan_skills_container(
            target.skills_dir,
            options.canonical_root,
            target.id,
            /*is_canonical_store=*/false,
            /*is_plugin_store=*/false,
            /*is_repo_local=*/false,
            {},
            meta_cache,
            out_mu,
            result.occurrences,
            result.broken_links);
    }

    // 3. Scan read-only plugin skill directories (e.g., ~/.gemini/config/plugins/*/skills)
    for (const auto& plugin_root : options.plugin_skill_roots) {
        std::string plugin_name = "plugin";
        if (plugin_root.has_parent_path()) {
            plugin_name = "plugin:" + plugin_root.parent_path().filename().string();
        }
        scan_skills_container(
            plugin_root,
            options.canonical_root,
            plugin_name,
            /*is_canonical_store=*/false,
            /*is_plugin_store=*/true,
            /*is_repo_local=*/false,
            {},
            meta_cache,
            out_mu,
            result.occurrences,
            result.broken_links);
    }

    // 4. Scan extra skill roots (e.g., opencode.jsonc custom paths)
    for (const auto& extra_root : options.extra_skill_roots) {
        std::string key = extra_root.lexically_normal().string();
        if (!scanned_containers.insert(key).second) continue;

        // If extra_root itself is a single skill directory (contains SKILL.md)
        fs::path skill_md = find_skill_md_file(extra_root);
        if (!skill_md.empty()) {
            fs::path real_p = safe_weakly_canonical(extra_root);
            std::string slug = extra_root.filename().string();
            auto meta = meta_cache.get_or_compute(real_p, slug);
            if (meta.valid) {
                SkillOccurrence occ;
                occ.slug = slug;
                occ.path = extra_root;
                occ.real_path = real_p;
                occ.target_id_or_repo = "opencode";
                occ.entry_kind = is_inside_dir(real_p, options.canonical_root)
                                     ? EntryKind::ManagedSymlink
                                     : EntryKind::PhysicalCopy;
                occ.tree_hash = meta.digest.hex_hash;
                occ.file_count = meta.digest.file_count;
                occ.total_bytes = meta.digest.total_bytes;
                occ.mtime_epoch = meta.mtime_epoch;
                occ.frontmatter = meta.frontmatter;
                result.occurrences.push_back(std::move(occ));
            }
        } else {
            scan_skills_container(
                extra_root,
                options.canonical_root,
                "opencode",
                /*is_canonical_store=*/false,
                /*is_plugin_store=*/false,
                /*is_repo_local=*/false,
                {},
                meta_cache,
                out_mu,
                result.occurrences,
                result.broken_links);
        }
    }

    // 5. Parallel work-stealing crawl of developer workspace / git repo roots
    std::mutex q_mu;
    std::condition_variable q_cv;
    std::deque<CrawlTask> queue;
    size_t active_workers = 0;

    std::mutex visited_mu;
    std::unordered_set<DevIno, DevInoHash> visited_inodes;
    std::set<std::string> discovered_repos_set;

    auto mark_visited = [&](const fs::path& dir) -> bool {
        struct stat st {};
        if (::stat(dir.c_str(), &st) != 0 || !S_ISDIR(st.st_mode)) {
            return false;
        }
        DevIno key{st.st_dev, st.st_ino};
        std::lock_guard<std::mutex> lk(visited_mu);
        return visited_inodes.insert(key).second;
    };

    for (const auto& root : options.repo_scan_roots) {
        if (root.empty() || is_volumes_path(root)) continue;
        if (mark_visited(root)) {
            queue.push_back(CrawlTask{root, 0, {}, /*is_scan_root=*/true});
        }
    }

    if (!queue.empty()) {
        size_t num_threads = options.worker_threads;
        if (num_threads == 0) {
            unsigned hw = std::thread::hardware_concurrency();
            num_threads = hw > 0 ? std::min<size_t>(hw, 8) : 4;
        }
        num_threads = std::max<size_t>(1, num_threads);

        auto worker_fn = [&]() {
            while (true) {
                CrawlTask task;
                {
                    std::unique_lock<std::mutex> lk(q_mu);
                    q_cv.wait(lk, [&]() { return !queue.empty() || active_workers == 0; });
                    if (queue.empty()) {
                        return;
                    }
                    task = std::move(queue.front());
                    queue.pop_front();
                    ++active_workers;
                }

                std::vector<CrawlTask> new_tasks;
                DIR* dp = ::opendir(task.dir.c_str());
                if (dp) {
                    bool has_git = false;
                    bool is_linked_worktree = false;
                    std::vector<std::pair<std::string, unsigned char>> entries;

                    while (struct dirent* ent = ::readdir(dp)) {
                        std::string name(ent->d_name);
                        if (name == "." || name == "..") continue;
                        if (name == ".git") {
                            has_git = true;
                            is_linked_worktree = is_linked_worktree_git_file(task.dir / name, ent->d_type);
                            continue;
                        }
                        entries.emplace_back(std::move(name), ent->d_type);
                    }
                    ::closedir(dp);

                    // Linked worktrees mirror their main checkout, so their skills are not separate copies.
                    // A worktree the user passed as a scan root is still scanned.
                    if (is_linked_worktree && !task.is_scan_root) {
                        entries.clear();
                        has_git = false;
                    }

                    fs::path effective_repo = task.repo_root;
                    if (has_git) {
                        effective_repo = task.dir;
                        std::lock_guard<std::mutex> lk(out_mu);
                        if (discovered_repos_set.insert(task.dir.string()).second) {
                            result.discovered_repos.push_back(task.dir);
                        }
                    }

                    std::string dir_name = task.dir.filename().string();
                    bool inside_agent_dot = is_agent_dot_dir(dir_name);

                    for (const auto& [name, dtype] : entries) {
                        // Zero-stat pruning via dirent.d_type
                        if (dtype == DT_REG || dtype == DT_FIFO || dtype == DT_SOCK ||
                            dtype == DT_CHR || dtype == DT_BLK) {
                            continue;
                        }

                        if (is_pruned_dir_name(name)) {
                            continue;
                        }

                        // Never follow general directory symlinks during repo crawl to avoid escapes/loops
                        if (dtype == DT_LNK) {
                            continue;
                        }

                        fs::path child = task.dir / name;
                        if (is_volumes_path(child)) continue;

                        if (dtype == DT_UNKNOWN) {
                            struct stat lst {};
                            if (::lstat(child.c_str(), &lst) != 0 || !S_ISDIR(lst.st_mode)) {
                                continue;
                            }
                        }

                        // If we're inside a repo-local agent dot-dir (e.g. <repo>/.claude) and see `skills`:
                        if (inside_agent_dot && name == "skills") {
                            fs::path repo_ref = effective_repo.empty() ? task.dir.parent_path() : effective_repo;
                            std::string repo_label = "repo:" + repo_ref.filename().string();
                            scan_skills_container(
                                child,
                                options.canonical_root,
                                repo_label,
                                /*is_canonical_store=*/false,
                                /*is_plugin_store=*/false,
                                /*is_repo_local=*/true,
                                repo_ref,
                                meta_cache,
                                out_mu,
                                result.occurrences,
                                result.broken_links);
                            continue;
                        }

                        // Skip other hidden directories unless they are agent dot-dirs
                        if (!name.empty() && name[0] == '.' && !is_agent_dot_dir(name)) {
                            continue;
                        }

                        // Allow descending 1 extra level into an agent dot-dir even at max_depth
                        bool allow_descend = (task.depth < options.max_depth) || is_agent_dot_dir(name);
                        if (allow_descend && mark_visited(child)) {
                            new_tasks.push_back(CrawlTask{
                                std::move(child),
                                is_agent_dot_dir(name) ? task.depth : task.depth + 1,
                                effective_repo});
                        }
                    }
                }

                {
                    std::lock_guard<std::mutex> lk(q_mu);
                    for (auto& nt : new_tasks) {
                        queue.push_back(std::move(nt));
                    }
                    --active_workers;
                    if (!new_tasks.empty() || (queue.empty() && active_workers == 0)) {
                        q_cv.notify_all();
                    }
                }
            }
        };

        std::vector<std::jthread> workers;
        workers.reserve(num_threads);
        for (size_t i = 0; i < num_threads; ++i) {
            workers.emplace_back(worker_fn);
        }
    }

    // Deterministic ordering of outputs
    std::sort(result.occurrences.begin(), result.occurrences.end(),
              [](const SkillOccurrence& a, const SkillOccurrence& b) {
                  if (a.slug != b.slug) return a.slug < b.slug;
                  if (a.target_id_or_repo != b.target_id_or_repo) return a.target_id_or_repo < b.target_id_or_repo;
                  return a.path.string() < b.path.string();
              });

    std::sort(result.broken_links.begin(), result.broken_links.end(),
              [](const SkillOccurrence& a, const SkillOccurrence& b) {
                  return a.path.string() < b.path.string();
              });

    std::sort(result.discovered_repos.begin(), result.discovered_repos.end());

    auto end_time = std::chrono::steady_clock::now();
    result.scan_duration_ms =
        std::chrono::duration<double, std::milli>(end_time - start_time).count();

    return result;
}

}  // namespace singlet::core
