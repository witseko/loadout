/** Isolated temporary home folders and helpers to run bin/singlet inside them. */
import { spawnSync } from 'node:child_process';
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import { SINGLET_BIN } from './paths.js';

const COMMAND_TIMEOUT_MS = 15_000;

export interface CommandResult {
  status: number | null;
  stdout: string;
  stderr: string;
}

/**
 * A fresh temporary home folder.
 *
 * Every singlet call gets both `--home <root>` and `SINGLET_HOME=<root>`, so the
 * suite never reads or writes the real ~/.agents, ~/.claude, or other agent folders.
 */
export class Sandbox {
  readonly root: string;

  constructor() {
    this.root = fs.mkdtempSync(path.join(os.tmpdir(), 'singlet-e2e-'));
    if (this.root === os.homedir() || this.root === path.parse(this.root).root) {
      throw new Error(`Refusing to use unsafe sandbox root: ${this.root}`);
    }
  }

  /** Absolute path of a path relative to the sandbox root. */
  path(relativePath: string): string {
    return path.join(this.root, relativePath);
  }

  /** Writes a file, creating parent folders. Returns its absolute path. */
  write(relativePath: string, content: string): string {
    const fullPath = this.path(relativePath);
    fs.mkdirSync(path.dirname(fullPath), { recursive: true });
    fs.writeFileSync(fullPath, content);
    return fullPath;
  }

  /** Writes a SKILL.md with frontmatter for `slug` under `skillsDir`. */
  writeSkill(skillsDir: string, slug: string, description: string, body = `# ${slug}\n`): string {
    return this.write(
      path.join(skillsDir, slug, 'SKILL.md'),
      `---\nname: ${slug}\ndescription: ${description}\n---\n\n${body}`,
    );
  }

  mkdir(relativePath: string): void {
    fs.mkdirSync(this.path(relativePath), { recursive: true });
  }

  copyDir(fromRelative: string, toRelative: string): void {
    fs.cpSync(this.path(fromRelative), this.path(toRelative), { recursive: true });
  }

  symlink(targetAbsolute: string, linkRelative: string): void {
    fs.symlinkSync(targetAbsolute, this.path(linkRelative));
  }

  /** Environment for child processes, pinned to this sandbox. */
  env(extra: Record<string, string> = {}): NodeJS.ProcessEnv {
    return { ...process.env, SINGLET_HOME: this.root, ...extra };
  }

  /** Runs `bin/singlet --home <root> ...args` and returns its output. */
  run(args: string[], input?: string): CommandResult {
    const result = spawnSync(SINGLET_BIN, ['--home', this.root, ...args], {
      env: this.env(),
      input,
      encoding: 'utf8',
      timeout: COMMAND_TIMEOUT_MS,
    });
    if (result.error) {
      throw result.error;
    }
    return { status: result.status, stdout: result.stdout, stderr: result.stderr };
  }

  /** Runs a singlet command and throws with its stderr when it exits non-zero. */
  runOk(args: string[], input?: string): string {
    const result = this.run(args, input);
    if (result.status !== 0) {
      throw new Error(`singlet ${args.join(' ')} exited ${result.status}: ${result.stderr}`);
    }
    return result.stdout;
  }

  /** Runs `singlet scan --json` with optional extra args and parses the catalog. */
  scanJson(extraArgs: string[] = []): Catalog {
    return JSON.parse(this.runOk(['scan', ...extraArgs, '--json'])) as Catalog;
  }

  /** Sends newline-delimited JSON-RPC requests to `singlet rpc` and returns each parsed response line. */
  rpc(requests: object[]): RpcResponse[] {
    const stdout = this.runOk(['rpc'], requests.map((request) => JSON.stringify(request)).join('\n') + '\n');
    return parseJsonLines(stdout) as RpcResponse[];
  }

  dispose(): void {
    fs.rmSync(this.root, { recursive: true, force: true });
  }
}

export function parseJsonLines(text: string): unknown[] {
  return text
    .split('\n')
    .map((line) => line.trim())
    .filter((line) => line.length > 0)
    .map((line) => JSON.parse(line) as unknown);
}

/** The subset of the catalog JSON the tests read. */
export interface Catalog {
  skills: CatalogSkill[];
  targets: CatalogTarget[];
  unconfiguredSiblings?: CatalogTarget[];
  duplicateGroups?: unknown[];
  driftedGroups?: unknown[];
  brokenLinks?: unknown[];
  lastScanMs?: number;
}

export interface CatalogSkill {
  slug: string;
  antigravityValid?: boolean;
  [key: string]: unknown;
}

export interface CatalogTarget {
  id: string;
  displayName?: string;
  skillsDir?: string;
}

export interface RpcResponse {
  jsonrpc: string;
  id?: number;
  result?: { ok?: boolean; [key: string]: unknown };
  error?: unknown;
}
