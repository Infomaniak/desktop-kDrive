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

#include <string>
#include <thread>
#include <random>

#if defined(KD_WINDOWS)
#include <strsafe.h>
#endif

#include <QByteArray>
#include <QCoreApplication>
#include <QDataStream>
#include <QIODevice>
#include <QThread>

#include <Poco/Dynamic/Struct.h>

#include <log4cplus/log4cplus.h>

namespace KDC {
struct COMMON_EXPORT CommonUtility {
        enum IconType {
            MAIN_FOLDER_ICON,
            COMMON_DOCUMENT_ICON,
            DROP_BOX_ICON,
            NORMAL_FOLDER_ICON
        };

        static inline const QString linkStyle = QString("color:#0098FF; font-weight:450; text-decoration:none;");

        static const int logsPurgeRate; // Delay after which the logs are purged, expressed in days
        static const int logMaxSize;

        static std::string getIconPath(IconType iconType);
        static SyncPath _workingDirPath;

        static bool hasDarkSystray();
        static bool setFolderCustomIcon(const SyncPath &folderPath, IconType iconType);

        static std::string generateRandomStringAlphaNum(int length = 10);
        static std::string generateRandomStringPKCE(int length = 10);

        // File system type
        static bool isNTFS(const SyncPath &targetPath);
        static bool isAPFS(const SyncPath &targetPath);
        static bool isFAT(const SyncPath &targetPath);
        static std::string fileSystemName(const SyncPath &targetPath);

        static qint64 freeDiskSpace(const QString &path);
        static void crash();
        static QString platformName();
        static Platform platform();
        static QString platformArch();
        static const std::string &userAgentString();
        static const std::string &currentVersion();
        static const std::string &versionTag();
        static uint64_t versionBuild();

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

        static QTranslator *_translator;
        static QTranslator *_qtTranslator;
        static const QString englishCode;
        static const QString frenchCode;
        static const QString germanCode;
        static const QString spanishCode;
        static const QString italianCode;
        static QString languageCode(Language language);
        static QStringList languageCodeList(Language enforcedLocale);
        static void setupTranslations(QCoreApplication *app, Language enforcedLocale);
        static bool languageCodeIsEnglish(const QString &languageCode);
        static bool isSupportedLanguage(const QString &languageCode);

        // Color threshold check
        static bool colorThresholdCheck(int red, int green, int blue);

        static SyncPath relativePath(const SyncPath &rootPath, const SyncPath &path);

        static SyncPath getAppDir();
        static SyncPath getAppSupportDir();
        static SyncPath getAppWorkingDir();
#ifdef __APPLE__
        static SyncPath getExtensionPath();
#endif

        static QString getFileIconPathFromFileName(const QString &fileName, NodeType type);

        static QString getRelativePathFromHome(const QString &dirPath);

        static bool isFileSizeMismatchDetectionEnabled();
        static size_t maxPathLength();

        static bool isSubDir(const SyncPath &path1, const SyncPath &path2);
        /**
         * @brief Determines if the given path is the root folder of a disk and optionally suggests a corrected path.
         * @param absolutePath The absolute path to check for being a disk root folder.
         * @param suggestedPath Reference to a path variable where a suggested corrected path may be stored if the input is not a
         * disk root.
         * @return True if the given path is a disk root folder; otherwise, false.
         */
        static bool isDiskRootFolder(const SyncPath &absolutePath, SyncPath &suggestedPath);
        static const std::string dbVersionNumber(const std::string &dbVersion);
        static bool isVersionLower(const std::string &currentVersion, const std::string &targetVersion);

        static bool dirNameIsValid(const SyncName &name);
        static bool fileNameIsValid(const SyncName &name);

#if defined(KD_MACOS)
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


#if defined(KD_WINDOWS)
        // Converts a std::wstring to std::string assuming that it contains only mono byte chars
        static std::string toUnsafeStr(const SyncName &name);
#endif

        static QString truncateLongLogMessage(const QString &message);

        static SyncPath applicationFilePath();

        static void resetTranslations();

