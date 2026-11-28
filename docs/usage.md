# Usage

The full reference for `singlet`. For a short introduction, see the [README](../README.md).

## Commands

| Command | What it does |
| :--- | :--- |
| `singlet scan [--roots a,b] [--json]` | List skills, targets, duplicates, and problems. Read-only. |
| `singlet ui [--port 4747] [--browser] [--no-open]` | Open the UI in a native desktop window. `--browser` opens it in your web browser instead. The server binds to `127.0.0.1`. |
| `singlet tui` | Start the terminal UI. |
| `singlet dedup [--apply] [--promote-repo a,b] [--json]` | Show the plan to move duplicates into `~/.agents/skills` and replace them with symlinks. Nothing changes without `--apply`. With `--apply`, originals go to quarantine first. `--promote-repo` also moves the given repo-local skills into the store. |
| `singlet undo` | Restore the last quarantine batch byte for byte. |
| `singlet toggle <skill> --on t1,t2 --off t3` | Wire or unwire a skill in the given targets. |
| `singlet install <github-url> [--on t1,t2] [--json]` | Install one skill or every skill in a GitHub repo or folder into `~/.agents/skills`. `--on` also wires the new skills into the given targets. See [Install from GitHub](#install-from-github). |
| `singlet update [skill...] [--apply] [--force] [--json]` | Check installed skills against their GitHub source. Read-only without `--apply`. |
| `singlet target list [--json]` | List discovered and custom targets. |
| `singlet target add <id> <dir> [--kind k] [--name n] [--env e] [--clone-from id]` | Register a custom folder or another account. |
| `singlet target clone <from> <to>` | Copy the wiring of one target to another. |
| `singlet target remove <id>` | Remove the managed symlinks and forget the target. |
| `singlet doctor [--fix]` | Find broken links and bad frontmatter. `--fix` repairs frontmatter and keeps the body. |
| `singlet rpc` | JSON-RPC 2.0 server on stdin/stdout (used by the T3 Code plugin). |
| `singlet mcp` | MCP server on stdin/stdout. |

Run `singlet --help` for the full list.

## Try it without touching your real setup

Every command accepts `--home <dir>`, or the `SINGLET_HOME` environment variable. `singlet` then treats that folder as your home folder and does not read or write the real one.

```bash
SINGLET_HOME=/tmp/singlet-sandbox singlet scan
```

## Install from GitHub

`singlet install` accepts these URL forms:

| URL | Installs |
| :--- | :--- |
| `https://github.com/<owner>/<repo>` | Every skill in the repo, from the default branch |
| `https://github.com/<owner>/<repo>/tree/<ref>/<path>` | The skill at `<path>`, or every skill below it, from branch or tag `<ref>` |
| `https://github.com/<owner>/<repo>/blob/<ref>/<path>/SKILL.md` | The skill in that folder |

A skill is a folder with a `SKILL.md` file. `singlet` looks up to six folders deep and skips `.git` and `node_modules`. The skill keeps its folder name as its slug. For a `SKILL.md` at the repo root, the slug is the repo name.

```bash
singlet install https://github.com/<owner>/<repo>/tree/main/skills/pdf --on claude-main
```

Rules:

- `install` never replaces a skill that is already in the store. It reports it as skipped.
- `singlet` copies regular files and folders only. It does not copy symlinks or `.git`.
- It runs `git` (it must be on your `PATH`) over HTTPS without terminal prompts, so only public repos work.
- A branch name that contains `/` does not work in a `tree` URL, because `singlet` reads the first segment after `tree` as the ref.

`singlet` records the source of each installed skill (repo, ref, path, commit, and tree hash) in `singlet.lock.json`. Then:

```bash
singlet update
```

```bash
singlet update --apply
```

The first command compares each installed skill with the latest commit of its source and shows `up-to-date`, `update-available`, `local-changes`, or `failed`. With `--apply`, `singlet` downloads the new version. The old folder goes to quarantine, so `singlet undo` restores it. A skill that you edited after the install shows `local-changes` and is not updated unless you add `--force`. Name skills to check or update only those, for example `singlet update pdf --apply`.

In `singlet ui`, click **+ Install** to do the same: paste a URL, optionally pick the targets to turn the new skills on in, and check or apply updates. While `git` runs, the UI waits for it.

## Supported agents

`singlet` discovers these folders in your home folder. Suffixed folders are separate accounts, for example `~/.claude-work` becomes target `claude-work`.

| Agent | Folders | `--kind` |
| :--- | :--- | :--- |
| Claude Code | `~/.claude`, `~/.claude-*` | `claude-code` |
| Codex | `~/.codex`, `~/.codex-*` | `codex` |
| Gemini / Antigravity | `~/.gemini`, `~/.gemini-*` | `antigravity` |
| Cursor | `~/.cursor`, `~/.cursor-*` | `cursor` |
| ZCode | `~/.zcode`, `~/.zcode-*` | `zcode` |
| OpenCode | `~/.config/opencode`, `~/.config/opencode-*` (also custom skill paths in `opencode.json`) | `opencode` |
| Windsurf | `~/.codeium/windsurf*` | `windsurf` |
| Any other folder | added with `singlet target add` | `custom-lenient`, `custom-strict` |

`singlet` also finds project skills in git repos (for example `<repo>/.claude/skills`) and keeps them in the repo.

## Desktop launcher

`make install-app` adds a launcher that runs `singlet ui`:

| OS | Launcher |
| :--- | :--- |
| macOS | App bundle `/Applications/Singlet.app` |
| Linux | Desktop entry `~/.local/share/applications/singlet.desktop` |

The command builds `bin/singlet` first. The launcher points at that binary, so a rebuild needs no reinstall. If you move or delete the checkout, the launcher stops working. In that case, point it at a binary that stays in place:

```bash
make install-app SINGLET_BIN=/usr/local/bin/singlet
```

To install the launcher at a different path, pass `APP_LAUNCHER`, for example `APP_LAUNCHER=$HOME/Applications/Singlet.app`.

To remove the launcher:

```bash
make uninstall-app
```

On Linux, the native window needs GTK 3 and WebKitGTK 4.1 (on Ubuntu: `libgtk-3-0` and `libwebkit2gtk-4.1-0`). Without them, `singlet ui` opens the UI in your web browser.

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

Add `singlet` to the MCP config of Claude Code, Codex, OpenCode, or T3 Code:

```json
{
  "mcpServers": {
    "singlet": { "command": "singlet", "args": ["mcp"] }
  }
}
```

The agent then gets these tools: `singlet_list_catalog`, `singlet_audit_duplicates`, `singlet_toggle_skill`, `singlet_add_target`, `singlet_hydrate_worktree`, `singlet_fix_frontmatter`, `singlet_install_skills`, `singlet_check_skill_updates`, `singlet_update_skills`. It can also read any skill as the resource `skill://<slug>/SKILL.md`.

For the deeper T3 Code integration, see [contrib/t3code-plugin](../contrib/t3code-plugin/README.md).

## What singlet writes

All state lives under `~/.agents` (or under `--home`):

| Path | Content |
| :--- | :--- |
| `~/.agents/skills/` | The canonical store: one real folder per skill. |
| `~/.agents/quarantine/` | Copies that `dedup` replaced, with a `manifest.json` per batch for `undo`. |
| `~/.agents/singlet.lock.json` | Custom targets, scan roots, profiles, quarantine history, and the GitHub source of each installed skill. |

In agent folders, `singlet` only creates and removes symlinks that point into the canonical store. It does not delete a real folder without a quarantine copy.

For git worktrees, `singlet` adds its symlinks to `.git/info/exclude`, so `git status` stays clean.
