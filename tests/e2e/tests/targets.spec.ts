/** Custom targets, account cloning, and independent per-account toggles. */
import { expect, test } from '../support/fixtures.js';
import { isRealDir, isSymlink, pathExists } from '../support/fs-checks.js';

test('custom target, target clone, and independent multi-account toggles', ({ sandbox }) => {
  sandbox.mkdir('.claude/skills');
  // Neither .claude-second/skills nor my-bot/skills exists yet: loadout must create them.
  sandbox.write('.claude-second/settings.json', '{}');
  sandbox.writeSkill('.agents/skills', 'alpha-skill', 'Alpha canonical skill');
  sandbox.writeSkill('.agents/skills', 'beta-skill', 'Beta canonical skill');

  sandbox.runOk(['target', 'add', 'custom-bot', sandbox.path('my-bot/skills'), '--kind', 'custom-lenient']);
  expect(sandbox.runOk(['target', 'list'])).toContain('custom-bot');

  sandbox.runOk(['toggle', 'alpha-skill', '--on', 'claude-main,custom-bot']);
  expect(isSymlink(sandbox.path('my-bot/skills/alpha-skill'))).toBe(true);
  expect(isSymlink(sandbox.path('.claude/skills/alpha-skill'))).toBe(true);

  sandbox.runOk(['target', 'clone', 'claude-main', 'claude-second']);
  expect(isSymlink(sandbox.path('.claude-second/skills/alpha-skill'))).toBe(true);

  sandbox.runOk(['toggle', 'alpha-skill', '--on', 'claude-second', '--off', 'claude-main']);
  expect(pathExists(sandbox.path('.claude/skills/alpha-skill'))).toBe(false);
  expect(isSymlink(sandbox.path('.claude-second/skills/alpha-skill'))).toBe(true);
  expect(isRealDir(sandbox.path('.agents/skills/alpha-skill'))).toBe(true);

  sandbox.runOk(['toggle', 'beta-skill', '--on', 'claude-second']);
  expect(isSymlink(sandbox.path('.claude-second/skills/beta-skill'))).toBe(true);
  expect(pathExists(sandbox.path('.claude/skills/beta-skill'))).toBe(false);
});
