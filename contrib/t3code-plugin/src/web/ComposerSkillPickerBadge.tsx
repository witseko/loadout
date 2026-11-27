/**
 * @singlet/t3code-plugin: Composer `$` Skill Picker Target-Wiring Badge (`apps/web`)
 *
 * Renders inline target-compatibility pills next to skills inside T3 Code's
 * `$` autocomplete menu matching Artboard 6 tokens (`#E0DDBD` accent, `#3DDC97`
 * emerald `Wired in <Target>`, `#F2647A` rose `Frontmatter ✗`, `#F5B544` amber
 * `1-click wire` + `Wire & Insert` primary CTA).
 */

import React, { useMemo, useState } from "react";
import {
  isSkillWiredInTarget,
  isStrictAdapter,
  normalizeSkillSlug,
  resolveTargetForEnv,
  type CanonicalSkill,
  type CatalogSnapshot,
  type SingletBrowserTransport,
  type TargetInstance,
} from "../protocol.js";
import {
  createWsSingletTransport,
  type T3CodeWsBridge,
} from "./SingletSwitchboardDrawer.js";

export interface ComposerSkillPickerBadgeProps {
  /** Either a full `CanonicalSkill` object or a skill slug (with optional leading `$`). */
  readonly skill: CanonicalSkill | string;
  /** Latest `singlet` catalog snapshot (used to resolve `targets` and/or lookup `skill` by slug). */
  readonly catalog?: CatalogSnapshot;
  /** Active thread provider id in T3 Code (e.g. `"claude"`, `"codex"`, `"opencode"`). */
  readonly providerId?: string;
  /** Active thread provider env map (e.g. `{ CLAUDE_CONFIG_DIR: "~/.claude-second" }`). */
  readonly providerEnv?: Readonly<Record<string, string | undefined>>;
  /** Optional explicit target id override (skips `resolveTargetForEnv` when provided). */
  readonly targetId?: string;
  /** Pre-built browser transport; falls back to `wsBridge` or `window.SINGLET_TRANSPORT`. */
  readonly transport?: SingletBrowserTransport;
  /** T3 Code WebSocket RPC bridge used if `transport` is not passed directly. */
  readonly wsBridge?: T3CodeWsBridge;
  /** Called right after `"Wire & Insert"` succeeds so the composer can insert `$<slug>`. */
  readonly onWireAndUse?: (slug: string, target: TargetInstance) => void;
  /** Optional callback when the user clicks `Insert` on an already-wired skill. */
  readonly onInsert?: (slug: string, target: TargetInstance) => void;
  /** Optional callback after the toggle RPC succeeds (even without composer insert). */
  readonly onWired?: (slug: string, target: TargetInstance) => void;
  /** Optional error callback when wiring fails. */
  readonly onError?: (error: Error) => void;
  /** Show compact 2-char glyphs of other targets where this skill is wired (Artboard 6). */
  readonly showOtherTargets?: boolean;
  /** Compact layout omitting secondary pills for tight autocomplete rows. */
  readonly compact?: boolean;
  /** Additional className applied to the outer pill container. */
  readonly className?: string;
}

const BADGE_BASE: React.CSSProperties = {
  display: "inline-flex",
  alignItems: "center",
  gap: "4px",
  height: "18px",
  padding: "0 6px",
  borderRadius: "4px",
  fontFamily:
    "var(--singlet-font-mono, var(--font-mono, 'JetBrains Mono', 'SF Mono', ui-monospace, monospace))",
  fontSize: "10.5px",
  fontWeight: 500,
  lineHeight: 1,
  whiteSpace: "nowrap",
  userSelect: "none",
};

const BTN_SM_BASE: React.CSSProperties = {
  display: "inline-flex",
  alignItems: "center",
  gap: "5px",
  height: "24px",
  padding: "0 8px",
  borderRadius: "6px",
  fontFamily:
    "var(--singlet-font-sans, var(--font-sans, Inter, system-ui, sans-serif))",
  fontSize: "11.5px",
  whiteSpace: "nowrap",
};

