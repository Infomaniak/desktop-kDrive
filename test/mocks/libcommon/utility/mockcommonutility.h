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

#include "testincludes.h"
#include "utility/types.h"
#include "libcommon/utility/utility.h"

namespace KDC {

class MockCommonUtility : public CommonUtilityBase, std::enable_shared_from_this<MockCommonUtility> {
    public:
        MockCommonUtility() = default;
        virtual ~MockCommonUtility() = default;
        QString linkStyle() override {
            if (_linkStyle) return _linkStyle();
            return CommonUtilityBase().linkStyle();
        }
        int logsPurgeRate() override {
            if (_logsPurgeRate) return _logsPurgeRate();
            return CommonUtilityBase().logsPurgeRate();
        }
        int logMaxSize() override {
            if (_logMaxSize) return _logMaxSize();
            return CommonUtilityBase().logMaxSize();
        }

        QString getIconPath(IconType iconType) override {
            if (_getIconPath) return _getIconPath(iconType);
            return CommonUtilityBase().getIconPath(iconType);
        }

        bool hasDarkSystray() override {
            if (_hasDarkSystray) return _hasDarkSystray();
            return CommonUtilityBase().hasDarkSystray();
        }
        bool setFolderCustomIcon(const QString &folderPath, IconType iconType) override {
            if (_setFolderCustomIcon) return _setFolderCustomIcon(folderPath, iconType);
            return CommonUtilityBase().setFolderCustomIcon(folderPath, iconType);
        }

        std::string generateRandomStringAlphaNum(int length = 10) override {
            if (_generateRandomStringAlphaNum) return _generateRandomStringAlphaNum(length);
            return CommonUtilityBase().generateRandomStringAlphaNum(length);
        }
        std::string generateRandomStringPKCE(int length = 10) override {
            if (_generateRandomStringPKCE) return _generateRandomStringPKCE(length);
            return CommonUtilityBase().generateRandomStringPKCE(length);
        }

        QString fileSystemName(const QString &dirPath) override {
            if (_fileSystemName) return _fileSystemName(dirPath);
            return CommonUtilityBase().fileSystemName(dirPath);
        }

        qint64 freeDiskSpace(const QString &path) override {
            if (_freeDiskSpace) return _freeDiskSpace(path);
            return CommonUtilityBase().freeDiskSpace(path);
        }
        void crash() override {
            if (_crash) _crash();
            CommonUtilityBase().crash();
        }
        QString platformName() override {
            if (_platformName) return _platformName();
            return CommonUtilityBase().platformName();
        }
        Platform platform() override {
            if (_platform) return _platform();
            return CommonUtilityBase().platform();
        }
        QString platformArch() override {
            if (_platformArch) return _platformArch();
            return CommonUtilityBase().platformArch();
        }
        const std::string &userAgentString() override {
            if (_userAgentString) return _userAgentString();
            return CommonUtilityBase().userAgentString();
        }

        const std::string &currentVersion() override {
            if (_currentVersion) return _currentVersion();
            return CommonUtilityBase().currentVersion();
        }

        QByteArray toQByteArray(qint32 source) override {
            if (_toQByteArray) return _toQByteArray(source);
            return CommonUtilityBase().toQByteArray(source);
        }
        int toInt(QByteArray source) override {
            if (_toInt) return _toInt(source);
            return CommonUtilityBase().toInt(source);
        }
        QString escape(const QString &in) override {
            if (_escape) return _escape(in);
            return CommonUtilityBase().escape(in);
        }
        bool stringToAppStateValue(const std::string &value, AppStateValue &appStateValue) override {
            if (_stringToAppStateValue) return _stringToAppStateValue(value, appStateValue);
            return CommonUtilityBase().stringToAppStateValue(value, appStateValue);
        }
        bool appStateValueToString(const AppStateValue &appStateValue, std::string &value) override {
            if (_appStateValueToString) return _appStateValueToString(appStateValue, value);
            return CommonUtilityBase().appStateValueToString(appStateValue, value);
        }

