# gui — Qt Widgets Desktop GUI

## Package Identity
200+ file Qt 6 Widgets application. Provides the tray icon, all dialogs, and the add-drive/add-sync wizards. Communicates with the server process over IPC. No sync logic here — all state lives in the server process.

## Key Files
- Application entry point: `src/gui/application.h`
- Tray icon + menu: `src/gui/systray.h`
- IPC client (talks to server): `src/gui/commclient.h`
- Add drive wizard: `src/gui/wizard/`
- Drive management dialog: `src/gui/driveselectiondialog.h`
- Sync folder management: `src/gui/folderman.h`
- Preferences dialog: `src/gui/preferencesdialog.h`
- Theme/style engine: `src/gui/guiutility.h`, `resources/style/`

## Patterns & Conventions
- **No sync logic in the GUI.** All operations are dispatched to the server via IPC jobs (`src/gui/requests/`). The GUI only displays state.
- **Qt signals/slots** for all async communication. Use `connect()` with `Qt::QueuedConnection` for cross-thread signals.
- **Dialogs** inherit `QDialog`. Use `exec()` for modal, `show()` for non-modal. See `src/gui/adddrivestepdialog.h` for a reference dialog.
- **Wizards** inherit `QWizard`. Reference: `src/gui/wizard/owssetuppage.h`.
- **Theming:** Colors and fonts come from `resources/style/` stylesheets and the Qt theme in `libcommon/theme/`. Don't hardcode hex colors.
- **Icons:** Use resource aliases from `client.qrc`, never filesystem paths.
- **Platform-specific GUI:** Suffix `_mac.mm` / `_win.cpp` for platform code. See `src/gui/guiutility_mac.mm`.
- DO: Keep dialogs thin — fetch data via IPC, display it, send commands back.
- DON'T: Block the GUI thread. All network/IO must go through the server process.

## JIT Index Hints
```bash
# Find all dialog classes
rg -n "class .*Dialog.*: public QDialog" src/gui/ -g "*.h"

# Find all wizard pages
rg -n "class .*Page.*: public QWizardPage" src/gui/ -g "*.h"

# Find IPC request job classes in GUI
rg -n "class .*Job" src/gui/requests/ -g "*.h"

# Find tray menu actions
rg -n "addAction\|QAction" src/gui/systray.cpp

# Find stylesheet files
rg --files resources/style/ -g "*.qss"
```

## Common Gotchas
- The GUI process can be restarted independently of the server. GUI state must be fully reconstructable from server IPC responses.
- `QLabel::setOpenExternalLinks(true)` must be set explicitly for clickable URLs in labels.
- HiDPI icons: provide `@2x` variants in `resources/` and reference via `.qrc`.
- macOS: native menu bar integration requires `QMenuBar` to be the application menu bar, not a widget menu bar.

## Pre-PR Checks
```bash
# Build GUI only
cmake --build build-macos --target kDrive --parallel
# Check for missing Qt resources
rg -n ":/[a-zA-Z]" src/gui/ -g "*.cpp" | grep -v ".qrc"
```
