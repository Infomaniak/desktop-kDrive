# gui4/linux — Linux v4 Qt/QML Frontend

## Maintenance Cadence

- This area changes frequently and often receives large feature additions.
- Update this file very regularly; it must stay aligned with the current architecture and workflows.
- Any significant change under `src/gui4/linux/` (new layer, new pattern, new build/run command) must include an
  `AGENTS.md` update in the same PR.

## User Preferences & Auto-Correction

> New norm: if the user corrects a working rule, add it here immediately to avoid repeating the same mistake.

### Local Norms (Linux v4)

- In versioned documentation, use repo-relative paths, not hardcoded absolute paths.
- Do not add links to `.md` files that are not versioned in git.
- Use Swiss German orthography for German Linux v4 translations: write `ss` instead of `ß`.
- Never launch a build unless explicitly asked by the user.
- For the Activities PR stack, preparing a PR means isolating and staging its changes only. Leave commit, push, and PR
  creation to the user unless they explicitly ask Codex to publish them.
- Treat native Wayland as the default Linux runtime on current Ubuntu/GNOME systems. XCB/XWayland is a compatibility
  path, not the primary platform; window-shell changes must cover both paths explicitly.
- Keep the Linux frameless header and custom shadow on native Wayland without depending on `Qt6::GuiPrivate`. Accept
  that Wayland snapping includes the transparent shadow margin, so the visible surface may not touch the screen edge.
- On a Linux host, validate natively: run `./infomaniak-build-tools/conan/build_dependencies.sh Debug`, configure with
  the generated Conan/CMake Debug preset, then build `kDrive`, `kDrive_client`, and `kdrive_qml`. Do not use the Podman
  release script for this local Linux validation path.
- Prefer documenting private implementation helpers in `.cpp` rather than headers.
- Do not duplicate method documentation between headers and implementation files. Document public API contracts in
  headers, private helpers in `.cpp` files, and keep implementation-specific comments next to the relevant code.
- Do not introduce raw `int` in new code when a fixed-width type fits (`uint8_t`, `int32_t`, ...).
- In new Linux v4 C++ code, import `Qt::StringLiterals` in the implementation and use `u"..."_s` instead of
  `QStringLiteral(...)`.
- Use the domain aliases from `libcommon/utility/types.h` whenever they match the represented concept. Keep `int` and Qt
  numeric types when required by an overridden Qt API or a QML boundary, and make that constraint explicit when unclear.
- Do not run `clang-format` on `CMakeLists.txt` in this repository.
- For shared infrastructure classes, document the class role explicitly in the header comment when relevant.
- Keep `ParametersStore` as a server-confirmed parameters snapshot only. Do not add global draft/pending state there;
  screen-specific drafts, such as proxy edition, belong to the owning UI/view model.
- Keep per-sync runtime status and progress exclusively in `AppCache`. Consumers such as the system tray and future UI
  adapters must observe and query that shared state instead of maintaining private copies.
- Design feature storage and presentation contracts for their intended final lifecycle. A temporarily unavailable UI or
  action may remain inactive, but must not make the underlying model discard state needed by the completed feature.
- For the bounded Activities projection, prefer the score-based linear error matcher over rebuilding temporary identity
  indexes unless profiling demonstrates that matching is a bottleneck.
- In range-for loops over associative containers, prefer `std::views::keys` / `std::views::values` over structured
  bindings with an unused `_` element when only keys or only values are needed.
- For Linux v4 model/UI checks, build only the `kdrive_qml` target unless a broader backend/server validation is
  explicitly needed.
- After resolving a rebase conflict that removes or renames a shared model/header, grep Linux v4 for stale includes and
  old type names, then validate at least the `kdrive_qml` target.
- For tray fallback testing, `KDRIVE_FORCE_NO_TRAY=1` is Debug-only and forces the startup tray probe to stay disabled.
- Avoid magic layout values in QML; put reusable or semantic dimensions and ratios in `ui/tokens/` with explicit names.
- Give an in-app modal its hosting window's `effectiveShadowMargin` and `surfaceRadius` as `scrimInset`/`scrimRadius`.
  The overlay spans the complete native window, so an un-inset scrim also dims the transparent custom-shadow margin.
- Do not place a `Secondary` `IKModalButton` on a card surface: its outline is too close to that surface in both
  themes. Use the `Tonal` role there, and keep `Secondary` for a plain modal or page background.
- Size Home Quick Access from the widest translated shortcut label or the drive name capped to the Windows-aligned
  display width. Keep shortcut labels fully visible, while the drive name wraps to two lines before eliding. Let the
  Home status panel consume the remaining horizontal space.
- Keep raw color values in T1 primitives. T3 contextual color tokens must reference T1 or T2 tokens instead of embedding
  hexadecimal or RGBA values.
- Do not run `qmlformat --normalize` on structured token files: it reorders QML members independently of the intended
  T1/T2/T3 hierarchy and detaches section comments from their tokens.
- Render the main-sidebar synchronization selector with the bare drive glyph tinted by the drive color; do not place the
  glyph on a colored tile.