        // String manipulation
        static bool startsWith(const std::string &str, const std::string &prefix);
        static bool startsWithInsensitive(const std::string &str, const std::string &prefix);
        static bool endsWith(const std::string &str, const std::string &suffix);
        static bool endsWithInsensitive(const std::string &str, const std::string &suffix);
#if defined(KD_WINDOWS)
        static bool startsWithInsensitive(const SyncName &str, const SyncName &prefix);
        static bool startsWith(const SyncName &str, const SyncName &prefix);
        static bool endsWith(const SyncName &str, const SyncName &suffix);
        static bool endsWithInsensitive(const SyncName &str, const SyncName &suffix);
#endif
        static bool contains(const std::string &str, const std::string &substr);
        static std::string toUpper(const std::string &str);
        static std::string toLower(const std::string &str);
        static std::wstring s2ws(const std::string &str);
        static std::string ws2s(const std::wstring &wstr);
        static std::string ltrim(const std::string &s);
        static std::string rtrim(const std::string &s);
        static std::string trim(const std::string &s);
#if defined(KD_WINDOWS)
        static SyncName ltrim(const SyncName &s);
        static SyncName rtrim(const SyncName &s);
        static SyncName trim(const SyncName &s);
#endif

        static bool isDescendantOrEqual(const SyncPath &potentialDescendant, const SyncPath &path);
        static bool isStrictDescendant(const SyncPath &potentialDescendant, const SyncPath &path);


        static bool normalizedSyncName(const SyncName &name, SyncName &normalizedName,
                                       UnicodeNormalization normalization = UnicodeNormalization::NFC) noexcept;
        /**
         * Split the input path into a vector of file and directory names.
         * @param path the path to split.
         * @return A vector of the file and directory names composing the path, sorted
         * in reverse order.
         * Example: the return value associated to path = SyncPath("A / B / c.txt") is the vector
         * ["c.txt", "B", "A"]
         */
        static std::list<SyncName> splitSyncPath(const SyncPath &path);

        /**
         * Split the input string wrt `separator` into a vector of strings.
         * @param name the string to split.
         * @return A vector of strings, sorted in the natural order.
         * Example: the return value associated to Str("A / B / c.txt") is the vector
         * ["A", "B", "c.txt"]
         */
        template<typename T>
        static std::vector<T> splitString(T name, const T &separator);
        static std::vector<SyncName> splitSyncName(SyncName name, const SyncName &delimiter);
        static std::vector<CommString> splitCommString(CommString str, const CommString &separator);

        /**
         * Split the input path string into a vector of file and directory names.
         * @param name the path to split. This path can be a regular expression, e.g., 'A/file_*.txt'.
         * @param separator string used as delimiter for the splitting, e.g. "/".
         * @return A vector of the file and directory names composing the path, sorted
         * in their natural order.
         * Example: the return value associated to path = SyncName("A / B / c.txt") is the vector
         * ["A", "B", "c.txt"]. The splitting is done wrt to the native filename separator.
         * The output of splitPath(pathName) can be significantly different from
         * splitPath(SyncPath{pathName}), see unit tests.
         */
        static std::vector<SyncName> splitPath(const SyncName &pathName);

        /**
         * Returns the preferred path separator.
         * @return SyncName("/") on Unix-like systems and SyncName("\\") on Windows.
         */
        static SyncName preferredPathSeparator();


        //! Computes and returns the NFC and NFD normalizations of `name`.
        /*!
          \param name is the string the normalizations of which are queried.
          \return a set of SyncNames containing the NFC and NFD normalizations of name, when those are successful.
          The returned set contains additionally the SyncName name in any case.
        */
        static SyncNameSet computeSyncNameNormalizations(const SyncName &name);
        //! Computes and returns all possible NFC and NFD normalizations of `path` segments
        //! interpreted as a file system path.
        /*!
          \param templateString is the path string the normalizations of which are queried.
          \return a set of SyncNames containing the NFC and NFD normalizations of path, when those are successful.
          The returned set additionally contains the string path in any case.
        */
        static SyncNameSet computePathNormalizations(const SyncName &path);

        static ReplicaSide syncNodeTypeSide(SyncNodeType type);

        // Convenience OS detection methods
        static bool isWindows();
        static bool isMac();
        static bool isLinux();

