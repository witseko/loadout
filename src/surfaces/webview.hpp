#pragma once

#include <string>

namespace loadout::surfaces {

/// Configuration options for the native OS desktop window (macOS WKWebView / Linux WebKitGTK).
struct NativeWindowOptions {
    std::string title = "loadout";
    std::string url;
    int width = 1360;
    int height = 860;
    int min_width = 760;
    int min_height = 520;
    bool dark_titlebar = true;
};

enum class NativeWindowResult {
    Closed,      // Window opened and user closed it normally
    Unavailable, // No display server or native webview runtime available (fall back to browser)
    Failed       // Runtime initialization error
};

/// Open a native macOS (Cocoa + WKWebView) or Linux (GTK3 + WebKitGTK) window loading `opts.url`.
/// Must be called on the process main thread. Dynamically loads OS libraries via dlopen() so
/// CLI/TUI/RPC invocations incur zero startup or linkage overhead.
[[nodiscard]] NativeWindowResult run_native_window(const NativeWindowOptions& opts);

/// Thread-safe request to close an active native window (e.g. when SIGINT/SIGTERM stops the server).
void request_native_window_close();

} // namespace loadout::surfaces
