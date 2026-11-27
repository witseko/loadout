/** HTTP surface of `singlet ui`: static assets, catalog, toggles, skill files, and profiles. */
import fs from 'node:fs';
import { expect, test } from '../support/fixtures.js';
import { isSymlink, pathExists } from '../support/fs-checks.js';
import type { Sandbox } from '../support/sandbox.js';

const EXTERNAL_HOSTS = ['https://cdn.', 'unpkg.com', 'fonts.googleapis.com', 'cdnjs.cloudflare.com'];

function seedWebSkill(sandbox: Sandbox): void {
  sandbox.mkdir('.claude/skills');
  sandbox.writeSkill('.agents/skills', 'web-skill', 'Skill verified via embedded HTTP Web UI');
  sandbox.write('.agents/skills/web-skill/scripts/check.sh', '#!/usr/bin/env bash\necho check-v1\n');
}

test('serves the embedded UI assets without external links', async ({ sandbox, startUi, request }) => {
  seedWebSkill(sandbox);
  const { url } = await startUi();

  const page = await request.get(`${url}/`);
  expect(page.status()).toBe(200);
  const html = await page.text();
  expect(html).toContain('<singlet-switchboard');
  for (const host of EXTERNAL_HOSTS) {
    expect(html).not.toContain(host);
  }

  const css = await (await request.get(`${url}/app.css`)).text();
  expect(css).toContain('--singlet-');
  expect(css.toUpperCase()).toContain('#E0DDBD');
  expect(css.toUpperCase()).toContain('#C8D46A');
  expect(css).toMatch(/\[data-theme=["']light["']\]/);

  const script = await request.get(`${url}/switchboard.js`);
  expect(script.status()).toBe(200);
  const js = await script.text();
  expect(js).toContain('singlet-switchboard');
  expect(js).toContain('customElements');
});

test('catalog, toggle, multi-file skill editing, and profiles work over the API', async ({ sandbox, startUi, request }) => {
  seedWebSkill(sandbox);
  const { url } = await startUi();
  const linkPath = sandbox.path('.claude/skills/web-skill');
  const toggle = (enabled: boolean) =>
    request.post(`${url}/api/toggle`, { data: { skill: 'web-skill', targetId: 'claude-main', enabled } });
  const profiles = (action: string) => request.post(`${url}/api/profiles`, { data: { action, name: 'e2e-snapshot' } });

  const catalog = (await (await request.get(`${url}/api/catalog`)).json()) as { skills: { slug: string }[] };
  expect(catalog.skills.map((skill) => skill.slug)).toContain('web-skill');

  expect((await toggle(true)).status()).toBe(200);
  expect(isSymlink(linkPath)).toBe(true);

  const detail = (await (await request.get(`${url}/api/skill?slug=web-skill`)).json()) as {
    files: { path: string; content?: string }[];
  };
  expect(detail.files.map((file) => file.path)).toEqual(expect.arrayContaining(['SKILL.md', 'scripts/check.sh']));
  expect(detail.files.find((file) => file.path === 'scripts/check.sh')?.content).toContain('check-v1');

  const save = await request.post(`${url}/api/skill`, {
    data: { slug: 'web-skill', relPath: 'scripts/check.sh', content: '#!/usr/bin/env bash\necho check-v2\n' },
  });
  expect(save.status()).toBe(200);
  expect(fs.readFileSync(sandbox.path('.agents/skills/web-skill/scripts/check.sh'), 'utf8')).toContain('check-v2');

  expect((await request.get(`${url}/api/profiles`)).status()).toBe(200);
  const saved = await profiles('save');
  expect(saved.status()).toBe(200);
  expect(((await saved.json()) as { ok: boolean }).ok).toBe(true);

  expect((await toggle(false)).status()).toBe(200);
  expect(pathExists(linkPath)).toBe(false);

  const applied = await profiles('apply');
  expect(applied.status()).toBe(200);
  expect(((await applied.json()) as { ok: boolean }).ok).toBe(true);
  expect(isSymlink(linkPath)).toBe(true);

  expect((await toggle(false)).status()).toBe(200);
  expect(pathExists(linkPath)).toBe(false);
});

test('rejects path traversal when saving skill files', async ({ sandbox, startUi, request }) => {
  seedWebSkill(sandbox);
  const { url } = await startUi();

  const response = await request.post(`${url}/api/skill`, {
    data: { slug: 'web-skill', relPath: '../escape.txt', content: 'nope' },
  });
  const rejected = response.status() >= 400 || ((await response.json()) as { ok?: boolean }).ok === false;
  expect(rejected).toBe(true);
  expect(pathExists(sandbox.path('.agents/skills/escape.txt'))).toBe(false);
});

test('rejects cross-site and DNS-rebinding requests', async ({ sandbox, startUi, request }) => {
  seedWebSkill(sandbox);
  const { url } = await startUi();
  const plantedPath = sandbox.path('.agents/skills/web-skill/planted.txt');
  const plant = { slug: 'web-skill', relPath: 'planted.txt', content: 'nope' };

  const foreignOrigin = await request.post(`${url}/api/skill`, {
    data: plant,
    headers: { Origin: 'https://evil.example' },
  });
  expect(foreignOrigin.status()).toBe(403);

  const plainText = await request.post(`${url}/api/skill`, {
    data: JSON.stringify(plant),
    headers: { 'Content-Type': 'text/plain' },
  });
  expect(plainText.status()).toBe(403);

  const reboundHost = await request.get(`${url}/api/catalog`, { headers: { Host: 'evil.example' } });
  expect(reboundHost.status()).toBe(403);

  const catalog = await request.get(`${url}/api/catalog`);
  expect(catalog.status()).toBe(200);
  expect(catalog.headers()['access-control-allow-origin']).toBeUndefined();
  expect(pathExists(plantedPath)).toBe(false);
});
