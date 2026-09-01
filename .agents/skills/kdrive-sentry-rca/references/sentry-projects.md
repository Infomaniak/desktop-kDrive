# kDrive Sentry Project Routing

## Instance

- Base URL: `https://sentry-desktop.infomaniak.com`
- Current organization slug discovered through Sentry MCP: `sentry`
- Discover the organization again if tools report that this slug is unavailable; do not fall back to Sentry SaaS.

## Projects

| Project | Application/process | Source area | Notes |
|---|---|---|---|
| `kdrive-server` | C++ background server/daemon (`kDrive`, `kDrive.exe`) | `src/server/`, `src/libsyncengine/`, server-side `src/libcommonserver/` | Owns sync state, API/network jobs, filesystem propagation, VFS integration, and server side of IPC. One project covers macOS, Windows, and Linux. |
| `kdrive-client` | Legacy Qt Widgets GUI (`kDrive_client`) | `src/gui/`, `src/libcommongui/` | Native C++ events. Use OS and release metadata to distinguish platforms. |
| `kdrive-win-client` | Windows WinUI3 GUI4 (`client/kDrive.exe`) | `src/gui4/windows/` | Sentry platform is normally `csharp`; releases resemble `kDrive@<version>+<commit>`. |
| `kdrive-macos-client` | macOS Swift/SwiftUI GUI4 (`kDrive.gui`) | `src/gui4/macOS/` | Active Cocoa data exists here. Releases resemble `com.infomaniak.drive.desktopclient.gui@<version>+<build>`. |
| `kdrive-client4` | macOS Swift/SwiftUI GUI4 during Sentry project migration | `src/gui4/macOS/` | Active and newer Cocoa releases also exist here. Search both macOS projects unless an exact issue/event identifies one. Do not assume duplicate issues correlate without matching metadata. |
| `kdrive-linux-client` | Linux redesigned client | Branch/version dependent | Native events. Confirm source availability in the current checkout before citing implementation. |
| `kdrive-tests` | Automated tests | `test/` | CI/test failures, not production customer crashes. |
| `internal` | Internal Sentry project | Unknown/miscellaneous | Do not use for product RCA unless an exact event points to it. |

## Cross-Project Search

Search both server and client projects when:

- The client reports lost IPC/XPC/TCP connection, timeout, stale data, or server restart.
- A UI action immediately precedes a server assertion/crash.
- Shutdown, update, login/logout, sleep/wake, drive removal, or LiteSync/VFS behavior spans processes.
- Timestamps plus release/OS and at least one stronger identifier align.

Search only the server project for incidents wholly inside sync detection, reconciliation, propagation, filesystem jobs, backend API handling, or daemon startup with no client symptom.

## Useful Filters

- Production incidents: `environment:production` where the project supplies environment metadata.
- Crashes: start with `level:fatal`, but inspect exception mechanisms because SDKs may report crashes at other levels.
- Native server channel: use `distribution_channel` when present.
- Native installation: use `appUUID` when present.
- Release regression: compare first seen, release distribution, event volume, and affected users; do not infer regression from first seen alone.
- “Biggest” without another metric: rank affected users first, then event volume. Always show both because throttling and repeated crashes can make either metric misleading alone.

## Link Policy

- Prefer exact issue/event URLs returned by Sentry MCP.
- Include the exact Sentry search/dashboard URL when a query tool returns one.
- Never build a URL from a guessed numeric project ID.
- If a UI and server issue are correlated, link both and state the evidence connecting them.
