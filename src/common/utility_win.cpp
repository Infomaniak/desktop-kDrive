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

#include "common/utility.h"
#include "libcommon/utility/qlogiffail.h"
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

static const char systemRunPathC[] = "HKEY_LOCAL_MACHINE\\Software\\Microsoft\\Windows\\CurrentVersion\\Run";
static const char runPathC[] = "HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run";
static const char themePathC[] = "HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize";
static const char lightThemeKeyC[] = "SystemUsesLightTheme";

namespace KDC {

bool OldUtility::hasSystemLaunchOnStartup(const QString &appName, log4cplus::Logger logger) {
    QString runPath = QLatin1String(systemRunPathC);
    QSettings settings(runPath, QSettings::NativeFormat);
    return settings.contains(appName);
}

bool OldUtility::hasLaunchOnStartup(const QString &appName, log4cplus::Logger logger) {
    QString runPath = QLatin1String(runPathC);
    QSettings settings(runPath, QSettings::NativeFormat);
    return settings.contains(appName);
}

void OldUtility::setLaunchOnStartup(const QString &appName, const QString &guiName, bool enable, log4cplus::Logger logger) {
    Q_UNUSED(guiName);
    QString runPath = QLatin1String(runPathC);
    QSettings settings(runPath, QSettings::NativeFormat);
    if (enable) {
        QString serverFilePath = QCoreApplication::applicationDirPath().replace('/', '\\') + "\\" + appName + ".exe";
        settings.setValue(appName, serverFilePath);
    } else {
        settings.remove(appName);
    }
}

#include <errno.h>
#include <wtypes.h>

bool OldUtility::registryExistKeyTree(HKEY hRootKey, const QString &subKey) {
    HKEY hKey;

    REGSAM sam = KEY_READ | KEY_WOW64_64KEY;
    LONG result = RegOpenKeyEx(hRootKey, reinterpret_cast<LPCWSTR>(subKey.utf16()), 0, sam, &hKey);
    QLOG_IF_FAIL(result == ERROR_SUCCESS || result == ERROR_FILE_NOT_FOUND)
    if (result != ERROR_SUCCESS && result != ERROR_FILE_NOT_FOUND) return false;

    RegCloseKey(hKey);

    return result == ERROR_SUCCESS;
}

bool OldUtility::registryExistKeyValue(HKEY hRootKey, const QString &subKey, const QString &valueName) {
    QVariant value;

    HKEY hKey;

    REGSAM sam = KEY_READ | KEY_WOW64_64KEY;
    LONG result = RegOpenKeyEx(hRootKey, reinterpret_cast<LPCWSTR>(subKey.utf16()), 0, sam, &hKey);
    QLOG_IF_FAIL(result == ERROR_SUCCESS || result == ERROR_FILE_NOT_FOUND)
    if (result != ERROR_SUCCESS && result != ERROR_FILE_NOT_FOUND) return false;

    DWORD type = 0, sizeInBytes = 0;
    result = RegQueryValueEx(hKey, reinterpret_cast<LPCWSTR>(valueName.utf16()), 0, &type, nullptr, &sizeInBytes);
    QLOG_IF_FAIL(result == ERROR_SUCCESS || result == ERROR_FILE_NOT_FOUND)
    if (result != ERROR_SUCCESS && result != ERROR_FILE_NOT_FOUND) return false;

    RegCloseKey(hKey);

    return result == ERROR_SUCCESS;
}

QVariant OldUtility::registryGetKeyValue(HKEY hRootKey, const QString &subKey, const QString &valueName) {
    QVariant value;

    HKEY hKey;

    REGSAM sam = KEY_READ | KEY_WOW64_64KEY;
    LONG result = RegOpenKeyEx(hRootKey, reinterpret_cast<LPCWSTR>(subKey.utf16()), 0, sam, &hKey);
    QLOG_IF_FAIL(result == ERROR_SUCCESS || result == ERROR_FILE_NOT_FOUND)
    if (result != ERROR_SUCCESS) return value;

    DWORD type = 0, sizeInBytes = 0;
    result = RegQueryValueEx(hKey, reinterpret_cast<LPCWSTR>(valueName.utf16()), 0, &type, nullptr, &sizeInBytes);
    QLOG_IF_FAIL(result == ERROR_SUCCESS || result == ERROR_FILE_NOT_FOUND)
    if (result == ERROR_SUCCESS) {
        switch (type) {
            case REG_DWORD:
                DWORD dword;
                Q_ASSERT(sizeInBytes == sizeof(dword));
                if (RegQueryValueEx(hKey, reinterpret_cast<LPCWSTR>(valueName.utf16()), 0, &type,
                                    reinterpret_cast<LPBYTE>(&dword), &sizeInBytes) == ERROR_SUCCESS) {
                    value = int(dword);
                }
                break;
            case REG_EXPAND_SZ:
            case REG_SZ: {
                QString string;
                string.resize(sizeInBytes / sizeof(QChar));
                result = RegQueryValueEx(hKey, reinterpret_cast<LPCWSTR>(valueName.utf16()), 0, &type,
                                         reinterpret_cast<LPBYTE>(string.data()), &sizeInBytes);

                if (result == ERROR_SUCCESS) {
                    int newCharSize = sizeInBytes / sizeof(QChar);
                    // From the doc:
                    // If the data has the REG_SZ, REG_MULTI_SZ or REG_EXPAND_SZ type, the string may not have been stored with
                    // the proper terminating null characters. Therefore, even if the function returns ERROR_SUCCESS,
                    // the application should ensure that the string is properly terminated before using it; otherwise, it may
                    // overwrite a buffer.
                    if (string.at(newCharSize - 1) == QChar('\0')) string.resize(newCharSize - 1);
                    value = string;
                }
                break;
            }
            default:
                Q_UNREACHABLE();
        }
    }
    QLOG_IF_FAIL(result == ERROR_SUCCESS || result == ERROR_FILE_NOT_FOUND)

    RegCloseKey(hKey);
    return value;
}

bool OldUtility::registrySetKeyValue(HKEY hRootKey, const QString &subKey, const QString &valueName, DWORD type,
                                     const QVariant &value, QString &error) {
    HKEY hKey;
    // KEY_WOW64_64KEY is necessary because CLSIDs are "Redirected and reflected only for CLSIDs that do not specify
    // InprocServer32 or InprocHandler32."
    // https://msdn.microsoft.com/en-us/library/windows/desktop/aa384253%28v=vs.85%29.aspx#redirected__shared__and_reflected_keys_under_wow64
    // This shouldn't be an issue in our case since we use shell32.dll as InprocServer32, so we could write those registry keys
    // for both 32 and 64bit.
    REGSAM sam = KEY_WRITE | KEY_WOW64_64KEY;
    LONG result =
            RegCreateKeyEx(hRootKey, reinterpret_cast<LPCWSTR>(subKey.utf16()), 0, nullptr, 0, sam, nullptr, &hKey, nullptr);
    QLOG_IF_FAIL(result == ERROR_SUCCESS)
    if (result != ERROR_SUCCESS) {
        LPTSTR errorText = NULL;
        if (!FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_IGNORE_INSERTS, NULL,
                           result, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                           (LPTSTR) &errorText, // output
                           0, NULL)) {
            error = "Format message failed";
        } else {
            error = QString::fromWCharArray(errorText);
        }

        return false;
    }

