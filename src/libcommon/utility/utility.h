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

#include "libcommon/libcommon.h"
#include "types.h"
#include "utility_base.h"

#include <string>
#include <thread>
#include <random>

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
enum class IconType { MAIN_FOLDER_ICON, COMMON_DOCUMENT_ICON, DROP_BOX_ICON, NORMAL_FOLDER_ICON };

struct CommonUtilityBase {
        virtual const QString linkStyle();

        virtual const int logsPurgeRate(); // Delay after which the logs are purged, expressed in days
        virtual const int logMaxSize();

        virtual QString getIconPath(IconType iconType);

        virtual bool hasDarkSystray();
        virtual bool setFolderCustomIcon(const QString &folderPath, IconType iconType);

        virtual std::string generateRandomStringAlphaNum(int length = 10);
        virtual std::string generateRandomStringPKCE(int length = 10);

        virtual QString fileSystemName(const QString &dirPath);
        virtual qint64 freeDiskSpace(const QString &path);
        virtual void crash();
        virtual QString platformName();
        virtual Platform platform();
        virtual QString platformArch();
        virtual const std::string &userAgentString();
        virtual const std::string &currentVersion();

        virtual QByteArray toQByteArray(qint32 source);
        virtual int toInt(QByteArray source);
        virtual QString escape(const QString &in);
        virtual bool stringToAppStateValue(const std::string &value, AppStateValue &appStateValue);
        virtual bool appStateValueToString(const AppStateValue &appStateValue, std::string &value);
        virtual std::string appStateKeyToString(const AppStateKey &appStateValue) noexcept;

        virtual bool compressFile(const std::wstring &originalName, const std::wstring &targetName,
                          const std::function<bool(int)> &progressCallback = nullptr);
        virtual bool compressFile(const std::string &originalName, const std::string &targetName,
                          const std::function<bool(int)> &progressCallback = nullptr);
        virtual bool compressFile(const QString &originalName, const QString &targetName,
                          const std::function<bool(int)> &progressCallback = nullptr);

        virtual const QString englishCode() { return "en"; }
        virtual const QString frenchCode() { return "fr"; }
        virtual const QString germanCode() { return "de"; }
        virtual const QString spanishCode() { return "es"; }
        virtual const QString italianCode() { return "it"; }
        virtual QString languageCode(Language language);
        virtual QStringList languageCodeList(Language enforcedLocale);
        virtual void setupTranslations(QCoreApplication *app, Language enforcedLocale);
        virtual bool languageCodeIsEnglish(const QString &languageCode);

        // Color threshold check
        virtual bool colorThresholdCheck(int red, int green, int blue);

        virtual SyncPath relativePath(const SyncPath &rootPath, const SyncPath &path);

        virtual SyncPath getAppDir();
        virtual SyncPath getAppSupportDir();
        virtual void setAppWorkingDir(const SyncPath &workingDirPath) { _workingDirPath = workingDirPath; }
        virtual SyncPath getAppWorkingDir();

        virtual QString getFileIconPathFromFileName(const QString &fileName, NodeType type);

        virtual QString getRelativePathFromHome(const QString &dirPath);

        virtual bool isFileSizeMismatchDetectionEnabled();
        virtual size_t maxPathLength();

        virtual bool isSubDir(const SyncPath &path1, const SyncPath &path2);

        virtual const std::string dbVersionNumber(const std::string &dbVersion);
        virtual bool isVersionLower(const std::string &currentVersion, const std::string &targetVersion);

        virtual bool dirNameIsValid(const SyncName &name);
        virtual bool fileNameIsValid(const SyncName &name);

#ifdef __APPLE__
        virtual const std::string loginItemAgentId();
        virtual const std::string liteSyncExtBundleId();
        virtual bool isLiteSyncExtEnabled();
        virtual bool isLiteSyncExtFullDiskAccessAuthOk(std::string &errorDescr);
#endif
        virtual std::string envVarValue(const std::string &name);
        virtual std::string envVarValue(const std::string &name, bool &isSet);

        virtual void handleSignals(void (*sigHandler)(int));
        virtual SyncPath signalFilePath(AppType appType, SignalCategory signalCategory);
        virtual void writeSignalFile(AppType appType, SignalType signalType) noexcept;
        virtual void clearSignalFile(AppType appType, SignalCategory signalCategory, SignalType &signalType) noexcept;

