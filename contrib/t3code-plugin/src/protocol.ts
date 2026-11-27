/**
 * @loadout/t3code-plugin: Wire Protocol & Domain Types
 *
 * Exact TypeScript types, JSON-RPC 2.0 request/response envelopes,
 * browser transport contract, and environment-to-target resolution helpers
 * aligned with `loadout rpc` and `<loadout-switchboard>`.
 */

// ---------------------------------------------------------------------------
// Core Domain Enums & Primitives
// ---------------------------------------------------------------------------

export type AdapterKind =
  | "antigravity"
  | "claude-code"
  | "codex"
  | "opencode"
  | "zcode"
  | "cursor"
  | "windsurf"
  | "custom-lenient"
  | "custom-strict";

export type LinkMode = "symlink-dir" | "antigravity-skills-json";

export type EntryKind =
  | "canonical"
  | "managed-symlink"
  | "external-symlink"
  | "dangling-symlink"
  | "physical-copy"
  | "plugin-shadow";

export type SkillTargetCellStatus = EntryKind | "unwired";

export function isStrictAdapter(adapter: AdapterKind): boolean {
  return adapter === "antigravity" || adapter === "custom-strict";
}

export function normalizeSkillSlug(raw: string): string {
  return raw.trim().replace(/^[$@\/]+/, "");
}

// ---------------------------------------------------------------------------
// Catalog Snapshot Types (matches `loadout::core::to_catalog_json`)
// ---------------------------------------------------------------------------

export interface FrontmatterInfo {
  readonly hasFrontmatter: boolean;
  readonly name: string;
  readonly description: string;
  readonly tags: readonly string[];
  readonly approxTokens: number;
  readonly antigravityValid: boolean;
  readonly issues: readonly string[];
}

export interface TargetInstance {
  readonly id: string;
  readonly displayName: string;
  readonly group: string;
  readonly adapter: AdapterKind;
  readonly skillsDir: string;
  readonly configRoot?: string;
  readonly envHint?: string;
  readonly linkMode: LinkMode;
  readonly autoDiscovered: boolean;
  readonly configured: boolean;
  readonly activeSkillCount?: number;
  readonly approxTokens?: number;
}

export interface UnconfiguredSibling {
  readonly suggestedId: string;
  readonly displayName: string;
  readonly adapter: AdapterKind;
  readonly configRoot: string;
  readonly skillsDir: string;
  readonly envHint?: string;
  readonly group?: string;
}

export interface SkillOccurrence {
  readonly slug: string;
  readonly path: string;
  readonly realPath: string;
  readonly targetIdOrRepo: string;
  readonly entryKind: EntryKind;
  readonly isRepoLocal: boolean;
  readonly repoRoot?: string;
  readonly treeHash: string;
  readonly fileCount: number;
  readonly totalBytes: number;
  readonly mtimeEpoch: number;
  readonly frontmatter: FrontmatterInfo;
}

export interface TargetSkillState {
  readonly enabled: boolean;
  readonly hasEntry?: boolean;
  readonly entryKind?: EntryKind;
  readonly path?: string;
  readonly incompatible?: boolean;
}

export interface CanonicalSkill {
  readonly slug: string;
  readonly canonicalPath: string;
  readonly treeHash: string;
  readonly fileCount: number;
  readonly totalBytes: number;
  readonly approxTokens: number;
  readonly name: string;
  readonly description: string;
  readonly tags: readonly string[];
  readonly antigravityValid: boolean;
  readonly frontmatterIssues: readonly string[];
  readonly enabledIn?: Readonly<Record<string, boolean>>;
  readonly targetStates?: Readonly<Record<string, TargetSkillState>>;
  readonly targetStatus?: Readonly<Record<string, SkillTargetCellStatus>>;
  readonly occurrences: readonly SkillOccurrence[];
  readonly hasScripts?: boolean;
  readonly hasReferences?: boolean;
  readonly isRepoLocal?: boolean;
  readonly isRepoLocalOnly?: boolean;
}

/**
 * Checks whether a canonical skill is wired/enabled for a given target id,
 * supporting both `enabledIn[targetId]` and `targetStates[targetId].enabled`.
 */
