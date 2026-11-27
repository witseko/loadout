/** Playwright fixtures: a fresh sandbox per test and an on-demand UI server inside it. */
import { test as base } from '@playwright/test';
import { Sandbox } from './sandbox.js';
import { startUiServer, type UiServer } from './ui-server.js';

interface LoadoutFixtures {
  sandbox: Sandbox;
  /** Starts `loadout ui` for the test's sandbox. Call it after seeding skills. */
  startUi: () => Promise<UiServer>;
}

export const test = base.extend<LoadoutFixtures>({
  sandbox: async ({}, use) => {
    const sandbox = new Sandbox();
    await use(sandbox);
    sandbox.dispose();
  },
  startUi: async ({ sandbox }, use) => {
    const servers: UiServer[] = [];
    await use(async () => {
      const server = await startUiServer(sandbox);
      servers.push(server);
      return server;
    });
    await Promise.all(servers.map((server) => server.stop()));
  },
});

export { expect } from '@playwright/test';
