/** Antigravity frontmatter auto-fix through `doctor --fix` and RPC skill.fixFrontmatter. */
import fs from 'node:fs';
import { expect, test } from '../support/fixtures.js';

const FRONTMATTER_PATTERN = /^---\s*\n([\s\S]*?)\n---\s*\n([\s\S]*)$/;

const EXPECTED_BODIES: Record<string, string[]> = {
  'no-fm-skill': ['# Automated Release Helper', 'Prepares release notes, tags commits', '1. Run verification suite', '2. Tag release'],
  'wrong-name-skill': ['Ensures database migrations are reversible and tested.'],
  'rpc-fm-skill': ['# RPC Fix Skill', 'Verifies JSON-RPC frontmatter auto-fix without losing markdown body content.'],
};

function escapeRegExp(text: string): string {
  return text.replace(/[.*+?^${}()|[\]\\]/g, '\\$&');
}

test('doctor --fix and RPC repair frontmatter and keep the markdown body', ({ sandbox }) => {
  sandbox.mkdir('.claude/skills');
  sandbox.mkdir('.gemini/config/skills');
  sandbox.write(
    '.agents/skills/no-fm-skill/SKILL.md',
    '# Automated Release Helper\n\nPrepares release notes, tags commits, and validates artifacts safely.\n\n## Steps\n1. Run verification suite\n2. Tag release\n',
  );
  sandbox.write(
    '.agents/skills/wrong-name-skill/SKILL.md',
    '---\nname: wrong-slug\ndescription:\n---\n\nEnsures database migrations are reversible and tested.\n',
  );

  sandbox.runOk(['doctor', '--fix']);

  sandbox.write(
    '.agents/skills/rpc-fm-skill/SKILL.md',
    '# RPC Fix Skill\n\nVerifies JSON-RPC frontmatter auto-fix without losing markdown body content.\n',
  );
  const responses = sandbox.rpc([
    { jsonrpc: '2.0', id: 1, method: 'skill.fixFrontmatter', params: { skill: 'rpc-fm-skill', apply: false } },
    { jsonrpc: '2.0', id: 2, method: 'skill.fixFrontmatter', params: { skill: 'rpc-fm-skill', apply: true } },
  ]);
  expect(responses[1]?.result?.ok).toBe(true);

  for (const [slug, snippets] of Object.entries(EXPECTED_BODIES)) {
    const text = fs.readFileSync(sandbox.path(`.agents/skills/${slug}/SKILL.md`), 'utf8');
    const match = FRONTMATTER_PATTERN.exec(text);
    expect(match, `${slug} has no frontmatter block`).not.toBeNull();
    const [, frontmatter = '', body = ''] = match ?? [];
    expect(frontmatter).toMatch(new RegExp(`^name:\\s*['"]?${escapeRegExp(slug)}['"]?\\s*$`, 'm'));
    expect(frontmatter).toMatch(/^description:\s*['"]?[^'"\s]/m);
    for (const snippet of snippets) {
      expect(body).toContain(snippet);
    }
  }

  const catalog = sandbox.scanJson();
  for (const slug of Object.keys(EXPECTED_BODIES)) {
    expect(catalog.skills.find((skill) => skill.slug === slug)?.antigravityValid, slug).toBe(true);
  }
});
