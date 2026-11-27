/** Starts `loadout ui` on a random port inside a sandbox. */
import { spawn, type ChildProcess } from 'node:child_process';
import { LOADOUT_BIN } from './paths.js';
import type { Sandbox } from './sandbox.js';

const STARTUP_TIMEOUT_MS = 10_000;
const UI_URL_PATTERN = /LOADOUT_UI_URL=(http:\/\/127\.0\.0\.1:\d+)/;

export interface UiServer {
  url: string;
  stop: () => Promise<void>;
}

/** Spawns `loadout ui --port 0 --no-open` and resolves once it prints its URL. */
export async function startUiServer(sandbox: Sandbox): Promise<UiServer> {
  const child = spawn(LOADOUT_BIN, ['--home', sandbox.root, 'ui', '--port', '0', '--no-open'], {
    env: sandbox.env(),
    stdio: ['ignore', 'pipe', 'pipe'],
  });
  const url = await waitForUrl(child);
  return { url, stop: () => stopProcess(child) };
}

function waitForUrl(child: ChildProcess): Promise<string> {
  return new Promise((resolve, reject) => {
    let output = '';
    const timer = setTimeout(() => {
      child.kill();
      reject(new Error(`Timed out waiting for LOADOUT_UI_URL. Output:\n${output}`));
    }, STARTUP_TIMEOUT_MS);

    const onData = (chunk: Buffer): void => {
      output += chunk.toString();
      const match = UI_URL_PATTERN.exec(output);
      if (match?.[1]) {
        clearTimeout(timer);
        resolve(match[1]);
      }
    };
    child.stdout?.on('data', onData);
    child.stderr?.on('data', onData);
    child.once('exit', (code) => {
      clearTimeout(timer);
      reject(new Error(`loadout ui exited early with code ${code}. Output:\n${output}`));
    });
  });
}

function stopProcess(child: ChildProcess): Promise<void> {
  if (child.exitCode !== null || child.signalCode !== null) {
    return Promise.resolve();
  }
  return new Promise((resolve) => {
    child.once('exit', () => resolve());
    child.kill();
  });
}
