/**
 * @loadout/t3code-plugin: Stdio JSON-RPC 2.0 Client (`loadout rpc`)
 *
 * Spawns the native `loadout rpc` daemon over `stdin`/`stdout`, multiplexes
 * concurrent JSON-RPC 2.0 requests by numeric `id`, dispatches server
 * notifications, and terminates cleanly on shutdown.
 *
 * Zero external runtime dependencies (Node/Bun built-ins only).
 */

import { spawn, type ChildProcessWithoutNullStreams } from "node:child_process";
import * as readline from "node:readline";
import type {
  JsonRpcNotification,
  JsonRpcRequest,
  JsonRpcResponse,
  LoadoutRpcMethod,
  LoadoutRpcParams,
  LoadoutRpcResult,
} from "../protocol.js";
import { isJsonRpcErrorResponse } from "../protocol.js";

export type LoadoutRpcErrorCode =
  | "SPAWN_FAILED"
  | "RPC_ERROR"
  | "TIMEOUT"
  | "CLOSED"
  | "PROTOCOL_ERROR"
  | "WRITE_FAILED";

export class LoadoutRpcError extends Error {
  readonly code: LoadoutRpcErrorCode;
  readonly rpcCode?: number;
  readonly method?: string;
  readonly data?: unknown;
  override readonly cause?: unknown;

  constructor(
    code: LoadoutRpcErrorCode,
    message: string,
    options?: {
      rpcCode?: number;
      method?: string;
      data?: unknown;
      cause?: unknown;
    },
  ) {
    super(message);
    this.name = "LoadoutRpcError";
    this.code = code;
    this.rpcCode = options?.rpcCode;
    this.method = options?.method;
    this.data = options?.data;
    this.cause = options?.cause;
  }
}

export interface LoadoutRpcClientOptions {
  /** Path to the `loadout` native binary (defaults to `process.env.LOADOUT_BIN ?? "loadout"`). */
  readonly binaryPath?: string;
  /** Optional `LOADOUT_HOME` override passed via both `--home` flag and `LOADOUT_HOME` env var. */
  readonly loadoutHome?: string;
  /** Additional CLI arguments placed before `rpc`. */
  readonly extraArgs?: readonly string[];
  /** Additional environment variables merged with `process.env`. */
  readonly env?: Readonly<Record<string, string | undefined>>;
  /** Working directory for the spawned `loadout rpc` process. */
  readonly cwd?: string;
  /** Default per-request timeout in milliseconds (default: 15000). */
  readonly requestTimeoutMs?: number;
  /** Graceful shutdown wait before sending SIGTERM/SIGKILL (default: 1500). */
  readonly shutdownTimeoutMs?: number;
  /** Optional hook receiving each stderr line emitted by `loadout rpc`. */
  readonly onStderr?: (line: string) => void;
}

interface PendingRequest {
  readonly method: string;
  readonly resolve: (value: unknown) => void;
  readonly reject: (err: LoadoutRpcError) => void;
  readonly timer: ReturnType<typeof setTimeout>;
}

function unrefTimer(timer: ReturnType<typeof setTimeout>): void {
  if (
    typeof timer === "object" &&
    timer !== null &&
    "unref" in timer &&
    typeof (timer as { unref?: unknown }).unref === "function"
  ) {
    (timer as { unref: () => void }).unref();
  }
}

export type NotificationListener = (notification: JsonRpcNotification) => void;
export type ExitListener = (info: {
  readonly code: number | null;
  readonly signal: NodeJS.Signals | null;
}) => void;
export type ErrorListener = (error: LoadoutRpcError) => void;

export class LoadoutRpcClient {
  readonly binaryPath: string;
  readonly loadoutHome?: string;

  private readonly requestTimeoutMs: number;
  private readonly shutdownTimeoutMs: number;
  private readonly child: ChildProcessWithoutNullStreams;
  private readonly stdoutRl: readline.Interface;
  private readonly stderrRl: readline.Interface;

  private nextId = 1;
  private closed = false;
  private exited = false;
  private readonly pending = new Map<number, PendingRequest>();
  private readonly notificationListeners = new Set<NotificationListener>();
  private readonly exitListeners = new Set<ExitListener>();
  private readonly errorListeners = new Set<ErrorListener>();
  private closePromise: Promise<void> | undefined;

