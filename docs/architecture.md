# Architecture

This document explains how `singlet` is built. Read it before you change the engine or add a new surface.

## Goals

- **One binary, zero dependencies.** Only the C++20 standard library and POSIX headers. JSON, SHA-256, diff, YAML frontmatter, HTTP, and the terminal UI are written in this repo.
- **No data loss.** A real skill folder is only replaced after a copy is in quarantine. Every batch can be undone.
- **Many surfaces, one engine.** The CLI, TUI, browser UI, JSON-RPC server, and MCP server all call the same `Engine`.

## Layers

```
src/main.cpp                -> surfaces::run_cli
src/surfaces/  cli, tui, http, rpc (+ mcp)   user-facing entry points
src/core/      engine, scanner, targets, types   domain logic and state
src/util/      json, sha256, diff, frontmatter   pure helpers, no domain knowledge
src/ui/        index.html, app.css, switchboard.js, embedded_ui.hpp   browser UI assets
```

Dependencies flow one way: `surfaces -> core -> util`. `util` does not include `core`, and `core` does not include `surfaces`.

| Module | Responsibility |
| :--- | :--- |
| `util/json` | RFC 8259 JSON and JSONC parser, DOM, and serializer. |
| `util/sha256` | SHA-256 and a Merkle hash of a skill folder (the "tree hash"). |
| `util/diff` | Myers line diff and unified diff output, used for drifted copies. |
| `util/frontmatter` | Parse `SKILL.md` frontmatter and repair it without touching the body. |
| `core/types` | Plain data: targets, adapters, occurrences, snapshots, results. |
| `core/targets` | Discover agent folders and accounts in the home folder. |
| `core/scanner` | Walk skill containers in parallel and record every occurrence of a skill. |
| `core/engine` | Own the lockfile, build the catalog snapshot, wire symlinks, consolidate, undo, profiles, worktree hydration. |
| `surfaces/cli` | Parse arguments and print results. Dispatches to the other surfaces. |
| `surfaces/tui` | Raw `termios` terminal UI. |
| `surfaces/http` | Local HTTP server with Server-Sent Events (SSE) for live updates in the browser UI. |
| `surfaces/rpc` | Line-delimited JSON-RPC 2.0 over stdin/stdout, and the MCP server on top of it. |

## Core concepts

- **Skill**: a folder that contains a `SKILL.md` file. Its folder name is the **slug**.
- **Target**: a folder where an agent reads skills, for example `~/.claude-work/skills`. Each target has an **adapter** (`claude-code`, `codex`, `antigravity`, ...) that sets the frontmatter rules and link style.
- **Occurrence**: one place where a skill appears. Its kind is canonical, managed symlink, physical copy, plugin shadow, or broken link.
- **Canonical store**: `~/.agents/skills`. Managed symlinks in targets point here.
- **Exact duplicate**: two or more physical copies of one slug with the same tree hash.
- **Drifted copy**: one slug with two or more different tree hashes. The UI shows a diff, and the user picks the version to keep.
- **Repo-local skill**: a skill inside a git repo. It stays in the repo unless the user promotes it with `dedup --promote-repo`.

## Data flow

1. `targets` discovers agent folders and adds custom targets from the lockfile.
2. `scanner` walks the canonical store, every target, and repo roots, and hashes each skill folder.
3. `engine` groups occurrences by slug and builds a **catalog snapshot**: skills, targets, the wiring matrix, duplicates, drift, and problems.
4. A surface shows the snapshot. A user action (toggle, dedup, fix, undo) calls an `engine` method, which changes the file system, saves the lockfile, and rescans.
5. `http` pushes an SSE event after each change, so open browser tabs update.

## State on disk

All paths are relative to the home folder (`--home` or `SINGLET_HOME` override it).

| Path | Written by | Content |
| :--- | :--- | :--- |
| `.agents/skills/<slug>/` | `dedup --apply` | Canonical skill folders. |
| `.agents/quarantine/<batch>/` | `dedup --apply` | Replaced copies and `manifest.json`. `undo` reads the manifest. |
| `.agents/singlet.lock.json` | every write action | Custom targets, scan roots, profiles, quarantine history. |
| `<worktree>/.git/info/exclude` (or the worktree git dir) | `worktree.hydrate` | Paths of hydrated symlinks, so `git status` stays clean. |

Files and symlinks are first written under a temporary name that contains `.singlet-tmp-<pid>-<n>`, then renamed into place. The scanner skips these names.

## Browser UI

The UI is a framework-free Web Component, `<singlet-switchboard>`, in `src/ui/switchboard.js` and `src/ui/app.css`. The binary serves it from memory, so there is nothing to install.

`src/ui/embedded_ui.hpp` holds a copy of `index.html`, `app.css`, and `switchboard.js` as raw string literals. **When you edit a file in `src/ui/`, make the same edit in `embedded_ui.hpp`**, or the binary keeps serving the old version.

HTTP endpoints (all on `127.0.0.1`):

| Endpoint | Purpose |
| :--- | :--- |
| `/`, `/app.css`, `/switchboard.js` | UI assets |
| `/api/catalog`, `/api/scan` | Read the snapshot, force a rescan |
| `/api/toggle`, `/api/consolidate`, `/api/undo` | Write actions |
| `/api/skill`, `/api/targets`, `/api/profiles` | Skill files, targets, profiles |
| `/api/events` | SSE stream |

## JSON-RPC and MCP

`singlet rpc` reads one JSON-RPC 2.0 request per line. Methods: `catalog.get`, `catalog.scan`, `skill.get`, `skill.save`, `skill.toggle`, `skill.consolidate`, `target.add`, `target.remove`, `target.clone`, `profile.list`, `profile.save`, `profile.apply`, `profile.delete`, `quarantine.undo`, `worktree.hydrate`, `ping`.

`singlet mcp` speaks the Model Context Protocol on the same transport and maps its tools to the engine. The TypeScript contract in `contrib/t3code-plugin/src/protocol.ts` must match `src/surfaces/rpc.cpp` one to one. The e2e suite (`tests/e2e/tests/mcp-contract.spec.ts`) checks this.
