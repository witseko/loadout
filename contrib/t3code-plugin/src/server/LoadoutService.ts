/**
 * @loadout/t3code-plugin: Effect-TS Backend Service (`apps/server`)
 *
 * Idiomatic Effect-TS `Context.Tag` + `Layer.scoped` integration for
 * `pingdotgg/t3code`'s `apps/server`:
 * - Spawns and supervises `loadout rpc` inside the Effect Scope
 * - Exposes typed catalog/skill/target/quarantine/profile operations & `events` Stream
 * - Provides T3 Code lifecycle hooks:
 *   1. `onWorktreeCreated`   -> hydrates untracked skill symlinks excluded via `.git/info/exclude`
 *   2. `onBeforeProviderTurn` -> validates (and optionally auto-wires) `$skill`s for the active
 *                                multi-account provider env (`CLAUDE_CONFIG_DIR`, `CODEX_HOME`, etc.)
 */

import { Context, Data, Effect, Layer, Option, PubSub, Stream } from "effect";
import {
  isSkillWiredInTarget,
  isStrictAdapter,
  normalizeSkillSlug,
  resolveTargetForEnv,
  type CatalogScanParams,
  type CatalogSnapshot,
  type ProfileApplyParams,
  type ProfileApplyResult,
  type ProfileDeleteParams,
  type ProfileDeleteResult,
  type ProfileListResult,
  type ProfileSaveParams,
  type ProfileSaveResult,
  type QuarantineUndoResult,
  type SkillConsolidateParams,
  type SkillConsolidateResult,
  type SkillDetail,
  type SkillFixFrontmatterParams,
  type SkillFixFrontmatterResult,
  type SkillResolveDriftParams,
  type SkillResolveDriftResult,
  type SkillSaveParams,
  type SkillSaveResult,
  type SkillToggleParams,
  type SkillToggleResult,
  type LoadoutEvent,
  type LoadoutRpcMethod,
  type LoadoutRpcParams,
  type LoadoutRpcResult,
  type TargetAddParams,
  type TargetCloneParams,
  type TargetCloneResult,
  type TargetInstance,
  type TargetRemoveParams,
  type TargetRemoveResult,
  type WorktreeHydrateResult,
} from "../protocol.js";
import {
  LoadoutRpcClient,
  LoadoutRpcError,
  type LoadoutRpcClientOptions,
  type LoadoutRpcErrorCode,
} from "./LoadoutRpcClient.js";

// ---------------------------------------------------------------------------
// Error & Configuration Types
// ---------------------------------------------------------------------------

export type LoadoutErrorCode = LoadoutRpcErrorCode | "UNKNOWN";

export class LoadoutError extends Data.TaggedError("LoadoutError")<{
  readonly code: LoadoutErrorCode;
  readonly message: string;
  readonly method?: string;
  readonly rpcCode?: number;
  readonly data?: unknown;
  readonly cause?: unknown;
}> {}

function toLoadoutError(err: unknown, fallbackMethod?: string): LoadoutError {
  if (err instanceof LoadoutError) return err;
  if (err instanceof LoadoutRpcError) {
    return new LoadoutError({
      code: err.code,
      message: err.message,
      method: err.method ?? fallbackMethod,
      rpcCode: err.rpcCode,
      data: err.data,
      cause: err.cause ?? err,
    });
  }
  return new LoadoutError({
    code: "UNKNOWN",
    message: err instanceof Error ? err.message : String(err),
    method: fallbackMethod,
    cause: err,
  });
}

export interface LoadoutServiceConfigOptions extends LoadoutRpcClientOptions {
  /** Default auto-wire behavior for `onBeforeProviderTurn` when not specified per-call. */
  readonly defaultAutoWire?: boolean;
  /** Default frontmatter auto-fix behavior for strict targets in `onBeforeProviderTurn`. */
  readonly defaultAutoFixFrontmatter?: boolean;
}

export class LoadoutServiceConfig extends Context.Tag(
  "t3code/plugin/LoadoutServiceConfig",
)<LoadoutServiceConfig, LoadoutServiceConfigOptions>() {}

