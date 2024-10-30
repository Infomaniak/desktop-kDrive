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
#include "libcommon/asserts.h"
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
    SentryHandler::instance()->captureMessage(SentryLevel::Warning, "Utility_win::getAppSupportDir_private",
                                              "Fail to get AppSupportDir through SHGetKnownFolderPath, using fallback method");
    return std::filesystem::temp_directory_path().parent_path().parent_path().native();
}

static KDC::SyncPath getAppDir_private() {
    return "";
}

static inline bool hasDarkSystray_private() {
    QString themePath = QLatin1String(themePathC);
    QSettings settings(themePath, QSettings::NativeFormat);
    return !settings.value(QLatin1String(lightThemeKeyC), true).toBool();
}

bool CommonUtility::fileExists(DWORD dwError) noexcept {
    return (dwError != ERROR_FILE_NOT_FOUND) && (dwError != ERROR_PATH_NOT_FOUND) && (dwError != ERROR_INVALID_DRIVE) &&
           (dwError != ERROR_BAD_NETPATH);
}

bool CommonUtility::fileExists(const std::error_code &code) noexcept {
    return fileExists(static_cast<DWORD>(code.value()));
}

std::wstring CommonUtility::getErrorMessage(DWORD errorMessageID) {
    if (errorMessageID == 0) return {};

    LPWSTR messageBuffer = nullptr;
    const size_t size =
            FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL,
                           errorMessageID, NULL, (LPWSTR) &messageBuffer, 0, NULL);

    // Escape quotes
    const auto msg = std::wstring(messageBuffer, size);
    std::wostringstream message;
    message << errorMessageID << L" - " << msg;

    LocalFree(messageBuffer);

    return message.str();
}

std::wstring CommonUtility::getLastErrorMessage() {
    return getErrorMessage(GetLastError());
}

} // namespace KDC
