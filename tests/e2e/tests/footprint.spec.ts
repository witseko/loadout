/** Zero-dependency and footprint audit of the release build. */
import { execFileSync } from 'node:child_process';
import fs from 'node:fs';
import path from 'node:path';
import { expect, test } from '@playwright/test';
import { BUILD_LOG_PATH, LOADOUT_BIN, REPO_ROOT, VERSION_TIMINGS_PATH } from '../support/paths.js';

const MAX_BINARY_BYTES = 1.5 * 1024 * 1024;
const MAX_MEDIAN_VERSION_MS = 10;
const REQUIRED_BUILD_FLAGS = ['-Wconversion', '-Wsign-conversion', '-Wshadow', '-Wpedantic', '-Werror', '-fstack-protector-strong'];
const ALLOWED_DARWIN_LIBS = new Set(['/usr/lib/libc++.1.dylib', '/usr/lib/libSystem.B.dylib']);
const SKIPPED_DIRS = new Set(['.git', 'node_modules', '.build', 'test-results', 'playwright-report']);
const FORBIDDEN_DASHES = /[\u2013\u2014]/;

test('release build has no warnings and uses the strict and hardening flags', () => {
  const buildLog = fs.readFileSync(BUILD_LOG_PATH, 'utf8');
  expect(buildLog).not.toMatch(/(^|[^-])warning:/m);
  for (const flag of REQUIRED_BUILD_FLAGS) {
    expect(buildLog, `missing build flag ${flag}`).toContain(flag);
  }
  expect(buildLog).toMatch(/_LIBCPP_HARDENING_MODE|_GLIBCXX_ASSERTIONS/);
});

test('frozen prototype/index.html has no uncommitted changes', () => {
  const diff = execFileSync('git', ['-C', REPO_ROOT, 'diff', '--', 'prototype/index.html'], { encoding: 'utf8' });
  expect(diff).toBe('');
});

test('repository files contain no en or em dashes', () => {
  const files = [path.join(REPO_ROOT, 'Makefile'), path.join(REPO_ROOT, 'README.md')];
  for (const folder of ['src', 'contrib', 'tests', '.github']) {
    files.push(...listFiles(path.join(REPO_ROOT, folder)));
  }
  const offenders = files
    .filter((file) => FORBIDDEN_DASHES.test(fs.readFileSync(file, 'utf8')))
    .map((file) => path.relative(REPO_ROOT, file));
  expect(offenders).toEqual([]);
});

test('binary links only libc++ and libSystem on macOS', () => {
  test.skip(process.platform !== 'darwin', 'otool check is macOS only');
  const otoolLines = execFileSync('otool', ['-L', LOADOUT_BIN], { encoding: 'utf8' }).trim().split('\n').slice(1);
  const linkedLibs = otoolLines.map((line) => line.trim().split(/\s+/)[0] ?? '');
  expect(linkedLibs.filter((lib) => !ALLOWED_DARWIN_LIBS.has(lib))).toEqual([]);
});

test('stripped binary is smaller than 1.5 MB', () => {
  expect(fs.statSync(LOADOUT_BIN).size).toBeLessThan(MAX_BINARY_BYTES);
});

test('cold --version median latency is under 10 ms', () => {
  const { timingsMs } = JSON.parse(fs.readFileSync(VERSION_TIMINGS_PATH, 'utf8')) as { timingsMs: number[] };
  expect(median(timingsMs)).toBeLessThan(MAX_MEDIAN_VERSION_MS);
});

test('repository root has no package.json or node_modules', () => {
  expect(fs.existsSync(path.join(REPO_ROOT, 'package.json'))).toBe(false);
  expect(fs.existsSync(path.join(REPO_ROOT, 'node_modules'))).toBe(false);
});

function listFiles(folder: string): string[] {
  if (!fs.existsSync(folder)) {
    return [];
  }
  const files: string[] = [];
  for (const entry of fs.readdirSync(folder, { withFileTypes: true })) {
    const fullPath = path.join(folder, entry.name);
    if (entry.isDirectory() && !SKIPPED_DIRS.has(entry.name)) {
      files.push(...listFiles(fullPath));
    } else if (entry.isFile()) {
      files.push(fullPath);
    }
  }
  return files;
}

function median(values: number[]): number {
  const sorted = [...values].sort((a, b) => a - b);
  const middle = Math.floor(sorted.length / 2);
  const upper = sorted[middle] ?? 0;
  return sorted.length % 2 === 0 ? ((sorted[middle - 1] ?? 0) + upper) / 2 : upper;
}
