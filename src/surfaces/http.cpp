#include "surfaces/http.hpp"

#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <poll.h>
#include <signal.h>
#include <sys/socket.h>
#include <unistd.h>

#include <algorithm>
#include <atomic>
#include <cctype>
#include <cerrno>
#include <chrono>
#include <csignal>
#include <cstring>
#include <iostream>
#include <map>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include "core/engine.hpp"
#include "ui/embedded_ui.hpp"
#include "util/json.hpp"

namespace loadout::surfaces {

namespace {

using loadout::core::AdapterKind;
using loadout::core::Engine;
using loadout::core::TargetInstance;
using loadout::util::JsonValue;

std::atomic<bool> g_http_stop{false};

void handle_http_signal(int) {
    g_http_stop.store(true, std::memory_order_relaxed);
}

bool set_nonblocking(int fd) {
    int flags = ::fcntl(fd, F_GETFL, 0);
    if (flags < 0) return false;
    return ::fcntl(fd, F_SETFL, flags | O_NONBLOCK) == 0;
}

bool send_all_nb(int fd, const char* data, size_t len, int timeout_ms = 5000) {
    size_t sent = 0;
    while (sent < len) {
        ssize_t n = ::send(fd, data + sent, len - sent, 0);
        if (n > 0) {
            sent += static_cast<size_t>(n);
            continue;
        }
        if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)) {
            pollfd pfd{};
            pfd.fd = fd;
            pfd.events = POLLOUT;
            int rc = ::poll(&pfd, 1, timeout_ms);
            if (rc <= 0 || (pfd.revents & (POLLERR | POLLHUP | POLLNVAL))) {
                return false;
            }
            continue;
        }
        return false;
    }
    return true;
}

void launch_browser(const std::string& url) {
    pid_t pid = ::fork();
    if (pid == 0) {
        // Child process: detach stdio to /dev/null and exec browser opener
        int devnull = ::open("/dev/null", O_RDWR);
        if (devnull >= 0) {
            ::dup2(devnull, STDIN_FILENO);
            ::dup2(devnull, STDOUT_FILENO);
            ::dup2(devnull, STDERR_FILENO);
            if (devnull > 2) ::close(devnull);
        }
#ifdef __APPLE__
        ::execlp("open", "open", url.c_str(), static_cast<char*>(nullptr));
#else
        ::execlp("xdg-open", "xdg-open", url.c_str(), static_cast<char*>(nullptr));
#endif
        ::_exit(127);
    }
}

std::string to_lower_ascii(std::string_view sv) {
    std::string out(sv);
    for (char& c : out) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return out;
}

std::string trim_ws(std::string_view sv) {
    size_t b = 0;
    while (b < sv.size() && (sv[b] == ' ' || sv[b] == '\t' || sv[b] == '\r' || sv[b] == '\n')) ++b;
    size_t e = sv.size();
    while (e > b && (sv[e - 1] == ' ' || sv[e - 1] == '\t' || sv[e - 1] == '\r' || sv[e - 1] == '\n')) --e;
    return std::string(sv.substr(b, e - b));
}

int hex_val(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return 10 + (c - 'a');
    if (c >= 'A' && c <= 'F') return 10 + (c - 'A');
    return -1;
}

std::string url_decode(std::string_view sv) {
    std::string out;
    out.reserve(sv.size());
    for (size_t i = 0; i < sv.size(); ++i) {
        if (sv[i] == '+') {
            out.push_back(' ');
        } else if (sv[i] == '%' && i + 2 < sv.size()) {
            int hi = hex_val(sv[i + 1]);
            int lo = hex_val(sv[i + 2]);
            if (hi >= 0 && lo >= 0) {
                out.push_back(static_cast<char>((hi << 4) | lo));
                i += 2;
            } else {
                out.push_back(sv[i]);
            }
        } else {
            out.push_back(sv[i]);
        }
    }
    return out;
}

std::map<std::string, std::string> parse_query_string(std::string_view qs) {
    std::map<std::string, std::string> params;
    size_t pos = 0;
    while (pos < qs.size()) {
        size_t amp = qs.find('&', pos);
        std::string_view part = (amp == std::string_view::npos) ? qs.substr(pos) : qs.substr(pos, amp - pos);
        if (!part.empty()) {
            size_t eq = part.find('=');
            if (eq == std::string_view::npos) {
                params[url_decode(part)] = "";
            } else {
                params[url_decode(part.substr(0, eq))] = url_decode(part.substr(eq + 1));
            }
        }
        if (amp == std::string_view::npos) break;
        pos = amp + 1;
    }
    return params;
}

