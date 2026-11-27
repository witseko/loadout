/**
 * @loadout/t3code-plugin: React 19 Embedded Switchboard Drawer (`apps/web`)
 *
 * Mounts `<loadout-switchboard mode="embedded" />` inside T3 Code's frontend
 * matching Artboard 6 (`.h-drawer`) tokens, segmented `Global targets` /
 * `Current Git worktree` switch, and WebSocket RPC bridge (`window.LOADOUT_TRANSPORT`
 * + `el.transport`).
 */

import React, { useEffect, useMemo, useRef, useState } from "react";
import {
  toSwitchboardEvent,
  type CatalogScanParams,
  type ManageTargetParams,
  type ProfileApplyParams,
  type ProfileDeleteParams,
  type ProfileSaveParams,
  type SkillConsolidateParams,
  type SkillFixFrontmatterParams,
  type SkillResolveDriftParams,
  type SkillSaveParams,
  type SkillToggleParams,
  type LoadoutBrowserTransport,
  type LoadoutEvent,
  type LoadoutRpcMethod,
  type LoadoutRpcParams,
  type LoadoutRpcResult,
  type LoadoutSwitchboardEvent,
  type TargetAddParams,
  type TargetCloneParams,
  type TargetRemoveParams,
  type WorktreeHydrateParams,
} from "../protocol.js";

// ---------------------------------------------------------------------------
// Global DOM & React 19 JSX Type Augmentation for `<loadout-switchboard>`
// ---------------------------------------------------------------------------

export type SwitchboardDrawerScope = "global" | "worktree";

export interface LoadoutSwitchboardElement extends HTMLElement {
  transport?: LoadoutBrowserTransport;
  mode?: "standalone" | "embedded";
  scope?: SwitchboardDrawerScope;
  worktreePath?: string;
  repoName?: string;
  activeTargetId?: string;
  refresh?: () => Promise<void> | void;
}

declare global {
  interface Window {
    LOADOUT_TRANSPORT?: LoadoutBrowserTransport;
  }
}

declare module "react" {
  namespace JSX {
    interface IntrinsicElements {
      "loadout-switchboard": React.DetailedHTMLProps<
        React.HTMLAttributes<LoadoutSwitchboardElement>,
        LoadoutSwitchboardElement
      > & {
        mode?: "standalone" | "embedded";
        scope?: SwitchboardDrawerScope;
        "worktree-path"?: string;
        "repo-name"?: string;
        "active-target"?: string;
      };
    }
  }
}

// ---------------------------------------------------------------------------
// T3 Code WebSocket RPC Bridge -> `window.LOADOUT_TRANSPORT` Adapter
// ---------------------------------------------------------------------------

/**
 * Minimal interface implemented by T3 Code's `apps/web` WebSocket client to
 * forward `loadout` RPC requests to `apps/server`'s `LoadoutService`.
 */
export interface T3CodeWsBridge {
  call<M extends LoadoutRpcMethod>(
    method: M,
    params?: LoadoutRpcParams<M>,
  ): Promise<LoadoutRpcResult<M>>;
  subscribe?(
    listener: (event: LoadoutEvent | LoadoutSwitchboardEvent) => void,
  ): () => void;
}

/**
 * Wraps a T3 Code WebSocket RPC bridge into the `LoadoutBrowserTransport` shape
 * consumed by `<loadout-switchboard mode="embedded">`.
 */
