#include "core/engine.hpp"

#include <dirent.h>
#include <limits.h>
#include <sys/stat.h>
#include <unistd.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <set>
#include <sstream>

#include "core/scanner.hpp"
#include "core/targets.hpp"
#include "util/diff.hpp"
#include "util/frontmatter.hpp"
#include "util/sha256.hpp"

namespace loadout::core {

namespace fs = std::filesystem;
using loadout::util::JsonValue;

namespace {

std::atomic<uint64_t> g_tmp_seq{0};

std::string iso_timestamp_now() {
    auto now = std::chrono::system_clock::now();
    std::time_t tt = std::chrono::system_clock::to_time_t(now);
    std::tm tm_utc{};
    ::gmtime_r(&tt, &tm_utc);
    std::ostringstream oss;
    oss << std::put_time(&tm_utc, "%Y-%m-%dT%H:%M:%SZ");
    return oss.str();
}

std::string compact_batch_id() {
    auto now = std::chrono::system_clock::now();
    std::time_t tt = std::chrono::system_clock::to_time_t(now);
    std::tm tm_utc{};
    ::gmtime_r(&tt, &tm_utc);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                  now.time_since_epoch())
                  .count() %
              1000;
    std::ostringstream oss;
    oss << std::put_time(&tm_utc, "%Y%m%dT%H%M%SZ") << "-" << std::setw(3) << std::setfill('0')
        << ms << "-" << ++g_tmp_seq;
    return oss.str();
}

std::string read_text_file(const fs::path& p) {
    std::ifstream in(p, std::ios::in | std::ios::binary);
    if (!in) return {};
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

bool write_text_file_atomic(const fs::path& p, std::string_view content) {
    std::error_code ec;
    fs::create_directories(p.parent_path(), ec);
    if (ec) return false;

    fs::path tmp = p.parent_path() /
                   (p.filename().string() + ".loadout-tmp-" + std::to_string(::getpid()) + "-" +
                    std::to_string(++g_tmp_seq));
    {
        std::ofstream out(tmp, std::ios::out | std::ios::binary | std::ios::trunc);
        if (!out) return false;
        out.write(content.data(), static_cast<std::streamsize>(content.size()));
        out.flush();
        if (!out) {
            fs::remove(tmp, ec);
            return false;
        }
    }
    if (::rename(tmp.c_str(), p.c_str()) != 0) {
        fs::remove(tmp, ec);
        return false;
    }
    return true;
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

bool lstat_exists(const fs::path& p, struct stat* out_st = nullptr) {
    struct stat st {};
    if (::lstat(p.c_str(), &st) != 0) return false;
    if (out_st) *out_st = st;
    return true;
}

bool move_directory_safe(const fs::path& src, const fs::path& dst, std::string& err) {
    std::error_code ec;
    fs::create_directories(dst.parent_path(), ec);
    if (ec) {
        err = "Failed to create parent directory: " + ec.message();
        return false;
    }

    fs::rename(src, dst, ec);
    if (!ec) {
        return true;
    }

    // Cross-device fallback: recursive copy without following symlinks, then remove source only on success
    ec.clear();
    fs::copy(src, dst, fs::copy_options::recursive | fs::copy_options::copy_symlinks, ec);
    if (ec) {
        err = "Failed to copy directory across filesystems: " + ec.message();
        std::error_code cleanup_ec;
        fs::remove_all(dst, cleanup_ec);
        return false;
    }
    fs::remove_all(src, ec);
    if (ec) {
        err = "Copied to destination but failed to clean up source: " + ec.message();
        return false;
    }
    return true;
}

bool copy_directory_safe(const fs::path& src, const fs::path& dst, std::string& err) {
    std::error_code ec;
    fs::create_directories(dst.parent_path(), ec);
    if (ec) {
        err = "Failed to create destination parent: " + ec.message();
        return false;
    }
    fs::copy(src, dst, fs::copy_options::recursive | fs::copy_options::copy_symlinks, ec);
    if (ec) {
        err = "Failed to copy skill directory: " + ec.message();
        return false;
    }
    return true;
}

fs::path find_skill_md_in_dir(const fs::path& dir) {
    struct stat st {};
    fs::path upper = dir / "SKILL.md";
    if (::stat(upper.c_str(), &st) == 0 && S_ISREG(st.st_mode)) return upper;
    fs::path lower = dir / "skill.md";
    if (::stat(lower.c_str(), &st) == 0 && S_ISREG(st.st_mode)) return lower;
    return {};
}

constexpr size_t kMaxPreviewBytes = 128 * 1024;
constexpr size_t kMaxWriteBytes = 1024 * 1024;
constexpr size_t kMaxProfileNameLength = 64;

std::vector<std::string> list_relative_files(const fs::path& dir) {
    std::vector<std::string> files;
    std::error_code ec;
    if (!fs::exists(dir, ec) || !fs::is_directory(dir, ec)) return files;
    for (auto it = fs::recursive_directory_iterator(
             dir, fs::directory_options::skip_permission_denied, ec);
         !ec && it != fs::recursive_directory_iterator();
         it.increment(ec)) {
        if (it->is_symlink(ec)) continue;
        if (!it->is_regular_file(ec)) continue;
        std::string fname = it->path().filename().string();
        if (fname == ".DS_Store" || fname == "Thumbs.db" || fname.starts_with(".loadout-tmp-")) continue;
        auto rel = fs::relative(it->path(), dir, ec);
        if (!ec) {
            files.push_back(rel.generic_string());
        }
    }
    std::sort(files.begin(), files.end());
    return files;
}

std::vector<SkillFileEntry> read_skill_file_entries(const fs::path& dir) {
    std::vector<SkillFileEntry> entries;
    std::error_code ec;
    if (!fs::exists(dir, ec) || !fs::is_directory(dir, ec)) return entries;

    for (auto it = fs::recursive_directory_iterator(
             dir, fs::directory_options::skip_permission_denied, ec);
         !ec && it != fs::recursive_directory_iterator();
         it.increment(ec)) {
        if (it->is_symlink(ec)) continue;
        if (!it->is_regular_file(ec)) continue;
        std::string fname = it->path().filename().string();
        if (fname == ".DS_Store" || fname == "Thumbs.db" || fname.starts_with(".loadout-tmp-")) continue;

        auto rel = fs::relative(it->path(), dir, ec);
        if (ec) continue;

        SkillFileEntry entry;
        entry.path = rel.generic_string();
        uint64_t fsize = it->file_size(ec);
        entry.size = ec ? 0 : fsize;

        if (!ec && entry.size <= kMaxPreviewBytes) {
            std::string raw = read_text_file(it->path());
            if (raw.find('\0') == std::string::npos) {
                entry.is_text = true;
                entry.content = std::move(raw);
            } else {
                entry.is_text = false;
            }
        } else {
            entry.is_text = false;
        }
        entries.push_back(std::move(entry));
    }

    std::sort(entries.begin(), entries.end(), [](const SkillFileEntry& a, const SkillFileEntry& b) {
        bool a_md = (a.path == "SKILL.md" || a.path == "skill.md");
        bool b_md = (b.path == "SKILL.md" || b.path == "skill.md");
        if (a_md != b_md) return a_md;
        return a.path < b.path;
    });
    return entries;
}

bool is_valid_slug(std::string_view slug) {
    if (slug.empty() || slug == "." || slug == "..") return false;
    for (char ch : slug) {
        if (ch == '/' || ch == '\\' || ch == '\0') return false;
    }
    return true;
}

bool is_safe_relative_skill_path(std::string_view rel_path) {
    if (rel_path.empty() || rel_path.front() == '/' || rel_path.front() == '\\') {
        return false;
    }
    for (char ch : rel_path) {
        if (ch == '\0' || ch == '\\') return false;
    }
    fs::path p(rel_path);
    if (p.is_absolute()) return false;
    bool has_component = false;
    for (const auto& part : p) {
        std::string s = part.string();
        if (s.empty() || s == "." || s == "..") return false;
        has_component = true;
    }
    return has_component;
}

std::string sanitize_filename_token(std::string_view raw) {
    std::string out;
    for (char ch : raw) {
        auto c = static_cast<unsigned char>(ch);
        if (std::isalnum(c) || c == '-' || c == '_') {
            out.push_back(static_cast<char>(c));
        } else {
            out.push_back('_');
        }
    }
    if (out.empty()) out = "item";
    return out;
}

std::string trim_whitespace(std::string_view sv) {
    size_t start = 0;
    while (start < sv.size() && std::isspace(static_cast<unsigned char>(sv[start]))) ++start;
    size_t end = sv.size();
    while (end > start && std::isspace(static_cast<unsigned char>(sv[end - 1]))) --end;
    return std::string(sv.substr(start, end - start));
}

fs::path resolve_git_exclude_file(const fs::path& worktree_path, fs::path& out_main_worktree) {
    fs::path dot_git = worktree_path / ".git";
    struct stat st {};
    if (::lstat(dot_git.c_str(), &st) != 0) {
        return {};
    }

    if (S_ISDIR(st.st_mode)) {
        out_main_worktree = worktree_path;
        return dot_git / "info" / "exclude";
    }

    if (S_ISREG(st.st_mode)) {
        std::string content = read_text_file(dot_git);
        constexpr std::string_view kPrefix = "gitdir:";
        auto pos = content.find(kPrefix);
        if (pos == std::string::npos) return {};
        std::string raw_gitdir = trim_whitespace(content.substr(pos + kPrefix.size()));
        fs::path gitdir(raw_gitdir);
        if (gitdir.is_relative()) {
            gitdir = (worktree_path / gitdir).lexically_normal();
        }
        gitdir = safe_weakly_canonical(gitdir);

        // Linked worktrees use <commondir>/info/exclude for git status!
        fs::path commondir_file = gitdir / "commondir";
        if (lstat_exists(commondir_file)) {
            std::string rel_common = trim_whitespace(read_text_file(commondir_file));
            if (!rel_common.empty()) {
                fs::path common_git = safe_weakly_canonical(gitdir / rel_common);
                if (common_git.filename() == ".git" && common_git.has_parent_path()) {
                    out_main_worktree = common_git.parent_path();
                }
                return common_git / "info" / "exclude";
            }
        }
        return gitdir / "info" / "exclude";
    }

    return {};
}

std::string adapter_worktree_skills_relpath(AdapterKind kind) {
    switch (kind) {
        case AdapterKind::ClaudeCode: return ".claude/skills";
        case AdapterKind::Codex: return ".agents/skills";
        case AdapterKind::OpenCode: return ".opencode/skills";
        case AdapterKind::Antigravity: return ".gemini/skills";
        case AdapterKind::Cursor: return ".cursor/skills";
        case AdapterKind::ZCode: return ".zcode/skills";
        case AdapterKind::Windsurf: return ".windsurf/skills";
        case AdapterKind::CustomLenient:
        case AdapterKind::CustomStrict: return ".agents/skills";
    }
    return ".agents/skills";
}

JsonValue to_frontmatter_json(const loadout::util::FrontmatterInfo& fm) {
    JsonValue obj = JsonValue::object();
    obj.set("hasFrontmatter", fm.has_frontmatter);
    obj.set("name", fm.name);
    obj.set("description", fm.description);
    JsonValue tags = JsonValue::array();
    for (const auto& t : fm.tags) tags.push_back(t);
    obj.set("tags", std::move(tags));
    obj.set("approxTokens", static_cast<int64_t>(fm.approx_tokens));
    obj.set("antigravityValid", fm.antigravity_valid);
    JsonValue issues = JsonValue::array();
    for (const auto& i : fm.issues) issues.push_back(i);
    obj.set("issues", std::move(issues));
    return obj;
}

JsonValue to_occurrence_json(const SkillOccurrence& occ) {
    JsonValue obj = JsonValue::object();
    obj.set("slug", occ.slug);
    obj.set("path", occ.path.string());
    obj.set("realPath", occ.real_path.string());
    obj.set("targetIdOrRepo", occ.target_id_or_repo);
    obj.set("entryKind", std::string(entry_kind_to_string(occ.entry_kind)));
    obj.set("isRepoLocal", occ.is_repo_local);
    if (!occ.repo_root.empty()) {
        obj.set("repoRoot", occ.repo_root.string());
    }
    obj.set("treeHash", occ.tree_hash);
    obj.set("fileCount", static_cast<int64_t>(occ.file_count));
    obj.set("totalBytes", static_cast<int64_t>(occ.total_bytes));
    obj.set("mtimeEpoch", occ.mtime_epoch);
    if (!occ.symlink_target.empty()) {
        obj.set("symlinkTarget", occ.symlink_target);
        obj.set("rawTarget", occ.symlink_target);
    }
    obj.set("frontmatter", to_frontmatter_json(occ.frontmatter));
    return obj;
}

}  // namespace

Engine::Engine(EngineConfig config)
    : config_(std::move(config)),
      home_dir_(resolve_home_dir(config_.home_dir)) {
    configured_scan_roots_ = config_.scan_roots;
    load_lockfile();
}

bool Engine::load_lockfile() {
    fs::path lf = lockfile_path();
    std::string content = read_text_file(lf);
    if (content.empty()) {
        return false;
    }

    auto parsed = loadout::util::parse_json(content);
    if (!parsed || !parsed->is_object()) {
        return false;
    }

    if (const auto* sr = parsed->find("scanRoots"); sr && sr->is_array() && config_.scan_roots.empty()) {
        configured_scan_roots_.clear();
        for (const auto& item : sr->as_array()) {
            if (item.is_string() && !item.as_string().empty()) {
                configured_scan_roots_.push_back(expand_home_path(item.as_string(), home_dir_));
            }
        }
    }

    if (const auto* targets_v = parsed->find("targets"); targets_v && targets_v->is_object()) {
        lockfile_targets_.clear();
        for (const auto& [id, val] : targets_v->as_object()) {
            if (!val.is_object()) continue;
            TargetInstance t;
            t.id = id;
            t.adapter = adapter_from_string(val.get_str("adapter", "custom-lenient"))
                            .value_or(AdapterKind::CustomLenient);
            t.skills_dir = expand_home_path(val.get_str("skillsDir"), home_dir_);
            t.display_name = val.get_str("displayName", id);
            t.group = val.get_str("group", std::string(adapter_default_group(t.adapter)));
            if (const auto* cr = val.find("configRoot"); cr && cr->is_string() && !cr->as_string().empty()) {
                t.config_root = expand_home_path(cr->as_string(), home_dir_);
            }
            if (const auto* eh = val.find("envHint"); eh && eh->is_string() && !eh->as_string().empty()) {
                t.env_hint = eh->as_string();
            }
            t.link_mode = link_mode_from_string(val.get_str("linkMode", "symlink-dir"))
                              .value_or(LinkMode::SymlinkDir);
            t.auto_discovered = val.get_bool("autoDiscovered", false);
            t.configured = true;
            if (!t.skills_dir.empty()) {
                lockfile_targets_.push_back(std::move(t));
            }
        }
    }

    if (const auto* rem_v = parsed->find("removedTargetIds"); rem_v && rem_v->is_array()) {
        removed_target_ids_.clear();
        for (const auto& item : rem_v->as_array()) {
            if (item.is_string()) removed_target_ids_.push_back(item.as_string());
        }
    }

    if (const auto* ap = parsed->find("activeProfile"); ap && ap->is_string()) {
        active_profile_ = ap->as_string();
    }

    if (const auto* prof_v = parsed->find("profiles"); prof_v && prof_v->is_object()) {
        profiles_.clear();
        for (const auto& [pname, val] : prof_v->as_object()) {
            Profile prof;
            prof.name = pname;
            if (val.is_array()) {
                for (const auto& s : val.as_array()) {
                    if (s.is_string()) prof.skills.push_back(s.as_string());
                }
            } else if (val.is_object()) {
                if (const auto* skills_v = val.find("skills"); skills_v && skills_v->is_array()) {
                    for (const auto& s : skills_v->as_array()) {
                        if (s.is_string()) prof.skills.push_back(s.as_string());
                    }
                }
                const JsonValue* wiring_obj = val.find("wiring");
                if (wiring_obj && wiring_obj->is_object()) {
                    for (const auto& [slug_k, tmap] : wiring_obj->as_object()) {
                        if (!tmap.is_object()) continue;
                        for (const auto& [tid_k, enabled_v] : tmap.as_object()) {
                            prof.wiring[slug_k][tid_k] = enabled_v.as_bool(false);
                        }
                    }
                } else if (!val.contains("skills") && !val.contains("wiring")) {
                    // Direct { slug: { targetId: bool } } map
                    for (const auto& [slug_k, tmap] : val.as_object()) {
                        if (!tmap.is_object()) continue;
                        for (const auto& [tid_k, enabled_v] : tmap.as_object()) {
                            prof.wiring[slug_k][tid_k] = enabled_v.as_bool(false);
                        }
                    }
                }
            }
            profiles_[pname] = std::move(prof);
        }
    }

    if (const auto* qh = parsed->find("quarantineHistory"); qh && qh->is_array()) {
        quarantine_history_.clear();
        for (const auto& item : qh->as_array()) {
            if (!item.is_object()) continue;
            QuarantineBatchRecord rec;
            rec.id = item.get_str("id");
            rec.timestamp = item.get_str("timestamp");
            rec.dir = expand_home_path(item.get_str("dir"), home_dir_);
            rec.entry_count = static_cast<size_t>(std::max<int64_t>(0, item.get_int("entryCount", 0)));
            if (!rec.id.empty() && lstat_exists(rec.dir / "manifest.json")) {
                quarantine_history_.push_back(std::move(rec));
            }
        }
    }

    return true;
}

bool Engine::save_lockfile() const {
    JsonValue root = JsonValue::object();
    root.set("version", 1);
    root.set("canonicalRoot", contract_home_path(canonical_skills_dir(), home_dir_));

    JsonValue sr = JsonValue::array();
    for (const auto& p : configured_scan_roots_) {
        sr.push_back(contract_home_path(p, home_dir_));
    }
    root.set("scanRoots", std::move(sr));

    if (!active_profile_.empty()) {
        root.set("activeProfile", active_profile_);
    }

    JsonValue profs = JsonValue::object();
    for (const auto& [pname, prof] : profiles_) {
        JsonValue pobj = JsonValue::object();
        JsonValue sarr = JsonValue::array();
        for (const auto& s : prof.skills) sarr.push_back(s);
        pobj.set("skills", std::move(sarr));

        JsonValue wobj = JsonValue::object();
        for (const auto& [slug_k, tmap] : prof.wiring) {
            JsonValue tobj = JsonValue::object();
            for (const auto& [tid_k, en] : tmap) {
                tobj.set(tid_k, en);
            }
            wobj.set(slug_k, std::move(tobj));
        }
        pobj.set("wiring", std::move(wobj));
        profs.set(pname, std::move(pobj));
    }
    root.set("profiles", std::move(profs));

    JsonValue targets_obj = JsonValue::object();
    for (const auto& t : lockfile_targets_) {
        JsonValue tobj = JsonValue::object();
        tobj.set("adapter", std::string(adapter_to_string(t.adapter)));
        tobj.set("skillsDir", contract_home_path(t.skills_dir, home_dir_));
        tobj.set("displayName", t.display_name);
        tobj.set("group", t.group);
        if (t.config_root) {
            tobj.set("configRoot", contract_home_path(*t.config_root, home_dir_));
        }
        if (t.env_hint) {
            tobj.set("envHint", *t.env_hint);
        }
        tobj.set("linkMode", std::string(link_mode_to_string(t.link_mode)));
        tobj.set("autoDiscovered", t.auto_discovered);
        targets_obj.set(t.id, std::move(tobj));
    }
    root.set("targets", std::move(targets_obj));

    if (!removed_target_ids_.empty()) {
        JsonValue rem = JsonValue::array();
        for (const auto& id : removed_target_ids_) rem.push_back(id);
        root.set("removedTargetIds", std::move(rem));
    }

    JsonValue skills_obj = JsonValue::object();
    if (cached_snapshot_) {
        for (const auto& sk : cached_snapshot_->skills) {
            JsonValue sobj = JsonValue::object();
            sobj.set("treeHash", sk.tree_hash.empty() ? "" : ("sha256:" + sk.tree_hash));
            sobj.set("approxTokens", static_cast<int64_t>(sk.approx_tokens));
            JsonValue tags = JsonValue::array();
            for (const auto& tg : sk.tags) tags.push_back(tg);
            sobj.set("tags", std::move(tags));
            JsonValue enabled_in = JsonValue::object();
            for (const auto& [tid, st] : sk.target_states) {
                enabled_in.set(tid, st.enabled);
            }
            sobj.set("enabledIn", std::move(enabled_in));
            skills_obj.set(sk.slug, std::move(sobj));
        }
    }
    root.set("skills", std::move(skills_obj));

    JsonValue qh = JsonValue::array();
    for (const auto& rec : quarantine_history_) {
        JsonValue qobj = JsonValue::object();
        qobj.set("id", rec.id);
        qobj.set("timestamp", rec.timestamp);
        qobj.set("dir", contract_home_path(rec.dir, home_dir_));
        qobj.set("entryCount", static_cast<int64_t>(rec.entry_count));
        qh.push_back(std::move(qobj));
    }
    root.set("quarantineHistory", std::move(qh));

    return write_text_file_atomic(lockfile_path(), loadout::util::to_json(root, true) + "\n");
}

CatalogSnapshot Engine::get_catalog() {
    if (cached_snapshot_) {
        return *cached_snapshot_;
    }
    return scan();
}

CatalogSnapshot Engine::scan(const std::vector<fs::path>& extra_roots) {
    // Re-sync quarantine history with disk in case another process created/undid a batch
    quarantine_history_.erase(
        std::remove_if(quarantine_history_.begin(), quarantine_history_.end(),
                       [](const QuarantineBatchRecord& r) {
                           return !lstat_exists(r.dir / "manifest.json");
                       }),
        quarantine_history_.end());

    // Also discover any quarantine batches on disk not yet in memory
    if (DIR* dp = ::opendir(quarantine_root_dir().c_str())) {
        std::set<std::string> known_ids;
        for (const auto& r : quarantine_history_) known_ids.insert(r.id);
        std::vector<QuarantineBatchRecord> disk_batches;
        while (struct dirent* ent = ::readdir(dp)) {
            std::string name(ent->d_name);
            if (name == "." || name == "..") continue;
            if (known_ids.contains(name)) continue;
            fs::path bdir = quarantine_root_dir() / name;
            if (lstat_exists(bdir / "manifest.json")) {
                QuarantineBatchRecord rec;
                rec.id = name;
                rec.dir = bdir;
                rec.timestamp = name;
                disk_batches.push_back(std::move(rec));
            }
        }
        ::closedir(dp);
        std::sort(disk_batches.begin(), disk_batches.end(),
                  [](const QuarantineBatchRecord& a, const QuarantineBatchRecord& b) {
                      return a.id < b.id;
                  });
        for (auto& b : disk_batches) quarantine_history_.push_back(std::move(b));
    }

    auto discovered = discover_targets(home_dir_);

    // Filter out explicitly removed auto-discovered target IDs
    std::set<std::string> removed_set(removed_target_ids_.begin(), removed_target_ids_.end());
    std::vector<TargetInstance> filtered_discovered;
    for (const auto& d : discovered.targets) {
        if (!removed_set.contains(d.id)) {
            filtered_discovered.push_back(d);
        }
    }

    CatalogSnapshot snap;
    snap.canonical_root = canonical_skills_dir();
    snap.active_profile = active_profile_;
    snap.profiles = get_profiles();
    merge_targets(lockfile_targets_, filtered_discovered, snap.targets, snap.unconfigured_siblings);

    // Determine repo scan roots
    std::vector<fs::path> scan_roots;
    if (!extra_roots.empty()) {
        for (const auto& r : extra_roots) {
            scan_roots.push_back(expand_home_path(r.string(), home_dir_));
        }
    } else if (!configured_scan_roots_.empty()) {
        scan_roots = configured_scan_roots_;
    } else {
        for (const char* def_dir : {"dev", "code", "Projects", "projects", "workspace", "src"}) {
            fs::path candidate = home_dir_ / def_dir;
            struct stat st {};
            if (::stat(candidate.c_str(), &st) == 0 && S_ISDIR(st.st_mode)) {
                scan_roots.push_back(candidate);
            }
        }
    }
    snap.scan_roots = scan_roots;

    ScanOptions opts;
    opts.home_dir = home_dir_;
    opts.canonical_root = canonical_skills_dir();
    opts.targets = snap.targets;
    opts.plugin_skill_roots = discovered.plugin_skill_roots;
    opts.extra_skill_roots = discovered.opencode_extra_roots;
    opts.repo_scan_roots = scan_roots;
    opts.max_depth = config_.max_scan_depth;
    opts.worker_threads = config_.worker_threads;

    ScanResult raw = scan_filesystem(opts);
    snap.last_scan_ms = raw.scan_duration_ms;
    snap.broken_links = raw.broken_links;
    snap.can_undo = !quarantine_history_.empty();
    if (!quarantine_history_.empty()) {
        snap.last_quarantine_batch = quarantine_history_.back().id;
    }

    // Group occurrences by slug
    std::map<std::string, std::vector<SkillOccurrence>> by_slug;
    for (auto& occ : raw.occurrences) {
        by_slug[occ.slug].push_back(std::move(occ));
    }

    // Build CanonicalSkill list + DuplicateGroup + DriftedGroup
    std::set<std::string> unique_repos;
    for (const auto& r : raw.discovered_repos) unique_repos.insert(r.string());

    for (auto& [slug, occs] : by_slug) {
        CanonicalSkill sk;
        sk.slug = slug;
        sk.canonical_path = canonical_skills_dir() / slug;

        // Pick primary occurrence: Canonical > global PhysicalCopy > ManagedSymlink > ExternalSymlink > repo > PluginShadow
        const SkillOccurrence* primary = nullptr;
        auto occ_priority = [](const SkillOccurrence& o) -> int {
            if (o.entry_kind == EntryKind::Canonical) return 0;
            if (o.entry_kind == EntryKind::PhysicalCopy && !o.is_repo_local) return 1;
            if (o.entry_kind == EntryKind::ManagedSymlink) return 2;
            if (o.entry_kind == EntryKind::ExternalSymlink) return 3;
            if (o.is_repo_local) return 4;
            if (o.entry_kind == EntryKind::PluginShadow) return 5;
            return 6;
        };

        bool all_repo_local = true;
        for (const auto& o : occs) {
            if (o.entry_kind == EntryKind::Canonical) {
                sk.is_canonical = true;
            }
            if (o.entry_kind == EntryKind::PhysicalCopy && !o.is_repo_local) {
                sk.has_physical_copy = true;
            }
            if (o.entry_kind == EntryKind::PluginShadow) {
                sk.has_plugin_shadow = true;
            }
            if (!o.is_repo_local) {
                all_repo_local = false;
            } else if (!o.repo_root.empty()) {
                unique_repos.insert(o.repo_root.string());
                if (sk.repo_root.empty()) sk.repo_root = o.repo_root.string();
            }
            if (!primary || occ_priority(o) < occ_priority(*primary)) {
                primary = &o;
            }
        }
        sk.is_repo_local = all_repo_local;

        if (primary) {
            sk.name = primary->frontmatter.name.empty() ? slug : primary->frontmatter.name;
            sk.description = primary->frontmatter.description;
            sk.tags = primary->frontmatter.tags;
            sk.tree_hash = primary->tree_hash;
            sk.file_count = primary->file_count;
            sk.total_bytes = primary->total_bytes;
            sk.approx_tokens = primary->frontmatter.approx_tokens;
            sk.antigravity_valid = primary->frontmatter.antigravity_valid;
            sk.frontmatter_issues = primary->frontmatter.issues;

            struct stat st {};
            sk.has_scripts = (::stat((primary->real_path / "scripts").c_str(), &st) == 0 && S_ISDIR(st.st_mode));
            sk.has_references = (::stat((primary->real_path / "references").c_str(), &st) == 0 && S_ISDIR(st.st_mode));
        }

        // Populate per-target states
        for (const auto& target : snap.targets) {
            TargetSkillState tst;
            tst.incompatible = adapter_requires_strict_frontmatter(target.adapter) && !sk.antigravity_valid;

            for (const auto& o : occs) {
                if (!o.is_repo_local && o.target_id_or_repo == target.id) {
                    tst.has_entry = true;
                    tst.entry_kind = o.entry_kind;
                    tst.path = o.path.string();
                    tst.enabled = (o.entry_kind == EntryKind::ManagedSymlink ||
                                   o.entry_kind == EntryKind::ExternalSymlink);
                    break;
                }
            }
            if (!tst.has_entry) {
                for (const auto& b : snap.broken_links) {
                    if (b.slug == slug && b.target_id_or_repo == target.id) {
                        tst.has_entry = true;
                        tst.entry_kind = EntryKind::DanglingSymlink;
                        tst.path = b.path.string();
                        tst.enabled = false;
                        break;
                    }
                }
            }
            sk.target_states[target.id] = std::move(tst);
        }

        // 3-Tier Duplicate & Drift Detection across physical directories (Canonical + PhysicalCopy)
        std::vector<SkillOccurrence> physicals;
        std::set<std::string> seen_phys_paths;
        for (const auto& o : occs) {
            if (o.entry_kind == EntryKind::Canonical || o.entry_kind == EntryKind::PhysicalCopy) {
                std::string rp = safe_weakly_canonical(o.path).string();
                if (seen_phys_paths.insert(rp).second) {
                    physicals.push_back(o);
                }
            }
        }

        if (physicals.size() >= 2) {
            std::map<std::string, std::vector<SkillOccurrence>> by_hash;
            for (const auto& p : physicals) {
                if (!p.tree_hash.empty()) {
                    by_hash[p.tree_hash].push_back(p);
                }
            }

            // Exact duplicate groups (same tree_hash across >= 2 physical folders)
            for (auto& [hash, group_occs] : by_hash) {
                if (group_occs.size() >= 2) {
                    DuplicateGroup dg;
                    dg.slug = slug;
                    dg.tree_hash = hash;
                    dg.redundant_bytes = group_occs[0].total_bytes * (group_occs.size() - 1);
                    dg.occurrences = group_occs;
                    snap.duplicate_groups.push_back(std::move(dg));
                    sk.has_duplicates = true;
                }
            }

            // Drifted groups (>= 2 distinct tree_hashes for the same slug)
            if (by_hash.size() >= 2) {
                DriftedGroup drift;
                drift.slug = slug;

                std::vector<SkillOccurrence> reps;
                for (const auto& [hash, group_occs] : by_hash) {
                    reps.push_back(group_occs.front());
                }
                std::sort(reps.begin(), reps.end(),
                          [](const SkillOccurrence& a, const SkillOccurrence& b) {
                              bool a_can = (a.entry_kind == EntryKind::Canonical);
                              bool b_can = (b.entry_kind == EntryKind::Canonical);
                              if (a_can != b_can) return a_can;
                              if (a.mtime_epoch != b.mtime_epoch) return a.mtime_epoch > b.mtime_epoch;
                              return a.path.string() < b.path.string();
                          });

                std::string primary_md = read_text_file(find_skill_md_in_dir(reps[0].real_path));
                std::string primary_label = contract_home_path(reps[0].path, home_dir_);
                auto primary_files = list_relative_files(reps[0].real_path);

                for (size_t i = 0; i < reps.size(); ++i) {
                    DriftedVariant var;
                    var.occurrence = reps[i];
                    var.files = list_relative_files(reps[i].real_path);
                    std::string md = (i == 0) ? primary_md : read_text_file(find_skill_md_in_dir(reps[i].real_path));
                    var.skill_md_preview = md.size() > 2048 ? md.substr(0, 2048) : md;
                    if (i > 0) {
                        std::string var_label = contract_home_path(reps[i].path, home_dir_);
                        var.unified_diff_from_primary =
                            loadout::util::unified_diff(primary_md, md, primary_label, var_label);
                        if (var.unified_diff_from_primary.empty()) {
                            // SKILL.md identical, auxiliary files differ
                            std::ostringstream oss;
                            oss << "--- " << primary_label << " (files)\n";
                            oss << "+++ " << var_label << " (files)\n";
                            std::set<std::string> pf(primary_files.begin(), primary_files.end());
                            std::set<std::string> vf(var.files.begin(), var.files.end());
                            for (const auto& f : pf) {
                                if (!vf.contains(f)) oss << "- " << f << "\n";
                            }
                            for (const auto& f : vf) {
                                if (!pf.contains(f)) oss << "+ " << f << "\n";
                            }
                            if (pf == vf) {
                                oss << "@@ auxiliary file contents differ @@\n";
                            }
                            var.unified_diff_from_primary = oss.str();
                        }
                    }
                    drift.variants.push_back(std::move(var));
                }

                if (drift.variants.size() >= 2) {
                    drift.unified_diff = drift.variants[1].unified_diff_from_primary;
                }
                snap.drifted_groups.push_back(std::move(drift));
                sk.has_drift = true;
            }
        }

        sk.occurrences = std::move(occs);

        // Update stats
        if (sk.is_canonical) ++snap.stats.canonical_skills;
        if (!sk.antigravity_valid) ++snap.stats.frontmatter_issues;
        if (sk.has_physical_copy) ++snap.stats.unmanaged_copies;
        if (sk.is_repo_local) ++snap.stats.project_local_skills;

        snap.skills.push_back(std::move(sk));
    }

    snap.stats.total_skills = snap.skills.size();
    snap.stats.total_targets = snap.targets.size();
    snap.stats.total_repos = unique_repos.size();
    snap.stats.exact_duplicates = snap.duplicate_groups.size();
    for (const auto& dg : snap.duplicate_groups) {
        snap.stats.duplicate_bytes += dg.redundant_bytes;
    }
    snap.stats.drifted_groups = snap.drifted_groups.size();
    snap.stats.broken_links = snap.broken_links.size();

    cached_snapshot_ = snap;
    return snap;
}

std::optional<TargetInstance> Engine::find_target(const std::string& target_id) const {
    if (cached_snapshot_) {
        for (const auto& t : cached_snapshot_->targets) {
            if (t.id == target_id) return t;
        }
        for (const auto& t : cached_snapshot_->unconfigured_siblings) {
            if (t.id == target_id) return t;
        }
    }
    for (const auto& t : lockfile_targets_) {
        if (t.id == target_id) return t;
    }
    auto disc = discover_targets(home_dir_);
    for (const auto& t : disc.targets) {
        if (t.id == target_id) return t;
    }
    return std::nullopt;
}

std::optional<fs::path> Engine::find_skill_dir(const std::string& slug) const {
    fs::path can = canonical_skills_dir() / slug;
    if (!find_skill_md_in_dir(can).empty()) {
        return can;
    }
    if (cached_snapshot_) {
        for (const auto& sk : cached_snapshot_->skills) {
            if (sk.slug == slug) {
                for (const auto& o : sk.occurrences) {
                    if (o.entry_kind == EntryKind::Canonical ||
                        o.entry_kind == EntryKind::PhysicalCopy ||
                        o.entry_kind == EntryKind::ManagedSymlink ||
                        o.entry_kind == EntryKind::ExternalSymlink) {
                        if (!find_skill_md_in_dir(o.real_path).empty()) {
                            return o.real_path;
                        }
                    }
                }
            }
        }
    }
    return std::nullopt;
}

bool Engine::is_managed_symlink(const fs::path& link_path) const {
    struct stat lst {};
    if (::lstat(link_path.c_str(), &lst) != 0 || !S_ISLNK(lst.st_mode)) {
        return false;
    }
    std::string buf(PATH_MAX, '\0');
    ssize_t len = ::readlink(link_path.c_str(), buf.data(), buf.size() - 1);
    if (len < 0) return false;
    buf.resize(static_cast<size_t>(len));

    fs::path target_p(buf);
    if (target_p.is_relative()) {
        target_p = (link_path.parent_path() / target_p).lexically_normal();
    }
    return is_inside_dir(target_p, canonical_skills_dir());
}

OperationResult Engine::write_atomic_symlink(
    const fs::path& link_path,
    const fs::path& target_path) const {
    std::error_code ec;
    fs::create_directories(link_path.parent_path(), ec);
    if (ec) {
        return {false, "Failed to create target directory '" + link_path.parent_path().string() + "': " + ec.message(), ""};
    }

    // Refuse to overwrite an existing real physical directory!
    struct stat lst {};
    if (::lstat(link_path.c_str(), &lst) == 0 && !S_ISLNK(lst.st_mode)) {
        return {false,
                "Refusing to overwrite unmanaged physical path at " + link_path.string() +
                    " (run consolidate first)",
                ""};
    }

    std::string tmp_name = ".loadout-tmp-" + std::to_string(::getpid()) + "-" + std::to_string(++g_tmp_seq);
    fs::path tmp_path = link_path.parent_path() / tmp_name;

    if (::symlink(target_path.c_str(), tmp_path.c_str()) != 0) {
        return {false, "symlink() failed for " + tmp_path.string(), ""};
    }

    if (::rename(tmp_path.c_str(), link_path.c_str()) != 0) {
        ::unlink(tmp_path.c_str());
        return {false, "Atomic rename() failed for " + link_path.string(), ""};
    }
    return {true, "", "Linked " + link_path.string() + " -> " + target_path.string()};
}

OperationResult Engine::remove_managed_symlink(const fs::path& link_path) const {
    struct stat lst {};
    if (::lstat(link_path.c_str(), &lst) != 0) {
        // Already absent -> idempotent success
        return {true, "", "Already unwired"};
    }
    if (!S_ISLNK(lst.st_mode)) {
        return {false,
                "Refusing to delete unmanaged physical directory at " + link_path.string() +
                    " (run consolidate first)",
                ""};
    }
    if (!is_managed_symlink(link_path)) {
        return {false,
                "Refusing to unlink external symlink not pointing inside " +
                    canonical_skills_dir().string(),
                ""};
    }
    if (::unlink(link_path.c_str()) != 0) {
        return {false, "unlink() failed for " + link_path.string(), ""};
    }
    return {true, "", "Unwired " + link_path.string()};
}

void Engine::sync_antigravity_skills_json(const TargetInstance& target) const {
    if (target.link_mode != LinkMode::AntigravitySkillsJson || !target.config_root) {
        return;
    }
    fs::path skills_json = *target.config_root / "config" / "skills.json";
    if (!lstat_exists(*target.config_root / "config")) {
        skills_json = *target.config_root / "skills.json";
    }

    JsonValue arr = JsonValue::array();
    if (DIR* dp = ::opendir(target.skills_dir.c_str())) {
        std::vector<std::string> slugs;
        while (struct dirent* ent = ::readdir(dp)) {
            std::string name(ent->d_name);
            if (name == "." || name == ".." || name.starts_with(".loadout-tmp-")) continue;
            fs::path p = target.skills_dir / name;
            if (!find_skill_md_in_dir(p).empty()) {
                slugs.push_back(name);
            }
        }
        ::closedir(dp);
        std::sort(slugs.begin(), slugs.end());
        for (const auto& s : slugs) {
            JsonValue entry = JsonValue::object();
            entry.set("name", s);
            entry.set("path", (target.skills_dir / s).string());
            arr.push_back(std::move(entry));
        }
    }

    JsonValue root = JsonValue::object();
    root.set("skills", std::move(arr));
    write_text_file_atomic(skills_json, loadout::util::to_json(root, true) + "\n");
}

OperationResult Engine::toggle_skill(
    const std::string& slug,
    const std::string& target_id,
    bool enabled) {
    if (slug.empty() || target_id.empty()) {
        return {false, "slug and target_id are required", ""};
    }
    if (!cached_snapshot_) {
        scan();
    }

    auto target_opt = find_target(target_id);
    if (!target_opt) {
        return {false, "Unknown target: " + target_id, ""};
    }
    TargetInstance target = *target_opt;

    // If this target was an unconfigured sibling, persist it as configured
    if (!target.configured) {
        target.configured = true;
        bool exists = false;
        for (auto& lt : lockfile_targets_) {
            if (lt.id == target.id) {
                lt = target;
                exists = true;
                break;
            }
        }
        if (!exists) lockfile_targets_.push_back(target);
    }

    fs::path link_path = target.skills_dir / slug;

    if (enabled) {
        fs::path can_dir = canonical_skills_dir() / slug;
        if (find_skill_md_in_dir(can_dir).empty()) {
            // Non-destructively seed canonical store if skill exists physically elsewhere
            auto existing_dir = find_skill_dir(slug);
            if (!existing_dir) {
                return {false, "Skill not found: " + slug, ""};
            }
            std::string err;
            if (!copy_directory_safe(*existing_dir, can_dir, err)) {
                return {false, err, ""};
            }
        }
        auto res = write_atomic_symlink(link_path, can_dir);
        if (!res.ok) return res;
        sync_antigravity_skills_json(target);
        cached_snapshot_.reset();
        scan();
        save_lockfile();
        return res;
    } else {
        auto res = remove_managed_symlink(link_path);
        if (!res.ok) return res;
        sync_antigravity_skills_json(target);
        cached_snapshot_.reset();
        scan();
        save_lockfile();
        return res;
    }
}

ConsolidateResult Engine::consolidate(
    bool dry_run,
    const std::vector<std::string>& promote_repo_paths) {
    ConsolidateResult result;
    result.plan.dry_run = dry_run;

    CatalogSnapshot snap = scan();

    std::set<std::string> promote_set;
    for (const auto& raw : promote_repo_paths) {
        fs::path p = expand_home_path(raw, home_dir_);
        promote_set.insert(p.lexically_normal().string());
        promote_set.insert(safe_weakly_canonical(p).string());
    }

    struct PendingOp {
        std::string slug;
        std::string kind;  // "promote", "quarantine_and_link"
        fs::path source;
        fs::path dest_canonical;
        fs::path quarantine_dst;
        std::string target_id;
        bool is_repo_local = false;
        uint64_t bytes = 0;
    };

    std::vector<PendingOp> ops;
    std::string batch_id = compact_batch_id();
    fs::path batch_dir = quarantine_root_dir() / batch_id;
    size_t q_counter = 0;

    for (const auto& sk : snap.skills) {
        fs::path can_dir = canonical_skills_dir() / sk.slug;

        std::vector<SkillOccurrence> eligible;
        std::optional<std::string> canonical_hash;
        if (sk.is_canonical) {
            canonical_hash = sk.tree_hash;
        }

        for (const auto& occ : sk.occurrences) {
            if (occ.entry_kind != EntryKind::PhysicalCopy) continue;
            if (occ.is_repo_local) {
                std::string lex = occ.path.lexically_normal().string();
                std::string canon = safe_weakly_canonical(occ.path).string();
                if (!promote_set.contains(lex) && !promote_set.contains(canon)) {
                    ConsolidateAction act;
                    act.slug = sk.slug;
                    act.kind = "keep_repo_local";
                    act.source_path = occ.path;
                    act.dest_path = occ.path;
                    act.is_repo_local = true;
                    act.target_id = occ.target_id_or_repo;
                    act.reason = "Project-local skill kept in repository";
                    result.plan.actions.push_back(std::move(act));
                    continue;
                }
            }
            eligible.push_back(occ);
        }

        if (eligible.empty()) continue;

        // Check if eligible physical copies diverge in content (drift)
        std::set<std::string> hashes;
        if (canonical_hash && !canonical_hash->empty()) {
            hashes.insert(*canonical_hash);
        }
        for (const auto& e : eligible) {
            if (!e.tree_hash.empty()) hashes.insert(e.tree_hash);
        }

        // If canonical doesn't exist yet and multiple distinct hashes exist, skip automatic overwrite
        if (!sk.is_canonical && hashes.size() > 1) {
            continue;
        }

        // Sort eligible so newest global copy is promoted first
        std::sort(eligible.begin(), eligible.end(),
                  [](const SkillOccurrence& a, const SkillOccurrence& b) {
                      if (a.is_repo_local != b.is_repo_local) return !a.is_repo_local;
                      if (a.mtime_epoch != b.mtime_epoch) return a.mtime_epoch > b.mtime_epoch;
                      return a.path.string() < b.path.string();
                  });

        bool has_canonical = sk.is_canonical;
        std::string target_hash = canonical_hash.value_or(eligible.front().tree_hash);

        for (const auto& e : eligible) {
            // Skip drifted variants when canonical already exists with a different hash
            if (!target_hash.empty() && !e.tree_hash.empty() && e.tree_hash != target_hash) {
                continue;
            }

            if (!has_canonical) {
                PendingOp op;
                op.slug = sk.slug;
                op.kind = "promote";
                op.source = e.path;
                op.dest_canonical = can_dir;
                op.target_id = e.target_id_or_repo;
                op.is_repo_local = e.is_repo_local;
                op.bytes = e.total_bytes;

                ConsolidateAction act;
                act.slug = sk.slug;
                act.kind = "promote";
                act.source_path = e.path;
                act.dest_path = can_dir;
                act.is_repo_local = e.is_repo_local;
                act.target_id = e.target_id_or_repo;
                act.reason = "Promote physical copy to canonical store and symlink back";
                result.plan.actions.push_back(std::move(act));

                ops.push_back(std::move(op));
                has_canonical = true;
                ++result.plan.skills_promoted;
                ++result.plan.symlinks_created;
            } else {
                std::string qname = sanitize_filename_token(sk.slug) + "__" +
                                    sanitize_filename_token(e.target_id_or_repo) + "_" +
                                    std::to_string(++q_counter);
                fs::path qdst = batch_dir / qname;

                PendingOp op;
                op.slug = sk.slug;
                op.kind = "quarantine_and_link";
                op.source = e.path;
                op.dest_canonical = can_dir;
                op.quarantine_dst = qdst;
                op.target_id = e.target_id_or_repo;
                op.is_repo_local = e.is_repo_local;
                op.bytes = e.total_bytes;

                ConsolidateAction act;
                act.slug = sk.slug;
                act.kind = "quarantine_and_link";
                act.source_path = e.path;
                act.dest_path = can_dir;
                act.quarantine_path = qdst;
                act.is_repo_local = e.is_repo_local;
                act.target_id = e.target_id_or_repo;
                act.reason = "Move redundant copy to quarantine and symlink to canonical";
                result.plan.actions.push_back(std::move(act));

                ops.push_back(std::move(op));
                ++result.plan.copies_quarantined;
                ++result.plan.symlinks_created;
                result.plan.bytes_deduplicated += e.total_bytes;
            }
        }
    }

    if (dry_run || ops.empty()) {
        return result;
    }

    std::error_code ec;
    fs::create_directories(canonical_skills_dir(), ec);
    fs::create_directories(batch_dir, ec);
    if (ec) {
        result.ok = false;
        result.error = "Failed to create quarantine batch directory: " + ec.message();
        return result;
    }

    JsonValue manifest_entries = JsonValue::array();
    std::string ts = iso_timestamp_now();

    for (const auto& op : ops) {
        std::string err;
        if (op.kind == "promote") {
            if (!move_directory_safe(op.source, op.dest_canonical, err)) {
                result.ok = false;
                result.error = err;
                break;
            }
            auto lres = write_atomic_symlink(op.source, op.dest_canonical);
            if (!lres.ok) {
                result.ok = false;
                result.error = lres.error;
                break;
            }
            JsonValue entry = JsonValue::object();
            entry.set("action", "promoted");
            entry.set("slug", op.slug);
            entry.set("originalPath", op.source.string());
            entry.set("canonicalPath", op.dest_canonical.string());
            entry.set("symlinkCreated", true);
            manifest_entries.push_back(std::move(entry));
        } else if (op.kind == "quarantine_and_link") {
            if (!move_directory_safe(op.source, op.quarantine_dst, err)) {
                result.ok = false;
                result.error = err;
                break;
            }
            auto lres = write_atomic_symlink(op.source, op.dest_canonical);
            if (!lres.ok) {
                result.ok = false;
                result.error = lres.error;
                break;
            }
            JsonValue entry = JsonValue::object();
            entry.set("action", "quarantined");
            entry.set("slug", op.slug);
            entry.set("originalPath", op.source.string());
            entry.set("quarantinePath", op.quarantine_dst.string());
            entry.set("canonicalPath", op.dest_canonical.string());
            entry.set("symlinkCreated", true);
            manifest_entries.push_back(std::move(entry));
        }
    }

    JsonValue manifest = JsonValue::object();
    manifest.set("batchId", batch_id);
    manifest.set("timestamp", ts);
    manifest.set("entries", manifest_entries);

    write_text_file_atomic(batch_dir / "manifest.json", loadout::util::to_json(manifest, true) + "\n");

    QuarantineBatchRecord rec;
    rec.id = batch_id;
    rec.timestamp = ts;
    rec.dir = batch_dir;
    rec.entry_count = manifest_entries.size();
    quarantine_history_.push_back(std::move(rec));

    result.quarantine_batch_id = batch_id;
    result.quarantine_dir = batch_dir;

    cached_snapshot_.reset();
    scan();
    save_lockfile();
    return result;
}

OperationResult Engine::resolve_drift(
    const std::string& slug,
    const std::string& chosen_path_raw,
    const std::string& keep_both_slug) {
    if (slug.empty() || chosen_path_raw.empty()) {
        return {false, "slug and chosen_path are required", ""};
    }
    if (!keep_both_slug.empty() && !is_valid_slug(keep_both_slug)) {
        return {false, "Invalid keep-both slug: " + keep_both_slug, ""};
    }

    CatalogSnapshot snap = scan();
    fs::path chosen_expanded = expand_home_path(chosen_path_raw, home_dir_);
    fs::path chosen_canon = safe_weakly_canonical(chosen_expanded);

    const CanonicalSkill* target_skill = nullptr;
    for (const auto& sk : snap.skills) {
        if (sk.slug == slug) {
            target_skill = &sk;
            break;
        }
    }
    if (!target_skill) {
        return {false, "Skill not found: " + slug, ""};
    }

    const SkillOccurrence* chosen_occ = nullptr;
    for (const auto& o : target_skill->occurrences) {
        if (safe_weakly_canonical(o.path) == chosen_canon ||
            o.path.lexically_normal() == chosen_expanded.lexically_normal()) {
            chosen_occ = &o;
            break;
        }
    }
    if (!chosen_occ) {
        return {false, "Chosen occurrence path not found for skill '" + slug + "': " + chosen_path_raw, ""};
    }

    std::string batch_id = compact_batch_id();
    fs::path batch_dir = quarantine_root_dir() / batch_id;
    std::error_code ec;
    fs::create_directories(canonical_skills_dir(), ec);
    fs::create_directories(batch_dir, ec);

    JsonValue manifest_entries = JsonValue::array();
    std::string ts = iso_timestamp_now();

    if (!keep_both_slug.empty()) {
        fs::path new_can = canonical_skills_dir() / keep_both_slug;
        if (lstat_exists(new_can)) {
            return {false, "Target slug already exists in canonical store: " + keep_both_slug, ""};
        }
        std::string err;
        if (!move_directory_safe(chosen_occ->path, new_can, err)) {
            return {false, err, ""};
        }
        fs::path new_link = chosen_occ->path.parent_path() / keep_both_slug;
        auto lres = write_atomic_symlink(new_link, new_can);
        if (!lres.ok) return lres;

        JsonValue entry = JsonValue::object();
        entry.set("action", "renamed_keep_both");
        entry.set("slug", keep_both_slug);
        entry.set("originalPath", chosen_occ->path.string());
        entry.set("newLinkPath", new_link.string());
        entry.set("canonicalPath", new_can.string());
        manifest_entries.push_back(std::move(entry));
    } else {
        fs::path can_dir = canonical_skills_dir() / slug;
        size_t q_counter = 0;

        // If chosen is not already canonical, quarantine existing canonical (if any) and promote chosen
        if (safe_weakly_canonical(chosen_occ->path) != safe_weakly_canonical(can_dir)) {
            if (lstat_exists(can_dir)) {
                fs::path q_old = batch_dir / (sanitize_filename_token(slug) + "__canonical_" + std::to_string(++q_counter));
                std::string err;
                if (!move_directory_safe(can_dir, q_old, err)) {
                    return {false, err, ""};
                }
                JsonValue entry = JsonValue::object();
                entry.set("action", "quarantined_canonical");
                entry.set("slug", slug);
                entry.set("originalPath", can_dir.string());
                entry.set("quarantinePath", q_old.string());
                manifest_entries.push_back(std::move(entry));
            }
            std::string err;
            if (!move_directory_safe(chosen_occ->path, can_dir, err)) {
                return {false, err, ""};
            }
            auto lres = write_atomic_symlink(chosen_occ->path, can_dir);
            if (!lres.ok) return lres;

            JsonValue entry = JsonValue::object();
            entry.set("action", "promoted");
            entry.set("slug", slug);
            entry.set("originalPath", chosen_occ->path.string());
            entry.set("canonicalPath", can_dir.string());
            entry.set("symlinkCreated", true);
            manifest_entries.push_back(std::move(entry));
        }

        // Quarantine all other physical copies of this slug and wire symlinks to canonical
        for (const auto& occ : target_skill->occurrences) {
            if (occ.entry_kind != EntryKind::PhysicalCopy) continue;
            if (safe_weakly_canonical(occ.path) == chosen_canon) continue;
            if (occ.is_repo_local) continue;

            fs::path qdst = batch_dir / (sanitize_filename_token(slug) + "__" +
                                         sanitize_filename_token(occ.target_id_or_repo) + "_" +
                                         std::to_string(++q_counter));
            std::string err;
            if (!move_directory_safe(occ.path, qdst, err)) {
                return {false, err, ""};
            }
            auto lres = write_atomic_symlink(occ.path, can_dir);
            if (!lres.ok) return lres;

            JsonValue entry = JsonValue::object();
            entry.set("action", "quarantined");
            entry.set("slug", slug);
            entry.set("originalPath", occ.path.string());
            entry.set("quarantinePath", qdst.string());
            entry.set("canonicalPath", can_dir.string());
            entry.set("symlinkCreated", true);
            manifest_entries.push_back(std::move(entry));
        }
    }

    JsonValue manifest = JsonValue::object();
    manifest.set("batchId", batch_id);
    manifest.set("timestamp", ts);
    manifest.set("entries", manifest_entries);
    write_text_file_atomic(batch_dir / "manifest.json", loadout::util::to_json(manifest, true) + "\n");

    QuarantineBatchRecord rec;
    rec.id = batch_id;
    rec.timestamp = ts;
    rec.dir = batch_dir;
    rec.entry_count = manifest_entries.size();
    quarantine_history_.push_back(std::move(rec));

    cached_snapshot_.reset();
    scan();
    save_lockfile();

    return {true, "", "Resolved drift for " + slug + " (quarantine batch " + batch_id + ")"};
}

OperationResult Engine::undo_last_quarantine() {
    if (quarantine_history_.empty()) {
        // Trigger scan to discover any on-disk quarantine batches
        scan();
    }
    if (quarantine_history_.empty()) {
        return {false, "No quarantine batch available to undo", ""};
    }

    QuarantineBatchRecord batch = quarantine_history_.back();
    fs::path manifest_path = batch.dir / "manifest.json";
    std::string text = read_text_file(manifest_path);
    auto parsed = loadout::util::parse_json(text);
    if (!parsed || !parsed->is_object()) {
        return {false, "Invalid or unreadable quarantine manifest: " + manifest_path.string(), ""};
    }

    const auto* entries_v = parsed->find("entries");
    if (!entries_v || !entries_v->is_array()) {
        return {false, "Manifest missing entries array", ""};
    }

    const auto& entries = entries_v->as_array();
    size_t restored = 0;

    // Reverse order to undo promoted + quarantined cleanly
    for (size_t i = entries.size(); i > 0; --i) {
        const auto& e = entries[i - 1];
        if (!e.is_object()) continue;
        std::string action = e.get_str("action");
        fs::path orig_path(e.get_str("originalPath"));
        fs::path can_path(e.get_str("canonicalPath"));
        fs::path q_path(e.get_str("quarantinePath"));
        bool sym_created = e.get_bool("symlinkCreated", true);

        if (action == "quarantined") {
            if (sym_created && lstat_exists(orig_path)) {
                auto ures = remove_managed_symlink(orig_path);
                if (!ures.ok) return ures;
            }
            if (lstat_exists(q_path)) {
                std::string err;
                if (!move_directory_safe(q_path, orig_path, err)) {
                    return {false, err, ""};
                }
                ++restored;
            }
        } else if (action == "promoted") {
            if (sym_created && lstat_exists(orig_path)) {
                auto ures = remove_managed_symlink(orig_path);
                if (!ures.ok) return ures;
            }
            if (lstat_exists(can_path)) {
                std::string err;
                if (!move_directory_safe(can_path, orig_path, err)) {
                    return {false, err, ""};
                }
                ++restored;
            }
        } else if (action == "quarantined_canonical") {
            if (lstat_exists(q_path)) {
                std::string err;
                if (!move_directory_safe(q_path, orig_path, err)) {
                    return {false, err, ""};
                }
                ++restored;
            }
        } else if (action == "renamed_keep_both") {
            fs::path new_link(e.get_str("newLinkPath"));
            if (!new_link.empty() && lstat_exists(new_link)) {
                auto ures = remove_managed_symlink(new_link);
                if (!ures.ok) return ures;
            }
            if (lstat_exists(can_path)) {
                std::string err;
                if (!move_directory_safe(can_path, orig_path, err)) {
                    return {false, err, ""};
                }
                ++restored;
            }
        }
    }

    // Remove the now-empty quarantine batch directory
    std::error_code ec;
    fs::remove(manifest_path, ec);
    fs::remove(batch.dir, ec);
    quarantine_history_.pop_back();

    cached_snapshot_.reset();
    scan();
    save_lockfile();

    return {true, "", "Restored " + std::to_string(restored) + " item(s) from batch " + batch.id};
}

FrontmatterFixResult Engine::fix_frontmatter(const std::string& slug, bool apply) {
    FrontmatterFixResult res;
    res.slug = slug;

    if (!cached_snapshot_) {
        scan();
    }
    auto dir_opt = find_skill_dir(slug);
    if (!dir_opt) {
        res.ok = false;
        res.error = "Skill not found: " + slug;
        return res;
    }

    fs::path skill_md = find_skill_md_in_dir(*dir_opt);
    if (skill_md.empty()) {
        res.ok = false;
        res.error = "SKILL.md not found in " + dir_opt->string();
        return res;
    }

    res.file_path = skill_md;
    res.before = read_text_file(skill_md);
    res.after = loadout::util::fix_skill_frontmatter(res.before, slug);
    res.changed = (res.before != res.after);
    if (res.changed) {
        res.diff = loadout::util::unified_diff(
            res.before, res.after, slug + "/SKILL.md (before)", slug + "/SKILL.md (after)");
    }

    if (apply && res.changed) {
        if (!write_text_file_atomic(skill_md, res.after)) {
            res.ok = false;
            res.error = "Failed to write repaired SKILL.md to " + skill_md.string();
            return res;
        }
        res.applied = true;
        cached_snapshot_.reset();
        scan();
    }

    return res;
}

std::vector<FrontmatterFixResult> Engine::fix_all_frontmatter(bool apply) {
    CatalogSnapshot snap = scan();
    std::vector<FrontmatterFixResult> results;
    for (const auto& sk : snap.skills) {
        if (!sk.antigravity_valid) {
            results.push_back(fix_frontmatter(sk.slug, apply));
        }
    }
    return results;
}

std::vector<TargetInstance> Engine::list_targets() {
    auto snap = get_catalog();
    return snap.targets;
}

OperationResult Engine::add_target(const TargetInstance& raw_target, const std::string& clone_from_id) {
    if (raw_target.id.empty() || raw_target.skills_dir.empty()) {
        return {false, "Target id and skills_dir are required", ""};
    }

    TargetInstance t = raw_target;
    t.skills_dir = expand_home_path(t.skills_dir.string(), home_dir_);
    if (t.config_root) {
        t.config_root = expand_home_path(t.config_root->string(), home_dir_);
    } else if (t.skills_dir.has_parent_path()) {
        t.config_root = t.skills_dir.parent_path();
    }
    if (t.display_name.empty()) {
        t.display_name = t.id;
    }
    if (t.group.empty()) {
        t.group = std::string(adapter_default_group(t.adapter));
    }
    t.configured = true;

    // Remove from removed_target_ids_ if previously removed
    removed_target_ids_.erase(
        std::remove(removed_target_ids_.begin(), removed_target_ids_.end(), t.id),
        removed_target_ids_.end());

    // Upsert into lockfile_targets_
    bool updated = false;
    for (auto& existing : lockfile_targets_) {
        if (existing.id == t.id) {
            existing = t;
            updated = true;
            break;
        }
    }
    if (!updated) {
        lockfile_targets_.push_back(t);
    }

    cached_snapshot_.reset();
    scan();
    save_lockfile();

    if (!clone_from_id.empty()) {
        auto cres = clone_target(clone_from_id, t.id);
        if (!cres.ok) return cres;
        return {true, "", "Added target " + t.id + " and " + cres.message};
    }

    return {true, "", "Added target " + t.id + " -> " + t.skills_dir.string()};
}

OperationResult Engine::remove_target(const std::string& target_id) {
    if (target_id.empty()) {
        return {false, "target_id is required", ""};
    }
    if (!cached_snapshot_) {
        scan();
    }
    auto target_opt = find_target(target_id);
    if (!target_opt) {
        return {false, "Unknown target: " + target_id, ""};
    }

    // Safely unlink ONLY managed symlinks inside target.skills_dir
    size_t unlinked = 0;
    if (DIR* dp = ::opendir(target_opt->skills_dir.c_str())) {
        while (struct dirent* ent = ::readdir(dp)) {
            std::string name(ent->d_name);
            if (name == "." || name == "..") continue;
            fs::path p = target_opt->skills_dir / name;
            if (is_managed_symlink(p)) {
                if (::unlink(p.c_str()) == 0) {
                    ++unlinked;
                }
            }
        }
        ::closedir(dp);
    }

    lockfile_targets_.erase(
        std::remove_if(lockfile_targets_.begin(), lockfile_targets_.end(),
                       [&](const TargetInstance& t) { return t.id == target_id; }),
        lockfile_targets_.end());

    if (std::find(removed_target_ids_.begin(), removed_target_ids_.end(), target_id) ==
        removed_target_ids_.end()) {
        removed_target_ids_.push_back(target_id);
    }

    cached_snapshot_.reset();
    scan();
    save_lockfile();

    return {true, "", "Removed target " + target_id + " (unlinked " + std::to_string(unlinked) + " managed symlinks)"};
}

OperationResult Engine::clone_target(const std::string& from_id, const std::string& to_id) {
    if (from_id.empty() || to_id.empty()) {
        return {false, "from_id and to_id are required", ""};
    }
    if (from_id == to_id) {
        return {false, "Cannot clone a target onto itself", ""};
    }

    CatalogSnapshot snap = scan();
    auto from_opt = find_target(from_id);
    auto to_opt = find_target(to_id);
    if (!from_opt) return {false, "Source target not found: " + from_id, ""};
    if (!to_opt) return {false, "Destination target not found: " + to_id, ""};

    std::vector<std::string> active_slugs;
    for (const auto& sk : snap.skills) {
        auto it = sk.target_states.find(from_id);
        if (it != sk.target_states.end() && it->second.has_entry &&
            it->second.entry_kind != EntryKind::DanglingSymlink &&
            it->second.entry_kind != EntryKind::PluginShadow) {
            active_slugs.push_back(sk.slug);
        }
    }

    // Ensure destination skills_dir exists even if 0 skills are cloned
    std::error_code ec;
    fs::create_directories(to_opt->skills_dir, ec);

    size_t cloned = 0;
    for (const auto& slug : active_slugs) {
        auto tres = toggle_skill(slug, to_id, true);
        if (tres.ok) {
            ++cloned;
        }
    }

    return {true, "", "Cloned " + std::to_string(cloned) + " skill(s) from " + from_id + " to " + to_id};
}

SkillDetail Engine::get_skill_detail(const std::string& slug) {
    SkillDetail detail;
    if (!is_valid_slug(slug)) {
        detail.error = "Invalid or empty slug";
        return detail;
    }

    CatalogSnapshot snap = get_catalog();
    for (const auto& sk : snap.skills) {
        if (sk.slug == slug) {
            detail.found = true;
            detail.skill = sk;
            auto dir_opt = find_skill_dir(slug);
            if (dir_opt) {
                fs::path md = find_skill_md_in_dir(*dir_opt);
                if (!md.empty()) {
                    detail.markdown = read_text_file(md);
                }
                detail.file_entries = read_skill_file_entries(*dir_opt);
                detail.files.reserve(detail.file_entries.size());
                for (const auto& fe : detail.file_entries) {
                    detail.files.push_back(fe.path);
                }
            }
            return detail;
        }
    }
    detail.error = "Skill not found: " + slug;
    return detail;
}

OperationResult Engine::save_skill_file(
    const std::string& slug,
    const std::string& rel_path,
    const std::string& content) {
    if (!is_valid_slug(slug)) {
        return {false, "Invalid skill slug", ""};
    }
    if (!is_safe_relative_skill_path(rel_path)) {
        return {false, "Invalid or unsafe relative path: " + rel_path, ""};
    }
    if (content.size() > kMaxWriteBytes) {
        return {false, "File content exceeds maximum allowed size (1 MiB)", ""};
    }

    if (!cached_snapshot_) {
        (void)scan();
    }
    auto dir_opt = find_skill_dir(slug);
    if (!dir_opt) {
        return {false, "Skill not found: " + slug, ""};
    }

    std::error_code ec;
    fs::path base_real = fs::canonical(*dir_opt, ec);
    if (ec || base_real.empty()) {
        return {false, "Failed to resolve skill directory: " + ec.message(), ""};
    }

    fs::path candidate = (base_real / fs::path(rel_path)).lexically_normal();
    std::string base_str = base_real.string();
    while (base_str.size() > 1 && base_str.back() == '/') base_str.pop_back();
    std::string cand_str = candidate.string();
    if (!cand_str.starts_with(base_str + "/")) {
        return {false, "Path escapes skill directory: " + rel_path, ""};
    }

    // Walk every existing component under base_real to block symlink escape attacks
    fs::path walk = base_real;
    for (const auto& part : fs::path(rel_path)) {
        walk /= part;
        struct stat st {};
        if (::lstat(walk.c_str(), &st) == 0 && S_ISLNK(st.st_mode)) {
            return {false, "Refusing to write through symlink inside skill directory: " + rel_path, ""};
        }
    }

    fs::path resolved_cand = safe_weakly_canonical(candidate);
    if (!resolved_cand.string().starts_with(base_str + "/")) {
        return {false, "Canonical path escapes skill directory: " + rel_path, ""};
    }

    if (!write_text_file_atomic(candidate, content)) {
        return {false, "Failed to save " + candidate.string(), ""};
    }
    cached_snapshot_.reset();
    (void)scan();
    return {true, "", "Saved " + rel_path + " in " + slug};
}

OperationResult Engine::save_skill_markdown(const std::string& slug, const std::string& markdown) {
    if (!is_valid_slug(slug)) {
        return {false, "Invalid skill slug", ""};
    }
    if (!cached_snapshot_) {
        (void)scan();
    }
    auto dir_opt = find_skill_dir(slug);
    if (!dir_opt) {
        return {false, "Skill not found: " + slug, ""};
    }
    fs::path md = find_skill_md_in_dir(*dir_opt);
    std::string rel_name = md.empty() ? "SKILL.md" : md.filename().string();
    return save_skill_file(slug, rel_name, markdown);
}

std::vector<Profile> Engine::get_profiles() const {
    std::vector<Profile> out;
    out.reserve(profiles_.size());
    for (const auto& [pname, prof] : profiles_) {
        (void)pname;
        out.push_back(prof);
    }
    return out;
}

OperationResult Engine::save_profile(
    const std::string& raw_name,
    const std::map<std::string, std::map<std::string, bool>>& wiring,
    const std::vector<std::string>& skills) {
    std::string name = trim_whitespace(raw_name);
    if (name.empty() || name.size() > kMaxProfileNameLength) {
        return {false, "Profile name must be 1 to 64 characters", ""};
    }
    for (char ch : name) {
        if (static_cast<unsigned char>(ch) < 0x20U) {
            return {false, "Profile name contains invalid control characters", ""};
        }
    }

    Profile prof;
    prof.name = name;
    prof.skills = skills;
    prof.wiring = wiring;

    if (prof.wiring.empty() && prof.skills.empty()) {
        CatalogSnapshot snap = get_catalog();
        for (const auto& sk : snap.skills) {
            for (const auto& [tid, tst] : sk.target_states) {
                prof.wiring[sk.slug][tid] = tst.enabled;
            }
        }
    }

    profiles_[name] = std::move(prof);
    active_profile_ = name;
    if (cached_snapshot_) {
        cached_snapshot_->active_profile = active_profile_;
        cached_snapshot_->profiles = get_profiles();
    }
    if (!save_lockfile()) {
        return {false, "Failed to persist profile to loadout.lock.json", ""};
    }
    return {true, "", "Saved profile '" + name + "'"};
}

OperationResult Engine::apply_profile(const std::string& name) {
    auto it = profiles_.find(name);
    if (it == profiles_.end()) {
        return {false, "Profile not found: " + name, ""};
    }
    const Profile prof = it->second;
    CatalogSnapshot snap = scan();

    std::set<std::string> skill_set(prof.skills.begin(), prof.skills.end());
    size_t wired_count = 0;
    size_t unwired_count = 0;
    std::set<std::string> touched_targets;

    for (const auto& sk : snap.skills) {
        for (const auto& target : snap.targets) {
            auto tst_it = sk.target_states.find(target.id);
            bool has_entry = (tst_it != sk.target_states.end() && tst_it->second.has_entry);
            EntryKind kind = has_entry ? tst_it->second.entry_kind : EntryKind::ManagedSymlink;

            // Profiles never alter physical copies, external symlinks, broken links, or plugin shadows
            if (has_entry && kind != EntryKind::ManagedSymlink) {
                continue;
            }
            bool currently_on = (has_entry && kind == EntryKind::ManagedSymlink);

            bool want_on = false;
            if (!prof.wiring.empty()) {
                if (auto s_it = prof.wiring.find(sk.slug); s_it != prof.wiring.end()) {
                    if (auto t_it = s_it->second.find(target.id); t_it != s_it->second.end()) {
                        want_on = t_it->second;
                    }
                }
            } else {
                want_on = skill_set.contains(sk.slug);
            }

            if (want_on == currently_on) {
                continue;
            }

            fs::path link_path = target.skills_dir / sk.slug;
            if (want_on) {
                fs::path can_dir = canonical_skills_dir() / sk.slug;
                if (find_skill_md_in_dir(can_dir).empty()) {
                    auto existing_dir = find_skill_dir(sk.slug);
                    if (!existing_dir) continue;
                    std::string err;
                    if (!copy_directory_safe(*existing_dir, can_dir, err)) continue;
                }
                if (write_atomic_symlink(link_path, can_dir).ok) {
                    ++wired_count;
                    touched_targets.insert(target.id);
                }
            } else {
                if (remove_managed_symlink(link_path).ok) {
                    ++unwired_count;
                    touched_targets.insert(target.id);
                }
            }
        }
    }

    for (const auto& target : snap.targets) {
        if (touched_targets.contains(target.id)) {
            sync_antigravity_skills_json(target);
        }
    }

    active_profile_ = name;
    cached_snapshot_.reset();
    (void)scan();
    save_lockfile();

    return {true,
            "",
            "Applied profile '" + name + "' (" + std::to_string(wired_count) + " wired, " +
                std::to_string(unwired_count) + " unwired)"};
}

OperationResult Engine::delete_profile(const std::string& name) {
    auto it = profiles_.find(name);
    if (it == profiles_.end()) {
        return {false, "Profile not found: " + name, ""};
    }
    profiles_.erase(it);
    if (active_profile_ == name) {
        active_profile_.clear();
    }
    if (cached_snapshot_) {
        cached_snapshot_->active_profile = active_profile_;
        cached_snapshot_->profiles = get_profiles();
    }
    if (!save_lockfile()) {
        return {false, "Failed to update loadout.lock.json", ""};
    }
    return {true, "", "Deleted profile '" + name + "'"};
}

WorktreeHydrateResult Engine::hydrate_worktree(
    const fs::path& worktree_path_raw,
    const std::map<std::string, std::string>& env_map,
    const fs::path& repo_root_raw,
    const std::string& provider_id) {
    WorktreeHydrateResult res;
    if (worktree_path_raw.empty()) {
        res.ok = false;
        res.error = "worktree_path is required";
        return res;
    }

    fs::path worktree_path = expand_home_path(worktree_path_raw.string(), home_dir_);
    struct stat wst {};
    if (::stat(worktree_path.c_str(), &wst) != 0 || !S_ISDIR(wst.st_mode)) {
        res.ok = false;
        res.error = "Worktree path does not exist: " + worktree_path.string();
        return res;
    }

    CatalogSnapshot snap = scan();

    // 1. Match TargetInstance from env_map (CLAUDE_CONFIG_DIR / CODEX_HOME / GEMINI_CONFIG_DIR / OPENCODE_CONFIG_DIR)
    auto match_config_dir = [&](const std::string& env_val, AdapterKind preferred_adapter) -> std::optional<TargetInstance> {
        fs::path want = safe_weakly_canonical(expand_home_path(env_val, home_dir_));
        for (const auto& t : snap.targets) {
            if (t.config_root && safe_weakly_canonical(*t.config_root) == want) {
                return t;
            }
            if (t.skills_dir.has_parent_path() &&
                safe_weakly_canonical(t.skills_dir.parent_path()) == want) {
                return t;
            }
            if (t.env_hint) {
                auto eq = t.env_hint->find('=');
                if (eq != std::string::npos) {
                    fs::path hint_dir = safe_weakly_canonical(
                        expand_home_path(t.env_hint->substr(eq + 1), home_dir_));
                    if (hint_dir == want) return t;
                }
            }
        }
        // Check unconfigured siblings too
        for (const auto& t : snap.unconfigured_siblings) {
            if (t.config_root && safe_weakly_canonical(*t.config_root) == want) {
                return t;
            }
        }
        // Fallback: synthetic target for the explicit env config dir
        TargetInstance syn;
        syn.id = std::string(adapter_to_string(preferred_adapter)) + "-env";
        syn.adapter = preferred_adapter;
        syn.config_root = want;
        syn.skills_dir = want / "skills";
        return syn;
    };

    std::optional<TargetInstance> matched;
    if (auto it = env_map.find("CLAUDE_CONFIG_DIR"); it != env_map.end() && !it->second.empty()) {
        matched = match_config_dir(it->second, AdapterKind::ClaudeCode);
    } else if (auto it2 = env_map.find("CODEX_HOME"); it2 != env_map.end() && !it2->second.empty()) {
        matched = match_config_dir(it2->second, AdapterKind::Codex);
    } else if (auto it3 = env_map.find("GEMINI_CONFIG_DIR"); it3 != env_map.end() && !it3->second.empty()) {
        matched = match_config_dir(it3->second, AdapterKind::Antigravity);
    } else if (auto it4 = env_map.find("OPENCODE_CONFIG_DIR"); it4 != env_map.end() && !it4->second.empty()) {
        matched = match_config_dir(it4->second, AdapterKind::OpenCode);
    }

    if (!matched && !provider_id.empty()) {
        auto want_adapter = adapter_from_string(provider_id);
        for (const auto& t : snap.targets) {
            if (t.id == provider_id || (want_adapter && t.adapter == *want_adapter)) {
                matched = t;
                break;
            }
        }
    }

    if (!matched && !snap.targets.empty()) {
        matched = snap.targets.front();
    }

    if (matched) {
        res.matched_target_id = matched->id;
    }

    // 2. Resolve git info/exclude (handling both standard repos & linked git worktrees!)
    fs::path discovered_main_wt;
    fs::path exclude_file = resolve_git_exclude_file(worktree_path, discovered_main_wt);
    res.exclude_file = exclude_file;

    fs::path main_repo = repo_root_raw.empty() ? discovered_main_wt
                                               : expand_home_path(repo_root_raw.string(), home_dir_);

    std::set<std::string> exclude_patterns;
    std::set<std::string> linked_set;

    auto link_into_worktree = [&](const std::string& rel_container,
                                  const std::string& slug,
                                  const fs::path& src_skill_dir) {
        fs::path dest_dir = worktree_path / rel_container;
        fs::path dest_link = dest_dir / slug;
        if (lstat_exists(dest_link)) {
            // Already present in worktree (e.g. tracked by git); don't clobber!
            return;
        }
        auto lres = write_atomic_symlink(dest_link, src_skill_dir);
        if (lres.ok) {
            std::string rel_entry = "/" + rel_container + "/" + slug;
            exclude_patterns.insert(rel_entry);
            if (linked_set.insert(slug).second) {
                res.linked_skills.push_back(slug);
            }
        }
    };

    // 3. Hydrate any untracked project-local skills from main repo into the worktree
    if (!main_repo.empty() && safe_weakly_canonical(main_repo) != safe_weakly_canonical(worktree_path)) {
        for (const char* rel_c : {".claude/skills", ".agents/skills", ".codex/skills",
                                  ".opencode/skills", ".cursor/skills", ".gemini/skills"}) {
            fs::path src_container = main_repo / rel_c;
            if (DIR* dp = ::opendir(src_container.c_str())) {
                while (struct dirent* ent = ::readdir(dp)) {
                    std::string slug(ent->d_name);
                    if (slug == "." || slug == ".." || slug.starts_with(".loadout-tmp-")) continue;
                    fs::path src_entry = src_container / slug;
                    if (!find_skill_md_in_dir(src_entry).empty()) {
                        link_into_worktree(rel_c, slug, safe_weakly_canonical(src_entry));
                    }
                }
                ::closedir(dp);
            }
        }
    }

    // 4. Hydrate active skills from the matched target into the worktree
    if (matched) {
        std::string rel_container = adapter_worktree_skills_relpath(matched->adapter);
        for (const auto& sk : snap.skills) {
            auto it = sk.target_states.find(matched->id);
            if (it == sk.target_states.end() || !it->second.has_entry) continue;
            if (it->second.entry_kind == EntryKind::DanglingSymlink ||
                it->second.entry_kind == EntryKind::PluginShadow) {
                continue;
            }
            fs::path src_dir = sk.is_canonical ? sk.canonical_path : safe_weakly_canonical(it->second.path);
            if (!find_skill_md_in_dir(src_dir).empty()) {
                link_into_worktree(rel_container, sk.slug, src_dir);
            }
        }
    }

    // 5. Append managed relative symlink paths to <git-common-dir>/info/exclude idempotently
    if (!exclude_file.empty() && !exclude_patterns.empty()) {
        std::error_code ec;
        fs::create_directories(exclude_file.parent_path(), ec);
        std::string existing = read_text_file(exclude_file);
        std::set<std::string> existing_lines;
        {
            std::istringstream iss(existing);
            std::string line;
            while (std::getline(iss, line)) {
                existing_lines.insert(trim_whitespace(line));
            }
        }

        std::ostringstream to_append;
        if (!existing.empty() && existing.back() != '\n') {
            to_append << "\n";
        }
        bool added_header = existing.find("# loadout-managed worktree skills") != std::string::npos;
        size_t newly_added = 0;
        for (const auto& pat : exclude_patterns) {
            res.excluded_entries.push_back(pat);
            if (!existing_lines.contains(pat)) {
                if (!added_header) {
                    to_append << "# loadout-managed worktree skills\n";
                    added_header = true;
                }
                to_append << pat << "\n";
                ++newly_added;
            }
        }
        if (newly_added > 0) {
            std::ofstream out(exclude_file, std::ios::out | std::ios::app | std::ios::binary);
            if (!out) {
                res.ok = false;
                res.error = "Failed to update git exclude file: " + exclude_file.string();
                return res;
            }
            out << to_append.str();
        }
    }

    return res;
}

// ---------------------------------------------------------------------------
// JSON Serializers (REST / JSON-RPC 2.0 / CLI --json contract)
// ---------------------------------------------------------------------------

JsonValue to_target_json(const TargetInstance& target) {
    JsonValue obj = JsonValue::object();
    obj.set("id", target.id);
    obj.set("suggestedId", target.id);
    obj.set("displayName", target.display_name);
    obj.set("group", target.group);
    obj.set("adapter", std::string(adapter_to_string(target.adapter)));
    obj.set("skillsDir", target.skills_dir.string());
    if (target.config_root) {
        obj.set("configRoot", target.config_root->string());
    }
    if (target.env_hint) {
        obj.set("envHint", *target.env_hint);
    }
    obj.set("linkMode", std::string(link_mode_to_string(target.link_mode)));
    obj.set("autoDiscovered", target.auto_discovered);
    obj.set("configured", target.configured);
    return obj;
}

JsonValue to_catalog_json(const CatalogSnapshot& snap) {
    JsonValue root = JsonValue::object();

    // Compute per-target active counts and context token overhead
    std::map<std::string, size_t> target_active_counts;
    std::map<std::string, size_t> target_token_sums;
    for (const auto& sk : snap.skills) {
        for (const auto& [tid, tst] : sk.target_states) {
            if (tst.has_entry && tst.entry_kind != EntryKind::DanglingSymlink &&
                tst.entry_kind != EntryKind::PluginShadow) {
                ++target_active_counts[tid];
                target_token_sums[tid] += sk.approx_tokens;
            }
        }
    }

    // stats
    JsonValue stats = JsonValue::object();
    stats.set("totalSkills", static_cast<int64_t>(snap.stats.total_skills));
    stats.set("canonicalSkills", static_cast<int64_t>(snap.stats.canonical_skills));
    stats.set("totalTargets", static_cast<int64_t>(snap.stats.total_targets));
    stats.set("configuredTargets", static_cast<int64_t>(snap.stats.total_targets));
    stats.set("totalRepos", static_cast<int64_t>(snap.stats.total_repos));
    stats.set("exactDuplicates", static_cast<int64_t>(snap.stats.exact_duplicates));
    stats.set("duplicateBytes", static_cast<int64_t>(snap.stats.duplicate_bytes));
    stats.set("redundantBytes", static_cast<int64_t>(snap.stats.duplicate_bytes));
    stats.set("driftedGroups", static_cast<int64_t>(snap.stats.drifted_groups));
    stats.set("driftedSkills", static_cast<int64_t>(snap.stats.drifted_groups));
    stats.set("brokenLinks", static_cast<int64_t>(snap.stats.broken_links));
    stats.set("frontmatterIssues", static_cast<int64_t>(snap.stats.frontmatter_issues));
    stats.set("frontmatterWarnings", static_cast<int64_t>(snap.stats.frontmatter_issues));
    stats.set("unmanagedCopies", static_cast<int64_t>(snap.stats.unmanaged_copies));
    stats.set("projectLocalSkills", static_cast<int64_t>(snap.stats.project_local_skills));
    stats.set("repoLocalSkills", static_cast<int64_t>(snap.stats.project_local_skills));
    root.set("stats", std::move(stats));

    root.set("canonicalRoot", snap.canonical_root.string());
    root.set("lastScanMs", snap.last_scan_ms);
    root.set("canUndo", snap.can_undo);
    if (!snap.last_quarantine_batch.empty()) {
        root.set("lastQuarantineBatch", snap.last_quarantine_batch);
    }
    root.set("activeProfile", snap.active_profile);
    {
        JsonValue profs = JsonValue::array();
        for (const auto& prof : snap.profiles) {
            JsonValue pj = JsonValue::object();
            pj.set("name", prof.name);
            JsonValue sarr = JsonValue::array();
            for (const auto& s : prof.skills) sarr.push_back(s);
            pj.set("skills", std::move(sarr));
            JsonValue wobj = JsonValue::object();
            for (const auto& [slug_k, tmap] : prof.wiring) {
                JsonValue tobj = JsonValue::object();
                for (const auto& [tid_k, en] : tmap) tobj.set(tid_k, en);
                wobj.set(slug_k, std::move(tobj));
            }
            pj.set("wiring", std::move(wobj));
            profs.push_back(std::move(pj));
        }
        root.set("profiles", std::move(profs));
    }

    // unconfiguredSiblings
    JsonValue siblings = JsonValue::array();
    for (const auto& s : snap.unconfigured_siblings) {
        siblings.push_back(to_target_json(s));
    }
    root.set("unconfiguredSiblings", std::move(siblings));

    // targets
    JsonValue targets = JsonValue::array();
    for (const auto& t : snap.targets) {
        JsonValue tj = to_target_json(t);
        tj.set("activeSkillCount", static_cast<int64_t>(target_active_counts[t.id]));
        tj.set("activeCount", static_cast<int64_t>(target_active_counts[t.id]));
        tj.set("approxTokens", static_cast<int64_t>(target_token_sums[t.id]));
        targets.push_back(std::move(tj));
    }
    root.set("targets", std::move(targets));

    // skills
    JsonValue skills = JsonValue::array();
    for (const auto& sk : snap.skills) {
        JsonValue sj = JsonValue::object();
        sj.set("slug", sk.slug);
        sj.set("name", sk.name);
        sj.set("description", sk.description);
        JsonValue tags = JsonValue::array();
        for (const auto& tg : sk.tags) tags.push_back(tg);
        sj.set("tags", std::move(tags));
        sj.set("canonicalPath", sk.canonical_path.string());
        sj.set("isCanonical", sk.is_canonical);
        sj.set("treeHash", sk.tree_hash);
        sj.set("fileCount", static_cast<int64_t>(sk.file_count));
        sj.set("totalBytes", static_cast<int64_t>(sk.total_bytes));
        sj.set("approxTokens", static_cast<int64_t>(sk.approx_tokens));
        sj.set("antigravityValid", sk.antigravity_valid);
        JsonValue issues = JsonValue::array();
        for (const auto& iss : sk.frontmatter_issues) issues.push_back(iss);
        sj.set("frontmatterIssues", std::move(issues));
        sj.set("hasScripts", sk.has_scripts);
        sj.set("hasReferences", sk.has_references);
        sj.set("isRepoLocalOnly", sk.is_repo_local);
        if (!sk.repo_root.empty()) sj.set("repoRoot", sk.repo_root);
        sj.set("hasDuplicates", sk.has_duplicates);
        sj.set("hasDrift", sk.has_drift);
        sj.set("hasPhysicalCopy", sk.has_physical_copy);
        sj.set("hasPluginShadow", sk.has_plugin_shadow);

        JsonValue enabled_in = JsonValue::object();
        JsonValue target_status = JsonValue::object();
        JsonValue target_states = JsonValue::object();
        for (const auto& [tid, tst] : sk.target_states) {
            enabled_in.set(tid, tst.enabled);
            target_status.set(
                tid, tst.has_entry ? std::string(entry_kind_to_string(tst.entry_kind)) : std::string("unwired"));
            JsonValue tsj = JsonValue::object();
            tsj.set("enabled", tst.enabled);
            tsj.set("hasEntry", tst.has_entry);
            tsj.set("entryKind", tst.has_entry ? std::string(entry_kind_to_string(tst.entry_kind))
                                               : std::string("unwired"));
            tsj.set("path", tst.path);
            tsj.set("incompatible", tst.incompatible);
            target_states.set(tid, std::move(tsj));
        }
        sj.set("enabledIn", std::move(enabled_in));
        sj.set("targetStatus", std::move(target_status));
        sj.set("targetStates", std::move(target_states));

        JsonValue occs = JsonValue::array();
        for (const auto& o : sk.occurrences) occs.push_back(to_occurrence_json(o));
        sj.set("occurrences", std::move(occs));
        skills.push_back(std::move(sj));
    }
    root.set("skills", std::move(skills));

    // duplicateGroups
    JsonValue dup_groups = JsonValue::array();
    for (const auto& dg : snap.duplicate_groups) {
        JsonValue dgj = JsonValue::object();
        dgj.set("slug", dg.slug);
        dgj.set("treeHash", dg.tree_hash);
        dgj.set("redundantBytes", static_cast<int64_t>(dg.redundant_bytes));
        JsonValue occs = JsonValue::array();
        for (const auto& o : dg.occurrences) occs.push_back(to_occurrence_json(o));
        dgj.set("occurrences", std::move(occs));
        dup_groups.push_back(std::move(dgj));
    }
    root.set("duplicateGroups", std::move(dup_groups));

    // driftedGroups
    JsonValue drift_groups = JsonValue::array();
    for (const auto& dr : snap.drifted_groups) {
        JsonValue drj = JsonValue::object();
        drj.set("slug", dr.slug);
        if (!dr.variants.empty()) {
            drj.set("basePath", dr.variants.front().occurrence.path.string());
        }
        drj.set("unifiedDiff", dr.unified_diff);
        JsonValue vars = JsonValue::array();
        for (const auto& v : dr.variants) {
            JsonValue vj = JsonValue::object();
            vj.set("path", v.occurrence.path.string());
            vj.set("realPath", v.occurrence.real_path.string());
            vj.set("targetIdOrRepo", v.occurrence.target_id_or_repo);
            vj.set("treeHash", v.occurrence.tree_hash);
            vj.set("fileCount", static_cast<int64_t>(v.occurrence.file_count));
            vj.set("totalBytes", static_cast<int64_t>(v.occurrence.total_bytes));
            vj.set("mtimeEpoch", v.occurrence.mtime_epoch);
            vj.set("entryKind", std::string(entry_kind_to_string(v.occurrence.entry_kind)));
            vj.set("isRepoLocal", v.occurrence.is_repo_local);
            vj.set("diffFromBase", v.unified_diff_from_primary);
            vj.set("diffVsFirst", v.unified_diff_from_primary);
            JsonValue files = JsonValue::array();
            for (const auto& f : v.files) files.push_back(f);
            vj.set("files", std::move(files));
            vars.push_back(std::move(vj));
        }
        drj.set("variants", std::move(vars));
        drift_groups.push_back(std::move(drj));
    }
    root.set("driftedGroups", std::move(drift_groups));

    // brokenLinks
    JsonValue broken = JsonValue::array();
    for (const auto& b : snap.broken_links) {
        JsonValue bj = JsonValue::object();
        bj.set("slug", b.slug);
        bj.set("path", b.path.string());
        bj.set("targetId", b.target_id_or_repo);
        bj.set("targetIdOrRepo", b.target_id_or_repo);
        bj.set("rawTarget", b.symlink_target);
        bj.set("danglingTarget", b.symlink_target);
        bj.set("symlinkTarget", b.symlink_target);
        broken.push_back(std::move(bj));
    }
    root.set("brokenLinks", std::move(broken));

    return root;
}

JsonValue to_skill_detail_json(const SkillDetail& detail) {
    JsonValue obj = JsonValue::object();
    obj.set("found", detail.found);
    if (!detail.error.empty()) obj.set("error", detail.error);
    if (!detail.found) return obj;

    obj.set("slug", detail.skill.slug);
    obj.set("name", detail.skill.name);
    obj.set("description", detail.skill.description);
    obj.set("canonicalPath", detail.skill.canonical_path.string());
    obj.set("markdown", detail.markdown);
    obj.set("antigravityValid", detail.skill.antigravity_valid);

    JsonValue files = JsonValue::array();
    JsonValue file_paths = JsonValue::array();
    if (!detail.file_entries.empty()) {
        for (const auto& fe : detail.file_entries) {
            JsonValue fj = JsonValue::object();
            fj.set("path", fe.path);
            fj.set("size", static_cast<int64_t>(fe.size));
            fj.set("isText", fe.is_text);
            fj.set("content", fe.content);
            files.push_back(std::move(fj));
            file_paths.push_back(fe.path);
        }
    } else {
        for (const auto& f : detail.files) {
            JsonValue fj = JsonValue::object();
            fj.set("path", f);
            fj.set("size", 0);
            fj.set("isText", true);
            fj.set("content", "");
            files.push_back(std::move(fj));
            file_paths.push_back(f);
        }
    }
    obj.set("files", std::move(files));
    obj.set("filePaths", std::move(file_paths));

    loadout::util::FrontmatterInfo fm = loadout::util::parse_skill_markdown(detail.markdown, detail.skill.slug);
    obj.set("frontmatter", to_frontmatter_json(fm));

    JsonValue enabled_in = JsonValue::object();
    for (const auto& [tid, tst] : detail.skill.target_states) {
        enabled_in.set(tid, tst.enabled);
    }
    obj.set("enabledIn", std::move(enabled_in));

    JsonValue occs = JsonValue::array();
    for (const auto& o : detail.skill.occurrences) occs.push_back(to_occurrence_json(o));
    obj.set("occurrences", std::move(occs));
    return obj;
}

JsonValue to_consolidate_result_json(const ConsolidateResult& res) {
    JsonValue obj = JsonValue::object();
    obj.set("ok", res.ok);
    if (!res.error.empty()) obj.set("error", res.error);
    obj.set("dryRun", res.plan.dry_run);
    obj.set("consolidatedCount", static_cast<int64_t>(res.plan.skills_promoted + res.plan.copies_quarantined));
    obj.set("skillsPromoted", static_cast<int64_t>(res.plan.skills_promoted));
    obj.set("quarantinedCount", static_cast<int64_t>(res.plan.copies_quarantined));
    obj.set("symlinksCreated", static_cast<int64_t>(res.plan.symlinks_created));
    obj.set("bytesReclaimed", static_cast<int64_t>(res.plan.bytes_deduplicated));
    if (!res.quarantine_batch_id.empty()) {
        JsonValue qr = JsonValue::object();
        qr.set("batchId", res.quarantine_batch_id);
        qr.set("quarantineDir", res.quarantine_dir.string());
        qr.set("movedCount", static_cast<int64_t>(res.plan.skills_promoted + res.plan.copies_quarantined));
        obj.set("quarantine", std::move(qr));
        obj.set("quarantineBatchId", res.quarantine_batch_id);
        obj.set("quarantineDir", res.quarantine_dir.string());
    }
    JsonValue actions = JsonValue::array();
    for (const auto& a : res.plan.actions) {
        JsonValue aj = JsonValue::object();
        aj.set("slug", a.slug);
        std::string mapped_action = a.kind;
        if (a.kind == "promote") mapped_action = "move-canonical";
        else if (a.kind == "quarantine_and_link") mapped_action = "quarantine-and-link";
        else if (a.kind == "keep_repo_local") mapped_action = "keep-repo-local";
        aj.set("action", mapped_action);
        aj.set("kind", a.kind);
        aj.set("sourcePath", a.source_path.string());
        aj.set("destPath", a.dest_path.string());
        if (!a.quarantine_path.empty()) aj.set("quarantinePath", a.quarantine_path.string());
        aj.set("isRepoLocal", a.is_repo_local);
        aj.set("targetId", a.target_id);
        aj.set("reason", a.reason);
        actions.push_back(std::move(aj));
    }
    obj.set("actions", std::move(actions));
    return obj;
}

JsonValue to_frontmatter_fix_json(const FrontmatterFixResult& res) {
    JsonValue obj = JsonValue::object();
    obj.set("ok", res.ok);
    if (!res.error.empty()) obj.set("error", res.error);
    obj.set("skill", res.slug);
    obj.set("slug", res.slug);
    obj.set("filePath", res.file_path.string());
    obj.set("changed", res.changed);
    obj.set("applied", res.applied);
    obj.set("before", res.before);
    obj.set("after", res.after);
    obj.set("unifiedDiff", res.diff);
    obj.set("diff", res.diff);
    if (!res.after.empty()) {
        auto fm = loadout::util::parse_skill_markdown(res.after, res.slug);
        obj.set("antigravityValid", fm.antigravity_valid);
    }
    return obj;
}

JsonValue to_worktree_hydrate_json(const WorktreeHydrateResult& res) {
    JsonValue obj = JsonValue::object();
    obj.set("ok", res.ok);
    if (!res.error.empty()) obj.set("error", res.error);
    obj.set("matchedTargetId", res.matched_target_id);
    if (!res.exclude_file.empty()) obj.set("excludeFile", res.exclude_file.string());
    JsonValue linked = JsonValue::array();
    for (const auto& s : res.linked_skills) linked.push_back(s);
    obj.set("linkedSkills", std::move(linked));
    JsonValue excl = JsonValue::array();
    for (const auto& e : res.excluded_entries) excl.push_back(e);
    obj.set("excludedEntries", std::move(excl));
    return obj;
}

JsonValue to_operation_result_json(const OperationResult& res) {
    JsonValue obj = JsonValue::object();
    obj.set("ok", res.ok);
    if (!res.error.empty()) obj.set("error", res.error);
    if (!res.message.empty()) obj.set("message", res.message);
    return obj;
}

JsonValue to_profiles_json(
    const std::vector<Profile>& profiles,
    std::string_view active_profile) {
    JsonValue obj = JsonValue::object();
    obj.set("activeProfile", std::string(active_profile));
    JsonValue arr = JsonValue::array();
    for (const auto& prof : profiles) {
        JsonValue pj = JsonValue::object();
        pj.set("name", prof.name);
        JsonValue sarr = JsonValue::array();
        for (const auto& s : prof.skills) sarr.push_back(s);
        pj.set("skills", std::move(sarr));
        JsonValue wobj = JsonValue::object();
        for (const auto& [slug_k, tmap] : prof.wiring) {
            JsonValue tobj = JsonValue::object();
            for (const auto& [tid_k, en] : tmap) tobj.set(tid_k, en);
            wobj.set(slug_k, std::move(tobj));
        }
        pj.set("wiring", std::move(wobj));
        arr.push_back(std::move(pj));
    }
    obj.set("profiles", std::move(arr));
    return obj;
}

}  // namespace loadout::core