struct HttpRequest {
    std::string method;
    std::string path;
    std::map<std::string, std::string> query;
    std::map<std::string, std::string> headers;
    std::string body;
};

struct HttpResponse {
    int status = 200;
    std::string status_text = "OK";
    std::string content_type = "application/json; charset=utf-8";
    std::string body;
};

std::string serialize_http_response(const HttpResponse& res) {
    std::ostringstream oss;
    oss << "HTTP/1.1 " << res.status << ' ' << res.status_text << "\r\n"
        << "Content-Type: " << res.content_type << "\r\n"
        << "Content-Length: " << res.body.size() << "\r\n"
        << "Cache-Control: no-cache\r\n"
        << "Connection: close\r\n\r\n"
        << res.body;
    return oss.str();
}

HttpResponse json_response(const JsonValue& val, int status = 200, const char* status_text = "OK") {
    return HttpResponse{
        status,
        status_text,
        "application/json; charset=utf-8",
        loadout::util::to_json(val, false),
    };
}

HttpResponse error_response(int status, const char* status_text, std::string message) {
    JsonValue obj = JsonValue::object();
    obj.set("ok", false);
    obj.set("error", std::move(message));
    return json_response(obj, status, status_text);
}

/// Returns a 403 response when a request did not come from the loadout UI's own origin.
///
/// Blocks other web pages (cross-site requests) and DNS rebinding from driving the API.
/// Requiring a JSON Content-Type on POST forces a CORS preflight, which browsers fail
/// because the server sends no Access-Control-Allow-Origin header.
std::optional<HttpResponse> reject_untrusted_request(const HttpRequest& req, uint16_t port) {
    const std::string port_suffix = ":" + std::to_string(port);
    auto host = req.headers.find("host");
    if (host == req.headers.end() ||
        (host->second != "127.0.0.1" + port_suffix && host->second != "localhost" + port_suffix)) {
        return error_response(403, "Forbidden", "Forbidden host");
    }
    auto origin = req.headers.find("origin");
    if (origin != req.headers.end() && origin->second != "http://" + host->second) {
        return error_response(403, "Forbidden", "Forbidden origin");
    }
    if (req.method == "POST") {
        auto content_type = req.headers.find("content-type");
        if (content_type == req.headers.end() ||
            !to_lower_ascii(content_type->second).starts_with("application/json")) {
            return error_response(403, "Forbidden", "POST requires Content-Type: application/json");
        }
    }
    return std::nullopt;
}

struct ClientConn {
    int fd = -1;
    std::string recv_buf;
    bool headers_parsed = false;
    size_t header_end = 0;
    size_t content_length = 0;
};

bool parse_request_headers(ClientConn& conn, HttpRequest& out_req) {
    size_t sep = conn.recv_buf.find("\r\n\r\n");
    size_t sep_len = 4;
    if (sep == std::string::npos) {
        sep = conn.recv_buf.find("\n\n");
        sep_len = 2;
        if (sep == std::string::npos) return false;
    }

    conn.headers_parsed = true;
    conn.header_end = sep + sep_len;
    conn.content_length = 0;

    std::string_view header_block(conn.recv_buf.data(), sep);
    size_t line_start = 0;
    bool first_line = true;
    while (line_start <= header_block.size()) {
        size_t line_end = header_block.find('\n', line_start);
        std::string_view line = (line_end == std::string_view::npos)
                                    ? header_block.substr(line_start)
                                    : header_block.substr(line_start, line_end - line_start);
        if (!line.empty() && line.back() == '\r') line.remove_suffix(1);

        if (first_line) {
            first_line = false;
            size_t sp1 = line.find(' ');
            if (sp1 != std::string_view::npos) {
                out_req.method = std::string(line.substr(0, sp1));
                size_t sp2 = line.find(' ', sp1 + 1);
                std::string_view target = (sp2 == std::string_view::npos)
                                              ? line.substr(sp1 + 1)
                                              : line.substr(sp1 + 1, sp2 - sp1 - 1);
                size_t qpos = target.find('?');
                if (qpos == std::string_view::npos) {
                    out_req.path = std::string(target);
                } else {
                    out_req.path = std::string(target.substr(0, qpos));
                    out_req.query = parse_query_string(target.substr(qpos + 1));
                }
            }
        } else if (!line.empty()) {
            size_t colon = line.find(':');
            if (colon != std::string_view::npos) {
                std::string key = to_lower_ascii(trim_ws(line.substr(0, colon)));
                std::string val = trim_ws(line.substr(colon + 1));
                if (key == "content-length") {
                    try {
                        conn.content_length = static_cast<size_t>(std::stoull(val));
                    } catch (...) {
                        conn.content_length = 0;
                    }
                }
                out_req.headers[std::move(key)] = std::move(val);
            }
        }

        if (line_end == std::string_view::npos) break;
        line_start = line_end + 1;
    }

    return true;
}

