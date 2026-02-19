# Copilot Instructions for `desktop-kDrive`

## Mandatory Context Loading
Before proposing code or review comments:
1. Read `AGENTS.md` (root).
2. Detect touched paths in the PR/diff.
3. Use the root `AGENTS.md` index to discover which sub-`AGENTS.md` files apply to touched paths.
4. If multiple sub-areas are touched, apply all relevant files and resolve conflicts with the nearest-file rule.

## Repository Engineering Rules
- Language: C++20, all code inside namespace `KDC`.
- Header guards: use `#pragma once`.
- Logging: use `LOG_INFO/LOG_DEBUG/LOG_WARN/LOG_ERROR`, never `std::cout`.
- Includes: use includes relative to `src/` conventions from `AGENTS.md`.
- Keep platform-specific code split into `*_mac.mm`, `*_win.cpp`, `*_linux.cpp` when needed.
- Respect existing architecture boundaries (GUI in `src/gui`, daemon in `src/server`, sync core in `src/libsyncengine`).

## Code Suggestion Rules for Copilot
- Prefer minimal, safe diffs that fit existing style and naming.
- Do not introduce new frameworks/patterns without clear repo precedent.
- For non-trivial logic changes, suggest or add corresponding tests in `test/`.
- Do not invent secrets, tokens, URLs, or environment values.
