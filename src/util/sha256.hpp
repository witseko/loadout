#pragma once

/// Zero-dependency FIPS 180-4 SHA-256 and deterministic skill directory Merkle hashing.

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <string_view>

namespace singlet::util {

/// Incremental SHA-256 streaming hasher; finalize() resets state for reuse.
class Sha256 {
public:
    Sha256() noexcept;

    void reset() noexcept;
    void update(std::span<const uint8_t> data) noexcept;
    void update(const void* data, size_t len) noexcept;
    void update(std::string_view text) noexcept;

    [[nodiscard]] std::array<uint8_t, 32> finalize() noexcept;
    [[nodiscard]] std::string finalize_hex();

private:
    void transform(const uint8_t block[64]) noexcept;

    uint32_t state_[8]{};
    uint64_t bit_count_ = 0;
    uint8_t buffer_[64]{};
    size_t buffer_len_ = 0;
};

/// Deterministic Merkle digest and byte/file totals for a skill directory tree.
struct TreeDigest {
    std::string hex_hash;
    size_t file_count = 0;
    uint64_t total_bytes = 0;
};

/// Computes lowercase hex SHA-256 digest of a byte span.
[[nodiscard]] std::string sha256_hex(std::span<const uint8_t> data);

/// Computes lowercase hex SHA-256 digest of a UTF-8 string view.
[[nodiscard]] std::string sha256_str(std::string_view text);

/// Computes lowercase hex SHA-256 digest of a file on disk, or std::nullopt if unreadable.
[[nodiscard]] std::optional<std::string> sha256_file(const std::filesystem::path& path);

/// Computes a deterministic sorted relative-path and content Merkle hash for dir.
/// Ignores .DS_Store and Thumbs.db and protects against symlink loops.
/// Returns empty hex_hash if dir does not exist or is not a directory.
[[nodiscard]] TreeDigest hash_skill_tree(const std::filesystem::path& dir);

} // namespace singlet::util
