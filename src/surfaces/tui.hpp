#pragma once

namespace loadout::core {
class Engine;
}

namespace loadout::surfaces {

/// Launch the interactive terminal Patchbay Matrix UI using raw POSIX termios.
/// Returns 0 on normal exit, non-zero if stdin/stdout is not a TTY.
[[nodiscard]] int run_tui(loadout::core::Engine& engine);

} // namespace loadout::surfaces