- Render advanced synchronizations with the outline `ui/assets/main/folder.svg` tinted by the owning drive color;
  reserve the bare drive glyph for classic root synchronizations.
- In the main-sidebar Figma variants, "no synchronization" means no advanced synchronization. The drive row is the
  classic root synchronization and must retain its local-folder action; do not model it as a drive-only entry.
- Use white source fills for monochrome SVGs tinted through `MultiEffect.colorization`; black source fills retain too
  little luminance and remain dark when the theme color changes.
- Keep main-sidebar item states composable: selection, disabled state, notification count or dot, and a trailing
  accessory must remain independent presentation inputs rather than a screen-specific state enum.
- Route orange error dots progressively in the main sidebar: selected-sync errors appear on Activities, unselected-sync
  errors appear on the closed selector, and per-sync dots appear inside the open selector. Normal activity has no dot.
- Keep the shared error banner surface visibly distinct from the page background in both themes.
- Match the Figma main-toolbar Pause/Resume and Settings group: one subtly outlined 68 x 36 capsule with 4 px padding
  and spacing, containing two independent 28 x 28 circular hover surfaces.
- Use the same resting surface for the main-toolbar Support and Pause/Settings controls, and the same stronger hover
  surface for all three buttons.
- Render the future main-toolbar Search action as a standalone 36 px circular icon-only button, without a text label,
  and center its 16 px magnifier with 10 px between the SVG and each horizontal edge. Reuse the Support button component
  so both outer circles remain identical.
- Use `IKToolTip` for every Linux v4 tooltip so controls share the rounded, theme-aware drive-name tooltip presentation;
  do not use Qt's attached `ToolTip` styling, which falls back to the native yellow tooltip on some desktops.
- For Activities status presentation, mirror the Windows fallback: `Unknown`, `Error`, `Conflict`, `Inconsistency`, and
  `Ignored` are all visible error activities; only `Success` and `Syncing` use non-error presentations.
- Keep Activities geometry in `IKActivities` and Activities-specific colors in the T3 section of `IKColors`. Store exact
  exported Figma assets under `ui/assets/main/activities/`; never reference temporary Figma URLs from QML.
- Preserve the visual aspect ratio of Activities menu icons and center differently sized glyphs in a fixed slot. Add a
  small safety area to SVG view boxes whose paths touch or exceed their bounds, since Qt SVG clips that antialiasing
  even when the asset declares `overflow="visible"`.
- Rasterize small tintable SVG sources at no less than 3x their logical size before applying `MultiEffect`, while
  constraining only one `sourceSize` dimension so Qt retains the SVG aspect ratio. Do not enable mipmapping for these
  icons. Wrap icons used as a `Control.contentItem` before assigning their intended size because the control owns the
  direct content item's geometry.
- Close an Activities row action menu when its delegate moves vertically. A popup must not follow a row displaced by
  incoming activities, sorting, or filtering because it can become detached from the viewport and obscure another row.
- Keep an Activities row action menu entirely inside the visible list viewport. Prefer opening it below its row, flip it
  above when there is insufficient room, and clamp the result when neither side provides its full height.
- On Activities, a retry in progress for an actively errored node shares the same projected row, and displayed Folder
  values open that exact folder even when the activity target no longer exists.
- On Activities, keep actual in-progress transfers above failed and synchronized rows; surface active errors separately
  without displacing transfers that are still running.
- Expose Activities row capabilities through one `availableActions` flags role. Keep target ids and paths internal, and
  revalidate every action in `ActivitiesController` or `ActivityService` instead of duplicating guards in QML.
- Present asynchronous share-link progress in the persistent bottom area of the main sidebar. Keep the notification
  component generic, non-modal, and independent from the activity row lifetime.
- Automated tests for the current Activities milestone are deferred. Do not add an Activities-specific test target or
  files under `test/gui4/linux/` until the user explicitly reopens that scope.
- Keep Linux Storage usage local to the GUI: use `QStorageInfo`/Qt filesystem tools where they expose the required
  semantics, supplement them with POSIX metadata only for device identity and allocated blocks, never descend onto a
  different `st_dev`, and count sparse files by their physically allocated blocks rather than logical size.
- Disable the Activities and Storage sidebar entries when no selected synchronization root exists; the unconfigured
  route remains on Home.
- Let Storage inherit the shared main-page surface, and use the same card surface as Home Quick Access. Keep the 12 px
  clipped card radius, 24 px horizontal page margins, and 32 px top margin.
- Mask the complete Storage graph with one rounded shape instead of rounding individual segments: `Rectangle.clip` is
  rectangular in Qt Quick, and segment-level rounding breaks when the leading segment is zero or subpixel-sized.
- Keep the synchronization and other-files Storage segments visible with a nominal 1% minimum, but let free space reach
  a true zero width. Normalize the displayed widths inside the single rounded graph mask.
- Main-sidebar selection changes only the row background; it must not recolor the icon or increase the label weight.
- Keep shared color primitives aligned with the macOS design-token assets; notably, `NeutralBlue200` is `#DCE3F0` and
  `NeutralBlue600` is `#1F242E`.
