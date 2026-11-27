#pragma once

/// Pure C++20 Myers O(ND) line diff and unified diff formatter.

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace singlet::util {

/// Edit operation type for a single line in a diff script.
enum class DiffOp {
    Equal,
    Delete,
    Insert,
};

/// Single entry in a line-level shortest edit script with 1-based line numbers.
struct DiffLine {
    DiffOp op = DiffOp::Equal;
    std::string text;
    size_t left_line = 0;  // 1-based line number in left, or 0 for Insert
    size_t right_line = 0; // 1-based line number in right, or 0 for Delete
};

/// Computes the Myers O(ND) shortest edit script between two line sequences.
[[nodiscard]] std::vector<DiffLine> myers_diff_lines(
    const std::vector<std::string>& left_lines,
    const std::vector<std::string>& right_lines
);

/// Produces a standard unified diff string between left and right, or empty string when equal.
[[nodiscard]] std::string unified_diff(
    std::string_view left,
    std::string_view right,
    std::string_view left_label = "a",
    std::string_view right_label = "b",
    int context_lines = 3
);

} // namespace singlet::util