        virtual bool isLikeFileNotFoundError(const std::error_code &ec) noexcept { return utility_base::isLikeFileNotFoundError(ec); };


#ifdef _WIN32
        // Converts a std::wstring to std::string assuming that it contains only mono byte chars
        virtual std::string toUnsafeStr(const SyncName &name);

        virtual std::wstring getErrorMessage(DWORD errorMessageId) { return utility_base::getErrorMessage(errorMessageId); }
        virtual std::wstring getLastErrorMessage() { return utility_base::getLastErrorMessage(); };
        virtual bool isLikeFileNotFoundError(DWORD dwError) noexcept { return utility_base::isLikeFileNotFoundError(dwError); };
#endif

        virtual QString truncateLongLogMessage(const QString &message);


    private:
        std::mutex _generateRandomStringMutex;
        static SyncPath _workingDirPath;
        std::string generateRandomString(const char *charArray, std::uniform_int_distribution<int> &distrib,
                                         const int length = 10);

        void extractIntFromStrVersion(const std::string &version, std::vector<int> &tabVersion);
};

struct COMMON_EXPORT CommonUtility {
        static QString linkStyle() { return _instance->linkStyle(); }
        static int logsPurgeRate() {
            return _instance->logsPurgeRate();
        } // Delay after which the logs are purged, expressed in days
        static int logMaxSize() { return _instance->logMaxSize(); }

        static QString getIconPath(IconType iconType) { return _instance->getIconPath(iconType); }

        static bool hasDarkSystray() { return _instance->hasDarkSystray(); }
        static bool setFolderCustomIcon(const QString &folderPath, IconType iconType) {
            return _instance->setFolderCustomIcon(folderPath, iconType);
        }

        static std::string generateRandomStringAlphaNum(int length = 10) {
            return _instance->generateRandomStringAlphaNum(length);
        }
        static std::string generateRandomStringPKCE(int length = 10) { return _instance->generateRandomStringPKCE(length); }

        static QString fileSystemName(const QString &dirPath) { return _instance->fileSystemName(dirPath); }
        static qint64 freeDiskSpace(const QString &path) { return _instance->freeDiskSpace(path); }
        static void crash() { _instance->crash(); }
        static QString platformName() { return _instance->platformName(); }
        static Platform platform() { return _instance->platform(); }
        static QString platformArch() { return _instance->platformArch(); }
        static const std::string &userAgentString() { return _instance->userAgentString(); }
        static const std::string &currentVersion() { return _instance->currentVersion(); }

        static QByteArray toQByteArray(qint32 source) { return _instance->toQByteArray(source); }
        static int toInt(QByteArray source) { return _instance->toInt(source); }
        static QString escape(const QString &in) { return _instance->escape(in); }
        static bool stringToAppStateValue(const std::string &value, AppStateValue &appStateValue) {
            return _instance->stringToAppStateValue(value, appStateValue);
        }
        static bool appStateValueToString(const AppStateValue &appStateValue, std::string &value) {
            return _instance->appStateValueToString(appStateValue, value);
        }
        static std::string appStateKeyToString(const AppStateKey &appStateValue) noexcept {
            return _instance->appStateKeyToString(appStateValue);
        }

        static bool compressFile(const std::wstring &originalName, const std::wstring &targetName,
                                 const std::function<bool(int)> &progressCallback = nullptr) {
            return _instance->compressFile(originalName, targetName, progressCallback);
        }
        static bool compressFile(const std::string &originalName, const std::string &targetName,
                                 const std::function<bool(int)> &progressCallback = nullptr) {
            return _instance->compressFile(originalName, targetName, progressCallback);
        }
        static bool compressFile(const QString &originalName, const QString &targetName,
                                 const std::function<bool(int)> &progressCallback = nullptr) {
            return _instance->compressFile(originalName, targetName, progressCallback);
        }

        static QString englishCode() { return _instance->englishCode(); }
        static QString frenchCode() { return _instance->frenchCode(); }
        static QString germanCode() { return _instance->germanCode(); }
        static QString spanishCode() { return _instance->spanishCode(); }
        static QString italianCode() { return _instance->italianCode(); }
        static QString languageCode(const Language language) { return _instance->languageCode(language); }
        static QStringList languageCodeList(const Language enforcedLocale) { return _instance->languageCodeList(enforcedLocale); }
        static void setupTranslations(QCoreApplication *app, const Language enforcedLocale) {
            _instance->setupTranslations(app, enforcedLocale);
        }
        static bool languageCodeIsEnglish(const QString &languageCode) { return _instance->languageCodeIsEnglish(languageCode); }