// ---------------------------------------------------------------------------
// T3 Code Lifecycle Hook Types
// ---------------------------------------------------------------------------

export interface WorktreeCreatedHookInput {
  readonly worktreePath: string;
  readonly repoRoot?: string;
  readonly providerId?: string;
  readonly env?: Readonly<Record<string, string | undefined>>;
}

export interface BeforeProviderTurnInput {
  readonly providerId: string;
  readonly env?: Readonly<Record<string, string | undefined>>;
  readonly selectedSkills: readonly string[];
  /** Automatically enable unwired skills for the resolved target before the turn starts. */
  readonly autoWire?: boolean;
  /** Automatically repair missing/invalid Antigravity frontmatter for strict targets. */
  readonly autoFixFrontmatter?: boolean;
}

export interface TurnSkillFrontmatterWarning {
  readonly slug: string;
  readonly issues: readonly string[];
  readonly fixed: boolean;
}

export interface BeforeProviderTurnReport {
  /** True when target resolved and every selected skill is wired & compatible. */
  readonly ok: boolean;
  readonly providerId: string;
  readonly resolvedTarget?: TargetInstance;
  readonly wiredSkills: readonly string[];
  readonly unwiredSkills: readonly string[];
  readonly missingSkills: readonly string[];
  readonly autoWiredSkills: readonly string[];
  readonly autoFixedSkills: readonly string[];
  readonly frontmatterWarnings: readonly TurnSkillFrontmatterWarning[];
  readonly warnings: readonly string[];
}

// ---------------------------------------------------------------------------
// Effect Service Contract
// ---------------------------------------------------------------------------

export interface LoadoutServiceShape {
  readonly getCatalog: () => Effect.Effect<CatalogSnapshot, LoadoutError>;
  readonly scanCatalog: (
    req?: CatalogScanParams,
  ) => Effect.Effect<CatalogSnapshot, LoadoutError>;
  readonly toggleSkill: (
    req: SkillToggleParams,
  ) => Effect.Effect<SkillToggleResult, LoadoutError>;
  readonly consolidateSkills: (
    req: SkillConsolidateParams,
  ) => Effect.Effect<SkillConsolidateResult, LoadoutError>;
  readonly resolveDuplicate: (
    req: SkillResolveDriftParams,
  ) => Effect.Effect<SkillResolveDriftResult, LoadoutError>;
  readonly fixFrontmatter: (
    req: SkillFixFrontmatterParams,
  ) => Effect.Effect<SkillFixFrontmatterResult, LoadoutError>;
  readonly getSkill: (slug: string) => Effect.Effect<SkillDetail, LoadoutError>;
  readonly saveSkill: (
    req: SkillSaveParams,
  ) => Effect.Effect<SkillSaveResult, LoadoutError>;
  readonly addCustomTarget: (
    req: TargetAddParams,
  ) => Effect.Effect<TargetInstance, LoadoutError>;
  readonly removeTarget: (
    req: TargetRemoveParams,
  ) => Effect.Effect<TargetRemoveResult, LoadoutError>;
  readonly cloneTarget: (
    req: TargetCloneParams,
  ) => Effect.Effect<TargetCloneResult, LoadoutError>;
  readonly undoLastQuarantine: () => Effect.Effect<
    QuarantineUndoResult,
    LoadoutError
  >;
  readonly listProfiles: () => Effect.Effect<ProfileListResult, LoadoutError>;
  readonly saveProfile: (
    req: ProfileSaveParams,
  ) => Effect.Effect<ProfileSaveResult, LoadoutError>;
  readonly applyProfile: (
    req: ProfileApplyParams,
  ) => Effect.Effect<ProfileApplyResult, LoadoutError>;
  readonly deleteProfile: (
    req: ProfileDeleteParams,
  ) => Effect.Effect<ProfileDeleteResult, LoadoutError>;
  readonly hydrateWorktree: (
    worktreePath: string,
    providerEnv?: Readonly<Record<string, string | undefined>>,
    options?: { readonly repoRoot?: string; readonly providerId?: string },
  ) => Effect.Effect<WorktreeHydrateResult, LoadoutError>;