void broadcast_sse_catalog(std::vector<int>& sse_fds, Engine& engine) {
    if (sse_fds.empty()) return;
    std::string payload = "event: catalog\ndata: " +
                          loadout::util::to_json(loadout::core::to_catalog_json(engine.get_catalog()), false) +
                          "\n\n";
    auto it = sse_fds.begin();
    while (it != sse_fds.end()) {
        if (!send_all_nb(*it, payload.data(), payload.size(), 1000)) {
            ::close(*it);
            it = sse_fds.erase(it);
        } else {
            ++it;
        }
    }
}

void send_sse_heartbeat(std::vector<int>& sse_fds) {
    if (sse_fds.empty()) return;
    static constexpr std::string_view kPing = ": ping\n\n";
    auto it = sse_fds.begin();
    while (it != sse_fds.end()) {
        if (!send_all_nb(*it, kPing.data(), kPing.size(), 500)) {
            ::close(*it);
            it = sse_fds.erase(it);
        } else {
            ++it;
        }
    }
}

HttpResponse handle_api_request(
    Engine& engine,
    const HttpRequest& req,
    bool& out_mutated) {
    out_mutated = false;

    // Embedded UI static assets
    if (req.method == "GET" && (req.path == "/" || req.path == "/index.html")) {
        return HttpResponse{200, "OK", "text/html; charset=utf-8", std::string(loadout::ui::INDEX_HTML)};
    }
    if (req.method == "GET" && req.path == "/app.css") {
        return HttpResponse{200, "OK", "text/css; charset=utf-8", std::string(loadout::ui::APP_CSS)};
    }
    if (req.method == "GET" && req.path == "/switchboard.js") {
        return HttpResponse{200, "OK", "application/javascript; charset=utf-8", std::string(loadout::ui::SWITCHBOARD_JS)};
    }

    // GET /api/catalog
    if (req.method == "GET" && req.path == "/api/catalog") {
        return json_response(loadout::core::to_catalog_json(engine.get_catalog()));
    }

    // GET /api/profiles
    if (req.method == "GET" && req.path == "/api/profiles") {
        return json_response(loadout::core::to_profiles_json(engine.get_profiles(), engine.active_profile()));
    }

    // GET /api/skill?slug=...
    if (req.method == "GET" && req.path == "/api/skill") {
        std::string slug;
        if (auto it = req.query.find("slug"); it != req.query.end()) {
            slug = it->second;
        } else if (auto it2 = req.query.find("skill"); it2 != req.query.end()) {
            slug = it2->second;
        }
        if (slug.empty()) {
            return error_response(400, "Bad Request", "Missing 'slug' query parameter");
        }
        auto detail = engine.get_skill_detail(slug);
        if (!detail.found) {
            return error_response(404, "Not Found", detail.error.empty() ? "Skill not found" : detail.error);
        }
        return json_response(loadout::core::to_skill_detail_json(detail));
    }

    // Parse JSON body for POST endpoints
    auto parse_body = [&]() -> std::optional<JsonValue> {
        if (trim_ws(req.body).empty()) {
            return JsonValue::object();
        }
        return loadout::util::parse_json(req.body);
    };

    if (req.method == "POST" && req.path == "/api/scan") {
        auto body_opt = parse_body();
        if (!body_opt || !body_opt->is_object()) {
            return error_response(400, "Bad Request", "Invalid JSON body");
        }
        std::vector<std::filesystem::path> roots;
        if (const JsonValue* arr = body_opt->find("roots"); arr && arr->is_array()) {
            for (const auto& item : arr->as_array()) {
                if (item.is_string() && !item.as_string().empty()) {
                    roots.push_back(loadout::core::expand_home_path(item.as_string(), engine.home_dir()));
                }
            }
        }
        auto snap = engine.scan(roots);
        out_mutated = true;
        return json_response(loadout::core::to_catalog_json(snap));
    }

    if (req.method == "POST" && req.path == "/api/toggle") {
        auto body_opt = parse_body();
        if (!body_opt || !body_opt->is_object()) {
            return error_response(400, "Bad Request", "Invalid JSON body");
        }
        const JsonValue& body = *body_opt;
        std::string skill = body.get_str("skill", "");
        if (skill.empty()) skill = body.get_str("slug", "");
        std::string target_id = body.get_str("targetId", "");
        bool enabled = body.get_bool("enabled", true);
        if (skill.empty() || target_id.empty()) {
            return error_response(400, "Bad Request", "Missing 'skill' or 'targetId'");
        }
        auto res = engine.toggle_skill(skill, target_id, enabled);
        JsonValue out = loadout::core::to_operation_result_json(res);
        out.set("skill", skill);
        out.set("targetId", target_id);
        out.set("enabled", enabled);
        if (res.ok) {
            out_mutated = true;
            out.set("catalog", loadout::core::to_catalog_json(engine.get_catalog()));
        }
        return json_response(out, res.ok ? 200 : 400, res.ok ? "OK" : "Bad Request");
    }

    if (req.method == "POST" && req.path == "/api/consolidate") {
        auto body_opt = parse_body();
        if (!body_opt || !body_opt->is_object()) {
            return error_response(400, "Bad Request", "Invalid JSON body");
        }
        const JsonValue& body = *body_opt;
        bool dry_run = body.get_bool("dryRun", true);
        std::vector<std::string> promote;
        if (const JsonValue* arr = body.find("promoteRepoSkills"); arr && arr->is_array()) {
            for (const auto& item : arr->as_array()) {
                if (item.is_string() && !item.as_string().empty()) {
                    promote.push_back(item.as_string());
                }
            }
        }
        auto res = engine.consolidate(dry_run, promote);
        JsonValue out = loadout::core::to_consolidate_result_json(res);
        if (!dry_run && res.ok) {
            out_mutated = true;
            out.set("catalog", loadout::core::to_catalog_json(engine.get_catalog()));
        }
        return json_response(out, res.ok ? 200 : 400, res.ok ? "OK" : "Bad Request");
    }

    if (req.method == "POST" && req.path == "/api/resolve-drift") {
        auto body_opt = parse_body();
        if (!body_opt || !body_opt->is_object()) {
            return error_response(400, "Bad Request", "Invalid JSON body");
        }
        const JsonValue& body = *body_opt;
        std::string slug = body.get_str("slug", "");
        if (slug.empty()) slug = body.get_str("skill", "");
        std::string chosen_path = body.get_str("chosenPath", "");
        std::string keep_both = body.get_str("keepBothSlug", "");
        if (slug.empty() || chosen_path.empty()) {
            return error_response(400, "Bad Request", "Missing 'slug' or 'chosenPath'");
        }
        auto res = engine.resolve_drift(slug, chosen_path, keep_both);
        JsonValue out = loadout::core::to_operation_result_json(res);
        out.set("slug", slug);
        out.set("chosenPath", chosen_path);
        if (res.ok) {
            out_mutated = true;
            out.set("catalog", loadout::core::to_catalog_json(engine.get_catalog()));
        }
        return json_response(out, res.ok ? 200 : 400, res.ok ? "OK" : "Bad Request");
    }

    if (req.method == "POST" && req.path == "/api/targets") {
        auto body_opt = parse_body();
        if (!body_opt || !body_opt->is_object()) {
            return error_response(400, "Bad Request", "Invalid JSON body");
        }
        const JsonValue& body = *body_opt;
        std::string action = body.get_str("action", "add");

        if (action == "add") {
            TargetInstance t;
            t.id = body.get_str("id", "");
            t.display_name = body.get_str("displayName", "");
            if (t.display_name.empty()) t.display_name = body.get_str("name", t.id);
            std::string adapter_str = body.get_str("adapter", body.get_str("kind", "custom-lenient"));
            t.adapter = loadout::core::adapter_from_string(adapter_str).value_or(AdapterKind::CustomLenient);
            t.group = body.get_str("group", "");
            if (t.group.empty()) t.group = std::string(loadout::core::adapter_default_group(t.adapter));
            t.skills_dir = loadout::core::expand_home_path(body.get_str("skillsDir", ""), engine.home_dir());
            std::string env_hint = body.get_str("envHint", "");
            if (!env_hint.empty()) t.env_hint = env_hint;
            t.configured = true;
            t.auto_discovered = false;

            if (t.id.empty() || t.skills_dir.empty()) {
                return error_response(400, "Bad Request", "target add requires 'id' and 'skillsDir'");
            }
            std::string clone_from = body.get_str("cloneFrom", "");
            if (clone_from.empty()) clone_from = body.get_str("fromId", "");

            auto res = engine.add_target(t, clone_from);
            JsonValue out = loadout::core::to_operation_result_json(res);
            out.set("target", loadout::core::to_target_json(t));
            if (res.ok) {
                out_mutated = true;
                out.set("catalog", loadout::core::to_catalog_json(engine.get_catalog()));
            }
            return json_response(out, res.ok ? 200 : 400, res.ok ? "OK" : "Bad Request");
        }

        if (action == "remove") {
            std::string id = body.get_str("id", "");
            if (id.empty()) {
                return error_response(400, "Bad Request", "target remove requires 'id'");
            }
            auto res = engine.remove_target(id);
            JsonValue out = loadout::core::to_operation_result_json(res);
            out.set("id", id);
            if (res.ok) {
                out_mutated = true;
                out.set("catalog", loadout::core::to_catalog_json(engine.get_catalog()));
            }
            return json_response(out, res.ok ? 200 : 400, res.ok ? "OK" : "Bad Request");
        }

        if (action == "clone") {
            std::string from_id = body.get_str("fromId", "");
            std::string to_id = body.get_str("toId", "");
            if (to_id.empty()) to_id = body.get_str("id", "");
            if (from_id.empty() || to_id.empty()) {
                return error_response(400, "Bad Request", "target clone requires 'fromId' and 'id'/'toId'");
            }
            auto res = engine.clone_target(from_id, to_id);
            JsonValue out = loadout::core::to_operation_result_json(res);
            out.set("fromId", from_id);
            out.set("toId", to_id);
            if (res.ok) {
                out_mutated = true;
                out.set("catalog", loadout::core::to_catalog_json(engine.get_catalog()));
            }
            return json_response(out, res.ok ? 200 : 400, res.ok ? "OK" : "Bad Request");
        }

        return error_response(400, "Bad Request", "Unknown target action: " + action);
    }

    if (req.method == "POST" && req.path == "/api/fix-frontmatter") {
        auto body_opt = parse_body();
        if (!body_opt || !body_opt->is_object()) {
            return error_response(400, "Bad Request", "Invalid JSON body");
        }
        const JsonValue& body = *body_opt;
        std::string skill = body.get_str("skill", "");
        if (skill.empty()) skill = body.get_str("slug", "");
        bool apply = body.get_bool("apply", false);
        if (skill.empty()) {
            return error_response(400, "Bad Request", "Missing 'skill'");
        }
        auto res = engine.fix_frontmatter(skill, apply);
        JsonValue out = loadout::core::to_frontmatter_fix_json(res);
        out.set("skill", skill);
        out.set("unifiedDiff", res.diff);
        if (apply && res.ok) {
            out_mutated = true;
            out.set("catalog", loadout::core::to_catalog_json(engine.get_catalog()));
        }
        return json_response(out, res.ok ? 200 : 400, res.ok ? "OK" : "Bad Request");
    }

    if (req.method == "POST" && req.path == "/api/skill") {
        auto body_opt = parse_body();
        if (!body_opt || !body_opt->is_object()) {
            return error_response(400, "Bad Request", "Invalid JSON body");
        }
        const JsonValue& body = *body_opt;
        std::string slug = body.get_str("slug", "");
        if (slug.empty()) slug = body.get_str("skill", "");
        std::string rel_path = body.get_str("relPath", "");
        if (rel_path.empty()) rel_path = body.get_str("path", "");
        std::string content;
        if (const JsonValue* cptr = body.find("content"); cptr && cptr->is_string()) {
            content = cptr->as_string();
        } else {
            content = body.get_str("markdown", "");
        }
        if (slug.empty()) {
            return error_response(400, "Bad Request", "Missing 'slug'");
        }
        auto res = rel_path.empty()
                       ? engine.save_skill_markdown(slug, content)
                       : engine.save_skill_file(slug, rel_path, content);
        JsonValue out = loadout::core::to_operation_result_json(res);
        out.set("slug", slug);
        out.set("relPath", rel_path.empty() ? std::string("SKILL.md") : rel_path);
        if (res.ok) {
            out_mutated = true;
            out.set("catalog", loadout::core::to_catalog_json(engine.get_catalog()));
        }
        return json_response(out, res.ok ? 200 : 400, res.ok ? "OK" : "Bad Request");
    }

    if (req.method == "POST" && req.path == "/api/profiles") {
        auto body_opt = parse_body();
        if (!body_opt || !body_opt->is_object()) {
            return error_response(400, "Bad Request", "Invalid JSON body");
        }
        const JsonValue& body = *body_opt;
        std::string action = body.get_str("action", "list");

        if (action == "list") {
            return json_response(loadout::core::to_profiles_json(engine.get_profiles(), engine.active_profile()));
        }

        std::string name = body.get_str("name", "");
        if (name.empty()) {
            return error_response(400, "Bad Request", "Profile operation requires 'name'");
        }

        if (action == "save") {
            std::map<std::string, std::map<std::string, bool>> wiring;
            if (const JsonValue* w = body.find("wiring"); w && w->is_object()) {
                for (const auto& [slug_key, targets_val] : w->as_object()) {
                    if (!targets_val.is_object()) continue;
                    std::map<std::string, bool> tmap;
                    for (const auto& [tid, en] : targets_val.as_object()) {
                        tmap[tid] = en.as_bool(false);
                    }
                    wiring[slug_key] = std::move(tmap);
                }
            }
            std::vector<std::string> skills;
            if (const JsonValue* arr = body.find("skills"); arr && arr->is_array()) {
                for (const auto& item : arr->as_array()) {
                    if (item.is_string() && !item.as_string().empty()) {
                        skills.push_back(item.as_string());
                    }
                }
            }
            auto res = engine.save_profile(name, wiring, skills);
            JsonValue out = loadout::core::to_operation_result_json(res);
            out.set("name", name);
            out.set("activeProfile", engine.active_profile());
            out.set("profiles", loadout::core::to_profiles_json(engine.get_profiles(), engine.active_profile())["profiles"]);
            if (res.ok) {
                out_mutated = true;
                out.set("catalog", loadout::core::to_catalog_json(engine.get_catalog()));
            }
            return json_response(out, res.ok ? 200 : 400, res.ok ? "OK" : "Bad Request");
        }

        if (action == "apply") {
            auto res = engine.apply_profile(name);
            JsonValue out = loadout::core::to_operation_result_json(res);
            out.set("name", name);
            out.set("activeProfile", engine.active_profile());
            out.set("profiles", loadout::core::to_profiles_json(engine.get_profiles(), engine.active_profile())["profiles"]);
            if (res.ok) {
                out_mutated = true;
                out.set("catalog", loadout::core::to_catalog_json(engine.get_catalog()));
            }
            return json_response(out, res.ok ? 200 : 400, res.ok ? "OK" : "Bad Request");
        }

        if (action == "delete" || action == "remove") {
            auto res = engine.delete_profile(name);
            JsonValue out = loadout::core::to_operation_result_json(res);
            out.set("name", name);
            out.set("activeProfile", engine.active_profile());
            out.set("profiles", loadout::core::to_profiles_json(engine.get_profiles(), engine.active_profile())["profiles"]);
            if (res.ok) {
                out_mutated = true;
                out.set("catalog", loadout::core::to_catalog_json(engine.get_catalog()));
            }
            return json_response(out, res.ok ? 200 : 400, res.ok ? "OK" : "Bad Request");
        }

        return error_response(400, "Bad Request", "Unknown profile action: " + action);
    }

    if (req.method == "POST" && req.path == "/api/undo") {
        auto res = engine.undo_last_quarantine();
        JsonValue out = loadout::core::to_operation_result_json(res);
        if (res.ok) {
            out_mutated = true;
            out.set("catalog", loadout::core::to_catalog_json(engine.get_catalog()));
        }
        return json_response(out, res.ok ? 200 : 400, res.ok ? "OK" : "Bad Request");
    }

    return error_response(404, "Not Found", "Unknown endpoint: " + req.path);
}

} // namespace

