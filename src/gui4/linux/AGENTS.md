# gui4/linux: Linux v4 Qt/QML Frontend

Linux-only v4 client (Qt 6.12.0, QML + C++). It handles the UI and the server communication; sync logic stays in
`src/libsyncengine/` and the server.

This file holds what the code does not say: user preferences, invariants and Qt pitfalls. Feature behavior lives in the
code. When the user corrects a working rule, add it to the matching section in one or two lines.

## Working Rules

- Prepare fixes and PRs in the current checkout; create another worktree only when explicitly requested.
- Share diffs as a patch file or an editor diff; show a full diff in chat only on request.
- Explain failure conditions with concrete prerequisites, not vague labels such as "normally configured".
- Build only on request: `kdrive_qml` for model and UI checks, `kDrive` too when the server side is involved (see Build).
- Keep tests of this frontend local: no committed tests and no test target under `test/gui4/linux/`.
- Qt 6.12.0 is the reference: check the configured dependency before attributing a behavior to an older Qt.
- The GUI and the server share one lifecycle: the server is never replaced while the GUI runs, so frontend caches need
  no invalidation for a server upgrade.
- Native Wayland is the default runtime and XCB/XWayland a compatibility path; window-shell changes cover both.
- After a rebase that removes or renames a shared model or header, grep this frontend for stale includes and type names.
- Link only to versioned `.md` files.

## Translations

- Reuse an existing Loco id with an equivalent label before creating Linux-specific wording or a new key.
- Change an existing Loco translation only when the user asks for that exact wording. Creating a key or auditing a
  catalog does not extend to harmonizing other locales.
- Run complete Loco imports only, never `import_loco -k`, then remove from the catalog diff the keys that do not belong
  to the current change.
- German translations use Swiss orthography: `ss` instead of `ß`.
- Translate in QML with `qsTrId`: C++ exposes states, enums or translation ids, so the QML engine retranslates on a
  language change. C++ translates only text QML cannot own (system tray menu); it then recomputes its cached strings
  before notifying, and translated properties are never `CONSTANT`.

## C++ Conventions

- Readability: blank lines between definitions and logical steps, explicit names, braces on every conditional body, one
  operation per line. Keep readability cleanups separate from architectural simplification.
- String literals: `u"..."_s` with `Qt::StringLiterals` in new code. In a file or block that mostly uses
  `QStringLiteral`, keep `QStringLiteral`, say so in the PR and offer a small card to migrate the file.
- Document with `/** */`: public contracts in the header, private helpers in the `.cpp`, each method in one place only.
- Use the `libcommon/utility/types.h` aliases for their concepts; keep `int` or Qt numeric types only where a Qt override
  or the QML boundary requires them.
- Log with `QLoggingCategory` and `qC*`. Declare file-local categories in an anonymous namespace, in the touched files
  only.
- Iterate the keys or values of an associative container with `std::views::keys` / `std::views::values`.
- App-level non-translatable constants go in the header-only `app/appconstants.h`.
- Guard async callbacks with `QPointer` when the receiver's lifetime is uncertain.

## Architecture Invariants

- Layers: `communicationlayer/` is transport and protocol, `app/services/` the typed backend facade, `ui/` presentation.
  QML never reaches `IpcClient`. A request is added to `CommService` and parsed into typed values there; a server push
  is registered in `CommService::register*Handlers` and re-emitted as a typed Qt signal. Use the `libcommon` `msgParam*`
  keys and enums.
- `AppCache` mirrors the server. `CachePopulator` owns the full snapshots (bootstrap, reconnection, parent-first
  reconciliation after a failed non-transactional mutation). `CachePipeline` is the only bridge from pushes to
  `AppCache` and `ActivityStore`, and drops pushes received before the first snapshot. Services expose targeted actions
  only: cache consistency is push-driven. Cache stores are mutated on the main thread.
- Per-sync runtime status and progress live only in `AppCache`; consumers observe it and keep no copy. The main
  selection lives in `MainSelectionStore`.
- Identify a configured drive by its `DriveDbId`: the backend `DriveId` is shared between users.
- `SyncService::addDriveSync` is the only sender of `SYNC_ADD`: its reservation keeps two windows from creating two
  classic syncs for one drive.
- `ParametersStore` holds the last server-confirmed parameters only. `ParametersService` publishes after
  `PARAMETERS_UPDATE` succeeds and skips unchanged snapshots; drafts belong to the owning controller, and consumers
  compare their own values before reapplying side effects.
- Services report UI-facing failures on the shared `ServiceEventBus` and pending state in `ServiceActionTracker`.
- Updater state comes from `UpdateStatusService` only. Sentry captures go through `SentryService` and tolerate an
  uninitialized SDK (consent off).
- The server does not announce updates on Linux: `UpdateNotifier` is the only source of update notifications.
- `app/desktop/` calls D-Bus asynchronously without checking first that the service is registered: a service started
  on demand only appears once called. A missing service falls back to the previous behavior when there is one, and its
  failure is logged once.
- `AppClientLinux` is a composition root: multi-step workflows go in coordinators. `AppRouter` holds navigation only and
  reads neither the cache nor the services.
