# gui4 — WinUI3 Windows GUI (v4 redesign)

## Package Identity
WinUI3 / Windows App SDK application written in C# (.NET 9), replacing the Qt-based `src/gui/` on Windows.
Single Visual Studio solution (`kDrive client.sln`) containing two projects:
- `kDrive client` — the main WinUI3 app (this is the primary project you will edit)
- `Bootstrapper` — A small C# project set up as a dependency of the kDrive client project. It is only used to update the version of the project with the one in `version.json` at the root of the repository.

Communicates with the C++ server daemon exclusively over a **TCP socket** (JSON protocol) — no sync logic here.
Namespace: `Infomaniak.kDrive`.

## Solution & Project Layout
```
src/gui4/windows/kDrive client/
  kDrive client.sln
  Bootstrapper/
    Bootstrapper.csproj
    Program.cs
  kDrive client/
    kDrive client.csproj           <- main project file (NuGet deps, build targets)
    App.xaml / App.xaml.cs         <- DI container setup, window lifecycle, OAuth handler
    MainWindow.xaml / .cs          <- main shell window
    OnBoardingWindow.xaml / .cs    <- onboarding shell window
    AppConstants.cs                <- environment-specific constants (Sentry DSN, URLs…)
    Logger/Logger.cs               <- Serilog + Sentry logging facade
    Localizer/Localizer.cs         <- runtime i18n via Windows ResourceManager
    TrayIcon/TrayIconManager.cs    <- system tray icon (H.NotifyIcon)
    Pages/                         <- full-page views (each = XAML + code-behind)
    CustomControls/                <- reusable user controls (each = XAML + code-behind)
    ViewModels/                    <- observable data models (no XAML)
    ServerCommunication/           <- IPC layer: socket protocol + service facade
    Converters/                    <- IValueConverter implementations
    Styles/                        <- XAML resource dictionaries (tokens, themes)
    Strings/<lang>/Resources.resw  <- localisation (fr-FR, en-US, de-DE, es-ES, it-IT)
    Assets/                        <- icons, images, Lottie animations
```

## Setup & Build (Windows only)
```powershell
# Open in Visual Studio 2022+ (Windows App SDK workload required)
# Or build from CLI with the Developer Command Prompt:
cd "src/gui4/windows/kDrive client"
dotnet build "kDrive client.sln" -c Debug

# Run the Bootstrapper (starts server + GUI)
dotnet run --project Bootstrapper

# Restore NuGet packages only
dotnet restore "kDrive client.sln"

# Fetch the current version string from GitHub releases
pwsh -ExecutionPolicy Bypass -File "kDrive client/fetchVersion.ps1"
```

Note: the project does not build on Linux/macOS — it is Windows-only (`net9.0-windows10.0.19041`).

## Architecture

### Dependency Injection
Services are registered as singletons in `App.xaml.cs` using `Microsoft.Extensions.DependencyInjection`:
```csharp
_services.AddSingleton<AppModel>();
_services.AddSingleton<IServerCommProtocol, SocketServerCommProtocol>();
_services.AddSingleton<IServerCommService, ServerCommService>();
_services.AddSingleton<TrayIconManager>();
```
Access services anywhere with `App.ServiceProvider.GetRequiredService<T>()`.
- DO: Resolve services via `App.ServiceProvider`
- DON'T: Use `new` to instantiate registered services — always go through DI

### AppModel (central state)
`ViewModels/App/App.cs` — the single source of truth for all runtime state.
Extends `UISafeObservableObject` (CommunityToolkit.Mvvm `ObservableObject` + UI-thread dispatch).
Hierarchy: `User` → `Account` → `Drive` → `Sync`.
- `AppModel.Users` — `ObservableCollection<User>`
- `AppModel.AllDrives` — `ReadOnlyObservableCollection<Drive>` (derived via DynamicData)
- `AppModel.AllSyncs` — `ReadOnlyObservableCollection<Sync>` (derived via DynamicData)
- `AppModel.SelectedSync` — currently displayed sync
- `AppModel.Settings` — global settings (proxy, log level, update channel…)

