/**
 * @singlet/t3code-plugin
 *
 * For bundle isolation in `pingdotgg/t3code`, prefer subpath imports:
 * - `@singlet/t3code-plugin/protocol`     (shared browser + server types & `resolveTargetForEnv`)
 * - `@singlet/t3code-plugin/server`       (`apps/server` Effect-TS `SingletService` & hooks)
 * - `@singlet/t3code-plugin/server/rpc`   (standalone stdio JSON-RPC 2.0 `SingletRpcClient`)
 * - `@singlet/t3code-plugin/web`          (`apps/web` React 19 `<SingletSwitchboardDrawer />`)
 * - `@singlet/t3code-plugin/web/composer` (`apps/web` React 19 `<ComposerSkillPickerBadge />`)
 */

export * from "./protocol.js";
export * from "./server/SingletRpcClient.js";
export * from "./server/SingletService.js";
export * from "./web/SingletSwitchboardDrawer.js";
export * from "./web/ComposerSkillPickerBadge.js";
