/** Zero-data-loss consolidation into the canonical store, and quarantine undo. */
import { expect, test } from '../support/fixtures.js';
import { findFiles, isRealDir, isSymlink, sha256 } from '../support/fs-checks.js';

const COPY_LOCATIONS = ['.claude/skills/dup-skill', '.claude-second/skills/dup-skill', '.zcode/skills/dup-skill'];

test('dedup previews by default, applies with --apply, and undo restores byte-exact copies', ({ sandbox }) => {
  sandbox.write('.claude-second/settings.json', '{}');
  sandbox.writeSkill('.claude/skills', 'dup-skill', 'Duplicate skill across three tools', 'Byte-exact payload.\n');
  sandbox.write('.claude/skills/dup-skill/scripts/helper.sh', '#!/usr/bin/env bash\necho helper-v1\n');
  sandbox.copyDir('.claude/skills/dup-skill', '.claude-second/skills/dup-skill');
  sandbox.copyDir('.claude/skills/dup-skill', '.zcode/skills/dup-skill');
  const originalHash = sha256(sandbox.path('.claude/skills/dup-skill/SKILL.md'));

  for (const previewArgs of [[], ['--dry-run']]) {
    sandbox.runOk(['dedup', ...previewArgs]);
    expect(isSymlink(sandbox.path('.claude/skills/dup-skill'))).toBe(false);
    expect(isRealDir(sandbox.path('.agents/skills/dup-skill'))).toBe(false);
  }

  sandbox.runOk(['dedup', '--apply']);
  expect(isRealDir(sandbox.path('.agents/skills/dup-skill'))).toBe(true);
  expect(sha256(sandbox.path('.agents/skills/dup-skill/SKILL.md'))).toBe(originalHash);
  for (const location of COPY_LOCATIONS) {
    expect(isSymlink(sandbox.path(location)), location).toBe(true);
  }
  expect(findFiles(sandbox.path('.agents/quarantine'), 'manifest.json').length).toBeGreaterThanOrEqual(1);
  expect(findFiles(sandbox.path('.agents/quarantine'), 'SKILL.md').length).toBeGreaterThanOrEqual(1);

  sandbox.runOk(['undo']);
  for (const location of COPY_LOCATIONS) {
    expect(isRealDir(sandbox.path(location)), location).toBe(true);
    expect(sha256(sandbox.path(`${location}/SKILL.md`))).toBe(originalHash);
  }
});