- Store Linux v4 app-level non-translatable constants in `app/appconstants.h`; keep it header-only while constants stay
  simple.
- Keep simple onboarding external-link actions in `OnboardingFlowController` when they do not mutate app/backend state;
  put multi-service onboarding backend effects in a dedicated onboarding coordinator.
- Keep only `OnboardingSessionManager` process-long. Onboarding state, flow, coordinators, and view models belong to an
  ephemeral `OnboardingSession` and must not be exposed as root QML context properties.
- Route tray and server main-window activation requests through `AppClientLinux::openMainWindow()`. It delegates to
  `OnboardingSessionManager` only when no sync is configured; otherwise it activates `AppRouter` and shows the main
  window shell.
- When ending an onboarding session, unpublish it so QML unloads the onboarding `Loader`, then destroy it with
  `deleteLater()`. Invalidate session-scoped asynchronous results before allowing a new session to consume them.
- Keep `AppClientLinux` as an application composition root. Move multi-step feature workflows such as onboarding login
  into dedicated coordinators instead of accumulating workflow lambdas in `AppClientLinux`.
- `Qt.labs.lottieqt` renders Lottie JSON assets, not `.lottie` zip containers.
- For Qt 6.11+ onboarding Lottie assets, prefer generated QML from `lottietoqml` over PNG spritesheets when the
  generated output builds and visually matches the source animation.
- `lottietoqml` expects the JSON animation payload; extract `animations/<id>.json` from `.lottie` containers before
  generating QML components.
- `LoaderStrokeAnimation` is intentionally used for the login loader in both light and dark themes. Do not restore or
  generate separate light/dark loader variants unless the user explicitly requests them.
- Version generated animation QML files in each window's `animations/` directory. Do not manually edit their geometry or
  timing; regenerate those from the source `.lottie` asset and keep the generated-file header.
- When Light and Dark use the same animation geometry, keep one generated component and deterministically replace its
  generated palette with theme-aware `IKColors` bindings instead of versioning separate Light and Dark QML files.
- Derive a Lottie state's theme palette only from the matching Light/Dark state assets; do not reuse colors from a
  similarly named or visually related animation state.
- Before generation, remove `h` only from animated shape-path keyframes when `lottietoqml` otherwise drops the
  corresponding `PathInterpolated`; never restore missing paths by editing generated QML.
- When a fill spans outer and inner contours, verify that `lottietoqml` preserves the holes. If it emits one filled
  `ShapePath` per contour, combine each outer contour with its reversed inner contour in the normalized JSON before
  regenerating.

## Scope

- Linux-only v4 frontend based on Qt 6.8 (QML + C++).
- This package handles UI bootstrap and server communication only.
- Sync business logic stays in `src/libsyncengine/` and server-side jobs.

## Current Structure

- `main.cpp`: process entry point, single-instance lock file, and Linux-v4 opt-in for forwarding Qt logs to Sentry
  breadcrumbs.
- `appclientlinux.*`: top-level app wiring (logging, QML warning forwarding, IPC lifecycle,
  dispatcher/service/coordinator ownership).
- `app/appconstants.h`: app-level non-translatable constants, mirroring the Windows `AppConstants` role where useful.
- `app/fileiconresolver.*`: reusable, cached `QMimeDatabase::MatchExtension` classifier mapping local file names to the
  semantic document-icon asset names consumed by QML views.
- `app/dialogs/manydeletescontroller.*`: process-long controller for mass-deletion warnings. It owns the feature FIFO,
  same-sync severity escalation, hard-warning acknowledgement, soft-warning preference mutation, and web-trash action;
  it requests main-window presentation without owning window routing.
- `app/systraycontroller.*`: Linux system tray ownership, 5-state tray icon selection derived from `AppCache` plus
  updater availability, GNOME-compatible tray menu actions, fallback-to-window startup behavior, retry loop for late
  tray availability, and main QML window show/hide behavior.
- `communicationlayer/ipcclient.*`: TLS-over-loopback JSON transport (`QSslSocket`), request/reply correlation,
  reconnect-before-first-connect logic, and pinned self-signed certificate verification. The pinned CA certificate is
  loaded once from the OS keychain via `CertReader` and set as the socket's only CA; the peer is verified against
  `kDrive-localhost`. SSL errors are logged and never ignored.
- `communicationlayer/certreader.*`: read-only accessor for the public TLS certificate stored in the OS keychain. Uses
  the shared `package` / `service` constants from `libcommon/utility/utility.h` (`keychainConstant` namespace, same
  slots as `KeyChainStorage`) and the `certKeychainKey` / `localHostName` constants from `libcommon/comm.h`; returns
  `false` (not an error) when the entry is not present yet, so the IPC client can retry during startup.
- `communicationlayer/serversignalsequencer.*`: internal `IpcClient` stage that restores the server-assigned order of
  asynchronous push signals before exposing them to semantic dispatch; buffers bounded gaps and reports persistent
  sequence violations as fatal IPC errors. It relies on the single GUI connection receiving a sequence starting at
  `firstGuiSignalId`; the server does not allocate signal ids while no GUI channel is connected.
