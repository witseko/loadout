#include <sys/stat.h>
#include <unistd.h>

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "core/engine.hpp"
#include "core/scanner.hpp"
#include "core/targets.hpp"
#include "core/types.hpp"
#include "util/json.hpp"
#include "util/sha256.hpp"

namespace fs = std::filesystem;
using namespace loadout::core;

namespace {

int g_assertions = 0;

#define CHECK(cond)                                                                         \
    do {                                                                                    \
        ++g_assertions;                                                                     \
        if (!(cond)) {                                                                      \
            std::cerr << "FAIL at " << __FILE__ << ":" << __LINE__ << ": " #cond << "\n";  \
            std::exit(1);                                                                   \
        }                                                                                   \
    } while (0)

struct TempSandbox {
    fs::path root;

    TempSandbox() {
        std::string tmpl = "/tmp/loadout_core_test_XXXXXX";
        std::vector<char> buf(tmpl.begin(), tmpl.end());
        buf.push_back('\0');
        char* created = ::mkdtemp(buf.data());
        CHECK(created != nullptr);
        root = fs::path(created);
    }

    ~TempSandbox() {
        if (!root.empty()) {
            std::error_code ec;
            fs::remove_all(root, ec);
        }
    }
};

void write_file(const fs::path& p, const std::string& content) {
    fs::create_directories(p.parent_path());
    std::ofstream out(p, std::ios::out | std::ios::binary | std::ios::trunc);
    CHECK(out.good());
    out << content;
}

std::string read_file(const fs::path& p) {
    std::ifstream in(p, std::ios::in | std::ios::binary);
    if (!in) return {};
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

bool is_symlink_path(const fs::path& p) {
    struct stat st {};
    return ::lstat(p.c_str(), &st) == 0 && S_ISLNK(st.st_mode);
}

bool is_real_dir_path(const fs::path& p) {
    struct stat st {};
    return ::lstat(p.c_str(), &st) == 0 && S_ISDIR(st.st_mode);
}

std::string run_capture(const std::string& cmd) {
    FILE* pipe = ::popen(cmd.c_str(), "r");
    CHECK(pipe != nullptr);
    std::string out;
    char buf[512];
    while (std::fgets(buf, sizeof(buf), pipe)) {
        out += buf;
    }
    ::pclose(pipe);
    return out;
}

void test_discovery_duplicates_drift_and_safety() {
    TempSandbox sb;
    const fs::path home = sb.root;

    // 1. Multi-account Claude Code (~/.claude + ~/.claude-second + unconfigured ~/.claude-work)
    const std::string git_release_md =
        "---\nname: git-release\ndescription: Tag and cut semantic releases safely.\ntags: [git, release]\n---\n\n# Git Release\n\nFollow semver.\n";
    write_file(home / ".claude/skills/git-release/SKILL.md", git_release_md);
    write_file(home / ".claude/skills/git-release/scripts/bump.sh", "#!/bin/sh\necho bump\n");

    // Exact duplicate in ZCode
    write_file(home / ".zcode/skills/git-release/SKILL.md", git_release_md);
    write_file(home / ".zcode/skills/git-release/scripts/bump.sh", "#!/bin/sh\necho bump\n");

    // Second Claude account with a skill missing frontmatter (Antigravity incompatible)
    const std::string no_fm_body =
        "# Legacy Helper\n\nHelps review pull requests quickly without YAML frontmatter.\n\n- Check tests\n- Check lint\n";
    write_file(home / ".claude-second/skills/legacy-helper/SKILL.md", no_fm_body);
    write_file(home / ".claude-second/settings.json", "{}\n");

    // Third Claude account with settings.json ONLY (no skills/ dir yet -> unconfigured sibling)
    write_file(home / ".claude-work/settings.json", "{}\n");

    // Codex + Codex sibling
    write_file(home / ".codex/skills/codex-only/SKILL.md",
               "---\nname: codex-only\ndescription: Codex specialized helper.\n---\nBody.\n");
    write_file(home / ".codex-alt/config.toml", "model = \"o3\"\n");

    // Drifted skill: playwright-e2e differs between Claude and ZCode
    write_file(home / ".claude/skills/playwright-e2e/SKILL.md",
               "---\nname: playwright-e2e\ndescription: Run Playwright E2E tests.\n---\nRun `npx playwright test --headed`.\n");
    write_file(home / ".zcode/skills/playwright-e2e/SKILL.md",
               "---\nname: playwright-e2e\ndescription: Old Playwright E2E runner.\n---\nRun `npx playwright test`.\n");

    // Gemini / Antigravity + read-only plugin shadow
    fs::create_directories(home / ".gemini/config/skills");
    write_file(home / ".gemini/config/plugins/acme/skills/plugin-tool/SKILL.md",
               "---\nname: plugin-tool\ndescription: Built-in plugin skill.\n---\nPlugin.\n");

    // OpenCode with opencode.jsonc (comments!) pointing to custom skill root
    fs::create_directories(home / ".config/opencode/skills");
    write_file(home / ".config/opencode/opencode.jsonc",
               "{\n  // Custom skills folder\n  /* block comment */\n  \"skills\": [\"~/oc-extra-skills\"],\n}\n");
    write_file(home / "oc-extra-skills/oc-custom/SKILL.md",
               "---\nname: oc-custom\ndescription: Loaded from opencode.jsonc.\n---\nOC.\n");

    // Cursor + Windsurf
    fs::create_directories(home / ".cursor/skills");
    fs::create_directories(home / ".codeium/windsurf/skills");

    // Dangling symlink & circular symlink loop inside targets
    CHECK(::symlink("/nonexistent/loadout/target/missing",
                    (home / ".claude/skills/broken-link").c_str()) == 0);
    CHECK(::symlink((home / ".claude/skills/loop-b").c_str(),
                    (home / ".claude/skills/loop-a").c_str()) == 0);
    CHECK(::symlink((home / ".claude/skills/loop-a").c_str(),
                    (home / ".claude/skills/loop-b").c_str()) == 0);

    // Fake git repo with repo-local skill + circular dir loop + pruned node_modules & TCC dir
    fs::create_directories(home / "dev/my-app/.git");
    write_file(home / "dev/my-app/.claude/skills/deploy-app/SKILL.md",
               "---\nname: deploy-app\ndescription: Project-local deploy workflow.\n---\nDeploy.\n");
    CHECK(::symlink((home / "dev/my-app").c_str(),
                    (home / "dev/my-app/self-loop").c_str()) == 0);
    write_file(home / "dev/my-app/node_modules/.claude/skills/should-skip-nm/SKILL.md",
               "---\nname: should-skip-nm\ndescription: Should be pruned.\n---\nSkip.\n");
    write_file(home / "dev/my-app/Library/.claude/skills/should-skip-tcc/SKILL.md",
               "---\nname: should-skip-tcc\ndescription: Should be pruned.\n---\nSkip.\n");

    Engine engine(EngineConfig{.home_dir = home});
    CatalogSnapshot snap = engine.scan();

    // Verify scan finished fast without hanging on circular symlinks
    CHECK(snap.last_scan_ms >= 0.0 && snap.last_scan_ms < 5000.0);

    // Verify targets discovered
    auto find_tgt = [&](const std::string& id) -> const TargetInstance* {
        for (const auto& t : snap.targets) {
            if (t.id == id) return &t;
        }
        return nullptr;
    };
    CHECK(find_tgt("claude-main") != nullptr);
    const auto* claude_second = find_tgt("claude-second");
    CHECK(claude_second != nullptr);
    CHECK(claude_second->env_hint.has_value());
    CHECK(claude_second->env_hint->find("CLAUDE_CONFIG_DIR=") == 0);
    CHECK(find_tgt("codex") != nullptr);
    CHECK(find_tgt("antigravity") != nullptr);
    CHECK(find_tgt("opencode") != nullptr);
    CHECK(find_tgt("zcode") != nullptr);
    CHECK(find_tgt("cursor") != nullptr);
    CHECK(find_tgt("windsurf") != nullptr);

    // Verify unconfigured siblings (claude-work & codex-alt)
    bool found_claude_work = false;
    bool found_codex_alt = false;
    for (const auto& sib : snap.unconfigured_siblings) {
        if (sib.id == "claude-work") found_claude_work = true;
        if (sib.id == "codex-alt") {
            found_codex_alt = true;
            CHECK(sib.env_hint.has_value());
            CHECK(sib.env_hint->find("CODEX_HOME=") == 0);
        }
    }
    CHECK(found_claude_work);
    CHECK(found_codex_alt);

    // Verify skills discovered (including opencode.jsonc custom skill, plugin shadow, repo-local)
    // and pruned dirs excluded
    auto find_skill = [&](const std::string& slug) -> const CanonicalSkill* {
        for (const auto& s : snap.skills) {
            if (s.slug == slug) return &s;
        }
        return nullptr;
    };
    CHECK(find_skill("git-release") != nullptr);
    CHECK(find_skill("legacy-helper") != nullptr);
    CHECK(find_skill("codex-only") != nullptr);
    CHECK(find_skill("playwright-e2e") != nullptr);
    CHECK(find_skill("plugin-tool") != nullptr);
    CHECK(find_skill("plugin-tool")->has_plugin_shadow);
    CHECK(find_skill("oc-custom") != nullptr);
    CHECK(find_skill("deploy-app") != nullptr);
    CHECK(find_skill("deploy-app")->is_repo_local);
    CHECK(find_skill("should-skip-nm") == nullptr);
    CHECK(find_skill("should-skip-tcc") == nullptr);

    // Verify broken links detected (broken-link + loop-a + loop-b)
    CHECK(snap.broken_links.size() >= 3);

    // Verify exact duplicates & drifted groups
    CHECK(snap.duplicate_groups.size() == 1);
    CHECK(snap.duplicate_groups[0].slug == "git-release");
    CHECK(snap.duplicate_groups[0].redundant_bytes > 0);

    CHECK(snap.drifted_groups.size() == 1);
    CHECK(snap.drifted_groups[0].slug == "playwright-e2e");
    CHECK(snap.drifted_groups[0].variants.size() == 2);
    CHECK(!snap.drifted_groups[0].unified_diff.empty());
    CHECK(snap.drifted_groups[0].unified_diff.find("---") != std::string::npos);

    // Verify Antigravity frontmatter incompatibility on legacy-helper
    const auto* legacy = find_skill("legacy-helper");
    CHECK(!legacy->antigravity_valid);
    CHECK(legacy->target_states.at("antigravity").incompatible);

    // Fix frontmatter preview (dry run) vs apply
    auto fm_preview = engine.fix_frontmatter("legacy-helper", false);
    CHECK(fm_preview.ok);
    CHECK(fm_preview.changed);
    CHECK(!fm_preview.applied);
    CHECK(!fm_preview.diff.empty());
    CHECK(read_file(home / ".claude-second/skills/legacy-helper/SKILL.md") == no_fm_body);

    auto fm_applied = engine.fix_frontmatter("legacy-helper", true);
    CHECK(fm_applied.ok);
    CHECK(fm_applied.changed);
    CHECK(fm_applied.applied);
    std::string repaired_md = read_file(home / ".claude-second/skills/legacy-helper/SKILL.md");
    CHECK(repaired_md.find("name: legacy-helper") != std::string::npos);
    CHECK(repaired_md.find("description:") != std::string::npos);
    CHECK(repaired_md.find("Check tests") != std::string::npos);
    auto snap_after_fm = engine.scan();
    for (const auto& s : snap_after_fm.skills) {
        if (s.slug == "legacy-helper") {
            CHECK(s.antigravity_valid);
            CHECK(!s.target_states.at("antigravity").incompatible);
        }
    }

    // Custom target add (non-existent nested parent dir!) + clone from claude-main + independent toggle
    TargetInstance custom;
    custom.id = "custom-bot";
    custom.display_name = "Custom Bot";
    custom.adapter = AdapterKind::CustomLenient;
    custom.skills_dir = home / "my-bot/nested/skills";
    auto add_res = engine.add_target(custom);
    CHECK(add_res.ok);

    auto clone_res = engine.clone_target("claude-main", "custom-bot");
    CHECK(clone_res.ok);
    CHECK(is_symlink_path(home / "my-bot/nested/skills/git-release"));
    CHECK(is_symlink_path(home / "my-bot/nested/skills/playwright-e2e"));

    // Toggle OFF on managed symlink works cleanly
    auto off_res = engine.toggle_skill("git-release", "custom-bot", false);
    CHECK(off_res.ok);
    CHECK(!fs::exists(home / "my-bot/nested/skills/git-release"));

    // Safety check: toggling OFF an unmanaged physical copy MUST fail and NOT delete it!
    auto unsafe_off = engine.toggle_skill("git-release", "claude-main", false);
    CHECK(!unsafe_off.ok);
    CHECK(is_real_dir_path(home / ".claude/skills/git-release"));

    // Safety check: toggling OFF an external symlink pointing outside ~/.agents/skills MUST fail!
    write_file(home / "ext-skill/SKILL.md", "---\nname: ext-skill\ndescription: External.\n---\nExt.\n");
    CHECK(::symlink((home / "ext-skill").c_str(),
                    (home / "my-bot/nested/skills/ext-skill").c_str()) == 0);
    auto ext_off = engine.toggle_skill("ext-skill", "custom-bot", false);
    CHECK(!ext_off.ok);
    CHECK(is_symlink_path(home / "my-bot/nested/skills/ext-skill"));

    // JSON catalog contract roundtrip check
    auto snap_json = to_catalog_json(engine.get_catalog());
    std::string serialized = loadout::util::to_json(snap_json, false);
    auto reparsed = loadout::util::parse_json(serialized);
    CHECK(reparsed.has_value());
    CHECK(reparsed->contains("stats"));
    CHECK(reparsed->contains("canonicalRoot"));
    CHECK(reparsed->contains("lastScanMs"));
    CHECK(reparsed->contains("canUndo"));
    CHECK(reparsed->contains("unconfiguredSiblings"));
    CHECK(reparsed->contains("targets"));
    CHECK(reparsed->contains("skills"));
    CHECK(reparsed->contains("duplicateGroups"));
    CHECK(reparsed->contains("driftedGroups"));
    CHECK(reparsed->contains("brokenLinks"));

    // Lockfile reload check with a brand-new Engine instance
    Engine reloaded(EngineConfig{.home_dir = home});
    auto reloaded_targets = reloaded.list_targets();
    bool has_custom_bot = false;
    for (const auto& t : reloaded_targets) {
        if (t.id == "custom-bot") has_custom_bot = true;
    }
    CHECK(has_custom_bot);

    // Target remove safely unlinks managed symlinks only
    auto rem_res = reloaded.remove_target("custom-bot");
    CHECK(rem_res.ok);
    CHECK(!is_symlink_path(home / "my-bot/nested/skills/playwright-e2e"));
    // External symlink left intact!
    CHECK(is_symlink_path(home / "my-bot/nested/skills/ext-skill"));
}

void test_consolidate_quarantine_undo_and_drift() {
    TempSandbox sb;
    const fs::path home = sb.root;

    const std::string skill_a =
        "---\nname: alpha-skill\ndescription: Alpha skill content.\n---\n# Alpha\nLine 1\nLine 2\n";
    write_file(home / ".claude/skills/alpha-skill/SKILL.md", skill_a);
    write_file(home / ".claude/skills/alpha-skill/references/guide.txt", "Reference data 12345\n");
    write_file(home / ".zcode/skills/alpha-skill/SKILL.md", skill_a);
    write_file(home / ".zcode/skills/alpha-skill/references/guide.txt", "Reference data 12345\n");

    // Drifted skill
    write_file(home / ".claude/skills/beta-drift/SKILL.md",
               "---\nname: beta-drift\ndescription: Claude version.\n---\nVersion A\n");
    write_file(home / ".zcode/skills/beta-drift/SKILL.md",
               "---\nname: beta-drift\ndescription: ZCode version.\n---\nVersion B\n");

    // Repo-local skill (must stay project-local by default)
    fs::create_directories(home / "dev/proj/.git");
    write_file(home / "dev/proj/.claude/skills/repo-only/SKILL.md",
               "---\nname: repo-only\ndescription: Repo local.\n---\nLocal.\n");

    auto hash_claude_before = loadout::util::hash_skill_tree(home / ".claude/skills/alpha-skill").hex_hash;
    auto hash_zcode_before = loadout::util::hash_skill_tree(home / ".zcode/skills/alpha-skill").hex_hash;
    CHECK(!hash_claude_before.empty());
    CHECK(hash_claude_before == hash_zcode_before);

    Engine engine(EngineConfig{.home_dir = home});

    // Dry-run consolidate: computes plan without mutating disk
    auto dry = engine.consolidate(true);
    CHECK(dry.ok);
    CHECK(dry.plan.dry_run);
    CHECK(dry.plan.skills_promoted == 1);
    CHECK(dry.plan.copies_quarantined == 1);
    CHECK(is_real_dir_path(home / ".claude/skills/alpha-skill"));
    CHECK(is_real_dir_path(home / ".zcode/skills/alpha-skill"));
    CHECK(!fs::exists(home / ".agents/skills/alpha-skill"));

    // Apply consolidate
    auto applied = engine.consolidate(false);
    CHECK(applied.ok);
    CHECK(!applied.quarantine_batch_id.empty());
    CHECK(fs::exists(applied.quarantine_dir / "manifest.json"));
    CHECK(is_real_dir_path(home / ".agents/skills/alpha-skill"));
    CHECK(is_symlink_path(home / ".claude/skills/alpha-skill"));
    CHECK(is_symlink_path(home / ".zcode/skills/alpha-skill"));
    // Repo-local stayed physical in repo
    CHECK(is_real_dir_path(home / "dev/proj/.claude/skills/repo-only"));
    // Drifted skill was NOT silently overwritten
    CHECK(is_real_dir_path(home / ".claude/skills/beta-drift"));
    CHECK(is_real_dir_path(home / ".zcode/skills/beta-drift"));

    auto snap_mid = engine.get_catalog();
    CHECK(snap_mid.can_undo);
    CHECK(snap_mid.duplicate_groups.empty());
    CHECK(snap_mid.drifted_groups.size() == 1);

    // Undo restores exact pre-consolidation physical directories byte-for-byte!
    auto undo_res = engine.undo_last_quarantine();
    CHECK(undo_res.ok);
    CHECK(is_real_dir_path(home / ".claude/skills/alpha-skill"));
    CHECK(is_real_dir_path(home / ".zcode/skills/alpha-skill"));
    CHECK(!fs::exists(home / ".agents/skills/alpha-skill"));
    CHECK(loadout::util::hash_skill_tree(home / ".claude/skills/alpha-skill").hex_hash == hash_claude_before);
    CHECK(loadout::util::hash_skill_tree(home / ".zcode/skills/alpha-skill").hex_hash == hash_zcode_before);
    CHECK(!engine.get_catalog().can_undo);

    // Resolve drift (keep both -> rename ZCode copy to beta-drift-zcode, then undo)
    auto kb_res = engine.resolve_drift(
        "beta-drift", (home / ".zcode/skills/beta-drift").string(), "beta-drift-zcode");
    CHECK(kb_res.ok);
    CHECK(is_real_dir_path(home / ".agents/skills/beta-drift-zcode"));
    CHECK(is_symlink_path(home / ".zcode/skills/beta-drift-zcode"));
    CHECK(engine.undo_last_quarantine().ok);
    CHECK(is_real_dir_path(home / ".zcode/skills/beta-drift"));

    // Resolve drift (make Claude copy canonical)
    auto can_res = engine.resolve_drift("beta-drift", (home / ".claude/skills/beta-drift").string());
    CHECK(can_res.ok);
    CHECK(is_real_dir_path(home / ".agents/skills/beta-drift"));
    CHECK(is_symlink_path(home / ".claude/skills/beta-drift"));
    CHECK(is_symlink_path(home / ".zcode/skills/beta-drift"));
    CHECK(read_file(home / ".agents/skills/beta-drift/SKILL.md").find("Version A") != std::string::npos);
    CHECK(engine.get_catalog().drifted_groups.empty());

    // Skill inspector + direct save
    auto detail = engine.get_skill_detail("beta-drift");
    CHECK(detail.found);
    CHECK(detail.markdown.find("Version A") != std::string::npos);
    auto save_res = engine.save_skill_markdown(
        "beta-drift", "---\nname: beta-drift\ndescription: Updated.\n---\nVersion A2\n");
    CHECK(save_res.ok);
    CHECK(read_file(home / ".agents/skills/beta-drift/SKILL.md").find("Version A2") != std::string::npos);
}

void test_git_worktree_hydration() {
    TempSandbox sb;
    const fs::path home = sb.root;

    // Setup canonical skill wired to claude-second
    write_file(home / ".agents/skills/work-skill/SKILL.md",
               "---\nname: work-skill\ndescription: Second account work skill.\n---\nWork.\n");
    fs::create_directories(home / ".claude-second/skills");

    Engine engine(EngineConfig{.home_dir = home});
    CHECK(engine.toggle_skill("work-skill", "claude-second", true).ok);

    // Create a real git repository + linked git worktree inside /tmp sandbox
    fs::path main_repo = home / "repo-main";
    fs::path wt_dir = home / "repo-wt";
    fs::create_directories(main_repo);
    write_file(main_repo / "README.md", "# Test Repo\n");

    std::string init_cmd =
        "git -C '" + main_repo.string() + "' init -q && "
        "git -C '" + main_repo.string() + "' config user.email 'test@loadout.local' && "
        "git -C '" + main_repo.string() + "' config user.name 'Loadout Test' && "
        "git -C '" + main_repo.string() + "' add README.md && "
        "git -C '" + main_repo.string() + "' commit -q -m 'init' && "
        "git -C '" + main_repo.string() + "' worktree add -q -b feat-wt '" + wt_dir.string() + "'";
    CHECK(std::system(init_cmd.c_str()) == 0);

    // Also add an untracked project-local skill in main_repo
    write_file(main_repo / ".claude/skills/repo-local-deploy/SKILL.md",
               "---\nname: repo-local-deploy\ndescription: Main checkout untracked skill.\n---\nDeploy.\n");

    std::map<std::string, std::string> env_map = {
        {"CLAUDE_CONFIG_DIR", (home / ".claude-second").string()},
    };
    auto hres = engine.hydrate_worktree(wt_dir, env_map);
    CHECK(hres.ok);
    CHECK(hres.matched_target_id == "claude-second");
    CHECK(is_symlink_path(wt_dir / ".claude/skills/work-skill"));
    CHECK(is_symlink_path(wt_dir / ".claude/skills/repo-local-deploy"));
    CHECK(!hres.exclude_file.empty());
    CHECK(fs::exists(hres.exclude_file));

    // Verify git status --porcelain inside the linked worktree is 100% CLEAN!
    std::string status = run_capture("git -C '" + wt_dir.string() + "' status --porcelain");
    if (!status.empty()) {
        std::cerr << "Unexpected git status in worktree:\n" << status << "\n";
    }
    CHECK(status.empty());

    // Calling hydrate_worktree a second time is idempotent and keeps git status clean
    auto hres2 = engine.hydrate_worktree(wt_dir, env_map);
    CHECK(hres2.ok);
    std::string status2 = run_capture("git -C '" + wt_dir.string() + "' status --porcelain");
    CHECK(status2.empty());
}

void test_multifile_inspector_and_security() {
    TempSandbox sb;
    const fs::path home = sb.root;

    const std::string skill_md =
        "---\nname: rust-perf-audit\ndescription: Profile Rust hot paths.\n---\n# Rust Perf Audit\n";
    const std::string script_sh = "#!/usr/bin/env bash\necho flamegraph\n";
    const std::string ref_md = "# Criterion Notes\nUse black_box.\n";
    write_file(home / ".agents/skills/rust-perf-audit/SKILL.md", skill_md);
    write_file(home / ".agents/skills/rust-perf-audit/scripts/flamegraph.sh", script_sh);
    write_file(home / ".agents/skills/rust-perf-audit/references/criterion.md", ref_md);
    {
        fs::create_directories(home / ".agents/skills/rust-perf-audit/assets");
        std::ofstream bin(home / ".agents/skills/rust-perf-audit/assets/icon.bin",
                          std::ios::out | std::ios::binary | std::ios::trunc);
        const char raw_bytes[] = {'\x89', 'P', 'N', 'G', '\0', '\x01', '\x02'};
        bin.write(raw_bytes, static_cast<std::streamsize>(sizeof(raw_bytes)));
    }
    fs::create_directories(home / ".claude/skills");

    Engine engine(EngineConfig{.home_dir = home});
    auto detail = engine.get_skill_detail("rust-perf-audit");
    CHECK(detail.found);
    CHECK(detail.file_entries.size() == 4);
    CHECK(detail.file_entries[0].path == "SKILL.md");
    CHECK(detail.file_entries[0].is_text);
    CHECK(detail.file_entries[0].content == skill_md);

    bool saw_script = false;
    bool saw_ref = false;
    bool saw_bin = false;
    for (const auto& fe : detail.file_entries) {
        if (fe.path == "scripts/flamegraph.sh") {
            saw_script = true;
            CHECK(fe.is_text);
            CHECK(fe.content == script_sh);
            CHECK(fe.size == script_sh.size());
        } else if (fe.path == "references/criterion.md") {
            saw_ref = true;
            CHECK(fe.is_text);
            CHECK(fe.content == ref_md);
        } else if (fe.path == "assets/icon.bin") {
            saw_bin = true;
            CHECK(!fe.is_text);
            CHECK(fe.content.empty());
            CHECK(fe.size == 7);
        }
    }
    CHECK(saw_script);
    CHECK(saw_ref);
    CHECK(saw_bin);

    // JSON shape check: files is array of { path, size, isText, content }
    auto detail_json = to_skill_detail_json(detail);
    const auto* files_arr = detail_json.find("files");
    CHECK(files_arr != nullptr && files_arr->is_array() && files_arr->size() == 4);
    CHECK((*files_arr)[0].get_str("path") == "SKILL.md");
    CHECK((*files_arr)[0].get_bool("isText"));
    CHECK((*files_arr)[0].get_str("content") == skill_md);

    // Edit non-SKILL.md file cleanly
    const std::string updated_script = "#!/usr/bin/env bash\necho updated\n";
    auto save_ok = engine.save_skill_file("rust-perf-audit", "scripts/flamegraph.sh", updated_script);
    CHECK(save_ok.ok);
    CHECK(read_file(home / ".agents/skills/rust-perf-audit/scripts/flamegraph.sh") == updated_script);

    // Negative security tests: path traversal, absolute path, invalid slug, and symlink escape
    CHECK(!engine.save_skill_file("rust-perf-audit", "../escape.txt", "bad").ok);
    CHECK(!fs::exists(home / ".agents/skills/escape.txt"));
    CHECK(!engine.save_skill_file("rust-perf-audit", "scripts/../../escape2.txt", "bad").ok);
    CHECK(!fs::exists(home / ".agents/skills/escape2.txt"));
    CHECK(!engine.save_skill_file("rust-perf-audit", (home / "abs-escape.txt").string(), "bad").ok);
    CHECK(!fs::exists(home / "abs-escape.txt"));
    CHECK(!engine.save_skill_file("../rust-perf-audit", "SKILL.md", "bad").ok);
    CHECK(!engine.save_skill_file("rust-perf-audit", "", "bad").ok);

    // Symlink escape inside skill directory must be blocked
    fs::create_directories(home / "outside-secret");
    CHECK(::symlink((home / "outside-secret").c_str(),
                    (home / ".agents/skills/rust-perf-audit/scripts/evil-link").c_str()) == 0);
    CHECK(!engine.save_skill_file("rust-perf-audit", "scripts/evil-link/pwn.txt", "bad").ok);
    CHECK(!fs::exists(home / "outside-secret/pwn.txt"));
}

void test_profiles_lifecycle() {
    TempSandbox sb;
    const fs::path home = sb.root;

    write_file(home / ".agents/skills/alpha/SKILL.md",
               "---\nname: alpha\ndescription: Alpha skill.\n---\nAlpha.\n");
    write_file(home / ".agents/skills/beta/SKILL.md",
               "---\nname: beta\ndescription: Beta skill.\n---\nBeta.\n");
    fs::create_directories(home / ".claude/skills");
    fs::create_directories(home / ".claude-second/skills");

    // Unmanaged physical copy that profiles must NEVER disturb
    write_file(home / ".claude/skills/unmanaged-phys/SKILL.md",
               "---\nname: unmanaged-phys\ndescription: Physical copy.\n---\nKeep me.\n");

    Engine engine(EngineConfig{.home_dir = home});
    CHECK(engine.toggle_skill("alpha", "claude-main", true).ok);
    CHECK(engine.toggle_skill("alpha", "claude-second", true).ok);
    CHECK(engine.toggle_skill("beta", "claude-main", true).ok);

    // Snapshot current live wiring into "work-full"
    CHECK(engine.save_profile("work-full").ok);
    CHECK(engine.active_profile() == "work-full");

    // Switch wiring and save "minimal"
    CHECK(engine.toggle_skill("alpha", "claude-second", false).ok);
    CHECK(engine.toggle_skill("beta", "claude-main", false).ok);
    CHECK(engine.save_profile("minimal").ok);
    CHECK(engine.active_profile() == "minimal");

    // Reject invalid profile names
    CHECK(!engine.save_profile("   ").ok);
    CHECK(!engine.save_profile("bad\nname").ok);

    // Reload Engine from lockfile and verify profiles persisted
    Engine reloaded(EngineConfig{.home_dir = home});
    CHECK(reloaded.active_profile() == "minimal");
    auto profs = reloaded.get_profiles();
    CHECK(profs.size() == 2);

    // Apply "work-full" -> restores symlinks without touching unmanaged physical copy
    CHECK(reloaded.apply_profile("work-full").ok);
    CHECK(reloaded.active_profile() == "work-full");
    CHECK(is_symlink_path(home / ".claude/skills/alpha"));
    CHECK(is_symlink_path(home / ".claude-second/skills/alpha"));
    CHECK(is_symlink_path(home / ".claude/skills/beta"));
    CHECK(!fs::exists(home / ".claude-second/skills/beta"));
    CHECK(is_real_dir_path(home / ".claude/skills/unmanaged-phys"));

    // Apply "minimal" -> unwires beta in claude-main and alpha in claude-second
    CHECK(reloaded.apply_profile("minimal").ok);
    CHECK(is_symlink_path(home / ".claude/skills/alpha"));
    CHECK(!fs::exists(home / ".claude-second/skills/alpha"));
    CHECK(!fs::exists(home / ".claude/skills/beta"));
    CHECK(is_real_dir_path(home / ".claude/skills/unmanaged-phys"));

    // Flat slug-list profile
    CHECK(reloaded.save_profile("alpha-everywhere", {}, {"alpha"}).ok);
    CHECK(reloaded.apply_profile("alpha-everywhere").ok);
    CHECK(is_symlink_path(home / ".claude/skills/alpha"));
    CHECK(is_symlink_path(home / ".claude-second/skills/alpha"));
    CHECK(!fs::exists(home / ".claude/skills/beta"));

    // JSON serialization check
    auto pjson = to_profiles_json(reloaded.get_profiles(), reloaded.active_profile());
    CHECK(pjson.get_str("activeProfile") == "alpha-everywhere");
    CHECK(pjson.find("profiles") != nullptr && pjson.find("profiles")->size() == 3);

    auto cjson = to_catalog_json(reloaded.get_catalog());
    CHECK(cjson.get_str("activeProfile") == "alpha-everywhere");
    CHECK(cjson.find("profiles") != nullptr && cjson.find("profiles")->size() == 3);

    // Delete profile
    CHECK(reloaded.delete_profile("alpha-everywhere").ok);
    CHECK(reloaded.get_profiles().size() == 2);
    CHECK(!reloaded.apply_profile("nonexistent-profile").ok);
}

}  // namespace

void test_scanner_skips_linked_worktrees() {
    TempSandbox sb;
    const fs::path repo = sb.root / "repo";
    const fs::path worktree = repo / ".claude/worktrees/feature";
    const std::string skill_md = "---\nname: wt-skill\ndescription: Worktree copy.\n---\nBody.\n";

    fs::create_directories(repo / ".git/worktrees/feature");
    write_file(repo / ".claude/skills/main-skill/SKILL.md",
               "---\nname: main-skill\ndescription: Main checkout.\n---\nBody.\n");
    write_file(worktree / ".git", "gitdir: " + (repo / ".git/worktrees/feature").string() + "\n");
    write_file(worktree / ".claude/skills/wt-skill/SKILL.md", skill_md);

    // Submodules also use a .git file, but point at .git/modules and must still be scanned
    const fs::path submodule = repo / "vendor/lib";
    write_file(submodule / ".git", "gitdir: ../../.git/modules/lib\n");
    write_file(submodule / ".claude/skills/sub-skill/SKILL.md",
               "---\nname: sub-skill\ndescription: Submodule.\n---\nBody.\n");

    auto has_slug = [](const ScanResult& res, const std::string& slug) {
        for (const auto& occ : res.occurrences) {
            if (occ.slug == slug) return true;
        }
        return false;
    };

    ScanResult from_repo = scan_filesystem(ScanOptions{
        .home_dir = sb.root,
        .canonical_root = sb.root / ".agents/skills",
        .repo_scan_roots = {repo},
    });
    CHECK(has_slug(from_repo, "main-skill"));
    CHECK(has_slug(from_repo, "sub-skill"));
    CHECK(!has_slug(from_repo, "wt-skill"));
    for (const auto& discovered : from_repo.discovered_repos) {
        CHECK(discovered != worktree);
    }

    // A worktree passed explicitly as a scan root is still scanned
    ScanResult from_worktree = scan_filesystem(ScanOptions{
        .home_dir = sb.root,
        .canonical_root = sb.root / ".agents/skills",
        .repo_scan_roots = {worktree},
    });
    CHECK(has_slug(from_worktree, "wt-skill"));
}

int main() {
    test_discovery_duplicates_drift_and_safety();
    test_consolidate_quarantine_undo_and_drift();
    test_git_worktree_hydration();
    test_scanner_skips_linked_worktrees();
    test_multifile_inspector_and_security();
    test_profiles_lifecycle();
    std::cout << "PASS: all " << g_assertions << " core assertions passed.\n";
    return 0;
}
