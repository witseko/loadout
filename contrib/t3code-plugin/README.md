# `@singlet/t3code-plugin`

Official [`pingdotgg/t3code`](https://github.com/pingdotgg/t3code) integration package for **`singlet`**: keeping skills synchronized across providers (`claude`, `codex`, `opencode`, `antigravity`), multi-account profiles (`CLAUDE_CONFIG_DIR=~/.claude-second`, `CODEX_HOME=~/.codex-work`), and isolated per-thread Git worktrees.

> This directory is intentionally **source-only** (`0` runtime dependencies, `0` checked-in `node_modules`) so the root `singlet` repository stays a pure zero-dependency C++20 codebase.

---

## Tier 1: Zero-Fork Integration (`singlet mcp`)

Works in T3 Code (and Claude Code / Codex / OpenCode) immediately with **zero changes to T3 Code's source code**. Add `singlet mcp` to your MCP configuration:

```json
{
  "mcpServers": {
    "singlet": {
      "command": "singlet",
      "args": ["mcp"],
      "env": {
        "SINGLET_HOME": ""
      }
    }
  }
}
```

Exposed MCP tools:
- `singlet_list_catalog`: Canonical skills, token estimates, and wiring matrix per target/account.
- `singlet_audit_duplicates`: Exact duplicates, drifted forks, broken symlinks, and Antigravity frontmatter issues.
- `singlet_toggle_skill`: Enable/disable a skill symlink in specific target instances.
- `singlet_add_target`: Register custom tool folders or sibling accounts (e.g. `~/.claude-second/skills`).
- `singlet_hydrate_worktree`: Materialize project skill symlinks in a Git worktree cleanly excluded via `.git/info/exclude`.
- `singlet_fix_frontmatter`: Repair missing/mismatched YAML frontmatter non-destructively.

Exposed MCP resources:
- `skill://<slug>/SKILL.md`: On-demand inspection of any canonical skill.

---

## Tier 2: `apps/server` Effect-TS Plugin (`@singlet/t3code-plugin/server`)

### 1. Provide `SingletServiceLive` in T3 Code's Server Layer

`SingletServiceLive` is an Effect `Layer.scoped` that spawns `singlet rpc`, multiplexes JSON-RPC 2.0 requests over `stdin`/`stdout`, exposes a reactive `events: Stream.Stream<SingletEvent>`, and terminates the child process cleanly when the Effect scope closes.

```ts
import { Effect, Layer } from "effect";
import {
  SingletService,
  SingletServiceLive,
  makeSingletServiceLayer,
  handleSingletWebSocketRpc,
} from "@singlet/t3code-plugin/server";

// Default: uses `SINGLET_BIN` (default `"singlet"`) and `SINGLET_HOME` from environment
const ServerLive = Layer.mergeAll(
  /* ...existing T3 Code layers... */
  SingletServiceLive,
);

// Or configure explicitly:
const CustomSingletLive = makeSingletServiceLayer({
  binaryPath: "/usr/local/bin/singlet",
  defaultAutoWire: false,
  defaultAutoFixFrontmatter: false,
});
```

### 2. Hydrate Per-Thread Git Worktrees (`onWorktreeCreated`)

Wire `onWorktreeCreated` into T3 Code's worktree manager so newly created thread worktrees automatically receive untracked skill symlinks without dirtying `git status` (managed paths are written to `<gitdir>/info/exclude`):

```ts
const hydrateOnCreate = (ctx: {
  worktreePath: string;
  repoRoot: string;
  providerId: string;
  env: Record<string, string>;
}) =>
  Effect.gen(function* () {
    const singlet = yield* SingletService;
    return yield* singlet.onWorktreeCreated(ctx);
  });
```

### 3. Verify Multi-Account Skill Wiring Before Each Turn (`onBeforeProviderTurn`)

`onBeforeProviderTurn` resolves the exact target instance from the provider's environment (e.g., `CLAUDE_CONFIG_DIR=~/.claude-second` -> `claude-second`, `CODEX_HOME=~/.codex-work` -> `codex-work`), verifies every `$skill` selected in the composer is wired and frontmatter-valid, and optionally auto-wires missing symlinks:

```ts
const preflightTurn = (ctx: {
  providerId: string;
  env: Record<string, string>;
  selectedSkills: string[];
}) =>
  Effect.gen(function* () {
    const singlet = yield* SingletService;
    const report = yield* singlet.onBeforeProviderTurn({
      ...ctx,
      autoWire: true, // set false to warn instead of auto-linking
    });
    if (!report.ok) {
      yield* Effect.logWarning("singlet preflight warnings", report.warnings);
    }
    return report;
  });
```

### 4. Bridge `singlet` RPC and Events Over T3 Code's WebSocket

Forward client RPC calls (`catalog.get`, `skill.toggle`, `target.add`, `profile.apply`, etc.) so desktop, browser, and mobile T3 Code clients can control skills on the host machine:

```ts
// Inside your WebSocket RPC router:
const onSingletMessage = (msg: { method: SingletRpcMethod; params?: Record<string, unknown> }) =>
  handleSingletWebSocketRpc(msg);
```

---

## Tier 3: `apps/web` React 19 Integration (`@singlet/t3code-plugin/web`)

### 1. Serve the Embedded `<singlet-switchboard>` Web Component

`singlet` bundles its zero-framework Web Component (`switchboard.js` + `app.css`) inside the native binary. Export it once from `apps/server` (using `singlet --dump-ui-js`) or serve it at `/api/singlet/switchboard.js` and `/api/singlet/app.css`.

### 2. Mount `<SingletSwitchboardDrawer />`

Includes the compact Artboard 6 header, `Global targets` / `Current Git worktree` segmented switch, `--singlet-*` design tokens (`#0B0D10`, `#E0DDBD`, `#3DDC97`, `#F5B544`, `#F2647A`), and `synced over host WebSocket` status bar.

```tsx
import { useState } from "react";
import {
  SingletSwitchboardDrawer,
  type T3CodeWsBridge,
} from "@singlet/t3code-plugin/web";

export function SkillsDrawerHost({
  wsBridge,
  activeTargetId,
  worktreePath,
}: {
  wsBridge: T3CodeWsBridge;
  activeTargetId?: string;
  worktreePath?: string;
}) {
  const [open, setOpen] = useState(false);

  return (
    <>
      <button type="button" onClick={() => setOpen(true)}>
        Skills drawer
      </button>

      <SingletSwitchboardDrawer
        open={open}
        onClose={() => setOpen(false)}
        wsBridge={wsBridge}
        scriptUrl="/api/singlet/switchboard.js"
        cssUrl="/api/singlet/app.css"
        scope={worktreePath ? "worktree" : "global"}
        worktreePath={worktreePath}
        activeTargetId={activeTargetId}
      />
    </>
  );
}
```

### 3. Add Target-Wiring Pills and 1-Click `"Wire & Insert"` to the `$` Composer Picker

```tsx
import { ComposerSkillPickerBadge } from "@singlet/t3code-plugin/web/composer";
import type { CatalogSnapshot } from "@singlet/t3code-plugin/protocol";
import type { T3CodeWsBridge } from "@singlet/t3code-plugin/web";

export function SkillAutocompleteRow({
  slug,
  catalog,
  providerId,
  providerEnv,
  wsBridge,
  insertIntoComposer,
}: {
  slug: string;
  catalog: CatalogSnapshot;
  providerId: string;
  providerEnv: Record<string, string>;
  wsBridge: T3CodeWsBridge;
  insertIntoComposer: (token: string) => void;
}) {
  return (
    <div style={{ display: "flex", justifyContent: "space-between", gap: 8 }}>
      <span>${slug}</span>
      <ComposerSkillPickerBadge
        skill={slug}
        catalog={catalog}
        providerId={providerId}
        providerEnv={providerEnv}
        wsBridge={wsBridge}
        onInsert={(s) => insertIntoComposer(`$${s} `)}
        onWireAndUse={(wiredSlug) => insertIntoComposer(`$${wiredSlug} `)}
      />
    </div>
  );
}
```

---

## Subpath Exports

| Subpath | Environment | Purpose |
| :--- | :--- | :--- |
| `@singlet/t3code-plugin/protocol` | Universal (Node/Bun/Browser) | JSON-RPC 2.0 envelopes, `CatalogSnapshot` types, `resolveTargetForEnv` |
| `@singlet/t3code-plugin/server` | Node / Bun (`apps/server`) | Effect-TS `SingletService`, `SingletServiceLive`, lifecycle hooks |
| `@singlet/t3code-plugin/server/rpc` | Node / Bun (`apps/server`) | Zero-dep stdio JSON-RPC 2.0 `SingletRpcClient` |
| `@singlet/t3code-plugin/web` | Browser / Electron (`apps/web`) | React 19 `<SingletSwitchboardDrawer />` & `createWsSingletTransport` |
| `@singlet/t3code-plugin/web/composer` | Browser / Electron (`apps/web`) | React 19 `<ComposerSkillPickerBadge />` |