  constructor(options: LoadoutRpcClientOptions = {}) {
    this.binaryPath = options.binaryPath ?? process.env.LOADOUT_BIN ?? "loadout";
    this.loadoutHome = options.loadoutHome ?? process.env.LOADOUT_HOME;
    this.requestTimeoutMs = options.requestTimeoutMs ?? 15_000;
    this.shutdownTimeoutMs = options.shutdownTimeoutMs ?? 1_500;

    const args: string[] = [];
    if (this.loadoutHome) {
      args.push("--home", this.loadoutHome);
    }
    if (options.extraArgs && options.extraArgs.length > 0) {
      args.push(...options.extraArgs);
    }
    args.push("rpc");

    const env: NodeJS.ProcessEnv = {
      ...process.env,
      ...options.env,
    };
    if (this.loadoutHome) {
      env.LOADOUT_HOME = this.loadoutHome;
    }

    this.child = spawn(this.binaryPath, args, {
      stdio: ["pipe", "pipe", "pipe"],
      cwd: options.cwd,
      env,
    });

    this.stdoutRl = readline.createInterface({
      input: this.child.stdout,
      crlfDelay: Infinity,
    });
    this.stdoutRl.on("line", (line) => this.handleStdoutLine(line));

    this.stderrRl = readline.createInterface({
      input: this.child.stderr,
      crlfDelay: Infinity,
    });
    this.stderrRl.on("line", (line) => {
      if (options.onStderr) {
        options.onStderr(line);
      }
    });

    this.child.on("error", (err) => {
      const rpcErr = new LoadoutRpcError(
        "SPAWN_FAILED",
        `Failed to spawn "${this.binaryPath} ${args.join(" ")}": ${err.message}`,
        { cause: err },
      );
      this.exited = true;
      this.rejectAllPending(rpcErr);
      for (const listener of this.errorListeners) {
        listener(rpcErr);
      }
    });

    this.child.on("exit", (code, signal) => {
      this.exited = true;
      const err = new LoadoutRpcError(
        "CLOSED",
        `loadout rpc process exited (code=${code ?? "null"}, signal=${signal ?? "null"})`,
      );
      this.rejectAllPending(err);
      for (const listener of this.exitListeners) {
        listener({ code, signal });
      }
    });
  }

  get pid(): number | undefined {
    return this.child.pid;
  }

  get isClosed(): boolean {
    return this.closed || this.exited;
  }

  /**
   * Subscribe to JSON-RPC 2.0 server notifications (messages without an `id`).
   */
  onNotification(listener: NotificationListener): () => void {
    this.notificationListeners.add(listener);
    return () => {
      this.notificationListeners.delete(listener);
    };
  }

  /**
   * Subscribe to child process exit events.
   */
  onExit(listener: ExitListener): () => void {
    this.exitListeners.add(listener);
    return () => {
      this.exitListeners.delete(listener);
    };
  }

  /**
   * Subscribe to transport-level errors (spawn failure, malformed frames).
   */
  onError(listener: ErrorListener): () => void {
    this.errorListeners.add(listener);
    return () => {
      this.errorListeners.delete(listener);
    };
  }

  /**
   * Sends a strongly typed JSON-RPC 2.0 request to `loadout rpc` and awaits its
   * correlated response.
   */
  call<M extends LoadoutRpcMethod>(
    method: M,
    params?: LoadoutRpcParams<M>,
    timeoutMs?: number,
  ): Promise<LoadoutRpcResult<M>> {
    return this.callRaw(method, params ?? {}, timeoutMs) as Promise<
      LoadoutRpcResult<M>
    >;
  }

  /**
   * Sends an arbitrary JSON-RPC 2.0 request over stdin/stdout.
   */
  callRaw(
    method: string,
    params: unknown = {},
    timeoutMs = this.requestTimeoutMs,
  ): Promise<unknown> {
    if (this.closed || this.exited) {
      return Promise.reject(
        new LoadoutRpcError(
          "CLOSED",
          `Cannot invoke "${method}": loadout rpc client is closed`,
          { method },
        ),
      );
    }

    const id = this.nextId++;
    const payload: JsonRpcRequest = {
      jsonrpc: "2.0",
      id,
      method,
      params: (params ?? {}) as Record<string, unknown>,
    };

    return new Promise<unknown>((resolve, reject) => {
      const timer = setTimeout(() => {
        this.pending.delete(id);
        reject(
          new LoadoutRpcError(
            "TIMEOUT",
            `loadout rpc request "${method}" (id=${id}) timed out after ${timeoutMs}ms`,
            { method },
          ),
        );
      }, timeoutMs);
      unrefTimer(timer);

      this.pending.set(id, { method, resolve, reject, timer });

      const frame = `${JSON.stringify(payload)}\n`;
      this.child.stdin.write(frame, "utf8", (writeErr) => {
        if (writeErr) {
          const entry = this.pending.get(id);
          if (entry) {
            clearTimeout(entry.timer);
            this.pending.delete(id);
            entry.reject(
              new LoadoutRpcError(
                "WRITE_FAILED",
                `Failed writing "${method}" to loadout rpc stdin: ${writeErr.message}`,
                { method, cause: writeErr },
              ),
            );
          }
        }
      });
    });
  }