- `communicationlayer/signaldispatcher.*`: server-push signal fanout to registered handlers.
- `app/services/commservice.*`: typed request/signal facade above `IpcClient`.
- `app/services/serviceactiontracker.*`: shared persistent state for in-flight service actions
  (`begin/end/isActionPending/isServicePending`).
- `app/services/serviceeventbus.*`: shared high-level service event hub (single UI subscription point for generic
  cross-service failures). Owned once by `AppClientLinux` and injected by reference into app services.
- `app/services/sentryservice.*`: Linux v4 Sentry coordinator. Owns cached consent reconciliation, delayed
  linux-v4-specific Sentry initialization, authenticated user binding, and UI/process capture helpers. Qt log
  breadcrumbs use the shared `Logger` bridge and remain inert whenever this service has not activated Sentry.
- `app/cache/appcache.*`: graph-backed cache (`AppCache` QObject) - owns configured users/accounts/drives/syncs, the
  single volatile runtime snapshot for each sync, split sync/server errors, per-user available drives, cascade removals,
  and derived read models. Sync snapshot replacement preserves runtime data for retained sync database ids.
- `app/cache/activitystore.*`: process-local, per-sync file-activity history. It retains server status and direction,
  updates valid operation ids in place, removes failed entries superseded by a successful or in-progress activity for
  the same node, clears interrupted in-progress entries when a synchronization becomes inactive, preserves distinct
  anonymous operations, and bounds retention to 500 entries per synchronization. It stays separate from the durable
  `AppCache`
  graph and is not exposed directly to QML.
- `app/cache/cachepipeline.*`: unique bridge for `CommService -> AppCache/ActivityStore` push signals.
    - Routes entity, sync-runtime, and file-activity pushes after population; drops and logs earlier pushes as invariant
      violations.
    - Prunes activity history when the configured synchronization set changes.
- `app/cache/cachetypes.h`: cache read models and onboarding keys (`SyncContext`, `DriveContext`,
  `SyncRuntimeInfo`, `AvailableDriveContext`, `AvailableDriveKey`, `PendingSyncConfig`).
    - Configured-drive state uses the unified `libcommon/data/drive.h` `Drive` model; do not reintroduce the removed
      `DriveInfo` type in Linux v4.
- `app/cache/mainselectionstore.*`: sync-first main-shell selection owner (`currentSyncDbId`) and selection healing. It
  prefers a classic root synchronization, then any synchronization.
    - Emits `currentContextChanged()` as a coarse invalidation signal when the selected ids stay unchanged but the
      underlying cache graph changes.
    - Exposes selected-sync runtime through its dedicated accessor and signals, including a status-only notification so
      progress ticks do not invalidate presentation that only depends on the synchronization status.
- `app/mainwindow/syncselectormodel.*`: QML adapter for the synchronization selector. It flattens each configured drive
  into classic and advanced synchronization rows and exposes only the presentation data required by the selector.
- `app/mainwindow/mainsidebarcontroller.*`: QML-facing sidebar interaction controller. It exposes `SyncSelectorModel`,
  delegates sync selection to `MainSelectionStore`, and opens the selected local sync folder through desktop services.
- `app/mainwindow/activitylistmodel.*`: selected-sync projection joining bounded recent activities with authoritative
  active node errors. It omits failed activities after their active error is resolved, maps server status and direction
  to the QML-facing presentation enums, keeps actual in-progress rows first, coalesces bursty cache invalidations, and
  keeps active errors visible even when their recent activity has been evicted.
- `app/mainwindow/activitiescontroller.*`: QML-facing Activities state and action boundary. It owns filtering and title
  presentation, including the local title-state resolver, validates local paths, opens activity and displayed-folder
  locations, and delegates asynchronous link actions to `ActivityService`. Dedicated share-link lifecycle signals keep
  transient feedback independent from projected row lifetime.
- `app/mainwindow/homecontroller.*`: cache-backed QML adapter for the modular Home and toolbar sync controls. It
  resolves the selected sync into one central presentation state, exposes user/drive/error data, owns web-link
  construction, and delegates pause/resume to `SyncService`.
- `app/mainwindow/homestateresolver.*`: pure status matrix used by `HomeController`. Structured sync errors remain an
  independent Home banner instead of replacing the central state.
- `app/mainwindow/networkstatusobserver.*`: process-long `QNetworkInformation` adapter. Only explicit disconnected
  reachability is treated as offline; unavailable or unknown backends preserve the cache-derived state.
- `app/mainwindow/storagecontroller.*`: QML-facing Storage lifecycle and process-local per-sync snapshot cache. It
  starts cancellable local scans only while Storage is visible, keeps the last resolved presentation during refresh, and
  refreshes once an active synchronization leaves `Starting`, `Running`, `PauseAsked`, or `StopAsked` for any non-active
  status.
- `app/mainwindow/storagescanner.*`: Linux local-volume scanner using `QStorageInfo` plus an explicit Qt directory
  stack. It stays on the synchronization root device, skips links and special files, deduplicates hard links, and sums
  allocated blocks so sparse files reflect their physical footprint.