        std::string appStateKeyToString(const AppStateKey &appStateValue) noexcept override {
            if (_appStateKeyToString) return _appStateKeyToString(appStateValue);
            return CommonUtilityBase().appStateKeyToString(appStateValue);
        }

        bool compressFile(const std::wstring &originalName, const std::wstring &targetName,
                          const std::function<bool(int)> &progressCallback = nullptr) override {
            if (_compressFile) return _compressFile(originalName, targetName, progressCallback);
            return CommonUtilityBase().compressFile(originalName, targetName, progressCallback);
        }
        bool compressFile(const std::string &originalName, const std::string &targetName,
                          const std::function<bool(int)> &progressCallback = nullptr) override {
            if (_compressFile2) return _compressFile2(originalName, targetName, progressCallback);
            return CommonUtilityBase().compressFile(originalName, targetName, progressCallback);
        }

        bool compressFile(const QString &originalName, const QString &targetName,
                          const std::function<bool(int)> &progressCallback = nullptr) override {
            if (_compressFile3) return _compressFile3(originalName, targetName, progressCallback);
            return CommonUtilityBase().compressFile(originalName, targetName, progressCallback);
        }

        QString englishCode() override {
            if (_englishCode) return _englishCode();
            return CommonUtilityBase().englishCode();
        }
        QString frenchCode() override {
            if (_frenchCode) return _frenchCode();
            return CommonUtilityBase().frenchCode();
        }
        QString germanCode() override {
            if (_germanCode) return _germanCode();
            return CommonUtilityBase().germanCode();
        }
        QString spanishCode() override {
            if (_spanishCode) return _spanishCode();
            return CommonUtilityBase().spanishCode();
        }
        QString italianCode() override {
            if (_italianCode) return _italianCode();
            return CommonUtilityBase().italianCode();
        }
        QString languageCode(Language language) override {
            if (_languageCode) return _languageCode(language);
            return CommonUtilityBase().languageCode(language);
        }
        QStringList languageCodeList(Language enforcedLocale) override {
            if (_languageCodeList) return _languageCodeList(enforcedLocale);
            return CommonUtilityBase().languageCodeList(enforcedLocale);
        }
        void setupTranslations(QCoreApplication *app, Language enforcedLocale) override {
            if (_setupTranslations) _setupTranslations(app, enforcedLocale);
            CommonUtilityBase().setupTranslations(app, enforcedLocale);
        }
        bool languageCodeIsEnglish(const QString &languageCode) override {
            if (_languageCodeIsEnglish) return _languageCodeIsEnglish(languageCode);
            return CommonUtilityBase().languageCodeIsEnglish(languageCode);
        }

        // Color threshold check
        bool colorThresholdCheck(int red, int green, int blue) override {
            if (_colorThresholdCheck) return _colorThresholdCheck(red, green, blue);
            return CommonUtilityBase().colorThresholdCheck(red, green, blue);
        }

        SyncPath relativePath(const SyncPath &rootPath, const SyncPath &path) override {
            if (_relativePath) return _relativePath(rootPath, path);
            return CommonUtilityBase().relativePath(rootPath, path);
        }

        SyncPath getAppDir() override {
            if (_getAppDir) return _getAppDir();
            return CommonUtilityBase().getAppDir();
        }
        SyncPath getAppSupportDir() override {
            if (_getAppSupportDir) return _getAppSupportDir();
            return CommonUtilityBase().getAppSupportDir();
        }
        SyncPath getAppWorkingDir() override {
            if (_getAppWorkingDir) return _getAppWorkingDir();
            return CommonUtilityBase().getAppWorkingDir();
        }

        QString getFileIconPathFromFileName(const QString &fileName, NodeType type) override {
            if (_getFileIconPathFromFileName) return _getFileIconPathFromFileName(fileName, type);
            return CommonUtilityBase().getFileIconPathFromFileName(fileName, type);
        }