export function isSkillWiredInTarget(
  skill: CanonicalSkill | undefined,
  targetId: string,
): boolean {
  if (!skill || !targetId) return false;
  if (skill.enabledIn && targetId in skill.enabledIn) {
    return Boolean(skill.enabledIn[targetId]);
  }
  const state = skill.targetStates?.[targetId];
  return Boolean(state?.enabled);
}

export interface DuplicateGroup {
  readonly slug: string;
  readonly treeHash: string;
  readonly redundantBytes: number;
  readonly occurrences: readonly SkillOccurrence[];
}

export interface DriftedVariant {
  readonly path: string;
  readonly targetIdOrRepo: string;
  readonly treeHash: string;
  readonly fileCount: number;
  readonly totalBytes: number;
  readonly mtimeEpoch: number;
  readonly entryKind: EntryKind;
  readonly isRepoLocal?: boolean;
  readonly diffFromBase?: string;
}

export interface DriftedGroup {
  readonly slug: string;
  readonly basePath: string;
  readonly unifiedDiff: string;
  readonly variants: readonly DriftedVariant[];
}

export interface BrokenLink {
  readonly slug: string;
  readonly path: string;
  readonly targetIdOrRepo: string;
  readonly rawTarget: string;
}

export interface CatalogStats {
  readonly totalSkills: number;
  readonly totalTargets: number;
  readonly configuredTargets: number;
  readonly exactDuplicates: number;
  readonly redundantBytes: number;
  readonly driftedSkills: number;
  readonly brokenLinks: number;
  readonly frontmatterWarnings: number;
  readonly repoLocalSkills: number;
}

export interface CatalogSnapshot {
  readonly stats: CatalogStats;
  readonly canonicalRoot: string;
  readonly lastScanMs: number;
  readonly canUndo: boolean;
  readonly activeProfile?: string;
  readonly profiles?: readonly ProfileEntry[];
  readonly unconfiguredSiblings: readonly (TargetInstance | UnconfiguredSibling)[];
  readonly targets: readonly TargetInstance[];
  readonly skills: readonly CanonicalSkill[];
  readonly duplicateGroups: readonly DuplicateGroup[];
  readonly driftedGroups: readonly DriftedGroup[];
  readonly brokenLinks: readonly BrokenLink[];
}

// ---------------------------------------------------------------------------
// JSON-RPC 2.0 Envelopes
// ---------------------------------------------------------------------------

export type JsonRpcId = number | string;

export interface JsonRpcRequest<
  M extends string = string,
  P = Record<string, unknown>,
> {
  readonly jsonrpc: "2.0";
  readonly id: JsonRpcId;
  readonly method: M;
  readonly params?: P;
}

export interface JsonRpcNotification<
  M extends string = string,
  P = Record<string, unknown>,
> {
  readonly jsonrpc: "2.0";
  readonly method: M;
  readonly params?: P;
}

export interface JsonRpcErrorObject {
  readonly code: number;
  readonly message: string;
  readonly data?: unknown;
}

export interface JsonRpcSuccessResponse<R = unknown> {
  readonly jsonrpc: "2.0";
  readonly id: JsonRpcId;
  readonly result: R;
}

export interface JsonRpcErrorResponse {
  readonly jsonrpc: "2.0";
  readonly id: JsonRpcId | null;
  readonly error: JsonRpcErrorObject;
}

export type JsonRpcResponse<R = unknown> =
  | JsonRpcSuccessResponse<R>
  | JsonRpcErrorResponse;

export function isJsonRpcErrorResponse(
  msg: JsonRpcResponse<unknown>,
): msg is JsonRpcErrorResponse {
  return "error" in msg && msg.error !== undefined;
}

// ---------------------------------------------------------------------------
// `loadout rpc` Method Contracts (1-to-1 with `src/surfaces/rpc.cpp`)
// ---------------------------------------------------------------------------