  /** Raw typed JSON-RPC 2.0 dispatch (handy for T3 Code WebSocket forwarding). */
  readonly dispatchRpc: <M extends LoadoutRpcMethod>(
    method: M,
    params?: LoadoutRpcParams<M>,
  ) => Effect.Effect<LoadoutRpcResult<M>, LoadoutError>;

  /**
   * T3 Code `apps/server` lifecycle hook invoked immediately after a fresh
   * per-thread Git worktree is created.
   */
  readonly onWorktreeCreated: (
    input: WorktreeCreatedHookInput,
  ) => Effect.Effect<WorktreeHydrateResult, LoadoutError>;

  /**
   * T3 Code `apps/server` lifecycle hook invoked right before spawning a
   * provider CLI turn (`claude`, `codex`, `opencode`, etc.).
   */
  readonly onBeforeProviderTurn: (
    input: BeforeProviderTurnInput,
  ) => Effect.Effect<BeforeProviderTurnReport, LoadoutError>;

  /** Live stream of catalog updates, toggles, worktree hydration, and RPC lifecycle events. */
  readonly events: Stream.Stream<LoadoutEvent>;
}

export class LoadoutService extends Context.Tag("t3code/plugin/LoadoutService")<
  LoadoutService,
  LoadoutServiceShape
>() {}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

function sanitizeEnvMap(
  env?: Readonly<Record<string, string | undefined>>,
): Record<string, string> {
  const out: Record<string, string> = {};
  if (!env) return out;
  for (const [k, v] of Object.entries(env)) {
    if (typeof v === "string") {
      out[k] = v;
    }
  }
  return out;
}

function nowIso(): string {
  return new Date().toISOString();
}

// ---------------------------------------------------------------------------
// Scoped Service Construction & Live Layer
// ---------------------------------------------------------------------------