- `app/navigation/approuter.*`: minimal main-window router. It owns only `mainWindowActive` and the selected main tab;
  it must not read `AppCache`, call backend services, or decide whether onboarding is required.
- `app/navigation/mainwindowactivationdecision.*`: pure post-bootstrap choice between onboarding, the unconfigured Home,
  and the configured Home. Closing onboarding or removing the last configured sync selects the unconfigured Home on the
  next activation.
- `app/cache/onboardingstate.*`: session-owned onboarding selected user, selected available-drive keys, and pending sync
  configs. Advanced-settings validation replaces the complete selected-drive config map atomically so cancelling a
  modal never leaks a partially edited drive.
- `app/cache/parametersstore.*`: process-wide cache for server-owned application parameters (`ParametersInfo`). It is
  populated during the bootstrap sequence next to the product graph snapshot, but remains separate from `AppCache`
  because the server is still the persistence source of truth for application settings. It stores only the last
  server-confirmed snapshot; update workflows publish a new value only after `PARAMETERS_UPDATE` succeeds.
- `app/onboarding/availabledrivesmodel.*`: QML adapter for onboarding drive selection. It derives rows from
  `AppCache::availableDriveContexts(selectedUserDbId)` and stores row selection through `OnboardingState`. It must not
  own screen-level loading, user, or navigation state.
- `app/onboarding/driveselectioncontroller.*`: QML-facing screen controller for user presentation, loading/error state,
  counts, retry, and navigation actions. It owns the session's `AvailableDrivesModel`.
- `app/onboarding/onboardingentrydecision.*`: pure post-bootstrap decision from `AppCache` to Inactive, Login, or
  DriveSelection. If several users are connected and no drive is configured, it selects the connected user with the
  lowest database id, preserving the stable ordering returned by `AppCache::users()`.
- `app/onboarding/onboardingsession.*`: ephemeral owner of onboarding state, flow, login workflow, and drive-selection
  controller/model.
- `app/onboarding/onboardingsessionmanager.*`: process-long nullable-session owner and the only onboarding object
  exposed at the root QML boundary. It creates a session after cache bootstrap and unpublishes it before deferred
  destruction. It accepts activation requests while onboarding is required and rejects them with an explanatory log when
  no onboarding route is displayable. On successful onboarding completion, `AppClientLinux` opens the main window route.
- `app/onboarding/onboardingflowcontroller.*`: QML-facing onboarding flow controller aligned with the macOS flow
  (`login -> drive selection -> synchronization -> ready`, with macOS permission steps omitted on Linux). It owns simple
  onboarding UI actions such as opening account signup and drive-offer URLs, plus synchronization/ready presentation
  state; OAuth launch, `LOGIN_REQUESTTOKEN`, available-drive loading, and sync creation stay outside QML-facing flow
  state.
- `app/onboarding/onboardinglogincoordinator.*`: login workflow coordinator for onboarding. It wires the flow
  controller, OAuth service, comm service, user service, app cache, and onboarding state so `AppClientLinux` does not
  accumulate login-specific workflow logic.
- `app/onboarding/onboardingdefaultpathresolver.*`: resolves the default local folder of a drive as soon as it is
  selected, so advanced settings never open on a blocking request. A drive whose folder cannot be resolved is
  unselected, and drive selection cannot continue while a resolution is in flight.
- `app/onboarding/onboardingsyncconfigurationcontroller.*`: session-scoped transactional editor for advanced onboarding
  synchronization settings. It owns global and per-drive draft snapshots, validates custom paths against the server and
  against the other drafts, and commits all selected-drive configs only from the final confirmation. A rejected commit
  keeps the modal open, because closing would silently drop everything the user configured.
- `app/onboarding/selectedsyncconfigurationsmodel.*`: flat presentation model for the multi-drive advanced-settings
  summary. It exposes display values only; stable backend identifiers remain in the controller drafts.
- `app/onboarding/onboardingsynccreationcoordinator.*`: automatic end-of-onboarding sync creation coordinator. It
  derives collision-free local folders, creates selected-drive syncs sequentially with the validated remote blacklist,
  preserves only failed and not-yet-attempted work for retry, and reconciles the parent-first cache snapshot after a
  failed `SYNC_ADD`.
- `app/onboarding/oauthloginservice.*`: Linux v4 OAuth browser-launch service. It owns PKCE/state generation, idempotent
  browser relaunch during an active authorization, callback validation, and emits the authorization code to app wiring.
  Do not expose OAuth details to QML.
- `app/syncconfiguration/remotefolderprovider.*`: injectable asynchronous boundary for remote folder metadata,
  children, and sizes. The production adapter uses `CommService`; tests and future settings integration can provide the
  same contract without onboarding dependencies.
- `app/syncconfiguration/localpaths.*`: local synchronization-folder rules shared by onboarding and future settings
  work: the `~`-shortened display form used at the QML boundary only, folder overlap detection, and the free-folder
  derivation that appends the attempt count without a separator, as the server does.
