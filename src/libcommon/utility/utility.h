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

#pragma once

#include "libcommon/libcommon.h"
#include "types.h"
#include "libcommon/info/nodeinfo.h"

#include <string>

#include <QByteArray>
#include <QCoreApplication>

#ifdef _WIN32
#include <strsafe.h>
#endif

namespace KDC {

struct COMMON_EXPORT CommonUtility {
        enum IconType { MAIN_FOLDER_ICON, COMMON_DOCUMENT_ICON, DROP_BOX_ICON, NORMAL_FOLDER_ICON };

        static inline const QString linkStyle = QString("color:#0098FF; font-weight:450; text-decoration:none;");

        static const int logsPurgeRate;  // Delay after which the logs are purged, expressed in days
        static const int logMaxSize;

        static QString getIconPath(IconType iconType);
        static SyncPath _workingDirPath;

        static bool hasDarkSystray();
        static bool setFolderCustomIcon(const QString &folderPath, IconType iconType);

        static std::string generateRandomStringAlphaNum(const int length = 10);
        static std::string userAgentString();
        static std::string generateRandomStringPKCE(const int length = 10);

        // static const KDC::SyncPath Utility::getAppDir();

        static QString fileSystemName(const QString &dirPath);

        static qint64 freeDiskSpace(const QString &path);
        static void crash();
        static QString platformName();
        static QString platformArch();
        static QByteArray IntToArray(qint32 source);
        static int ArrayToInt(QByteArray source);
        static QString escape(const QString &in);
        static bool stringToAppStateValue(const std::string &value, AppStateValue &appStateValue);
        static bool appStateValueToString(const AppStateValue &appStateValue, std::string &value);
        static std::string appStateKeyToString(const AppStateKey &appStateValue) noexcept;

        static bool compressFile(const std::wstring &originalName, const std::wstring &targetName);
        static bool compressFile(const std::string &originalName, const std::string &targetName);
        static bool compressFile(const QString &originalName, const QString &targetName);

        static QString languageCode(::KDC::Language enforcedLocale);
        static QStringList languageCodeList(::KDC::Language enforcedLocale);
        static void setupTranslations(QCoreApplication *app, ::KDC::Language enforcedLocale);
        static bool languageCodeIsEnglish(const QString &languageCode);

        // Color threshold check
        static bool colorThresholdCheck(int red, int green, int blue);

        static SyncPath relativePath(const SyncPath &rootPath, const SyncPath &path);

        static SyncPath getAppDir();
        static SyncPath getAppSupportDir();
        static SyncPath getAppWorkingDir();

        static QString getFileIconPathFromFileName(const QString &fileName, NodeType type);

        static QString getRelativePathFromHome(const QString &dirPath);

        static size_t maxPathLength();
#if defined(_WIN32)
        static size_t maxPathLengthFolder();
#endif
        static bool isSubDir(const SyncPath &path1, const SyncPath &path2);

        static const std::string dbVersionNumber(const std::string &dbVersion);
        static bool isVersionLower(const std::string &currentVersion, const std::string &targetVersion);

        static bool dirNameIsValid(const SyncName &name);
        static bool fileNameIsValid(const SyncName &name);

#ifdef Q_OS_MAC
        static bool isLiteSyncExtEnabled();
        static bool isLiteSyncExtFullDiskAccessAuthOk(std::string &errorDescr);
#endif

        static std::string envVarValue(const std::string &name);

    private:
        static void extractIntFromStrVersion(const std::string &version, std::vector<int> &tabVersion);
};

}  // namespace KDC