        QString getRelativePathFromHome(const QString &dirPath) override {
            if (_getRelativePathFromHome) return _getRelativePathFromHome(dirPath);
            return CommonUtilityBase().getRelativePathFromHome(dirPath);
        }

        bool isFileSizeMismatchDetectionEnabled() override {
            if (_isFileSizeMismatchDetectionEnabled) return _isFileSizeMismatchDetectionEnabled();
            return CommonUtilityBase().isFileSizeMismatchDetectionEnabled();
        }
        size_t maxPathLength() override {
            if (_maxPathLength) return _maxPathLength();
            return CommonUtilityBase().maxPathLength();
        }

        bool isSubDir(const SyncPath &path1, const SyncPath &path2) override {
            if (_isSubDir) return _isSubDir(path1, path2);
            return CommonUtilityBase().isSubDir(path1, path2);
        }

        const std::string dbVersionNumber(const std::string &dbVersion) override {
            if (_dbVersionNumber) return _dbVersionNumber(dbVersion);
            return CommonUtilityBase().dbVersionNumber(dbVersion);
        }
        bool isVersionLower(const std::string &currentVersion, const std::string &targetVersion) override {
            if (_isVersionLower) return _isVersionLower(currentVersion, targetVersion);
            return CommonUtilityBase().isVersionLower(currentVersion, targetVersion);
        }

        bool dirNameIsValid(const SyncName &name) override {
            if (_dirNameIsValid) return _dirNameIsValid(name);
            return CommonUtilityBase().dirNameIsValid(name);
        }
        bool fileNameIsValid(const SyncName &name) override {
            if (_fileNameIsValid) return _fileNameIsValid(name);
            return CommonUtilityBase().fileNameIsValid(name);
        }

#ifdef __APPLE__
        const std::string loginItemAgentId() override {
            if (_loginItemAgentId) return _loginItemAgentId();
            return CommonUtilityBase().loginItemAgentId();
        }
        const std::string liteSyncExtBundleId() override {
            if (_liteSyncExtBundleId) return _liteSyncExtBundleId();
            return CommonUtilityBase().liteSyncExtBundleId();
        }
        bool isLiteSyncExtEnabled() override {
            if (_isLiteSyncExtEnabled) return _isLiteSyncExtEnabled();
            return CommonUtilityBase().isLiteSyncExtEnabled();
        }
        bool isLiteSyncExtFullDiskAccessAuthOk(std::string &errorDescr) override {
            if (_isLiteSyncExtFullDiskAccessAuthOk) return _isLiteSyncExtFullDiskAccessAuthOk(errorDescr);
            return CommonUtilityBase().isLiteSyncExtFullDiskAccessAuthOk(errorDescr);
        }
#endif
        std::string envVarValue(const std::string &name) override {
            if (_envVarValue) return _envVarValue(name);
            return CommonUtilityBase().envVarValue(name);
        }
        std::string envVarValue(const std::string &name, bool &isSet) override {
            if (_envVarValue2) return _envVarValue2(name, isSet);
            return CommonUtilityBase().envVarValue(name, isSet);
        }

        void handleSignals(void (*sigHandler)(int)) override {
            if (_handleSignals) _handleSignals(sigHandler);
            CommonUtilityBase().handleSignals(sigHandler);
        }
        SyncPath signalFilePath(AppType appType, SignalCategory signalCategory) override {
            if (_signalFilePath) return _signalFilePath(appType, signalCategory);
            return CommonUtilityBase().signalFilePath(appType, signalCategory);
        }
        void writeSignalFile(AppType appType, SignalType signalType) noexcept override {
            if (_writeSignalFile) _writeSignalFile(appType, signalType);
            CommonUtilityBase().writeSignalFile(appType, signalType);
        }
        void clearSignalFile(AppType appType, SignalCategory signalCategory, SignalType &signalType) noexcept override {
            if (_clearSignalFile) _clearSignalFile(appType, signalCategory, signalType);
            CommonUtilityBase().clearSignalFile(appType, signalCategory, signalType);
        }

