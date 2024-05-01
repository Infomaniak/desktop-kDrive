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

#include "utility.h"
#include "Poco/URI.h"
#include "config.h"
#include "libcommon/utility/utility.h"
#include "libcommonserver/io/iohelper.h"

#if defined(__APPLE__)
#include "utility_mac.cpp"
#elif defined(__unix__)
#include "utility_linux.cpp"
#elif defined(_WIN32)
#include "utility_win.cpp"
#endif

#if defined(__APPLE__)
#include <sys/statvfs.h>
#include <sys/mount.h>
#elif defined(__unix__)
#include <sys/statvfs.h>
#elif defined(_WIN32)
#include <fileapi.h>
#endif

#include <locale>
#include <algorithm>
#include <thread>
#include <variant>
#include <ctime>
#include <filesystem>
#include <sstream>
#include <ctime>

#include <sentry.h>

#ifndef _WIN32
#include <utf8proc.h>
#endif

#include <Poco/MD5Engine.h>
#include <Poco/UnicodeConverter.h>
#include <Poco/DOM/Node.h>

#include <log4cplus/loggingmacros.h>

#define LOGFILE_TIME_FORMAT "%Y%m%d_%H%M"
#define LOGFILE_EXT ".log"

#define COMMON_DOC_FOLDER "Common documents"
#define SHARED_FOLDER "Shared"

