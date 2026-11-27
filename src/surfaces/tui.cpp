#include "surfaces/tui.hpp"

#include <poll.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

#include <algorithm>
#include <cctype>
#include <csignal>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include "core/engine.hpp"
#include "surfaces/cli.hpp"

namespace singlet::surfaces {

namespace {

// Artboard 7 truecolor ANSI palette matching prototype/index.html
constexpr std::string_view kReset = "\x1b[0m";
constexpr std::string_view kInv = "\x1b[48;2;201;206;214;38;2;8;9;11m";        // #C9CED6 bg, #08090B fg
constexpr std::string_view kAcc = "\x1b[48;2;224;221;189;38;2;8;9;11m";        // #E0DDBD bg, #08090B fg
constexpr std::string_view kSelBg = "\x1b[48;2;29;34;42m";                     // #1D222A row highlight
constexpr std::string_view kEm = "\x1b[38;2;61;220;151m";                      // #3DDC97 emerald
constexpr std::string_view kAm = "\x1b[38;2;245;181;68m";                      // #F5B544 amber
constexpr std::string_view kRo = "\x1b[38;2;242;100;122m";                     // #F2647A rose
constexpr std::string_view kSk = "\x1b[38;2;108;182;255m";                     // #6CB6FF sky
constexpr std::string_view kDim = "\x1b[38;2;93;102;118m";                     // #5D6676 dim
constexpr std::string_view kBold = "\x1b[1;38;2;255;255;255m";                 // bold white
constexpr std::string_view kAccFg = "\x1b[1;38;2;224;221;189m";                // #E0DDBD fg

constexpr size_t kTokenWarnThreshold = 3000;
constexpr int kCellWidth = 5;
constexpr int kTokColWidth = 6;
constexpr int kWideThreshold = 100;

volatile sig_atomic_t g_tui_stop = 0;
volatile sig_atomic_t g_tui_resized = 0;
termios g_orig_termios{};
bool g_raw_active = false;

void write_all_fd(int fd, const char* data, size_t len) noexcept {
    while (len > 0) {
        ssize_t n = ::write(fd, data, len);
        if (n <= 0) break;
        data += n;
        len -= static_cast<size_t>(n);
    }
}

void restore_terminal_raw() noexcept {
    if (g_raw_active) {
        ::tcsetattr(STDIN_FILENO, TCSAFLUSH, &g_orig_termios);
        static constexpr std::string_view kRestoreSeq = "\x1b[?25h\x1b[0m\x1b[?1049l";
        write_all_fd(STDOUT_FILENO, kRestoreSeq.data(), kRestoreSeq.size());
        g_raw_active = false;
    }
}

void handle_tui_signal(int sig) {
    if (sig == SIGWINCH) {
        g_tui_resized = 1;
        return;
    }
    g_tui_stop = 1;
}

class TerminalGuard {
public:
    TerminalGuard() noexcept = default;
    TerminalGuard(const TerminalGuard&) = delete;
    TerminalGuard& operator=(const TerminalGuard&) = delete;
    ~TerminalGuard() noexcept { restore(); }

    [[nodiscard]] bool enter() noexcept {
        if (!::isatty(STDIN_FILENO) || !::isatty(STDOUT_FILENO)) {
            return false;
        }
        if (::tcgetattr(STDIN_FILENO, &g_orig_termios) != 0) {
            return false;
        }
        termios raw = g_orig_termios;
        raw.c_iflag &= static_cast<tcflag_t>(~(BRKINT | ICRNL | INPCK | ISTRIP | IXON));
        raw.c_oflag &= static_cast<tcflag_t>(~(OPOST));
        raw.c_cflag |= static_cast<tcflag_t>(CS8);
        raw.c_lflag &= static_cast<tcflag_t>(~(ECHO | ICANON | IEXTEN | ISIG));
        raw.c_cc[VMIN] = 0;
        raw.c_cc[VTIME] = 1;

        if (::tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) != 0) {
            return false;
        }
        g_raw_active = true;
        entered_ = true;

        static constexpr std::string_view kInitSeq = "\x1b[?1049h\x1b[?25l\x1b[2J\x1b[H";
        write_all_fd(STDOUT_FILENO, kInitSeq.data(), kInitSeq.size());
        return true;
    }

