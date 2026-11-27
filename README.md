# loadout

[![CI](https://github.com/witseko/loadout/actions/workflows/ci.yml/badge.svg)](https://github.com/witseko/loadout/actions/workflows/ci.yml)
[![License: MIT](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)

Pick the skills each coding agent loads. One copy of every skill, symlinked into every agent and account that needs it.

`loadout` finds the `SKILL.md` folders that coding agents keep in many places (`~/.claude`, `~/.claude-work`, `~/.codex`, `~/.gemini`, `~/.config/opencode`, project repos, and more). It moves duplicates into one canonical store at `~/.agents/skills`, and it lets you turn each skill ON or OFF per agent and per account. You can do this from a browser UI, a terminal UI, the CLI, an MCP server, or a T3 Code plugin.

It is one native binary with zero third-party dependencies. It runs on macOS and Linux.

> **Status: 0.1.0.** This is the first public release. Back up `~/.agents` and your agent config folders before the first `dedup --apply`.

## Why

Each agent reads skills from its own folder. When you use several agents, or several accounts of one agent, the same skill ends up copied in many places. The copies drift apart, and you lose track of which agent has which skill.

`loadout` keeps one real copy per skill and puts symlinks where the agents look. A symlink is a small file that points to the real folder. When you edit a skill once, every agent sees the change.

## Install

You need a C++20 compiler (`clang++` or `g++`), `make`, and a POSIX system.

```bash
git clone https://github.com/witseko/loadout.git
```

```bash
cd loadout && make release
```

The binary is `bin/loadout`. Copy it to a folder on your `PATH`:

```bash
cp bin/loadout /usr/local/bin/
```

## Quick start

1. See what is on your machine. This command only reads:

   ```bash
   loadout scan
   ```

2. Open the patchbay UI in a native desktop window (add `--browser` to use your web browser):

   ```bash
   loadout ui
   ```

   Each row is a skill. Each column is an agent or account. Click a cell to wire or unwire the skill.

3. Preview how duplicates will merge. This command only reads:

   ```bash
   loadout dedup
   ```

   If the plan looks right, apply it:

   ```bash
   loadout dedup --apply
   ```

4. If you do not like the result, restore the last batch:

   ```bash
   loadout undo
   ```

## Try it without touching your real setup

Every command accepts `--home <dir>`, or the `LOADOUT_HOME` environment variable. `loadout` then treats that folder as your home folder and does not read or write the real one.

```bash
LOADOUT_HOME=/tmp/loadout-sandbox loadout scan
```

## Supported agents

`loadout` discovers these folders in your home folder. Suffixed folders are separate accounts, for example `~/.claude-work` becomes target `claude-work`.

| Agent | Folders | `--kind` |
| :--- | :--- | :--- |
| Claude Code | `~/.claude`, `~/.claude-*` | `claude-code` |
| Codex | `~/.codex`, `~/.codex-*` | `codex` |
| Gemini / Antigravity | `~/.gemini`, `~/.gemini-*` | `antigravity` |
| Cursor | `~/.cursor`, `~/.cursor-*` | `cursor` |
| ZCode | `~/.zcode`, `~/.zcode-*` | `zcode` |
| OpenCode | `~/.config/opencode`, `~/.config/opencode-*` (also custom skill paths in `opencode.json`) | `opencode` |
| Windsurf | `~/.codeium/windsurf*` | `windsurf` |
| Any other folder | added with `loadout target add` | `custom-lenient`, `custom-strict` |

`loadout` also finds project skills in git repos (for example `<repo>/.claude/skills`) and keeps them in the repo.

## Commands

| Command | What it does |
| :--- | :--- |
| `loadout scan [--roots a,b] [--json]` | List skills, targets, duplicates, and problems. Read-only. |
| `loadout ui [--port 4747] [--browser] [--no-open]` | Open the UI in a native desktop window. `--browser` opens it in your web browser instead. The server binds to `127.0.0.1`. |
| `loadout tui` | Start the terminal UI. |
| `loadout dedup [--apply] [--promote-repo a,b] [--json]` | Show the plan to move duplicates into `~/.agents/skills` and replace them with symlinks. Nothing changes without `--apply`. With `--apply`, originals go to quarantine first. `--promote-repo` also moves the given repo-local skills into the store. |
| `loadout undo` | Restore the last quarantine batch byte for byte. |
| `loadout toggle <skill> --on t1,t2 --off t3` | Wire or unwire a skill in the given targets. |
| `loadout target list [--json]` | List discovered and custom targets. |
| `loadout target add <id> <dir> [--kind k] [--name n] [--env e] [--clone-from id]` | Register a custom folder or another account. |
| `loadout target clone <from> <to>` | Copy the wiring of one target to another. |
| `loadout target remove <id>` | Remove the managed symlinks and forget the target. |
| `loadout doctor [--fix]` | Find broken links and bad frontmatter. `--fix` repairs frontmatter and keeps the body. |
| `loadout rpc` | JSON-RPC 2.0 server on stdin/stdout (used by the T3 Code plugin). |
| `loadout mcp` | MCP server on stdin/stdout. |

Run `loadout --help` for the full list.

### Terminal UI keys

| Key | Action |
| :--- | :--- |
| arrows or `h` `j` `k` `l` | Move |
| `Space` or `Enter` | Toggle the cell |
| `/` | Search |
| `a` `!` `~` `x` | Filter: all, needs attention, drifted, broken |
| `f` | Fix frontmatter of the current skill |
| `c` | Consolidate duplicates |
| `u` | Undo the last consolidation |
| `s` or `r` | Rescan |
| `p` | Save the current wiring as profile `default` |
| `q` or `Esc` | Quit |

## Use it from an agent (MCP)

Add `loadout` to the MCP config of Claude Code, Codex, OpenCode, or T3 Code:

```json
{
  "mcpServers": {
    "loadout": { "command": "loadout", "args": ["mcp"] }
  }
}
```

The agent then gets these tools: `loadout_list_catalog`, `loadout_audit_duplicates`, `loadout_toggle_skill`, `loadout_add_target`, `loadout_hydrate_worktree`, `loadout_fix_frontmatter`. It can also read any skill as the resource `skill://<slug>/SKILL.md`.

For the deeper T3 Code integration, see [contrib/t3code-plugin](contrib/t3code-plugin/README.md).

## What loadout writes

All state lives under `~/.agents` (or under `--home`):

| Path | Content |
| :--- | :--- |
| `~/.agents/skills/` | The canonical store: one real folder per skill. |
| `~/.agents/quarantine/` | Copies that `dedup` replaced, with a `manifest.json` per batch for `undo`. |
| `~/.agents/loadout.lock.json` | Custom targets, scan roots, profiles, and quarantine history. |

In agent folders, `loadout` only creates and removes symlinks that point into the canonical store. It does not delete a real folder without a quarantine copy.

For git worktrees, `loadout` adds its symlinks to `.git/info/exclude`, so `git status` stays clean.

## Security

- The UI server binds to `127.0.0.1` only. It has no authentication, so do not expose the port to other machines.
- `loadout` makes no network calls.
- To report a vulnerability, see [SECURITY.md](SECURITY.md).

## Development

See [CONTRIBUTING.md](CONTRIBUTING.md) for build, test, and code layout, and [docs/architecture.md](docs/architecture.md) for how the parts fit together. Changes are listed in [CHANGELOG.md](CHANGELOG.md). Everyone who takes part follows the [Code of Conduct](CODE_OF_CONDUCT.md).

## License

[MIT](LICENSE)
