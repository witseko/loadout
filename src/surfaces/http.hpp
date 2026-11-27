#pragma once

#include <cstdint>
#include <functional>
#include <string>

namespace singlet::core {
class Engine;
}

namespace singlet::surfaces {

/// Configuration options for the local HTTP + SSE server.
struct HttpServerOptions {
    uint16_t port = 4747; // 0 = OS-assigned ephemeral free port
    bool fallback_ephemeral = false;
    bool open_browser = true;
    std::string host = "127.0.0.1";
    std::function<void(const std::string& url)> on_ready;
    std::function<void()> on_stop;
};

/// Start the zero-dependency POSIX localhost HTTP + SSE server.
/// Prints "SINGLET_UI_URL=http://127.0.0.1:<port>\n" immediately once listening.
[[nodiscard]] int run_http_server(singlet::core::Engine& engine, const HttpServerOptions& opts);

/// Request a running HTTP server loop to shut down cleanly.
void request_http_server_stop();

/// Launch the platform default web browser (`open` on macOS, `xdg-open` on Linux).
void launch_system_browser(const std::string& url);

} // namespace singlet::surfaces
