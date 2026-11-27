#pragma once

namespace singlet::core {
class Engine;
}

namespace singlet::surfaces {

/// Launch the interactive terminal Patchbay Matrix UI using raw POSIX termios.
/// Returns 0 on normal exit, non-zero if stdin/stdout is not a TTY.
[[nodiscard]] int run_tui(singlet::core::Engine& engine);

} // namespace singlet::surfaces
