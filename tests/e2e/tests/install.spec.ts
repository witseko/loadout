/** `install` and `update` argument handling. Network installs are covered by the unit tests with local repos. */
import { expect, test } from '../support/fixtures.js';
import { pathExists } from '../support/fs-checks.js';

test('install rejects missing and non-GitHub URLs without touching the store', ({ sandbox }) => {
  expect(sandbox.run(['install']).status).toBe(1);

  for (const url of ['http://github.com/acme/skills', 'https://example.com/acme/skills', 'https://github.com/acme/skills/tree/main/../x']) {
    const result = sandbox.run(['install', url]);
    expect(result.status).toBe(1);
    expect(result.stderr).toContain('singlet install:');
  }
  expect(pathExists(sandbox.path('.agents/skills'))).toBe(false);
});

test('update reports skills without a recorded source', ({ sandbox }) => {
  sandbox.writeSkill('.agents/skills', 'local-skill', 'A skill written by hand');

  expect(sandbox.runOk(['update'])).toContain('No skills installed from GitHub');

  const result = sandbox.run(['update', 'local-skill', '--json']);
  expect(result.status).toBe(1);
  expect(JSON.parse(result.stdout)).toEqual([expect.objectContaining({ slug: 'local-skill', state: 'failed' })]);
});

test('install and update endpoints validate input over HTTP', async ({ sandbox, startUi, request }) => {
  sandbox.mkdir('.claude/skills');
  const { url } = await startUi();

  const missing = await request.post(`${url}/api/install`, { data: {} });
  expect(missing.status()).toBe(400);

  const bad = await request.post(`${url}/api/install`, { data: { url: 'https://example.com/acme/skills' } });
  expect(bad.status()).toBe(400);
  expect(await bad.json()).toEqual(expect.objectContaining({ ok: false, error: expect.stringContaining('GitHub') }));

  const check = await request.post(`${url}/api/sources/check`, { data: {} });
  expect(check.status()).toBe(200);
  expect(await check.json()).toEqual({ statuses: [] });

  const update = await request.post(`${url}/api/sources/update`, { data: { skills: ['nope'] } });
  expect((await update.json()).statuses).toEqual([expect.objectContaining({ slug: 'nope', state: 'failed' })]);

  // The trust check still applies: a POST without a JSON Content-Type is refused.
  const plain = await request.post(`${url}/api/install`, { data: 'url=x', headers: { 'Content-Type': 'text/plain' } });
  expect(plain.status()).toBe(403);
});

test('install and update are reachable over JSON-RPC and MCP', ({ sandbox }) => {
  const rpc = sandbox.rpc([
    { jsonrpc: '2.0', id: 1, method: 'skill.install', params: {} },
    { jsonrpc: '2.0', id: 2, method: 'skill.install', params: { url: 'http://github.com/acme/skills' } },
    { jsonrpc: '2.0', id: 3, method: 'source.check', params: {} },
  ]);
  const byId = new Map(rpc.map((r) => [r.id, r]));
  expect(JSON.stringify(byId.get(1)?.error)).toContain('-32602');
  expect(byId.get(2)?.result).toEqual(expect.objectContaining({ ok: false }));
  expect(byId.get(3)?.result).toEqual({ statuses: [] });

  const call = { jsonrpc: '2.0', id: 4, method: 'tools/call', params: { name: 'singlet_install_skills', arguments: { url: 'not a url' } } };
  const stdout = sandbox.runOk(['mcp'], JSON.stringify(call) + '\n');
  const response = JSON.parse(stdout.trim().split('\n').pop() ?? '{}');
  expect(response.result?.isError).toBe(true);
});
