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

#include "libcommonserver/log/log.h"
#include "libcommonserver/commonserverlib.h"
#include "libcommonserver/db/dbdefs.h"
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

namespace Poco {
class URI;
}

namespace KDC {

struct COMMONSERVER_EXPORT Utility {
#ifdef _WIN32
        static std::unique_ptr<BYTE[]> _psid;
        static TRUSTEE _trustee;
#endif

        inline static void setLogger(log4cplus::Logger logger) { _logger = logger; }

        static bool init();
        static void free();
        static int64_t freeDiskSpace(const SyncPath &path);
        static int64_t freeDiskSpaceLimit();
        static bool enoughSpace(const SyncPath &path);
        static bool findNodeValue(const Poco::XML::Document &doc, const std::string &nodeName, std::string *outValue);
        static bool setFileDates(const KDC::SyncPath &filePath, std::optional<KDC::SyncTime> creationDate,
                                 std::optional<KDC::SyncTime> modificationDate, bool symlink, bool &exists);
        static bool isCreationDateValid(uint64_t creationDate);

        static std::wstring s2ws(const std::string &str);
        static std::string ws2s(const std::wstring &wstr);
        static std::string ltrim(const std::string &s);
        static std::string rtrim(const std::string &s);
        static std::string trim(const std::string &s);
        static void msleep(int msec);
        static std::wstring v2ws(const dbtype &v);

        static std::wstring formatStdError(const std::error_code &ec);
        static std::wstring formatStdError(const SyncPath &path, const std::error_code &ec);
        static std::wstring formatIoError(const SyncPath &path, IoError ioError);
        static std::wstring formatSyncPath(const SyncPath &path);

        static std::string formatRequest(const Poco::URI &uri, const std::string &code, const std::string &description);

        static std::string formatGenericServerError(std::istream &inputStream, const Poco::Net::HTTPResponse &httpResponse);
        static void logGenericServerError(const log4cplus::Logger &logger, const std::string &errorTitle, std::istream &inputStream, const Poco::Net::HTTPResponse &httpResponse);

#ifdef _WIN32
        static bool isNtfs(const SyncPath &dirPath);
#endif
        static std::string fileSystemName(const SyncPath &dirPath);
        static bool startsWith(const std::string &str, const std::string &prefix);
        static bool startsWithInsensitive(const std::string &str, const std::string &prefix);
        static bool endsWith(const std::string &str, const std::string &suffix);
        static bool endsWithInsensitive(const std::string &str, const std::string &suffix);
        static bool isEqualInsensitive(const std::string &strA, const std::string &strB);
#ifdef _WIN32
        static bool startsWithInsensitive(const SyncName &str, const SyncName &prefix);
        static bool startsWith(const SyncName &str, const SyncName &prefix);
        static bool endsWith(const SyncName &str, const SyncName &suffix);
        static bool endsWithInsensitive(const SyncName &str, const SyncName &suffix);
        static bool isEqualInsensitive(const SyncName &a, const SyncName &b);
#endif
        static bool moveItemToTrash(const SyncPath &itemPath);
#ifdef __APPLE__
        static bool preventSleeping(bool enable);
#endif
        static bool getLinuxDesktopType(std::string &currentDesktop);

        static void str2hexstr(const std::string &str, std::string &hexstr, bool capital = false);
        static void strhex2str(const std::string &hexstr, std::string &str);
        static std::vector<std::string> splitStr(const std::string &str, char sep);
        static std::string joinStr(const std::vector<std::string> &strList, char sep = 0);
        static std::string opType2Str(OperationType opType);
        static std::string conflictType2Str(ConflictType conflictType);
        static std::string side2Str(ReplicaSide side);
        static std::string nodeType2Str(NodeType type);
        static std::string logLevel2Str(LogLevel level);
        static std::string syncFileStatus2Str(SyncFileStatus status);
        static std::string list2str(std::unordered_set<std::string> inList);
        static std::string list2str(std::list<std::string> inList);

        static int pathDepth(const SyncPath path);
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
        static std::string errId(const char *file, int line);

        static SyncName normalizedSyncName(const SyncName &name);
        static SyncPath normalizedSyncPath(const SyncPath &path) noexcept;
#ifdef _WIN32
        static bool fileExists(DWORD dwordError) noexcept;
        static bool longPath(const SyncPath &shortPathIn, SyncPath &longPathOut, bool &notFound);
#endif
        static bool checkIfDirEntryIsManaged(std::filesystem::recursive_directory_iterator &dirIt, bool &isManaged, bool &isLink,
                                             IoError &ioError);

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

    private:
        static log4cplus::Logger _logger;

        inline static log4cplus::Logger logger() { return Log::isSet() ? Log::instance()->getLogger() : _logger; }
};

}  // namespace KDC
