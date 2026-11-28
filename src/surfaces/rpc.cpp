#include "surfaces/rpc.hpp"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <iostream>
#include <map>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include "core/engine.hpp"
#include "surfaces/cli.hpp"
#include "util/json.hpp"

namespace singlet::surfaces {

namespace {

using singlet::core::AdapterKind;
using singlet::core::CatalogSnapshot;
using singlet::core::Engine;
using singlet::core::TargetInstance;
using singlet::util::JsonValue;

JsonValue make_rpc_result(const JsonValue& id, JsonValue result) {
    JsonValue obj = JsonValue::object();
    obj.set("jsonrpc", "2.0");
    obj.set("id", id);
    obj.set("result", std::move(result));
    return obj;
}

JsonValue make_rpc_error(const JsonValue& id, int code, std::string message) {
    JsonValue err = JsonValue::object();
    err.set("code", code);
    err.set("message", std::move(message));

    JsonValue obj = JsonValue::object();
    obj.set("jsonrpc", "2.0");
    obj.set("id", id);
    obj.set("error", std::move(err));
    return obj;
}

std::vector<std::filesystem::path> extract_roots(const JsonValue& params, const std::filesystem::path& home_dir) {
    std::vector<std::filesystem::path> roots;
    const JsonValue* arr = params.find("roots");
    if (arr && arr->is_array()) {
        for (const auto& item : arr->as_array()) {
            if (item.is_string() && !item.as_string().empty()) {
                roots.push_back(singlet::core::expand_home_path(item.as_string(), home_dir));
            }
        }
    }
    return roots;
}

std::vector<std::string> extract_string_array(const JsonValue& params, std::string_view key) {
    std::vector<std::string> out;
    const JsonValue* arr = params.find(key);
    if (arr && arr->is_array()) {
        for (const auto& item : arr->as_array()) {
            if (item.is_string() && !item.as_string().empty()) {
                out.push_back(item.as_string());
            }
        }
    }
    return out;
}

std::map<std::string, std::string> extract_env_map(const JsonValue& params) {
    std::map<std::string, std::string> env_map;
    const JsonValue* env = params.find("env");
    if (env && env->is_object()) {
        for (const auto& [k, v] : env->as_object()) {
            if (v.is_string()) {
                env_map[k] = v.as_string();
            }
        }
    }
    return env_map;
}

TargetInstance build_target_from_params(const JsonValue& params, const std::filesystem::path& home_dir) {
    TargetInstance t;
    t.id = params.get_str("id", "");
    t.display_name = params.get_str("displayName", "");
    if (t.display_name.empty()) {
        t.display_name = params.get_str("name", t.id);
    }
    std::string adapter_str = params.get_str("adapter", "");
    if (adapter_str.empty()) {
        adapter_str = params.get_str("kind", "custom-lenient");
    }
    auto parsed_adapter = singlet::core::adapter_from_string(adapter_str);
    t.adapter = parsed_adapter.value_or(AdapterKind::CustomLenient);

    t.group = params.get_str("group", "");
    if (t.group.empty()) {
        t.group = std::string(singlet::core::adapter_default_group(t.adapter));
    }
    std::string raw_dir = params.get_str("skillsDir", "");
    t.skills_dir = singlet::core::expand_home_path(raw_dir, home_dir);

    std::string env_hint = params.get_str("envHint", "");
    if (!env_hint.empty()) {
        t.env_hint = env_hint;
    }
    t.configured = true;
    t.auto_discovered = false;
    return t;
}

JsonValue dispatch_rpc_method(Engine& engine, const JsonValue& id, std::string_view method, const JsonValue& params) {
    if (method == "catalog.get") {
        return make_rpc_result(id, singlet::core::to_catalog_json(engine.get_catalog()));
    }

    if (method == "catalog.scan") {
        auto roots = extract_roots(params, engine.home_dir());
        return make_rpc_result(id, singlet::core::to_catalog_json(engine.scan(roots)));
    }

    if (method == "skill.toggle") {
        std::string skill = params.get_str("skill", "");
        if (skill.empty()) skill = params.get_str("slug", "");
        std::string target_id = params.get_str("targetId", "");
        if (skill.empty() || target_id.empty()) {
            return make_rpc_error(id, -32602, "skill.toggle requires 'skill' and 'targetId'");
        }
        bool enabled = params.get_bool("enabled", true);
        auto res = engine.toggle_skill(skill, target_id, enabled);
        JsonValue out = singlet::core::to_operation_result_json(res);
        out.set("skill", skill);
        out.set("targetId", target_id);
        out.set("enabled", enabled);
        if (res.ok) {
            out.set("catalog", singlet::core::to_catalog_json(engine.get_catalog()));
        }
        return make_rpc_result(id, std::move(out));
    }

    if (method == "skill.consolidate") {
        bool dry_run = params.get_bool("dryRun", true);
        auto promote = extract_string_array(params, "promoteRepoSkills");
        auto res = engine.consolidate(dry_run, promote);
        JsonValue out = singlet::core::to_consolidate_result_json(res);
        if (!dry_run && res.ok) {
            out.set("catalog", singlet::core::to_catalog_json(engine.get_catalog()));
        }
        return make_rpc_result(id, std::move(out));
    }

    if (method == "skill.resolveDrift") {
        std::string slug = params.get_str("slug", "");
        if (slug.empty()) slug = params.get_str("skill", "");
        std::string chosen = params.get_str("chosenPath", "");
        std::string keep_both = params.get_str("keepBothSlug", "");
        if (slug.empty() || chosen.empty()) {
            return make_rpc_error(id, -32602, "skill.resolveDrift requires 'slug' and 'chosenPath'");
        }
        auto res = engine.resolve_drift(slug, chosen, keep_both);
        JsonValue out = singlet::core::to_operation_result_json(res);
        out.set("slug", slug);
        out.set("chosenPath", chosen);
        if (!keep_both.empty()) {
            out.set("keepBothSlug", keep_both);
        }
        if (res.ok) {
            out.set("catalog", singlet::core::to_catalog_json(engine.get_catalog()));
        }
        return make_rpc_result(id, std::move(out));
    }

    if (method == "skill.fixFrontmatter") {
        std::string skill = params.get_str("skill", "");
        if (skill.empty()) skill = params.get_str("slug", "");
        bool apply = params.get_bool("apply", false);
        if (skill.empty()) {
            return make_rpc_error(id, -32602, "skill.fixFrontmatter requires 'skill'");
        }
        auto res = engine.fix_frontmatter(skill, apply);
        JsonValue out = singlet::core::to_frontmatter_fix_json(res);
        out.set("skill", skill);
        out.set("unifiedDiff", res.diff);
        if (apply && res.ok) {
            out.set("catalog", singlet::core::to_catalog_json(engine.get_catalog()));
        }
        return make_rpc_result(id, std::move(out));
    }

    if (method == "skill.get") {
        std::string slug = params.get_str("slug", "");
        if (slug.empty()) slug = params.get_str("skill", "");
        if (slug.empty()) {
            return make_rpc_error(id, -32602, "skill.get requires 'slug'");
        }
        auto detail = engine.get_skill_detail(slug);
        if (!detail.found) {
            return make_rpc_error(id, -32004, detail.error.empty() ? "Skill not found" : detail.error);
        }
        return make_rpc_result(id, singlet::core::to_skill_detail_json(detail));
    }

    if (method == "skill.save") {
        std::string slug = params.get_str("slug", "");
        if (slug.empty()) slug = params.get_str("skill", "");
        std::string rel_path = params.get_str("relPath", "");
        if (rel_path.empty()) rel_path = params.get_str("path", "");
        std::string content;
        if (const JsonValue* cptr = params.find("content"); cptr && cptr->is_string()) {
            content = cptr->as_string();
        } else {
            content = params.get_str("markdown", "");
        }
        if (slug.empty()) {
            return make_rpc_error(id, -32602, "skill.save requires 'slug'");
        }
        auto res = rel_path.empty()
                       ? engine.save_skill_markdown(slug, content)
                       : engine.save_skill_file(slug, rel_path, content);
        JsonValue out = singlet::core::to_operation_result_json(res);
        out.set("slug", slug);
        out.set("relPath", rel_path.empty() ? std::string("SKILL.md") : rel_path);
        if (res.ok) {
            out.set("catalog", singlet::core::to_catalog_json(engine.get_catalog()));
        }
        return make_rpc_result(id, std::move(out));
    }

    if (method == "profile.list") {
        return make_rpc_result(id, singlet::core::to_profiles_json(engine.get_profiles(), engine.active_profile()));
    }

    if (method == "profile.save") {
        std::string name = params.get_str("name", "");
        if (name.empty()) {
            return make_rpc_error(id, -32602, "profile.save requires 'name'");
        }
        std::map<std::string, std::map<std::string, bool>> wiring;
        if (const JsonValue* w = params.find("wiring"); w && w->is_object()) {
            for (const auto& [slug_key, targets_val] : w->as_object()) {
                if (!targets_val.is_object()) continue;
                std::map<std::string, bool> tmap;
                for (const auto& [tid, en] : targets_val.as_object()) {
                    tmap[tid] = en.as_bool(false);
                }
                wiring[slug_key] = std::move(tmap);
            }
        }
        auto skills = extract_string_array(params, "skills");
        auto res = engine.save_profile(name, wiring, skills);
        JsonValue out = singlet::core::to_operation_result_json(res);
        out.set("name", name);
        out.set("activeProfile", engine.active_profile());
        out.set("profiles", singlet::core::to_profiles_json(engine.get_profiles(), engine.active_profile())["profiles"]);
        if (res.ok) {
            out.set("catalog", singlet::core::to_catalog_json(engine.get_catalog()));
        }
        return make_rpc_result(id, std::move(out));
    }

    if (method == "profile.apply") {
        std::string name = params.get_str("name", "");
        if (name.empty()) {
            return make_rpc_error(id, -32602, "profile.apply requires 'name'");
        }
        auto res = engine.apply_profile(name);
        JsonValue out = singlet::core::to_operation_result_json(res);
        out.set("name", name);
        out.set("activeProfile", engine.active_profile());
        out.set("profiles", singlet::core::to_profiles_json(engine.get_profiles(), engine.active_profile())["profiles"]);
        if (res.ok) {
            out.set("catalog", singlet::core::to_catalog_json(engine.get_catalog()));
        }
        return make_rpc_result(id, std::move(out));
    }

    if (method == "profile.delete") {
        std::string name = params.get_str("name", "");
        if (name.empty()) {
            return make_rpc_error(id, -32602, "profile.delete requires 'name'");
        }
        auto res = engine.delete_profile(name);
        JsonValue out = singlet::core::to_operation_result_json(res);
        out.set("name", name);
        out.set("activeProfile", engine.active_profile());
        out.set("profiles", singlet::core::to_profiles_json(engine.get_profiles(), engine.active_profile())["profiles"]);
        if (res.ok) {
            out.set("catalog", singlet::core::to_catalog_json(engine.get_catalog()));
        }
        return make_rpc_result(id, std::move(out));
    }

    if (method == "target.add") {
        TargetInstance t = build_target_from_params(params, engine.home_dir());
        if (t.id.empty() || t.skills_dir.empty()) {
            return make_rpc_error(id, -32602, "target.add requires 'id' and 'skillsDir'");
        }
        std::string clone_from = params.get_str("cloneFrom", "");
        auto res = engine.add_target(t, clone_from);
        JsonValue out = singlet::core::to_operation_result_json(res);
        out.set("target", singlet::core::to_target_json(t));
        if (res.ok) {
            out.set("catalog", singlet::core::to_catalog_json(engine.get_catalog()));
        }
        return make_rpc_result(id, std::move(out));
    }

    if (method == "target.remove") {
        std::string target_id = params.get_str("id", "");
        if (target_id.empty()) {
            return make_rpc_error(id, -32602, "target.remove requires 'id'");
        }
        auto res = engine.remove_target(target_id);
        JsonValue out = singlet::core::to_operation_result_json(res);
        out.set("id", target_id);
        if (res.ok) {
            out.set("catalog", singlet::core::to_catalog_json(engine.get_catalog()));
        }
        return make_rpc_result(id, std::move(out));
    }

    if (method == "target.clone") {
        std::string from_id = params.get_str("fromId", "");
        std::string to_id = params.get_str("toId", "");
        if (from_id.empty() || to_id.empty()) {
            return make_rpc_error(id, -32602, "target.clone requires 'fromId' and 'toId'");
        }
        auto res = engine.clone_target(from_id, to_id);
        JsonValue out = singlet::core::to_operation_result_json(res);
        out.set("fromId", from_id);
        out.set("toId", to_id);
        if (res.ok) {
            out.set("catalog", singlet::core::to_catalog_json(engine.get_catalog()));
        }
        return make_rpc_result(id, std::move(out));
    }

    if (method == "skill.install") {
        std::string url = params.get_str("url", "");
        if (url.empty()) {
            return make_rpc_error(id, -32602, "skill.install requires 'url'");
        }
        auto res = engine.install_from_github(url, extract_string_array(params, "targets"));
        JsonValue out = singlet::core::to_install_result_json(res);
        if (res.ok) {
            out.set("catalog", singlet::core::to_catalog_json(engine.get_catalog()));
        }
        return make_rpc_result(id, std::move(out));
    }

    if (method == "source.check") {
        auto statuses = engine.check_source_updates(extract_string_array(params, "skills"));
        JsonValue out = JsonValue::object();
        out.set("statuses", singlet::core::to_source_statuses_json(statuses));
        return make_rpc_result(id, std::move(out));
    }

    if (method == "source.update") {
        auto statuses = engine.update_from_sources(
            extract_string_array(params, "skills"), params.get_bool("force", false));
        JsonValue out = JsonValue::object();
        out.set("statuses", singlet::core::to_source_statuses_json(statuses));
        out.set("catalog", singlet::core::to_catalog_json(engine.get_catalog()));
        return make_rpc_result(id, std::move(out));
    }

    if (method == "quarantine.undo") {
        auto res = engine.undo_last_quarantine();
        JsonValue out = singlet::core::to_operation_result_json(res);
        if (res.ok) {
            out.set("catalog", singlet::core::to_catalog_json(engine.get_catalog()));
        }
        return make_rpc_result(id, std::move(out));
    }

    if (method == "worktree.hydrate") {
        std::string wt = params.get_str("worktreePath", "");
        if (wt.empty()) {
            return make_rpc_error(id, -32602, "worktree.hydrate requires 'worktreePath'");
        }
        std::filesystem::path wt_path = singlet::core::expand_home_path(wt, engine.home_dir());
        std::string repo_root_str = params.get_str("repoRoot", "");
        std::filesystem::path repo_root = repo_root_str.empty()
                                              ? std::filesystem::path{}
                                              : singlet::core::expand_home_path(repo_root_str, engine.home_dir());
        std::string provider_id = params.get_str("providerId", "");
        auto env_map = extract_env_map(params);

        auto res = engine.hydrate_worktree(wt_path, env_map, repo_root, provider_id);
        JsonValue out = singlet::core::to_worktree_hydrate_json(res);
        out.set("worktreePath", wt_path.string());
        if (!res.matched_target_id.empty()) {
            out.set("resolvedTargetId", res.matched_target_id);
        }
        JsonValue excl = JsonValue::array();
        for (const auto& entry : res.excluded_entries) {
            excl.push_back(entry);
        }
        out.set("excludedPaths", std::move(excl));
        return make_rpc_result(id, std::move(out));
    }

    return make_rpc_error(id, -32601, std::string("Method not found: ") + std::string(method));
}

// ---------------------------------------------------------------------------
// MCP Helpers
// ---------------------------------------------------------------------------

JsonValue make_mcp_tool(
    std::string name,
    std::string description,
    JsonValue properties,
    std::vector<std::string> required = {}) {
    JsonValue schema = JsonValue::object();
    schema.set("type", "object");
    schema.set("properties", std::move(properties));
    if (!required.empty()) {
        JsonValue req = JsonValue::array();
        for (auto& r : required) {
            req.push_back(std::move(r));
        }
        schema.set("required", std::move(req));
    }

    JsonValue tool = JsonValue::object();
    tool.set("name", std::move(name));
    tool.set("description", std::move(description));
    tool.set("inputSchema", std::move(schema));
    return tool;
}

JsonValue make_prop(std::string type, std::string description) {
    JsonValue p = JsonValue::object();
    p.set("type", std::move(type));
    p.set("description", std::move(description));
    return p;
}

JsonValue make_mcp_tools_list() {
    JsonValue tools = JsonValue::array();

    {
        JsonValue props = JsonValue::object();
        JsonValue roots_prop = make_prop("array", "Optional extra directory roots to scan");
        roots_prop.set("items", JsonValue::object({{"type", "string"}}));
        props.set("roots", std::move(roots_prop));
        tools.push_back(make_mcp_tool(
            "singlet_list_catalog",
            "List all discovered agent skills, targets (including multi-account siblings), token estimates, and symlink wiring status.",
            std::move(props)));
    }

    {
        JsonValue props = JsonValue::object();
        JsonValue roots_prop = make_prop("array", "Optional extra directory roots to scan");
        roots_prop.set("items", JsonValue::object({{"type", "string"}}));
        props.set("roots", std::move(roots_prop));
        tools.push_back(make_mcp_tool(
            "singlet_audit_duplicates",
            "Audit exact skill duplicates, drifted copies with diffs, broken symlinks, and Antigravity frontmatter issues.",
            std::move(props)));
    }

    {
        JsonValue props = JsonValue::object();
        props.set("skill", make_prop("string", "Skill slug name"));
        props.set("targetId", make_prop("string", "Target instance ID (e.g. claude-main, claude-second, codex)"));
        props.set("enabled", make_prop("boolean", "true to wire symlink, false to unlink"));
        tools.push_back(make_mcp_tool(
            "singlet_toggle_skill",
            "Enable or disable a skill symlink in a specific target instance.",
            std::move(props),
            {"skill", "targetId", "enabled"}));
    }

    {
        JsonValue props = JsonValue::object();
        props.set("id", make_prop("string", "Unique target identifier (e.g. claude-work, custom-bot)"));
        props.set("skillsDir", make_prop("string", "Path to the target's skills directory"));
        props.set("displayName", make_prop("string", "Human-readable label"));
        props.set("adapter", make_prop("string", "Adapter kind: claude-code, antigravity, codex, opencode, zcode, cursor, windsurf, custom-lenient, custom-strict"));
        props.set("envHint", make_prop("string", "Optional env var hint (e.g. CLAUDE_CONFIG_DIR=~/.claude-work)"));
        props.set("cloneFrom", make_prop("string", "Optional target ID to clone active skill symlinks from"));
        tools.push_back(make_mcp_tool(
            "singlet_add_target",
            "Register a custom target or multi-account sibling skills directory.",
            std::move(props),
            {"id", "skillsDir"}));
    }

    {
        JsonValue props = JsonValue::object();
        props.set("worktreePath", make_prop("string", "Absolute path to the Git worktree"));
        props.set("repoRoot", make_prop("string", "Optional main repository root path"));
        props.set("providerId", make_prop("string", "Optional provider hint (claude, codex, opencode)"));
        props.set("env", make_prop("object", "Optional environment variables (e.g. CLAUDE_CONFIG_DIR, CODEX_HOME)"));
        tools.push_back(make_mcp_tool(
            "singlet_hydrate_worktree",
            "Materialize workspace/project skill symlinks into a Git worktree and exclude them via .git/info/exclude.",
            std::move(props),
            {"worktreePath"}));
    }

    {
        JsonValue props = JsonValue::object();
        props.set("skill", make_prop("string", "Skill slug name"));
        props.set("apply", make_prop("boolean", "true to write repaired frontmatter to disk, false to preview only"));
        tools.push_back(make_mcp_tool(
            "singlet_fix_frontmatter",
            "Preview or apply Antigravity-compliant YAML frontmatter repair to a skill's SKILL.md.",
            std::move(props),
            {"skill"}));
    }

    {
        JsonValue props = JsonValue::object();
        props.set("url", make_prop("string", "GitHub URL: a repo, a tree/<ref>/<path> folder, or a blob/<ref>/<path>/SKILL.md link"));
        JsonValue targets_prop = make_prop("array", "Optional target IDs to turn the new skills on in");
        targets_prop.set("items", JsonValue::object({{"type", "string"}}));
        props.set("targets", std::move(targets_prop));
        tools.push_back(make_mcp_tool(
            "singlet_install_skills",
            "Install one skill or every skill at a GitHub URL into the canonical store and record its source for updates. Never replaces an existing skill.",
            std::move(props),
            {"url"}));
    }

    {
        JsonValue props = JsonValue::object();
        JsonValue skills_prop = make_prop("array", "Optional skill slugs; all skills installed from GitHub when omitted");
        skills_prop.set("items", JsonValue::object({{"type", "string"}}));
        props.set("skills", std::move(skills_prop));
        tools.push_back(make_mcp_tool(
            "singlet_check_skill_updates",
            "Compare skills installed from GitHub with the latest commit of their source. Read-only.",
            std::move(props)));
    }

    {
        JsonValue props = JsonValue::object();
        JsonValue skills_prop = make_prop("array", "Optional skill slugs; all skills installed from GitHub when omitted");
        skills_prop.set("items", JsonValue::object({{"type", "string"}}));
        props.set("skills", std::move(skills_prop));
        props.set("force", make_prop("boolean", "true to also replace skills with local edits (old files go to quarantine)"));
        tools.push_back(make_mcp_tool(
            "singlet_update_skills",
            "Update skills from their GitHub source. Replaced folders go to quarantine, so undo restores them.",
            std::move(props)));
    }

    JsonValue result = JsonValue::object();
    result.set("tools", std::move(tools));
    return result;
}

JsonValue make_mcp_tool_response(const JsonValue& payload, bool is_error = false) {
    JsonValue content_item = JsonValue::object();
    content_item.set("type", "text");
    content_item.set("text", singlet::util::to_json(payload, false));

    JsonValue content = JsonValue::array();
    content.push_back(std::move(content_item));

    JsonValue result = JsonValue::object();
    result.set("content", std::move(content));
    result.set("isError", is_error);
    return result;
}

JsonValue handle_mcp_tool_call(Engine& engine, const JsonValue& id, const JsonValue& params) {
    std::string tool_name = params.get_str("name", "");
    const JsonValue* args_ptr = params.find("arguments");
    JsonValue args = (args_ptr && args_ptr->is_object()) ? *args_ptr : JsonValue::object();

    if (tool_name == "singlet_list_catalog") {
        auto roots = extract_roots(args, engine.home_dir());
        CatalogSnapshot snap = roots.empty() ? engine.get_catalog() : engine.scan(roots);
        return make_rpc_result(id, make_mcp_tool_response(singlet::core::to_catalog_json(snap), false));
    }

    if (tool_name == "singlet_audit_duplicates") {
        auto roots = extract_roots(args, engine.home_dir());
        CatalogSnapshot snap = roots.empty() ? engine.scan() : engine.scan(roots);
        JsonValue full = singlet::core::to_catalog_json(snap);
        JsonValue audit = JsonValue::object();
        audit.set("stats", full["stats"]);
        audit.set("duplicateGroups", full["duplicateGroups"]);
        audit.set("driftedGroups", full["driftedGroups"]);
        audit.set("brokenLinks", full["brokenLinks"]);
        audit.set("unconfiguredSiblings", full["unconfiguredSiblings"]);

        JsonValue fm_issues = JsonValue::array();
        for (const auto& s : snap.skills) {
            if (!s.antigravity_valid || !s.frontmatter_issues.empty()) {
                JsonValue item = JsonValue::object();
                item.set("slug", s.slug);
                item.set("canonicalPath", s.canonical_path.string());
                JsonValue iss = JsonValue::array();
                for (const auto& msg : s.frontmatter_issues) {
                    iss.push_back(msg);
                }
                item.set("issues", std::move(iss));
                fm_issues.push_back(std::move(item));
            }
        }
        audit.set("frontmatterIssues", std::move(fm_issues));
        return make_rpc_result(id, make_mcp_tool_response(audit, false));
    }

    if (tool_name == "singlet_toggle_skill") {
        std::string skill = args.get_str("skill", "");
        if (skill.empty()) skill = args.get_str("slug", "");
        std::string target_id = args.get_str("targetId", "");
        bool enabled = args.get_bool("enabled", true);
        if (skill.empty() || target_id.empty()) {
            return make_rpc_result(
                id,
                make_mcp_tool_response(JsonValue::object({{"ok", false}, {"error", "Missing 'skill' or 'targetId'"}}), true));
        }
        auto res = engine.toggle_skill(skill, target_id, enabled);
        return make_rpc_result(id, make_mcp_tool_response(singlet::core::to_operation_result_json(res), !res.ok));
    }

    if (tool_name == "singlet_add_target") {
        TargetInstance t = build_target_from_params(args, engine.home_dir());
        if (t.id.empty() || t.skills_dir.empty()) {
            return make_rpc_result(
                id,
                make_mcp_tool_response(JsonValue::object({{"ok", false}, {"error", "Missing 'id' or 'skillsDir'"}}), true));
        }
        std::string clone_from = args.get_str("cloneFrom", "");
        auto res = engine.add_target(t, clone_from);
        JsonValue out = singlet::core::to_operation_result_json(res);
        out.set("target", singlet::core::to_target_json(t));
        return make_rpc_result(id, make_mcp_tool_response(out, !res.ok));
    }

    if (tool_name == "singlet_hydrate_worktree") {
        std::string wt = args.get_str("worktreePath", "");
        if (wt.empty()) {
            return make_rpc_result(
                id,
                make_mcp_tool_response(JsonValue::object({{"ok", false}, {"error", "Missing 'worktreePath'"}}), true));
        }
        std::filesystem::path wt_path = singlet::core::expand_home_path(wt, engine.home_dir());
        std::string repo_str = args.get_str("repoRoot", "");
        std::filesystem::path repo_root = repo_str.empty()
                                              ? std::filesystem::path{}
                                              : singlet::core::expand_home_path(repo_str, engine.home_dir());
        std::string provider_id = args.get_str("providerId", "");
        auto env_map = extract_env_map(args);
        auto res = engine.hydrate_worktree(wt_path, env_map, repo_root, provider_id);
        return make_rpc_result(id, make_mcp_tool_response(singlet::core::to_worktree_hydrate_json(res), !res.ok));
    }

    if (tool_name == "singlet_fix_frontmatter") {
        std::string skill = args.get_str("skill", "");
        if (skill.empty()) skill = args.get_str("slug", "");
        bool apply = args.get_bool("apply", false);
        if (skill.empty()) {
            return make_rpc_result(
                id,
                make_mcp_tool_response(JsonValue::object({{"ok", false}, {"error", "Missing 'skill'"}}), true));
        }
        auto res = engine.fix_frontmatter(skill, apply);
        return make_rpc_result(id, make_mcp_tool_response(singlet::core::to_frontmatter_fix_json(res), !res.ok));
    }

    if (tool_name == "singlet_install_skills") {
        std::string url = args.get_str("url", "");
        if (url.empty()) {
            return make_rpc_result(
                id,
                make_mcp_tool_response(JsonValue::object({{"ok", false}, {"error", "Missing 'url'"}}), true));
        }
        auto res = engine.install_from_github(url, extract_string_array(args, "targets"));
        return make_rpc_result(id, make_mcp_tool_response(singlet::core::to_install_result_json(res), !res.ok));
    }

    if (tool_name == "singlet_check_skill_updates" || tool_name == "singlet_update_skills") {
        auto slugs = extract_string_array(args, "skills");
        auto statuses = tool_name == "singlet_update_skills"
                            ? engine.update_from_sources(slugs, args.get_bool("force", false))
                            : engine.check_source_updates(slugs);
        bool failed = std::any_of(statuses.begin(), statuses.end(), [](const auto& st) {
            return st.state == singlet::core::SourceState::Failed;
        });
        JsonValue out = JsonValue::object();
        out.set("statuses", singlet::core::to_source_statuses_json(statuses));
        return make_rpc_result(id, make_mcp_tool_response(out, failed));
    }

    return make_rpc_error(id, -32602, "Unknown MCP tool: " + tool_name);
}

bool starts_with_icase(std::string_view s, std::string_view prefix) {
    if (s.size() < prefix.size()) return false;
    for (size_t i = 0; i < prefix.size(); ++i) {
        if (std::tolower(static_cast<unsigned char>(s[i])) != std::tolower(static_cast<unsigned char>(prefix[i]))) {
            return false;
        }
    }
    return true;
}

std::string trim_copy(std::string_view sv) {
    size_t b = 0;
    while (b < sv.size() && std::isspace(static_cast<unsigned char>(sv[b]))) ++b;
    size_t e = sv.size();
    while (e > b && std::isspace(static_cast<unsigned char>(sv[e - 1]))) --e;
    return std::string(sv.substr(b, e - b));
}

// Read one MCP message from stdin, auto-detecting Content-Length framing vs newline-delimited JSON.
bool read_mcp_frame(std::istream& in, std::string& out_body, bool& out_was_framed) {
    out_body.clear();
    out_was_framed = false;

    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        std::string trimmed = trim_copy(line);
        if (trimmed.empty()) {
            continue;
        }

        if (trimmed.front() == '{' || trimmed.front() == '[') {
            out_body = std::move(trimmed);
            out_was_framed = false;
            return true;
        }

        // Content-Length framed mode
        size_t content_length = 0;
        if (starts_with_icase(trimmed, "content-length:")) {
            std::string num_str = trim_copy(std::string_view(trimmed).substr(15));
            try {
                content_length = static_cast<size_t>(std::stoull(num_str));
            } catch (...) {
                content_length = 0;
            }
        }

        // Consume remaining headers until empty line
        while (std::getline(in, line)) {
            if (!line.empty() && line.back() == '\r') {
                line.pop_back();
            }
            if (line.empty()) {
                break;
            }
            if (starts_with_icase(line, "content-length:")) {
                std::string num_str = trim_copy(std::string_view(line).substr(15));
                try {
                    content_length = static_cast<size_t>(std::stoull(num_str));
                } catch (...) {
                    content_length = 0;
                }
            }
        }

        if (content_length == 0) {
            continue;
        }
        out_body.resize(content_length);
        in.read(out_body.data(), static_cast<std::streamsize>(content_length));
        if (in.gcount() != static_cast<std::streamsize>(content_length)) {
            return false;
        }
        out_was_framed = true;
        return true;
    }

