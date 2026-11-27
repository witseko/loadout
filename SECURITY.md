# Security Policy

## Supported versions

Only the latest release gets security fixes.

| Version | Supported |
| :--- | :--- |
| 0.1.x | Yes |

## Report a vulnerability

Do not open a public issue for a security problem.

Report it privately through GitHub: open the [Security tab](https://github.com/witseko/singlet/security) and click **Report a vulnerability**. Include:

- The `singlet` version (`singlet --version`) and your OS.
- The steps to reproduce the problem.
- What an attacker can do with it.

You get a first reply within 7 days. When a fix is ready, it ships in a new release, and the advisory is published with credit to you, unless you ask to stay anonymous.

## Scope

These areas matter most:

- **File system safety.** `singlet` moves folders, creates symlinks, and restores quarantine batches. A path that escapes `~/.agents` or a target folder, or a way to lose user data, is a vulnerability.
- **Local UI server.** `singlet ui` binds to `127.0.0.1` and has no authentication. A way for a web page or another host to read or change skills through it is a vulnerability.
- **RPC and MCP servers.** Input on stdin that makes `singlet` write outside its folders, or crash on malformed JSON, is in scope.

Out of scope: problems that need an attacker who already has write access to your home folder, and running the UI server on a port that you expose to other machines on purpose.
