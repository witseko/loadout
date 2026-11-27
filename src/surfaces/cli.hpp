#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace loadout::surfaces {

inline constexpr const char* LOADOUT_VERSION = "0.1.0";

/// Parsed top-level CLI options extracted from argv.
struct CliOptions {
    std::optional<std::filesystem::path> home_override;
    std::string command;
    std::vector<std::string> args;
};

/// Resolve effective home directory honoring: --home, LOADOUT_HOME, then HOME.
[[nodiscard]] std::filesystem::path resolve_home_dir(const std::optional<std::filesystem::path>& home_override);

/// Parse top-level arguments, extracting --home anywhere in argv.
[[nodiscard]] CliOptions parse_cli_args(int argc, char** argv);

/// Main CLI entrypoint; returns process exit code.
[[nodiscard]] int run_cli(int argc, char** argv);

} // namespace loadout::surfaces
