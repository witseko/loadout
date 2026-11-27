# Contributing

## Build

You need `clang++` or `g++` with C++20 support, `make`, and macOS or Linux.

| Command | Result |
| :--- | :--- |
| `make` or `make release` | Optimized, stripped binary at `bin/loadout` |
| `make debug` | ASan + UBSan binary at `bin/loadout-debug` |
| `make test` | Build and run the unit tests (`tests/test_util.cpp`, `tests/test_core.cpp`) |
| `make clean` | Remove `bin/` |

The build uses strict warnings with `-Werror`. A warning is a build failure.

## Test

Run both suites before you open a pull request:

```bash
make test
```

```bash
cd tests/e2e && npm ci && npx playwright install chromium && npx playwright test
```

The end-to-end suite in `tests/e2e` uses [Playwright](https://playwright.dev). It needs Node.js 24 or later. Its Node dependencies stay in `tests/e2e`, so the repository root has no `package.json`. Global setup runs `make clean && make release` first, so you test a fresh `bin/loadout`. The suite covers the CLI, JSON-RPC, MCP, and HTTP surfaces, and it drives the switchboard UI in Chromium. Each test uses a fresh temporary home folder, so it never touches your real `~/.agents` or agent folders. One test fails if `prototype/index.html` has uncommitted changes, because that file is the frozen design reference.

To run one file, pass its name, for example `npx playwright test ui`. Use `npx playwright test --ui` to debug interactively.

GitHub Actions (`.github/workflows/ci.yml`) runs the unit tests and the e2e suite on macOS and Ubuntu for each push to `main` and each pull request.

Add a unit test for new behavior in `util` or `core`. Add an e2e test in `tests/e2e/tests` when you add a new command or surface.

## Code layout

See [docs/architecture.md](docs/architecture.md). In short: `src/util` has pure helpers, `src/core` has the engine, `src/surfaces` has the CLI, TUI, HTTP, RPC, and MCP entry points. Dependencies flow `surfaces -> core -> util`.

## Rules

- **No third-party dependencies** in the C++ code. Use the standard library and POSIX.
- **UI files are copied into the binary.** When you edit `src/ui/index.html`, `src/ui/app.css`, or `src/ui/switchboard.js`, make the same change in `src/ui/embedded_ui.hpp`.
- **RPC contract.** When you add or change a JSON-RPC method in `src/surfaces/rpc.cpp`, update `contrib/t3code-plugin/src/protocol.ts` to match.
- **Never lose user data.** Any code that replaces a real folder must quarantine it first and record it so `loadout undo` can restore it.
- **Test in a sandbox.** Use `--home <tmp-dir>` or `LOADOUT_HOME` when you try changes by hand.
- Keep functions small and names descriptive. Add a one-line doc comment (`///`) to every public type and function.

## Commits and pull requests

- Commit messages: short, imperative, one line, for example `fix drift detection for symlinked skills`.
- Keep a pull request to one topic.
- Add a line to the `Unreleased` section of [CHANGELOG.md](CHANGELOG.md) for user-visible changes.