export function createWsLoadoutTransport(
  bridge: T3CodeWsBridge,
): LoadoutBrowserTransport {
  const call = <M extends LoadoutRpcMethod>(
    method: M,
    params?: LoadoutRpcParams<M>,
  ): Promise<LoadoutRpcResult<M>> => bridge.call(method, params);

  const manageTarget = (params: ManageTargetParams) => {
    switch (params.action) {
      case "add": {
        if (!params.id || !params.skillsDir) {
          return Promise.reject(
            new Error(
              'manageTarget({ action: "add" }) requires both `id` and `skillsDir`.',
            ),
          );
        }
        const addReq: TargetAddParams = {
          id: params.id,
          skillsDir: params.skillsDir,
          displayName: params.displayName,
          adapter: params.adapter,
          group: params.group,
          envHint: params.envHint,
          cloneFrom: params.cloneFrom ?? params.fromId,
        };
        return call("target.add", addReq);
      }
      case "remove": {
        if (!params.id) {
          return Promise.reject(
            new Error('manageTarget({ action: "remove" }) requires `id`.'),
          );
        }
        return call("target.remove", { id: params.id });
      }
      case "clone": {
        const fromId = params.fromId ?? params.cloneFrom;
        const toId = params.toId ?? params.id;
        if (!fromId || !toId) {
          return Promise.reject(
            new Error(
              'manageTarget({ action: "clone" }) requires both `fromId` and `toId`/`id`.',
            ),
          );
        }
        return call("target.clone", { fromId, toId });
      }
    }
  };

  return {
    getCatalog: () => call("catalog.get", {}),
    scan: (params?: CatalogScanParams) => call("catalog.scan", params ?? {}),
    toggle: (params: SkillToggleParams) => call("skill.toggle", params),
    consolidate: (params: SkillConsolidateParams) =>
      call("skill.consolidate", params),
    resolveDrift: (params: SkillResolveDriftParams) =>
      call("skill.resolveDrift", params),
    manageTarget,
    targets: manageTarget,
    addTarget: (params: TargetAddParams) => call("target.add", params),
    removeTarget: (params: TargetRemoveParams) => call("target.remove", params),
    cloneTarget: (params: TargetCloneParams) => call("target.clone", params),
    fixFrontmatter: (params: SkillFixFrontmatterParams) =>
      call("skill.fixFrontmatter", params),
    getSkill: (slug: string) => call("skill.get", { slug }),
    saveSkill: (params: SkillSaveParams) => call("skill.save", params),
    undo: () => call("quarantine.undo", {}),
    listProfiles: () => call("profile.list", {}),
    saveProfile: (params: ProfileSaveParams) => call("profile.save", params),
    applyProfile: (params: ProfileApplyParams) => call("profile.apply", params),
    deleteProfile: (params: ProfileDeleteParams) =>
      call("profile.delete", params),
    hydrateWorktree: (params: WorktreeHydrateParams) =>
      call("worktree.hydrate", params),
    subscribe: bridge.subscribe
      ? (listener) =>
          bridge.subscribe!((rawEvent) => {
            const mapped = toSwitchboardEvent(rawEvent);
            if (mapped) {
              listener(mapped);
            }
          })
      : undefined,
    rpc: call,
    call,
    request: call,
  };
}

// ---------------------------------------------------------------------------
// Artboard 6 Theme Tokens (`--loadout-*` aligned with `prototype/index.html`)
// ---------------------------------------------------------------------------

export interface LoadoutThemeTokens {
  readonly "--loadout-bg": string;
  readonly "--loadout-surface": string;
  readonly "--loadout-surface-2": string;
  readonly "--loadout-surface-3": string;
  readonly "--loadout-border": string;
  readonly "--loadout-border-2": string;
  readonly "--loadout-text": string;
  readonly "--loadout-dim": string;
  readonly "--loadout-muted": string;
  readonly "--loadout-faint": string;
  readonly "--loadout-accent": string;
  readonly "--loadout-accent-dim": string;
  readonly "--loadout-on-accent": string;
  readonly "--loadout-brand-o": string;
  readonly "--loadout-emerald": string;
  readonly "--loadout-emerald-dim": string;
  readonly "--loadout-amber": string;
  readonly "--loadout-amber-dim": string;
  readonly "--loadout-rose": string;
  readonly "--loadout-rose-dim": string;
  readonly "--loadout-sky": string;
  readonly "--loadout-sky-dim": string;
  readonly "--loadout-font-sans": string;
  readonly "--loadout-font-mono": string;
}

