---
name: kdrive-sentry-rca
description: "Investigate kDrive crashes, Sentry issues, and application logs to produce source-grounded root cause analyses. Use when asked to diagnose a crash, analyze a Sentry issue or URL, correlate client/UI and server failures, inspect support logs, rank crash causes, or explain why a kDrive process failed."
---
# kDrive Sentry Root Cause Analysis

Use the self-hosted Sentry at `https://sentry-desktop.infomaniak.com` together with repository source and any supplied logs or support archive. Never use Sentry SaaS for kDrive incidents.

The objective is an evidence-based causal explanation, not a paraphrase of the top stack frame. Trace the failure backward from the crash through breadcrumbs/logs and source until identifying the violated invariant, invalid state, ownership/lifetime error, or external condition that made the crash possible.

Read `references/sentry-projects.md` to route each process to the right Sentry project. Read `references/logs-and-correlation.md` when logs, multiple processes, support archives, or cross-project correlation are involved.

## Investigation Rules

- Start read-only. Do not resolve, assign, comment on, or otherwise mutate a Sentry issue unless the user explicitly asks.
- Treat issue frequency, user count, first/last seen, release, environment, OS, architecture, distribution channel, and regression state as evidence.
- Inspect a representative event, not only aggregate issue metadata. Prefer a recent fully symbolicated event in the affected release/OS. Compare more than one event when stacks or contexts vary.
- Use exact Sentry URLs returned by tools. Do not invent issue IDs, organization slugs, project links, or event links.
- Never expose access tokens, DSNs, emails, user names, IP addresses, geolocation, device names, customer filenames, user-specific local paths, request/response bodies containing personal data, or raw user/app/drive/sync/request identifiers. Use identifiers internally for matching, but report only that the values matched or show a minimally masked suffix when essential. Redact log excerpts to the minimum needed evidence.
- Do not claim that two events belong to one incident based on time alone. Require additional matching evidence such as release, OS, a value shared from the same identifier domain, sync ID, IPC request ID, backend request ID, or an identical causal sequence. Do not compare fields merely because both are named `user.id`.
- Do not equate the crash site with the root cause. An abort, allocator failure, `pthread_kill`, `RtlReportFatalFailure`, or destructor frame is usually a terminal symptom.
- Distinguish a product defect from expected external failures such as disk removal, permission changes, network loss, backend errors, or user termination. Explain why handling of that condition is defective if it leads to a crash.
- Account for Sentry rate limiting and sampling. Event totals are not guaranteed to equal real-world occurrence counts.
- If symbols, logs, source for the release, or correlation identifiers are missing, report that limitation explicitly and reduce confidence.

## Workflow

### 1. Frame The Incident

Extract or ask for only information that materially narrows the search:

- Sentry issue/event URL or issue ID, or the observable symptom.
- Approximate timestamp and timezone.
- UI/client generation and OS when known.
- App release/build and whether the channel is production, beta, internal, or legacy.
- Scope requested: one occurrence, one issue group, a release regression, or highest-impact crashes.
- Available logs/support archive and whether customer data may be inspected.

If the user provides a Sentry URL, fetch that exact resource first. Otherwise discover the organization/project and search the narrowest reasonable period. For broad ranking requests, default to unresolved crash-like issues in the last 30 days. Search crash/exception mechanisms as well as `level:fatal`, because SDK crashes are not consistently labeled fatal.

Prefer explicit Sentry query syntax. If the local MCP reports that AI-powered search is unavailable, continue with direct filters and aggregate fields rather than treating it as a connectivity failure.

### 2. Identify Process And Project

Classify the failing process before searching source:

- Sync, networking, filesystem propagation, VFS, daemon startup, or IPC server failures normally belong to `kdrive-server`.
- Legacy Qt Widgets UI failures belong to `kdrive-client`.
- WinUI failures belong to `kdrive-win-client`.
- macOS Swift/SwiftUI failures can belong to either `kdrive-macos-client` or `kdrive-client4`; both contain active GUI4 data during project migration. Use an exact event/project or search both and compare release metadata rather than assuming one is current.
- Linux redesign failures belong to `kdrive-linux-client`.

Query the server project as well as the relevant UI project when the symptom crosses IPC, the UI loses its server connection, the server exits/restarts, or either side contains matching timestamps/identifiers. A UI error can be fallout from a server crash; a server error can be triggered by malformed or badly ordered UI requests.

### 3. Build A Timeline

Record timestamps in chronological order and normalize timezone before correlating sources. Include:

- Last successful operation.
- First warning/error or state transition.
- Retries, cancellation, shutdown, disconnect, update, sleep/wake, drive removal, or network changes.
- Fatal event and process restart/recovery.