export const LOADOUT_RPC_METHODS = [
  "catalog.get",
  "catalog.scan",
  "skill.toggle",
  "skill.consolidate",
  "skill.resolveDrift",
  "skill.fixFrontmatter",
  "skill.get",
  "skill.save",
  "target.add",
  "target.remove",
  "target.clone",
  "quarantine.undo",
  "worktree.hydrate",
  "profile.list",
  "profile.save",
  "profile.apply",
  "profile.delete",
] as const;

export type LoadoutRpcMethod = (typeof LOADOUT_RPC_METHODS)[number];

export interface CatalogGetParams {}

export interface CatalogScanParams {
  readonly roots?: readonly string[];
}

export interface SkillToggleParams {
  readonly skill: string;
  readonly targetId: string;
  readonly enabled: boolean;
}

export interface SkillToggleResult {
  readonly ok: boolean;
  readonly skill: string;
  readonly targetId: string;
  readonly enabled: boolean;
  readonly linkPath?: string;
  readonly canonicalPath?: string;
  readonly catalog?: CatalogSnapshot;
}

export interface SkillConsolidateParams {
  readonly dryRun: boolean;
  readonly promoteRepoSkills?: readonly string[];
}

export interface ConsolidateAction {
  readonly slug: string;
  readonly action: "move-canonical" | "quarantine-and-link" | "keep-repo-local" | "link";
  readonly sourcePath: string;
  readonly destPath: string;
  readonly isRepoLocal?: boolean;
}

export interface QuarantineReceipt {
  readonly batchId: string;
  readonly quarantineDir: string;
  readonly timestamp: string;
  readonly movedCount: number;
  readonly entries?: readonly {
    readonly originalPath: string;
    readonly backupPath: string;
  }[];
}

export interface SkillConsolidateResult {
  readonly ok: boolean;
  readonly dryRun: boolean;
  readonly actions: readonly ConsolidateAction[];
  readonly consolidatedCount: number;
  readonly quarantinedCount: number;
  readonly bytesReclaimed: number;
  readonly quarantine?: QuarantineReceipt;
  readonly catalog?: CatalogSnapshot;
}

export interface SkillResolveDriftParams {
  readonly slug: string;
  readonly chosenPath: string;
  readonly keepBothSlug?: string;
}

export interface SkillResolveDriftResult {
  readonly ok: boolean;
  readonly slug: string;
  readonly chosenPath: string;
  readonly keepBothSlug?: string;
  readonly quarantine?: QuarantineReceipt;
  readonly catalog?: CatalogSnapshot;
}

export interface SkillFixFrontmatterParams {
  readonly skill: string;
  readonly apply: boolean;
}

export interface SkillFixFrontmatterResult {
  readonly ok: boolean;
  readonly skill: string;
  readonly applied: boolean;
  readonly before?: string;
  readonly after?: string;
  readonly unifiedDiff?: string;
  readonly antigravityValid?: boolean;
  readonly catalog?: CatalogSnapshot;
}

export interface SkillGetParams {
  readonly slug: string;
}

export interface SkillFileEntry {
  readonly path: string;
  readonly size: number;
  readonly isText: boolean;
  readonly content?: string;
}

export interface SkillDetail {
  readonly slug: string;
  readonly canonicalPath: string;
  readonly markdown: string;
  readonly files: readonly (SkillFileEntry | string)[];
  readonly filePaths?: readonly string[];
  readonly frontmatter: FrontmatterInfo;
  readonly enabledIn?: Readonly<Record<string, boolean>>;
  readonly targetStates?: Readonly<Record<string, TargetSkillState>>;
  readonly occurrences?: readonly SkillOccurrence[];
}

export interface SkillSaveParams {
  readonly slug: string;
  /** Markdown content when saving `SKILL.md` directly. */
  readonly markdown?: string;
  /** Optional relative file path inside `<canonicalRoot>/<slug>/` (e.g. `SKILL.md`, `scripts/check.sh`). */
  readonly relPath?: string;
  /** File contents when saving via `relPath`. */
  readonly content?: string;
}

export interface SkillSaveResult {
  readonly ok: boolean;
  readonly slug: string;
  readonly relPath?: string;
  readonly frontmatter?: FrontmatterInfo;
  readonly catalog?: CatalogSnapshot;
}

