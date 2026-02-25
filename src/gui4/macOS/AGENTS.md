# macOS GUI Refonte — Swift Front-End

## Package Identity
New macOS GUI written in Swift 6, replacing the Qt-based `src/gui/`. Hybrid SwiftUI + AppKit app.
Three Xcode targets: `kDriveCore` (server bridge + cache), `kDriveCoreUI` (design system + shared UI), `kDrive` (main app).
Communicates with the C++ server daemon exclusively over **XPC** — no sync logic here.

## Setup & Run
```bash
# Open project in Xcode
open src/front/macOS/kDrive.xcodeproj

# Run tests (all)
xcodebuild test -project src/front/macOS/kDrive.xcodeproj -scheme kDriveTests

# Run SwiftLint
cd src/front/macOS && swiftlint lint

# Run SwiftLint auto-correct
cd src/front/macOS && swiftlint --fix
```

## Architecture (3 targets)

```
kDriveCore/          ← server bridge: XPC, cache, DTOs, signals
kDriveCoreUI/        ← shared UI: design tokens, AppKit components, SwiftUI components
kDrive/              ← main app: VCs, ViewModels, SwiftUI views, router
kDriveResources/     ← assets, colors, images, localizable strings
kDriveTests/         ← unit tests
```

## Patterns & Conventions

### IPC with C++ server (XPC)
All server calls go through `XPCQueryFetcher`, injected via DI. Never call XPC directly from a VC or View.

```
// Pattern for a Job (kDriveCore/ServerBridge/Jobs/):
let query = MyQuery(param: value)
let request = await RequestMessage<MyQuery>(num: RequestNum.MY_REQUEST, body: query)
let response = try await queryFetcher.query(request, responseType: CallbackMessage<MyResponse>.self)
```

- Jobs are grouped by domain: `UtilityJobs.swift`, `SyncJobs.swift`, `DriveJobs.swift`, `NodeJobs.swift`, `UserJobs.swift`
- DO: Add new jobs to the appropriate `*Jobs.swift` file
- DON'T: Create one-off job calls outside the `Jobs/` folder

### Cache (CoherentCache)
Hierarchy: **User → Account → Drive → Synchro**. Always async actor-based.

```swift
// Inject and use:
@LazyInjectService private var coherentCache: CoherentCache
let synchro = await coherentCache.getSynchro(synchroDbId: id)

// Observe reactively (property wrapper in @MainActor class):
@ObservedSynchro(synchroDbId: id) var synchro: Synchro?
```

- `ObservedXxx` property wrappers (in `Cache/Observation/`) wrap Combine publishers for reactive observation
- `CoherentCacheObservable` provides `usersPublisher` — use extension methods (`.synchroPublisher(...)`, `.allSynchrosPublisher()`, etc.)
- DO: Use `ObservedXxx` wrappers or `usersPublisher` chains for reactive UI updates
- DON'T: Poll the cache — subscribe reactively

### Dependency Injection (InfomaniakDI)
```swift
@LazyInjectService private var coherentCache: CoherentCache   // lazy, used in classes
@InjectService var service: MyService                          // immediate, used in functions
```
Services are registered at app startup. Check `AppDelegate` or the DI setup file for registration.

### ViewModels & Views
- ViewModels are `@MainActor final class` conforming to `ObservableObject`
- ViewControllers (AppKit `NSViewController`) host SwiftUI views via `NSHostingView`
- Example ViewModel: `kDrive/UI/Controllers/MainWindow/MainView/MainViewModel.swift`
- Example VC: `kDrive/UI/Controllers/MainWindow/MainView/MainViewController.swift`

### Navigation / Router
`MainViewRouter` manages the main window (injected via DI):
```swift
@LazyInjectService private var router: MainViewRouter
router.setCurrentTab(.home)       // switch tab
router.append(.activityError)     // push detail
router.removeLast()               // pop detail
```
Tabs: `.home` · `.activities` · `.storage` · `.blockingError`

### Onboarding Flow
`OnboardingFlowCoordinator` drives the onboarding steps:
- Steps: `login → drivesSelection → permissions(MacOSPermission) → synchronization → appReady`
- Reference: `kDrive/UI/Controllers/MainWindow/Onboarding/OnboardingFlowCoordinator.swift`

