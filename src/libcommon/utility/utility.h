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
#include "utility_base.h"

#include <string>
#include <thread>

#ifdef _WIN32
#include <strsafe.h>
#endif

#include <QByteArray>
#include <QCoreApplication>
#include <QDataStream>
#include <QIODevice>
#include <QThread>

#include <log4cplus/log4cplus.h>

namespace KDC {
struct COMMON_EXPORT CommonUtility {
        enum IconType { MAIN_FOLDER_ICON, COMMON_DOCUMENT_ICON, DROP_BOX_ICON, NORMAL_FOLDER_ICON };

        static inline const QString linkStyle = QString("color:#0098FF; font-weight:450; text-decoration:none;");

        static const int logsPurgeRate; // Delay after which the logs are purged, expressed in days
        static const int logMaxSize;

        static QString getIconPath(IconType iconType);
        static SyncPath _workingDirPath;

        static bool hasDarkSystray();
        static bool setFolderCustomIcon(const QString &folderPath, IconType iconType);

        static std::string generateRandomStringAlphaNum(int length = 10);
        static std::string generateRandomStringPKCE(int length = 10);

        static QString fileSystemName(const QString &dirPath);

        static qint64 freeDiskSpace(const QString &path);
        static void crash();
        static QString platformName();
        static Platform platform();
        static QString platformArch();
        static const std::string &userAgentString();
        static const std::string &currentVersion();

        static QByteArray toQByteArray(qint32 source);
        static int toInt(QByteArray source);
        static QString escape(const QString &in);
        static bool stringToAppStateValue(const std::string &value, AppStateValue &appStateValue);
        static bool appStateValueToString(const AppStateValue &appStateValue, std::string &value);
        static std::string appStateKeyToString(const AppStateKey &appStateValue) noexcept;

        static bool compressFile(const std::wstring &originalName, const std::wstring &targetName,
                                 const std::function<bool(int)> &progressCallback = nullptr);
        static bool compressFile(const std::string &originalName, const std::string &targetName,
                                 const std::function<bool(int)> &progressCallback = nullptr);
        static bool compressFile(const QString &originalName, const QString &targetName,
                                 const std::function<bool(int)> &progressCallback = nullptr);

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

        static bool isFileSizeMismatchDetectionEnabled();
        static size_t maxPathLength();

        static bool isSubDir(const SyncPath &path1, const SyncPath &path2);

        static const std::string dbVersionNumber(const std::string &dbVersion);
        static bool isVersionLower(const std::string &currentVersion, const std::string &targetVersion);

        static bool dirNameIsValid(const SyncName &name);
        static bool fileNameIsValid(const SyncName &name);

#ifdef __APPLE__
        static const std::string loginItemAgentId();
        static const std::string liteSyncExtBundleId();
        static bool isLiteSyncExtEnabled();
        static bool isLiteSyncExtFullDiskAccessAuthOk(std::string &errorDescr);
#endif
        static std::string envVarValue(const std::string &name);
        static std::string envVarValue(const std::string &name, bool &isSet);

        static void handleSignals(void (*sigHandler)(int));
        static SyncPath signalFilePath(AppType appType, SignalCategory signalCategory);
        static void writeSignalFile(AppType appType, SignalType signalType) noexcept;
        static void clearSignalFile(AppType appType, SignalCategory signalCategory, SignalType &signalType) noexcept;


#ifdef _WIN32
        // Converts a std::wstring to std::string assuming that it contains only mono byte chars
        static std::string toUnsafeStr(const SyncName &name);

        static bool isLikeFileNotFoundError(const std::error_code &ec) noexcept {
            return utility_base::isLikeFileNotFoundError(ec);
        };

        static std::wstring getErrorMessage(DWORD errorMessageId) { return utility_base::getErrorMessage(errorMessageId); }
        static std::wstring getLastErrorMessage() { return utility_base::getLastErrorMessage(); };
        static bool isLikeFileNotFoundError(DWORD dwError) noexcept { return utility_base::isLikeFileNotFoundError(dwError); };
#endif

    private:
        static void extractIntFromStrVersion(const std::string &version, std::vector<int> &tabVersion);
};

struct COMMON_EXPORT StdLoggingThread : public std::thread {
        template<class... Args>
        explicit StdLoggingThread(void (*runFct)(Args...), Args &&...args) :
            std::thread(
                    [=]() {
                        runFct(args...);
                        log4cplus::threadCleanup();
                    },
                    args...) {}
};

struct COMMON_EXPORT QtLoggingThread : public QThread {
        void run() override {
            QThread::run();
            log4cplus::threadCleanup();
        }
};

struct ArgsReader {
        template<class... Args>
        explicit ArgsReader(Args... args) : stream(&params, QIODevice::WriteOnly) {
            read(args...);
        }

        template<class T>
        void read(const T p) {
            stream << p;
        }

        template<class T, class... Args>
        void read(const T p, Args... args) {
            stream << p;
            read(args...);
        }

        explicit operator QByteArray() const { return params; }
        QByteArray params;
        QDataStream stream;
};

struct ArgsWriter {
        explicit ArgsWriter(const QByteArray &results) : stream{QDataStream(results)} {};

        template<class T>
        void write(T &r) {
            stream >> r;
        }

        template<class T, class... Args>
        void write(T &r, Args &...args) {
            stream >> r;
            write(args...);
        }

        QDataStream stream;
};
} // namespace KDC