export interface ProfileEntry {
  readonly name: string;
  readonly skills: readonly string[];
  readonly wiring?: Readonly<Record<string, Readonly<Record<string, boolean>>>>;
}

export interface ProfileListParams {}

export interface ProfileListResult {
  readonly ok?: boolean;
  readonly activeProfile?: string;
  readonly profiles: readonly ProfileEntry[];
}

export interface ProfileSaveParams {
  readonly name: string;
  readonly skills?: readonly string[];
  readonly wiring?: Readonly<Record<string, Readonly<Record<string, boolean>>>>;
}

export interface ProfileSaveResult {
  readonly ok: boolean;
  readonly name: string;
  readonly message?: string;
  readonly activeProfile?: string;
  readonly profiles?: readonly ProfileEntry[];
  readonly catalog?: CatalogSnapshot;
}

export interface ProfileApplyParams {
  readonly name: string;
}

export interface ProfileApplyResult {
  readonly ok: boolean;
  readonly name: string;
  readonly message?: string;
  readonly activeProfile?: string;
  readonly profiles?: readonly ProfileEntry[];
  readonly catalog?: CatalogSnapshot;
}

export interface ProfileDeleteParams {
  readonly name: string;
}

export interface ProfileDeleteResult {
  readonly ok: boolean;
  readonly name: string;
  readonly message?: string;
  readonly activeProfile?: string;
  readonly profiles?: readonly ProfileEntry[];
  readonly catalog?: CatalogSnapshot;
}

export interface TargetAddParams {
  readonly id: string;
  readonly skillsDir: string;
  readonly displayName?: string;
  readonly adapter?: AdapterKind;
  readonly group?: string;
  readonly envHint?: string;
  readonly cloneFrom?: string;
}

export interface TargetAddResult {
  readonly ok: boolean;
  readonly target: TargetInstance;
  readonly clonedCount?: number;
  readonly catalog?: CatalogSnapshot;
}

export interface TargetRemoveParams {
  readonly id: string;
}

export interface TargetRemoveResult {
  readonly ok: boolean;
  readonly id: string;
  readonly unlinkedCount?: number;
  readonly catalog?: CatalogSnapshot;
}

export interface TargetCloneParams {
  readonly fromId: string;
  readonly toId: string;
}

export interface TargetCloneResult {
  readonly ok: boolean;
  readonly fromId: string;
  readonly toId: string;
  readonly clonedCount: number;
  readonly catalog?: CatalogSnapshot;
}

export interface QuarantineUndoParams {}

export interface QuarantineUndoResult {
  readonly ok: boolean;
  readonly restoredBatchId?: string;
  readonly restoredCount: number;
  readonly message?: string;
  readonly catalog?: CatalogSnapshot;
}

export interface WorktreeHydrateParams {
  readonly worktreePath: string;
  readonly repoRoot?: string;
  readonly providerId?: string;
  readonly env?: Readonly<Record<string, string>>;
}

export interface WorktreeHydrateResult {
  readonly ok: boolean;
  readonly worktreePath: string;
  readonly resolvedTargetId?: string;
  readonly linkedSkills: readonly string[];
  readonly excludedPaths: readonly string[];
  readonly excludeFile?: string;
}