    void restore() noexcept {
        if (entered_) {
            restore_terminal_raw();
            entered_ = false;
        }
    }

private:
    bool entered_ = false;
};

struct TermSize {
    int rows = 24;
    int cols = 80;
};

[[nodiscard]] TermSize query_term_size() noexcept {
    winsize ws{};
    if (::ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_row > 0 && ws.ws_col > 0) {
        return TermSize{static_cast<int>(ws.ws_row), static_cast<int>(ws.ws_col)};
    }
    return TermSize{};
}

[[nodiscard]] std::string truncate_ascii(std::string_view s, size_t max_len) {
    if (max_len == 0) return "";
    if (s.size() <= max_len) return std::string(s);
    if (max_len == 1) return std::string(s.substr(0, 1));
    return std::string(s.substr(0, max_len - 1)) + "~";
}

[[nodiscard]] std::string pad_ascii(std::string_view s, size_t width) {
    if (s.size() >= width) return truncate_ascii(s, width);
    std::string out(s);
    out.append(width - s.size(), ' ');
    return out;
}

[[nodiscard]] std::string to_lower_copy(std::string_view s) {
    std::string out(s);
    std::transform(out.begin(), out.end(), out.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return out;
}

[[nodiscard]] std::string fmt_tok(size_t tokens) {
    if (tokens >= 1000) {
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(1) << (static_cast<double>(tokens) / 1000.0) << "k";
        return oss.str();
    }
    return std::to_string(tokens);
}

[[nodiscard]] std::string target_glyph(const singlet::core::TargetInstance& t) {
    using singlet::core::AdapterKind;
    switch (t.adapter) {
        case AdapterKind::Antigravity: return "AG";
        case AdapterKind::Codex: return "CX";
        case AdapterKind::OpenCode: return "OC";
        case AdapterKind::ZCode: return "ZC";
        case AdapterKind::Cursor: return "CR";
        case AdapterKind::Windsurf: return "WS";
        case AdapterKind::ClaudeCode: {
            std::string lid = to_lower_copy(t.id);
            if (lid == "claude" || lid == "claude-main" || lid == "claude-personal") return "CP";
            size_t dash = lid.find('-');
            if (dash != std::string::npos && dash + 1 < lid.size()) {
                char second = static_cast<char>(std::toupper(static_cast<unsigned char>(lid[dash + 1])));
                return std::string("C") + second;
            }
            return "CC";
        }
        case AdapterKind::CustomLenient:
        case AdapterKind::CustomStrict: {
            std::string initials;
            bool take_next = true;
            for (char c : t.display_name) {
                if (std::isalnum(static_cast<unsigned char>(c))) {
                    if (take_next) {
                        initials.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(c))));
                        take_next = false;
                        if (initials.size() == 2) break;
                    }
                } else {
                    take_next = true;
                }
            }
            if (initials.size() < 2) {
                for (char c : t.id) {
                    if (std::isalnum(static_cast<unsigned char>(c))) {
                        initials.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(c))));
                        if (initials.size() == 2) break;
                    }
                }
            }
            while (initials.size() < 2) initials.push_back('T');
            return initials.substr(0, 2);
        }
    }
    return "TG";
}

enum class CellState {
    On,
    Off,
    Copy,
    Drift,
    Frontmatter,
    Broken,
    Shadow,
};

[[nodiscard]] CellState resolve_cell_state(
    const singlet::core::CanonicalSkill& skill,
    const singlet::core::TargetInstance& target) noexcept {
    auto it = skill.target_states.find(target.id);
    if (it == skill.target_states.end()) {
        return CellState::Off;
    }
    const auto& st = it->second;
    if (!st.has_entry && !st.enabled) {
        return CellState::Off;
    }
    using singlet::core::EntryKind;
    if (st.entry_kind == EntryKind::DanglingSymlink) return CellState::Broken;
    if (st.entry_kind == EntryKind::PluginShadow) return CellState::Shadow;
    if (st.entry_kind == EntryKind::PhysicalCopy) {
        return skill.has_drift ? CellState::Drift : CellState::Copy;
    }
    if (st.enabled) {
        if (st.incompatible || (!skill.antigravity_valid && singlet::core::adapter_requires_strict_frontmatter(target.adapter))) {
            return CellState::Frontmatter;
        }
        return CellState::On;
    }
    return CellState::Off;
}

