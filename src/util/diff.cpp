#include "util/diff.hpp"

#include <algorithm>
#include <cstddef>
#include <string>
#include <vector>

namespace singlet::util {

namespace {

std::vector<std::string> split_lines(std::string_view text) {
    std::vector<std::string> lines;
    if (text.empty()) {
        return lines;
    }
    size_t pos = 0;
    while (pos < text.size()) {
        size_t nl = text.find('\n', pos);
        if (nl == std::string_view::npos) {
            std::string_view line = text.substr(pos);
            if (!line.empty() && line.back() == '\r') {
                line.remove_suffix(1);
            }
            lines.emplace_back(line);
            break;
        }
        std::string_view line = text.substr(pos, nl - pos);
        if (!line.empty() && line.back() == '\r') {
            line.remove_suffix(1);
        }
        lines.emplace_back(line);
        pos = nl + 1;
    }
    return lines;
}

struct MyersTrace {
    std::vector<std::vector<int>> snapshots;
    int edit_distance = -1;
};

MyersTrace compute_myers_trace(
    const std::vector<std::string>& left_lines,
    const std::vector<std::string>& right_lines,
    size_t prefix,
    int n,
    int m
) {
    constexpr int kHardMaxD = 8192;
    const int limit_d = std::min(n + m, kHardMaxD);

    MyersTrace res;
    res.snapshots.reserve(static_cast<size_t>(std::min(limit_d + 1, 256)));

    std::vector<int> v(static_cast<size_t>(2 * limit_d + 3), 0);
    const int offset = limit_d + 1;

    for (int d = 0; d <= limit_d; ++d) {
        std::vector<int> snap(static_cast<size_t>(2 * d + 1));
        for (int k = -d; k <= d; ++k) {
            snap[static_cast<size_t>(k + d)] = v[static_cast<size_t>(offset + k)];
        }
        res.snapshots.push_back(std::move(snap));

        for (int k = -d; k <= d; k += 2) {
            const bool move_down =
                k == -d ||
                (k != d && v[static_cast<size_t>(offset + k - 1)] < v[static_cast<size_t>(offset + k + 1)]);
            int x = move_down
                ? v[static_cast<size_t>(offset + k + 1)]
                : v[static_cast<size_t>(offset + k - 1)] + 1;
            int y = x - k;

            while (x < n && y < m &&
                   left_lines[prefix + static_cast<size_t>(x)] ==
                   right_lines[prefix + static_cast<size_t>(y)]) {
                ++x;
                ++y;
            }
            v[static_cast<size_t>(offset + k)] = x;
            if (x >= n && y >= m) {
                res.edit_distance = d;
                return res;
            }
        }
    }
    return res;
}

std::vector<DiffLine> backtrack_myers(
    const MyersTrace& trace,
    const std::vector<std::string>& left_lines,
    const std::vector<std::string>& right_lines,
    size_t prefix,
    int n,
    int m
) {
    std::vector<DiffLine> mid_rev;
    mid_rev.reserve(static_cast<size_t>(n + m));

    int x = n;
    int y = m;
    for (int d = trace.edit_distance; d > 0; --d) {
        const auto& prev_v = trace.snapshots[static_cast<size_t>(d)];
        auto get_prev = [&](int diag) -> int {
            return prev_v[static_cast<size_t>(diag + d)];
        };

        const int k = x - y;
        const bool from_up =
            k == -d || (k != d && get_prev(k - 1) < get_prev(k + 1));
        const int prev_k = from_up ? (k + 1) : (k - 1);
        const int prev_x = get_prev(prev_k);
        const int prev_y = prev_x - prev_k;

        while (x > prev_x && y > prev_y) {
            --x;
            --y;
            const size_t l_idx = prefix + static_cast<size_t>(x);
            const size_t r_idx = prefix + static_cast<size_t>(y);
            mid_rev.push_back(DiffLine{DiffOp::Equal, left_lines[l_idx], l_idx + 1, r_idx + 1});
        }

        if (x == prev_x) {
            --y;
            const size_t r_idx = prefix + static_cast<size_t>(y);
            mid_rev.push_back(DiffLine{DiffOp::Insert, right_lines[r_idx], 0, r_idx + 1});
        } else {
            --x;
            const size_t l_idx = prefix + static_cast<size_t>(x);
            mid_rev.push_back(DiffLine{DiffOp::Delete, left_lines[l_idx], l_idx + 1, 0});
        }
    }

    while (x > 0 && y > 0) {
        --x;
        --y;
        const size_t l_idx = prefix + static_cast<size_t>(x);
        const size_t r_idx = prefix + static_cast<size_t>(y);
        mid_rev.push_back(DiffLine{DiffOp::Equal, left_lines[l_idx], l_idx + 1, r_idx + 1});
    }

    std::reverse(mid_rev.begin(), mid_rev.end());
    return mid_rev;
}

} // namespace

std::vector<DiffLine> myers_diff_lines(
    const std::vector<std::string>& left_lines,
    const std::vector<std::string>& right_lines
) {
    const size_t N_total = left_lines.size();
    const size_t M_total = right_lines.size();

    size_t prefix = 0;
    while (prefix < N_total && prefix < M_total && left_lines[prefix] == right_lines[prefix]) {
        ++prefix;
    }

    size_t suffix = 0;
    while (suffix < (N_total - prefix) &&
           suffix < (M_total - prefix) &&
           left_lines[N_total - 1 - suffix] == right_lines[M_total - 1 - suffix]) {
        ++suffix;
    }

    const int n = static_cast<int>(N_total - prefix - suffix);
    const int m = static_cast<int>(M_total - prefix - suffix);

    std::vector<DiffLine> result;
    result.reserve(N_total + M_total);

    for (size_t i = 0; i < prefix; ++i) {
        result.push_back(DiffLine{DiffOp::Equal, left_lines[i], i + 1, i + 1});
    }

    if (n == 0) {
        for (int j = 0; j < m; ++j) {
            const size_t r_idx = prefix + static_cast<size_t>(j);
            result.push_back(DiffLine{DiffOp::Insert, right_lines[r_idx], 0, r_idx + 1});
        }
    } else if (m == 0) {
        for (int i = 0; i < n; ++i) {
            const size_t l_idx = prefix + static_cast<size_t>(i);
            result.push_back(DiffLine{DiffOp::Delete, left_lines[l_idx], l_idx + 1, 0});
        }
    } else {
        auto trace = compute_myers_trace(left_lines, right_lines, prefix, n, m);
        if (trace.edit_distance < 0) {
            for (int i = 0; i < n; ++i) {
                const size_t l_idx = prefix + static_cast<size_t>(i);
                result.push_back(DiffLine{DiffOp::Delete, left_lines[l_idx], l_idx + 1, 0});
            }
            for (int j = 0; j < m; ++j) {
                const size_t r_idx = prefix + static_cast<size_t>(j);
                result.push_back(DiffLine{DiffOp::Insert, right_lines[r_idx], 0, r_idx + 1});
            }
        } else {
            auto mid = backtrack_myers(trace, left_lines, right_lines, prefix, n, m);
            for (auto& dl : mid) {
                result.push_back(std::move(dl));
            }
        }
    }

    for (size_t s = 0; s < suffix; ++s) {
        const size_t l_idx = N_total - suffix + s;
        const size_t r_idx = M_total - suffix + s;
        result.push_back(DiffLine{DiffOp::Equal, left_lines[l_idx], l_idx + 1, r_idx + 1});
    }

    return result;
}

std::string unified_diff(
    std::string_view left,
    std::string_view right,
    std::string_view left_label,
    std::string_view right_label,
    int context_lines
) {
    if (context_lines < 0) {
        context_lines = 0;
    }
    auto left_lines = split_lines(left);
    auto right_lines = split_lines(right);
    if (left_lines == right_lines) {
        return {};
    }

    auto ops = myers_diff_lines(left_lines, right_lines);

    // Collect edit ranges [start, end) expanded by context_lines, merging overlaps.
    struct HunkRange {
        size_t start;
        size_t end;
    };
    std::vector<HunkRange> hunks;
    const size_t ctx = static_cast<size_t>(context_lines);

    size_t i = 0;
    while (i < ops.size()) {
        if (ops[i].op == DiffOp::Equal) {
            ++i;
            continue;
        }
        size_t edit_start = i;
        while (i < ops.size() && ops[i].op != DiffOp::Equal) {
            ++i;
        }
        size_t edit_end = i;

        size_t h_start = edit_start > ctx ? edit_start - ctx : 0;
        size_t h_end = std::min(ops.size(), edit_end + ctx);

        if (!hunks.empty() && h_start <= hunks.back().end) {
            hunks.back().end = h_end;
        } else {
            hunks.push_back(HunkRange{h_start, h_end});
        }
    }

    if (hunks.empty()) {
        return {};
    }

    std::string out;
    out += "--- ";
    out.append(left_label);
    out += "\n+++ ";
    out.append(right_label);
    out += "\n";

    for (const auto& h : hunks) {
        size_t l_count = 0;
        size_t r_count = 0;
        size_t l_first = 0;
        size_t r_first = 0;

        for (size_t idx = h.start; idx < h.end; ++idx) {
            const auto& dl = ops[idx];
            if (dl.op == DiffOp::Equal || dl.op == DiffOp::Delete) {
                if (l_first == 0) l_first = dl.left_line;
                ++l_count;
            }
            if (dl.op == DiffOp::Equal || dl.op == DiffOp::Insert) {
                if (r_first == 0) r_first = dl.right_line;
                ++r_count;
            }
        }

        // Standard unified diff 0-count start position calculation.
        if (l_count == 0) {
            // Find last left line before h.start.
            size_t prev_l = 0;
            for (size_t idx = h.start; idx > 0; --idx) {
                if (ops[idx - 1].left_line != 0) {
                    prev_l = ops[idx - 1].left_line;
                    break;
                }
            }
            l_first = prev_l;
        }
        if (r_count == 0) {
            size_t prev_r = 0;
            for (size_t idx = h.start; idx > 0; --idx) {
                if (ops[idx - 1].right_line != 0) {
                    prev_r = ops[idx - 1].right_line;
                    break;
                }
            }
            r_first = prev_r;
        }

        out += "@@ -";
        out += std::to_string(l_first);
        out += ",";
        out += std::to_string(l_count);
        out += " +";
        out += std::to_string(r_first);
        out += ",";
        out += std::to_string(r_count);
        out += " @@\n";

        for (size_t idx = h.start; idx < h.end; ++idx) {
            const auto& dl = ops[idx];
            switch (dl.op) {
                case DiffOp::Equal:  out.push_back(' '); break;
                case DiffOp::Delete: out.push_back('-'); break;
                case DiffOp::Insert: out.push_back('+'); break;
            }
            out += dl.text;
            out.push_back('\n');
        }
    }

    return out;
}

} // namespace singlet::util
