#include "util/sha256.hpp"

#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>

#include <algorithm>
#include <cstring>
#include <fstream>
#include <set>
#include <utility>
#include <vector>

namespace loadout::util {

namespace {

constexpr uint32_t kRoundConstants[64] = {
    0x428a2f98u, 0x71374491u, 0xb5c0fbcfu, 0xe9b5dba5u,
    0x3956c25bu, 0x59f111f1u, 0x923f82a4u, 0xab1c5ed5u,
    0xd807aa98u, 0x12835b01u, 0x243185beu, 0x550c7dc3u,
    0x72be5d74u, 0x80deb1feu, 0x9bdc06a7u, 0xc19bf174u,
    0xe49b69c1u, 0xefbe4786u, 0x0fc19dc6u, 0x240ca1ccu,
    0x2de92c6fu, 0x4a7484aau, 0x5cb0a9dcu, 0x76f988dau,
    0x983e5152u, 0xa831c66du, 0xb00327c8u, 0xbf597fc7u,
    0xc6e00bf3u, 0xd5a79147u, 0x06ca6351u, 0x14292967u,
    0x27b70a85u, 0x2e1b2138u, 0x4d2c6dfcu, 0x53380d13u,
    0x650a7354u, 0x766a0abbu, 0x81c2c92eu, 0x92722c85u,
    0xa2bfe8a1u, 0xa81a664bu, 0xc24b8b70u, 0xc76c51a3u,
    0xd192e819u, 0xd6990624u, 0xf40e3585u, 0x106aa070u,
    0x19a4c116u, 0x1e376c08u, 0x2748774cu, 0x34b0bcb5u,
    0x391c0cb3u, 0x4ed8aa4au, 0x5b9cca4fu, 0x682e6ff3u,
    0x748f82eeu, 0x78a5636fu, 0x84c87814u, 0x8cc70208u,
    0x90befffau, 0xa4506cebu, 0xbef9a3f7u, 0xc67178f2u,
};

inline uint32_t rotr32(uint32_t x, uint32_t n) noexcept {
    return (x >> n) | (x << (32u - n));
}

std::string bytes_to_hex(const std::array<uint8_t, 32>& digest) {
    static constexpr char kHex[] = "0123456789abcdef";
    std::string out;
    out.resize(64);
    for (size_t i = 0; i < 32; ++i) {
        out[i * 2] = kHex[(digest[i] >> 4) & 0x0Fu];
        out[i * 2 + 1] = kHex[digest[i] & 0x0Fu];
    }
    return out;
}

bool is_ignored_tree_name(std::string_view name) noexcept {
    return name == "." || name == ".." || name == ".DS_Store" || name == "Thumbs.db";
}

struct TreeEntry {
    std::string rel_path;
    bool is_symlink = false;
    uint64_t size = 0;
    std::string hash_hex;
};

struct DirGuard {
    DIR* dir = nullptr;

    explicit DirGuard(const std::filesystem::path& path) noexcept
        : dir(::opendir(path.c_str())) {}
    ~DirGuard() noexcept {
        if (dir) {
            ::closedir(dir);
        }
    }