export const makeLoadoutService = (
  options: LoadoutServiceConfigOptions = {},
) =>
  Effect.gen(function* () {
    const pubsub = yield* Effect.acquireRelease(
      PubSub.unbounded<LoadoutEvent>(),
      (ps) => PubSub.shutdown(ps),
    );

    const client = yield* Effect.acquireRelease(
      Effect.sync(() => {
        const rpc = new LoadoutRpcClient(options);

        pubsub.unsafeOffer({
          type: "rpc.connected",
          pid: rpc.pid,
          timestamp: nowIso(),
        });

        rpc.onNotification((notification) => {
          if (
            notification.method === "catalog.updated" &&
            notification.params &&
            "catalog" in notification.params
          ) {
            pubsub.unsafeOffer({
              type: "catalog.updated",
              catalog: notification.params.catalog as CatalogSnapshot,
              reason:
                typeof notification.params.reason === "string"
                  ? notification.params.reason
                  : "notification",
              timestamp: nowIso(),
            });
          }
        });

        rpc.onExit(({ code, signal }) => {
          pubsub.unsafeOffer({
            type: "rpc.disconnected",
            exitCode: code,
            signal,
            timestamp: nowIso(),
          });
        });

        rpc.onError((err) => {
          pubsub.unsafeOffer({
            type: "rpc.error",
            message: err.message,
            timestamp: nowIso(),
          });
        });

        return rpc;
      }),
      (rpc) => Effect.promise(() => rpc.close()),
    );

    const invoke = <M extends LoadoutRpcMethod>(
      method: M,
      params?: LoadoutRpcParams<M>,
    ): Effect.Effect<LoadoutRpcResult<M>, LoadoutError> =>
      Effect.tryPromise({
        try: () => client.call(method, params),
        catch: (err) => toLoadoutError(err, method),
      });

    const publishCatalogIfPresent = (
      catalog: CatalogSnapshot | undefined,
      reason: string,
    ): Effect.Effect<void> =>
      catalog
        ? PubSub.publish(pubsub, {
            type: "catalog.updated",
            catalog,
            reason,
            timestamp: nowIso(),
          }).pipe(Effect.asVoid)
        : Effect.void;

    const getCatalog = (): Effect.Effect<CatalogSnapshot, LoadoutError> =>
      invoke("catalog.get", {});

    const scanCatalog = (
      req: CatalogScanParams = {},
    ): Effect.Effect<CatalogSnapshot, LoadoutError> =>
      Effect.gen(function* () {
        const snapshot = yield* invoke("catalog.scan", req);
        yield* publishCatalogIfPresent(snapshot, "catalog.scan");
        return snapshot;
      });

    const toggleSkill = (
      req: SkillToggleParams,
    ): Effect.Effect<SkillToggleResult, LoadoutError> =>
      Effect.gen(function* () {
        const result = yield* invoke("skill.toggle", req);
        yield* PubSub.publish(pubsub, {
          type: "skill.toggled",
          skill: req.skill,
          targetId: req.targetId,
          enabled: req.enabled,
          timestamp: nowIso(),
        });
        yield* publishCatalogIfPresent(result.catalog, "skill.toggle");
        return result;
      });

    const consolidateSkills = (
      req: SkillConsolidateParams,
    ): Effect.Effect<SkillConsolidateResult, LoadoutError> =>
      Effect.gen(function* () {
        const result = yield* invoke("skill.consolidate", req);
        if (!req.dryRun) {
          yield* publishCatalogIfPresent(result.catalog, "skill.consolidate");
        }
        return result;
      });

    const resolveDuplicate = (
      req: SkillResolveDriftParams,
    ): Effect.Effect<SkillResolveDriftResult, LoadoutError> =>
      Effect.gen(function* () {
        const result = yield* invoke("skill.resolveDrift", req);
        yield* publishCatalogIfPresent(result.catalog, "skill.resolveDrift");
        return result;
      });

    const fixFrontmatter = (
      req: SkillFixFrontmatterParams,
    ): Effect.Effect<SkillFixFrontmatterResult, LoadoutError> =>
      Effect.gen(function* () {
        const result = yield* invoke("skill.fixFrontmatter", req);
        if (req.apply) {
          yield* publishCatalogIfPresent(result.catalog, "skill.fixFrontmatter");
        }
        return result;
      });

    const getSkill = (slug: string): Effect.Effect<SkillDetail, LoadoutError> =>
      invoke("skill.get", { slug });

    const saveSkill = (
      req: SkillSaveParams,
    ): Effect.Effect<SkillSaveResult, LoadoutError> =>
      Effect.gen(function* () {
        const result = yield* invoke("skill.save", req);
        yield* publishCatalogIfPresent(result.catalog, "skill.save");
        return result;
      });

    const addCustomTarget = (
      req: TargetAddParams,
    ): Effect.Effect<TargetInstance, LoadoutError> =>
      Effect.gen(function* () {
        const result = yield* invoke("target.add", req);
        yield* publishCatalogIfPresent(result.catalog, "target.add");
        return result.target;
      });

    const removeTarget = (
      req: TargetRemoveParams,
    ): Effect.Effect<TargetRemoveResult, LoadoutError> =>
      Effect.gen(function* () {
        const result = yield* invoke("target.remove", req);
        yield* publishCatalogIfPresent(result.catalog, "target.remove");
        return result;
      });

    const cloneTarget = (
      req: TargetCloneParams,
    ): Effect.Effect<TargetCloneResult, LoadoutError> =>
      Effect.gen(function* () {
        const result = yield* invoke("target.clone", req);
        yield* publishCatalogIfPresent(result.catalog, "target.clone");
        return result;
      });

    const undoLastQuarantine = (): Effect.Effect<
      QuarantineUndoResult,
      LoadoutError
    > =>
      Effect.gen(function* () {
        const result = yield* invoke("quarantine.undo", {});
        yield* publishCatalogIfPresent(result.catalog, "quarantine.undo");
        return result;
      });

    const listProfiles = (): Effect.Effect<ProfileListResult, LoadoutError> =>
      invoke("profile.list", {});

    const saveProfile = (
      req: ProfileSaveParams,
    ): Effect.Effect<ProfileSaveResult, LoadoutError> =>
      Effect.gen(function* () {
        const result = yield* invoke("profile.save", req);
        yield* publishCatalogIfPresent(result.catalog, "profile.save");
        return result;
      });

    const applyProfile = (
      req: ProfileApplyParams,
    ): Effect.Effect<ProfileApplyResult, LoadoutError> =>
      Effect.gen(function* () {
        const result = yield* invoke("profile.apply", req);
        yield* publishCatalogIfPresent(result.catalog, "profile.apply");
        return result;
      });

    const deleteProfile = (
      req: ProfileDeleteParams,
    ): Effect.Effect<ProfileDeleteResult, LoadoutError> =>
      Effect.gen(function* () {
        const result = yield* invoke("profile.delete", req);
        yield* publishCatalogIfPresent(result.catalog, "profile.delete");
        return result;
      });

    const hydrateWorktree = (
      worktreePath: string,
      providerEnv?: Readonly<Record<string, string | undefined>>,
      hookOpts?: { readonly repoRoot?: string; readonly providerId?: string },
    ): Effect.Effect<WorktreeHydrateResult, LoadoutError> =>
      Effect.gen(function* () {
        const result = yield* invoke("worktree.hydrate", {
          worktreePath,
          repoRoot: hookOpts?.repoRoot,
          providerId: hookOpts?.providerId,
          env: sanitizeEnvMap(providerEnv),
        });
        yield* PubSub.publish(pubsub, {
          type: "worktree.hydrated",
          worktreePath: result.worktreePath,
          resolvedTargetId: result.resolvedTargetId,
          linkedSkills: result.linkedSkills,
          timestamp: nowIso(),
        });
        return result;
      });

    const onWorktreeCreated = (
      input: WorktreeCreatedHookInput,
    ): Effect.Effect<WorktreeHydrateResult, LoadoutError> =>
      hydrateWorktree(input.worktreePath, input.env, {
        repoRoot: input.repoRoot,
        providerId: input.providerId,
      });

    const onBeforeProviderTurn = (
      input: BeforeProviderTurnInput,
    ): Effect.Effect<BeforeProviderTurnReport, LoadoutError> =>
      Effect.gen(function* () {
        const autoWire = input.autoWire ?? options.defaultAutoWire ?? false;
        const autoFixFrontmatter =
          input.autoFixFrontmatter ??
          options.defaultAutoFixFrontmatter ??
          false;

        const requestedSlugs = Array.from(
          new Set(
            input.selectedSkills
              .map(normalizeSkillSlug)
              .filter((slug) => slug.length > 0),
          ),
        );

        const catalog = yield* getCatalog();
        const envWithHome: Record<string, string | undefined> = {
          ...(options.loadoutHome ? { LOADOUT_HOME: options.loadoutHome } : {}),
          ...input.env,
        };
        const resolvedTarget = resolveTargetForEnv(
          catalog.targets,
          input.providerId,
          envWithHome,
          { homeDir: options.loadoutHome ?? input.env?.HOME ?? process.env.HOME },
        );

        const warnings: string[] = [];
        const wiredSkills: string[] = [];
        const unwiredSkills: string[] = [];
        const missingSkills: string[] = [];
        const autoWiredSkills: string[] = [];
        const autoFixedSkills: string[] = [];
        const frontmatterWarnings: TurnSkillFrontmatterWarning[] = [];

        if (!resolvedTarget) {
          warnings.push(
            `No registered loadout target matched provider "${input.providerId}" for the current environment.`,
          );
          return {
            ok: requestedSlugs.length === 0,
            providerId: input.providerId,
            resolvedTarget: undefined,
            wiredSkills,
            unwiredSkills: requestedSlugs,
            missingSkills,
            autoWiredSkills,
            autoFixedSkills,
            frontmatterWarnings,
            warnings,
          };
        }

        const skillsBySlug = new Map(
          catalog.skills.map((skill) => [skill.slug, skill] as const),
        );
        const strict = isStrictAdapter(resolvedTarget.adapter);

        for (const slug of requestedSlugs) {
          const skill = skillsBySlug.get(slug);
          if (!skill) {
            missingSkills.push(slug);
            warnings.push(
              `Selected skill "$${slug}" was not found in the loadout catalog.`,
            );
            continue;
          }

          let isWired = isSkillWiredInTarget(skill, resolvedTarget.id);
          if (!isWired) {
            if (autoWire) {
              yield* toggleSkill({
                skill: slug,
                targetId: resolvedTarget.id,
                enabled: true,
              });
              isWired = true;
              autoWiredSkills.push(slug);
            } else {
              unwiredSkills.push(slug);
              warnings.push(
                `Skill "$${slug}" is not wired for target "${resolvedTarget.displayName}" (${resolvedTarget.id}).`,
              );
            }
          }

          if (isWired) {
            wiredSkills.push(slug);
          }

          if (strict && !skill.antigravityValid) {
            if (autoFixFrontmatter) {
              const fixRes = yield* fixFrontmatter({
                skill: slug,
                apply: true,
              });
              const fixed = Boolean(fixRes.applied && fixRes.antigravityValid !== false);
              if (fixed) {
                autoFixedSkills.push(slug);
              }
              frontmatterWarnings.push({
                slug,
                issues: skill.frontmatterIssues,
                fixed,
              });
              if (!fixed) {
                warnings.push(
                  `Skill "$${slug}" failed frontmatter validation for strict target "${resolvedTarget.displayName}".`,
                );
              }
            } else {
              frontmatterWarnings.push({
                slug,
                issues: skill.frontmatterIssues,
                fixed: false,
              });
              warnings.push(
                `Skill "$${slug}" has invalid YAML frontmatter and will be ignored by "${resolvedTarget.displayName}" (${skill.frontmatterIssues.join("; ") || "missing name/description"}).`,
              );
            }
          }
        }

        const unresolvedFrontmatter = frontmatterWarnings.some((w) => !w.fixed);
        const ok =
          missingSkills.length === 0 &&
          unwiredSkills.length === 0 &&
          !unresolvedFrontmatter;

        return {
          ok,
          providerId: input.providerId,
          resolvedTarget,
          wiredSkills,
          unwiredSkills,
          missingSkills,
          autoWiredSkills,
          autoFixedSkills,
          frontmatterWarnings,
          warnings,
        };
      });

    const dispatchRpc = <M extends LoadoutRpcMethod>(
      method: M,
      params?: LoadoutRpcParams<M>,
    ): Effect.Effect<LoadoutRpcResult<M>, LoadoutError> => {
      switch (method) {
        case "catalog.get":
          return getCatalog() as Effect.Effect<LoadoutRpcResult<M>, LoadoutError>;
        case "catalog.scan":
          return scanCatalog(params as CatalogScanParams) as Effect.Effect<
            LoadoutRpcResult<M>,
            LoadoutError
          >;
        case "skill.toggle":
          return toggleSkill(params as SkillToggleParams) as Effect.Effect<
            LoadoutRpcResult<M>,
            LoadoutError
          >;
        case "skill.consolidate":
          return consolidateSkills(
            params as SkillConsolidateParams,
          ) as Effect.Effect<LoadoutRpcResult<M>, LoadoutError>;
        case "skill.resolveDrift":
          return resolveDuplicate(
            params as SkillResolveDriftParams,
          ) as Effect.Effect<LoadoutRpcResult<M>, LoadoutError>;
        case "skill.fixFrontmatter":
          return fixFrontmatter(
            params as SkillFixFrontmatterParams,
          ) as Effect.Effect<LoadoutRpcResult<M>, LoadoutError>;
        case "skill.get":
          return getSkill(
            (params as { readonly slug: string }).slug,
          ) as Effect.Effect<LoadoutRpcResult<M>, LoadoutError>;
        case "skill.save":
          return saveSkill(params as SkillSaveParams) as Effect.Effect<
            LoadoutRpcResult<M>,
            LoadoutError
          >;
        case "target.add":
          return invoke("target.add", params as TargetAddParams).pipe(
            Effect.tap((res) =>
              publishCatalogIfPresent(res.catalog, "target.add"),
            ),
          ) as Effect.Effect<LoadoutRpcResult<M>, LoadoutError>;
        case "target.remove":
          return removeTarget(params as TargetRemoveParams) as Effect.Effect<
            LoadoutRpcResult<M>,
            LoadoutError
          >;
        case "target.clone":
          return cloneTarget(params as TargetCloneParams) as Effect.Effect<
            LoadoutRpcResult<M>,
            LoadoutError
          >;
        case "quarantine.undo":
          return undoLastQuarantine() as Effect.Effect<
            LoadoutRpcResult<M>,
            LoadoutError
          >;
        case "worktree.hydrate":
          return invoke(
            "worktree.hydrate",
            params as LoadoutRpcParams<"worktree.hydrate">,
          ).pipe(
            Effect.tap((res) =>
              PubSub.publish(pubsub, {
                type: "worktree.hydrated",
                worktreePath: res.worktreePath,
                resolvedTargetId: res.resolvedTargetId,
                linkedSkills: res.linkedSkills,
                timestamp: nowIso(),
              }),
            ),
          ) as Effect.Effect<LoadoutRpcResult<M>, LoadoutError>;
        case "profile.list":
          return listProfiles() as Effect.Effect<LoadoutRpcResult<M>, LoadoutError>;
        case "profile.save":
          return saveProfile(params as ProfileSaveParams) as Effect.Effect<
            LoadoutRpcResult<M>,
            LoadoutError
          >;
        case "profile.apply":
          return applyProfile(params as ProfileApplyParams) as Effect.Effect<
            LoadoutRpcResult<M>,
            LoadoutError
          >;
        case "profile.delete":
          return deleteProfile(params as ProfileDeleteParams) as Effect.Effect<
            LoadoutRpcResult<M>,
            LoadoutError
          >;
      }
    };

    const events: Stream.Stream<LoadoutEvent> = Stream.fromPubSub(pubsub);

    const service: LoadoutServiceShape = {
      getCatalog,
      scanCatalog,
      toggleSkill,
      consolidateSkills,
      resolveDuplicate,
      fixFrontmatter,
      getSkill,
      saveSkill,
      addCustomTarget,
      removeTarget,
      cloneTarget,
      undoLastQuarantine,
      listProfiles,
      saveProfile,
      applyProfile,
      deleteProfile,
      hydrateWorktree,
      dispatchRpc,
      onWorktreeCreated,
      onBeforeProviderTurn,
      events,
    };

    return service;
  });