Search Sentry breadcrumbs and supplied logs around the event. Use the correlation hierarchy in `references/logs-and-correlation.md`. State which identifiers matched and which did not exist.

### 4. Inspect The Event Deeply

Capture evidence from the Sentry event:

- Exception/signal/assertion and mechanism.
- In-app stack from the youngest relevant frame through callers.
- Crashed thread plus other threads implicated in a deadlock, shutdown, or ownership issue.
- Breadcrumbs immediately preceding failure.
- Release, commit if encoded in release, environment, OS/architecture, relevant identity fields, distribution channel, and tags/contexts. Use identity values only for internal matching and redact them from the report.
- Symbolication quality, missing debug files, suspect grouping, and whether several mechanisms are grouped together.

For broad issues, compare representative events across major OS/release variants before proposing one cause.

### 5. Trace Into Source

Search exact function names, assertion text, log messages, enum values, and error strings. Read enough surrounding implementation and callers to reconstruct state and ownership.

Follow the relevant path:

- GUI request -> IPC transport -> server GUI job -> `SyncPal` public API.
- Update detection -> reconciliation -> propagation -> local/remote operation job.
- Network job -> retry/error mapping -> caller state transition.
- Shutdown/cancellation -> worker/thread owner -> destructor/join/stop ordering.
- VFS callback -> server bridge -> sync operation.

Read the nearest `AGENTS.md` before relying on component behavior. Use `git log`, `git show`, and `git blame` when a release or regression boundary matters. Do not check out, reset, or modify branches. If the event release includes a commit SHA, inspect that commit. Otherwise compare the release against current source and label any mismatch.

Look for tests that encode the intended invariant. A missing test is supporting evidence, not proof of the bug.

### 6. Test Competing Hypotheses

Maintain at least one alternative explanation until evidence rules it out. For every hypothesis, record:

- Evidence for it.
- Evidence against it.
- What observation would confirm or falsify it.

Use these confidence levels:

- **Confirmed:** direct event/log evidence and source path establish the causal chain, ideally reproduced or covered by a failing test.
- **High:** multiple independent signals support the cause and no material evidence conflicts, but no reproduction exists.
- **Medium:** source makes the cause plausible and some runtime evidence matches, but a key transition or identifier is missing.
- **Low:** primarily inferred from the terminal stack or timing; substantial alternatives remain.

Never write “root cause” as fact below Confirmed confidence. Use “probable cause” or “leading hypothesis.”

### 7. Report Explicitly

Lead with the conclusion and use this structure:

```markdown
## Conclusion
**Probable cause:** One precise causal statement.
**Confidence:** High, Medium, or Low. Use Confirmed only with direct proof.
**Impact:** Affected process, releases/platforms, event count, user count, and time window when available.

## Evidence
1. Runtime evidence with timestamp and Sentry link.
2. Correlated UI/server event or redacted log evidence.
3. Source evidence with `path:line` references.

## Failure Sequence
1. Chronological initiating condition.
2. Invalid transition or mishandled state.
3. Crash mechanism and visible symptom.

## Related Sentry
- Server: [issue/event title](exact URL), `Searched <project/window> and found no correlated server issue`, or `Not searched because no server symptom indicated correlation`.
- UI/client: [issue/event title](exact URL), `Searched <projects/window> and found no correlated UI issue`, or `Not searched because no client symptom indicated correlation`.
- Search/dashboard: [query description](exact URL), when returned by Sentry.

## Alternatives
- Alternative explanation and why it is less likely or still open.

## Recommended Fix
- Smallest code-level correction and target source area.
- Regression test that reproduces the causal sequence.
- Telemetry improvement if missing data prevented confirmation.

## Unknowns
- Missing logs, symbols, event fields, release source, or reproduction steps.
```

Include links for both server and UI/client Sentry whenever each has a genuinely correlated event or issue. Do not add an unrelated project homepage just to fill the section. If only one side exists, say so explicitly.

For ranking requests, state the ranking metric. If the user says only “biggest,” rank by affected users first and event volume second; include recency and regression state as context. Label uninvestigated rows as **crash issue groups** or **terminal signatures**, not root causes. Provide a compact impact table with exact issue links, then perform a full RCA only for the top issue unless the user asks for every row. If the user explicitly asks to rank **causes**, investigate every reported row sufficiently to establish a cause or group issues by an evidenced common cause; otherwise ask to narrow the number of issues.

## Completion Standard

An RCA is complete only when it:

- Names the affected process and Sentry project.
- Separates initiating condition, product defect, and terminal crash mechanism.
- Cites exact runtime and source evidence.
- Provides server and UI/client links when correlated and explicitly distinguishes no match from a project that was not searched.
- Assigns confidence and lists unresolved alternatives.
- Proposes a regression test and the smallest plausible fix area.
- Avoids exposing customer data or credentials.
