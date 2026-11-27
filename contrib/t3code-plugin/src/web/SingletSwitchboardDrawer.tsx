/**
 * @singlet/t3code-plugin: React 19 Embedded Switchboard Drawer (`apps/web`)
 *
 * Mounts `<singlet-switchboard mode="embedded" />` inside T3 Code's frontend
 * matching Artboard 6 (`.h-drawer`) tokens, segmented `Global targets` /
 * `Current Git worktree` switch, and WebSocket RPC bridge (`window.SINGLET_TRANSPORT`
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
  type SingletBrowserTransport,
  type SingletEvent,
  type SingletRpcMethod,
  type SingletRpcParams,
  type SingletRpcResult,
  type SingletSwitchboardEvent,
  type TargetAddParams,
  type TargetCloneParams,
  type TargetRemoveParams,
  type WorktreeHydrateParams,
} from "../protocol.js";

// ---------------------------------------------------------------------------
// Global DOM & React 19 JSX Type Augmentation for `<singlet-switchboard>`
// ---------------------------------------------------------------------------

export type SwitchboardDrawerScope = "global" | "worktree";

export interface SingletSwitchboardElement extends HTMLElement {
  transport?: SingletBrowserTransport;
  mode?: "standalone" | "embedded";
  scope?: SwitchboardDrawerScope;
  worktreePath?: string;
  repoName?: string;
  activeTargetId?: string;
  refresh?: () => Promise<void> | void;
}

declare global {
  interface Window {
    SINGLET_TRANSPORT?: SingletBrowserTransport;
  }
}

declare module "react" {
  namespace JSX {
    interface IntrinsicElements {
      "singlet-switchboard": React.DetailedHTMLProps<
        React.HTMLAttributes<SingletSwitchboardElement>,
        SingletSwitchboardElement
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
// T3 Code WebSocket RPC Bridge -> `window.SINGLET_TRANSPORT` Adapter
// ---------------------------------------------------------------------------

/**
 * Minimal interface implemented by T3 Code's `apps/web` WebSocket client to
 * forward `singlet` RPC requests to `apps/server`'s `SingletService`.
 */
export interface T3CodeWsBridge {
  call<M extends SingletRpcMethod>(
    method: M,
    params?: SingletRpcParams<M>,
  ): Promise<SingletRpcResult<M>>;
  subscribe?(
    listener: (event: SingletEvent | SingletSwitchboardEvent) => void,
  ): () => void;
}

/**
 * Wraps a T3 Code WebSocket RPC bridge into the `SingletBrowserTransport` shape
 * consumed by `<singlet-switchboard mode="embedded">`.
 */