- `app/syncconfiguration/remotefoldertreemodel.*`: reusable lazy `QAbstractItemModel` for selective synchronization. It
  owns canonical blacklist editing, tri-state propagation, access-denied rows, retryable child loads, and the bounded
  visible-row size queue. It must remain independent from onboarding state and synchronization database ids. A folder
  is included or excluded with its complete subtree, as on Windows; the partial state reports that a descendant is
  excluded and is never a state the user selects, and the drive root itself can never be excluded. A visible row loads
  its size and its immediate children, so its expand affordance reflects whether the folder really has sub-folders. An
  initial blacklist whose paths cannot be resolved fails the page instead of displaying ancestors as fully selected;
  a node the server no longer knows is dropped from the blacklist rather than treated as a failure.
- `app/services/cachepopulator.*`: two-branch snapshot loader for application parameters and user data. The user-data
  branch remains sequential and parent-first (users, accounts, drives, syncs, then sync errors); completion is emitted
  only after both branches succeed, and overlapping population requests are ignored. It is used at initial connection
  and for explicit reconciliation after a non-transactional backend mutation may have persisted parents without emitting
  their normal pushes; after each snapshot, it activates the server live-info refresh so only drive updates reach
  `CachePipeline`.
- `app/services/driveservice.*`: targeted drive use-case facade driven by `ServiceActionTracker` + `ServiceEventBus`;
  durable cache mutations stay signal-driven through `CachePipeline`.
- `app/services/activityservice.*`: activity-row link facade. It resolves private/public URLs through `CommService`,
  owns browser and clipboard side effects, and tracks concurrent actions without owning activity history.
- `app/services/parametersservice.*`: targeted facade for application settings updates. It starts from the confirmed
  `ParametersStore` snapshot, sends the full `PARAMETERS_UPDATE` payload, and updates the store only after server
  confirmation.
- `app/services/sentryservice.*`: Linux v4 Sentry coordinator. It reconciles cached and server-confirmed consent,
  publishes normalized Linux/Qt runtime tags after the GUI application exists, and refreshes the distribution channel
  from the confirmed `ParametersStore` snapshot.
- `app/services/syncservice.*`: targeted sync use-case facade driven by `ServiceActionTracker` + `ServiceEventBus`;
  durable cache mutations stay signal-driven through `CachePipeline`.
- `ui/`: QML shell, product windows, design tokens, reusable components, and bundled UI assets such as tray icons and
  onboarding Lottie animations.
    - `ui/dialogs/`: app-global dialog composition. `GlobalModalHost` stays alive across waiting, onboarding, and main
      routes; feature queueing remains in the owning C++ controller until several global modal families require shared
      arbitration.
    - `ui/windows/main/`: main-window shell, remaining temporary tab placeholders, and feature families grouped under
      `activities/`, `home/`, and `sidebar/`. Home presentation is split between its root composition, `shortcuts/`,
      `states/`, and versioned generated `animations/`. The shell is loaded only when `AppRouter` marks the main window
      active and no onboarding session is active. Do not add IPC calls here; dynamic data belongs in cache-backed QML
      models.
    - `ui/windows/main/activities/`: selected-sync Activities page, filter and action popups, table rows, source/status
      presentation, and empty state. Time, size, and status columns have fixed widths; only the name/folder boundary is
      draggable. It consumes `ActivitiesController` and `ActivityListModel`; it must not call IPC or own activity
      history.
    - `ui/windows/main/home/animations/`: versioned generated QML animations for Home statuses. Instantiate finite
      status animations only while their state is active so that they start when the status becomes visible.
    - `ui/windows/waiting/`: app-level preloading screen shown whenever the main window is opened before the initial IPC
      connection and cache bootstrap complete. It yields to onboarding or the main shell once a product route is ready.
    - `ui/windows/onboarding/`: onboarding window composition and flow screens. Onboarding-only QML stays here unless it
      becomes reusable from another product window.
    - `ui/features/syncconfiguration/`: reusable advanced-sync presentation. The remote-folder tree is a single tab
      stop that moves a current row internally, so tabbing never walks through every checkbox of a large folder list;
      the current row is a navigation cursor drawn as a tint, kept distinct from the keyboard focus ring.
    - `ui/components/`: reusable presentation primitives without product-window ownership. Main-window sidebar
      primitives accept display values and emit interactions; they do not read `AppCache`, own selection, or call
      services directly. `IKModal` and `IKModalButton` provide the styled in-app modal surface and semantic action roles;
      feature dialogs supply their own wording, state, and actions. `IKCheckBox` is the shared tri-state indicator: it
      renders the state it is given and only reports clicks, so a model owning the selection stays authoritative. `IKLinkButton` is the
      inline textual action for rows and cards that must not carry a button surface of their own.
    - `ui/chrome/`: shared window chrome: frameless shell, header bar, controls, resize handles, and shadow wrapper.
      Top-level app-owned QML windows should use `IKShadowedWindow`; its `headerBackgroundData` and `headerData` slots
      accept page-specific header visuals and content while preserving the standard move, resize, minimize, maximize,
      and close behavior. `IKWindowResizeHandles` is the single owner of custom-frame resize hit areas and can be reused
      by an interaction layer above a modal overlay without unblocking the underlying application content. Onboarding
      uses `headerOverlaysContent` so window controls do not shift its fixed visual composition. The window decoration
      controller limits input to the surface and resize handles without clipping the diffuse shadow. It publishes
      `_GTK_FRAME_EXTENTS` on X11/XWayland so those window managers align the visible surface rather than the transparent
      shadow during snapping and maximization. Native Wayland intentionally uses public Qt APIs only and therefore snaps
      the complete native window, including its transparent shadow margin.
    - `ui/windows/onboarding/animations/`: versioned generated QML animation components produced from Lottie JSON
      payloads. Do not edit these files manually. They are excluded from `qmllint`; validation belongs to the generator
      and the QML compilation step.

