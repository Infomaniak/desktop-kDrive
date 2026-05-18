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

## Pull Request Review Rules
- When reviewing a PR, inspect SonarCloud findings for the PR if a SonarCloud MCP/tool is available.
- Treat new SonarCloud issues as review-relevant context, especially correctness, reliability, security, and maintainability issues.
- Never guess, infer, fabricate, or reconstruct SonarCloud findings from code alone. Only mention a SonarCloud issue when it was read from SonarCloud/tool output.
- If SonarCloud MCP/tool access is unavailable or returns no data during review, include the note: "SonarCloud findings were unavailable during this review." Then continue with normal code review.
- SonarCloud findings can be stale during a PR review. Use them as hints only, verify against the current diff, and be cautious about their freshness.
- Do not duplicate SonarCloud findings verbatim unless they identify a concrete actionable problem in the changed code.

## Pull Request & Commit Rules (MANDATORY)
When the user asks to create a PR or prepare PR content:
1. Use English only for PR title and PR description.
2. Create the PR as Draft by default.
3. Use `develop` as the default target branch unless the user explicitly asks for a different base branch.

When proposing commit messages:
1. Use English only.
2. Use Conventional Commits format: `<type>(<scope>): <subject>`.
3. Allowed types: `feat`, `fix`, `refactor`, `perf`, `test`, `docs`, `chore`, `ci`.
