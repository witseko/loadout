#include "surfaces/cli.hpp"

#include <unistd.h>

#include <algorithm>
#include <condition_variable>
#include <cstdlib>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <sstream>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include "core/engine.hpp"
#include "surfaces/http.hpp"
#include "surfaces/rpc.hpp"
#include "surfaces/tui.hpp"
#include "surfaces/webview.hpp"
#include "ui/embedded_ui.hpp"
#include "util/json.hpp"

namespace singlet::surfaces {

namespace {

bool use_ansi_color() {
    if (const char* no_color = std::getenv("NO_COLOR"); no_color && no_color[0] != '\0') {
        return false;
    }
    return ::isatty(STDOUT_FILENO) != 0;
}

const char* c_reset(bool ansi) { return ansi ? "\x1b[0m" : ""; }
const char* c_bold(bool ansi) { return ansi ? "\x1b[1m" : ""; }
const char* c_dim(bool ansi) { return ansi ? "\x1b[2m" : ""; }
const char* c_cyan(bool ansi) { return ansi ? "\x1b[36m" : ""; }
const char* c_green(bool ansi) { return ansi ? "\x1b[32m" : ""; }
const char* c_amber(bool ansi) { return ansi ? "\x1b[33m" : ""; }
const char* c_red(bool ansi) { return ansi ? "\x1b[31m" : ""; }

std::vector<std::string> split_csv(std::string_view input) {
    std::vector<std::string> out;
    size_t start = 0;
    while (start <= input.size()) {
        size_t comma = input.find(',', start);
        std::string_view part = (comma == std::string_view::npos)
                                    ? input.substr(start)
                                    : input.substr(start, comma - start);
        while (!part.empty() && (part.front() == ' ' || part.front() == '\t')) part.remove_prefix(1);
        while (!part.empty() && (part.back() == ' ' || part.back() == '\t')) part.remove_suffix(1);
        if (!part.empty()) {
            out.emplace_back(part);
        }
        if (comma == std::string_view::npos) break;
        start = comma + 1;
    }
    return out;
}

std::string format_bytes(uint64_t bytes) {
    std::ostringstream oss;
    if (bytes >= 1024 * 1024) {
        oss << std::fixed << std::setprecision(1) << (static_cast<double>(bytes) / (1024.0 * 1024.0)) << " MB";
    } else if (bytes >= 1024) {
        oss << std::fixed << std::setprecision(1) << (static_cast<double>(bytes) / 1024.0) << " KB";
    } else {
        oss << bytes << " B";
    }
    return oss.str();
}

void print_usage() {
    std::cout
        << "singlet " << SINGLET_VERSION << " - zero-dependency local agent skills manager & switchboard\n\n"
        << "USAGE:\n"
        << "  singlet [--home <dir>] <command> [options]\n\n"
        << "COMMANDS:\n"
        << "  scan [--roots dir1,dir2] [--json]       Discover skills, accounts, duplicates & issues\n"
        << "  ui [--port 4747] [--browser] [--no-open] Launch native desktop Patchbay UI (or browser)\n"
        << "  tui                                     Launch interactive terminal Patchbay Matrix\n"
        << "  dedup [--apply] [--promote-repo a,b]    Preview consolidation into <home>/.agents/skills;\n"
        << "                                          --apply moves copies to quarantine and links\n"
        << "  undo                                    Restore last quarantine batch byte-for-byte\n"
        << "  toggle <skill> [--on t1,t2] [--off t3]  Toggle skill symlinks across targets\n"
        << "  target list [--json]                    List auto-discovered & custom targets\n"
        << "  target add <id> <dir> [--kind <k>]      Register a target or second account\n"
        << "             [--name <label>] [--env <e>] [--clone-from <id>]\n"
        << "  target clone <from-id> <to-id>          Clone active skill wiring between targets\n"
        << "  target remove <id>                      Unlink managed symlinks & remove target\n"
        << "  doctor [--fix]                          Audit broken links, frontmatter & sibling accounts\n"
        << "  rpc                                     Stdio JSON-RPC 2.0 daemon (T3 Code backend)\n"
        << "  mcp                                     Stdio Model Context Protocol (MCP) server\n\n"
        << "GLOBAL OPTIONS:\n"
        << "  --home <path>                           Override home directory (also SINGLET_HOME env var)\n"
        << "  --dump-ui-js                            Print embedded Web Component JS + CSS bundle\n"
        << "  --version, -v                           Print version and exit\n"
        << "  --help, -h                              Show this help message\n";
}

int cmd_scan(singlet::core::Engine& engine, const std::vector<std::string>& args) {
    bool as_json = false;
    std::vector<std::filesystem::path> roots;

    for (size_t i = 0; i < args.size(); ++i) {
        const auto& a = args[i];
        if (a == "--json") {
            as_json = true;
        } else if (a == "--roots" && i + 1 < args.size()) {
            for (const auto& r : split_csv(args[++i])) {
                roots.push_back(singlet::core::expand_home_path(r, engine.home_dir()));
            }
        } else if (a.rfind("--roots=", 0) == 0) {
            for (const auto& r : split_csv(std::string_view(a).substr(8))) {
                roots.push_back(singlet::core::expand_home_path(r, engine.home_dir()));
            }
        }
    }

    auto snap = engine.scan(roots);

    if (as_json) {
        std::cout << singlet::util::to_json(singlet::core::to_catalog_json(snap), true) << '\n';
        return 0;
    }

    bool ansi = use_ansi_color();
    std::cout << c_bold(ansi) << c_cyan(ansi) << "singlet scan" << c_reset(ansi)
              << "  " << c_dim(ansi) << "home=" << engine.home_dir().string()
              << "  scanned in " << std::fixed << std::setprecision(1) << snap.last_scan_ms << " ms"
              << c_reset(ansi) << "\n\n";

    // Summary metrics
    std::cout << c_bold(ansi) << "SUMMARY" << c_reset(ansi) << "\n"
              << "  Canonical Root:     " << snap.canonical_root.string() << "\n"
              << "  Unique Skills:      " << c_bold(ansi) << snap.stats.total_skills << c_reset(ansi)
              << " (" << snap.stats.canonical_skills << " canonical, "
              << snap.stats.project_local_skills << " project-local)\n"
              << "  Targets Discovered: " << c_bold(ansi) << snap.stats.total_targets << c_reset(ansi);
    if (!snap.unconfigured_siblings.empty()) {
        std::cout << "  " << c_amber(ansi) << "(+" << snap.unconfigured_siblings.size()
                  << " unconfigured sibling account(s))" << c_reset(ansi);
    }
    std::cout << "\n";

    std::cout << "  Exact Duplicates:   "
              << (snap.stats.exact_duplicates > 0 ? c_amber(ansi) : c_green(ansi))
              << snap.stats.exact_duplicates << c_reset(ansi)
              << " (" << format_bytes(snap.stats.duplicate_bytes) << " redundant)\n"
              << "  Drifted Copies:     "
              << (snap.stats.drifted_groups > 0 ? c_amber(ansi) : c_green(ansi))
              << snap.stats.drifted_groups << c_reset(ansi) << "\n"
              << "  Broken Symlinks:    "
              << (snap.stats.broken_links > 0 ? c_red(ansi) : c_green(ansi))
              << snap.stats.broken_links << c_reset(ansi) << "\n"
              << "  Frontmatter Issues: "
              << (snap.stats.frontmatter_issues > 0 ? c_red(ansi) : c_green(ansi))
              << snap.stats.frontmatter_issues << c_reset(ansi) << "\n\n";

    // Targets table
    std::cout << c_bold(ansi) << "TARGETS" << c_reset(ansi) << "\n";
    std::cout << "  " << std::left
              << std::setw(18) << "ID"
              << std::setw(20) << "NAME"
              << std::setw(15) << "ADAPTER"
              << std::setw(8) << "ACTIVE"
              << std::setw(10) << "TOKENS"
              << "SKILLS DIR\n";
    std::cout << "  " << std::string(90, '-') << "\n";

    for (const auto& t : snap.targets) {
        size_t active = 0;
        size_t toks = 0;
        for (const auto& s : snap.skills) {
            auto it = s.target_states.find(t.id);
            if (it != s.target_states.end() && it->second.enabled) {
                active++;
                toks += s.approx_tokens;
            }
        }
        std::cout << "  " << std::left
                  << std::setw(18) << t.id
                  << std::setw(20) << t.display_name
                  << std::setw(15) << singlet::core::adapter_to_string(t.adapter)
                  << std::setw(8) << active
                  << std::setw(10) << ("~" + std::to_string(toks))
                  << singlet::core::contract_home_path(t.skills_dir, engine.home_dir())
                  << "\n";
    }

    if (!snap.unconfigured_siblings.empty()) {
        std::cout << "\n" << c_amber(ansi) << c_bold(ansi) << "DETECTED UNCONFIGURED SIBLING ACCOUNTS" << c_reset(ansi) << "\n";
        for (const auto& sib : snap.unconfigured_siblings) {
            std::cout << "  • " << sib.id << " (" << sib.display_name << ") -> "
                      << singlet::core::contract_home_path(sib.skills_dir, engine.home_dir());
            if (sib.env_hint && !sib.env_hint->empty()) {
                std::cout << "  [" << *sib.env_hint << "]";
            }
            std::cout << "\n";
        }
    }

    if (!snap.drifted_groups.empty()) {
        std::cout << "\n" << c_amber(ansi) << c_bold(ansi) << "DRIFTED SKILLS" << c_reset(ansi) << "\n";
        for (const auto& d : snap.drifted_groups) {
            std::cout << "  ⇄ " << d.slug << " (" << d.variants.size() << " diverging copies)\n";
            for (const auto& v : d.variants) {
                std::cout << "      - " << v.occurrence.target_id_or_repo << ": "
                          << v.occurrence.path.string() << "\n";
            }
        }
    }

    if (!snap.broken_links.empty()) {
        std::cout << "\n" << c_red(ansi) << c_bold(ansi) << "BROKEN SYMLINKS" << c_reset(ansi) << "\n";
        for (const auto& b : snap.broken_links) {
            std::cout << "  ✗ " << b.path.string() << " -> " << b.symlink_target << "\n";
        }
    }

    if (snap.stats.frontmatter_issues > 0) {
        std::cout << "\n" << c_red(ansi) << c_bold(ansi) << "ANTIGRAVITY FRONTMATTER ISSUES" << c_reset(ansi) << "\n";
        for (const auto& s : snap.skills) {
            if (!s.antigravity_valid || !s.frontmatter_issues.empty()) {
                std::cout << "  ! " << s.slug << ": ";
                for (size_t idx = 0; idx < s.frontmatter_issues.size(); ++idx) {
                    if (idx > 0) std::cout << "; ";
                    std::cout << s.frontmatter_issues[idx];
                }
                std::cout << "\n";
            }
        }
        std::cout << c_dim(ansi) << "  Tip: run `singlet doctor --fix` to repair frontmatter non-destructively.\n" << c_reset(ansi);
    }

    return 0;
}

int cmd_ui(singlet::core::Engine& engine, const std::vector<std::string>& args) {
    HttpServerOptions opts;
    bool browser_mode = false;
    bool no_open = false;
    bool explicit_port = false;

    for (size_t i = 0; i < args.size(); ++i) {
        const auto& a = args[i];
        if (a == "--no-open") {
            no_open = true;
        } else if (a == "--browser") {
            browser_mode = true;
        } else if (a == "--port" && i + 1 < args.size()) {
            try {
                int p = std::stoi(args[++i]);
                if (p < 0 || p > 65535) {
                    std::cerr << "singlet ui: invalid port: " << p << '\n';
                    return 1;
                }
                opts.port = static_cast<uint16_t>(p);
                explicit_port = true;
            } catch (...) {
                std::cerr << "singlet ui: invalid port argument\n";
                return 1;
            }
        } else if (a.rfind("--port=", 0) == 0) {
            try {
                int p = std::stoi(a.substr(7));
                if (p < 0 || p > 65535) {
                    std::cerr << "singlet ui: invalid port: " << p << '\n';
                    return 1;
                }
                opts.port = static_cast<uint16_t>(p);
                explicit_port = true;
            } catch (...) {
                std::cerr << "singlet ui: invalid port argument\n";
                return 1;
            }
        }
    }

    if (no_open || browser_mode) {
        opts.open_browser = !no_open;
        return run_http_server(engine, opts);
    }

    // Native desktop window mode (default): run localhost HTTP+SSE server on a background thread
    // and host the OS native webview (macOS WKWebView / Linux WebKitGTK) on the main thread.
    opts.open_browser = false;
    opts.fallback_ephemeral = !explicit_port;

    std::mutex mu;
    std::condition_variable cv;
    bool ready = false;
    bool server_exited = false;
    int server_rc = 0;
    std::string ready_url;

    opts.on_ready = [&](const std::string& url) {
        {
            std::lock_guard<std::mutex> lock(mu);
            ready_url = url;
            ready = true;
        }
        cv.notify_all();
    };
    opts.on_stop = []() {
        request_native_window_close();
    };

    std::thread server_thread([&]() {
        int rc = run_http_server(engine, opts);
        {
            std::lock_guard<std::mutex> lock(mu);
            server_rc = rc;
            server_exited = true;
        }
        cv.notify_all();
    });

    {
        std::unique_lock<std::mutex> lock(mu);
        cv.wait(lock, [&]() { return ready || server_exited; });
        if (!ready) {
            lock.unlock();
            server_thread.join();
            return server_rc != 0 ? server_rc : 1;
        }
    }

    NativeWindowOptions win_opts;
    win_opts.title = "singlet";
    win_opts.url = ready_url;

    NativeWindowResult win_res = run_native_window(win_opts);
    if (win_res == NativeWindowResult::Unavailable) {
        std::cerr << "singlet ui: native window unavailable, opening in browser...\n";
        launch_system_browser(ready_url);
        server_thread.join();
        return server_rc;
    }

    request_http_server_stop();
    server_thread.join();
    return (win_res == NativeWindowResult::Failed) ? 1 : server_rc;
}

int cmd_dedup(singlet::core::Engine& engine, const std::vector<std::string>& args) {
    bool dry_run = true;
    bool as_json = false;
    std::vector<std::string> promote_repo_paths;

    for (size_t i = 0; i < args.size(); ++i) {
        const auto& a = args[i];
        if (a == "--dry-run") {
            dry_run = true;
        } else if (a == "--apply") {
            dry_run = false;
        } else if (a == "--json") {
            as_json = true;
        } else if (a == "--promote-repo" && i + 1 < args.size()) {
            for (const auto& p : split_csv(args[++i])) {
                promote_repo_paths.push_back(p);
            }
        }
    }

    auto res = engine.consolidate(dry_run, promote_repo_paths);
    if (as_json) {
        std::cout << singlet::util::to_json(singlet::core::to_consolidate_result_json(res), true) << '\n';
        return res.ok ? 0 : 1;
    }

    if (!res.ok) {
        std::cerr << "singlet dedup error: " << res.error << '\n';
        return 1;
    }

    bool ansi = use_ansi_color();
    std::cout << c_bold(ansi) << (dry_run ? "CONSOLIDATION DRY-RUN PLAN" : "CONSOLIDATION COMPLETE")
              << c_reset(ansi) << "\n"
              << "  Skills Promoted:    " << res.plan.skills_promoted << "\n"
              << "  Copies Quarantined: " << res.plan.copies_quarantined << "\n"
              << "  Symlinks Created:   " << res.plan.symlinks_created << "\n"
              << "  Bytes Deduplicated: " << format_bytes(res.plan.bytes_deduplicated) << "\n";

    if (!res.plan.actions.empty()) {
        std::cout << "\nACTIONS:\n";
        for (const auto& act : res.plan.actions) {
            std::cout << "  [" << act.kind << "] " << act.slug << ": "
                      << act.source_path.string() << " -> " << act.dest_path.string();
            if (!act.quarantine_path.empty()) {
                std::cout << " (quarantine: " << act.quarantine_path.string() << ")";
            }
            std::cout << "\n";
        }
    } else {
        std::cout << "  Nothing to consolidate - skills are already canonical.\n";
    }

    if (dry_run && !res.plan.actions.empty()) {
        std::cout << "\n  Dry run: nothing changed. Run `singlet dedup --apply` to apply this plan.\n";
    }
    if (!dry_run && !res.quarantine_dir.empty()) {
        std::cout << "\n  Quarantine backup: " << res.quarantine_dir.string()
                  << "\n  Run `singlet undo` to restore previous physical folders byte-for-byte.\n";
    }
    return 0;
}

int cmd_undo(singlet::core::Engine& engine) {
    auto res = engine.undo_last_quarantine();
    if (!res.ok) {
        std::cerr << "singlet undo: " << res.error << '\n';
        return 1;
    }
    std::cout << (res.message.empty() ? "Quarantine batch restored successfully." : res.message) << '\n';
    return 0;
}

int cmd_toggle(singlet::core::Engine& engine, const std::vector<std::string>& args) {
    if (args.empty()) {
        std::cerr << "Usage: singlet toggle <skill> [--on t1,t2] [--off t3,t4]\n";
        return 1;
    }

    std::string skill;
    std::vector<std::string> on_targets;
    std::vector<std::string> off_targets;

    for (size_t i = 0; i < args.size(); ++i) {
        const auto& a = args[i];
        if (a == "--on" && i + 1 < args.size()) {
            for (const auto& t : split_csv(args[++i])) on_targets.push_back(t);
        } else if (a.rfind("--on=", 0) == 0) {
            for (const auto& t : split_csv(std::string_view(a).substr(5))) on_targets.push_back(t);
        } else if (a == "--off" && i + 1 < args.size()) {
            for (const auto& t : split_csv(args[++i])) off_targets.push_back(t);
        } else if (a.rfind("--off=", 0) == 0) {
            for (const auto& t : split_csv(std::string_view(a).substr(6))) off_targets.push_back(t);
        } else if (!a.empty() && a[0] != '-' && skill.empty()) {
            skill = a;
        }
    }

    if (skill.empty() || (on_targets.empty() && off_targets.empty())) {
        std::cerr << "Usage: singlet toggle <skill> [--on t1,t2] [--off t3,t4]\n";
        return 1;
    }

    int exit_code = 0;
    for (const auto& tid : on_targets) {
        auto res = engine.toggle_skill(skill, tid, true);
        if (res.ok) {
            std::cout << "[ON]  " << skill << " -> " << tid
                      << (res.message.empty() ? "" : (" (" + res.message + ")")) << '\n';
        } else {
            std::cerr << "[ERR] " << skill << " -> " << tid << ": " << res.error << '\n';
            exit_code = 1;
        }
    }
    for (const auto& tid : off_targets) {
        auto res = engine.toggle_skill(skill, tid, false);
        if (res.ok) {
            std::cout << "[OFF] " << skill << " -> " << tid
                      << (res.message.empty() ? "" : (" (" + res.message + ")")) << '\n';
        } else {
            std::cerr << "[ERR] " << skill << " -> " << tid << ": " << res.error << '\n';
            exit_code = 1;
        }
    }
    return exit_code;
}

int cmd_target(singlet::core::Engine& engine, const std::vector<std::string>& args) {
    if (args.empty()) {
        std::cerr << "Usage: singlet target list | add <id> <skills-dir> [--kind <k>] [--name <n>] [--env <e>] | clone <from> <to> | remove <id>\n";
        return 1;
    }

    const std::string& sub = args[0];

    if (sub == "list") {
        bool as_json = false;
        for (size_t i = 1; i < args.size(); ++i) {
            if (args[i] == "--json") as_json = true;
        }
        auto targets = engine.list_targets();
        if (as_json) {
            singlet::util::JsonValue arr = singlet::util::JsonValue::array();
            for (const auto& t : targets) {
                arr.push_back(singlet::core::to_target_json(t));
            }
            std::cout << singlet::util::to_json(arr, true) << '\n';
            return 0;
        }
        std::cout << std::left
                  << std::setw(18) << "ID"
                  << std::setw(22) << "NAME"
                  << std::setw(16) << "ADAPTER"
                  << std::setw(12) << "STATUS"
                  << "SKILLS DIR\n";
        std::cout << std::string(90, '-') << '\n';
        for (const auto& t : targets) {
            std::cout << std::left
                      << std::setw(18) << t.id
                      << std::setw(22) << t.display_name
                      << std::setw(16) << singlet::core::adapter_to_string(t.adapter)
                      << std::setw(12) << (t.configured ? "configured" : "sibling")
                      << singlet::core::contract_home_path(t.skills_dir, engine.home_dir())
                      << '\n';
        }
        return 0;
    }

    if (sub == "add") {
        if (args.size() < 3) {
            std::cerr << "Usage: singlet target add <id> <skills-dir> [--kind <adapter>] [--name <label>] [--env <hint>] [--clone-from <id>]\n";
            return 1;
        }
        singlet::core::TargetInstance t;
        t.id = args[1];
        t.skills_dir = singlet::core::expand_home_path(args[2], engine.home_dir());
        t.display_name = t.id;
        t.adapter = singlet::core::AdapterKind::CustomLenient;
        t.configured = true;
        t.auto_discovered = false;
        std::string clone_from;

        for (size_t i = 3; i < args.size(); ++i) {
            const auto& a = args[i];
            if ((a == "--kind" || a == "--adapter") && i + 1 < args.size()) {
                auto parsed = singlet::core::adapter_from_string(args[++i]);
                if (!parsed) {
                    std::cerr << "Unknown adapter kind: " << args[i] << '\n';
                    return 1;
                }
                t.adapter = *parsed;
            } else if (a == "--name" && i + 1 < args.size()) {
                t.display_name = args[++i];
            } else if (a == "--env" && i + 1 < args.size()) {
                t.env_hint = args[++i];
            } else if (a == "--group" && i + 1 < args.size()) {
                t.group = args[++i];
            } else if (a == "--clone-from" && i + 1 < args.size()) {
                clone_from = args[++i];
            }
        }
        if (t.group.empty()) {
            t.group = std::string(singlet::core::adapter_default_group(t.adapter));
        }

        auto res = engine.add_target(t, clone_from);
        if (!res.ok) {
            std::cerr << "singlet target add failed: " << res.error << '\n';
            return 1;
        }
        std::cout << "Added target '" << t.id << "' (" << t.skills_dir.string() << ")";
        if (!res.message.empty()) std::cout << " - " << res.message;
        std::cout << '\n';
        return 0;
    }

    if (sub == "clone") {
        if (args.size() < 3) {
            std::cerr << "Usage: singlet target clone <from-id> <to-id>\n";
            return 1;
        }
        auto res = engine.clone_target(args[1], args[2]);
        if (!res.ok) {
            std::cerr << "singlet target clone failed: " << res.error << '\n';
            return 1;
        }
        std::cout << (res.message.empty() ? ("Cloned wiring from " + args[1] + " to " + args[2]) : res.message) << '\n';
        return 0;
    }

    if (sub == "remove" || sub == "rm") {
        if (args.size() < 2) {
            std::cerr << "Usage: singlet target remove <id>\n";
            return 1;
        }
        auto res = engine.remove_target(args[1]);
        if (!res.ok) {
            std::cerr << "singlet target remove failed: " << res.error << '\n';
            return 1;
        }
        std::cout << (res.message.empty() ? ("Removed target " + args[1]) : res.message) << '\n';
        return 0;
    }

    std::cerr << "Unknown target subcommand: " << sub << '\n';
    return 1;
}

int cmd_doctor(singlet::core::Engine& engine, const std::vector<std::string>& args) {
    bool do_fix = false;
    for (const auto& a : args) {
        if (a == "--fix") do_fix = true;
    }

    auto snap = engine.scan();
    bool ansi = use_ansi_color();
    int issues_found = 0;

    std::cout << c_bold(ansi) << "singlet doctor" << c_reset(ansi)
              << "  (" << engine.home_dir().string() << ")\n\n";

    // 1. Dangling symlinks
    if (snap.broken_links.empty()) {
        std::cout << c_green(ansi) << "✓" << c_reset(ansi) << " No dangling symlinks found.\n";
    } else {
        issues_found += static_cast<int>(snap.broken_links.size());
        std::cout << c_red(ansi) << "✗ " << snap.broken_links.size() << " dangling symlink(s):" << c_reset(ansi) << "\n";
        for (const auto& b : snap.broken_links) {
            std::cout << "    " << b.path.string() << " -> " << b.symlink_target << "\n";
        }
    }

    // 2. Frontmatter compliance
    if (do_fix) {
        auto fixes = engine.fix_all_frontmatter(true);
        size_t fixed_count = 0;
        for (const auto& f : fixes) {
            if (f.ok && f.applied && f.changed) {
                fixed_count++;
                std::cout << c_green(ansi) << "✓ Fixed frontmatter: " << c_reset(ansi)
                          << f.slug << " (" << f.file_path.string() << ")\n";
            } else if (!f.ok) {
                issues_found++;
                std::cout << c_red(ansi) << "✗ Failed to fix " << f.slug << ": " << f.error << c_reset(ansi) << "\n";
            }
        }
        if (fixed_count == 0) {
            std::cout << c_green(ansi) << "✓" << c_reset(ansi) << " All skill frontmatter is Antigravity-compliant.\n";
        }
    } else {
        size_t fm_count = 0;
        for (const auto& s : snap.skills) {
            if (!s.antigravity_valid || !s.frontmatter_issues.empty()) {
                fm_count++;
                issues_found++;
                std::cout << c_amber(ansi) << "! Frontmatter issue in '" << s.slug << "': " << c_reset(ansi);
                for (size_t i = 0; i < s.frontmatter_issues.size(); ++i) {
                    if (i > 0) std::cout << "; ";
                    std::cout << s.frontmatter_issues[i];
                }
                std::cout << "\n";
            }
        }
        if (fm_count == 0) {
            std::cout << c_green(ansi) << "✓" << c_reset(ansi) << " All skill frontmatter is Antigravity-compliant.\n";
        } else {
            std::cout << "  Run `singlet doctor --fix` to repair automatically.\n";
        }
    }

    // 3. Unconfigured sibling accounts
    if (snap.unconfigured_siblings.empty()) {
        std::cout << c_green(ansi) << "✓" << c_reset(ansi) << " No unconfigured sibling accounts.\n";
    } else {
        std::cout << c_amber(ansi) << "! " << snap.unconfigured_siblings.size()
                  << " unconfigured sibling account(s) detected:" << c_reset(ansi) << "\n";
        for (const auto& sib : snap.unconfigured_siblings) {
            std::cout << "    " << sib.id << " -> " << sib.skills_dir.string();
            if (sib.env_hint && !sib.env_hint->empty()) {
                std::cout << "  (" << *sib.env_hint << ")";
            }
            std::cout << "\n";
        }
    }

    if (issues_found == 0) {
        std::cout << "\n" << c_green(ansi) << "Doctor check passed with 0 issues." << c_reset(ansi) << "\n";
    } else {
        std::cout << "\n" << c_amber(ansi) << "Doctor found " << issues_found << " issue(s)." << c_reset(ansi) << "\n";
    }

    return 0;
}

} // namespace

std::filesystem::path resolve_home_dir(const std::optional<std::filesystem::path>& home_override) {
    if (home_override && !home_override->empty()) {
        return singlet::core::resolve_home_dir(*home_override);
    }
    return singlet::core::resolve_home_dir({});
}

CliOptions parse_cli_args(int argc, char** argv) {
    CliOptions opts;
    for (int i = 1; i < argc; ++i) {
        std::string arg(argv[i]);
        if (arg == "--home" && i + 1 < argc) {
            opts.home_override = std::filesystem::path(argv[++i]);
        } else if (arg.rfind("--home=", 0) == 0) {
            opts.home_override = std::filesystem::path(arg.substr(7));
        } else if (opts.command.empty()) {
            opts.command = std::move(arg);
        } else {
            opts.args.push_back(std::move(arg));
        }
    }
    return opts;
}

int run_cli(int argc, char** argv) {
    CliOptions opts = parse_cli_args(argc, argv);

    // Ultra-fast paths (zero Engine initialization, < 1 ms cold start)
    if (opts.command.empty() || opts.command == "--help" || opts.command == "-h" || opts.command == "help") {
        print_usage();
        return 0;
    }

    if (opts.command == "--version" || opts.command == "-v" || opts.command == "version") {
        std::cout << "singlet " << SINGLET_VERSION << '\n';
        return 0;
    }

    if (opts.command == "--dump-ui-js") {
        std::cout << "/* === singlet embedded app.css === */\n"
                  << "export const SINGLET_APP_CSS = "
                  << singlet::util::to_json(singlet::util::JsonValue(std::string(singlet::ui::APP_CSS)), false) << ";\n\n"
                  << "/* === singlet embedded switchboard.js === */\n"
                  << singlet::ui::SWITCHBOARD_JS << '\n';
        return 0;
    }

    // All remaining commands construct Engine bound strictly to resolved home_dir
    singlet::core::EngineConfig cfg;
    cfg.home_dir = resolve_home_dir(opts.home_override);
    singlet::core::Engine engine(std::move(cfg));

    if (opts.command == "scan") {
        return cmd_scan(engine, opts.args);
    }
    if (opts.command == "ui") {
        return cmd_ui(engine, opts.args);
    }
    if (opts.command == "tui") {
        return run_tui(engine);
    }
    if (opts.command == "dedup") {
        return cmd_dedup(engine, opts.args);
    }
    if (opts.command == "undo") {
        return cmd_undo(engine);
    }
    if (opts.command == "toggle") {
        return cmd_toggle(engine, opts.args);
    }
    if (opts.command == "target") {
        return cmd_target(engine, opts.args);
    }
    if (opts.command == "doctor") {
        return cmd_doctor(engine, opts.args);
    }
    if (opts.command == "rpc") {
        return run_rpc_server(engine);
    }
    if (opts.command == "mcp") {
        return run_mcp_server(engine);
    }

    std::cerr << "Unknown command: " << opts.command << "\nRun `singlet --help` for usage.\n";
    return 1;
}

} // namespace singlet::surfaces
