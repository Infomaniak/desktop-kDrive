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

#ifdef _WIN32
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
        static bool setFileDates(const KDC::SyncPath &filePath, std::optional<KDC::SyncTime> creationDate,
                                 std::optional<KDC::SyncTime> modificationDate, bool symlink, bool &exists);
        static bool isCreationDateValid(int64_t creationDate);

        static std::wstring s2ws(const std::string &str);
        static std::string ws2s(const std::wstring &wstr);
        static std::string ltrim(const std::string &s);
        static std::string rtrim(const std::string &s);
        static std::string trim(const std::string &s);
#ifdef _WIN32
        static SyncName ltrim(const SyncName &s);
        static SyncName rtrim(const SyncName &s);
        static SyncName trim(const SyncName &s);
#endif
        static void msleep(int msec);
        static std::wstring v2ws(const dbtype &v);

        static std::wstring formatStdError(const std::error_code &ec);
        static std::wstring formatStdError(const SyncPath &path, const std::error_code &ec);
        static std::wstring formatIoError(IoError ioError);
        static std::wstring formatIoError(const SyncPath &path, IoError ioError);
        static std::wstring formatIoError(const QString &path, IoError ioError);
        static std::wstring formatErrno(const SyncPath &path, long cError);
        static std::wstring formatErrno(const QString &path, long cError);
        static std::wstring formatSyncName(const SyncName &name);
        static std::wstring formatSyncPath(const SyncPath &path);
        static std::wstring formatPath(const QString &path);

        static std::string formatRequest(const Poco::URI &uri, const std::string &code, const std::string &description);

        static std::string formatGenericServerError(std::istream &inputStream, const Poco::Net::HTTPResponse &httpResponse);
        static void logGenericServerError(const log4cplus::Logger &logger, const std::string &errorTitle,
                                          std::istream &inputStream, const Poco::Net::HTTPResponse &httpResponse);

#ifdef _WIN32
        static bool isNtfs(const SyncPath &targetPath);
#endif
        static std::string fileSystemName(const SyncPath &targetPath);
        static bool startsWith(const std::string &str, const std::string &prefix);
        static bool startsWithInsensitive(const std::string &str, const std::string &prefix);
        static bool endsWith(const std::string &str, const std::string &suffix);
        static bool endsWithInsensitive(const std::string &str, const std::string &suffix);
#ifdef _WIN32
        static bool startsWithInsensitive(const SyncName &str, const SyncName &prefix);
        static bool startsWith(const SyncName &str, const SyncName &prefix);
        static bool endsWith(const SyncName &str, const SyncName &suffix);
        static bool endsWithInsensitive(const SyncName &str, const SyncName &suffix);
#endif
        /**
         * Check if two paths coincide up to case and encoding of file names.
         * @param a SyncPath value to be compared.
         * @param b Other SyncPath value to be compared.
         * @param isEqual true if a is equal to b up to case and NFC-normalization.
         * @return true if no normalization issue occurs when comparing.
         */
        static bool checkIfEqualUpToCaseAndEncoding(const SyncPath &a, const SyncPath &b, bool &isEqual);
        static bool isDescendantOrEqual(const SyncPath &potentialDescendant, const SyncPath &path);
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
        /**
         * Split the input path into a vector of file and directory names.
         * @param path SyncPath the path to split.
         * @return A vector of the file and directory names composing the path, sorted
         * in reverse order.
         * Example: the return value associated to path = SyncPath("A / B / c.txt") is the vector
         * ["c.txt", "B", "A"]
         */
        static std::vector<SyncName> splitPath(const SyncPath &path);

        static bool moveItemToTrash(const SyncPath &itemPath);
#ifdef __APPLE__
        static bool preventSleeping(bool enable);
#endif
        static void restartFinderExtension();
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

#ifdef __APPLE__
        static SyncName getExcludedAppFilePath(bool test = false);
#endif
        static SyncName getExcludedTemplateFilePath(bool test = false);
        static SyncPath binRelativePath();
        static SyncPath resourcesRelativePath();
        static SyncName logFileName();
        static SyncName logFileNameWithTime();
        static std::string toUpper(const std::string &str);

        /* TODO : Replace with std::source_location when we will bump gcc version to 10 or higher
         *  static std::string errId(std::source_location location = std::source_location::current());
         */
        static std::string _errId(const char *file, int line);


        enum class UnicodeNormalization { NFC, NFD };
        static bool normalizedSyncName(const SyncName &name, SyncName &normalizedName,
                                       UnicodeNormalization normalization = UnicodeNormalization::NFC) noexcept;
        static bool normalizedSyncPath(const SyncPath &path, SyncPath &normalizedPath,
                                       UnicodeNormalization normalization = UnicodeNormalization::NFC) noexcept;

#ifdef _WIN32
        static bool longPath(const SyncPath &shortPathIn, SyncPath &longPathOut, bool &notFound);
        static bool runDetachedProcess(std::wstring cmd);
#endif
        static bool checkIfDirEntryIsManaged(const DirectoryEntry &dirEntry, bool &isManaged, const ItemType &itemType, IoError &ioError);
        static bool checkIfDirEntryIsManaged(const DirectoryEntry &dirEntry, bool &isManaged, bool &isLink, IoError &ioError);
        static bool checkIfDirEntryIsManaged(const std::filesystem::recursive_directory_iterator &dirIt, bool &isManaged,
                                             bool &isLink, IoError &ioError);

        /* Resources analyser */
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

    private:
        static log4cplus::Logger _logger;

        inline static log4cplus::Logger logger() { return Log::isSet() ? Log::instance()->getLogger() : _logger; }
};

struct TimeCounter {
        explicit TimeCounter(const std::string &name) : _name(name) {}
        void start() { _start = clock(); }
        void end() {
            _end = clock();
            _total += (double) (_end - _start) / CLOCKS_PER_SEC;
        }
        void trace() { LOG_DEBUG(Log::instance()->getLogger(), "Time counter " << _name.c_str() << " value:" << _total); }

    private:
        std::string _name;
        clock_t _start = 0;
        clock_t _end = 0;
        double _total = 0;
};

} // namespace KDC