namespace KDC {

static const SyncName excludedTemplateFileName(Str("sync-exclude.lst"));

#if defined(__APPLE__)
static const SyncName excludedAppFileName(Str("litesync-exclude.lst"));
#endif

// Resources relative path from working dir
#if defined(__APPLE__)
static const SyncName resourcesPath(Str("../../Contents/Resources"));
static const SyncName testResourcesPath(Str("kDrive.app/Contents/Resources/"));
#elif defined(__unix__)
static const SyncName resourcesPath(Str(""));
static const SyncName testResourcesPath(Str(""));
#elif defined(_WIN32)
static const SyncName resourcesPath(Str(""));
static const SyncName testResourcesPath(Str(""));
static const std::string NTFS("NTFS");
#endif

struct VariantPrinter {
        std::wstring operator()(std::monostate) { return std::wstring(L"NULL"); }
        std::wstring operator()(int value) { return std::to_wstring(value); }
        std::wstring operator()(int64_t value) { return std::to_wstring(value); }
        std::wstring operator()(double value) { return std::to_wstring(value); }
        std::wstring operator()(const std::string &value) { return Utility::s2ws(value); }
        std::wstring operator()(const std::wstring &value) { return value; }
        std::wstring operator()(std::shared_ptr<std::vector<char>>) { return std::wstring(); }
};

log4cplus::Logger Utility::_logger;

#ifdef _WIN32
std::unique_ptr<BYTE[]> Utility::_psid = nullptr;
TRUSTEE Utility::_trustee = {0};
#endif

bool Utility::init() {
    return init_private();
}

void Utility::free() {
    free_private();
}

int64_t Utility::freeDiskSpace(const SyncPath &path) {
#if defined(__APPLE__)
    struct statvfs stat;
    if (statvfs(path.c_str(), &stat) == 0) {
        return (int64_t)stat.f_bavail * stat.f_frsize;
    }
#elif defined(__unix__)
    struct statvfs64 stat;
    if (statvfs64(path.c_str(), &stat) == 0) {
        return (int64_t)stat.f_bavail * stat.f_frsize;
    }
#elif defined(_WIN32)
    ULARGE_INTEGER freeBytes;
    freeBytes.QuadPart = 0L;
    if (GetDiskFreeSpaceEx(reinterpret_cast<const wchar_t *>(Path2WStr(path).c_str()), &freeBytes, NULL, NULL)) {
        return freeBytes.QuadPart;
    }
#endif
    return -1;
}

int64_t Utility::freeDiskSpaceLimit() {
    static int64_t limit = 250 * 1000 * 1000LL;  // 250MB
    return limit;
}

bool Utility::enoughSpace(const SyncPath &path) {
    const int64_t freeBytes = Utility::freeDiskSpace(path);
    if (freeBytes >= 0) {
        if (freeBytes < Utility::freeDiskSpaceLimit()) {
            return false;
        }
    }
    return true;
}

bool Utility::findNodeValue(const Poco::XML::Document &doc, const std::string &nodeName, std::string *outValue) {
    Poco::XML::Node *pNode = doc.getNodeByPath(nodeName);
    if (pNode != nullptr) {
        *outValue = XMLStr2Str(pNode->innerText());
        return (true);
    } else {
        outValue->clear();
        return (false);
    }
}

bool Utility::setFileDates(const KDC::SyncPath &filePath, std::optional<KDC::SyncTime> creationDate,
                           std::optional<KDC::SyncTime> modificationDate, bool &exists) {
    if (!setFileDates_private(filePath,
                              creationDate.has_value() && isCreationDateValid(creationDate.value()) ? creationDate : std::nullopt,
                              modificationDate, exists)) {
        return false;
    }
    return true;
}

bool Utility::isCreationDateValid(uint64_t creationDate) {
    if (creationDate == 0 || creationDate == 443779200) {
        // Do not upload or save on DB invalid dates
        // 443779200 correspond to "Tuesday 24 January 1984 08:00:00" which is a default date set by macOS
        return false;
    }
    return true;
}

std::wstring Utility::s2ws(const std::string &str) {
    Poco::UnicodeConverter converter;
    std::wstring output;
    converter.convert(str, output);
    return output;
}

std::string Utility::ws2s(const std::wstring &wstr) {
    Poco::UnicodeConverter converter;
    std::string output;
    converter.convert(wstr, output);
    return output;
}

std::string Utility::ltrim(const std::string &s) {
    std::string sout(s);
    auto it = std::find_if(sout.begin(), sout.end(), [](char c) { return !std::isspace<char>(c, std::locale::classic()); });
    sout.erase(sout.begin(), it);
    return sout;
}

std::string Utility::rtrim(const std::string &s) {
    std::string sout(s);
    auto it = std::find_if(sout.rbegin(), sout.rend(), [](char c) { return !std::isspace<char>(c, std::locale::classic()); });
    sout.erase(it.base(), sout.end());
    return sout;
}

std::string Utility::trim(const std::string &s) {
    return ltrim(rtrim(s));
}

void Utility::msleep(int msec) {
    std::chrono::milliseconds dura(msec);
    std::this_thread::sleep_for(dura);
}

std::wstring Utility::v2ws(const dbtype &v) {
    return std::visit(VariantPrinter{}, v);
}

std::wstring Utility::formatSyncPath(const SyncPath &path) {
    std::wstringstream ss;
    ss << L"path='" << Path2WStr(path) << L"'";

    return ss.str();
}

std::wstring Utility::formatStdError(const std::error_code &ec) {
#ifdef _WIN32
    std::stringstream ss;
    ss << ec.message() << " (code: " << ec.value() << ")";
    return Utility::s2ws(ss.str());
#elif defined(__unix__)
    std::stringstream ss;
    ss << ec.message() << ". (code: " << ec.value() << ")";
    return Utility::s2ws(ss.str());
#elif defined(__APPLE__)
    return Utility::s2ws(ec.message());
#endif
}

std::wstring Utility::formatStdError(const SyncPath &path, const std::error_code &ec) {
    std::wstringstream ss;
    ss << L"path='" << Path2WStr(path) << L"', err='" << formatStdError(ec) << L"'";

    return ss.str();
}

std::wstring Utility::formatIoError(const SyncPath &path, IoError ioError) {
    std::wstringstream ss;
    ss << L"path='" << Path2WStr(path) << L"', err='" << s2ws(IoHelper::ioError2StdString(ioError)) << L"'";

    return ss.str();
}

std::string Utility::formatRequest(const Poco::URI &uri, const std::string &code, const std::string &description) {
    std::stringstream ss;
    ss << uri.toString().c_str() << " : " << code.c_str() << " - " << description.c_str();

    return ss.str();
}

std::string Utility::formatGenericServerError(std::istream &inputStream, const Poco::Net::HTTPResponse &httpResponse) {
    std::stringstream errorStream;
    errorStream << "Error in reply";

    // Try to parse as string
    if (std::string str(std::istreambuf_iterator<char>(inputStream), {}); !str.empty()) {
        errorStream << ", error: " << str.c_str();
    }

    errorStream << ", content type: " << httpResponse.getContentType().c_str();
    errorStream << ", reason: " << httpResponse.getReason().c_str();

    std::string encoding = httpResponse.get("Content-Encoding", "");
    if (!encoding.empty()) {
        errorStream << ", encoding: " << encoding.c_str();
    }

    return errorStream.str();  // str() return a copy of the underlying string
}

void Utility::logGenericServerError(const std::string &errorTitle, std::istream &inputStream,
                                    const Poco::Net::HTTPResponse &httpResponse) {
    std::string errorMsg = formatGenericServerError(inputStream, httpResponse);
#ifdef NDEBUG
    sentry_capture_event(sentry_value_new_message_event(SENTRY_LEVEL_WARNING, errorTitle.c_str(), errorMsg.c_str()));
#endif
    LOG_WARN(_logger, errorTitle.c_str() << ": " << errorMsg.c_str());
}

#ifdef _WIN32
static std::unordered_map<std::string, bool> rootFsTypeMap;

bool Utility::isNtfs(const SyncPath &dirPath) {
    auto it = rootFsTypeMap.find(dirPath.root_name().string());
    if (it == rootFsTypeMap.end()) {
        std::string fsType = Utility::fileSystemName(dirPath);
        auto val = rootFsTypeMap.insert({dirPath.root_name().string(), fsType == NTFS});
        if (!val.second) {
            // Failed to insert into map
            return false;
        }
        it = val.first;
    }
    return it->second;
}
#endif

std::string Utility::fileSystemName(const SyncPath &dirPath) {
    bool exists;
    IoError ioError = IoErrorSuccess;
    if (!IoHelper::checkIfPathExists(dirPath, exists, ioError)) {
        LOGW_WARN(logger(), L"Error in IoHelper::checkIfPathExists: " << Utility::formatIoError(dirPath, ioError).c_str());
        return std::string();
    }

    if (exists) {
#if defined(__APPLE__)
        struct statfs stat;
        if (statfs(dirPath.native().c_str(), &stat) == 0) {
            return stat.f_fstypename;
        }
#elif defined(_WIN32)
        TCHAR szFileSystemName[MAX_PATH + 1];
        DWORD dwMaxFileNameLength = 0;
        DWORD dwFileSystemFlags = 0;

        if (GetVolumeInformation(dirPath.root_path().c_str(), NULL, 0, NULL, &dwMaxFileNameLength, &dwFileSystemFlags,
                                 szFileSystemName, sizeof(szFileSystemName)) == TRUE) {
            return Utility::ws2s(szFileSystemName);
        } else {
            // Not all the requested information is retrieved
            DWORD dwError = GetLastError();
            LOGW_WARN(logger(), L"Error in GetVolumeInformation for " << Path2WStr(dirPath.root_name()).c_str() << L" ("
                                                                      << dwError << L")");

            // !!! File system name can be OK or not !!!
            return Utility::ws2s(szFileSystemName);
        }
#endif
    } else {
        return fileSystemName(dirPath.parent_path());
    }

    return std::string();
}

bool Utility::startsWith(const std::string &str, const std::string &prefix) {
    return str.size() >= prefix.size() &&
           std::equal(prefix.begin(), prefix.end(), str.begin(), [](char c1, char c2) { return c1 == c2; });
}

bool Utility::startsWithInsensitive(const std::string &str, const std::string &prefix) {
    return str.size() >= prefix.size() && std::equal(prefix.begin(), prefix.end(), str.begin(), [](char c1, char c2) {
               return std::tolower(c1, std::locale()) == std::tolower(c2, std::locale());
           });
}

bool Utility::endsWith(const std::string &str, const std::string &suffix) {
    return str.size() >= suffix.size() && std::equal(str.begin() + str.length() - suffix.length(), str.end(), suffix.begin(),
                                                     [](char c1, char c2) { return c1 == c2; });
}

bool Utility::endsWithInsensitive(const std::string &str, const std::string &suffix) {
    return str.size() >= suffix.size() &&
           std::equal(str.begin() + str.length() - suffix.length(), str.end(), suffix.begin(),
                      [](char c1, char c2) { return std::tolower(c1, std::locale()) == std::tolower(c2, std::locale()); });
}

bool Utility::isEqualInsensitive(const std::string &strA, const std::string &strB) {
    return strA.size() == strB.size() && std::equal(strA.begin(), strA.end(), strB.begin(), strB.end(), [](char a, char b) {
               return std::tolower(a, std::locale()) == std::tolower(b, std::locale());
           });
}

#ifdef _WIN32
bool Utility::startsWithInsensitive(const SyncName &str, const SyncName &prefix) {
    return str.size() >= prefix.size() && std::equal(prefix.begin(), prefix.end(), str.begin(), [](SyncChar c1, SyncChar c2) {
               return std::tolower(c1, std::locale()) == std::tolower(c2, std::locale());
           });
}

bool Utility::startsWith(const SyncName &str, const SyncName &prefix) {
    return str.size() >= prefix.size() &&
           std::equal(prefix.begin(), prefix.end(), str.begin(), [](SyncChar c1, SyncChar c2) { return c1 == c2; });
}

bool Utility::endsWith(const SyncName &str, const SyncName &suffix) {
    return str.size() >= suffix.size() && std::equal(str.begin() + str.length() - suffix.length(), str.end(), suffix.begin(),
                                                     [](char c1, char c2) { return c1 == c2; });
}

bool Utility::endsWithInsensitive(const SyncName &str, const SyncName &suffix) {
    return str.size() >= suffix.size() &&
           std::equal(str.begin() + str.length() - suffix.length(), str.end(), suffix.begin(),
                      [](char c1, char c2) { return std::tolower(c1, std::locale()) == std::tolower(c2, std::locale()); });
}

bool Utility::isEqualInsensitive(const SyncName &a, const SyncName &b) {
    return a.size() == b.size() && std::equal(a.begin(), a.end(), b.begin(), b.end(), [](SyncChar a, SyncChar b) {
               return std::tolower(a, std::locale()) == std::tolower(b, std::locale());
           });
}
#endif

bool Utility::moveItemToTrash(const SyncPath &itemPath) {
    return moveItemToTrash_private(itemPath);
}

#ifdef __APPLE__
bool Utility::preventSleeping(bool enable) {
    return preventSleeping_private(enable);
}
#endif

void Utility::str2hexstr(const std::string &str, std::string &hexstr, bool capital) {
    hexstr.resize(str.size() * 2);
    const char a = capital ? 'A' - 1 : 'a' - 1;

    size_t i;
    int c;
    for (i = 0, c = str[0] & 0xFF; i < hexstr.size(); c = str[i / 2] & 0xFF) {
        hexstr[i++] = c > 0x9F ? (c / 16 - 9) | a : c / 16 | '0';
        hexstr[i++] = (c & 0xF) > 9 ? (c % 16 - 9) | a : c % 16 | '0';
    }
}

// Convert string of hex numbers to its equivalent char-stream
void Utility::strhex2str(const std::string &hexstr, std::string &str) {
    str.resize((hexstr.size() + 1) / 2);

    for (size_t i = 0, j = 0; i < str.size(); i++, j++) {
        str[i] = (hexstr[j] & '@' ? hexstr[j] + 9 : hexstr[j]) << 4, j++;
        str[i] |= (hexstr[j] & '@' ? hexstr[j] + 9 : hexstr[j]) & 0xF;
    }
}

std::vector<std::string> Utility::splitStr(const std::string &str, char sep) {
    std::vector<std::string> strings;
    std::istringstream ss(str);
    std::string s;
    while (getline(ss, s, sep)) {
        strings.push_back(s);
    }
    return strings;
}

std::string Utility::joinStr(const std::vector<std::string> &strList, char sep /*= 0*/) {
    std::string str;
    for (std::vector<std::string>::const_iterator it = strList.begin(); it != strList.end();) {
        str += *it;
        it++;
        if (sep > 0 && it != strList.end()) {
            str += sep;
        }
    }
    return str;
}

std::string Utility::opType2Str(OperationType opType) {
    switch (opType) {
        case OperationTypeCreate:
            return "CREATE";
        case OperationTypeDelete:
            return "DELETE";
        case OperationTypeEdit:
            return "EDIT";
        case OperationTypeMove:
            return "MOVE";
        case OperationTypeRights:
            return "RIGHTS";
        default:
            return "UNKNOWN";
    }
}

std::string Utility::conflictType2Str(ConflictType conflictType) {
    switch (conflictType) {
        case ConflictTypeMoveParentDelete: {
            return "Move-ParentDelete";
        }
        case ConflictTypeMoveDelete: {
            return "Move-Delete";
        }
        case ConflictTypeCreateParentDelete: {
            return "Create-ParentDelete";
        }
        case ConflictTypeMoveMoveSource: {
            return "Move-Move(Source)";
        }
        case ConflictTypeMoveMoveDest: {
            return "Move-Move(Dest)";
        }
        case ConflictTypeMoveCreate: {
            return "Move-Create";
        }
        case ConflictTypeEditDelete: {
            return "Edit-Delete";
        }
        case ConflictTypeCreateCreate: {
            return "Create-Create";
        }
        case ConflictTypeEditEdit: {
            return "Edit-Edit";
        }
        case ConflictTypeMoveMoveCycle: {
            return "Move-Move(Cycle)";
        }
        default: {
            return "Unknown";
        }
    }
}

std::string Utility::side2Str(ReplicaSide side) {
    switch (side) {
        case ReplicaSideLocal: {
            return "Local";
        }
        case ReplicaSideRemote: {
            return "Remote";
        }
        default: {
            return "Unknown";
        }
    }
}

std::string Utility::nodeType2Str(NodeType type) {
    switch (type) {
        case NodeTypeDirectory: {
            return "Directory";
        }
        case NodeTypeFile: {
            return "File";
        }
        default: {
            return "Unknown";
        }
    }
}

std::string Utility::logLevel2Str(LogLevel level) {
    switch (level) {
        case LogLevelDebug: {
            return "debug";
        }
        case LogLevelInfo: {
            return "info";
        }
        case LogLevelWarning: {
            return "warning";
        }
        case LogLevelError: {
            return "error";
        }
        case LogLevelFatal: {
            return "fatal";
        }
        default:
            break;
    }

    return std::string();
}

std::string Utility::syncFileStatus2Str(SyncFileStatus status) {
    switch (status) {
        case SyncFileStatusUnknown: {
            return "Unknown";
        }
        case SyncFileStatusError: {
            return "Error";
        }
        case SyncFileStatusSuccess: {
            return "Success";
        }
        case SyncFileStatusConflict: {
            return "Conflict";
        }
        case SyncFileStatusInconsistency: {
            return "Inconsistency";
        }
        case SyncFileStatusIgnored: {
            return "Ignored";
        }
        case SyncFileStatusSyncing: {
            return "Syncing";
        }
    }

    return "";
}

std::string Utility::list2str(std::unordered_set<std::string> inList) {
    bool first = true;
    std::string out;
    for (const auto &str : inList) {
        if (!first) {
            out += ",";
        }
        if (!str.empty()) {
            out += str;
            first = false;
        }
    }
    return out;
}

std::string Utility::list2str(std::list<std::string> inList) {
    bool first = true;
    std::string out;
    for (const auto &str : inList) {
        if (!first) {
            out += ",";
        }
        if (!str.empty()) {
            out += str;
            first = false;
        }
    }
    return out;
}

int Utility::pathDepth(const SyncPath path) {
    int level = 0;
    for (auto it = path.begin(); it != path.end(); ++it) {
        level++;
    }
    return level;
}

std::string Utility::computeMd5Hash(const std::string &in) {
    Poco::MD5Engine md5;
    md5.update(in);
    return Poco::DigestEngine::digestToHex(md5.digest());
}

std::string Utility::computeMd5Hash(const char *in, std::size_t length) {
    Poco::MD5Engine md5;
    md5.update(in, length);
    return Poco::DigestEngine::digestToHex(md5.digest());
}

std::string Utility::computeXxHash(const std::string &in) {
    return computeXxHash(in.data(), in.size());
}

std::string Utility::computeXxHash(const char *in, std::size_t length) {
    XXH64_hash_t hash = XXH3_64bits(in, length);
    return xxHashToStr(hash);
}

#define BUF_SIZE 32

std::string Utility::xxHashToStr(XXH64_hash_t hash) {
    XXH64_canonical_t canonical;
    XXH64_canonicalFromHash(&canonical, hash);
    std::string output;
    char buf[BUF_SIZE];
    for (unsigned int i = 0; i < sizeof(canonical.digest); ++i) {
        std::snprintf(buf, BUF_SIZE, "%02x", canonical.digest[i]);
        output.append(buf);
    }
    return output;
}

#if defined(__APPLE__)
SyncName Utility::getExcludedAppFilePath(bool test /*= false*/) {
    return (test ? (testResourcesPath + excludedAppFileName)
                 : (CommonUtility::getAppWorkingDir() / binRelativePath() / excludedAppFileName).native());
}
#endif

SyncName Utility::getExcludedTemplateFilePath(bool test /*= false*/) {
    return (test ? testResourcesPath + excludedTemplateFileName
                 : (CommonUtility::getAppWorkingDir() / binRelativePath() / excludedTemplateFileName).native());
}

SyncPath Utility::binRelativePath() {
    SyncPath path(resourcesPath);

#ifdef __unix__
    if (getenv("APPIMAGE") != NULL) {
        path = path / "usr/bin";
    }
#endif

    return path;
}

SyncName Utility::logFileName() {
    SyncName name = SyncName(Str2SyncName(APPLICATION_NAME)) + SyncName(Str2SyncName(LOGFILE_EXT));
    return name;
}

SyncName Utility::logFileNameWithTime() {
    std::time_t now = std::time(nullptr);
    std::tm tm = *std::localtime(&now);
    OStringStream woss;
    woss << std::put_time(&tm, SyncName(Str2SyncName(LOGFILE_TIME_FORMAT)).c_str()) << Str("_") << logFileName();
    return woss.str();
}

std::string Utility::toUpper(const std::string &str) {
    std::string upperStr(str);
    std::transform(str.begin(), str.end(), upperStr.begin(), [](unsigned char c) { return std::toupper(c); });

    return upperStr;
}

std::string Utility::errId(const char *file, int line) {
    std::string err =
        Utility::toUpper(std::filesystem::path(file).filename().stem().string().substr(0, 3)) + ":" + std::to_string(line);

    return err;
}

// Be careful, some characters have 2 different encodings in Unicode
// For example 'é' can be coded as 0x65 + 0xcc + 0x81  or 0xc3 + 0xa9
SyncName Utility::normalizedSyncName(const SyncName &name) {
#ifdef _WIN32
    if (name.empty()) {
        return name;
    }

    static const int maxIterations = 10;
    LPWSTR strResult = NULL;
    HANDLE hHeap = GetProcessHeap();

    int iSizeEstimated = NormalizeString(NormalizationC, name.c_str(), -1, NULL, 0);
    for (int i = 0; i < maxIterations; i++) {
        if (strResult) {
            HeapFree(hHeap, 0, strResult);
        }
        strResult = (LPWSTR)HeapAlloc(hHeap, 0, iSizeEstimated * sizeof(WCHAR));
        iSizeEstimated = NormalizeString(NormalizationC, name.c_str(), -1, strResult, iSizeEstimated);

        if (iSizeEstimated > 0) {
            break;  // success
        }

        if (iSizeEstimated <= 0) {
            DWORD dwError = GetLastError();
            if (dwError != ERROR_INSUFFICIENT_BUFFER) {
                // Real error, not buffer error
                LOGW_WARN(_logger, L"Failed to normalize string " << SyncName2WStr(name).c_str() << L" - error code: "
                                                                  << std::to_wstring(dwError));

#ifdef NDEBUG
                sentry_capture_event(sentry_value_new_message_event(SENTRY_LEVEL_FATAL, "Utility::normalizedSyncName",
                                                                    "Failed to normalize string"));
#endif

                throw std::runtime_error("Failed to normalize string");
            }

            // New guess is negative of the return value.
            iSizeEstimated = -iSizeEstimated;
        }
    }

    if (iSizeEstimated <= 0) {
        DWORD dwError = GetLastError();
        LOGW_WARN(_logger, L"Failed to normalize string " << SyncName2WStr(name).c_str() << L" - error code: "
                                                          << std::to_wstring(dwError));

#ifdef NDEBUG
        sentry_capture_event(
            sentry_value_new_message_event(SENTRY_LEVEL_FATAL, "Utility::normalizedSyncName", "Failed to normalize string"));
#endif

        throw std::runtime_error("Failed to normalize string");
    }

    SyncName syncName(strResult);
    HeapFree(hHeap, 0, strResult);
    return syncName;
#else
    const char *str = reinterpret_cast<const char *>(utf8proc_NFC(reinterpret_cast<const uint8_t *>(name.c_str())));
    if (!str) {  // Some special characters seem to be not supported, therefore a null pointer is returned if the conversion has
                 // failed. e.g.: Linux can sometime send filesystem events with strange charater in the path
        return "";  // TODO : we should return a boolean value to explicitly say that the conversion has failed. Output value
                    // should be passed by reference as a parameter.
    }

    SyncName syncName(str);
    std::free((void *)str);
    return syncName;
#endif
}

SyncPath Utility::normalizedSyncPath(const SyncPath &path) noexcept {
    auto segmentIt = path.begin();
    if (segmentIt == path.end()) return {};

    auto segment = *segmentIt;
    if (segmentIt->native() != Str("/")) segment = normalizedSyncName(segment);

    SyncPath result{segment};
    ++segmentIt;

    for (; segmentIt != path.end(); ++segmentIt) {
        if (segmentIt->native() != Str("/")) {
            result /= normalizedSyncName(*segmentIt);
        }
    }

    return result;
}

bool Utility::checkIfDirEntryIsManaged(std::filesystem::recursive_directory_iterator &dirIt, bool &isManaged, IoError &ioError) {
    isManaged = true;
    ioError = IoErrorSuccess;

    ItemType itemType;
    if (!IoHelper::getItemType(dirIt->path(), itemType)) {
        ioError = itemType.ioError;
        LOGW_WARN(logger(), L"Error in IoHelper::getItemType: " << Utility::formatIoError(dirIt->path(), ioError).c_str());
        return false;
    }

    if (itemType.ioError == IoErrorNoSuchFileOrDirectory || itemType.ioError == IoErrorAccessDenied) {
        LOGW_DEBUG(logger(), L"Error in IoHelper::getItemType: " << formatIoError(dirIt->path(), ioError).c_str());
        return true;
    }

    bool isLink = itemType.linkType != LinkTypeNone;
    if (!dirIt->is_directory() && !dirIt->is_regular_file() && !isLink) {
        LOGW_WARN(logger(), L"Ignore " << formatSyncPath(dirIt->path()).c_str()
                                       << L" because it's not a directory, a regular file or a symlink");
        isManaged = false;
        return true;
    }

    if (dirIt->path().native().length() > CommonUtility::maxPathLength()) {
        LOGW_WARN(logger(),
                  L"Ignore " << formatSyncPath(dirIt->path()).c_str() << L" because size > " << CommonUtility::maxPathLength());
        isManaged = false;
        return true;
    }

    return true;
}

bool Utility::getLinuxDesktopType(std::string &currentDesktop) {
    const char *xdgDesktopEnv = std::getenv("XDG_CURRENT_DESKTOP");
    if (!xdgDesktopEnv) {
        return false;
    }

    std::string xdgCurrentDesktop(xdgDesktopEnv);
    // ':' is the separator in the env variable, like "ubuntu:GNOME"
    size_t colon_pos = xdgCurrentDesktop.find(':');

    if (colon_pos != std::string::npos) {
        currentDesktop = xdgCurrentDesktop.substr(colon_pos + 1);
        return true;
    }

    return false;
}

#ifdef _WIN32
bool Utility::fileExists(DWORD dwError) noexcept {
    return (dwError != ERROR_FILE_NOT_FOUND && dwError != ERROR_PATH_NOT_FOUND && dwError != ERROR_INVALID_DRIVE);
}

bool Utility::longPath(const SyncPath &shortPathIn, SyncPath &longPathOut, bool &notFound) {
    int length = GetLongPathNameW(shortPathIn.native().c_str(), 0, 0);
    if (!length) {
        const bool exists = fileExists(GetLastError());
        if (!exists) {
            notFound = true;
        }
        return false;
    }

    SyncChar *buffer = new SyncChar[length + 1];
    if (!buffer) {
        return false;
    }

    length = GetLongPathNameW(shortPathIn.native().c_str(), buffer, length);
    if (!length) {
        const bool exists = fileExists(GetLastError());
        if (!exists) {
            notFound = true;
        }
        delete[] buffer;
        return false;
    }

    buffer[length] = 0;
    longPathOut = SyncPath(buffer);
    delete[] buffer;

    return true;
}
#endif


bool Utility::totalRamAvailable(uint64_t &ram, int &errorCode) {
    if (totalRamAvailable_private(ram, errorCode)) {
        return true;
    }
    // log errorCode;
    return false;
}

bool Utility::ramCurrentlyUsed(uint64_t &ram, int &errorCode) {
    if (ramCurrentlyUsed_private(ram, errorCode)) {
        return true;
    }
    // log errorCode;
    return false;
}

bool Utility::ramCurrentlyUsedByProcess(uint64_t &ram, int &errorCode) {
    if (ramCurrentlyUsedByProcess_private(ram, errorCode)) {
        return true;
    }
    // log errorCode;
    return false;
}


bool Utility::cpuUsage(uint64_t &lastTotalUser, uint64_t &lastTotalUserLow, uint64_t &lastTotalSys, uint64_t &lastTotalIdle,
                       double &percent) {
#ifdef __unix__
    return cpuUsage_private(lastTotalUser, lastTotalUserLow, lastTotalSys, lastTotalIdle, percent);
#else
    (void)(lastTotalUser);
    (void)(lastTotalUserLow);
    (void)(lastTotalSys);
    (void)(lastTotalIdle);
    (void)(percent);
#endif
    return false;
}

bool Utility::cpuUsage(uint64_t &previousTotalTicks, uint64_t &previousIdleTicks, double &percent) {
#ifdef __APPLE__
    return cpuUsage_private(previousTotalTicks, previousIdleTicks, percent);
#else
    (void)(previousTotalTicks);
    (void)(previousIdleTicks);
    (void)(percent);
#endif
    return false;
}

bool Utility::cpuUsageByProcess(double &percent) {
    return cpuUsageByProcess_private(percent);
}

SyncPath Utility::commonDocumentsFolderName() {
    return Str2SyncName(COMMON_DOC_FOLDER);
}

SyncPath Utility::sharedFolderName() {
    return Str2SyncName(SHARED_FOLDER);
}

}  // namespace KDC
