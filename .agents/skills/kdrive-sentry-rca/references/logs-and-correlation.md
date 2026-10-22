# kDrive Logs And Correlation

## Log Sources

### C++ Server

- File: `yyyyMMdd_HHmm_kDrive.log`, with compressed rotations such as `.1.gz` through `.4.gz`.
- Format: local wall-clock timestamp with milliseconds, level, thread, source file/line, and message.
- Locations:
  - macOS: `~/Library/Logs/kDrive/`
  - Windows: normally `%LOCALAPPDATA%\Temp\kDrive-logdir\`
  - Linux: `$XDG_STATE_HOME/kDrive/logs/` or `~/.local/state/kDrive/logs/`
- Logger implementation: `src/libcommonserver/log/`.

### Legacy Qt Client

- File: `yyyyMMdd_HHmm_kDrive_client.log.N`; archived files may be gzip-compressed.
- Same platform directory as the C++ server.
- Format includes local timestamp with milliseconds, Qt severity, thread, and source file/line.
- Logger implementation: `src/libcommongui/logger.cpp`.

### Windows GUI4

- File base: `yyyyMMdd_HHmm_kDriveClient.log` with Serilog rolling suffixes.
- Location: `%LOCALAPPDATA%\temp\kDrive-logdir\`.
- Logger implementation: `src/gui4/windows/kDrive client/kDrive client/Logger/Logger.cs`.

### macOS GUI4 And Extensions

- GUI4 primarily uses Apple unified logging with subsystem `com.infomaniak.drive.desktopclient.gui` and categories `view`, `XPC`, `data`, `general`, and `debug`.
- Finder/LiteSync/login-agent and some XPC logs use messages prefixed with `[KD]`.
- There is no ordinary macOS GUI4 text log file in the current source.
- A support archive includes up to 24 hours of `[KD]` unified logs in `kdrive_extension_logs.txt`, but does not capture every GUI4 OSLog message.

### Support Archive

The server-generated ZIP can contain application logs, `.parms.db`, `user_description.txt`, and macOS `[KD]` unified logs. It is uploaded to the Infomaniak support backend, not attached to Sentry. There is no automatic Sentry-event-to-support-archive identifier.

Treat support archives as sensitive: they can contain emails, user/drive IDs, local paths, filenames, request bodies, and configuration. Never reproduce unnecessary customer data in an RCA.

## Correlation Hierarchy

Use the strongest available combination, in this order:

1. Exact event ID or an identifier explicitly shared by both records from the same identifier domain.
2. Backend `X-Request-ID` UUID from network-job logs.
3. Sync marker `*<syncDbId>*` plus matching operation/node/path context.
4. GUI IPC request/reply ID within the same client/server process lifetime.
5. A stable identity known to have the same semantics on both records, such as the same backend user/drive ID emitted by application code.
6. Same release/build, OS/architecture, distribution channel, and narrowly matching timestamp.
7. Timestamp alone, which is only a lead and not sufficient proof.

There is no universal cross-process session or trace ID. IPC IDs reset on process restart, and server job IDs are not the same as GUI request IDs.

Identity semantics differ by project:

- Native server `appUUID` identifies a server installation. GUI4 projects do not currently emit that tag, so it cannot correlate server and GUI4 events by itself.
- Native server and legacy client can use backend user IDs supplied by application code.
- WinUI and macOS SDK `user.id` values may be SDK-generated installation identifiers rather than backend user IDs.
- Accept identity correlation only when source/event context proves that both values come from the same domain. Otherwise record the fields as non-comparable.
- Keep raw identifiers out of the final report. State that a value matched, or use a minimally masked suffix only when needed to distinguish multiple timelines.

## Correlation Markers

- Sync-engine messages commonly begin with `*<syncDbId>*`.
- C++ network jobs log a process-local `jobId` and send/log an `X-Request-ID` UUID. Prefer `X-Request-ID` for backend correlation.
- Legacy Qt IPC commonly logs `Snd rqst <id> <num>` and `Rpl rcvd <id>`.
- Windows GUI4 IPC logs outgoing message type/request ID and reply ID.
- macOS GUI4/XPC DTOs and server XPC messages carry request/callback/signal IDs.
- Server startup logs version, OS, locale, app ID, users, drives, log level, and extended-log state.

## Timeline Procedure

1. Determine timezone for each source. File logs usually use local time; Sentry commonly presents UTC or viewer-local time.
2. Select a narrow window around the crash, normally 30-120 seconds, then expand only if initialization/retry behavior requires it.
3. Search backward from the fatal line to the first anomalous state transition, warning, failed operation, cancellation, or external change.
4. Search the companion process log for matching correlation markers.
5. Separate repeated retries/rate-limit escalations from the first causal failure.
6. Cite short redacted excerpts. Preserve timestamps, levels, source locations, and only masked correlation-ID suffixes needed to support the sequence.

## Sentry Caveats

- Release C++ `LOG_*` calls are breadcrumbs; `LOG_FATAL` also emits an event. Debug builds do not add these breadcrumbs.
- Windows GUI4 log calls become breadcrumbs, and Warning/Error/Fatal calls may become throttled events.
- Current macOS GUI4 OSLog messages are not automatically mirrored to Sentry breadcrumbs.
- Sentry does not contain complete text log files or support archives.
- Native event capture is rate-limited, and performance traces are sampled. Event counts understate or reshape actual occurrence rates.
- Missing symbols can make allocator, abort, or system frames look like the cause. Check debug-file status before concluding.
- The source checked out locally may not match the event release. Prefer a commit SHA encoded in release metadata; otherwise disclose the mismatch.