function targetGlyph(target: TargetInstance): string {
  const id = target.id.toLowerCase();
  if (id.includes("second") || id.includes("work")) return "C2";
  switch (target.adapter) {
    case "claude-code":
      return "CC";
    case "antigravity":
      return "AG";
    case "codex":
      return "CX";
    case "opencode":
      return "OC";
    case "zcode":
      return "ZC";
    case "cursor":
      return "CU";
    case "windsurf":
      return "WS";
    default:
      return target.displayName.slice(0, 2).toUpperCase();
  }
}

export function ComposerSkillPickerBadge({
  skill,
  catalog,
  providerId,
  providerEnv,
  targetId,
  transport,
  wsBridge,
  onWireAndUse,
  onInsert,
  onWired,
  onError,
  showOtherTargets = true,
  compact = false,
  className,
}: ComposerSkillPickerBadgeProps): React.ReactElement | null {
  const [wiring, setWiring] = useState(false);
  const [optimisticWiredTargetId, setOptimisticWiredTargetId] = useState<
    string | null
  >(null);
  const [errorMsg, setErrorMsg] = useState<string | null>(null);

  const resolvedTransport = useMemo<SingletBrowserTransport | undefined>(() => {
    if (transport) return transport;
    if (wsBridge) return createWsSingletTransport(wsBridge);
    if (typeof window !== "undefined") return window.SINGLET_TRANSPORT;
    return undefined;
  }, [transport, wsBridge]);

  const resolvedSkill = useMemo<CanonicalSkill | undefined>(() => {
    if (typeof skill !== "string") return skill;
    const slug = normalizeSkillSlug(skill);
    return catalog?.skills.find((s) => s.slug === slug);
  }, [skill, catalog]);

  const slug =
    typeof skill === "string" ? normalizeSkillSlug(skill) : skill.slug;

  const resolvedTarget = useMemo<TargetInstance | undefined>(() => {
    const targets = catalog?.targets ?? [];
    if (targets.length === 0) return undefined;
    if (targetId) {
      return targets.find((t) => t.id === targetId);
    }
    return resolveTargetForEnv(targets, providerId, providerEnv);
  }, [catalog, targetId, providerId, providerEnv]);

  const otherTargetGlyphs = useMemo<readonly string[]>(() => {
    if (!showOtherTargets || !catalog || !resolvedSkill || !resolvedTarget) {
      return [];
    }
    return catalog.targets
      .filter(
        (t) =>
          t.id !== resolvedTarget.id && isSkillWiredInTarget(resolvedSkill, t.id),
      )
      .slice(0, 4)
      .map(targetGlyph);
  }, [showOtherTargets, catalog, resolvedSkill, resolvedTarget]);

  if (!resolvedSkill && !resolvedTarget) {
    return null;
  }

  const target = resolvedTarget;
  const isWired = Boolean(
    target &&
      (optimisticWiredTargetId === target.id ||
        isSkillWiredInTarget(resolvedSkill, target.id)),
  );
  const isStrictTarget = target ? isStrictAdapter(target.adapter) : false;
  const hasFrontmatterIssue = Boolean(
    isStrictTarget && resolvedSkill && !resolvedSkill.antigravityValid,
  );

  const handleWireAndInsert = async (
    event: React.MouseEvent<HTMLButtonElement>,
  ) => {
    event.preventDefault();
    event.stopPropagation();

    if (!target || !resolvedTransport || wiring) return;

    setWiring(true);
    setErrorMsg(null);
    try {
      await resolvedTransport.toggle({
        skill: slug,
        targetId: target.id,
        enabled: true,
      });
      setOptimisticWiredTargetId(target.id);
      onWired?.(slug, target);
      onWireAndUse?.(slug, target);
    } catch (err) {
      const error = err instanceof Error ? err : new Error(String(err));
      setErrorMsg(error.message);
      onError?.(error);
    } finally {
      setWiring(false);
    }
  };

  const handleInsert = (event: React.MouseEvent<HTMLButtonElement>) => {
    event.preventDefault();
    event.stopPropagation();
    if (!target) return;
    if (onInsert) {
      onInsert(slug, target);
    } else {
      onWireAndUse?.(slug, target);
    }
  };

  return (
    <span
      className={className}
      style={{
        display: "inline-flex",
        alignItems: "center",
        gap: "6px",
        flexShrink: 0,
      }}
    >
      {!compact && otherTargetGlyphs.length > 0 && (
        <span
          title={`Also wired in: ${otherTargetGlyphs.join(", ")}`}
          style={{
            fontFamily:
              "var(--singlet-font-mono, var(--font-mono, 'JetBrains Mono', ui-monospace, monospace))",
            fontSize: "10px",
            color: "var(--singlet-faint, #5D6676)",
          }}
        >
          {otherTargetGlyphs.join(" ")}
        </span>
      )}

      {!target ? (
        <span
          title={`No singlet target matched provider "${providerId ?? "unknown"}" for this environment`}
          style={{
            ...BADGE_BASE,
            color: "var(--singlet-amber, #F5B544)",
            backgroundColor: "var(--singlet-amber-dim, rgba(245, 181, 68, 0.14))",
          }}
        >
          Unconfigured account
        </span>
      ) : hasFrontmatterIssue ? (
        <span
          title={
            resolvedSkill?.frontmatterIssues.join("; ") ||
            "Missing or mismatched YAML frontmatter (required by strict targets)"
          }
          style={{
            ...BADGE_BASE,
            color: "var(--singlet-rose, #F2647A)",
            backgroundColor: "var(--singlet-rose-dim, rgba(242, 100, 122, 0.14))",
          }}
        >
          {compact
            ? "Frontmatter ✗"
            : `Frontmatter ✗ for ${target.displayName}`}
        </span>
      ) : isWired ? (
        <>
          <span
            title={`Symlink wired in ${target.skillsDir}`}
            style={{
              ...BADGE_BASE,
              color: "var(--singlet-emerald, #3DDC97)",
              backgroundColor:
                "var(--singlet-emerald-dim, rgba(61, 220, 151, 0.14))",
            }}
          >
            {compact ? target.displayName : `Wired in ${target.displayName}`}
          </span>
          {(onInsert || (!compact && onWireAndUse)) && (
            <button
              type="button"
              onClick={handleInsert}
              style={{
                ...BTN_SM_BASE,
                cursor: "pointer",
                border: "1px solid var(--singlet-border-2, #2C323D)",
                backgroundColor: "var(--singlet-surface-2, #171B22)",
                color: "var(--singlet-text, #E7E9EC)",
              }}
            >
              Insert
            </button>
          )}
        </>
      ) : (
        <>
          {!compact && (
            <span
              title={
                errorMsg
                  ? `Wire failed: ${errorMsg}`
                  : `Not wired in ${target.skillsDir}`
              }
              style={{
                ...BADGE_BASE,
                color: errorMsg
                  ? "var(--singlet-rose, #F2647A)"
                  : "var(--singlet-amber, #F5B544)",
                backgroundColor: errorMsg
                  ? "var(--singlet-rose-dim, rgba(242, 100, 122, 0.14))"
                  : "var(--singlet-amber-dim, rgba(245, 181, 68, 0.14))",
              }}
            >
              {errorMsg ? "Wire failed" : `1-click wire for ${target.displayName}`}
            </span>
          )}
          <button
            type="button"
            disabled={wiring || !resolvedTransport}
            onClick={handleWireAndInsert}
            title={
              errorMsg
                ? `Failed to wire: ${errorMsg}`
                : `Create symlink in ${target.skillsDir} and insert $${slug}`
            }
            style={{
              ...BTN_SM_BASE,
              cursor: wiring || !resolvedTransport ? "not-allowed" : "pointer",
              border: "1px solid var(--singlet-accent, #E0DDBD)",
              backgroundColor: "var(--singlet-accent, #E0DDBD)",
              color: "var(--singlet-on-accent, #0B0D10)",
              fontWeight: 600,
              opacity: wiring || !resolvedTransport ? 0.55 : 1,
            }}
          >
            {wiring ? "Wiring..." : "Wire & Insert"}
          </button>
        </>
      )}
    </span>
  );
}
