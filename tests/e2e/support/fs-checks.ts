/** Small filesystem predicates used by assertions. */
import { createHash } from 'node:crypto';
import fs from 'node:fs';
import path from 'node:path';

export function isSymlink(fullPath: string): boolean {
  return fs.lstatSync(fullPath, { throwIfNoEntry: false })?.isSymbolicLink() ?? false;
}

/** True when the path is a real folder, not a symlink to one. */
export function isRealDir(fullPath: string): boolean {
  return fs.lstatSync(fullPath, { throwIfNoEntry: false })?.isDirectory() ?? false;
}

/** True when anything, including a dangling symlink, exists at the path. */
export function pathExists(fullPath: string): boolean {
  return fs.lstatSync(fullPath, { throwIfNoEntry: false }) !== undefined;
}

export function sha256(fullPath: string): string {
  return createHash('sha256').update(fs.readFileSync(fullPath)).digest('hex');
}

/** Recursively lists files named `fileName` under `folder`. */
export function findFiles(folder: string, fileName: string): string[] {
  if (!fs.existsSync(folder)) {
    return [];
  }
  return fs
    .readdirSync(folder, { recursive: true, encoding: 'utf8' })
    .filter((relativePath) => path.basename(relativePath) === fileName)
    .map((relativePath) => path.join(folder, relativePath));
}
