/*
 * Infomaniak kDrive - Desktop
 * Copyright (C) 2023-2025 Infomaniak Network SA
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#pragma once
#include <windows.h>
#include <shellapi.h>
namespace KDC {
struct OverlayIconsHelper {
        static bool areOverlayIconsVisible() {
            auto KDOverlaysDll = LoadLibrary(kdoverlaysDllPath().c_str());
            if (!KDOverlaysDll) {
                // TODO log
                return true;
            }
            auto areIconsVisibleMethod = GetProcAddress(KDOverlaysDll, "AreIconsVisible");
            if (!areIconsVisibleMethod) {
                // TODO log
                return true;
            }

            return areIconsVisibleMethod();
        }
        static void fixOverlaysIcons() {
            // Unregister and register the DLL will bring kDrive Icons to the top of the list (see KDOverlayRegistrationHandler)

            const auto dllPath = kdoverlaysDllPath();
            std::wstring parameters = (dllPath.wstring() + std::wstring(L" /u /s"));
            SHELLEXECUTEINFOW sei = {sizeof(sei)};
            sei.lpVerb = L"runas"; // Causes UAC prompt
            sei.lpFile = L"regsvr32.exe";
            sei.lpParameters = parameters.c_str();
            sei.hwnd = NULL;
            sei.nShow = SW_NORMAL;

            if (!ShellExecuteExW(&sei)) {
                return;
            }

            parameters = (dllPath.wstring() + std::wstring(L" /s"));
            sei.lpParameters = parameters.c_str();
            if (!ShellExecuteExW(&sei)) {
            }
        }

    private:
        static SyncPath kdoverlaysDllPath() {
            WCHAR path[MAX_PATH];
            GetModuleFileNameW(NULL, path, MAX_PATH);
            return SyncPath(path).parent_path() / L"KDOverlays.dll";
        }
};
} // namespace KDC