/**
 * Default Effect `Layer.scoped` for `LoadoutService`.
 * Reads optional `LoadoutServiceConfig` from context if provided, otherwise uses
 * `LOADOUT_BIN` / `LOADOUT_HOME` environment variables.
 */
export const LoadoutServiceLive: Layer.Layer<LoadoutService> = Layer.scoped(
  LoadoutService,
  Effect.gen(function* () {
    const maybeConfig = yield* Effect.serviceOption(LoadoutServiceConfig);
    const config = Option.isSome(maybeConfig) ? maybeConfig.value : {};
    return yield* makeLoadoutService(config);
  }),
);

/**
 * Convenience Layer factory when supplying explicit options directly.
 */
export const makeLoadoutServiceLayer = (
  config: LoadoutServiceConfigOptions,
): Layer.Layer<LoadoutService> =>
  Layer.scoped(LoadoutService, makeLoadoutService(config));

// ---------------------------------------------------------------------------
// T3 Code WebSocket Server Bridge Helper
// ---------------------------------------------------------------------------

export interface T3CodeWsIncomingRpc {
  readonly method: LoadoutRpcMethod;
  readonly params?: Record<string, unknown>;
}

/**
 * Creates a lightweight handler for `apps/server` WebSocket routes that
 * forwards `loadout` RPC calls from desktop/browser/mobile T3 Code clients to
 * the host `LoadoutService`.
 */
export const handleLoadoutWebSocketRpc = (
  request: T3CodeWsIncomingRpc,
): Effect.Effect<unknown, LoadoutError, LoadoutService> =>
  Effect.flatMap(LoadoutService, (loadout) =>
    loadout.dispatchRpc(
      request.method,
      request.params as LoadoutRpcParams<typeof request.method>,
    ),
  );