        bool isLikeFileNotFoundError(const std::error_code &ec) noexcept override {
            if (_isLikeFileNotFoundError) return _isLikeFileNotFoundError(ec);
            return CommonUtilityBase().isLikeFileNotFoundError(ec);
        }

#ifdef _WIN32
        // Converts a std::wstring to std::string assuming that it contains only mono byte chars
        std::string toUnsafeStr(const SyncName &name) override {
            if (_toUnsafeStr) return _toUnsafeStr(name);
            return CommonUtilityBase().toUnsafeStr(name);
        }

        std::wstring getErrorMessage(DWORD errorMessageId) override {
            if (_getErrorMessage) return _getErrorMessage(errorMessageId);
            return CommonUtilityBase().getErrorMessage(errorMessageId);
        }
        std::wstring getLastErrorMessage() override {
            if (_getLastErrorMessage) return _getLastErrorMessage();
            return CommonUtilityBase().getLastErrorMessage();
        }
        bool isLikeFileNotFoundError(DWORD dwError) noexcept override  {
            if (_isLikeFileNotFoundError2) return _isLikeFileNotFoundError2(dwError);
            return CommonUtilityBase().isLikeFileNotFoundError(dwError);
        }
#endif

        QString truncateLongLogMessage(const QString &message) override {
            if (_truncateLongLogMessage) return _truncateLongLogMessage(message);
            return CommonUtilityBase().truncateLongLogMessage(message);
        }

        // Mock setters
        void setLinkStyleMock(std::function<QString()> linkStyle) { _linkStyle = linkStyle; }
        void setLogsPurgeRateMock(std::function<int()> logsPurgeRate) { _logsPurgeRate = logsPurgeRate; }
        void setLogMaxSizeMock(std::function<int()> logMaxSize) { _logMaxSize = logMaxSize; }
        void setGetIconPathMock(std::function<QString(IconType)> getIconPath) { _getIconPath = getIconPath; }
        void setHasDarkSystrayMock(std::function<bool()> hasDarkSystray) { _hasDarkSystray = hasDarkSystray; }
        void setSetFolderCustomIconMock(std::function<bool(const QString &, IconType)> setFolderCustomIcon) {
            _setFolderCustomIcon = setFolderCustomIcon;
        }
        void setGenerateRandomStringAlphaNumMock(std::function<std::string(int)> generateRandomStringAlphaNum) {
            _generateRandomStringAlphaNum = generateRandomStringAlphaNum;
        }
        void setGenerateRandomStringPKCEMock(std::function<std::string(int)> generateRandomStringPKCE) {
            _generateRandomStringPKCE = generateRandomStringPKCE;
        }
        void setFileSystemNameMock(std::function<QString(const QString &)> fileSystemName) { _fileSystemName = fileSystemName; }
        void setFreeDiskSpaceMock(std::function<qint64(const QString &)> freeDiskSpace) { _freeDiskSpace = freeDiskSpace; }
        void setCrashMock(std::function<void()> crash) { _crash = crash; }
        void setPlatformNameMock(std::function<QString()> platformName) { _platformName = platformName; }
        void setPlatformMock(std::function<Platform()> platform) { _platform = platform; }
        void setPlatformArchMock(std::function<QString()> platformArch) { _platformArch = platformArch; }
        void setUserAgentStringMock(std::function<const std::string &()> userAgentString) { _userAgentString = userAgentString; }
        void setCurrentVersionMock(std::function<const std::string &()> currentVersion) { _currentVersion = currentVersion; }
        void setToQByteArrayMock(std::function<QByteArray(qint32)> toQByteArray) { _toQByteArray = toQByteArray; }
        void setToIntMock(std::function<int(QByteArray)> toInt) { _toInt = toInt; }
        void setEscapeMock(std::function<QString(const QString &)> escape) { _escape = escape; }
        void setStringToAppStateValueMock(std::function<bool(const std::string &, AppStateValue &)> stringToAppStateValue) {
            _stringToAppStateValue = stringToAppStateValue;
        }
        void setAppStateValueToStringMock(std::function<bool(const AppStateValue &, std::string &)> appStateValueToString) {
            _appStateValueToString = appStateValueToString;
        }
        void setAppStateKeyToStringMock(std::function<std::string(const AppStateKey &)> appStateKeyToString) {
            _appStateKeyToString = appStateKeyToString;
        }
        void setCompressFile(
                std::function<bool(const std::wstring &, const std::wstring &, const std::function<bool(int)> &)> compressFile) {
            _compressFile = compressFile;
        }
        void setCompressFile2(
                std::function<bool(const std::string &, const std::string &, const std::function<bool(int)> &)> compressFile2) {
            _compressFile2 = compressFile2;
        }
        void setCompressFile3(
                std::function<bool(const QString &, const QString &, const std::function<bool(int)> &)> compressFile3) {
            _compressFile3 = compressFile3;
        }
        void setEnglishCodeMock(std::function<QString()> englishCode) { _englishCode = englishCode; }
        void setFrenchCodeMock(std::function<QString()> frenchCode) { _frenchCode = frenchCode; }
        void setGermanCodeMock(std::function<QString()> germanCode) { _germanCode = germanCode; }
        void setSpanishCodeMock(std::function<QString()> spanishCode) { _spanishCode = spanishCode; }
        void setItalianCodeMock(std::function<QString()> italianCode) { _italianCode = italianCode; }
        void setLanguageCodeMock(std::function<QString(Language)> languageCode) { _languageCode = languageCode; }
        void setLanguageCodeListMock(std::function<QStringList(Language)> languageCodeList) {
            _languageCodeList = languageCodeList;
        }
        void setSetupTranslationsMock(std::function<void(QCoreApplication *, Language)> setupTranslations) {
            _setupTranslations = setupTranslations;
        }

