#include "util/diff.hpp"
#include "util/frontmatter.hpp"
#include "util/json.hpp"
#include "util/sha256.hpp"

#include <unistd.h>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

int g_failures = 0;
int g_assertions = 0;

void check(bool cond, const char* expr, int line) {
    ++g_assertions;
    if (!cond) {
        ++g_failures;
        std::cerr << "[FAIL] line " << line << ": " << expr << "\n";
    }
}

#define CHECK(expr) check((expr), #expr, __LINE__)

struct TempDir {
    std::filesystem::path path;

    TempDir() {
        std::string tmpl = "/tmp/loadout_test_util_XXXXXX";
        char* dir = ::mkdtemp(tmpl.data());
        if (!dir) {
            throw std::runtime_error("mkdtemp failed");
        }
        path = dir;
    }

    ~TempDir() {
        std::error_code ec;
        std::filesystem::remove_all(path, ec);
    }
};

void write_text_file(const std::filesystem::path& p, std::string_view content) {
    std::filesystem::create_directories(p.parent_path());
    std::ofstream ofs(p, std::ios::binary);
    ofs.write(content.data(), static_cast<std::streamsize>(content.size()));
}

void test_json() {
    using namespace loadout::util;

    // Basic construction & ergonomic helpers.
    JsonValue root = JsonValue::object();
    root["version"] = 1;
    root["enabled"] = true;
    root["ratio"] = 3.25;
    root["name"] = "loadout";
    root["empty"] = nullptr;
    root["count"] = size_t{42};
    root["tags"] = JsonValue::array({"testing", "browser"});
    root["tags"].push_back("cli");

    CHECK(root.is_object());
    CHECK(root.contains("version"));
    CHECK(!root.contains("missing"));
    CHECK(root.get_int("version") == 1);
    CHECK(root.get_int("count") == 42);
    CHECK(root.get_bool("enabled") == true);
    CHECK(root.get_bool("missing", true) == true);
    CHECK(root.get_double("ratio") == 3.25);
    CHECK(root.get_str("name") == "loadout");
    CHECK(root.get_str("missing", "fallback") == "fallback");
    CHECK(root["empty"].is_null());
    CHECK(root["tags"].is_array());
    CHECK(root["tags"].size() == 3);
    CHECK(root["tags"][0].as_string() == "testing");
    CHECK(root["tags"][size_t{2}].as_string() == "cli");
    CHECK(root["nonexistent"].is_null());

    // Roundtrip compact and pretty.
    std::string compact = to_json(root, false);
    std::string pretty = to_json(root, true);
    auto parsed_compact = parse_json(compact);
    auto parsed_pretty = parse_json(pretty);
    CHECK(parsed_compact.has_value());
    CHECK(parsed_pretty.has_value());
    if (parsed_compact && parsed_pretty) {
        CHECK(*parsed_compact == root);
        CHECK(*parsed_pretty == root);
    }

    // Unicode escapes including UTF-16 surrogate pair (rocket emoji U+1F680).
    auto unicode = parse_json(R"({"emoji":"\uD83D\uDE80","euro":"\u20AC","ctrl":"\n\t\"\\\/"})");
    CHECK(unicode.has_value());
    if (unicode) {
        CHECK(unicode->get_str("emoji") == "\xF0\x9F\x9A\x80");
        CHECK(unicode->get_str("euro") == "\xE2\x82\xAC");
        CHECK(unicode->get_str("ctrl") == "\n\t\"\\/");
        auto rt = parse_json(to_json(*unicode));
        CHECK(rt.has_value() && *rt == *unicode);
    }

    // JSONC parsing (opencode.jsonc style with //, /* */, strings with URLs, trailing commas).
    const std::string jsonc_doc = R"({
        // OpenCode configuration with custom skill paths
        "url": "https://opencode.ai/docs /* not a comment */",
        /* multi-line
           block comment */
        "skills": {
            "paths": [
                "~/.config/opencode/skills",
                "~/custom/skills", // trailing comma below!
            ],
        },
        "enabled": true,
    })";
    CHECK(!parse_json(jsonc_doc).has_value()); // strict JSON rejects comments/trailing commas
    auto jsonc = parse_jsonc(jsonc_doc);
    CHECK(jsonc.has_value());
    if (jsonc) {
        CHECK(jsonc->get_str("url") == "https://opencode.ai/docs /* not a comment */");
        CHECK((*jsonc)["skills"]["paths"].size() == 2);
        CHECK((*jsonc)["skills"]["paths"][0].as_string() == "~/.config/opencode/skills");
        CHECK((*jsonc)["skills"]["paths"][1].as_string() == "~/custom/skills");
        CHECK(jsonc->get_bool("enabled") == true);
    }

    // Strict JSON error cases.
    CHECK(!parse_json("").has_value());
    CHECK(!parse_json("01").has_value());
    CHECK(!parse_json("{\"a\":1} trailing").has_value());
    CHECK(!parse_json("{\"a\":1,}").has_value());
    CHECK(!parse_json("\"raw\nnewline\"").has_value());
    CHECK(!parse_json("\"\\uD83D\"").has_value());
    CHECK(!parse_jsonc("/* unterminated").has_value());

    // Erase & mutate.
    CHECK(root.erase("empty"));
    CHECK(!root.contains("empty"));
    CHECK(!root.erase("empty"));
}