export const DEFAULT_T3CODE_LOADOUT_THEME: LoadoutThemeTokens = {
  "--loadout-bg": "var(--background, #0B0D10)",
  "--loadout-surface": "var(--card, #12151B)",
  "--loadout-surface-2": "var(--muted, #171B22)",
  "--loadout-surface-3": "var(--secondary, #1D222A)",
  "--loadout-border": "var(--border, #20252E)",
  "--loadout-border-2": "var(--input, #2C323D)",
  "--loadout-text": "var(--foreground, #E7E9EC)",
  "--loadout-dim": "var(--muted-foreground, #98A1B0)",
  "--loadout-muted": "var(--muted-foreground, #98A1B0)",
  "--loadout-faint": "#5D6676",
  "--loadout-accent": "#E0DDBD",
  "--loadout-accent-dim": "rgba(224, 221, 189, 0.16)",
  "--loadout-on-accent": "#0B0D10",
  "--loadout-brand-o": "#C8D46A",
  "--loadout-emerald": "#3DDC97",
  "--loadout-emerald-dim": "rgba(61, 220, 151, 0.14)",
  "--loadout-amber": "#F5B544",
  "--loadout-amber-dim": "rgba(245, 181, 68, 0.14)",
  "--loadout-rose": "#F2647A",
  "--loadout-rose-dim": "rgba(242, 100, 122, 0.14)",
  "--loadout-sky": "#6CB6FF",
  "--loadout-sky-dim": "rgba(108, 182, 255, 0.14)",
  "--loadout-font-sans":
    "var(--font-sans, Inter, ui-sans-serif, system-ui, -apple-system, 'Segoe UI', sans-serif)",
  "--loadout-font-mono":
    "var(--font-mono, 'JetBrains Mono', 'SF Mono', ui-monospace, Menlo, Consolas, monospace)",
};

// ---------------------------------------------------------------------------
// Lazy Web Component Script / CSS Loader
// ---------------------------------------------------------------------------

function useLoadoutComponentAssets(
  enabled: boolean,
  scriptUrl?: string,
  cssUrl?: string,
): { readonly ready: boolean; readonly loadError: string | null } {
  const isRegistered =
    typeof window !== "undefined" &&
    typeof window.customElements !== "undefined" &&
    Boolean(window.customElements.get("loadout-switchboard"));

  const [ready, setReady] = useState<boolean>(isRegistered || !scriptUrl);
  const [loadError, setLoadError] = useState<string | null>(null);

  useEffect(() => {
    if (!enabled || typeof document === "undefined") return;

    if (cssUrl) {
      const existingCss = document.querySelector<HTMLLinkElement>(
        `link[data-loadout-css="${CSS.escape(cssUrl)}"]`,
      );
      if (!existingCss) {
        const link = document.createElement("link");
        link.rel = "stylesheet";
        link.href = cssUrl;
        link.dataset.loadoutCss = cssUrl;
        document.head.appendChild(link);
      }
    }

    if (
      typeof window !== "undefined" &&
      window.customElements?.get("loadout-switchboard")
    ) {
      setReady(true);
      setLoadError(null);
      return;
    }

    if (!scriptUrl) {
      setReady(true);
      return;
    }

    let cancelled = false;
    const selector = `script[data-loadout-js="${CSS.escape(scriptUrl)}"]`;
    const existingScript = document.querySelector<HTMLScriptElement>(selector);

    const onLoad = () => {
      if (!cancelled) {
        setReady(true);
        setLoadError(null);
      }
    };
    const onError = () => {
      if (!cancelled) {
        setLoadError(
          `Failed to load loadout switchboard bundle from "${scriptUrl}".`,
        );
      }
    };

    if (existingScript) {
      if (existingScript.dataset.loadoutLoaded === "true") {
        setReady(true);
        return;
      }
      existingScript.addEventListener("load", onLoad);
      existingScript.addEventListener("error", onError);
      return () => {
        cancelled = true;
        existingScript.removeEventListener("load", onLoad);
        existingScript.removeEventListener("error", onError);
      };
    }

    const script = document.createElement("script");
    script.src = scriptUrl;
    script.type = "module";
    script.async = true;
    script.dataset.loadoutJs = scriptUrl;
    script.addEventListener("load", () => {
      script.dataset.loadoutLoaded = "true";
      onLoad();
    });
    script.addEventListener("error", onError);
    document.head.appendChild(script);

    return () => {
      cancelled = true;
    };
  }, [enabled, scriptUrl, cssUrl]);

  return { ready, loadError };
}