[[nodiscard]] std::string_view cell_glyph(CellState st) noexcept {
    switch (st) {
        case CellState::On: return "\xe2\x97\x8f";           // ●
        case CellState::Off: return "\xc2\xb7";              // ·
        case CellState::Copy: return "\xe2\x96\xa3";         // ▣
        case CellState::Drift: return "\xe2\x89\x88";        // ≈
        case CellState::Frontmatter: return "!";
        case CellState::Broken: return "\xc3\x97";           // ×
        case CellState::Shadow: return "p";
    }
    return " ";
}

[[nodiscard]] std::string_view cell_color(CellState st) noexcept {
    switch (st) {
        case CellState::On: return kEm;
        case CellState::Off: return kDim;
        case CellState::Copy: return kAm;
        case CellState::Drift: return kAm;
        case CellState::Frontmatter: return kRo;
        case CellState::Broken: return kRo;
        case CellState::Shadow: return kSk;
    }
    return kDim;
}

[[nodiscard]] std::string_view cell_label(CellState st) noexcept {
    switch (st) {
        case CellState::On: return "wired";
        case CellState::Off: return "unwired";
        case CellState::Copy: return "physical copy";
        case CellState::Drift: return "drifted copy";
        case CellState::Frontmatter: return "frontmatter invalid";
        case CellState::Broken: return "broken link";
        case CellState::Shadow: return "plugin shadow";
    }
    return "unknown";
}

[[nodiscard]] bool skill_has_broken(const singlet::core::CanonicalSkill& s) noexcept {
    for (const auto& [tid, st] : s.target_states) {
        (void)tid;
        if (st.has_entry && st.entry_kind == singlet::core::EntryKind::DanglingSymlink) {
            return true;
        }
    }
    return false;
}

[[nodiscard]] bool skill_needs_attention(const singlet::core::CanonicalSkill& s) noexcept {
    return !s.antigravity_valid || s.has_drift || s.has_duplicates || s.has_physical_copy || skill_has_broken(s);
}

enum class FilterKind {
    All,
    Attention,
    Drift,
    Broken,
};

enum class Key {
    None,
    Up,
    Down,
    Left,
    Right,
    Space,
    Enter,
    Esc,
    Backspace,
    CtrlC,
    Char,
};

struct KeyEvent {
    Key key = Key::None;
    char ch = 0;
};

[[nodiscard]] KeyEvent read_key_event() noexcept {
    pollfd pfd{};
    pfd.fd = STDIN_FILENO;
    pfd.events = POLLIN;
    int rc = ::poll(&pfd, 1, 100);
    if (rc <= 0 || (pfd.revents & POLLIN) == 0) {
        return KeyEvent{};
    }

    char c = 0;
    if (::read(STDIN_FILENO, &c, 1) != 1) {
        return KeyEvent{};
    }

    if (c == 3) return KeyEvent{Key::CtrlC, 0};
    if (c == ' ') return KeyEvent{Key::Space, ' '};
    if (c == '\r' || c == '\n') return KeyEvent{Key::Enter, c};
    if (c == 127 || c == 8) return KeyEvent{Key::Backspace, 0};

    if (c == '\x1b') {
        pollfd esc_pfd{};
        esc_pfd.fd = STDIN_FILENO;
        esc_pfd.events = POLLIN;
        if (::poll(&esc_pfd, 1, 25) > 0 && (esc_pfd.revents & POLLIN) != 0) {
            char seq[2] = {0, 0};
            if (::read(STDIN_FILENO, &seq[0], 1) == 1 && seq[0] == '[') {
                if (::read(STDIN_FILENO, &seq[1], 1) == 1) {
                    switch (seq[1]) {
                        case 'A': return KeyEvent{Key::Up, 0};
                        case 'B': return KeyEvent{Key::Down, 0};
                        case 'C': return KeyEvent{Key::Right, 0};
                        case 'D': return KeyEvent{Key::Left, 0};
                        default: break;
                    }
                }
            }
        }
        return KeyEvent{Key::Esc, 0};
    }

    return KeyEvent{Key::Char, c};
}

} // namespace