void test_sha256_and_tree() {
    using namespace loadout::util;

    // NIST FIPS 180-4 test vectors.
    CHECK(sha256_str("") == "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
    CHECK(sha256_str("abc") == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
    CHECK(sha256_str("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq") ==
          "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1");

    // 1,000,000 'a' characters vector via incremental updates.
    {
        Sha256 ctx;
        std::string chunk(1000, 'a');
        for (int i = 0; i < 1000; ++i) {
            ctx.update(chunk);
        }
        CHECK(ctx.finalize_hex() ==
              "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0");
    }

    // Span overload.
    const uint8_t abc[] = {'a', 'b', 'c'};
    CHECK(sha256_hex(std::span<const uint8_t>(abc, 3)) ==
          "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");

    // File + Tree hashing in isolated temp dir.
    TempDir tmp;
    auto tree_a = tmp.path / "skill-a";
    auto tree_b = tmp.path / "skill-b";
    write_text_file(tree_a / "SKILL.md", "---\nname: demo\ndescription: hi\n---\nbody\n");
    write_text_file(tree_a / "scripts" / "run.sh", "#!/bin/sh\necho ok\n");
    write_text_file(tree_b / "SKILL.md", "---\nname: demo\ndescription: hi\n---\nbody\n");
    write_text_file(tree_b / "scripts" / "run.sh", "#!/bin/sh\necho ok\n");

    // Add OS junk files to tree_b: must be ignored!
    write_text_file(tree_b / ".DS_Store", "junk-ds-store");
    write_text_file(tree_b / "scripts" / "Thumbs.db", "junk-thumbs");

    auto file_hash = sha256_file(tree_a / "SKILL.md");
    CHECK(file_hash.has_value());
    CHECK(*file_hash == sha256_str("---\nname: demo\ndescription: hi\n---\nbody\n"));
    CHECK(!sha256_file(tmp.path / "does-not-exist").has_value());

    TreeDigest da = hash_skill_tree(tree_a);
    TreeDigest db = hash_skill_tree(tree_b);
    CHECK(!da.hex_hash.empty());
    CHECK(da.hex_hash == db.hex_hash);
    CHECK(da.file_count == 2);
    CHECK(db.file_count == 2);
    CHECK(da.total_bytes == db.total_bytes);

    // Mutate one byte in tree_b -> hash must diverge.
    write_text_file(tree_b / "scripts" / "run.sh", "#!/bin/sh\necho OK\n");
    TreeDigest db_mod = hash_skill_tree(tree_b);
    CHECK(db_mod.hex_hash != da.hex_hash);

    // Circular symlink inside tree must NOT hang.
    auto loop_tree = tmp.path / "loop-skill";
    write_text_file(loop_tree / "SKILL.md", "hello");
    std::filesystem::create_directory_symlink(loop_tree, loop_tree / "self_loop");
    TreeDigest dloop = hash_skill_tree(loop_tree);
    CHECK(!dloop.hex_hash.empty());
    CHECK(dloop.file_count == 2); // SKILL.md + symlink entry

    // Missing directory returns empty hex_hash.
    TreeDigest dmiss = hash_skill_tree(tmp.path / "nonexistent-dir");
    CHECK(dmiss.hex_hash.empty());
    CHECK(dmiss.file_count == 0);
}

void test_diff() {
    using namespace loadout::util;

    // Identical inputs -> empty diff.
    CHECK(unified_diff("a\nb\nc\n", "a\nb\nc\n", "left", "right").empty());

    // Simple edit + reconstruction invariant.
    std::vector<std::string> left = {"line1", "line2", "line3", "line4", "line5", "line6", "line7", "line8"};
    std::vector<std::string> right = {"line1", "line2-edit", "line3", "line4", "line5", "line6", "line7", "line7.5", "line8"};
    auto ops = myers_diff_lines(left, right);

    std::vector<std::string> reconstructed_left;
    std::vector<std::string> reconstructed_right;
    for (const auto& dl : ops) {
        if (dl.op != DiffOp::Insert) reconstructed_left.push_back(dl.text);
        if (dl.op != DiffOp::Delete) reconstructed_right.push_back(dl.text);
    }
    CHECK(reconstructed_left == left);
    CHECK(reconstructed_right == right);

    // Unified diff format checks (2 separate hunks with context=1).
    std::string left_s = "l1\nl2\nl3\nl4\nl5\nl6\nl7\nl8\nl9\nl10\n";
    std::string right_s = "l1\nl2-changed\nl3\nl4\nl5\nl6\nl7\nl8\nl9-changed\nl10\n";
    std::string ud = unified_diff(left_s, right_s, "a/SKILL.md", "b/SKILL.md", 1);
    CHECK(ud.find("--- a/SKILL.md\n+++ b/SKILL.md\n") == 0);
    CHECK(ud.find("@@ -1,3 +1,3 @@\n") != std::string::npos);
    CHECK(ud.find("-l2\n+l2-changed\n") != std::string::npos);
    CHECK(ud.find("@@ -8,3 +8,3 @@\n") != std::string::npos);
    CHECK(ud.find("-l9\n+l9-changed\n") != std::string::npos);

    // Pure insertion from empty file & pure deletion to empty file.
    std::string add_diff = unified_diff("", "alpha\nbeta\n", "empty", "new");
    CHECK(add_diff.find("@@ -0,0 +1,2 @@\n+alpha\n+beta\n") != std::string::npos);

    std::string del_diff = unified_diff("alpha\nbeta\n", "", "old", "empty");
    CHECK(del_diff.find("@@ -1,2 +0,0 @@\n-alpha\n-beta\n") != std::string::npos);
}

void test_frontmatter() {
    using namespace loadout::util;

    // 1. Valid Antigravity frontmatter with inline tags.
    const std::string valid_md =
        "---\n"
        "name: playwright-e2e\n"
        "description: \"Run end-to-end browser tests with Playwright.\"\n"
        "tags: [testing, \"browser\", 'e2e']\n"
        "---\n"
        "\n"
        "# Playwright E2E\n"
        "Use this skill to test web apps.\n";
    auto f1 = parse_skill_markdown(valid_md, "playwright-e2e");
    CHECK(f1.has_frontmatter);
    CHECK(f1.name == "playwright-e2e");
    CHECK(f1.description == "Run end-to-end browser tests with Playwright.");
    CHECK(f1.tags.size() == 3);
    CHECK(f1.tags[0] == "testing" && f1.tags[1] == "browser" && f1.tags[2] == "e2e");
    CHECK(f1.approx_tokens > 0);
    CHECK(f1.antigravity_valid);
    CHECK(f1.issues.empty());
    CHECK(f1.body.find("# Playwright E2E") != std::string::npos);

    // 2. Folded (>-) multiline description + block list tags.
    const std::string folded_md =
        "---\n"
        "name: git-release\n"
        "description: >-\n"
        "  Automate semantic versioning,\n"
        "  changelog generation, and GitHub releases.\n"
        "tags:\n"
        "  - git\n"
        "  - release\n"
        "---\n"
        "Body text.\n";
    auto f2 = parse_skill_markdown(folded_md, "git-release");
    CHECK(f2.has_frontmatter);
    CHECK(f2.antigravity_valid);
    CHECK(f2.description == "Automate semantic versioning, changelog generation, and GitHub releases.");
    CHECK(f2.tags.size() == 2 && f2.tags[0] == "git" && f2.tags[1] == "release");

    // 3. Literal (|) multiline description.
    const std::string literal_md =
        "---\n"
        "name: audit\n"
        "description: |\n"
        "  Line one.\n"
        "  Line two.\n"
        "---\n"
        "Details.\n";
    auto f3 = parse_skill_markdown(literal_md, "audit");
    CHECK(f3.antigravity_valid);
    CHECK(f3.description == "Line one.\nLine two.");

    // 4. Frontmatter-less Claude skill -> invalid for Antigravity, then fixed non-destructively.
    const std::string raw_claude_md =
        "# Rust Perf Audit\n"
        "\n"
        "```bash\n"
        "cargo flamegraph\n"
        "```\n"
        "\n"
        "Analyze CPU hotspots: profile hot loops and allocation spikes in Rust binaries.\n"
        "\n"
        "## Steps\n"
        "1. Run profiler.\n";
    auto f4 = parse_skill_markdown(raw_claude_md, "rust-perf-audit");
    CHECK(!f4.has_frontmatter);
    CHECK(!f4.antigravity_valid);
    CHECK(!f4.issues.empty());

    std::string fixed4 = fix_skill_frontmatter(raw_claude_md, "rust-perf-audit");
    auto f4_fixed = parse_skill_markdown(fixed4, "rust-perf-audit");
    CHECK(f4_fixed.has_frontmatter);
    CHECK(f4_fixed.antigravity_valid);
    CHECK(f4_fixed.name == "rust-perf-audit");
    CHECK(f4_fixed.description == "Analyze CPU hotspots: profile hot loops and allocation spikes in Rust binaries.");
    CHECK(f4_fixed.issues.empty());
    CHECK(f4_fixed.body.find("# Rust Perf Audit") != std::string::npos);
    CHECK(f4_fixed.body.find("1. Run profiler.") != std::string::npos);

    // 5. Mismatched name + extra metadata preserved on repair.
    const std::string mismatched_md =
        "---\n"
        "name: old-slug\n"
        "license: MIT\n"
        "description: >-\n"
        "  Keep this rich\n"
        "  existing description.\n"
        "tags: [perf]\n"
        "---\n"
        "Original body stays intact.\n";
    auto f5 = parse_skill_markdown(mismatched_md, "new-slug");
    CHECK(f5.has_frontmatter);
    CHECK(!f5.antigravity_valid);
    CHECK(f5.issues.size() == 1);

    std::string fixed5 = fix_skill_frontmatter(mismatched_md, "new-slug");
    auto f5_fixed = parse_skill_markdown(fixed5, "new-slug");
    CHECK(f5_fixed.antigravity_valid);
    CHECK(f5_fixed.name == "new-slug");
    CHECK(f5_fixed.description == "Keep this rich existing description.");
    CHECK(f5_fixed.tags.size() == 1 && f5_fixed.tags[0] == "perf");
    CHECK(fixed5.find("license: MIT") != std::string::npos);
    CHECK(f5_fixed.body == "Original body stays intact.\n");
}

} // namespace

int main() {
    test_json();
    test_sha256_and_tree();
    test_diff();
    test_frontmatter();

    if (g_failures > 0) {
        std::cerr << "FAILED: " << g_failures << " / " << g_assertions << " assertions failed.\n";
        return 1;
    }
    std::cout << "PASS: all " << g_assertions << " assertions passed.\n";
    return 0;
}