- Only `OnboardingSessionManager` is process-long: session objects stay off the root QML context, and a finished session
  is unpublished, then destroyed with `deleteLater()`.
- QML receives app-owned controllers as initial properties, then explicit required properties. Expose a process-long
  service directly when a controller would only forward it; add a controller for view state or orchestration. Expose
  only the model roles current QML consumes.
- A one-off transactional dialog gets a controller created per session by its page's controller, passed as an initial
  property and released after the closing transition; late responses then reach a dead `QPointer`.
- App-global modals live in `GlobalModalHost`; contextual modals instantiate `IKModal` in their own window.
- Register C++ types for QML in `AppClientLinux::setupQmlEngine`: Qt skips `QML_ELEMENT` / `QML_SINGLETON` once
  `kDrive.UI` holds manual registrations. The exception is `UserActionLog`, provided before any manual registration.
- Design storage and presentation contracts for the final feature: an inactive UI keeps the model state it will need.

## QML Conventions And Pitfalls

- Name layout values in `ui/tokens/`. Raw colors belong to T1 primitives; T3 tokens reference T1 or T2. Format token
  files without `qmlformat --normalize`, which reorders members across the T1/T2/T3 sections.
- Text from the server or the filesystem (names, paths) uses `textFormat: Text.PlainText`.
- Tooltips use `IKToolTip`, which owns `visible`: buttons set `targetButton`, other items `showRequested`, never
  `activeFocus`.
- Shortcuts are `Shortcut` items with `context: Qt.WindowShortcut` in the window root. Prefer a `StandardKey` annotated
  with its usual key, explicit sequences only where desktops bind nothing (`"Ctrl+,"`), and `"Ctrl+Shift+Tab"` rather
  than `StandardKey.PreviousChild` (Backtab never matches on Wayland). Give a key press one matching sequence: two make
  the shortcut ambiguous and it never fires. Tooltips show it through `Shortcut.nativeText`.
- `TableView` / `TreeView` model roles avoid delegate property names (`selected`, `editing`, `current`, `row`,
  `column`, `expanded`, ...): the view sets those from its own state.
- Tint icons with `IKTintedIcon` and white source fills. Pad an SVG `viewBox` whose paths touch its bounds: Qt SVG clips
  their antialiasing even with `overflow="visible"`.
- Wrap an icon used as a `Control.contentItem` before sizing it: the control owns its content item's geometry.
- `Rectangle.clip` is rectangular: mask a rounded composite with one shape.
- In-app modals take their window's `effectiveShadowMargin` and `surfaceRadius` as `scrimInset` and `scrimRadius`;
  otherwise the scrim dims the transparent shadow margin.
- On a card surface, an `IKModalButton` uses the `Tonal` role: the `Secondary` outline is too close to the card.
- Every labelled `IKLinkButton` or `IKModalButton` that opens the browser sets `external: true`; icon-only controls
  name the destination in their tooltip.
- Size a fixed-width column from the widest localized strings the model exposes and from its header label
  (`maxTextWidth`, `TextMetrics`).
- Generated Lottie animations: read [`ui/LOTTIE_ANIMATIONS.md`](ui/LOTTIE_ANIMATIONS.md) before generating, regenerating
  or fixing one.

## User Action Log

- Log every QML interaction through `UserActionLog` (`action`, `screen`, `dialog`, `window`).
- Ids are lowerCamel segments `<surface>.<zone>.<control>`, with surfaces `main`, `dialog`, `onboarding`, `syncConfig`,
  `settings`, `window`, `shortcut`, `tray` and `notification`. A prefix used more than once in a file is written once:
  `screenId`, `dialogId`, or a root `readonly property string actionIdPrefix`.
- Shared interactive components require their id (`actionId`, `dialogId`, `windowId`, `actionIdPrefix` for trees) and
  log by themselves; an ad hoc control calls `UserActionLog.action(...)` first in its handler. qmllint misses instances
  in an aliased default property (`SettingsRow`): check those by hand.
- Log a screen while it and its window are visible, again on each new showing. Keyboard and assistive activations go
  through the logged path, before the effect.
- Details carry technical ids, booleans, enum names and row indexes only: no displayed or typed text, paths, patterns
  or proxy settings.
- A new Matomo event or page view gets an `AnalyticsCatalog` entry from the kDrive v4 taxonomy.

## Debug Switches

- `KDRIVE_SIMULATE_ERRORS=<scenario>` (Debug only, `app/devtools/errorsimulator.*`) shows fake sync errors. Give every
  new `SyncErrorKind` a fixture.
- `KDRIVE_FORCE_NO_TRAY=1` (Debug only) keeps the tray disabled to test the window fallback.
- `KDRIVE_TRAY_ICONS=white|black` forces the tray icon set.

## Build

From the repository root; `kdrive_qml` is written next to the `kDrive` server in `${BIN_OUTPUT_DIRECTORY}`:

```bash
./infomaniak-build-tools/conan/build_dependencies.sh Debug
cmake --preset conan-debug -S . -B build-linux/build/build/Debug
cmake --build build-linux/build/build/Debug --target kDrive kdrive_qml -- -j 8
```