### ViewModels
Located in `ViewModels/`. All view model classes extend `UISafeObservableObject`.
Use `SetPropertyInUIThread(ref field, value)` for properties bound to UI elements that may be updated from background threads.
Use the standard `SetProperty(ref field, value)` for non-UI properties.

Example ViewModel:
- App-level: `ViewModels/App/User.cs`, `Drive.cs`, `Sync.cs`, `Error.cs`
- Onboarding: `ViewModels/Onboarding/Onboarding.cs`, `NewSync.cs`
- Settings: `ViewModels/Settings/Settings.cs`, `UpdateManager.cs`, `LogUploadManager.cs`

### IPC with C++ server (TCP socket)
All server calls go through `IServerCommService`, implemented by `ServerCommService`.
The wire protocol is handled by `IServerCommProtocol`, implemented by `SocketServerCommProtocol`.

Pattern for a request:
```csharp
// In ServerCommService — inject via DI, never call directly from a Page/Control
CommData data = await _commClient.SendRequestAsync(
    RequestNum.SYNC_START,
    new JsonObject { [JsonKeys.SyncDbId] = syncDbId },
    cancellationToken);
if (!CheckJobResultAndLogIfError(data, parms)) return false;
```

Pattern for handling a server-push signal:
```csharp
// In ServerCommService.OnSignalReceived — add a new case to the switch
case SignalNum.MY_NEW_SIGNAL:
    await HandleMyNewSignalAsync(sender, args);
    break;
```

- All `RequestNum` values are defined in `ServerCommunication/Services/CommSharedEnums.cs`
- All JSON keys are defined in `ServerCommunication/Services/CommSharedJsonKeys.cs`
- Struct/DTO types are in `ServerCommunication/Services/CommSharedStructs.cs`
- All string paths transmitted over IPC must be Base64-encoded: use `Utility.ToBase64String(path)`
- All string paths received from IPC are Base64-encoded: deserialize with `Base64StringJsonConverter`
- DO: Add new server calls to `ServerCommService.cs` only
- DON'T: Call `_commClient.SendRequestAsync` directly from a Page or Control

### Pages
Located in `Pages/`. Each page is a `Microsoft.UI.Xaml.Controls.Page` subclass.
XAML file + `.cs` code-behind, paired by name (e.g. `HomePage.xaml` / `HomePage.xaml.cs`).
Pages are navigated to via `Frame.Navigate(typeof(MyPage))` from `MainWindow` or `OnBoardingWindow`.

Main pages:
- `Pages/HomePage.xaml` — dashboard for the selected sync
- `Pages/ActivityPage.xaml` — recent sync file activity
- `Pages/ErrorPage.xaml` — sync errors list
- `Pages/StoragePage.xaml` — storage usage
- `Pages/Settings/SettingsPage.xaml` — general settings
- `Pages/Settings/DriveManagementPage.xaml` — per-drive management
- `Pages/Settings/DriveAdvancedSyncsPage.xaml` — advanced sync configuration
- `Pages/Onboarding/` — multi-step onboarding flow (Welcome → OAuth → DriveSelection → Sync → Finish)
- `Pages/SpecialErrorsPages/` — full-page blocking error states (maintenance, drive asleep, access denied…)

### CustomControls
Located in `CustomControls/`. Reusable `UserControl` subclasses.
Follow the same XAML + `.cs` code-behind pattern as Pages.

Key controls:
- `CustomControls/AppNavigationView.xaml` — main NavigationView shell
- `CustomControls/SyncActivityTable.xaml` — activity feed table
- `CustomControls/Errors/ErrorPresenter.xaml` — error list with `ErrorFactory` for template selection
- `CustomControls/Errors/ErrorBar.xaml` — compact inline error bar
- `CustomControls/DriveSetupContentDialog/` — add-drive dialog (multi-page flow)