int run_tui(singlet::core::Engine& engine) {
    if (!::isatty(STDIN_FILENO) || !::isatty(STDOUT_FILENO)) {
        std::cerr << "singlet tui requires an interactive TTY.\n";
        return 1;
    }

    g_tui_stop = 0;
    g_tui_resized = 0;

    struct sigaction sa{};
    sa.sa_handler = handle_tui_signal;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    ::sigaction(SIGINT, &sa, nullptr);
    ::sigaction(SIGTERM, &sa, nullptr);
    ::sigaction(SIGWINCH, &sa, nullptr);

    TerminalGuard guard;
    if (!guard.enter()) {
        std::cerr << "singlet tui: failed to initialize raw terminal mode.\n";
        return 1;
    }

    singlet::core::CatalogSnapshot snap = engine.scan();

    size_t cursor_row = 0;
    size_t cursor_col = 0;
    size_t scroll_col = 0;

    FilterKind active_filter = FilterKind::All;
    bool search_mode = false;
    std::string search_query;
    std::string status_msg;
    std::string active_profile = engine.active_profile().empty() ? "default" : engine.active_profile();

    auto compute_visible_indices = [&]() -> std::vector<size_t> {
        std::vector<size_t> indices;
        std::string q = to_lower_copy(search_query);
        for (size_t i = 0; i < snap.skills.size(); ++i) {
            const auto& s = snap.skills[i];
            if (active_filter == FilterKind::Attention && !skill_needs_attention(s)) continue;
            if (active_filter == FilterKind::Drift && !s.has_drift) continue;
            if (active_filter == FilterKind::Broken && !skill_has_broken(s)) continue;

            if (!q.empty()) {
                bool match = to_lower_copy(s.slug).find(q) != std::string::npos ||
                             to_lower_copy(s.description).find(q) != std::string::npos ||
                             to_lower_copy(s.name).find(q) != std::string::npos;
                if (!match) {
                    for (const auto& tag : s.tags) {
                        if (to_lower_copy(tag).find(q) != std::string::npos) {
                            match = true;
                            break;
                        }
                    }
                }
                if (!match) continue;
            }
            indices.push_back(i);
        }
        return indices;
    };

    auto render = [&]() {
        const TermSize ts = query_term_size();
        const size_t W = static_cast<size_t>(std::max(40, ts.cols));
        const int H = std::max(14, ts.rows);
        const bool is_wide = (ts.cols >= kWideThreshold);
        const size_t name_w = is_wide ? 26U : 24U;

        auto visible_skills = compute_visible_indices();

        if (!visible_skills.empty() && cursor_row >= visible_skills.size()) {
            cursor_row = visible_skills.size() - 1;
        } else if (visible_skills.empty()) {
            cursor_row = 0;
        }
        if (!snap.targets.empty() && cursor_col >= snap.targets.size()) {
            cursor_col = snap.targets.size() - 1;
        } else if (snap.targets.empty()) {
            cursor_col = 0;
        }

        // Compute horizontal target window
        const size_t fixed_left = 2U + name_w + static_cast<size_t>(kTokColWidth);
        const size_t desc_reserve = is_wide ? 26U : 0U;
        const size_t avail_for_targets = (W > fixed_left + desc_reserve + static_cast<size_t>(kCellWidth))
                                             ? (W - fixed_left - desc_reserve)
                                             : static_cast<size_t>(kCellWidth);
        const size_t max_visible_targets = std::max<size_t>(1U, avail_for_targets / static_cast<size_t>(kCellWidth));

        if (cursor_col < scroll_col) scroll_col = cursor_col;
        if (cursor_col >= scroll_col + max_visible_targets) {
            scroll_col = cursor_col - max_visible_targets + 1;
        }
        const size_t end_col = std::min(snap.targets.size(), scroll_col + max_visible_targets);

        // Precompute stats & target token budgets
        size_t att_count = 0;
        size_t drift_count = 0;
        size_t broken_count = 0;
        for (const auto& s : snap.skills) {
            if (skill_needs_attention(s)) att_count++;
            if (s.has_drift) drift_count++;
            if (skill_has_broken(s)) broken_count++;
        }

        std::vector<size_t> target_tokens(snap.targets.size(), 0);
        for (const auto& s : snap.skills) {
            for (size_t ti = 0; ti < snap.targets.size(); ++ti) {
                CellState st = resolve_cell_state(s, snap.targets[ti]);
                if (st == CellState::On || st == CellState::Copy || st == CellState::Drift || st == CellState::Frontmatter) {
                    target_tokens[ti] += s.approx_tokens;
                }
            }
        }

        std::ostringstream buf;
        buf << "\x1b[H\x1b[2J";

        // Row 1: Inverted title bar
        {
            std::ostringstream left_ss;
            left_ss << " singlet tui \xc2\xb7 "
                    << singlet::core::contract_home_path(snap.canonical_root, engine.home_dir())
                    << " \xc2\xb7 " << snap.skills.size() << " skills \xc2\xb7 "
                    << snap.targets.size() << " targets \xc2\xb7 profile " << active_profile;
            std::ostringstream right_ss;
            right_ss << "scan " << std::fixed << std::setprecision(1) << snap.last_scan_ms << "ms ";
            std::string right = right_ss.str();
            size_t left_budget = (W > right.size()) ? (W - right.size()) : 0U;
            buf << kInv << pad_ascii(left_ss.str(), left_budget) << right << kReset << "\r\n";
        }

        // Row 2: Filter pill bar ([a]ll [!]attention [~]drift [x]broken)
        {
            auto pill = [&](FilterKind kind, const std::string& label) {
                if (active_filter == kind) {
                    buf << kAccFg << label << kReset;
                } else {
                    buf << kDim << label << kReset;
                }
            };
            buf << " ";
            pill(FilterKind::All, "[a]ll " + std::to_string(snap.skills.size()));
            buf << "  ";
            pill(FilterKind::Attention, "[!]attention " + std::to_string(att_count));
            buf << "  ";
            pill(FilterKind::Drift, "[~]drift " + std::to_string(drift_count));
            buf << "  ";
            pill(FilterKind::Broken, "[x]broken " + std::to_string(broken_count));
            if (snap.can_undo) {
                buf << "   " << kEm << "[u]ndo ready" << kReset;
            }
            buf << "\x1b[K\r\n";
        }

        // Row 3: Search bar or blank line
        if (search_mode) {
            buf << " " << kAccFg << "search: /" << search_query << "_" << kReset << "\x1b[K\r\n";
        } else if (!search_query.empty()) {
            buf << " " << kAm << "filter: /" << search_query << " (" << visible_skills.size() << " matches)" << kReset << "\x1b[K\r\n";
        } else {
            buf << "\x1b[K\r\n";
        }

        // Row 4: Header row 1 (SKILL, TOK, 2-char target glyphs, optional DESCRIPTION)
        {
            std::string head = "  " + pad_ascii("SKILL", name_w) + pad_ascii("TOK", static_cast<size_t>(kTokColWidth));
            size_t col_used = head.size();
            buf << kBold << head << kReset;
            for (size_t ci = scroll_col; ci < end_col; ++ci) {
                std::string g = pad_ascii(" " + target_glyph(snap.targets[ci]), static_cast<size_t>(kCellWidth));
                if (ci == cursor_col) {
                    buf << kAcc << g << kReset;
                } else {
                    buf << kBold << g << kReset;
                }
                col_used += static_cast<size_t>(kCellWidth);
            }
            if (is_wide && col_used + 13 <= W) {
                buf << kBold << "  DESCRIPTION" << kReset;
            }
            buf << "\x1b[K\r\n";
        }

        // Row 5: Header row 2 (per-target token budgets)
        {
            buf << std::string(fixed_left, ' ');
            for (size_t ci = scroll_col; ci < end_col; ++ci) {
                bool hot = target_tokens[ci] >= kTokenWarnThreshold;
                std::string b = pad_ascii(fmt_tok(target_tokens[ci]) + (hot ? "!" : ""), static_cast<size_t>(kCellWidth));
                buf << (hot ? kAm : kDim) << b << kReset;
            }
            buf << "\x1b[K\r\n";
        }

        // Row 6: Horizontal rule
        {
            buf << kDim;
            for (size_t i = 0; i < W; ++i) buf << "\xe2\x94\x80";
            buf << kReset << "\r\n";
        }

        // Matrix body rows (centered scrolling matching Artboard 7)
        const int header_rows = 6;
        const int footer_rows = 6;
        const int visible_rows = std::max(1, H - header_rows - footer_rows);
        const size_t vis_u = static_cast<size_t>(visible_rows);

        size_t start_row = 0;
        if (visible_skills.size() > vis_u) {
            size_t half = vis_u / 2U;
            if (cursor_row > half) {
                start_row = std::min(cursor_row - half, visible_skills.size() - vis_u);
            }
        }

        for (int r = 0; r < visible_rows; ++r) {
            size_t row_idx = start_row + static_cast<size_t>(r);
            if (row_idx >= visible_skills.size()) {
                buf << "\x1b[K\r\n";
                continue;
            }
            const auto& skill = snap.skills[visible_skills[row_idx]];
            const bool sel = (row_idx == cursor_row);
            const bool iss = skill_needs_attention(skill);

            if (sel) buf << kSelBg;
            const char* lead = sel ? "\xe2\x96\xb8 " : "  "; // ▸ or spaces
            buf << (sel ? kBold : kReset);
            if (sel) buf << kSelBg;
            buf << lead << pad_ascii(skill.slug, name_w);
            buf << kDim;
            if (sel) buf << kSelBg;
            buf << pad_ascii("~" + std::to_string(skill.approx_tokens), static_cast<size_t>(kTokColWidth));

            size_t used_w = fixed_left;
            for (size_t ci = scroll_col; ci < end_col; ++ci) {
                CellState st = resolve_cell_state(skill, snap.targets[ci]);
                std::string_view g = cell_glyph(st);
                if (sel && ci == cursor_col) {
                    buf << kAcc << "  " << g << "  " << kReset;
                    if (sel) buf << kSelBg;
                } else {
                    buf << cell_color(st);
                    if (sel) buf << kSelBg;
                    buf << "  " << g << "  ";
                }
                used_w += static_cast<size_t>(kCellWidth);
            }

            if (is_wide && W > used_w + 2U) {
                size_t desc_w = W - used_w - 2U;
                buf << (iss ? kAm : kDim);
                if (sel) buf << kSelBg;
                buf << "  " << pad_ascii(skill.description, desc_w);
            } else if (W > used_w) {
                buf << std::string(W - used_w, ' ');
            }
            buf << kReset << "\x1b[K\r\n";
        }

        // Footer Line 1: Horizontal rule
        {
            buf << kDim;
            for (size_t i = 0; i < W; ++i) buf << "\xe2\x94\x80";
            buf << kReset << "\r\n";
        }

        // Footer Lines 2-4: Selected cell inspector
        if (!visible_skills.empty() && !snap.targets.empty()) {
            const auto& cs = snap.skills[visible_skills[cursor_row]];
            const auto& ct = snap.targets[cursor_col];
            const CellState st = resolve_cell_state(cs, ct);

            // Line 2: slug · target · state
            std::string d1 = " " + cs.slug + " \xc2\xb7 " + ct.display_name + " \xc2\xb7 " + std::string(cell_label(st));
            buf << kBold << pad_ascii(d1, W) << kReset << "\r\n";

            // Line 3: symlink/path arrow
            std::string target_path = singlet::core::contract_home_path(ct.skills_dir / cs.slug, engine.home_dir());
            std::string canon_path = singlet::core::contract_home_path(snap.canonical_root / cs.slug, engine.home_dir());
            std::string d2;
            if (st == CellState::Broken) {
                d2 = " " + target_path + " -> (dangling symlink)";
            } else if (st == CellState::On || st == CellState::Frontmatter) {
                d2 = " " + target_path + " -> " + canon_path;
            } else if (st == CellState::Off) {
                d2 = " " + target_path + " (not linked)";
            } else if (st == CellState::Shadow) {
                d2 = " " + target_path + " (shadowed by plugin)";
            } else {
                d2 = " " + target_path + " (physical folder)";
            }
            buf << kDim << pad_ascii(d2, W) << kReset << "\r\n";

            // Line 4: status message or contextual hint
            std::string d3;
            std::string_view d3_col = kDim;
            if (!status_msg.empty()) {
                d3 = " " + status_msg;
                d3_col = kEm;
            } else if (!cs.antigravity_valid) {
                d3 = " ! frontmatter: missing/invalid name or description, strict targets drop it  (f fix)";
                d3_col = kAm;
            } else if (st == CellState::Drift) {
                d3 = " ~ drifted copy: press c to consolidate or open singlet ui for diff resolver";
                d3_col = kAm;
            } else if (st == CellState::Copy) {
                d3 = " * unmanaged physical copy: press c to consolidate into canonical store";
                d3_col = kAm;
            } else {
                d3 = " " + cs.description;
                d3_col = kDim;
            }
            buf << d3_col << pad_ascii(d3, W) << kReset << "\r\n";
        } else {
            buf << kDim << pad_ascii(" No matching skills in current view.", W) << kReset << "\r\n";
            buf << "\x1b[K\r\n";
            buf << (status_msg.empty() ? kDim : kEm) << pad_ascii(" " + status_msg, W) << kReset << "\r\n";
        }

        // Footer Line 5: blank spacer
        buf << "\x1b[K\r\n";

        // Footer Line 6: Inverted shortcut bar
        {
            std::string_view keys = is_wide
                ? " j/k skill  h/l target  Space toggle  c consolidate  f fix frontmatter  u undo  / search  p profile  s scan  q quit "
                : " j/k h/l move  Space toggle  c dedup  f fix  u undo  / search  s scan  q quit ";
            buf << kInv << pad_ascii(keys, W) << kReset;
        }

        std::string frame = buf.str();
        write_all_fd(STDOUT_FILENO, frame.data(), frame.size());
    };

    render();

    while (!g_tui_stop) {
        if (g_tui_resized) {
            g_tui_resized = 0;
            render();
        }

        KeyEvent ev = read_key_event();
        if (ev.key == Key::None) {
            continue;
        }
        if (ev.key == Key::CtrlC) {
            break;
        }

        if (search_mode) {
            if (ev.key == Key::Esc) {
                search_mode = false;
                search_query.clear();
                cursor_row = 0;
                status_msg = "search cleared";
            } else if (ev.key == Key::Enter) {
                search_mode = false;
                cursor_row = 0;
                status_msg = search_query.empty() ? "search cleared" : ("filtered by /" + search_query);
            } else if (ev.key == Key::Backspace) {
                if (!search_query.empty()) search_query.pop_back();
                cursor_row = 0;
            } else if (ev.key == Key::Char && std::isprint(static_cast<unsigned char>(ev.ch))) {
                search_query.push_back(ev.ch);
                cursor_row = 0;
            }
            render();
            continue;
        }

        auto visible_skills = compute_visible_indices();
        status_msg.clear();

        if (ev.key == Key::Esc || (ev.key == Key::Char && ev.ch == 'q')) {
            break;
        } else if (ev.key == Key::Down || (ev.key == Key::Char && ev.ch == 'j')) {
            if (!visible_skills.empty() && cursor_row + 1 < visible_skills.size()) cursor_row++;
        } else if (ev.key == Key::Up || (ev.key == Key::Char && ev.ch == 'k')) {
            if (cursor_row > 0) cursor_row--;
        } else if (ev.key == Key::Right || (ev.key == Key::Char && ev.ch == 'l')) {
            if (!snap.targets.empty() && cursor_col + 1 < snap.targets.size()) cursor_col++;
        } else if (ev.key == Key::Left || (ev.key == Key::Char && ev.ch == 'h')) {
            if (cursor_col > 0) cursor_col--;
        } else if (ev.key == Key::Char && ev.ch == 'a') {
            active_filter = FilterKind::All;
            cursor_row = 0;
        } else if (ev.key == Key::Char && ev.ch == '!') {
            active_filter = FilterKind::Attention;
            cursor_row = 0;
        } else if (ev.key == Key::Char && ev.ch == '~') {
            active_filter = FilterKind::Drift;
            cursor_row = 0;
        } else if (ev.key == Key::Char && ev.ch == 'x') {
            active_filter = FilterKind::Broken;
            cursor_row = 0;
        } else if (ev.key == Key::Char && ev.ch == '/') {
            search_mode = true;
            status_msg = "type to filter skills, Enter to confirm, Esc to clear";
        } else if (ev.key == Key::Space || ev.key == Key::Enter) {
            if (!visible_skills.empty() && !snap.targets.empty()) {
                const auto& skill = snap.skills[visible_skills[cursor_row]];
                const auto& tgt = snap.targets[cursor_col];
                CellState st = resolve_cell_state(skill, tgt);
                bool next_enabled = (st == CellState::Off || st == CellState::Broken);
                auto res = engine.toggle_skill(skill.slug, tgt.id, next_enabled);
                if (res.ok) {
                    status_msg = std::string(st == CellState::Broken ? "repaired " : (next_enabled ? "linked " : "unlinked ")) +
                                 skill.slug + " in " + tgt.display_name;
                    snap = engine.get_catalog();
                } else {
                    status_msg = "toggle error: " + res.error;
                }
            }
        } else if (ev.key == Key::Char && ev.ch == 'f') {
            if (!visible_skills.empty()) {
                const auto& skill = snap.skills[visible_skills[cursor_row]];
                auto res = engine.fix_frontmatter(skill.slug, true);
                if (res.ok && res.changed) {
                    status_msg = "frontmatter fixed for " + skill.slug;
                    snap = engine.scan();
                } else if (res.ok) {
                    status_msg = "frontmatter already valid for " + skill.slug;
                } else {
                    status_msg = "fix error: " + res.error;
                }
            }
        } else if (ev.key == Key::Char && ev.ch == 'c') {
            auto res = engine.consolidate(false);
            if (res.ok) {
                status_msg = "consolidated: " + std::to_string(res.plan.skills_promoted) + " promoted, " +
                             std::to_string(res.plan.copies_quarantined) + " quarantined, " +
                             std::to_string(res.plan.symlinks_created) + " linked";
                snap = engine.scan();
            } else {
                status_msg = "consolidate error: " + res.error;
            }
        } else if (ev.key == Key::Char && ev.ch == 'u') {
            auto res = engine.undo_last_quarantine();
            if (res.ok) {
                status_msg = res.message.empty() ? "undo completed" : res.message;
                snap = engine.scan();
            } else {
                status_msg = "undo error: " + res.error;
            }
        } else if (ev.key == Key::Char && (ev.ch == 's' || ev.ch == 'r')) {
            snap = engine.scan();
            std::ostringstream oss;
            oss << "scanned in " << std::fixed << std::setprecision(1) << snap.last_scan_ms << " ms";
            status_msg = oss.str();
        } else if (ev.key == Key::Char && ev.ch == 'p') {
            auto profiles = engine.get_profiles();
            if (profiles.empty()) {
                auto saved = engine.save_profile("default");
                if (saved.ok) {
                    active_profile = "default";
                    status_msg = "saved current wiring as profile 'default'";
                } else {
                    status_msg = "profile error: " + saved.error;
                }
            } else {
                size_t cur_idx = 0;
                bool found_cur = false;
                for (size_t i = 0; i < profiles.size(); ++i) {
                    if (profiles[i].name == active_profile) {
                        cur_idx = i;
                        found_cur = true;
                        break;
                    }
                }
                size_t next_idx = found_cur ? ((cur_idx + 1U) % profiles.size()) : 0U;
                const std::string& next_name = profiles[next_idx].name;
                auto applied = engine.apply_profile(next_name);
                if (applied.ok) {
                    active_profile = next_name;
                    snap = engine.get_catalog();
                    status_msg = "applied profile '" + next_name + "'";
                } else {
                    status_msg = "profile error: " + applied.error;
                }
            }
        }

        render();
    }

    guard.restore();
    return 0;
}

} // namespace singlet::surfaces
