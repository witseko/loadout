# Changelog

All notable changes to this project are listed here. The format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and versions follow [Semantic Versioning](https://semver.org/).

## [Unreleased]

### Added

- `singlet install <github-url>`: install one skill or every skill in a GitHub repo, folder, or `SKILL.md` link into `~/.agents/skills`, with `--on` to wire them right away.
- `singlet update [skill...] [--apply] [--force]`: check installed skills against their GitHub source and update them. Old versions go to quarantine, so `singlet undo` restores them. Skills with local edits need `--force`.
- Install and update in every surface: a **+ Install** modal in `singlet ui`, the MCP tools `singlet_install_skills`, `singlet_check_skill_updates`, and `singlet_update_skills`, and the JSON-RPC methods `skill.install`, `source.check`, and `source.update`.

## [0.1.0] - 2026-11-27

First public release.

### Added

- `singlet scan`: discover skills and targets for Claude Code, Codex, Gemini / Antigravity, Cursor, ZCode, OpenCode, Windsurf, extra accounts (`~/.claude-*`, `~/.codex-*`, ...), and repo-local skills.
- Exact duplicate and drifted copy detection with SHA-256 tree hashes and unified diffs.
- `singlet dedup` and `singlet undo`: preview, then consolidate with `--apply` into `~/.agents/skills` with quarantine backup and byte-exact restore.
- `singlet toggle` and `singlet target add | clone | remove | list`: per-target and per-account symlink wiring, custom targets.
- `singlet doctor [--fix]`: broken link audit and non-destructive frontmatter repair.
- Profiles: save and apply named wiring sets.
- Git worktree hydration with `.git/info/exclude`, so `git status` stays clean.
- `singlet ui`: browser patchbay UI on `127.0.0.1` with live updates over SSE, skill inspector, and diff resolver.
- `singlet tui`: keyboard-driven terminal patchbay.
- `singlet rpc`: JSON-RPC 2.0 server on stdin/stdout.
- `singlet mcp`: MCP server with six tools and `skill://` resources.
- `make install-app` and `make uninstall-app`: add or remove a desktop launcher for `singlet ui` (an app bundle in `/Applications` on macOS, a desktop entry in `~/.local/share/applications` on Linux).
- `contrib/t3code-plugin`: source-only T3 Code integration (Effect-TS service, React drawer, composer badge).
- `--home` and `SINGLET_HOME` to run against a sandbox home folder.
- Prebuilt binaries for macOS (`arm64`, `x86_64`) and Linux (`x86_64`, `arm64`) on each GitHub release, with `SHA256SUMS`.

[Unreleased]: https://github.com/witseko/singlet/compare/v0.1.0...HEAD
[0.1.0]: https://github.com/witseko/singlet/releases/tag/v0.1.0
