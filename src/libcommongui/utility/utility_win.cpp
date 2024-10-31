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

#include <shlobj.h>
#include <winbase.h>
#include <windows.h>
#include <winerror.h>
#include <shlguid.h>

#include <QFile>
#include <QDir>

namespace KDC {

static void setupFavLink_private(const QString &folder) {
    // First create a Desktop.ini so that the folder and favorite link show our application's icon.
    QFile desktopIni(folder + QLatin1String("/Desktop.ini"));
    if (!desktopIni.exists()) {
        desktopIni.open(QFile::WriteOnly);
        desktopIni.write("[.ShellClassInfo]\r\nIconResource=");
        desktopIni.write(QDir::toNativeSeparators(qApp->applicationFilePath()).toUtf8());
        desktopIni.write(",0\r\n");
        desktopIni.close();

        // Set the folder as system and Desktop.ini as hidden+system for explorer to pick it.
        // https://msdn.microsoft.com/en-us/library/windows/desktop/cc144102
        DWORD folderAttrs = GetFileAttributesW((wchar_t *) folder.utf16());
        SetFileAttributesW((wchar_t *) folder.utf16(), folderAttrs | FILE_ATTRIBUTE_SYSTEM);
        SetFileAttributesW((wchar_t *) desktopIni.fileName().utf16(), FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM);
    }

    // Windows Explorer: Place under "Favorites" (Links)
    QString linkName;
    QDir folderDir(QDir::fromNativeSeparators(folder));

    /* Use new WINAPI functions */
    PWSTR path;

    if (SHGetKnownFolderPath(FOLDERID_Links, 0, NULL, &path) == S_OK) {
        QString links = QDir::fromNativeSeparators(QString::fromWCharArray(path));
        linkName = QDir(links).filePath(folderDir.dirName() + QLatin1String(".lnk"));
        CoTaskMemFree(path);
    }
    QFile::link(folder, linkName);
}

} // namespace KDC