// ---------------------------------------------------------------------------
// `<LoadoutSwitchboardDrawer />` Component (Artboard 6 `.h-drawer`)
// ---------------------------------------------------------------------------

export interface LoadoutSwitchboardDrawerProps {
  /** Whether the embedded switchboard drawer is open. */
  readonly open: boolean;
  /** Called when the user closes the drawer (close button, backdrop, or Escape). */
  readonly onClose: () => void;
  /** T3 Code WebSocket RPC bridge (automatically wrapped into `LoadoutBrowserTransport`). */
  readonly wsBridge?: T3CodeWsBridge;
  /** Pre-built `LoadoutBrowserTransport` (takes precedence over `wsBridge`). */
  readonly transport?: LoadoutBrowserTransport;
  /** Optional URL serving `switchboard.js` (e.g. from `loadout --dump-ui-js`). */
  readonly scriptUrl?: string;
  /** Optional URL serving `app.css`. */
  readonly cssUrl?: string;
  /** Initial or controlled drawer scope: `"global"` or `"worktree"`. */
  readonly scope?: SwitchboardDrawerScope;
  /** Optional callback when user switches between `Global targets` and `Current Git worktree`. */
  readonly onScopeChange?: (scope: SwitchboardDrawerScope) => void;
  /** Active thread worktree path when inspecting `"worktree"` scope. */
  readonly worktreePath?: string;
  /** Active repository name passed to `<loadout-switchboard repo-name="..." />`. */
  readonly repoName?: string;
  /** Active target instance id for the current T3 Code thread (e.g. `"claude-second"`). */
  readonly activeTargetId?: string;
  /** Optional callback for the footer `Open full switchboard` action. */
  readonly onOpenFullSwitchboard?: () => void;
  /** Optional `--loadout-*` CSS custom property overrides. */
  readonly theme?: Partial<LoadoutThemeTokens>;
  /** Drawer width (defaults to `"min(420px, 94vw)"` matching Artboard 6 compact drawer). */
  readonly width?: string | number;
  /** Drawer header title (defaults to `"Skills"`). */
  readonly title?: string;
}