export function createWsSingletTransport(
  bridge: T3CodeWsBridge,
): SingletBrowserTransport {
  const call = <M extends SingletRpcMethod>(
    method: M,
    params?: SingletRpcParams<M>,
  ): Promise<SingletRpcResult<M>> => bridge.call(method, params);

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
// Artboard 6 Theme Tokens (`--singlet-*` aligned with `prototype/index.html`)
// ---------------------------------------------------------------------------

export interface SingletThemeTokens {
  readonly "--singlet-bg": string;
  readonly "--singlet-surface": string;
  readonly "--singlet-surface-2": string;
  readonly "--singlet-surface-3": string;
  readonly "--singlet-border": string;
  readonly "--singlet-border-2": string;
  readonly "--singlet-text": string;
  readonly "--singlet-dim": string;
  readonly "--singlet-muted": string;
  readonly "--singlet-faint": string;
  readonly "--singlet-accent": string;
  readonly "--singlet-accent-dim": string;
  readonly "--singlet-on-accent": string;
  readonly "--singlet-brand-o": string;
  readonly "--singlet-emerald": string;
  readonly "--singlet-emerald-dim": string;
  readonly "--singlet-amber": string;
  readonly "--singlet-amber-dim": string;
  readonly "--singlet-rose": string;
  readonly "--singlet-rose-dim": string;
  readonly "--singlet-sky": string;
  readonly "--singlet-sky-dim": string;
  readonly "--singlet-font-sans": string;
  readonly "--singlet-font-mono": string;
}

export const DEFAULT_T3CODE_SINGLET_THEME: SingletThemeTokens = {
  "--singlet-bg": "var(--background, #0B0D10)",
  "--singlet-surface": "var(--card, #12151B)",
  "--singlet-surface-2": "var(--muted, #171B22)",
  "--singlet-surface-3": "var(--secondary, #1D222A)",
  "--singlet-border": "var(--border, #20252E)",
  "--singlet-border-2": "var(--input, #2C323D)",
  "--singlet-text": "var(--foreground, #E7E9EC)",
  "--singlet-dim": "var(--muted-foreground, #98A1B0)",
  "--singlet-muted": "var(--muted-foreground, #98A1B0)",
  "--singlet-faint": "#5D6676",
  "--singlet-accent": "#E0DDBD",
  "--singlet-accent-dim": "rgba(224, 221, 189, 0.16)",
  "--singlet-on-accent": "#0B0D10",
  "--singlet-brand-o": "#C8D46A",
  "--singlet-emerald": "#3DDC97",
  "--singlet-emerald-dim": "rgba(61, 220, 151, 0.14)",
  "--singlet-amber": "#F5B544",
  "--singlet-amber-dim": "rgba(245, 181, 68, 0.14)",
  "--singlet-rose": "#F2647A",
  "--singlet-rose-dim": "rgba(242, 100, 122, 0.14)",
  "--singlet-sky": "#6CB6FF",
  "--singlet-sky-dim": "rgba(108, 182, 255, 0.14)",
  "--singlet-font-sans":
    "var(--font-sans, Inter, ui-sans-serif, system-ui, -apple-system, 'Segoe UI', sans-serif)",
  "--singlet-font-mono":
    "var(--font-mono, 'JetBrains Mono', 'SF Mono', ui-monospace, Menlo, Consolas, monospace)",
};

// ---------------------------------------------------------------------------
// Lazy Web Component Script / CSS Loader
// ---------------------------------------------------------------------------

function useSingletComponentAssets(
  enabled: boolean,
  scriptUrl?: string,
  cssUrl?: string,
): { readonly ready: boolean; readonly loadError: string | null } {
  const isRegistered =
    typeof window !== "undefined" &&
    typeof window.customElements !== "undefined" &&
    Boolean(window.customElements.get("singlet-switchboard"));

  const [ready, setReady] = useState<boolean>(isRegistered || !scriptUrl);
  const [loadError, setLoadError] = useState<string | null>(null);

  useEffect(() => {
    if (!enabled || typeof document === "undefined") return;

    if (cssUrl) {
      const existingCss = document.querySelector<HTMLLinkElement>(
        `link[data-singlet-css="${CSS.escape(cssUrl)}"]`,
      );
      if (!existingCss) {
        const link = document.createElement("link");
        link.rel = "stylesheet";
        link.href = cssUrl;
        link.dataset.singletCss = cssUrl;
        document.head.appendChild(link);
      }
    }

    if (
      typeof window !== "undefined" &&
      window.customElements?.get("singlet-switchboard")
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
    const selector = `script[data-singlet-js="${CSS.escape(scriptUrl)}"]`;
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
          `Failed to load singlet switchboard bundle from "${scriptUrl}".`,
        );
      }
    };

    if (existingScript) {
      if (existingScript.dataset.singletLoaded === "true") {
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
    script.dataset.singletJs = scriptUrl;
    script.addEventListener("load", () => {
      script.dataset.singletLoaded = "true";
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
// `<SingletSwitchboardDrawer />` Component (Artboard 6 `.h-drawer`)
// ---------------------------------------------------------------------------

export interface SingletSwitchboardDrawerProps {
  /** Whether the embedded switchboard drawer is open. */
  readonly open: boolean;
  /** Called when the user closes the drawer (close button, backdrop, or Escape). */
  readonly onClose: () => void;
  /** T3 Code WebSocket RPC bridge (automatically wrapped into `SingletBrowserTransport`). */
  readonly wsBridge?: T3CodeWsBridge;
  /** Pre-built `SingletBrowserTransport` (takes precedence over `wsBridge`). */
  readonly transport?: SingletBrowserTransport;
  /** Optional URL serving `switchboard.js` (e.g. from `singlet --dump-ui-js`). */
  readonly scriptUrl?: string;
  /** Optional URL serving `app.css`. */
  readonly cssUrl?: string;
  /** Initial or controlled drawer scope: `"global"` or `"worktree"`. */
  readonly scope?: SwitchboardDrawerScope;
  /** Optional callback when user switches between `Global targets` and `Current Git worktree`. */
  readonly onScopeChange?: (scope: SwitchboardDrawerScope) => void;
  /** Active thread worktree path when inspecting `"worktree"` scope. */
  readonly worktreePath?: string;
  /** Active repository name passed to `<singlet-switchboard repo-name="..." />`. */
  readonly repoName?: string;
  /** Active target instance id for the current T3 Code thread (e.g. `"claude-second"`). */
  readonly activeTargetId?: string;
  /** Optional callback for the footer `Open full switchboard` action. */
  readonly onOpenFullSwitchboard?: () => void;
  /** Optional `--singlet-*` CSS custom property overrides. */
  readonly theme?: Partial<SingletThemeTokens>;
  /** Drawer width (defaults to `"min(420px, 94vw)"` matching Artboard 6 compact drawer). */
  readonly width?: string | number;
  /** Drawer header title (defaults to `"Skills"`). */
  readonly title?: string;
}

export function SingletSwitchboardDrawer({
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
}: SingletSwitchboardDrawerProps): React.ReactElement | null {
  const switchboardRef = useRef<SingletSwitchboardElement | null>(null);
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

  const resolvedTransport = useMemo<SingletBrowserTransport | undefined>(() => {
    if (transport) return transport;
    if (wsBridge) return createWsSingletTransport(wsBridge);
    return undefined;
  }, [transport, wsBridge]);

  const { ready, loadError } = useSingletComponentAssets(open, scriptUrl, cssUrl);

  // Install `window.SINGLET_TRANSPORT` & direct `el.transport` property
  useEffect(() => {
    if (!open || !resolvedTransport || typeof window === "undefined") {
      return;
    }

    const previousTransport = window.SINGLET_TRANSPORT;
    window.SINGLET_TRANSPORT = resolvedTransport;

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
      if (window.SINGLET_TRANSPORT === resolvedTransport) {
        if (previousTransport === undefined) {
          delete window.SINGLET_TRANSPORT;
        } else {
          window.SINGLET_TRANSPORT = previousTransport;
        }
      }
    };
  }, [open, resolvedTransport, ready, activeScope, worktreePath, repoName, activeTargetId]);

  // Close drawer on Escape (when not captured by an inner modal inside `<singlet-switchboard>`)
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
        ...DEFAULT_T3CODE_SINGLET_THEME,
        ...theme,
      }) as React.CSSProperties,
    [theme],
  );

  if (!open) return null;

  const segButtonStyle = (pressed: boolean): React.CSSProperties => ({
    flex: 1,
    border: 0,
    background: pressed ? "var(--singlet-surface-3)" : "transparent",
    color: pressed ? "var(--singlet-text)" : "var(--singlet-dim)",
    boxShadow: pressed ? "0 0 0 1px var(--singlet-border-2)" : "none",
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
          backgroundColor: "var(--singlet-surface)",
          color: "var(--singlet-text)",
          borderLeft: "1px solid var(--singlet-border-2)",
          boxShadow: "0 24px 64px rgba(0, 0, 0, 0.55)",
          fontFamily: "var(--singlet-font-sans)",
        }}
      >
        <header
          style={{
            display: "grid",
            gap: "8px",
            padding: "10px 12px",
            borderBottom: "1px solid var(--singlet-border)",
            backgroundColor: "var(--singlet-surface)",
          }}
        >
          <div style={{ display: "flex", alignItems: "center", gap: "8px" }}>
            <b
              style={{
                fontFamily: "var(--singlet-font-mono)",
                fontSize: "13px",
                fontWeight: 700,
                letterSpacing: "-0.02em",
              }}
            >
              s
              <i style={{ color: "var(--singlet-brand-o)", fontStyle: "normal" }}>
                i
              </i>
              nglet
            </b>
            <span style={{ fontSize: "13px", fontWeight: 600 }}>{title}</span>
            <span
              style={{
                fontFamily: "var(--singlet-font-mono)",
                fontSize: "10.5px",
                color: "var(--singlet-faint)",
                overflow: "hidden",
                textOverflow: "ellipsis",
                whiteSpace: "nowrap",
              }}
            >
              &lt;singlet-switchboard mode=&quot;embedded&quot;&gt;
            </span>
            <button
              type="button"
              onClick={onClose}
              aria-label="Close drawer"
              style={{
                marginLeft: "auto",
                cursor: "pointer",
                background: "transparent",
                color: "var(--singlet-dim)",
                border: "1px solid transparent",
                borderRadius: "6px",
                height: "24px",
                padding: "0 8px",
                fontSize: "12px",
                fontFamily: "var(--singlet-font-mono)",
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
              border: "1px solid var(--singlet-border-2)",
              borderRadius: "7px",
              padding: "2px",
              backgroundColor: "var(--singlet-surface-2)",
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
                fontFamily: "var(--singlet-font-mono)",
                fontSize: "11.5px",
                color: "var(--singlet-rose)",
              }}
            >
              {loadError}
            </div>
          ) : !ready ? (
            <div
              style={{
                padding: "18px 14px",
                fontFamily: "var(--singlet-font-mono)",
                fontSize: "11.5px",
                color: "var(--singlet-faint)",
              }}
            >
              Loading singlet switchboard...
            </div>
          ) : (
            <singlet-switchboard
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
            borderTop: "1px solid var(--singlet-border)",
            display: "flex",
            alignItems: "center",
            gap: "6px",
            fontFamily: "var(--singlet-font-mono)",
            fontSize: "11px",
            color: "var(--singlet-faint)",
            backgroundColor: "var(--singlet-surface)",
          }}
        >
          <span style={{ color: "var(--singlet-emerald)" }} aria-hidden="true">
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
                  color: "var(--singlet-dim)",
                  cursor: "pointer",
                  padding: "2px 6px",
                  borderRadius: "4px",
                  fontSize: "11px",
                  fontFamily: "var(--singlet-font-sans)",
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