        void setLanguageCodeIsEnglishMock(std::function<bool(const QString &)> languageCodeIsEnglish) {
            _languageCodeIsEnglish = languageCodeIsEnglish;
        }
        void setColorThresholdCheckMock(std::function<bool(int, int, int)> colorThresholdCheck) {
            _colorThresholdCheck = colorThresholdCheck;
        }
        void setRelativePathMock(std::function<SyncPath(const SyncPath &, const SyncPath &)> relativePath) {
            _relativePath = relativePath;
        }
        void setGetAppDirMock(std::function<SyncPath()> getAppDir) { _getAppDir = getAppDir; }
        void setGetAppSupportDirMock(std::function<SyncPath()> getAppSupportDir) { _getAppSupportDir = getAppSupportDir; }
        void setGetAppWorkingDirMock(std::function<SyncPath()> getAppWorkingDir) { _getAppWorkingDir = getAppWorkingDir; }

        void setGetFileIconPathFromFileNameMock(std::function<QString(const QString &, NodeType)> getFileIconPathFromFileName) {
            _getFileIconPathFromFileName = getFileIconPathFromFileName;
        }
        void setGetRelativePathFromHomeMock(std::function<QString(const QString &)> getRelativePathFromHome) {
            _getRelativePathFromHome = getRelativePathFromHome;
        }
        void setIsFileSizeMismatchDetectionEnabledMock(std::function<bool()> isFileSizeMismatchDetectionEnabled) {
            _isFileSizeMismatchDetectionEnabled = isFileSizeMismatchDetectionEnabled;
        }
        void setMaxPathLengthMock(std::function<size_t()> maxPathLength) { _maxPathLength = maxPathLength; }
        void setIsSubDirMock(std::function<bool(const SyncPath &, const SyncPath &)> isSubDir) { _isSubDir = isSubDir; }
        void setDbVersionNumberMock(std::function<std::string(const std::string &)> dbVersionNumber) {
            _dbVersionNumber = dbVersionNumber;
        }
        void setIsVersionLowerMock(std::function<bool(const std::string &, const std::string &)> isVersionLower) {
            _isVersionLower = isVersionLower;
        }
        void setDirNameIsValidMock(std::function<bool(const SyncName &)> dirNameIsValid) { _dirNameIsValid = dirNameIsValid; }
        void setFileNameIsValidMock(std::function<bool(const SyncName &)> fileNameIsValid) { _fileNameIsValid = fileNameIsValid; }

#ifdef __APPLE__
        void setLoginItemAgentIdMock(std::function<std::string()> loginItemAgentId) { _loginItemAgentId = loginItemAgentId; }
        void setLiteSyncExtBundleIdMock(std::function<std::string()> liteSyncExtBundleId) {
            _liteSyncExtBundleId = liteSyncExtBundleId;
        }
        void setIsLiteSyncExtEnabledMock(std::function<bool()> isLiteSyncExtEnabled) {
            _isLiteSyncExtEnabled = isLiteSyncExtEnabled;
        }
        void setIsLiteSyncExtFullDiskAccessAuthOkMock(std::function<bool(std::string &)> isLiteSyncExtFullDiskAccessAuthOk) {
            _isLiteSyncExtFullDiskAccessAuthOk = isLiteSyncExtFullDiskAccessAuthOk;
        }

#endif
        void setEnvVarValueMock(std::function<std::string(const std::string &)> envVarValue) { _envVarValue = envVarValue; }
        void setEnvVarValueMock(std::function<std::string(const std::string &, bool &)> envVarValue) {
            _envVarValue2 = envVarValue;
        }
        void setHandleSignalsMock(std::function<void(void (*)(int))> handleSignals) { _handleSignals = handleSignals; }
        void setSignalFilePathMock(std::function<SyncPath(AppType, SignalCategory)> signalFilePath) {
            _signalFilePath = signalFilePath;
        }
        void setWriteSignalFileMock(std::function<void(AppType, SignalType)> writeSignalFile) {
            _writeSignalFile = writeSignalFile;
        }
        void setClearSignalFileMock(std::function<void(AppType, SignalCategory, SignalType &)> clearSignalFile) {
            _clearSignalFile = clearSignalFile;
        }
        void setIsLikeFileNotFoundErrorMock(std::function<bool(const std::error_code &)> isLikeFileNotFoundError) {
            _isLikeFileNotFoundError = isLikeFileNotFoundError;
        }

#ifdef _WIN32
        void setToUnsafeStrMock(std::function<std::string(const SyncName &)> toUnsafeStr) { _toUnsafeStr = toUnsafeStr; }
        void setGetErrorMessageMock(std::function<std::wstring(DWORD)> getErrorMessage) { _getErrorMessage = getErrorMessage; }
        void setGetLastErrorMessageMock(std::function<std::wstring()> getLastErrorMessage) {
            _getLastErrorMessage = getLastErrorMessage;
        }
        void setIsLikeFileNotFoundErrorMock(std::function<bool(DWORD)> isLikeFileNotFoundError) {
            _isLikeFileNotFoundError2 = isLikeFileNotFoundError;
        }
#endif
        void setTruncateLongLogMessageMock(std::function<QString(const QString &)> truncateLongLogMessage) {
            _truncateLongLogMessage = truncateLongLogMessage;
        }