export interface LoadoutRpcContract {
  "catalog.get": {
    params: CatalogGetParams;
    result: CatalogSnapshot;
  };
  "catalog.scan": {
    params: CatalogScanParams;
    result: CatalogSnapshot;
  };
  "skill.toggle": {
    params: SkillToggleParams;
    result: SkillToggleResult;
  };
  "skill.consolidate": {
    params: SkillConsolidateParams;
    result: SkillConsolidateResult;
  };
  "skill.resolveDrift": {
    params: SkillResolveDriftParams;
    result: SkillResolveDriftResult;
  };
  "skill.fixFrontmatter": {
    params: SkillFixFrontmatterParams;
    result: SkillFixFrontmatterResult;
  };
  "skill.get": {
    params: SkillGetParams;
    result: SkillDetail;
  };
  "skill.save": {
    params: SkillSaveParams;
    result: SkillSaveResult;
  };
  "target.add": {
    params: TargetAddParams;
    result: TargetAddResult;
  };
  "target.remove": {
    params: TargetRemoveParams;
    result: TargetRemoveResult;
  };
  "target.clone": {
    params: TargetCloneParams;
    result: TargetCloneResult;
  };
  "quarantine.undo": {
    params: QuarantineUndoParams;
    result: QuarantineUndoResult;
  };
  "worktree.hydrate": {
    params: WorktreeHydrateParams;
    result: WorktreeHydrateResult;
  };
  "profile.list": {
    params: ProfileListParams;
    result: ProfileListResult;
  };
  "profile.save": {
    params: ProfileSaveParams;
    result: ProfileSaveResult;
  };
  "profile.apply": {
    params: ProfileApplyParams;
    result: ProfileApplyResult;
  };
  "profile.delete": {
    params: ProfileDeleteParams;
    result: ProfileDeleteResult;
  };
}

export type LoadoutRpcParams<M extends LoadoutRpcMethod> = LoadoutRpcContract[M]["params"];
export type LoadoutRpcResult<M extends LoadoutRpcMethod> = LoadoutRpcContract[M]["result"];

// ---------------------------------------------------------------------------
// Reactive Event Stream Types
// ---------------------------------------------------------------------------

export type LoadoutEvent =
  | {
      readonly type: "catalog.updated";
      readonly catalog: CatalogSnapshot;
      readonly reason?: string;
      readonly timestamp: string;
    }
  | {
      readonly type: "skill.toggled";
      readonly skill: string;
      readonly targetId: string;
      readonly enabled: boolean;
      readonly timestamp: string;
    }
  | {
      readonly type: "worktree.hydrated";
      readonly worktreePath: string;
      readonly resolvedTargetId?: string;
      readonly linkedSkills: readonly string[];
      readonly timestamp: string;
    }
  | {
      readonly type: "rpc.connected";
      readonly pid?: number;
      readonly timestamp: string;
    }
  | {
      readonly type: "rpc.disconnected";
      readonly exitCode?: number | null;
      readonly signal?: string | null;
      readonly timestamp: string;
    }
  | {
      readonly type: "rpc.error";
      readonly message: string;
      readonly timestamp: string;
    };

export type LoadoutCatalogEvent = LoadoutEvent;

/**
 * Event shape consumed directly by `<loadout-switchboard>`'s `transport.subscribe`.
 */
export type LoadoutSwitchboardEvent =
  | { readonly type: "catalog"; readonly data: CatalogSnapshot }
  | { readonly type: "connection"; readonly connected: boolean }
  | { readonly type: "refresh" };

export function toSwitchboardEvent(
  event: LoadoutEvent | LoadoutSwitchboardEvent,
): LoadoutSwitchboardEvent | undefined {
  switch (event.type) {
    case "catalog":
    case "connection":
    case "refresh":
      return event;
    case "catalog.updated":
      return { type: "catalog", data: event.catalog };
    case "rpc.connected":
      return { type: "connection", connected: true };
    case "rpc.disconnected":
      return { type: "connection", connected: false };
    case "skill.toggled":
    case "worktree.hydrated":
      return { type: "refresh" };
    case "rpc.error":
      return undefined;
  }
}

// ---------------------------------------------------------------------------
// Embedded `<loadout-switchboard>` Browser Transport Contract
// ---------------------------------------------------------------------------

export interface ManageTargetParams {
  readonly action: "add" | "remove" | "clone";
  readonly id?: string;
  readonly displayName?: string;
  readonly skillsDir?: string;
  readonly adapter?: AdapterKind;
  readonly group?: string;
  readonly envHint?: string;
  readonly fromId?: string;
  readonly toId?: string;
  readonly cloneFrom?: string;
}

