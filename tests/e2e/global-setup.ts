/**
 * Builds a clean release binary once, then measures cold `--version` latency
 * while nothing else runs, so parallel workers cannot skew the timing.
 */
import { execFileSync, spawnSync } from 'node:child_process';
import fs from 'node:fs';
import { BUILD_LOG_PATH, BUILD_OUTPUT_DIR, SINGLET_BIN, REPO_ROOT, VERSION_TIMINGS_PATH } from './support/paths.js';

const VERSION_RUNS = 10;

export default function globalSetup(): void {
  fs.mkdirSync(BUILD_OUTPUT_DIR, { recursive: true });
  execFileSync('make', ['clean'], { cwd: REPO_ROOT, stdio: 'ignore' });

  const build = spawnSync('make', ['release'], { cwd: REPO_ROOT, encoding: 'utf8' });
  fs.writeFileSync(BUILD_LOG_PATH, build.stdout + build.stderr);
  if (build.status !== 0) {
    throw new Error(`make release failed:\n${build.stdout}${build.stderr}`);
  }

  fs.writeFileSync(VERSION_TIMINGS_PATH, JSON.stringify(measureVersionLatency()));
}

function measureVersionLatency(): { version: string; timingsMs: number[] } {
  const timingsMs: number[] = [];
  let version = '';
  for (let run = 0; run < VERSION_RUNS; run++) {
    const start = process.hrtime.bigint();
    const result = spawnSync(SINGLET_BIN, ['--version'], { encoding: 'utf8' });
    const end = process.hrtime.bigint();
    if (result.status !== 0) {
      throw new Error(`singlet --version exited ${result.status}: ${result.stderr}`);
    }
    version = result.stdout.trim();
    timingsMs.push(Number(end - start) / 1e6);
  }
  return { version, timingsMs };
}
