# Usage

The full reference for `loadout`. For a short introduction, see the [README](../README.md).

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

## Desktop launcher

`make install-app` adds a launcher that runs `loadout ui`:

| OS | Launcher |
| :--- | :--- |
| macOS | App bundle `/Applications/Loadout.app` |
| Linux | Desktop entry `~/.local/share/applications/loadout.desktop` |

The command builds `bin/loadout` first. The launcher points at that binary, so a rebuild needs no reinstall. If you move or delete the checkout, the launcher stops working. In that case, point it at a binary that stays in place:

```bash
make install-app LOADOUT_BIN=/usr/local/bin/loadout
```

To install the launcher at a different path, pass `APP_LAUNCHER`, for example `APP_LAUNCHER=$HOME/Applications/Loadout.app`.

To remove the launcher:

```bash
make uninstall-app
```

On Linux, the native window needs GTK 3 and WebKitGTK 4.1 (on Ubuntu: `libgtk-3-0` and `libwebkit2gtk-4.1-0`). Without them, `loadout ui` opens the UI in your web browser.

## Terminal UI keys

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

For the deeper T3 Code integration, see [contrib/t3code-plugin](../contrib/t3code-plugin/README.md).

## What loadout writes

All state lives under `~/.agents` (or under `--home`):

| Path | Content |
| :--- | :--- |
| `~/.agents/skills/` | The canonical store: one real folder per skill. |
| `~/.agents/quarantine/` | Copies that `dedup` replaced, with a `manifest.json` per batch for `undo`. |
| `~/.agents/loadout.lock.json` | Custom targets, scan roots, profiles, and quarantine history. |

In agent folders, `loadout` only creates and removes symlinks that point into the canonical store. It does not delete a real folder without a quarantine copy.

For git worktrees, `loadout` adds its symlinks to `.git/info/exclude`, so `git status` stays clean.
