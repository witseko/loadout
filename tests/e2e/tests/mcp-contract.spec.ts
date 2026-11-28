/** `singlet mcp` server behavior and the t3code plugin RPC contract. */
import fs from 'node:fs';
import path from 'node:path';
import { expect, test } from '../support/fixtures.js';
import { REPO_ROOT } from '../support/paths.js';
import { parseJsonLines, type RpcResponse } from '../support/sandbox.js';

const EXPECTED_TOOLS = ['singlet_list_catalog', 'singlet_audit_duplicates', 'singlet_toggle_skill', 'singlet_hydrate_worktree', 'singlet_install_skills', 'singlet_check_skill_updates', 'singlet_update_skills'];

const REQUIRED_RPC_METHODS = [
  'catalog.get',
  'catalog.scan',
  'skill.toggle',
  'skill.consolidate',
  'skill.resolveDrift',
  'skill.fixFrontmatter',
  'skill.get',
  'skill.save',
  'target.add',
  'target.remove',
  'target.clone',
  'profile.list',
  'profile.save',
  'profile.apply',
  'profile.delete',
  'quarantine.undo',
  'worktree.hydrate',
  'skill.install',
  'source.check',
  'source.update',
];

function contentLengthFrame(message: object): string {
  const body = JSON.stringify(message);
  return `Content-Length: ${Buffer.byteLength(body)}\r\n\r\n${body}`;
}

test('mcp answers initialize, tools/list, and tools/call over newline-delimited JSON', ({ sandbox }) => {
  sandbox.mkdir('.claude/skills');
  sandbox.writeSkill('.agents/skills', 'mcp-skill', 'Skill verified over MCP stdio server', '# MCP Skill\n');

  const requests = [
    { jsonrpc: '2.0', id: 1, method: 'initialize', params: { protocolVersion: '2024-11-05', capabilities: {}, clientInfo: { name: 'e2e', version: '1.0' } } },
    { jsonrpc: '2.0', method: 'notifications/initialized', params: {} },
    { jsonrpc: '2.0', id: 2, method: 'tools/list', params: {} },
    { jsonrpc: '2.0', id: 3, method: 'tools/call', params: { name: 'singlet_list_catalog', arguments: {} } },
  ];
  const stdout = sandbox.runOk(['mcp'], requests.map((request) => JSON.stringify(request)).join('\n') + '\n');
  const responses = new Map(
    (parseJsonLines(stdout) as RpcResponse[]).filter((r) => r.id !== undefined).map((r) => [r.id, r]),
  );

  for (const id of [1, 2, 3]) {
    expect(responses.get(id)?.jsonrpc, `response ${id}`).toBe('2.0');
    expect(responses.get(id)?.error, `response ${id}`).toBeUndefined();
  }
  expect(responses.get(1)?.result).toEqual(expect.objectContaining({ protocolVersion: expect.anything(), serverInfo: expect.anything() }));
  const tools = (responses.get(2)?.result?.tools ?? []) as { name: string }[];
  expect(tools.map((tool) => tool.name)).toEqual(expect.arrayContaining(EXPECTED_TOOLS));
  expect(JSON.stringify(responses.get(3)?.result)).toContain('mcp-skill');
});

test('mcp reads a skill resource over Content-Length framing', ({ sandbox }) => {
  sandbox.writeSkill('.agents/skills', 'mcp-skill', 'Skill verified over MCP stdio server', '# MCP Skill\n');

  const stdout = sandbox.runOk(
    ['mcp'],
    contentLengthFrame({ jsonrpc: '2.0', id: 10, method: 'resources/read', params: { uri: 'skill://mcp-skill/SKILL.md' } }),
  );
  expect(stdout).toContain('MCP Skill');
});

test('protocol.ts declares exactly the RPC methods rpc.cpp dispatches', () => {
  const pluginDir = path.join(REPO_ROOT, 'contrib', 't3code-plugin');
  expect(fs.existsSync(path.join(pluginDir, 'node_modules'))).toBe(false);

  const protocolSource = fs.readFileSync(path.join(pluginDir, 'src', 'protocol.ts'), 'utf8');
  const rpcSource = fs.readFileSync(path.join(REPO_ROOT, 'src', 'surfaces', 'rpc.cpp'), 'utf8');

  const methodList = /export\s+const\s+SINGLET_RPC_METHODS\s*=\s*\[([\s\S]*?)\]\s*as\s+const/.exec(protocolSource);
  expect(methodList, 'SINGLET_RPC_METHODS not found in protocol.ts').not.toBeNull();
  const protocolMethods = [...(methodList?.[1] ?? '').matchAll(/["']([a-z]+\.[A-Za-z0-9_]+)["']/g)].map((m) => m[1]);

  const dispatchScope = /dispatch_rpc_method\s*\([\s\S]*?\n\}/.exec(rpcSource)?.[0] ?? rpcSource;
  const rpcMethods = [...dispatchScope.matchAll(/method\s*==\s*"([a-z]+\.[A-Za-z0-9_]+)"/g)].map((m) => m[1]);

  expect(protocolMethods).toEqual(expect.arrayContaining(REQUIRED_RPC_METHODS));
  expect(new Set(protocolMethods)).toEqual(new Set(rpcMethods));
});
