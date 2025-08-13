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

#include "libcommonserver/log/log.h"
#include "libcommonserver/commonserverlib.h"
#include "libcommonserver/db/dbdefs.h"
#include "libcommon/utility/filename.h"
#include "libcommon/utility/types.h"

#include <xxhash.h>

#include <string>
#include <vector>
#include <filesystem>
#include <unordered_set>
#include <list>

#include <Poco/DOM/Document.h>
#include <Poco/Net/HTTPResponse.h>

#if defined(KD_WINDOWS)
#include <windows.h>
#include <Accctrl.h>
#endif

class QString;

namespace Poco {
class URI;
}

/* TODO : Replace with std::source_location when we will bump gcc version to 10 or higher
 *  static std::string errId(std::source_location location = std::source_location::current());
 */
#define errId() Utility::_errId(__FILENAME__, __LINE__)

namespace KDC {
struct COMMONSERVER_EXPORT Utility {
        inline static void setLogger(const log4cplus::Logger &logger) { _logger = logger; }

        static bool init();
        static void free();
        static int64_t getFreeDiskSpace(const SyncPath &path);
        static int64_t freeDiskSpaceLimit();
        static bool enoughSpace(const SyncPath &path);
        static bool findNodeValue(const Poco::XML::Document &doc, const std::string &nodeName, std::string *outValue);
        static bool isCreationDateValid(int64_t creationDate);

        static void msleep(int msec);
        static std::wstring v2ws(const dbtype &v);

        static std::wstring formatStdError(const std::error_code &ec);
        static std::wstring formatStdError(const SyncPath &path, const std::error_code &ec);
        static std::wstring formatIoError(IoError ioError);
        static std::wstring formatIoError(const SyncPath &path, IoError ioError);
        static std::wstring formatIoError(const QString &path, IoError ioError);
        static std::wstring formatErrno(const SyncPath &path, long cError);
        static std::wstring formatErrno(const QString &path, long cError);
        static std::wstring quotedSyncName(const SyncName &name);
        static std::wstring formatSyncName(const SyncName &name);
        static std::wstring formatSyncPath(const SyncPath &path);
        static std::wstring formatPath(const QString &path);

        static std::string formatRequest(const Poco::URI &uri, const std::string &code, const std::string &description);

        static std::string formatGenericServerError(std::istream &inputStream, const Poco::Net::HTTPResponse &httpResponse);
        static void logGenericServerError(const log4cplus::Logger &logger, const std::string &errorTitle,
                                          std::istream &inputStream, const Poco::Net::HTTPResponse &httpResponse);

        /**
         * Check if two paths coincide up to case and encoding of file names.
         * @param a SyncPath value to be compared.
         * @param b Other SyncPath value to be compared.
         * @param isEqual true if a is equal to b up to case and NFC-normalization.
         * @return true if no normalization issue occurs when comparing.
         */
        static bool checkIfEqualUpToCaseAndEncoding(const SyncPath &a, const SyncPath &b, bool &isEqual);
        /**
         * Normalize the SyncName parameters before comparing them.
         * @param a SyncName value to be compared.
         * @param b Other SyncName value to be compared.
         * @param isEqual true if the normalized strings are equal.
         * @return true if no normalization issue.
         */
        static bool checkIfSameNormalization(const SyncName &a, const SyncName &b, bool &areSame);
        /**
         * Normalize the SyncPath parameters before comparing them.
         * @param a SyncPath value to be compared.
         * @param b Other SyncPath value to be compared.
         * @param isEqual true if the normalized strings are equal.
         * @return true if no normalization issue.
         */
        static bool checkIfSameNormalization(const SyncPath &a, const SyncPath &b, bool &areSame);


        static bool moveItemToTrash(const SyncPath &itemPath);
#if defined(KD_MACOS) || defined(KD_LINUX)
        static SyncPath getTrashPath();
#endif
        inline bool isInTrash(const SyncPath &path);
#if defined(KD_MACOS)
        static bool preventSleeping(bool enable);
        static void restartFinderExtension();
#endif
        static bool getLinuxDesktopType(std::string &currentDesktop);

        static void str2hexstr(const std::string &str, std::string &hexstr, bool capital = false);
        static void strhex2str(const std::string &hexstr, std::string &str);
        static std::vector<std::string> splitStr(const std::string &str, char sep);
        static std::string joinStr(const std::vector<std::string> &strList, char sep = 0);

        static std::string nodeSet2str(const NodeSet &set);