        // CommString conversion functions
#if defined(KD_WINDOWS)
        static CommString str2CommString(const std::string &s) { return KDC::CommonUtility::s2ws(s); }
        static std::string commString2Str(const CommString &s) { return KDC::CommonUtility::ws2s(s); }
        static std::wstring commString2WStr(const CommString &s) { return s; }
        static CommString qStr2CommString(const QString &s) { return s.toStdWString(); }
        static QString commString2QStr(const CommString &s) { return QString::fromStdWString(s); }
#else
        static CommString str2CommString(const std::string &s) { return s; }
        static std::string commString2Str(const CommString &s) { return s; }
        static std::wstring commString2WStr(const CommString &s) { return KDC::CommonUtility::s2ws(s); }
        static CommString qStr2CommString(const QString &s) { return s.toStdString(); }
        static QString commString2QStr(const CommString &s) { return QString::fromStdString(s); }
#endif

        //! Read an input built-in/std::string/std::wstring/CommBLOB parameter from a Poco::DynamicStruct.
        /*!
          \param parms is a Poco::DynamicStruct.
          \param key is the key of the JSON pair.
          \param value is the value of the JSON pair.
          \exception
                throws a Poco::RangeException if the value does not fit into the result variable.
                throws a Poco::NotImplementedException if conversion is not available for the given type.
                throws an Poco::InvalidAccessException if the key is not found.
                throws a std::runtime_error if T is an enum and the value is not in [T::Unknown + 1, T::EnumEnd]
        */
        template<typename T>
        static void readValueFromStruct(const Poco::DynamicStruct &parms, const std::string &key, T &value) {
            if constexpr (std::is_same_v<T, bool>) {
                value = parms[key].convert<bool>();
            } else if constexpr (std::is_same_v<T, std::string> || std::is_same_v<T, std::wstring> ||
                                 std::is_same_v<T, CommBLOB>) {
                const auto base64Str = parms[key].convert<std::string>();
                convertFromBase64Str(base64Str, value);
            } else if constexpr (std::is_enum_v<T>) {
                int intValue;
                intValue = parms[key].convert<int>();
                if (intValue >= 1 && intValue < static_cast<int>(T::EnumEnd)) {
                    value = static_cast<T>(intValue);
                } else {
                    throw std::runtime_error("Invalid enumeration value");
                }
            } else {
                value = parms[key].convert<T>();
            }
        }

        //! Read an input std container parameter from a Poco::DynamicStruct.
        /*!
          \param parms is a Poco::DynamicStruct.
          \param key is the key of the JSON pair.
          \param value is the value of the JSON pair.
          \exception
                throws a Poco::RangeException if the value does not fit into the result variable.
                throws a Poco::NotImplementedException if conversion is not available for the given type.
                throws an Poco::InvalidAccessException if the key is not found.
                throws a std::runtime_error if T is an enum and the value is not in [T::Unknown + 1, T::EnumEnd]
        */
        template<template<typename, typename> class C, typename T, typename A = std::allocator<T>>
        static void readValuesFromStruct(const Poco::DynamicStruct &str, const std::string &key, C<T, A> &values,
                                         std::function<T(const Poco::Dynamic::Var &)> convertFct) {
            auto arrValues = str[key].extract<Poco::Dynamic::Array>();
            std::transform(arrValues.begin(), arrValues.end(), std::back_inserter(values), convertFct);
        }

        template<template<typename, typename> class C, typename T, typename A = std::allocator<T>>
        static void readValuesFromStruct(const Poco::DynamicStruct &str, const std::string &key, C<T, A> &values) {
            std::function<T(const Poco::Dynamic::Var &)> convertFct = [](const Poco::Dynamic::Var &value) {
                if constexpr (std::is_same_v<T, std::string> || std::is_same_v<T, std::wstring> || std::is_same_v<T, CommBLOB>) {
                    const auto base64Str = value.convert<std::string>();
                    T str;
                    convertFromBase64Str(base64Str, str);
                    return str;
                } else {
                    return value.convert<T>();
                }
            };
            readValuesFromStruct(str, key, values, convertFct);
        }

        //! Write an output built-in/std::string/std::wstring/CommBLOB parameter to a Poco::DynamicStruct..
        /*!
          \param parms is a Poco::DynamicStruct.
          \param key is the key of the JSON pair.
          \param value is the value of the JSON pair.
        */
        template<typename T>
        static void writeValueToStruct(Poco::DynamicStruct &str, const std::string &key, const T &value) {
            if constexpr (std::is_same_v<T, std::string> || std::is_same_v<T, std::wstring> || std::is_same_v<T, CommBLOB>) {
                std::string base64Str;
                convertToBase64Str(value, base64Str);
                str.insert(key, base64Str);
            } else if constexpr (std::is_enum_v<T>) {
                str.insert(key, static_cast<int>(value));
            } else {
                str.insert(key, value);
            }
        }

