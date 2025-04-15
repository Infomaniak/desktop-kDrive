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

#include "config.h"

#include "common/utility.h"
#include "libcommon/utility/utility.h"

// Note:  This file must compile without QtGui
#include <QDir>
#include <QDateTime>
#include <QUuid>

#ifdef Q_OS_UNIX
#include <sys/statvfs.h>
#include <sys/types.h>
#include <unistd.h>
#endif

#if defined(Q_OS_WIN)
#include <fileapi.h>
#endif

#include <math.h>
#include <stdarg.h>

namespace KDC {

#ifdef Q_OS_WIN
void OldUtility::setFolderPinState(const QUuid &clsid, bool show) {
    QString clsidStr = clsid.toString();
    QString clsidPath = QString() % "Software\\Classes\\CLSID\\" % clsidStr;
    QString clsidPathWow64 = QString() % "Software\\Classes\\Wow6432Node\\CLSID\\" % clsidStr;

    QString error;
    if (OldUtility::registryExistKeyTree(HKEY_CURRENT_USER, clsidPath)) {
        OldUtility::registrySetKeyValue(HKEY_CURRENT_USER, clsidPath, "System.IsPinnedToNameSpaceTree", REG_DWORD, show, error);
    }

    if (OldUtility::registryExistKeyTree(HKEY_CURRENT_USER, clsidPathWow64)) {
        OldUtility::registrySetKeyValue(HKEY_CURRENT_USER, clsidPathWow64, "System.IsPinnedToNameSpaceTree", REG_DWORD, show,
                                        error);
    }
}
#endif

qint64 OldUtility::qDateTimeToTime_t(const QDateTime &t) {
    return t.toMSecsSinceEpoch() / 1000;
}

/* --------------------------------------------------------------------------- */

#ifdef Q_OS_WIN
// Add legacy sync root keys
void OldUtility::addLegacySyncRootKeys(const QUuid &clsid, const QString &folderPath, bool show) {
    QString clsidStr = clsid.toString();
    QString clsidPath = QString() % "Software\\Classes\\CLSID\\" % clsidStr;
    QString clsidPathWow64 = QString() % "Software\\Classes\\Wow6432Node\\CLSID\\" % clsidStr;
    QString namespacePath = QString() % "Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Desktop\\NameSpace\\" % clsidStr;

    QDir path = QDir(folderPath);
    QString title = path.dirName();
    QString iconPath = QString::fromStdWString(CommonUtility::applicationFilePath().native());
    QString targetFolderPath = QDir::toNativeSeparators(QDir::cleanPath(folderPath));

    // qCInfo(lcUtility) << "Explorer Cloud storage provider: saving path" << targetFolderPath << "to CLSID" << clsidStr;
    //  Steps taken from: https://msdn.microsoft.com/en-us/library/windows/desktop/dn889934%28v=vs.85%29.aspx
    //  Step 1: Add your CLSID and name your extension
    QString error;
    OldUtility::registrySetKeyValue(HKEY_CURRENT_USER, clsidPath, QString(), REG_SZ, title, error);
    OldUtility::registrySetKeyValue(HKEY_CURRENT_USER, clsidPathWow64, QString(), REG_SZ, title, error);
    // Step 2: Set the image for your icon
    OldUtility::registrySetKeyValue(HKEY_CURRENT_USER, clsidPath + QStringLiteral("\\DefaultIcon"), QString(), REG_SZ, iconPath,
                                    error);
    OldUtility::registrySetKeyValue(HKEY_CURRENT_USER, clsidPathWow64 + QStringLiteral("\\DefaultIcon"), QString(), REG_SZ,
                                    iconPath, error);
    // Step 3: Add your extension to the Navigation Pane and make it visible
    OldUtility::registrySetKeyValue(HKEY_CURRENT_USER, clsidPath, QStringLiteral("System.IsPinnedToNameSpaceTree"), REG_DWORD,
                                    show, error);
    OldUtility::registrySetKeyValue(HKEY_CURRENT_USER, clsidPathWow64, QStringLiteral("System.IsPinnedToNameSpaceTree"),
                                    REG_DWORD, show, error);
    // Step 4: Set the location for your extension in the Navigation Pane
    OldUtility::registrySetKeyValue(HKEY_CURRENT_USER, clsidPath, QStringLiteral("SortOrderIndex"), REG_DWORD, 0x41, error);
    OldUtility::registrySetKeyValue(HKEY_CURRENT_USER, clsidPathWow64, QStringLiteral("SortOrderIndex"), REG_DWORD, 0x41, error);
    // Step 5: Provide the dll that hosts your extension.
    OldUtility::registrySetKeyValue(HKEY_CURRENT_USER, clsidPath + QStringLiteral("\\InProcServer32"), QString(), REG_EXPAND_SZ,
                                    QStringLiteral("%systemroot%\\system32\\shell32.dll"), error);
    OldUtility::registrySetKeyValue(HKEY_CURRENT_USER, clsidPathWow64 + QStringLiteral("\\InProcServer32"), QString(),
                                    REG_EXPAND_SZ, QStringLiteral("%systemroot%\\system32\\shell32.dll"), error);
    // Step 6: Define the instance object
    // Indicate that your namespace extension should function like other file folder structures in File Explorer.
    OldUtility::registrySetKeyValue(HKEY_CURRENT_USER, clsidPath + QStringLiteral("\\Instance"), QStringLiteral("CLSID"), REG_SZ,
                                    QStringLiteral("{0E5AAE11-A475-4c5b-AB00-C66DE400274E}"), error);
    OldUtility::registrySetKeyValue(HKEY_CURRENT_USER, clsidPathWow64 + QStringLiteral("\\Instance"), QStringLiteral("CLSID"),
                                    REG_SZ, QStringLiteral("{0E5AAE11-A475-4c5b-AB00-C66DE400274E}"), error);
    // Step 7: Provide the file system attributes of the target folder
    OldUtility::registrySetKeyValue(HKEY_CURRENT_USER, clsidPath + QStringLiteral("\\Instance\\InitPropertyBag"),
                                    QStringLiteral("Attributes"), REG_DWORD, 0x11, error);
    OldUtility::registrySetKeyValue(HKEY_CURRENT_USER, clsidPathWow64 + QStringLiteral("\\Instance\\InitPropertyBag"),
                                    QStringLiteral("Attributes"), REG_DWORD, 0x11, error);
    // Step 8: Set the path for the sync root
    OldUtility::registrySetKeyValue(HKEY_CURRENT_USER, clsidPath + QStringLiteral("\\Instance\\InitPropertyBag"),
                                    QStringLiteral("TargetFolderPath"), REG_SZ, targetFolderPath, error);
    OldUtility::registrySetKeyValue(HKEY_CURRENT_USER, clsidPathWow64 + QStringLiteral("\\Instance\\InitPropertyBag"),
                                    QStringLiteral("TargetFolderPath"), REG_SZ, targetFolderPath, error);
    // Step 9: Set appropriate shell flags
    OldUtility::registrySetKeyValue(HKEY_CURRENT_USER, clsidPath + QStringLiteral("\\ShellFolder"),
                                    QStringLiteral("FolderValueFlags"), REG_DWORD, 0x28, error);
    OldUtility::registrySetKeyValue(HKEY_CURRENT_USER, clsidPathWow64 + QStringLiteral("\\ShellFolder"),
                                    QStringLiteral("FolderValueFlags"), REG_DWORD, 0x28, error);
    // Step 10: Set the appropriate flags to control your shell behavior
    OldUtility::registrySetKeyValue(HKEY_CURRENT_USER, clsidPath + QStringLiteral("\\ShellFolder"), QStringLiteral("Attributes"),
                                    REG_DWORD, 0xF080004D, error);
    OldUtility::registrySetKeyValue(HKEY_CURRENT_USER, clsidPathWow64 + QStringLiteral("\\ShellFolder"),
                                    QStringLiteral("Attributes"), REG_DWORD, 0xF080004D, error);
    // Step 11: Register your extension in the namespace root
    OldUtility::registrySetKeyValue(HKEY_CURRENT_USER, namespacePath, QString(), REG_SZ, title, error);
    // Step 12: Hide your extension from the Desktop
    OldUtility::registrySetKeyValue(
            HKEY_CURRENT_USER,
            QStringLiteral("Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\HideDesktopIcons\\NewStartPanel"), clsidStr,
            REG_DWORD, 0x1, error);

    // For us, to later be able to iterate and find our own namespace entries and associated CLSID.
    // Use the macro instead of the theme to make sure it matches with the uninstaller.
    OldUtility::registrySetKeyValue(HKEY_CURRENT_USER, namespacePath, QStringLiteral("ApplicationName"), REG_SZ,
                                    QLatin1String(APPLICATION_NAME), error);
}

// Remove legacy sync root keys
void OldUtility::removeLegacySyncRootKeys(const QUuid &clsid) {
    QString clsidStr = clsid.toString();
    QString clsidPath = QString() % "Software\\Classes\\CLSID\\" % clsidStr;
    QString clsidPathWow64 = QString() % "Software\\Classes\\Wow6432Node\\CLSID\\" % clsidStr;
    QString namespacePath = QString() % "Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Desktop\\NameSpace\\" % clsidStr;
    QString newstartpanelPath =
            QString() % "Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\HideDesktopIcons\\NewStartPanel";

    if (OldUtility::registryExistKeyTree(HKEY_CURRENT_USER, clsidPath)) {
        OldUtility::registryDeleteKeyTree(HKEY_CURRENT_USER, clsidPath);
    }
    if (OldUtility::registryExistKeyTree(HKEY_CURRENT_USER, clsidPathWow64)) {
        OldUtility::registryDeleteKeyTree(HKEY_CURRENT_USER, clsidPathWow64);
    }
    if (OldUtility::registryExistKeyTree(HKEY_CURRENT_USER, namespacePath)) {
        OldUtility::registryDeleteKeyTree(HKEY_CURRENT_USER, namespacePath);
    }
    if (OldUtility::registryExistKeyValue(HKEY_CURRENT_USER, newstartpanelPath, clsidStr)) {
        OldUtility::registryDeleteKeyValue(HKEY_CURRENT_USER, newstartpanelPath, clsidStr);
    }
}

#endif

} // namespace KDC