  /**
   * Gracefully shuts down `loadout rpc`: closes stdin, waits briefly for clean exit,
   * and escalates to SIGTERM / SIGKILL if the child does not exit promptly.
   */
  close(): Promise<void> {
    if (this.closePromise) {
      return this.closePromise;
    }

    this.closed = true;
    this.rejectAllPending(
      new LoadoutRpcError("CLOSED", "loadout rpc client shutting down"),
    );

    this.closePromise = new Promise<void>((resolve) => {
      const cleanup = () => {
        this.stdoutRl.close();
        this.stderrRl.close();
        this.notificationListeners.clear();
        this.exitListeners.clear();
        this.errorListeners.clear();
        resolve();
      };

      if (this.exited || this.child.exitCode !== null || this.child.killed) {
        cleanup();
        return;
      }

      let finished = false;
      const done = () => {
        if (finished) return;
        finished = true;
        clearTimeout(termTimer);
        clearTimeout(killTimer);
        cleanup();
      };

      this.child.once("exit", done);
      this.child.once("error", done);

      try {
        this.child.stdin.end();
      } catch {
        // Ignore broken pipe on already-closing stdin
      }

      const termTimer = setTimeout(() => {
        if (!this.exited && !this.child.killed) {
          this.child.kill("SIGTERM");
        }
      }, Math.min(400, this.shutdownTimeoutMs));
      unrefTimer(termTimer);

      const killTimer = setTimeout(() => {
        if (!this.exited) {
          try {
            this.child.kill("SIGKILL");
          } catch {
            // Process may have exited between check and signal
          }
        }
        done();
      }, this.shutdownTimeoutMs);
      unrefTimer(killTimer);
    });

    return this.closePromise;
  }

  private handleStdoutLine(rawLine: string): void {
    const line = rawLine.trim();
    if (!line) return;

    let parsed: unknown;
    try {
      parsed = JSON.parse(line);
    } catch (err) {
      const protocolErr = new LoadoutRpcError(
        "PROTOCOL_ERROR",
        `Invalid JSON frame from loadout rpc: ${line.slice(0, 160)}`,
        { cause: err },
      );
      for (const listener of this.errorListeners) {
        listener(protocolErr);
      }
      return;
    }

    if (typeof parsed !== "object" || parsed === null) {
      return;
    }

    const msg = parsed as Record<string, unknown>;

    // Server-initiated notification (no id, has method)
    if (msg.id === undefined && typeof msg.method === "string") {
      const notification: JsonRpcNotification = {
        jsonrpc: "2.0",
        method: msg.method,
        params:
          typeof msg.params === "object" && msg.params !== null
            ? (msg.params as Record<string, unknown>)
            : undefined,
      };
      for (const listener of this.notificationListeners) {
        listener(notification);
      }
      return;
    }

    // Response correlated by numeric id
    const numericId =
      typeof msg.id === "number"
        ? msg.id
        : typeof msg.id === "string"
          ? Number(msg.id)
          : NaN;

    const entry = Number.isFinite(numericId)
      ? this.pending.get(numericId)
      : undefined;
    if (!entry) {
      return;
    }

    clearTimeout(entry.timer);
    this.pending.delete(numericId);

    const response = parsed as JsonRpcResponse<unknown>;
    if (isJsonRpcErrorResponse(response)) {
      entry.reject(
        new LoadoutRpcError("RPC_ERROR", response.error.message, {
          rpcCode: response.error.code,
          method: entry.method,
          data: response.error.data,
        }),
      );
      return;
    }

    entry.resolve(response.result);
  }

  private rejectAllPending(err: LoadoutRpcError): void {
    for (const [, entry] of this.pending) {
      clearTimeout(entry.timer);
      entry.reject(err);
    }
    this.pending.clear();
  }
}