export interface LoadoutBrowserTransport {
  getCatalog(): Promise<CatalogSnapshot>;
  scan(params?: CatalogScanParams): Promise<CatalogSnapshot>;
  toggle(params: SkillToggleParams): Promise<SkillToggleResult>;
  consolidate(params: SkillConsolidateParams): Promise<SkillConsolidateResult>;
  resolveDrift(params: SkillResolveDriftParams): Promise<SkillResolveDriftResult>;
  manageTarget(
    params: ManageTargetParams,
  ): Promise<TargetAddResult | TargetRemoveResult | TargetCloneResult>;
  /** Alias for `manageTarget` matching REST `/api/targets` */
  targets?(
    params: ManageTargetParams,
  ): Promise<TargetAddResult | TargetRemoveResult | TargetCloneResult>;
  addTarget?(params: TargetAddParams): Promise<TargetAddResult>;
  removeTarget?(params: TargetRemoveParams): Promise<TargetRemoveResult>;
  cloneTarget?(params: TargetCloneParams): Promise<TargetCloneResult>;
  fixFrontmatter(
    params: SkillFixFrontmatterParams,
  ): Promise<SkillFixFrontmatterResult>;
  getSkill(slug: string): Promise<SkillDetail>;
  saveSkill(params: SkillSaveParams): Promise<SkillSaveResult>;
  undo(): Promise<QuarantineUndoResult>;
  listProfiles?(): Promise<ProfileListResult>;
  saveProfile?(params: ProfileSaveParams): Promise<ProfileSaveResult>;
  applyProfile?(params: ProfileApplyParams): Promise<ProfileApplyResult>;
  deleteProfile?(params: ProfileDeleteParams): Promise<ProfileDeleteResult>;
  hydrateWorktree?(params: WorktreeHydrateParams): Promise<WorktreeHydrateResult>;
  subscribe?(listener: (event: LoadoutSwitchboardEvent) => void): () => void;
  /** Generic JSON-RPC 2.0 dispatch compatible with `<loadout-switchboard>` */
  rpc?<M extends LoadoutRpcMethod>(
    method: M,
    params?: LoadoutRpcParams<M>,
  ): Promise<LoadoutRpcResult<M>>;
  call?<M extends LoadoutRpcMethod>(
    method: M,
    params?: LoadoutRpcParams<M>,
  ): Promise<LoadoutRpcResult<M>>;
  request?<M extends LoadoutRpcMethod>(
    method: M,
    params?: LoadoutRpcParams<M>,
  ): Promise<LoadoutRpcResult<M>>;
}

// ---------------------------------------------------------------------------
// Environment-to-Target Resolver (`CLAUDE_CONFIG_DIR`, `CODEX_HOME`, etc.)
// ---------------------------------------------------------------------------

export interface ResolveTargetOptions {
  /** Override home directory used when expanding `~` (defaults to `env.LOADOUT_HOME ?? env.HOME`). */
  readonly homeDir?: string;
  /**
   * When a provider-specific env var (e.g. `CLAUDE_CONFIG_DIR=~/.claude-third`)
   * is set but matches no registered target, return `undefined` by default so
   * callers do not silently fall back to `claude-main`. Set to `true` to force
   * falling back to the provider's default target.
   */
  readonly fallbackToDefaultOnUnmatchedEnv?: boolean;
}

const PROVIDER_ADAPTER_ALIASES: Readonly<Record<string, AdapterKind>> = {
  claude: "claude-code",
  "claude-code": "claude-code",
  claude_code: "claude-code",
  claudecode: "claude-code",
  anthropic: "claude-code",
  codex: "codex",
  "codex-cli": "codex",
  openai: "codex",
  opencode: "opencode",
  "open-code": "opencode",
  antigravity: "antigravity",
  gemini: "antigravity",
  "gemini-antigravity": "antigravity",
  agy: "antigravity",
  zcode: "zcode",
  zai: "zcode",
  cursor: "cursor",
  windsurf: "windsurf",
  codeium: "windsurf",
};

