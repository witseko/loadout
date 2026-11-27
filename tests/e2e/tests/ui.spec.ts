/** Browser tests of the embedded switchboard UI. */
import { expect, test } from '../support/fixtures.js';
import { isSymlink, pathExists } from '../support/fs-checks.js';

test('switchboard renders the skill matrix and toggles a skill from the browser', async ({ sandbox, startUi, page }) => {
  sandbox.mkdir('.claude/skills');
  sandbox.writeSkill('.agents/skills', 'web-skill', 'Skill toggled from the browser');
  const { url } = await startUi();
  const consoleErrors: string[] = [];
  page.on('console', (message) => {
    if (message.type() === 'error') {
      consoleErrors.push(message.text());
    }
  });
  const externalRequests: string[] = [];
  page.on('request', (pageRequest) => {
    if (!pageRequest.url().startsWith(url) && !pageRequest.url().startsWith('data:')) {
      externalRequests.push(pageRequest.url());
    }
  });

  await page.goto(url);
  const matrix = page.getByRole('table', { name: 'Skill wiring matrix' });
  await expect(matrix).toBeVisible();
  await expect(matrix.getByText('web-skill', { exact: true })).toBeVisible();

  // Jack buttons are labelled "<slug> in <target display name>: <state>". Pick the claude-main column.
  const catalog = (await (await page.request.get(`${url}/api/catalog`)).json()) as {
    targets: { id: string; displayName: string }[];
  };
  const claudeMain = catalog.targets.find((target) => target.id === 'claude-main');
  expect(claudeMain).toBeDefined();
  const jackLabel = `web-skill in ${claudeMain?.displayName ?? ''}`;
  const linkPath = sandbox.path('.claude/skills/web-skill');

  await page.getByRole('button', { name: `${jackLabel}: unwired` }).click();
  await expect(page.getByRole('button', { name: `${jackLabel}: wired` })).toBeEnabled();
  await expect.poll(() => isSymlink(linkPath)).toBe(true);

  await page.getByRole('button', { name: `${jackLabel}: wired` }).click();
  await expect(page.getByRole('button', { name: `${jackLabel}: unwired` })).toBeEnabled();
  await expect.poll(() => pathExists(linkPath)).toBe(false);

  expect(consoleErrors).toEqual([]);
  expect(externalRequests).toEqual([]);
});

test('theme button switches between dark and light', async ({ sandbox, startUi, page }) => {
  sandbox.writeSkill('.agents/skills', 'theme-skill', 'Skill present so the matrix renders');
  const { url } = await startUi();

  await page.goto(url);
  const root = page.locator('html');
  await expect(root).toHaveAttribute('data-theme', 'dark');
  await page.getByRole('button', { name: 'Light / Dark' }).click();
  await expect(root).toHaveAttribute('data-theme', 'light');
});
