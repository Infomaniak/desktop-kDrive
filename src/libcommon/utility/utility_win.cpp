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

#include "libcommon/utility/utility.h"
#include "libcommon/utility/types.h"

#include <shlobj.h>
#include <winbase.h>
#include <windows.h>
#include <winerror.h>
#include <shlguid.h>
#include <string>

#include <QLibrary>
#include <QFile>
#include <QDir>
#include <QSettings>
#include <QVariant>
#include <QCoreApplication>
#include <sentry.h>

static const char systemRunPathC[] = "HKEY_LOCAL_MACHINE\\Software\\Microsoft\\Windows\\CurrentVersion\\Run";
static const char runPathC[] = "HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run";
static const char themePathC[] = "HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize";
static const char lightThemeKeyC[] = "SystemUsesLightTheme";

namespace KDC {

static SyncPath getAppSupportDir_private() {
    if (PWSTR path = nullptr; SUCCEEDED(SHGetKnownFolderPath(FOLDERID_LocalAppData, KF_FLAG_NO_ALIAS, nullptr, &path))) {
        SyncPath appDataPath = path;
        CoTaskMemFree(path);
        return appDataPath;
    }
    sentry::Handler::captureMessage(sentry::Level::Warning, "Utility_win::getAppSupportDir_private",
                                    "Fail to get AppSupportDir through SHGetKnownFolderPath, using fallback method");

    return std::filesystem::temp_directory_path().parent_path().parent_path().native();
}

static KDC::SyncPath getAppDir_private() {
    return "";
}

static inline bool hasDarkSystray_private() {
    const QString themePath = QLatin1String(themePathC);
    const QSettings settings(themePath, QSettings::NativeFormat);

    return !settings.value(QLatin1String(lightThemeKeyC), true).toBool();
}

// Be careful, some characters have 2 different encodings in Unicode
// For example 'é' can be coded as 0x65 + 0xcc + 0x81  or 0xc3 + 0xa9
bool CommonUtility::normalizedSyncName(const SyncName &name, SyncName &normalizedName,
                                       const UnicodeNormalization normalization) noexcept {
    if (name.empty()) {
        normalizedName = name;
        return true;
    }

    static const uint32_t maxIterations = 10;
    LPWSTR strResult = nullptr;
    const HANDLE hHeap = GetProcessHeap();

    int64_t iSizeEstimated = static_cast<int64_t>(name.length() + 1) * 2;
    for (uint32_t i = 0; i < maxIterations; ++i) {
        if (strResult) {
            (void) HeapFree(hHeap, 0, strResult);
        }
        strResult = (LPWSTR) HeapAlloc(hHeap, 0, iSizeEstimated * sizeof(WCHAR));
        iSizeEstimated = NormalizeString(normalization == UnicodeNormalization::NFD ? NormalizationD : NormalizationC,
                                         name.c_str(), -1, strResult, iSizeEstimated);

        if (iSizeEstimated > 0) {
            break; // success
        }

        if (iSizeEstimated <= 0) {
            if (GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
                // Real error, not buffer error
                return false;
            }

            // New guess is negative of the return value.
            iSizeEstimated = -iSizeEstimated;
        }
    }

    if (iSizeEstimated <= 0) {
        return false;
    }

    (void) normalizedName.assign(strResult, iSizeEstimated - 1);
    (void) HeapFree(hHeap, 0, strResult);
    return true;
}

std::string CommonUtility::toUnsafeStr(const SyncName &name) {
    std::string unsafeName(name.begin(), name.end());
    return unsafeName;
}

} // namespace KDC