        template<size_t n>
        static void writeValueToStruct(Poco::DynamicStruct &str, const std::string &key, const char (&value)[n]) {
            writeValueToStruct(str, key, std::string(value));
        }

        template<size_t n>
        static void writeValueToStruct(Poco::DynamicStruct &str, const std::string &key, const wchar_t (&value)[n]) {
            writeValueToStruct(str, key, std::wstring(value));
        }

        //! Write an output std container parameter to a a Poco::DynamicStruct.
        /*!
          \param parms is a Poco::DynamicStruct.
          \param key is the key of the JSON pair.
          \param value is the value of the JSON pair.
        */
        template<template<typename, typename> class C, typename T, typename A = std::allocator<T>>
        static void writeValuesToStruct(Poco::DynamicStruct &str, const std::string &key, const C<T, A> &values,
                                        std::function<Poco::Dynamic::Var(const T &)> convertFct) {
            Poco::Dynamic::Array arrValues;
            std::transform(values.begin(), values.end(), std::back_inserter(arrValues), convertFct);
            str.insert(key, arrValues);
        }

        template<template<typename, typename> class C, typename T, typename A = std::allocator<T>>
        static void writeValuesToStruct(Poco::DynamicStruct &str, const std::string &key, const C<T, A> &values) {
            std::function<Poco::Dynamic::Var(const T &)> convertFct = [](const T &value) {
                if constexpr (std::is_same_v<T, std::string> || std::is_same_v<T, std::wstring> || std::is_same_v<T, CommBLOB>) {
                    std::string base64Str;
                    convertToBase64Str(value, base64Str);
                    return Poco::Dynamic::Var(base64Str);
                } else {
                    return Poco::Dynamic::Var(value);
                }
            };
            CommonUtility::writeValuesToStruct(str, key, values, convertFct);
        }

        // Base64 conversion
        static void convertFromBase64Str(const std::string &base64Str, std::string &value);
        static void convertFromBase64Str(const std::string &base64Str, std::wstring &value);
        static void convertFromBase64Str(const std::string &base64Str, CommBLOB &value);

        static void convertToBase64Str(const std::string &str, std::string &base64Str);
        static void convertToBase64Str(const std::wstring &wstr, std::string &base64Str);
        static void convertToBase64Str(const CommBLOB &blob, std::string &base64Str);

    private:
        static std::mutex _generateRandomStringMutex;

        static std::string generateRandomString(const char *charArray, std::uniform_int_distribution<int> &distrib,
                                                const int length = 10);

        static void extractIntFromStrVersion(const std::string &version, std::vector<int> &tabVersion);

        //! Computes recursively and returns all possible NFC and NFD normalizations of `pathSegments` segments
        //! interpreted as a file system path.
        /*!
          \param pathSegments is the split path string the normalizations of which are queried.
          \param lastIndex is the index of the las segment that should be considered.
          \return a set of SyncNames containing the NFC and NFD normalizations of the elements of pathSegments
          from index 0 to lastIndex, when those are successful.
          The returned set contains additionally the concatenated elements of pathSegments, from index 0 to index lastIndex,
          in any case.
        */
        static SyncNameSet computePathNormalizations(const std::vector<SyncName> &pathSegments, const int64_t lastIndex);

        //! Computes recursively and returns all possible NFC and NFD normalizations of `pathSegments` segments
        //! interpreted as a file system path.
        /*!
          \param pathSegments is the split path strings the normalizations of which are queried.
          \return a set of SyncNames containing the NFC and NFD normalizations of pathSegments, when those are successful.
          The returned set contains additionally the concatenated pathSegments in any case.
        */
        static SyncNameSet computePathNormalizations(const std::vector<SyncName> &pathSegments);

        static SyncPath getGenericAppSupportDir();
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

        template<class... Args>
        explicit StdLoggingThread(std::function<void(Args...)> runFct, Args &&...args) :
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
        explicit ArgsReader(Args... args) :
            stream(&params, QIODevice::WriteOnly) {
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
        explicit ArgsWriter(const QByteArray &results) :
            stream{QDataStream(results)} {};

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