    return false;
}

void write_mcp_frame(std::ostream& out, const JsonValue& msg, bool framed) {
    std::string body = singlet::util::to_json(msg, false);
    if (framed) {
        out << "Content-Length: " << body.size() << "\r\n\r\n" << body << std::flush;
    } else {
        out << body << '\n' << std::flush;
    }
}

} // namespace

int run_rpc_server(Engine& engine) {
    std::string line;
    while (std::getline(std::cin, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        std::string trimmed = trim_copy(line);
        if (trimmed.empty()) {
            continue;
        }

        auto parsed = singlet::util::parse_json_ex(trimmed);
        if (!parsed) {
            JsonValue err = make_rpc_error(JsonValue::make_null(), -32700, "Parse error: " + parsed.error);
            std::cout << singlet::util::to_json(err, false) << '\n' << std::flush;
            continue;
        }

        const JsonValue& req = *parsed.value;
        if (!req.is_object()) {
            JsonValue err = make_rpc_error(JsonValue::make_null(), -32600, "Invalid Request: expected object");
            std::cout << singlet::util::to_json(err, false) << '\n' << std::flush;
            continue;
        }

        const JsonValue* id_ptr = req.find("id");
        bool is_notification = (id_ptr == nullptr);
        JsonValue id = id_ptr ? *id_ptr : JsonValue::make_null();

        std::string method = req.get_str("method", "");
        if (method.empty()) {
            if (!is_notification) {
                JsonValue err = make_rpc_error(id, -32600, "Invalid Request: missing method");
                std::cout << singlet::util::to_json(err, false) << '\n' << std::flush;
            }
            continue;
        }

        const JsonValue* params_ptr = req.find("params");
        JsonValue params = (params_ptr && params_ptr->is_object()) ? *params_ptr : JsonValue::object();

        JsonValue resp = dispatch_rpc_method(engine, id, method, params);
        if (!is_notification) {
            std::cout << singlet::util::to_json(resp, false) << '\n' << std::flush;
        }
    }
    return 0;
}

int run_mcp_server(Engine& engine) {
    std::string body;
    bool framed = false;
    while (read_mcp_frame(std::cin, body, framed)) {
        auto parsed = singlet::util::parse_json_ex(body);
        if (!parsed) {
            JsonValue err = make_rpc_error(JsonValue::make_null(), -32700, "Parse error: " + parsed.error);
            write_mcp_frame(std::cout, err, framed);
            continue;
        }

        const JsonValue& req = *parsed.value;
        if (!req.is_object()) {
            JsonValue err = make_rpc_error(JsonValue::make_null(), -32600, "Invalid Request");
            write_mcp_frame(std::cout, err, framed);
            continue;
        }

        const JsonValue* id_ptr = req.find("id");
        bool is_notification = (id_ptr == nullptr);
        JsonValue id = id_ptr ? *id_ptr : JsonValue::make_null();

        std::string method = req.get_str("method", "");
        const JsonValue* params_ptr = req.find("params");
        JsonValue params = (params_ptr && params_ptr->is_object()) ? *params_ptr : JsonValue::object();

        if (method == "initialize") {
            std::string proto = params.get_str("protocolVersion", "2024-11-05");
            if (proto.empty()) proto = "2024-11-05";

            JsonValue caps = JsonValue::object();
            caps.set("tools", JsonValue::object({{"listChanged", false}}));
            caps.set("resources", JsonValue::object({{"subscribe", false}, {"listChanged", false}}));

            JsonValue server_info = JsonValue::object();
            server_info.set("name", "singlet");
            server_info.set("version", SINGLET_VERSION);

            JsonValue res = JsonValue::object();
            res.set("protocolVersion", std::move(proto));
            res.set("capabilities", std::move(caps));
            res.set("serverInfo", std::move(server_info));

            if (!is_notification) {
                write_mcp_frame(std::cout, make_rpc_result(id, std::move(res)), framed);
            }
            continue;
        }

        if (method == "notifications/initialized" || method == "initialized") {
            if (!is_notification) {
                write_mcp_frame(std::cout, make_rpc_result(id, JsonValue::object()), framed);
            }
            continue;
        }

        if (method == "ping") {
            if (!is_notification) {
                write_mcp_frame(std::cout, make_rpc_result(id, JsonValue::object()), framed);
            }
            continue;
        }

        if (method == "tools/list") {
            if (!is_notification) {
                write_mcp_frame(std::cout, make_rpc_result(id, make_mcp_tools_list()), framed);
            }
            continue;
        }

        if (method == "tools/call") {
            JsonValue resp = handle_mcp_tool_call(engine, id, params);
            if (!is_notification) {
                write_mcp_frame(std::cout, resp, framed);
            }
            continue;
        }

        if (method == "resources/list") {
            CatalogSnapshot snap = engine.get_catalog();
            JsonValue resources = JsonValue::array();
            for (const auto& s : snap.skills) {
                JsonValue r = JsonValue::object();
                r.set("uri", "skill://" + s.slug + "/SKILL.md");
                r.set("name", s.name.empty() ? s.slug : s.name);
                r.set("description", s.description);
                r.set("mimeType", "text/markdown");
                resources.push_back(std::move(r));
            }
            JsonValue res = JsonValue::object();
            res.set("resources", std::move(resources));
            if (!is_notification) {
                write_mcp_frame(std::cout, make_rpc_result(id, std::move(res)), framed);
            }
            continue;
        }

        if (method == "resources/read") {
            std::string uri = params.get_str("uri", "");
            std::string slug = uri;
            constexpr std::string_view kPrefix = "skill://";
            if (slug.rfind(kPrefix, 0) == 0) {
                slug = slug.substr(kPrefix.size());
            }
            auto slash = slug.find('/');
            if (slash != std::string::npos) {
                slug = slug.substr(0, slash);
            }
            auto detail = engine.get_skill_detail(slug);
            if (!detail.found) {
                if (!is_notification) {
                    write_mcp_frame(std::cout, make_rpc_error(id, -32004, "Resource not found: " + uri), framed);
                }
                continue;
            }
            JsonValue content = JsonValue::object();
            content.set("uri", uri);
            content.set("mimeType", "text/markdown");
            content.set("text", detail.markdown);

            JsonValue contents = JsonValue::array();
            contents.push_back(std::move(content));

            JsonValue res = JsonValue::object();
            res.set("contents", std::move(contents));
            if (!is_notification) {
                write_mcp_frame(std::cout, make_rpc_result(id, std::move(res)), framed);
            }
            continue;
        }

        if (!is_notification) {
            write_mcp_frame(std::cout, make_rpc_error(id, -32601, "Method not found: " + method), framed);
        }
    }
    return 0;
}

} // namespace singlet::surfaces
