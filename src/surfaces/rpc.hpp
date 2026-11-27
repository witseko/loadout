#pragma once

namespace singlet::core {
class Engine;
}

namespace singlet::surfaces {

/// Run the line-delimited JSON-RPC 2.0 daemon over stdin/stdout (`singlet rpc`).
[[nodiscard]] int run_rpc_server(singlet::core::Engine& engine);

/// Run the Model Context Protocol JSON-RPC 2.0 server over stdin/stdout (`singlet mcp`).
/// Supports both newline-delimited JSON-RPC and Content-Length framed JSON-RPC.
[[nodiscard]] int run_mcp_server(singlet::core::Engine& engine);

} // namespace singlet::surfaces