const ADAPTER_ENV_VARS: Readonly<Record<AdapterKind, readonly string[]>> = {
  "claude-code": ["CLAUDE_CONFIG_DIR"],
  codex: ["CODEX_HOME"],
  opencode: ["OPENCODE_CONFIG_DIR", "OPENCODE_HOME"],
  antigravity: ["ANTIGRAVITY_CONFIG_DIR", "GEMINI_CONFIG_DIR", "GEMINI_HOME"],
  zcode: ["ZCODE_HOME", "ZCODE_CONFIG_DIR"],
  cursor: ["CURSOR_CONFIG_DIR", "CURSOR_HOME"],
  windsurf: ["WINDSURF_CONFIG_DIR", "CODEIUM_HOME"],
  "custom-lenient": [],
  "custom-strict": [],
};

const DEFAULT_CONFIG_SUBDIRS: Readonly<Record<AdapterKind, readonly string[]>> = {
  "claude-code": ["~/.claude"],
  codex: ["~/.codex"],
  opencode: ["~/.config/opencode"],
  antigravity: ["~/.gemini/config", "~/.gemini"],
  zcode: ["~/.zcode"],
  cursor: ["~/.cursor"],
  windsurf: ["~/.codeium/windsurf"],
  "custom-lenient": [],
  "custom-strict": [],
};

function normalizePath(rawPath: string, homeDir?: string): string {
  let p = rawPath.trim();
  if (!p) return "";

  if (homeDir) {
    const cleanHome = homeDir.replace(/\/+$/, "");
    if (p === "~") {
      p = cleanHome;
    } else if (p.startsWith("~/")) {
      p = `${cleanHome}${p.slice(1)}`;
    } else if (p.startsWith("$HOME/")) {
      p = `${cleanHome}${p.slice(5)}`;
    } else if (p === "$HOME" || p === "${HOME}") {
      p = cleanHome;
    } else if (p.startsWith("${HOME}/")) {
      p = `${cleanHome}${p.slice(7)}`;
    }
  }

  // Collapse duplicate slashes and strip trailing slash (preserve root "/")
  p = p.replace(/\/+/g, "/");
  if (p.length > 1 && p.endsWith("/")) {
    p = p.slice(0, -1);
  }
  return p;
}

function dirnamePosix(p: string): string {
  const idx = p.lastIndexOf("/");
  if (idx <= 0) return idx === 0 ? "/" : "";
  return p.slice(0, idx);
}

