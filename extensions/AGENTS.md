# extensions — Platform Shell Extensions

## Package Identity
Native shell integration extensions. **Separate build systems** from the main CMake project. macOS uses Xcode; Windows uses Visual Studio. These are loaded by the OS as privileged extensions — changes here have higher risk.

## Structure
| Directory | Purpose | Build System |
|---|---|---|
| `extensions/MacOSX/` | macOS Finder Sync + LiteSync VFS extension | Xcode workspace |
| `extensions/windows/cfapi/` | Windows Cloud Filter API (CFAPI) shell integration | Visual Studio / NuGet |
| `extensions/windows/standard/` | Windows standard shell integration | Visual Studio |
| `extensions/icons/` | Shell overlay icons (shared assets) | N/A |

## macOS Extensions
```bash
# Open workspace in Xcode
open extensions/MacOSX/kDriveLiteSync.xcworkspace

# Build from CLI (requires Xcode command line tools)
xcodebuild -workspace extensions/MacOSX/kDriveLiteSync.xcworkspace \
           -scheme kDriveFinderSync \
           -configuration Debug \
           build
```

Key files:
- Finder Sync extension entry: `extensions/MacOSX/kDriveFinderSync/FinderSync.mm`
- LiteSync VFS extension: `extensions/MacOSX/kDriveLiteSync/`
- IPC with server process (XPC): look for `NSXPCConnection` in `.mm` files

## Windows CFAPI Extension
```powershell
# Restore NuGet packages
cd extensions/windows/cfapi
nuget restore

# Build via MSBuild
msbuild kDriveCfApiExtension.sln /p:Configuration=Release /p:Platform=x64
```

Key files:
- CFAPI shell integration entry: `extensions/windows/cfapi/` — look for `CfRegisterSyncRoot`.

## Patterns & Conventions
- **Language:** Objective-C/Objective-C++ for macOS; C++ for Windows CFAPI.
- **IPC with server:** Use XPC (macOS) or named pipes (Windows) to communicate extension status back to the server process. Mirror the comm protocol in `src/common/comm.h`.
- **Signing:** Extensions must be signed and notarized on macOS for Gatekeeper. Signing is handled by CI; never disable code signing for a release build.
- **System Extension vs App Extension:** LiteSync uses a System Extension (privileged). Any API changes require re-approval from Apple.
- DO: Test Finder Sync badge changes on a real macOS machine; simulator doesn't support Finder extensions.
- DON'T: Add business logic to extensions. They should only relay events to the server process.

## JIT Index Hints
```bash
# Find XPC service definitions
rg -rn "NSXPCInterface|NSXPCConnection" extensions/MacOSX/ -g "*.mm"

# Find CFAPI callbacks
rg -rn "CF_CALLBACK|CfRegisterSyncRoot" extensions/windows/cfapi/

# Find icon overlay registration
rg -rni "overlay|badge" extensions/ -g "*.mm" -g "*.cpp"
```

## Common Gotchas
- macOS System Extensions require a provisioning profile even in debug. Keep the profile up to date in CI secrets.
- Windows CFAPI requires the process to run elevated for `CfRegisterSyncRoot`. Integration tests on Windows may need elevated rights.
- Changes to the macOS extension bundle identifier require updating the server's XPC service lookup.