### Regenerate Onboarding Lottie QML

Run `lottietoqml` from the Qt/Conan package that provides `qtlottie`. The loader source JSON is supplied locally and is
not versioned; only the generated QML component is tracked. Set `LOTTIE_INPUT` to that source file before running:

```bash
source build-linux/build/build/Debug/generators/conanrun.sh
mkdir -p build-linux/lottie-json
LOTTIE_INPUT=${LOTTIE_INPUT:?Set LOTTIE_INPUT to the loader-stroke JSON source}

LOTTIE_QML_LICENSE=$(cat <<'EOF'
Infomaniak kDrive - Desktop
Copyright (C) 2023-2026 Infomaniak Network SA

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program. If not, see <http://www.gnu.org/licenses/>.
EOF
)

jq '(.layers[] | select(.nm == "page-front") | .shapes) |=
      [.[0], (.[-1] | .ty = "st" | .w = {"a": 0, "k": 1.25} |
      .lc = 2 | .lj = 2 | .ml = 4 | del(.r))] |
    ((.. | objects | select(.ty? == "sh") | .ks.k[]?) |= del(.h))' \
  "$LOTTIE_INPUT" \
  > build-linux/lottie-json/kDrive-LoaderStroke-LIGHT.json
lottietoqml -c -p \
  --copyright-statement \
  "$LOTTIE_QML_LICENSE
Generated by lottietoqml. Do not edit manually." \
  build-linux/lottie-json/kDrive-LoaderStroke-LIGHT.json \
  src/gui4/linux/ui/windows/onboarding/animations/LoaderStrokeAnimation.qml
```

## Build And Validation

`kdrive_qml` is configured to output into `${BIN_OUTPUT_DIRECTORY}`, next to the `kDrive` server executable.

```bash
# From repo root: install Debug dependencies and generate Conan / CMake presets
./infomaniak-build-tools/conan/build_dependencies.sh Debug

# Configure the generated Debug preset, then build the relevant Linux targets
cmake --preset conan-debug -S . -B build-linux/build/build/Debug
cmake --build build-linux/build/build/Debug --target kDrive kDrive_client kdrive_qml -- -j 8
```

## Architecture Rules

- Keep layers strict:
    - `communicationlayer/*` = transport/protocol mechanics only.
    - `app/services/*` = typed backend facade for upper layers.
    - `ui/*` = presentation and bindings; no protocol code.
- QML must never call `IpcClient` directly.
- New backend calls must be added in `CommService`, not in UI files.
- Server-push events must be registered in `CommService::register*Handlers` and re-emitted as typed Qt signals.
- When object lifetime is uncertain in async callbacks, guard with `QPointer<T>`.
- For cross-service UI error notification, use a single shared `ServiceEventBus` instance and inject it by reference
  into each service (`UserService`, `DriveService`, `SyncService`, ...).
- Keep responsibilities split:
  `ServiceEventBus` for transient events, `ServiceActionTracker` for durable pending-action state.
- Do not create per-service formatted error-string state for UI display; services emit generic bus signals and keep
  structured backend error information in request handlers/logs.
- `DriveService` and `SyncService` use `ServiceActionTracker` for loading/pending state and `ServiceEventBus` for
  transient failure notification; avoid reintroducing local `lastError` / ad hoc pending counters there.
- `AppCache`, `MainSelectionStore`, and `OnboardingState` mutations must run on the Qt main thread.
- `AppCache` must not own mutable main selection; derive main context through `MainSelectionStore.currentSyncDbId`.
- Main-sidebar drive/sync rows belong in `SyncSelectorModel`. Keep window state, tab navigation, desktop actions, and
  future Home/Activities/Storage data out of that model.
- Store available drives per user via `AppCache::replaceAvailableDrivesForUser(...)`; do not reintroduce a global
  available-drives snapshot.
- `CachePipeline` owns the direct push-signal bridge from `CommService` to `AppCache`; service classes should not wire
  those pushes themselves.
- `CachePipeline` must not let server pushes mutate `AppCache` before the initial `CachePopulator` snapshot has
  completed.