    result = -1;
    switch (type) {
        case REG_DWORD: {
            DWORD dword = value.toInt();
            result = RegSetValueEx(hKey, reinterpret_cast<LPCWSTR>(valueName.utf16()), 0, type,
                                   reinterpret_cast<const BYTE *>(&dword), sizeof(dword));
            break;
        }
        case REG_EXPAND_SZ:
        case REG_SZ: {
            QString string = value.toString();
            result = RegSetValueEx(hKey, reinterpret_cast<LPCWSTR>(valueName.utf16()), 0, type,
                                   reinterpret_cast<const BYTE *>(string.constData()),
                                   ((DWORD) string.size() + 1) * sizeof(QChar));
            break;
        }
        default:
            Q_UNREACHABLE();
    }
    QLOG_IF_FAIL(result == ERROR_SUCCESS)
    if (result != ERROR_SUCCESS) {
        LPTSTR errorText = NULL;
        if (!FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_IGNORE_INSERTS, NULL,
                           result, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                           (LPTSTR) &errorText, // output
                           0, NULL)) {
            error = "Format message failed";
        } else {
            error = *errorText;
        }

        return false;
    }

    RegCloseKey(hKey);
    return result == ERROR_SUCCESS;
}

bool OldUtility::registryDeleteKeyTree(HKEY hRootKey, const QString &subKey) {
    HKEY hKey;
    REGSAM sam = DELETE | KEY_ENUMERATE_SUB_KEYS | KEY_QUERY_VALUE | KEY_SET_VALUE | KEY_WOW64_64KEY;
    LONG result = RegOpenKeyEx(hRootKey, reinterpret_cast<LPCWSTR>(subKey.utf16()), 0, sam, &hKey);
    QLOG_IF_FAIL(result == ERROR_SUCCESS)
    if (result != ERROR_SUCCESS) return false;

    result = RegDeleteTree(hKey, nullptr);
    RegCloseKey(hKey);
    QLOG_IF_FAIL(result == ERROR_SUCCESS)

    result |= RegDeleteKeyEx(hRootKey, reinterpret_cast<LPCWSTR>(subKey.utf16()), sam, 0);
    QLOG_IF_FAIL(result == ERROR_SUCCESS)

    return result == ERROR_SUCCESS;
}

