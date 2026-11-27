/** Git worktree hydration through RPC worktree.hydrate. */
import { execFileSync } from 'node:child_process';
import fs from 'node:fs';
import path from 'node:path';
import { expect, test } from '../support/fixtures.js';

function git(args: string[]): string {
  return execFileSync('git', args, { encoding: 'utf8' });
}

function findSymlinks(folder: string): string[] {
  return fs
    .readdirSync(folder, { recursive: true, encoding: 'utf8' })
    .filter((relativePath) => !relativePath.split(path.sep).includes('.git'))
    .filter((relativePath) => fs.lstatSync(path.join(folder, relativePath)).isSymbolicLink());
}

test('worktree.hydrate links skills into a worktree and keeps git status clean', ({ sandbox }) => {
  sandbox.write('.claude-second/settings.json', '{}');
  sandbox.mkdir('.claude-second/skills');
  sandbox.writeSkill('.agents/skills', 'wt-canonical', 'Canonical skill enabled on claude-second');
  sandbox.runOk(['toggle', 'wt-canonical', '--on', 'claude-second']);

  const repo = sandbox.path('repo');
  const worktree = sandbox.path('wt-feat');
  sandbox.write('repo/README.md', '# Demo Repo\n');
  git(['init', '-q', repo]);
  git(['-C', repo, 'config', 'user.name', 'Loadout Test']);
  git(['-C', repo, 'config', 'user.email', 'loadout@example.com']);
  git(['-C', repo, 'add', 'README.md']);
  git(['-C', repo, 'commit', '-qm', 'Initial commit']);
  // An untracked project-local skill in the main checkout.
  sandbox.writeSkill('repo/.claude/skills', 'local-deploy', 'Untracked project-local deploy skill');
  git(['-C', repo, 'worktree', 'add', '-q', worktree, '-b', 'feat-branch']);

  const [response] = sandbox.rpc([
    {
      jsonrpc: '2.0',
      id: 42,
      method: 'worktree.hydrate',
      params: {
        worktreePath: worktree,
        repoRoot: repo,
        providerId: 'claude',
        env: { CLAUDE_CONFIG_DIR: sandbox.path('.claude-second') },
      },
    },
  ]);
  expect(response?.jsonrpc).toBe('2.0');
  expect(response?.id).toBe(42);
  expect(response?.error).toBeUndefined();

  expect(findSymlinks(worktree).length).toBeGreaterThan(0);
  expect(git(['-C', worktree, 'status', '--porcelain']).trim()).toBe('');
});
