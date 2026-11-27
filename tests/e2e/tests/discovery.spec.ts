/** Multi-agent and multi-account discovery across every supported tool layout. */
import { expect, test } from '../support/fixtures.js';
import type { Sandbox } from '../support/sandbox.js';

const PLAYWRIGHT_SKILL = `---
name: playwright-e2e
description: Run Playwright E2E browser tests across viewports
tags: [testing, browser]
---

# Playwright E2E
`;

function seedDiscoveryLayout(sandbox: Sandbox): void {
  sandbox.write('.claude/skills/playwright-e2e/SKILL.md', PLAYWRIGHT_SKILL);
  sandbox.write('.claude/skills/playwright-e2e/scripts/run.sh', '#!/usr/bin/env bash\necho playwright\n');
  // Valid for Claude, but Antigravity rejects a SKILL.md without frontmatter.
  sandbox.write('.claude/skills/code-review/SKILL.md', '# Code Review\n\nReviews pull requests.\n');
  sandbox.writeSkill('.claude/skills', 'drifted-skill', 'Drifted skill variant A', 'Line one shared.\nVersion A.\n');

  sandbox.write('.claude-second/settings.json', '{"theme": "dark"}');
  sandbox.copyDir('.claude/skills/playwright-e2e', '.claude-second/skills/playwright-e2e');
  sandbox.writeSkill('.codex/skills', 'drifted-skill', 'Drifted skill variant B', 'Line one shared.\nVersion B.\n');

  sandbox.writeSkill('.gemini/config/skills', 'gemini-helper', 'Gemini Antigravity helper skill');
  sandbox.writeSkill('.gemini/config/plugins/foo/skills', 'plugin-only', 'Read-only plugin skill');

  sandbox.writeSkill('.config/opencode/skills', 'opencode-skill', 'OpenCode native skill');
  sandbox.writeSkill('extra-opencode-skills', 'oc-jsonc-skill', 'Skill from an opencode.jsonc custom path');
  const extraPath = sandbox.path('extra-opencode-skills');
  sandbox.write(
    '.config/opencode/opencode.jsonc',
    `{\n  // line comment\n  /* block\n     comment */\n  "skills": { "paths": ["${extraPath}"] },\n  "skillPaths": ["${extraPath}"]\n}\n`,
  );

  sandbox.copyDir('.claude/skills/playwright-e2e', '.zcode/skills/playwright-e2e');

  sandbox.symlink(sandbox.path('non-existent-dir/ghost-skill'), '.claude/skills/broken-link');

  sandbox.mkdir('dev/loop-repo/.git');
  sandbox.writeSkill('dev/loop-repo/.claude/skills', 'loop-dir', 'Skill with a circular symlink inside');
  sandbox.symlink(sandbox.path('dev/loop-repo/.claude/skills/loop-dir'), 'dev/loop-repo/.claude/skills/loop-dir/self-loop');
  sandbox.symlink(sandbox.path('.claude/skills/mutual-b'), '.claude/skills/mutual-a');
  sandbox.symlink(sandbox.path('.claude/skills/mutual-a'), '.claude/skills/mutual-b');

  sandbox.mkdir('dev/app/.git');
  sandbox.writeSkill('dev/app/.claude/skills', 'deploy', 'Project-local deployment workflow');
}

test('scan discovers skills, accounts, duplicates, drift, broken links, and frontmatter issues', ({ sandbox }) => {
  seedDiscoveryLayout(sandbox);

  // The command timeout also catches a hang on the circular symlinks.
  const catalog = sandbox.scanJson(['--roots', sandbox.path('dev')]);

  const allTargets = [...catalog.targets, ...(catalog.unconfiguredSiblings ?? [])];
  expect(allTargets.some((t) => `${t.id} ${t.skillsDir ?? ''}`.includes('claude-second'))).toBe(true);

  const slugs = catalog.skills.map((skill) => skill.slug);
  for (const slug of ['playwright-e2e', 'code-review', 'drifted-skill', 'gemini-helper', 'opencode-skill', 'oc-jsonc-skill', 'deploy']) {
    expect(slugs).toContain(slug);
  }
  expect(JSON.stringify(catalog)).toContain('plugin-only');

  expect(JSON.stringify(catalog.duplicateGroups ?? [])).toContain('playwright-e2e');
  expect(JSON.stringify(catalog.driftedGroups ?? [])).toContain('drifted-skill');
  expect(JSON.stringify(catalog.brokenLinks ?? [])).toContain('broken-link');

  const codeReview = catalog.skills.find((skill) => skill.slug === 'code-review');
  expect(codeReview?.antigravityValid).toBe(false);
});
