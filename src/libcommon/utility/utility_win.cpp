/*
 * Infomaniak kDrive - Desktop
 * Copyright (C) 2023-2024 Infomaniak Network SA
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
} // namespace KDC