### Styles & Design Tokens
Never hardcode colors, spacing, or font sizes. Use XAML resource keys:
```xaml
<!-- Colors (theme-aware via light/dark dictionaries) -->
<TextBlock Foreground="{ThemeResource Infomaniak.Style.Color.Brand.DefaultBrush}" />

<!-- Spacing (from Styles/InfomaniakStyles.xaml) -->
<Border Padding="{StaticResource Infomaniak.Style.Thickness.Uniform.M}" />

<!-- Typography -->
<TextBlock Style="{StaticResource Infomaniak.Style.TextBlock.BodyStrong}" />
```

Resource dictionaries:
- `Styles/InfomaniakStyles.xaml` — static tokens (spacing, font sizes, corner radii, button styles)
- `Styles/DS/InfomaniakStylesDark.xaml` / `InfomaniakStylesLight.xaml` — DS-defined theme-aware colors
- `Styles/Custom/InfomaniakStylesDark.xaml` / `InfomaniakStylesLight.xaml` — custom theme-aware overrides

### Localization
5 supported languages: `fr-FR`, `en-US`, `de-DE`, `es-ES`, `it-IT`.
Strings live in `Strings/<lang>/Resources.resw`.

In XAML (via `BindingLocalizerHelper`):
```xaml
Text="{Binding Source={StaticResource Localizer}, Path=GetString[my.key]}"
```

In C# code:
```csharp
string label = Localizer.Instance.GetString("mykey");
string withArg = Localizer.Instance.GetString1s("myKeyWithArg", someValue);
```

Missing keys render as `!key!` at runtime — always check both compile and run.
- DO: Add new strings to all 5 supported languages through the Poco MCP. If they are not available, do not add them manually to the .resw files and warn the user.
- DON'T: Hardcode any user-visible string in C# or XAML

### Logging
Use `Logger.Log(Logger.Level.X, "message")` everywhere. Never use `Console.Write*` or `Debug.Write*`.
Log levels: `Extended` (verbose),  `Debug`, `Info`, `Warning`, `Error`, `Fatal`.
Output: rotating Serilog file at `%LOCALAPPDATA%\temp\kDrive-logdir\` + Sentry breadcrumbs/events.

```csharp
Logger.Log(Logger.Level.Info, "My operation succeeded.");
Logger.Log(Logger.Level.Error, $"Failed to do X: {ex.Message}");
```

### Converters
Located in `Converters/`. Implement `IValueConverter` for XAML bindings.
Reference existing converters before writing a new one:
- `BooleanToVisibilityConverter` — `bool` → `Visibility`
- `BytesToHumanReadableStringConverter` — bytes → "1.2 MB"
- `DateTimeToTimeAgoConverter` — `DateTime` → "3 min ago"
- `FilePathToIconConverter` — file path → icon `BitmapImage`
- `SyncDirectionToIconUriConverter` — `SyncDirection` → icon URI

### TrayIcon
`TrayIcon/TrayIconManager.cs` — manages the system tray icon using H.NotifyIcon.
Icon assets are in `Assets/logo/taskbar-ico-*.ico` (dark/light × states: default, sync, pause, error, notif).

## Key Files
- DI + app lifecycle: `src/gui4/windows/kDrive client/kDrive client/App.xaml.cs`
- Central state model: `src/gui4/windows/kDrive client/kDrive client/ViewModels/App/App.cs`
- IPC service facade: `src/gui4/windows/kDrive client/kDrive client/ServerCommunication/Services/ServerCommService.cs`
- IPC socket protocol: `src/gui4/windows/kDrive client/kDrive client/ServerCommunication/Services/SocketServerCommProtocol.cs`
- IPC enums (RequestNum, SignalNum): `src/gui4/windows/kDrive client/kDrive client/ServerCommunication/Services/CommSharedEnums.cs`
- IPC JSON keys: `src/gui4/windows/kDrive client/kDrive client/ServerCommunication/Services/CommSharedJsonKeys.cs`
- Design tokens: `src/gui4/windows/kDrive client/kDrive client/Styles/InfomaniakStyles.xaml`
- Logger: `src/gui4/windows/kDrive client/kDrive client/Logger/Logger.cs`
- Localizer: `src/gui4/windows/kDrive client/kDrive client/Localizer/Localizer.cs`
- Thread-safe base class: `src/gui4/windows/kDrive client/kDrive client/Utility/UISafeObservableObject.cs`

## JIT Index Hints
```powershell
# Find a Page class
rg -n "class .*Page.*: Page" "src/gui4/windows/kDrive client" -g "*.cs"

