#include "core/sources.hpp"

#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <spawn.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#include <algorithm>
#include <cctype>
#include <cerrno>
#include <chrono>
#include <cstring>

extern char** environ;

namespace singlet::core {

namespace fs = std::filesystem;
using singlet::util::JsonValue;

namespace {

/// A clone of a large repository can take a while; past this we assume git hangs.
constexpr std::chrono::seconds GIT_TIMEOUT{180};
/// git output we keep for error messages and ls-remote parsing.
constexpr size_t MAX_GIT_OUTPUT_BYTES = 64 * 1024;
/// GitHub limits: owner names up to 39 characters, repository names up to 100.
constexpr size_t MAX_OWNER_LENGTH = 39;
constexpr size_t MAX_REPO_LENGTH = 100;
constexpr size_t MAX_REF_LENGTH = 255;
/// Skill folders sit a few levels deep at most (for example `.claude/skills/<slug>`).
constexpr size_t MAX_DISCOVERY_DEPTH = 6;
constexpr size_t MAX_COPY_DEPTH = 32;
constexpr size_t COMMIT_SHA_LENGTH = 40;

struct ProcessResult {
    bool ok = false;
    std::string output;
};

std::string trim(std::string_view text) {
    size_t start = 0;
    while (start < text.size() && std::isspace(static_cast<unsigned char>(text[start]))) ++start;
    size_t end = text.size();
    while (end > start && std::isspace(static_cast<unsigned char>(text[end - 1]))) --end;
    return std::string(text.substr(start, end - start));
}

std::vector<char*> build_environment(std::vector<std::string>& storage) {
    for (char** entry = environ; entry != nullptr && *entry != nullptr; ++entry) {
        std::string_view var(*entry);
        if (!var.starts_with("GIT_TERMINAL_PROMPT=")) storage.emplace_back(var);
    }
    // Fail instead of waiting for a username and password on the terminal.
    storage.emplace_back("GIT_TERMINAL_PROMPT=0");
    std::vector<char*> envp;
    for (auto& var : storage) envp.push_back(var.data());
    envp.push_back(nullptr);
    return envp;
}

/// Reads child output until EOF or the deadline. Returns false on timeout.
bool read_until_eof(int fd, std::string& output) {
    const auto deadline = std::chrono::steady_clock::now() + GIT_TIMEOUT;
    char buffer[4096];
    while (true) {
        auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(
            deadline - std::chrono::steady_clock::now());
        if (remaining.count() <= 0) return false;
        pollfd pfd{fd, POLLIN, 0};
        int ready = ::poll(&pfd, 1, static_cast<int>(remaining.count()));
        if (ready < 0 && errno == EINTR) continue;
        if (ready <= 0) return false;
        ssize_t count = ::read(fd, buffer, sizeof(buffer));
        if (count < 0 && errno == EINTR) continue;
        if (count <= 0) return true;
        if (output.size() < MAX_GIT_OUTPUT_BYTES) output.append(buffer, static_cast<size_t>(count));
    }
}

/// Runs `git` with args (no shell), stdin from /dev/null, stdout and stderr combined.
ProcessResult run_git(const std::vector<std::string>& args) {
    int pipe_fds[2] = {-1, -1};
    if (::pipe(pipe_fds) != 0) return {false, "Failed to create a pipe for git"};

    posix_spawn_file_actions_t actions;
    posix_spawn_file_actions_init(&actions);
    posix_spawn_file_actions_addopen(&actions, STDIN_FILENO, "/dev/null", O_RDONLY, 0);
    posix_spawn_file_actions_adddup2(&actions, pipe_fds[1], STDOUT_FILENO);
    posix_spawn_file_actions_adddup2(&actions, pipe_fds[1], STDERR_FILENO);
    posix_spawn_file_actions_addclose(&actions, pipe_fds[0]);
    posix_spawn_file_actions_addclose(&actions, pipe_fds[1]);

    std::vector<std::string> argv_storage = {"git"};
    argv_storage.insert(argv_storage.end(), args.begin(), args.end());
    std::vector<char*> argv;
    for (auto& arg : argv_storage) argv.push_back(arg.data());
    argv.push_back(nullptr);
    std::vector<std::string> env_storage;
    std::vector<char*> envp = build_environment(env_storage);

    pid_t pid = 0;
    int spawn_rc = ::posix_spawnp(&pid, "git", &actions, nullptr, argv.data(), envp.data());
    posix_spawn_file_actions_destroy(&actions);
    ::close(pipe_fds[1]);
    if (spawn_rc != 0) {
        ::close(pipe_fds[0]);
        return {false, "Could not run git (is it installed and on PATH?): " + std::string(std::strerror(spawn_rc))};
    }

    ProcessResult result;
    bool finished = read_until_eof(pipe_fds[0], result.output);
    ::close(pipe_fds[0]);
    if (!finished) ::kill(pid, SIGKILL);

    int status = 0;
    while (::waitpid(pid, &status, 0) < 0 && errno == EINTR) {
    }
    if (!finished) return {false, "git timed out"};
    result.ok = WIFEXITED(status) && WEXITSTATUS(status) == 0;
    result.output = trim(result.output);
    return result;
}

std::string url_scheme(std::string_view host_url) {
    size_t pos = host_url.find("://");
    return pos == std::string_view::npos ? std::string() : std::string(host_url.substr(0, pos));
}

/// git config that blocks hooks and every transport except the one host_url uses.
std::vector<std::string> safe_git_config(std::string_view host_url) {
    return {"-c", "core.hooksPath=/dev/null",
            "-c", "protocol.allow=never",
            "-c", "protocol." + url_scheme(host_url) + ".allow=always"};
}

std::string repo_url(std::string_view host_url, const std::string& repo) {
    return std::string(host_url) + "/" + repo + ".git";
}

bool is_commit_sha(std::string_view text) {
    return text.size() == COMMIT_SHA_LENGTH &&
           std::all_of(text.begin(), text.end(), [](char ch) { return std::isxdigit(static_cast<unsigned char>(ch)); });
}

std::vector<std::string_view> split_segments(std::string_view text) {
    std::vector<std::string_view> segments;
    size_t start = 0;
    while (start <= text.size()) {
        size_t slash = text.find('/', start);
        size_t end = slash == std::string_view::npos ? text.size() : slash;
        if (end > start) segments.push_back(text.substr(start, end - start));
        if (slash == std::string_view::npos) break;
        start = slash + 1;
    }
    return segments;
}

bool all_chars(std::string_view text, std::string_view extra) {
    return std::all_of(text.begin(), text.end(), [extra](char ch) {
        return std::isalnum(static_cast<unsigned char>(ch)) || extra.find(ch) != std::string_view::npos;
    });
}

bool is_valid_owner(std::string_view owner) {
    return !owner.empty() && owner.size() <= MAX_OWNER_LENGTH && all_chars(owner, "-");
}

bool is_valid_repo_name(std::string_view name) {
    return !name.empty() && name.size() <= MAX_REPO_LENGTH && name != "." && name != ".." && all_chars(name, "._-");
}

bool is_valid_ref(std::string_view ref) {
    return !ref.empty() && ref.size() <= MAX_REF_LENGTH && ref.front() != '-' && ref.front() != '.' &&
           all_chars(ref, "._-+");
}

bool is_valid_path_segment(std::string_view segment) {
    if (segment == "." || segment == "..") return false;
    return std::none_of(segment.begin(), segment.end(), [](char ch) {
        return static_cast<unsigned char>(ch) < 0x20 || ch == '\\';
    });
}

bool is_skill_md_name(std::string_view name) {
    return name == "SKILL.md" || name == "skill.md";
}

std::string_view strip_scheme_and_host(std::string_view url, std::string& error) {
    if (url.starts_with("https://")) {
        url.remove_prefix(8);
    } else if (url.find("://") != std::string_view::npos) {
        error = "Only https:// GitHub URLs are supported";
        return {};
    }
    if (url.starts_with("www.")) url.remove_prefix(4);
    if (!url.starts_with("github.com/")) {
        error = "Not a GitHub URL (expected github.com/<owner>/<repo>)";
        return {};
    }
    return url.substr(11);
}

/// Fills ref and path from the segments after owner/repo: `tree|blob/<ref>/<path...>`.
bool parse_tree_suffix(const std::vector<std::string_view>& segments, GithubLocation& location, std::string& error) {
    if (segments.size() == 2) return true;
    bool is_blob = segments[2] == "blob";
    if ((segments[2] != "tree" && !is_blob) || segments.size() < 4) {
        error = "Expected github.com/<owner>/<repo>/tree/<ref>/<path>";
        return false;
    }
    if (!is_valid_ref(segments[3])) {
        error = "Invalid branch or tag name: " + std::string(segments[3]);
        return false;
    }
    location.ref = std::string(segments[3]);

    size_t path_end = segments.size();
    if (is_blob) {
        if (!is_skill_md_name(segments.back())) {
            error = "A blob URL must point at a SKILL.md file";
            return false;
        }
        --path_end;
    }
    for (size_t i = 4; i < path_end; ++i) {
        if (!is_valid_path_segment(segments[i])) {
            error = "Invalid path segment: " + std::string(segments[i]);
            return false;
        }
        if (!location.path.empty()) location.path += '/';
        location.path += segments[i];
    }
    return true;
}

bool is_real_dir(const fs::path& path) {
    struct stat st {};
    return ::lstat(path.c_str(), &st) == 0 && S_ISDIR(st.st_mode);
}

bool has_skill_md(const fs::path& dir) {
    struct stat st {};
    for (const char* name : {"SKILL.md", "skill.md"}) {
        fs::path candidate = dir / name;
        if (::lstat(candidate.c_str(), &st) == 0 && S_ISREG(st.st_mode)) return true;
    }
    return false;
}

/// True when every component of subpath under checkout is a real folder, not a symlink.
bool is_real_dir_chain(const fs::path& checkout, const std::string& subpath) {
    fs::path current = checkout;
    for (const auto& segment : split_segments(subpath)) {
        if (!is_valid_path_segment(segment)) return false;
        current /= std::string(segment);
        if (!is_real_dir(current)) return false;
    }
    return true;
}

void collect_skill_dirs(const fs::path& checkout, const fs::path& dir, size_t depth, std::vector<std::string>& out) {
    if (has_skill_md(dir)) {
        std::string rel = dir.lexically_relative(checkout).generic_string();
        out.push_back(rel == "." ? std::string() : rel);
        return;
    }
    if (depth == 0) return;
    std::error_code ec;
    for (const auto& entry : fs::directory_iterator(dir, ec)) {
        std::string name = entry.path().filename().string();
        if (name == ".git" || name == "node_modules") continue;
        if (!is_real_dir(entry.path())) continue;
        collect_skill_dirs(checkout, entry.path(), depth - 1, out);
    }
}

bool copy_dir_contents(const fs::path& src, const fs::path& dst, size_t depth, std::string& err) {
    if (depth == 0) {
        err = "Skill folder is nested too deeply: " + src.string();
        return false;
    }
    std::error_code ec;
    if (!fs::create_directory(dst, ec)) {
        err = "Failed to create " + dst.string() + (ec ? ": " + ec.message() : " (already exists)");
        return false;
    }
    for (const auto& entry : fs::directory_iterator(src, ec)) {
        std::string name = entry.path().filename().string();
        if (name == ".git") continue;
        std::error_code status_ec;
        auto status = entry.symlink_status(status_ec);
        if (status_ec || fs::is_symlink(status)) continue;
        if (fs::is_directory(status)) {
            if (!copy_dir_contents(entry.path(), dst / name, depth - 1, err)) return false;
        } else if (fs::is_regular_file(status)) {
            std::error_code copy_ec;
            fs::copy_file(entry.path(), dst / name, copy_ec);
            if (copy_ec) {
                err = "Failed to copy " + entry.path().string() + ": " + copy_ec.message();
                return false;
            }
        }
    }
    if (ec) {
        err = "Failed to read " + src.string() + ": " + ec.message();
        return false;
    }
    return true;
}

/// Picks the commit from `git ls-remote` output in the order `git clone --branch` resolves refs.
std::string pick_ls_remote_commit(const std::string& output, const std::string& ref) {
    std::vector<std::string> preferred = ref.empty()
        ? std::vector<std::string>{"HEAD"}
        : std::vector<std::string>{"refs/heads/" + ref, "refs/tags/" + ref + "^{}", "refs/tags/" + ref};
    for (const auto& wanted : preferred) {
        size_t start = 0;
        while (start < output.size()) {
            size_t end = output.find('\n', start);
            if (end == std::string::npos) end = output.size();
            std::string_view line(output.data() + start, end - start);
            size_t tab = line.find('\t');
            if (tab != std::string_view::npos && line.substr(tab + 1) == wanted && is_commit_sha(line.substr(0, tab))) {
                return std::string(line.substr(0, tab));
            }
            start = end + 1;
        }
    }
    return {};
}

}  // namespace

std::optional<GithubLocation> parse_github_url(std::string_view url, std::string& error) {
    std::string trimmed = trim(url);
    std::string_view rest(trimmed);
    if (rest.find_first_of("?#\\ \t") != std::string_view::npos) {
        error = "URL must not contain query strings, fragments, or spaces";
        return std::nullopt;
    }
    rest = strip_scheme_and_host(rest, error);
    if (!error.empty()) return std::nullopt;

    auto segments = split_segments(rest);
    if (segments.size() < 2) {
        error = "Expected github.com/<owner>/<repo>";
        return std::nullopt;
    }
    std::string_view repo_name = segments[1];
    if (repo_name.ends_with(".git")) repo_name.remove_suffix(4);
    if (!is_valid_owner(segments[0]) || !is_valid_repo_name(repo_name)) {
        error = "Invalid GitHub owner or repository name";
        return std::nullopt;
    }

    GithubLocation location;
    location.repo = std::string(segments[0]) + "/" + std::string(repo_name);
    if (!parse_tree_suffix(segments, location, error)) return std::nullopt;
    return location;
}

FetchResult clone_repo(
    std::string_view host_url,
    const std::string& repo,
    const std::string& ref,
    const fs::path& dest) {
    std::vector<std::string> args = safe_git_config(host_url);
    args.insert(args.end(), {"clone", "--depth", "1", "--quiet", "--no-tags"});
    if (!ref.empty()) args.insert(args.end(), {"--branch", ref});
    args.insert(args.end(), {"--", repo_url(host_url, repo), dest.string()});

    ProcessResult cloned = run_git(args);
    if (!cloned.ok) return {false, "git clone of " + repo + " failed: " + cloned.output, ""};

    ProcessResult head = run_git({"-C", dest.string(), "rev-parse", "HEAD"});
    if (!head.ok || !is_commit_sha(head.output)) {
        return {false, "Could not read the cloned commit: " + head.output, ""};
    }
    return {true, "", head.output};
}

FetchResult remote_commit(std::string_view host_url, const std::string& repo, const std::string& ref) {
    std::vector<std::string> args = safe_git_config(host_url);
    args.insert(args.end(), {"ls-remote", "--", repo_url(host_url, repo)});
    if (ref.empty()) {
        args.emplace_back("HEAD");
    } else {
        args.insert(args.end(), {"refs/heads/" + ref, "refs/tags/" + ref, "refs/tags/" + ref + "^{}"});
    }
    ProcessResult listed = run_git(args);
    if (!listed.ok) return {false, "git ls-remote of " + repo + " failed: " + listed.output, ""};
    std::string commit = pick_ls_remote_commit(listed.output, ref);
    if (commit.empty()) return {false, "Ref not found on " + repo + ": " + (ref.empty() ? "HEAD" : ref), ""};
    return {true, "", commit};
}

std::vector<std::string> find_skill_paths(const fs::path& checkout, const std::string& subpath) {
    std::vector<std::string> out;
    if (!is_real_dir(checkout) || !is_real_dir_chain(checkout, subpath)) return out;
    collect_skill_dirs(checkout, checkout / subpath, MAX_DISCOVERY_DEPTH, out);
    std::sort(out.begin(), out.end());
    return out;
}

std::string slug_for_skill_path(const std::string& repo, const std::string& repo_path) {
    std::string_view source = repo_path.empty() ? std::string_view(repo) : std::string_view(repo_path);
    size_t slash = source.rfind('/');
    return std::string(slash == std::string_view::npos ? source : source.substr(slash + 1));
}

bool copy_skill_tree(const fs::path& src, const fs::path& dst, std::string& err) {
    if (!is_real_dir(src)) {
        err = "Not a folder: " + src.string();
        return false;
    }
    if (!copy_dir_contents(src, dst, MAX_COPY_DEPTH, err)) {
        std::error_code ec;
        fs::remove_all(dst, ec);
        return false;
    }
    return true;
}

std::string_view source_state_to_string(SourceState state) {
    switch (state) {
        case SourceState::UpToDate: return "up-to-date";
        case SourceState::UpdateAvailable: return "update-available";
        case SourceState::LocalChanges: return "local-changes";
        case SourceState::Updated: return "updated";
        case SourceState::Failed: return "failed";
    }
    return "failed";
}

JsonValue to_skill_source_json(const SkillSource& source) {
    JsonValue obj = JsonValue::object();
    obj.set("repo", source.repo);
    obj.set("ref", source.ref);
    obj.set("path", source.path);
    obj.set("commit", source.commit);
    obj.set("treeHash", source.tree_hash);
    obj.set("updatedAt", source.updated_at);
    return obj;
}

std::optional<SkillSource> skill_source_from_json(const JsonValue& value) {
    if (!value.is_object()) return std::nullopt;
    SkillSource source;
    source.repo = value.get_str("repo");
    if (source.repo.empty()) return std::nullopt;
    source.ref = value.get_str("ref");
    source.path = value.get_str("path");
    source.commit = value.get_str("commit");
    source.tree_hash = value.get_str("treeHash");
    source.updated_at = value.get_str("updatedAt");
    return source;
}

}  // namespace singlet::core
