/**
 * @loadout/t3code-plugin
 *
 * For bundle isolation in `pingdotgg/t3code`, prefer subpath imports:
 * - `@loadout/t3code-plugin/protocol`     (shared browser + server types & `resolveTargetForEnv`)
 * - `@loadout/t3code-plugin/server`       (`apps/server` Effect-TS `LoadoutService` & hooks)
 * - `@loadout/t3code-plugin/server/rpc`   (standalone stdio JSON-RPC 2.0 `LoadoutRpcClient`)
 * - `@loadout/t3code-plugin/web`          (`apps/web` React 19 `<LoadoutSwitchboardDrawer />`)
 * - `@loadout/t3code-plugin/web/composer` (`apps/web` React 19 `<ComposerSkillPickerBadge />`)
 */

export * from "./protocol.js";
export * from "./server/LoadoutRpcClient.js";
export * from "./server/LoadoutService.js";
export * from "./web/LoadoutSwitchboardDrawer.js";
export * from "./web/ComposerSkillPickerBadge.js";