        // Color threshold check
        static bool colorThresholdCheck(int red, int green, int blue) { return _instance->colorThresholdCheck(red, green, blue); }

        static SyncPath relativePath(const SyncPath &rootPath, const SyncPath &path) {
            return _instance->relativePath(rootPath, path);
        }

        static SyncPath getAppDir() { return _instance->getAppDir(); }
        static SyncPath getAppSupportDir() { return _instance->getAppSupportDir(); }
        static void setAppWorkingDir(const SyncPath &path) { return _instance->setAppWorkingDir(path); }
        static SyncPath getAppWorkingDir() { return _instance->getAppWorkingDir(); }

        static QString getFileIconPathFromFileName(const QString &fileName, NodeType type) {
            return _instance->getFileIconPathFromFileName(fileName, type);
        }

        static QString getRelativePathFromHome(const QString &dirPath) { return _instance->getRelativePathFromHome(dirPath); }

        static bool isFileSizeMismatchDetectionEnabled() { return _instance->isFileSizeMismatchDetectionEnabled(); }
        static size_t maxPathLength() { return _instance->maxPathLength(); }

        static bool isSubDir(const SyncPath &path1, const SyncPath &path2) { return _instance->isSubDir(path1, path2); }

        static const std::string dbVersionNumber(const std::string &dbVersion) { return _instance->dbVersionNumber(dbVersion); }
        static bool isVersionLower(const std::string &currentVersion, const std::string &targetVersion) {
            return _instance->isVersionLower(currentVersion, targetVersion);
        }

        static bool dirNameIsValid(const SyncName &name) { return _instance->dirNameIsValid(name); }
        static bool fileNameIsValid(const SyncName &name) { return _instance->fileNameIsValid(name); }

#ifdef __APPLE__
        static std::string loginItemAgentId() { return _instance->loginItemAgentId(); }
        static std::string liteSyncExtBundleId() { return _instance->liteSyncExtBundleId(); }
        static bool isLiteSyncExtEnabled() { return _instance->isLiteSyncExtEnabled(); }
        static bool isLiteSyncExtFullDiskAccessAuthOk(std::string &errorDescr) {
            return _instance->isLiteSyncExtFullDiskAccessAuthOk(errorDescr);
        }
#endif
        static std::string envVarValue(const std::string &name) { return _instance->envVarValue(name); }
        static std::string envVarValue(const std::string &name, bool &isSet) { return _instance->envVarValue(name, isSet); }

        static void handleSignals(void (*sigHandler)(int)) { _instance->handleSignals(sigHandler); }
        static SyncPath signalFilePath(AppType appType, SignalCategory signalCategory) {
            return _instance->signalFilePath(appType, signalCategory);
        }
        static void writeSignalFile(AppType appType, SignalType signalType) noexcept {
            _instance->writeSignalFile(appType, signalType);
        }
        static void clearSignalFile(AppType appType, SignalCategory signalCategory, SignalType &signalType) noexcept {
            _instance->clearSignalFile(appType, signalCategory, signalType);
        }

        static bool isLikeFileNotFoundError(const std::error_code &ec) noexcept {
            return _instance->isLikeFileNotFoundError(ec);
        };


#ifdef _WIN32
        // Converts a std::wstring to std::string assuming that it contains only mono byte chars
        static std::string toUnsafeStr(const SyncName &name) { return _instance->toUnsafeStr(name); }

        static std::wstring getErrorMessage(DWORD errorMessageId) { return _instance->getErrorMessage(errorMessageId); }
        static std::wstring getLastErrorMessage() { return _instance->getLastErrorMessage(); };
        static bool isLikeFileNotFoundError(DWORD dwError) noexcept { return _instance->isLikeFileNotFoundError(dwError); };
#endif

        static QString truncateLongLogMessage(const QString &message) { return _instance->truncateLongLogMessage(message); }

        static void setCustomCommonUtility(std::shared_ptr<CommonUtilityBase> instance) { _instance = std::move(instance); }

    private:
        static std::shared_ptr<CommonUtilityBase> _instance;
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