    private:
        std::function<QString()> _linkStyle;
        std::function<int()> _logsPurgeRate;
        std::function<int()> _logMaxSize;
        std::function<QString(IconType)> _getIconPath;
        std::function<bool()> _hasDarkSystray;
        std::function<bool(const QString &, IconType)> _setFolderCustomIcon;
        std::function<std::string(int)> _generateRandomStringAlphaNum;
        std::function<std::string(int)> _generateRandomStringPKCE;
        std::function<QString(const QString &)> _fileSystemName;
        std::function<qint64(const QString &)> _freeDiskSpace;
        std::function<void()> _crash;
        std::function<QString()> _platformName;
        std::function<Platform()> _platform;
        std::function<QString()> _platformArch;
        std::function<const std::string &()> _userAgentString;
        std::function<const std::string &()> _currentVersion;
        std::function<QByteArray(qint32)> _toQByteArray;
        std::function<int(QByteArray)> _toInt;
        std::function<QString(const QString &)> _escape;
        std::function<bool(const std::string &, AppStateValue &)> _stringToAppStateValue;
        std::function<bool(const AppStateValue &, std::string &)> _appStateValueToString;
        std::function<std::string(const AppStateKey &)> _appStateKeyToString;
        std::function<bool(const std::wstring &, const std::wstring &, const std::function<bool(int)> &)> _compressFile;
        std::function<bool(const std::string &, const std::string &, const std::function<bool(int)> &)> _compressFile2;
        std::function<bool(const QString &, const QString &, const std::function<bool(int)> &)> _compressFile3;
        std::function<QString()> _englishCode;
        std::function<QString()> _frenchCode;
        std::function<QString()> _germanCode;
        std::function<QString()> _spanishCode;
        std::function<QString()> _italianCode;
        std::function<QString(Language)> _languageCode;
        std::function<QStringList(Language)> _languageCodeList;
        std::function<void(QCoreApplication *, Language)> _setupTranslations;
        std::function<bool(const QString &)> _languageCodeIsEnglish;
        std::function<bool(int, int, int)> _colorThresholdCheck;
        std::function<SyncPath(const SyncPath &, const SyncPath &)> _relativePath;
        std::function<SyncPath()> _getAppDir;
        std::function<SyncPath()> _getAppSupportDir;
        std::function<SyncPath()> _getAppWorkingDir;
        std::function<QString(const QString &, NodeType)> _getFileIconPathFromFileName;
        std::function<QString(const QString &)> _getRelativePathFromHome;
        std::function<bool()> _isFileSizeMismatchDetectionEnabled;
        std::function<size_t()> _maxPathLength;
        std::function<bool(const SyncPath &, const SyncPath &)> _isSubDir;
        std::function<std::string(const std::string &)> _dbVersionNumber;
        std::function<bool(const std::string &, const std::string &)> _isVersionLower;
        std::function<bool(const SyncName &)> _dirNameIsValid;
        std::function<bool(const SyncName &)> _fileNameIsValid;
#ifdef __APPLE__
        std::function<std::string()> _loginItemAgentId;
        std::function<std::string()> _liteSyncExtBundleId;
        std::function<bool()> _isLiteSyncExtEnabled;
        std::function<bool(std::string &)> _isLiteSyncExtFullDiskAccessAuthOk;
#endif
        std::function<std::string(const std::string &)> _envVarValue;
        std::function<std::string(const std::string &, bool &)> _envVarValue2;
        std::function<void(void (*)(int))> _handleSignals;
        std::function<SyncPath(AppType, SignalCategory)> _signalFilePath;
        std::function<void(AppType, SignalType)> _writeSignalFile;
        std::function<void(AppType, SignalCategory, SignalType &)> _clearSignalFile;
        std::function<bool(const std::error_code &)> _isLikeFileNotFoundError;
#ifdef _WIN32
        std::function<std::string(const SyncName &)> _toUnsafeStr;
        std::function<std::wstring(DWORD)> _getErrorMessage;
        std::function<std::wstring()> _getLastErrorMessage;
        std::function<bool(DWORD)> _isLikeFileNotFoundError2;
#endif
        std::function<QString(const QString &)> _truncateLongLogMessage;

    private:
        static std::shared_ptr<MockCommonUtility> _instance;
};
} // namespace KDC