# Find a CustomControl class
rg -n "class .*: UserControl" "src/gui4/windows/kDrive client" -g "*.cs"

# Find all RequestNum usages (IPC calls)
rg -n "RequestNum\." "src/gui4/windows/kDrive client" -g "*.cs"

# Find all SignalNum usages (signal handlers)
rg -n "SignalNum\." "src/gui4/windows/kDrive client" -g "*.cs"

# Find all IValueConverter implementations
rg -n "IValueConverter" "src/gui4/windows/kDrive client" -g "*.cs"

# Find a localization key usage
rg -n "GetString\[" "src/gui4/windows/kDrive client" -g "*.xaml"

# Find resource key references
rg -n "StaticResource Infomaniak\." "src/gui4/windows/kDrive client" -g "*.xaml"

# Find ViewModel property definitions
rg -n "SetPropertyInUIThread\|SetProperty" "src/gui4/windows/kDrive client/kDrive client/ViewModels" -g "*.cs"
```

## Common Gotchas
- **UI thread rule**: any modification to an `ObservableCollection` or a bound property must happen on the UI thread. Use `Utility.RunOnUIThread(action)` from a background task, or `SetPropertyInUIThread` in ViewModels.
- **Base64 paths**: all file/folder paths sent to or received from the C++ server are Base64-encoded. Always wrap with `Utility.ToBase64String(path)` before sending; always add `Base64StringJsonConverter` to `JsonSerializerOptions` when deserializing.
- **`MockServerCommProtocol`**: there is a mock implementation for testing without a running server. Switch via `App.xaml.cs` DI registration to `MockServerCommProtocol` during development.
- **Post-build PRI rename**: the build automatically renames `kDrive.pri` to `Resources.pri` (a known unpackaged WinUI3 bug). Do not manually create or modify `.pri` files.
- **Lottie pre-build**: animation JSON files in `Assets/Custom/Animations/` are converted by a PowerShell pre-build step (`lottieConverter.ps1`). Add new animations there only.
- **SVG icons**: rendered at runtime via the `Svg` NuGet package through the `SvgIcon` control (`CustomControls/SvgIcon.cs`). Do not use `<Image>` for SVG assets.
- **Nullable**: the project has `<Nullable>enable</Nullable>`. All new code must be null-safe; suppress warnings only with justification.
- **Overflow checks**: enabled in all configurations — avoid unchecked integer arithmetic.

## Pre-PR Checks
```powershell
# Build the solution (Debug, x64)
dotnet build "src/gui4/windows/kDrive client/kDrive client.sln" -c Debug -p:Platform=x64

# Verify no missing localization keys (look for !key! patterns at runtime, or grep for incomplete resw)
rg -n "" "src/gui4/windows/kDrive client/kDrive client/Strings/en-US/Resources.resw" | wc -l
# Compare count against other locales — all should match

# Check for hardcoded strings in XAML (no Text="..." with user-visible content)
rg -n 'Text="[A-Z]' "src/gui4/windows/kDrive client" -g "*.xaml"
```