export function LoadoutSwitchboardDrawer({
  open,
  onClose,
  wsBridge,
  transport,
  scriptUrl,
  cssUrl,
  scope,
  onScopeChange,
  worktreePath,
  repoName,
  activeTargetId,
  onOpenFullSwitchboard,
  theme,
  width = "min(420px, 94vw)",
  title = "Skills",
}: LoadoutSwitchboardDrawerProps): React.ReactElement | null {
  const switchboardRef = useRef<LoadoutSwitchboardElement | null>(null);
  const [internalScope, setInternalScope] = useState<SwitchboardDrawerScope>(
    scope ?? "global",
  );

  const activeScope = scope ?? internalScope;

  useEffect(() => {
    if (scope !== undefined) {
      setInternalScope(scope);
    }
  }, [scope]);

  const handleSelectScope = (next: SwitchboardDrawerScope) => {
    if (scope === undefined) {
      setInternalScope(next);
    }
    onScopeChange?.(next);
  };

  const resolvedTransport = useMemo<LoadoutBrowserTransport | undefined>(() => {
    if (transport) return transport;
    if (wsBridge) return createWsLoadoutTransport(wsBridge);
    return undefined;
  }, [transport, wsBridge]);

  const { ready, loadError } = useLoadoutComponentAssets(open, scriptUrl, cssUrl);

  // Install `window.LOADOUT_TRANSPORT` & direct `el.transport` property
  useEffect(() => {
    if (!open || !resolvedTransport || typeof window === "undefined") {
      return;
    }

    const previousTransport = window.LOADOUT_TRANSPORT;
    window.LOADOUT_TRANSPORT = resolvedTransport;

    const el = switchboardRef.current;
    if (el) {
      el.transport = resolvedTransport;
      el.mode = "embedded";
      el.scope = activeScope;
      el.worktreePath = worktreePath;
      el.repoName = repoName;
      el.activeTargetId = activeTargetId;
    }

    return () => {
      if (window.LOADOUT_TRANSPORT === resolvedTransport) {
        if (previousTransport === undefined) {
          delete window.LOADOUT_TRANSPORT;
        } else {
          window.LOADOUT_TRANSPORT = previousTransport;
        }
      }
    };
  }, [open, resolvedTransport, ready, activeScope, worktreePath, repoName, activeTargetId]);

  // Close drawer on Escape (when not captured by an inner modal inside `<loadout-switchboard>`)
  useEffect(() => {
    if (!open || typeof window === "undefined") return;
    const onKeyDown = (event: KeyboardEvent) => {
      if (event.key === "Escape" && !event.defaultPrevented) {
        onClose();
      }
    };
    window.addEventListener("keydown", onKeyDown);
    return () => window.removeEventListener("keydown", onKeyDown);
  }, [open, onClose]);

  const mergedThemeStyle = useMemo<React.CSSProperties>(
    () =>
      ({
        ...DEFAULT_T3CODE_LOADOUT_THEME,
        ...theme,
      }) as React.CSSProperties,
    [theme],
  );

  if (!open) return null;

  const segButtonStyle = (pressed: boolean): React.CSSProperties => ({
    flex: 1,
    border: 0,
    background: pressed ? "var(--loadout-surface-3)" : "transparent",
    color: pressed ? "var(--loadout-text)" : "var(--loadout-dim)",
    boxShadow: pressed ? "0 0 0 1px var(--loadout-border-2)" : "none",
    height: "24px",
    padding: "0 9px",
    borderRadius: "5px",
    fontSize: "11.5px",
    fontWeight: pressed ? 600 : 500,
    whiteSpace: "nowrap",
    cursor: "pointer",
  });

  return (
    <div
      role="dialog"
      aria-modal="true"
      aria-label={`${title} drawer`}
      style={{
        ...mergedThemeStyle,
        position: "fixed",
        inset: 0,
        zIndex: 60,
        display: "flex",
        justifyContent: "flex-end",
        backgroundColor: "rgba(5, 6, 8, 0.55)",
        backdropFilter: "blur(3px)",
      }}
      onClick={(e) => {
        if (e.target === e.currentTarget) {
          onClose();
        }
      }}
    >
      <aside
        aria-label="Skills drawer"
        style={{
          width: typeof width === "number" ? `${width}px` : width,
          maxWidth: "100vw",
          height: "100%",
          display: "flex",
          flexDirection: "column",
          backgroundColor: "var(--loadout-surface)",
          color: "var(--loadout-text)",
          borderLeft: "1px solid var(--loadout-border-2)",
          boxShadow: "0 24px 64px rgba(0, 0, 0, 0.55)",
          fontFamily: "var(--loadout-font-sans)",
        }}
      >
        <header
          style={{
            display: "grid",
            gap: "8px",
            padding: "10px 12px",
            borderBottom: "1px solid var(--loadout-border)",
            backgroundColor: "var(--loadout-surface)",
          }}
        >
          <div style={{ display: "flex", alignItems: "center", gap: "8px" }}>
            <b
              style={{
                fontFamily: "var(--loadout-font-mono)",
                fontSize: "13px",
                fontWeight: 700,
                letterSpacing: "-0.02em",
              }}
            >
              l
              <i style={{ color: "var(--loadout-brand-o)", fontStyle: "normal" }}>
                o
              </i>
              adout
            </b>
            <span style={{ fontSize: "13px", fontWeight: 600 }}>{title}</span>
            <span
              style={{
                fontFamily: "var(--loadout-font-mono)",
                fontSize: "10.5px",
                color: "var(--loadout-faint)",
                overflow: "hidden",
                textOverflow: "ellipsis",
                whiteSpace: "nowrap",
              }}
            >
              &lt;loadout-switchboard mode=&quot;embedded&quot;&gt;
            </span>
            <button
              type="button"
              onClick={onClose}
              aria-label="Close drawer"
              style={{
                marginLeft: "auto",
                cursor: "pointer",
                background: "transparent",
                color: "var(--loadout-dim)",
                border: "1px solid transparent",
                borderRadius: "6px",
                height: "24px",
                padding: "0 8px",
                fontSize: "12px",
                fontFamily: "var(--loadout-font-mono)",
              }}
            >
              ✕
            </button>
          </div>

          <div
            role="group"
            aria-label="Switchboard scope"
            style={{
              display: "inline-flex",
              border: "1px solid var(--loadout-border-2)",
              borderRadius: "7px",
              padding: "2px",
              backgroundColor: "var(--loadout-surface-2)",
              gap: "2px",
            }}
          >
            <button
              type="button"
              aria-pressed={activeScope === "global"}
              onClick={() => handleSelectScope("global")}
              style={segButtonStyle(activeScope === "global")}
            >
              Global targets
            </button>
            <button
              type="button"
              aria-pressed={activeScope === "worktree"}
              onClick={() => handleSelectScope("worktree")}
              style={segButtonStyle(activeScope === "worktree")}
            >
              Current Git worktree
            </button>
          </div>
        </header>

        <div
          style={{
            flex: 1,
            minHeight: 0,
            overflow: "auto",
            position: "relative",
          }}
        >
          {loadError ? (
            <div
              style={{
                padding: "18px 14px",
                fontFamily: "var(--loadout-font-mono)",
                fontSize: "11.5px",
                color: "var(--loadout-rose)",
              }}
            >
              {loadError}
            </div>
          ) : !ready ? (
            <div
              style={{
                padding: "18px 14px",
                fontFamily: "var(--loadout-font-mono)",
                fontSize: "11.5px",
                color: "var(--loadout-faint)",
              }}
            >
              Loading loadout switchboard...
            </div>
          ) : (
            <loadout-switchboard
              ref={switchboardRef}
              mode="embedded"
              scope={activeScope}
              worktree-path={worktreePath}
              repo-name={repoName}
              active-target={activeTargetId}
              style={{ display: "block", width: "100%", height: "100%" }}
            />
          )}
        </div>

        <footer
          style={{
            padding: "8px 12px",
            borderTop: "1px solid var(--loadout-border)",
            display: "flex",
            alignItems: "center",
            gap: "6px",
            fontFamily: "var(--loadout-font-mono)",
            fontSize: "11px",
            color: "var(--loadout-faint)",
            backgroundColor: "var(--loadout-surface)",
          }}
        >
          <span style={{ color: "var(--loadout-emerald)" }} aria-hidden="true">
            ●
          </span>
          <span>synced over host WebSocket</span>
          {onOpenFullSwitchboard && (
            <>
              <span aria-hidden="true">·</span>
              <button
                type="button"
                onClick={onOpenFullSwitchboard}
                style={{
                  border: 0,
                  background: "transparent",
                  color: "var(--loadout-dim)",
                  cursor: "pointer",
                  padding: "2px 6px",
                  borderRadius: "4px",
                  fontSize: "11px",
                  fontFamily: "var(--loadout-font-sans)",
                }}
              >
                Open full switchboard ↗
              </button>
            </>
          )}
        </footer>
      </aside>
    </div>
  );
}
