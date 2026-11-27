# Changelog

All notable changes to this project are listed here. The format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and versions follow [Semantic Versioning](https://semver.org/).

## [Unreleased]

### Added

- `make install-app` and `make uninstall-app`: add or remove a desktop launcher for `loadout ui` (an app bundle in `/Applications` on macOS, a desktop entry in `~/.local/share/applications` on Linux).

### Fixed

- Profile picker in the UI: dark themed on Linux (WebKitGTK drew a light native widget) and no longer wraps to a second row.

## [0.1.0] - 2026-11-27

First public release.

### Added

- `loadout scan`: discover skills and targets for Claude Code, Codex, Gemini / Antigravity, Cursor, ZCode, OpenCode, Windsurf, extra accounts (`~/.claude-*`, `~/.codex-*`, ...), and repo-local skills.
- Exact duplicate and drifted copy detection with SHA-256 tree hashes and unified diffs.
- `loadout dedup` and `loadout undo`: preview, then consolidate with `--apply` into `~/.agents/skills` with quarantine backup and byte-exact restore.
- `loadout toggle` and `loadout target add | clone | remove | list`: per-target and per-account symlink wiring, custom targets.
- `loadout doctor [--fix]`: broken link audit and non-destructive frontmatter repair.
- Profiles: save and apply named wiring sets.
- Git worktree hydration with `.git/info/exclude`, so `git status` stays clean.
- `loadout ui`: browser patchbay UI on `127.0.0.1` with live updates over SSE, skill inspector, and diff resolver.
- `loadout tui`: keyboard-driven terminal patchbay.
- `loadout rpc`: JSON-RPC 2.0 server on stdin/stdout.
- `loadout mcp`: MCP server with six tools and `skill://` resources.
- `contrib/t3code-plugin`: source-only T3 Code integration (Effect-TS service, React drawer, composer badge).
- `--home` and `LOADOUT_HOME` to run against a sandbox home folder.
- Prebuilt binaries for macOS (`arm64`, `x86_64`) and Linux (`x86_64`, `arm64`) on each GitHub release, with `SHA256SUMS`.

[Unreleased]: https://github.com/witseko/loadout/compare/v0.1.0...HEAD
[0.1.0]: https://github.com/witseko/loadout/releases/tag/v0.1.0