bool OldUtility::registryDeleteKeyValue(HKEY hRootKey, const QString &subKey, const QString &valueName) {
    HKEY hKey;
    REGSAM sam = KEY_WRITE | KEY_WOW64_64KEY;
    LONG result = RegOpenKeyEx(hRootKey, reinterpret_cast<LPCWSTR>(subKey.utf16()), 0, sam, &hKey);
    QLOG_IF_FAIL(result == ERROR_SUCCESS)
    if (result != ERROR_SUCCESS) return false;

    result = RegDeleteValue(hKey, reinterpret_cast<LPCWSTR>(valueName.utf16()));
    QLOG_IF_FAIL(result == ERROR_SUCCESS || result == ERROR_FILE_NOT_FOUND)

    RegCloseKey(hKey);
    return result == ERROR_SUCCESS;
}

bool OldUtility::registryWalkSubKeys(HKEY hRootKey, const QString &subKey,
                                     const std::function<void(HKEY, const QString &)> &callback) {
    HKEY hKey;
    REGSAM sam = KEY_READ | KEY_WOW64_64KEY;
    LONG result = RegOpenKeyEx(hRootKey, reinterpret_cast<LPCWSTR>(subKey.utf16()), 0, sam, &hKey);
    QLOG_IF_FAIL(result == ERROR_SUCCESS)
    if (result != ERROR_SUCCESS) return false;

    DWORD maxSubKeyNameSize;
    // Get the largest keyname size once instead of relying each call on ERROR_MORE_DATA.
    result = RegQueryInfoKey(hKey, nullptr, nullptr, nullptr, nullptr, &maxSubKeyNameSize, nullptr, nullptr, nullptr, nullptr,
                             nullptr, nullptr);
    QLOG_IF_FAIL(result == ERROR_SUCCESS)
    if (result != ERROR_SUCCESS) {
        RegCloseKey(hKey);
        return false;
    }

    QString subKeyName;
    subKeyName.reserve(maxSubKeyNameSize + 1);

    DWORD retCode = ERROR_SUCCESS;
    for (DWORD i = 0; retCode == ERROR_SUCCESS; ++i) {
        Q_ASSERT(unsigned(subKeyName.capacity()) > maxSubKeyNameSize);
        // Make the previously reserved capacity official again.
        subKeyName.resize(subKeyName.capacity());
        DWORD subKeyNameSize = subKeyName.size();
        retCode = RegEnumKeyEx(hKey, i, reinterpret_cast<LPWSTR>(subKeyName.data()), &subKeyNameSize, nullptr, nullptr, nullptr,
                               nullptr);

        QLOG_IF_FAIL(result == ERROR_SUCCESS || retCode == ERROR_NO_MORE_ITEMS)
        if (retCode == ERROR_SUCCESS) {
            // subKeyNameSize excludes the trailing \0
            subKeyName.resize(subKeyNameSize);
            // Pass only the sub keyname, not the full path.
            callback(hKey, subKeyName);
        }
    }

    RegCloseKey(hKey);
    return retCode != ERROR_NO_MORE_ITEMS;
}

void OldUtility::UnixTimeToFiletime(time_t t, FILETIME *filetime) {
    LONGLONG ll = Int32x32To64(t, 10000000) + 116444736000000000;
    filetime->dwLowDateTime = (DWORD) ll;
    filetime->dwHighDateTime = ll >> 32;
}

} // namespace KDC