### UI Models (UIXxx)
DTOs from the server are never used directly in Views. Map them to `UIXxx` structs first:
- `UISynchro`, `UISynchroContext`, `UIDrive`, `UIUser`, `UIAccount`, `UIBlockingError`
- Mappers live in `kDriveCoreUI/Models/UIs/Mappers/`
- DO: `let uiSynchro = UISynchro(synchro: serverSynchro)`
- DON'T: Pass raw `Synchro` DTO to a SwiftUI View

### Design Tokens (kDriveCoreUI/Tokens/)
Never hardcode colors, spacing, or radii. Use tokens:
```swift
// Colors (semantic, light/dark aware)
ColorToken.Text.primary.asColor          // SwiftUI Color
ColorToken.Surface.secondary.asNSColor   // NSColor

// Spacing
AppPadding.page          // 24pt page margin
AppPadding.padding16

// Radius
AppRadius.large

// Icons/images
AppIconSize.medium
```

### Signals (server-push)
Server sends unsolicited signals decoded by `XPCSignalHandler` → dispatched to domain handlers:
- `AccountSignalHandler`, `DriveSignalHandler`, `SynchroSignalHandler`, `UserSignalHandler`, `UtilitySignalHandler`
- Signal DTOs in `kDriveCore/ServerBridge/Signal/DTOs/`

### Localization
5 languages: `fr` / `en` / `de` / `es` / `it`.
Strings live in `kDriveResources/Localizable/<lang>.lproj/`.
```swift
// Use NSLocalizedString with a key from the resource bundle
NSLocalizedString("key", bundle: .kDriveResources, comment: "")
```

## Key Files
- XPC query entry point: `kDriveCore/ServerBridge/XPC/XPCQueryFetcher.swift`
- XPC connection: `kDriveCore/ServerBridge/XPC/XPCConnectionManager.swift`
- Cache protocol: `kDriveCore/ServerBridge/Cache/CoherentCache.swift`
- Cache (server impl): `kDriveCore/ServerBridge/Cache/ServerCoherentCache.swift`
- Color tokens: `kDriveCoreUI/Tokens/ColorToken.swift`
- Sidebar model: `kDriveCoreUI/Models/SidebarItem.swift`
- Main router: `kDrive/UI/Controllers/MainWindow/MainView/MainViewRouter.swift`
- Main ViewModel: `kDrive/UI/Controllers/MainWindow/MainView/MainViewModel.swift`
- Onboarding coordinator: `kDrive/UI/Controllers/MainWindow/Onboarding/OnboardingFlowCoordinator.swift`

## JIT Index Hints
```bash
# Find a Job function
rg -n "func " src/front/macOS/kDriveCore/ServerBridge/Jobs/

# Find a signal handler
rg -n "class .*SignalHandler" src/front/macOS/kDriveCore/

# Find DI service registration
rg -n "SimpleResolver\|register(" src/front/macOS/kDrive/

# Find all ObservedXxx property wrappers
rg -n "@propertyWrapper" src/front/macOS/kDriveCore/ServerBridge/Cache/Observation/

# Find SwiftUI views
rg -n "struct .*View.*: View" src/front/macOS/kDrive/UI/Views/

# Find all RequestNum usages
rg -n "RequestNum\." src/front/macOS/kDriveCore/

# Find localization key usages
rg -n "NSLocalizedString" src/front/macOS/kDrive/
```

## Common Gotchas
- `@MainActor` is required on all ViewModels — missing it causes runtime warnings in Swift 6 strict concurrency.
- Signal handlers update the cache; the cache then publishes via Combine. Never update UI directly from a signal handler.
- `RequestMessage.init` is `async` (uses actor for ID generation) — always `await` it.
- XPC data is base64-encoded for binary payloads — use `@Base64Coded*` property wrappers (in `XPC/DTOs/PropertyWrappers/`) for `URL`, `String`, `Data`, `NSColor`.
- AppKit views (`NSView`) needing SwiftUI content: wrap with `NSHostingView<ContentView>`.
- SwiftLint's `unused_import` analyzer rule is active — remove unused imports before committing.

## Pre-PR Checks
```bash
# Lint
cd src/front/macOS && swiftlint lint

# Build + test
xcodebuild test -project src/front/macOS/kDrive.xcodeproj -scheme kDriveTests -destination 'platform=macOS'
```