    DirGuard(const DirGuard&) = delete;
    DirGuard& operator=(const DirGuard&) = delete;
};

void walk_skill_tree(
    const std::filesystem::path& current_dir,
    const std::string& rel_prefix,
    std::set<std::pair<dev_t, ino_t>>& visited_dirs,
    std::vector<TreeEntry>& entries
) {
    DirGuard guard(current_dir);
    if (!guard.dir) {
        return;
    }

    struct dirent* de = nullptr;
    while ((de = ::readdir(guard.dir)) != nullptr) {
        std::string_view name(de->d_name);
        if (is_ignored_tree_name(name)) {
            continue;
        }

        std::filesystem::path child_path = current_dir / name;
        std::string child_rel = rel_prefix.empty()
            ? std::string(name)
            : (rel_prefix + "/" + std::string(name));

        struct stat st {};
        if (::lstat(child_path.c_str(), &st) != 0) {
            continue;
        }

        if (S_ISDIR(st.st_mode)) {
            auto key = std::make_pair(st.st_dev, st.st_ino);
            if (visited_dirs.insert(key).second) {
                walk_skill_tree(child_path, child_rel, visited_dirs, entries);
            }
        } else if (S_ISREG(st.st_mode)) {
            auto fh = sha256_file(child_path);
            if (!fh) {
                continue;
            }
            TreeEntry entry;
            entry.rel_path = std::move(child_rel);
            entry.is_symlink = false;
            entry.size = static_cast<uint64_t>(st.st_size < 0 ? 0 : st.st_size);
            entry.hash_hex = std::move(*fh);
            entries.push_back(std::move(entry));
        } else if (S_ISLNK(st.st_mode)) {
            std::string link_target(4096, '\0');
            ssize_t n = ::readlink(child_path.c_str(), link_target.data(), link_target.size());
            if (n >= 0) {
                link_target.resize(static_cast<size_t>(n));
            } else {
                link_target.clear();
            }
            TreeEntry entry;
            entry.rel_path = std::move(child_rel);
            entry.is_symlink = true;
            entry.size = static_cast<uint64_t>(link_target.size());
            entry.hash_hex = sha256_str(link_target);
            entries.push_back(std::move(entry));
        }
    }
}

} // namespace

Sha256::Sha256() noexcept {
    reset();
}

void Sha256::reset() noexcept {
    state_[0] = 0x6a09e667u;
    state_[1] = 0xbb67ae85u;
    state_[2] = 0x3c6ef372u;
    state_[3] = 0xa54ff53au;
    state_[4] = 0x510e527fu;
    state_[5] = 0x9b05688cu;
    state_[6] = 0x1f83d9abu;
    state_[7] = 0x5be0cd19u;
    bit_count_ = 0;
    buffer_len_ = 0;
}

void Sha256::transform(const uint8_t block[64]) noexcept {
    uint32_t w[64];
    for (size_t i = 0; i < 16; ++i) {
        w[i] = (static_cast<uint32_t>(block[i * 4]) << 24) |
               (static_cast<uint32_t>(block[i * 4 + 1]) << 16) |
               (static_cast<uint32_t>(block[i * 4 + 2]) << 8) |
               (static_cast<uint32_t>(block[i * 4 + 3]));
    }
    for (size_t i = 16; i < 64; ++i) {
        uint32_t s0 = rotr32(w[i - 15], 7) ^ rotr32(w[i - 15], 18) ^ (w[i - 15] >> 3);
        uint32_t s1 = rotr32(w[i - 2], 17) ^ rotr32(w[i - 2], 19) ^ (w[i - 2] >> 10);
        w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }

    uint32_t a = state_[0];
    uint32_t b = state_[1];
    uint32_t c = state_[2];
    uint32_t d = state_[3];
    uint32_t e = state_[4];
    uint32_t f = state_[5];
    uint32_t g = state_[6];
    uint32_t h = state_[7];

    for (size_t i = 0; i < 64; ++i) {
        uint32_t S1 = rotr32(e, 6) ^ rotr32(e, 11) ^ rotr32(e, 25);
        uint32_t ch = (e & f) ^ ((~e) & g);
        uint32_t temp1 = h + S1 + ch + kRoundConstants[i] + w[i];
        uint32_t S0 = rotr32(a, 2) ^ rotr32(a, 13) ^ rotr32(a, 22);
        uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
        uint32_t temp2 = S0 + maj;

        h = g;
        g = f;
        f = e;
        e = d + temp1;
        d = c;
        c = b;
        b = a;
        a = temp1 + temp2;
    }

    state_[0] += a;
    state_[1] += b;
    state_[2] += c;
    state_[3] += d;
    state_[4] += e;
    state_[5] += f;
    state_[6] += g;
    state_[7] += h;
}

void Sha256::update(std::span<const uint8_t> data) noexcept {
    update(data.data(), data.size());
}

void Sha256::update(const void* data, size_t len) noexcept {
    if (!data || len == 0) return;
    const auto* bytes = static_cast<const uint8_t*>(data);
    bit_count_ += static_cast<uint64_t>(len) * 8u;

    size_t offset = 0;
    if (buffer_len_ > 0) {
        size_t take = std::min(64 - buffer_len_, len);
        std::memcpy(buffer_ + buffer_len_, bytes, take);
        buffer_len_ += take;
        offset += take;
        if (buffer_len_ == 64) {
            transform(buffer_);
            buffer_len_ = 0;
        }
    }

    while (offset + 64 <= len) {
        transform(bytes + offset);
        offset += 64;
    }

    if (offset < len) {
        size_t rem = len - offset;
        std::memcpy(buffer_, bytes + offset, rem);
        buffer_len_ = rem;
    }
}

void Sha256::update(std::string_view text) noexcept {
    update(text.data(), text.size());
}

std::array<uint8_t, 32> Sha256::finalize() noexcept {
    buffer_[buffer_len_++] = 0x80u;
    if (buffer_len_ > 56) {
        std::memset(buffer_ + buffer_len_, 0, 64 - buffer_len_);
        transform(buffer_);
        buffer_len_ = 0;
    }
    std::memset(buffer_ + buffer_len_, 0, 56 - buffer_len_);

    for (int i = 0; i < 8; ++i) {
        buffer_[56 + i] = static_cast<uint8_t>((bit_count_ >> (56 - i * 8)) & 0xFFu);
    }
    transform(buffer_);

    std::array<uint8_t, 32> digest{};
    for (size_t i = 0; i < 8; ++i) {
        digest[i * 4]     = static_cast<uint8_t>((state_[i] >> 24) & 0xFFu);
        digest[i * 4 + 1] = static_cast<uint8_t>((state_[i] >> 16) & 0xFFu);
        digest[i * 4 + 2] = static_cast<uint8_t>((state_[i] >> 8) & 0xFFu);
        digest[i * 4 + 3] = static_cast<uint8_t>(state_[i] & 0xFFu);
    }
    reset();
    return digest;
}

std::string Sha256::finalize_hex() {
    return bytes_to_hex(finalize());
}

std::string sha256_hex(std::span<const uint8_t> data) {
    Sha256 ctx;
    ctx.update(data);
    return ctx.finalize_hex();
}

std::string sha256_str(std::string_view text) {
    Sha256 ctx;
    ctx.update(text);
    return ctx.finalize_hex();
}

std::optional<std::string> sha256_file(const std::filesystem::path& path) {
    std::ifstream ifs(path, std::ios::binary);
    if (!ifs.is_open()) {
        return std::nullopt;
    }
    Sha256 ctx;
    constexpr size_t kChunkSize = 65536;
    char buf[kChunkSize]{};
    while (ifs) {
        ifs.read(buf, static_cast<std::streamsize>(kChunkSize));
        std::streamsize n = ifs.gcount();
        if (n > 0) {
            ctx.update(buf, static_cast<size_t>(n));
        }
    }
    if (ifs.bad()) {
        return std::nullopt;
    }
    return ctx.finalize_hex();
}

TreeDigest hash_skill_tree(const std::filesystem::path& dir) {
    TreeDigest result{};
    struct stat root_st {};
    if (::stat(dir.c_str(), &root_st) != 0 || !S_ISDIR(root_st.st_mode)) {
        return result;
    }

    std::set<std::pair<dev_t, ino_t>> visited_dirs;
    visited_dirs.insert(std::make_pair(root_st.st_dev, root_st.st_ino));

    std::vector<TreeEntry> entries;
    walk_skill_tree(dir, "", visited_dirs, entries);

    std::sort(entries.begin(), entries.end(), [](const TreeEntry& a, const TreeEntry& b) {
        return a.rel_path < b.rel_path;
    });

    Sha256 merkle;
    constexpr char kNul = '\0';
    for (const auto& entry : entries) {
        merkle.update(entry.is_symlink ? "L" : "F");
        merkle.update(&kNul, 1);
        merkle.update(entry.rel_path);
        merkle.update(&kNul, 1);
        merkle.update(std::to_string(entry.size));
        merkle.update(&kNul, 1);
        merkle.update(entry.hash_hex);
        merkle.update("\n");

        ++result.file_count;
        result.total_bytes += entry.size;
    }

    result.hex_hash = merkle.finalize_hex();
    return result;
}

} // namespace loadout::util
