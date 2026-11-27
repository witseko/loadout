/** Shared filesystem locations used by the e2e suite. */
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const E2E_ROOT = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');

export const REPO_ROOT = path.resolve(E2E_ROOT, '..', '..');
export const SINGLET_BIN = path.join(REPO_ROOT, 'bin', 'singlet');

/** Global setup writes the release build log and timing results here. */
export const BUILD_OUTPUT_DIR = path.join(E2E_ROOT, '.build');
export const BUILD_LOG_PATH = path.join(BUILD_OUTPUT_DIR, 'release.log');
export const VERSION_TIMINGS_PATH = path.join(BUILD_OUTPUT_DIR, 'version-timings.json');