- Full graph snapshots (`USER_INFOLIST`, `ACCOUNT_INFOLIST`, `DRIVE_INFOLIST`, `SYNC_INFOLIST`, initial error list)
  belong to `CachePopulator` for bootstrap/reconnect and explicit parent-first reconciliation after a non-transactional
  mutation failure. Do not expose user/drive/sync full-refresh methods to QML services.
- QML-facing services should provide targeted actions only; user/account/drive/sync cache consistency is
  push-signal-driven.
- Onboarding navigation belongs in `OnboardingFlowController`; keep long-running backend work in service facades and
  durable selections in `OnboardingState`. The login screen must not advance optimistically: it advances only after the
  server login-token request succeeds, the logged-in user appears in `AppCache`, and available-drive loading has been
  requested. The drive-selection screen then owns the loading/empty/loaded presentation while the request completes.
- Pass stable app-owned controllers such as `OnboardingSessionManager` and `MainSidebarController` to `Main.qml` as
  initial properties. Pass controllers/models down through explicit required QML properties; do not add dynamic context
  properties for window-owned composition.
- Keep app-global modals in `GlobalModalHost` so they survive route loader changes. Contextual onboarding/settings
  modals should instantiate `IKModal` in their owning window or view instead of moving their workflow into the global
  host.

## IPC And Error Handling

- Transport is loopback TCP + JSON envelope shared with `libcommon/comm.h` / `libcommon/commjson.h`, wrapped in TLS via
  `QSslSocket`. The server presents a self-signed certificate generated by `SelfSignedCert` and stored in the OS
  keychain under `certKeychainKey` (`libcommon/comm.h`). The client pins that certificate as the only trusted CA
  (`CertReader` loads it at startup) and verifies the peer name against `kDrive-localhost` (`localHostName` in
  `libcommon/comm.h`); SSL errors are logged and never ignored. The shared keychain `package` / `service` constants live
  in `libcommon/utility/utility.h` (`keychainConstant` namespace) and must stay in sync with `KeyChainStorage`
  on the server side.
- `IpcClient` treats post-connection socket failure as fatal (disconnect/error after first successful connection exits
  process).
- Request methods should parse `Poco::DynamicStruct` into typed DTOs before exposing data upward.
- Generic UI-facing request failures from high-level services should be emitted through `ServiceEventBus` so UI can
  subscribe once (`genericErrorOccurred()`).
- Sentry captures in Linux v4 should go through `SentryService`; capture helpers must tolerate Sentry being
  uninitialized because consent can disable the native SDK.
- Action-level and per-service loading state should come from `ServiceActionTracker`
  (`isActionPending(...)`, `isServicePending(...)`).
- Use shared `msgParam*` keys and enums from `libcommon`; avoid ad-hoc string keys.

## Coding Conventions (Linux v4)

- Use `QLoggingCategory` + `qCDebug/qCInfo/qCWarning/qCCritical`; no `std::cout`.
- Do not introduce raw `int` when fixed-width types are appropriate (`int32_t`, `uint8_t`, ...).
- Prefer documenting private implementation helpers in `.cpp` rather than headers.
- Do not run `clang-format` on `CMakeLists.txt` in this repository.
- Size a fixed-width column from its content, never from a hard-coded constant. The model exposes the widest strings the
  column can render in the active locale (`ActivityListModel::timeTextSamples`, `sizeTextSamples`) and measures them
  with `maxTextWidth(texts, font)`; the view takes the max with the header label, measured separately via `TextMetrics`
  because the header elides too. A constant tuned on one language truncates in the ones with longer wordings.

## Change Playbooks

### Add a new request in `CommService`

1. Verify `RequestNum` and `msgParam*` exist in `libcommon`.
2. Add callback typedef if needed.
3. Add method declaration in `commservice.h`.
4. Implement method in `commservice.cpp` using `_ipcClient.sendRequest(...)`.
5. Convert IPC payload to typed result object before invoking callback.

### Add a new server-push signal

1. Verify `SignalNum` and payload keys in `libcommon`.
2. Register handler in the appropriate `register*Handlers` method.
3. Parse payload safely into typed fields/DTOs.
4. Emit a typed Qt signal from `CommService`.

## Quick Find

```bash
# Linux v4 files
find src/gui4/linux -type f | sort

# IPC request methods and callsites
rg -n "request[A-Z].*\\(" src/gui4/linux/app/services/commservice.* -g "*.h" -g "*.cpp"

# Signal registrations
rg -n "registerHandler\\(" src/gui4/linux/app/services/commservice.cpp

# Shared request/signal enums and JSON keys
rg -n "enum class (RequestNum|SignalNum)|msgParam" src/libcommon/comm.h src/libcommon/commjson.h

# Logging categories in Linux v4 frontend
rg -n "Q_LOGGING_CATEGORY|qC(Debug|Info|Warning|Critical)" src/gui4/linux -g "*.cpp" -g "*.h"
```

## Pre-PR Checks

```bash
# Native Linux validation from repo root
./infomaniak-build-tools/conan/build_dependencies.sh Debug
cmake --preset conan-debug -S . -B build-linux/build/build/Debug
cmake --build build-linux/build/build/Debug --target kDrive kDrive_client kdrive_qml -- -j 8
```
