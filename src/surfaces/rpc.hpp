#pragma once

namespace loadout::core {
class Engine;
}

namespace loadout::surfaces {

/// Run the line-delimited JSON-RPC 2.0 daemon over stdin/stdout (`loadout rpc`).
[[nodiscard]] int run_rpc_server(loadout::core::Engine& engine);

/// Run the Model Context Protocol JSON-RPC 2.0 server over stdin/stdout (`loadout mcp`).
/// Supports both newline-delimited JSON-RPC and Content-Length framed JSON-RPC.
[[nodiscard]] int run_mcp_server(loadout::core::Engine& engine);

} // namespace loadout::surfaces