function parseEnvHint(hint?: string): { key: string; value: string } | undefined {
  if (!hint) return undefined;
  const eq = hint.indexOf("=");
  if (eq <= 0) return undefined;
  const key = hint.slice(0, eq).trim();
  const value = hint
    .slice(eq + 1)
    .trim()
    .replace(/^['"]|['"]$/g, "");
  if (!key || !value) return undefined;
  return { key, value };
}

function targetMatchesConfigDir(
  target: TargetInstance,
  normalizedDir: string,
  homeDir?: string,
): boolean {
  if (!normalizedDir) return false;

  if (target.configRoot) {
    if (normalizePath(target.configRoot, homeDir) === normalizedDir) {
      return true;
    }
  }

  const skillsNorm = normalizePath(target.skillsDir, homeDir);
  if (skillsNorm === normalizedDir) {
    return true;
  }

  // e.g. skillsDir = "~/.claude-second/skills" -> parent is "~/.claude-second"
  if (dirnamePosix(skillsNorm) === normalizedDir) {
    return true;
  }

  // e.g. antigravity skillsDir = "~/.gemini/config/skills" -> grandparent "~/.gemini"
  if (
    target.adapter === "antigravity" &&
    dirnamePosix(dirnamePosix(skillsNorm)) === normalizedDir
  ) {
    return true;
  }

  return false;
}

/**
 * Resolves the exact `loadout` `TargetInstance` for a T3 Code provider & process
 * environment map (e.g., matching `CLAUDE_CONFIG_DIR=~/.claude-second` to
 * `claude-second`, or `CODEX_HOME=~/.codex-work` to `codex-work`).
 */
export function resolveTargetForEnv(
  targets: readonly TargetInstance[],
  providerId: string | undefined,
  env: Readonly<Record<string, string | undefined>> = {},
  options: ResolveTargetOptions = {},
): TargetInstance | undefined {
  if (targets.length === 0) return undefined;

  const homeDir = options.homeDir ?? env.LOADOUT_HOME ?? env.HOME;

  // 1. Explicit target override via LOADOUT_TARGET_ID
  const explicitTargetId = env.LOADOUT_TARGET_ID?.trim();
  if (explicitTargetId) {
    const exact = targets.find((t) => t.id === explicitTargetId);
    if (exact) return exact;
  }

  // 2. Direct target id match if caller passed a loadout target id as providerId
  const normalizedProvider = providerId?.trim().toLowerCase();
  if (normalizedProvider) {
    const directById = targets.find(
      (t) => t.id.toLowerCase() === normalizedProvider,
    );
    if (directById) {
      // Only return immediately if no provider config env var overrides it
      const overrideVars = ADAPTER_ENV_VARS[directById.adapter] ?? [];
      const hasEnvOverride = overrideVars.some((k) => Boolean(env[k]?.trim()));
      if (!hasEnvOverride) {
        return directById;
      }
    }
  }

  const adapter: AdapterKind | undefined = normalizedProvider
    ? PROVIDER_ADAPTER_ALIASES[normalizedProvider]
    : undefined;

  const candidateTargets = adapter
    ? targets.filter((t) => t.adapter === adapter)
    : targets;

  // 3. Exact `envHint` match (e.g. `CLAUDE_CONFIG_DIR=~/.claude-second`)
  for (const target of candidateTargets) {
    const parsed = parseEnvHint(target.envHint);
    if (!parsed) continue;
    const rawEnvVal = env[parsed.key];
    if (!rawEnvVal || !rawEnvVal.trim()) continue;
    if (
      normalizePath(rawEnvVal, homeDir) ===
      normalizePath(parsed.value, homeDir)
    ) {
      return target;
    }
  }

  // Also check all targets for an explicit envHint match even if providerId was omitted/custom
  if (candidateTargets !== targets) {
    for (const target of targets) {
      const parsed = parseEnvHint(target.envHint);
      if (!parsed) continue;
      const rawEnvVal = env[parsed.key];
      if (!rawEnvVal || !rawEnvVal.trim()) continue;
      if (
        normalizePath(rawEnvVal, homeDir) ===
        normalizePath(parsed.value, homeDir)
      ) {
        return target;
      }
    }
  }

  // 4. Known provider environment variables matched against `configRoot` / `skillsDir`
  const envKeysToCheck: string[] = adapter
    ? [...ADAPTER_ENV_VARS[adapter]]
    : Object.values(ADAPTER_ENV_VARS).flat();

  let sawExplicitProviderEnvVar = false;
  for (const key of envKeysToCheck) {
    const rawVal = env[key]?.trim();
    if (!rawVal) continue;
    sawExplicitProviderEnvVar = true;
    const normalizedDir = normalizePath(rawVal, homeDir);
    const match = candidateTargets.find((t) =>
      targetMatchesConfigDir(t, normalizedDir, homeDir),
    );
    if (match) return match;
  }

  // If the caller explicitly set e.g. CLAUDE_CONFIG_DIR=~/.claude-unconfigured,
  // avoid silently returning claude-main unless explicitly requested.
  if (sawExplicitProviderEnvVar && !options.fallbackToDefaultOnUnmatchedEnv) {
    return undefined;
  }

  if (!adapter || candidateTargets.length === 0) {
    return undefined;
  }

  // 5. Primary / default target for the resolved adapter
  const defaultSubdirs = DEFAULT_CONFIG_SUBDIRS[adapter].map((d) =>
    normalizePath(d, homeDir),
  );
  for (const defaultDir of defaultSubdirs) {
    const defaultMatch = candidateTargets.find((t) =>
      targetMatchesConfigDir(t, defaultDir, homeDir),
    );
    if (defaultMatch) return defaultMatch;
  }

  const mainById = candidateTargets.find(
    (t) =>
      t.id === adapter ||
      t.id === `${normalizedProvider}-main` ||
      t.id.endsWith("-main") ||
      t.id === "claude-main",
  );
  if (mainById) return mainById;

  return (
    candidateTargets.find((t) => t.configured) ?? candidateTargets[0]
  );
}