void request_http_server_stop() {
    g_http_stop.store(true, std::memory_order_relaxed);
}

void launch_system_browser(const std::string& url) {
    launch_browser(url);
}

int run_http_server(Engine& engine, const HttpServerOptions& opts) {
    g_http_stop.store(false, std::memory_order_relaxed);

    ::signal(SIGPIPE, SIG_IGN);
    ::signal(SIGCHLD, SIG_IGN);

    struct sigaction sa{};
    sa.sa_handler = handle_http_signal;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    ::sigaction(SIGINT, &sa, nullptr);
    ::sigaction(SIGTERM, &sa, nullptr);

    int listen_fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd < 0) {
        std::cerr << "loadout ui: socket() failed: " << std::strerror(errno) << '\n';
        return 1;
    }

    int optval = 1;
    ::setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(opts.port);
    // Bind strictly to 127.0.0.1 for security
    if (::inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr) != 1) {
        ::close(listen_fd);
        std::cerr << "loadout ui: failed to parse 127.0.0.1\n";
        return 1;
    }

    if (::bind(listen_fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
        if (opts.fallback_ephemeral && opts.port != 0 && errno == EADDRINUSE) {
            addr.sin_port = htons(0);
            if (::bind(listen_fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
                std::cerr << "loadout ui: bind(127.0.0.1:0) failed: " << std::strerror(errno) << '\n';
                ::close(listen_fd);
                return 1;
            }
        } else {
            std::cerr << "loadout ui: bind(127.0.0.1:" << opts.port << ") failed: " << std::strerror(errno) << '\n';
            ::close(listen_fd);
            return 1;
        }
    }

    if (::listen(listen_fd, 64) != 0) {
        std::cerr << "loadout ui: listen() failed: " << std::strerror(errno) << '\n';
        ::close(listen_fd);
        return 1;
    }

    sockaddr_in bound{};
    socklen_t bound_len = sizeof(bound);
    uint16_t actual_port = opts.port;
    if (::getsockname(listen_fd, reinterpret_cast<sockaddr*>(&bound), &bound_len) == 0) {
        actual_port = ntohs(bound.sin_port);
    }

    set_nonblocking(listen_fd);

    // Prime initial scan so first page load is instant
    (void)engine.scan();

    std::string url = "http://127.0.0.1:" + std::to_string(actual_port);
    std::cout << "LOADOUT_UI_URL=" << url << '\n' << std::flush;

    if (opts.on_ready) {
        opts.on_ready(url);
    }

    if (opts.open_browser) {
        launch_browser(url);
    }

    std::vector<ClientConn> clients;
    std::vector<int> sse_fds;
    auto last_ping = std::chrono::steady_clock::now();

    while (!g_http_stop.load(std::memory_order_relaxed)) {
        const size_t polled_client_count = clients.size();
        const size_t polled_sse_count = sse_fds.size();

        std::vector<pollfd> pfds;
        pfds.reserve(1U + polled_client_count + polled_sse_count);

        pfds.push_back(pollfd{listen_fd, POLLIN, 0});
        for (const auto& c : clients) {
            pfds.push_back(pollfd{c.fd, POLLIN, 0});
        }
        for (int sfd : sse_fds) {
            pfds.push_back(pollfd{sfd, POLLIN, 0});
        }

        int rc = ::poll(pfds.data(), static_cast<nfds_t>(pfds.size()), 500);
        if (rc < 0) {
            if (errno == EINTR) continue;
            break;
        }

        // 1. Reap disconnected SSE clients first while indices match pfds 1-to-1
        const size_t sse_offset = 1U + polled_client_count;
        for (size_t i = polled_sse_count; i-- > 0;) {
            short rev = pfds[sse_offset + i].revents;
            if ((rev & (POLLERR | POLLHUP | POLLNVAL)) != 0) {
                ::close(sse_fds[i]);
                sse_fds.erase(sse_fds.begin() + static_cast<std::ptrdiff_t>(i));
            } else if ((rev & POLLIN) != 0) {
                char drain[64];
                ssize_t n = ::recv(sse_fds[i], drain, sizeof(drain), 0);
                if (n <= 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
                    ::close(sse_fds[i]);
                    sse_fds.erase(sse_fds.begin() + static_cast<std::ptrdiff_t>(i));
                }
            }
        }

        // 2. Process polled regular HTTP clients (indices 0..polled_client_count-1)
        const size_t client_offset = 1U;
        std::vector<size_t> to_remove_clients;

        for (size_t i = 0; i < polled_client_count; ++i) {
            short rev = pfds[client_offset + i].revents;
            if (rev == 0) continue;

            auto& conn = clients[i];
            if ((rev & (POLLERR | POLLHUP | POLLNVAL)) != 0) {
                ::close(conn.fd);
                conn.fd = -1;
                to_remove_clients.push_back(i);
                continue;
            }

            if ((rev & POLLIN) != 0) {
                char buf[8192];
                bool closed = false;
                while (true) {
                    ssize_t n = ::recv(conn.fd, buf, sizeof(buf), 0);
                    if (n > 0) {
                        conn.recv_buf.append(buf, static_cast<size_t>(n));
                        // Guard against absurd payloads (>16MB)
                        if (conn.recv_buf.size() > 16U * 1024U * 1024U) {
                            closed = true;
                            break;
                        }
                    } else if (n == 0) {
                        closed = true;
                        break;
                    } else {
                        if (errno == EAGAIN || errno == EWOULDBLOCK) break;
                        closed = true;
                        break;
                    }
                }

                HttpRequest req;
                bool ready = false;
                if (parse_request_headers(conn, req)) {
                    if (conn.recv_buf.size() >= conn.header_end + conn.content_length) {
                        req.body = conn.recv_buf.substr(conn.header_end, conn.content_length);
                        ready = true;
                    }
                }

                if (ready) {
                    std::optional<HttpResponse> rejection = reject_untrusted_request(req, actual_port);
                    if (!rejection && req.method == "GET" && req.path == "/api/events") {
                        static constexpr std::string_view kSseInit =
                            "HTTP/1.1 200 OK\r\n"
                            "Content-Type: text/event-stream\r\n"
                            "Cache-Control: no-cache\r\n"
                            "Connection: keep-alive\r\n\r\n"
                            ": connected\n\n";
                        if (send_all_nb(conn.fd, kSseInit.data(), kSseInit.size(), 2000)) {
                            sse_fds.push_back(conn.fd);
                        } else {
                            ::close(conn.fd);
                        }
                        conn.fd = -1;
                        to_remove_clients.push_back(i);
                    } else {
                        bool mutated = false;
                        HttpResponse resp = rejection ? *rejection : handle_api_request(engine, req, mutated);
                        std::string raw = serialize_http_response(resp);
                        (void)send_all_nb(conn.fd, raw.data(), raw.size(), 5000);
                        ::close(conn.fd);
                        conn.fd = -1;
                        to_remove_clients.push_back(i);

                        if (mutated) {
                            broadcast_sse_catalog(sse_fds, engine);
                        }
                    }
                } else if (closed) {
                    ::close(conn.fd);
                    conn.fd = -1;
                    to_remove_clients.push_back(i);
                }
            }
        }

        if (!to_remove_clients.empty()) {
            std::sort(to_remove_clients.rbegin(), to_remove_clients.rend());
            for (size_t idx : to_remove_clients) {
                clients.erase(clients.begin() + static_cast<std::ptrdiff_t>(idx));
            }
        }

        // 3. Accept new connections after processing existing polled descriptors
        if ((pfds[0].revents & POLLIN) != 0) {
            while (true) {
                sockaddr_in peer{};
                socklen_t peer_len = sizeof(peer);
                int cfd = ::accept(listen_fd, reinterpret_cast<sockaddr*>(&peer), &peer_len);
                if (cfd < 0) {
                    break;
                }
                set_nonblocking(cfd);
                clients.push_back(ClientConn{cfd, {}, false, 0, 0});
            }
        }

        auto now = std::chrono::steady_clock::now();
        if (std::chrono::duration_cast<std::chrono::seconds>(now - last_ping).count() >= 15) {
            send_sse_heartbeat(sse_fds);
            last_ping = now;
        }
    }

    for (auto& c : clients) {
        if (c.fd >= 0) ::close(c.fd);
    }
    for (int sfd : sse_fds) {
        if (sfd >= 0) ::close(sfd);
    }
    ::close(listen_fd);
    if (opts.on_stop) {
        opts.on_stop();
    }
    return 0;
}

} // namespace loadout::surfaces