        inline static int pathDepth(const SyncPath &path) { return (int) std::distance(path.begin(), path.end()); }
        static std::string computeMd5Hash(const std::string &in);
        static std::string computeMd5Hash(const char *in, std::size_t length);
        static std::string computeXxHash(const std::string &in);
        static std::string computeXxHash(const char *in, std::size_t length);
        static std::string xxHashToStr(XXH64_hash_t hash);

#if defined(KD_MACOS)
        static SyncPath getExcludedAppFilePath(bool test = false);
#endif
        static SyncPath getExcludedTemplateFilePath(bool test = false);
        static SyncPath binRelativePath();
        static SyncPath resourcesRelativePath();
        static SyncName logFileName();
        static SyncName logFileNameWithTime();

        /* TODO : Replace with std::source_location when we will bump gcc version to 10 or higher
         *  static std::string errId(std::source_location location = std::source_location::current());
         */
        static std::string _errId(const char *file, int line);

        static bool normalizedSyncName(const SyncName &name, SyncName &normalizedName,
                                       UnicodeNormalization normalization = UnicodeNormalization::NFC) noexcept;
        static bool normalizedSyncPath(const SyncPath &path, SyncPath &normalizedPath,
                                       UnicodeNormalization normalization = UnicodeNormalization::NFC) noexcept;

#if defined(KD_WINDOWS)
        static bool longPath(const SyncPath &shortPathIn, SyncPath &longPathOut, bool &notFound);
        static bool runDetachedProcess(std::wstring cmd);
#endif
        static bool checkIfDirEntryIsManaged(const DirectoryEntry &dirEntry, bool &isManaged, IoError &ioError,
                                             const ItemType &itemType = ItemType());
        /* Resource analyzer */
        static bool totalRamAvailable(uint64_t &ram, int &errorCode);
        static bool ramCurrentlyUsed(uint64_t &ram, int &errorCode);
        static bool ramCurrentlyUsedByProcess(uint64_t &ram, int &errorCode);
        static bool cpuUsage(uint64_t &lastTotalUser, uint64_t &lastTotalUserLow, uint64_t &lastTotalSys, uint64_t &lastTotalIdle,
                             double &percent);
        static bool cpuUsage(uint64_t &previousTotalTicks, uint64_t &_previousIdleTicks, double &percent);
        static bool cpuUsageByProcess(double &percent);

        static SyncPath commonDocumentsFolderName();
        static SyncPath sharedFolderName();
        static std::string userName();

        static bool hasSystemLaunchOnStartup(const std::string &appName);
        static bool hasLaunchOnStartup(const std::string &appName);
        static bool setLaunchOnStartup(const std::string &appName, const std::string &guiName, bool enable);

#ifdef _WIN32
        using kdVariant = std::variant<int, std::wstring>;

        static void setFolderPinState(const std::wstring &clsid, bool show);

        static bool registryExistKeyTree(HKEY hRootKey, const std::wstring &subKey);
        static bool registryExistKeyValue(HKEY hRootKey, const std::wstring &subKey, const std::wstring &valueName);
        static kdVariant registryGetKeyValue(HKEY hRootKey, const std::wstring &subKey, const std::wstring &valueName);
        static bool registrySetKeyValue(HKEY hRootKey, const std::wstring &subKey, const std::wstring &valueName, DWORD type,
                                        const kdVariant &value, std::wstring &error);
        static bool registryDeleteKeyTree(HKEY hRootKey, const std::wstring &subKey);
        static bool registryDeleteKeyValue(HKEY hRootKey, const std::wstring &subKey, const std::wstring &valueName);
        static bool registryWalkSubKeys(HKEY hRootKey, const std::wstring &subKey,
                                        const std::function<void(HKEY, const std::wstring &)> &callback);

        // Add/remove legacy sync root keys
        static void addLegacySyncRootKeys(const std::wstring &clsid, const SyncPath &folderPath, bool show);
        static void removeLegacySyncRootKeys(const std::wstring &clsid);

        // Possibly refactor to share code with UnixTimevalToFileTime in c_time.c
        static void unixTimeToFiletime(time_t t, FILETIME *filetime);
#endif
        static bool isError500(const Poco::Net::HTTPResponse::HTTPStatus httpErrorCode);

    private:
        static log4cplus::Logger _logger;

        inline static log4cplus::Logger logger() { return Log::isSet() ? Log::instance()->getLogger() : _logger; }
};

struct TimeCounter {
        explicit TimeCounter(const std::string &name) :
            _name(name) {}
        void start() { _start = clock(); }
        void end() {
            _end = clock();
            _total += (double) (_end - _start) / CLOCKS_PER_SEC;
        }
        void trace() { LOG_DEBUG(Log::instance()->getLogger(), "Time counter " << _name << " value:" << _total); }

    private:
        std::string _name;
        clock_t _start = 0;
        clock_t _end = 0;
        double _total = 0;
};

} // namespace KDC
