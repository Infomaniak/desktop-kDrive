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

int64_t Utility::getFreeDiskSpace(const SyncPath &path) {
    bool isDirectory = false;
    IoError ioError = IoError::Unknown;

    IoHelper::checkIfIsDirectory(path, isDirectory, ioError);
    if (ioError == IoError::NoSuchFileOrDirectory) {
        isDirectory = false;
    } else if (ioError != IoError::Success) {
        return -1;
    }
    SyncPath tmpDirPath = isDirectory ? path : path.parent_path();

#if defined(__APPLE__)
    struct statvfs stat;
    if (statvfs(tmpDirPath.c_str(), &stat) == 0) {
        return (int64_t) (stat.f_bavail * stat.f_frsize);
    }
#elif defined(__unix__)
    struct statvfs64 stat;
    if (statvfs64(tmpDirPath.c_str(), &stat) == 0) {
        return (int64_t) (stat.f_bavail * stat.f_frsize);
    }
#elif defined(_WIN32)
    ULARGE_INTEGER freeBytes;
    freeBytes.QuadPart = 0L;
    if (GetDiskFreeSpaceEx(reinterpret_cast<const wchar_t *>(Path2WStr(tmpDirPath).c_str()), &freeBytes, NULL, NULL)) {
        return freeBytes.QuadPart;
    }
#endif
    return -1;
}

int64_t Utility::freeDiskSpaceLimit() {
    static int64_t limit = 250 * 1000 * 1000LL; // 250MB
    return limit;
}

bool Utility::enoughSpace(const SyncPath &path) {
    const int64_t freeBytes = getFreeDiskSpace(path);
    if (freeBytes >= 0) {
        if (freeBytes < freeDiskSpaceLimit()) {
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
                           std::optional<KDC::SyncTime> modificationDate, bool symlink, bool &exists) {
    if (!setFileDates_private(filePath,
                              creationDate.has_value() && isCreationDateValid(creationDate.value()) ? creationDate : std::nullopt,
                              modificationDate, symlink, exists)) {
        return false;
    }
    return true;
}

bool Utility::isCreationDateValid(int64_t creationDate) {
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
    const auto it =
            std::find_if(sout.begin(), sout.end(), [](const char c) { return !std::isspace<char>(c, std::locale::classic()); });
    sout.erase(sout.begin(), it);
    return sout;
}

std::string Utility::rtrim(const std::string &s) {
    std::string sout(s);
    const auto it =
            std::find_if(sout.rbegin(), sout.rend(), [](const char c) { return !std::isspace<char>(c, std::locale::classic()); });
    sout.erase(it.base(), sout.end());
    return sout;
}

std::string Utility::trim(const std::string &s) {
    return ltrim(rtrim(s));
}

#ifdef _WIN32
SyncName Utility::ltrim(const SyncName &s) {
    SyncName sout(s);
    const auto it =
            std::find_if(sout.begin(), sout.end(), [](const char c) { return !std::isspace<char>(c, std::locale::classic()); });
    sout.erase(sout.begin(), it);
    return sout;
}

SyncName Utility::rtrim(const SyncName &s) {
    SyncName sout(s);
    const auto it =
            std::find_if(sout.rbegin(), sout.rend(), [](const char c) { return !std::isspace<char>(c, std::locale::classic()); });
    sout.erase(it.base(), sout.end());
    return sout;
}

SyncName Utility::trim(const SyncName &s) {
    return ltrim(rtrim(s));
}
#endif

void Utility::msleep(int msec) {
    std::chrono::milliseconds dura(msec);
    std::this_thread::sleep_for(dura);
}

std::wstring Utility::v2ws(const dbtype &v) {
    return std::visit(VariantPrinter{}, v);
}

std::wstring Utility::formatSyncName(const SyncName &name) {
    std::wstringstream ss;
    ss << L"name='" << SyncName2WStr(name) << L"'";

    return ss.str();
}

std::wstring Utility::formatSyncPath(const SyncPath &path) {
    std::wstringstream ss;
    ss << L"path='" << Path2WStr(path) << L"'";

    return ss.str();
}


std::wstring Utility::formatPath(const QString &path) {
    std::wstringstream ss;
    ss << L"path='" << QStr2WStr(path) << L"'";

    return ss.str();
}

std::wstring Utility::formatStdError(const std::error_code &ec) {
#ifdef _WIN32
    std::stringstream ss;
    ss << ec.message() << " (code: " << ec.value() << ")";
    return s2ws(ss.str());
#elif defined(__unix__)
    std::stringstream ss;
    ss << ec.message() << ". (code: " << ec.value() << ")";
    return s2ws(ss.str());
#elif defined(__APPLE__)
    return s2ws(ec.message());
#endif
}

std::wstring Utility::formatStdError(const SyncPath &path, const std::error_code &ec) {
    std::wstringstream ss;
    ss << L"path='" << Path2WStr(path) << L"', err='" << formatStdError(ec) << L"'";

    return ss.str();
}

std::wstring Utility::formatIoError(const IoError ioError) {
    std::wstringstream ss;
    ss << s2ws(IoHelper::ioError2StdString(ioError));

    return ss.str();
}

std::wstring Utility::formatIoError(const SyncPath &path, const IoError ioError) {
    std::wstringstream ss;
    ss << L"path='" << Path2WStr(path) << L"', err='" << formatIoError(ioError) << L"'";

    return ss.str();
}

std::wstring Utility::formatIoError(const QString &path, const IoError ioError) {
    return formatIoError(QStr2Path(path), ioError);
}

std::wstring Utility::formatErrno(const SyncPath &path, long cError) {
    std::wstringstream ss;
    ss << L"path='" << Path2WStr(path) << L"', errno=" << cError;

    return ss.str();
}

std::wstring Utility::formatErrno(const QString &path, long cError) {
    return formatErrno(QStr2Path(path), cError);
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
    if (const std::string str(std::istreambuf_iterator<char>(inputStream), {}); !str.empty()) {
        errorStream << ", error: " << str.c_str();
    }

    errorStream << ", content type: " << httpResponse.getContentType().c_str();
    errorStream << ", reason: " << httpResponse.getReason().c_str();

    const std::string encoding = httpResponse.get("Content-Encoding", "");
    if (!encoding.empty()) {
        errorStream << ", encoding: " << encoding.c_str();
    }

    return errorStream.str(); // str() return a copy of the underlying string
}

void Utility::logGenericServerError(const log4cplus::Logger &logger, const std::string &errorTitle, std::istream &inputStream,
                                    const Poco::Net::HTTPResponse &httpResponse) {
    std::string errorMsg = formatGenericServerError(inputStream, httpResponse);
    sentry::Handler::captureMessage(sentry::Level::Warning, errorTitle, errorMsg);
    LOG_WARN(logger, errorTitle.c_str() << ": " << errorMsg.c_str());
}

#ifdef _WIN32
static std::unordered_map<std::string, bool> rootFsTypeMap;

bool Utility::isNtfs(const SyncPath &targetPath) {
    auto it = rootFsTypeMap.find(targetPath.root_name().string());
    if (it == rootFsTypeMap.end()) {
        std::string fsType = fileSystemName(targetPath);
        auto val = rootFsTypeMap.insert({targetPath.root_name().string(), fsType == NTFS});
        if (!val.second) {
            // Failed to insert into map
            return false;
        }
        it = val.first;
    }
    return it->second;
}
#endif

std::string Utility::fileSystemName(const SyncPath &targetPath) {
#if defined(__APPLE__)
    struct statfs stat;
    if (statfs(targetPath.root_path().native().c_str(), &stat) == 0) {
        return stat.f_fstypename;
    }
#elif defined(_WIN32)
    TCHAR szFileSystemName[MAX_PATH + 1];
    DWORD dwMaxFileNameLength = 0;
    DWORD dwFileSystemFlags = 0;

    if (GetVolumeInformation(targetPath.root_path().c_str(), NULL, 0, NULL, &dwMaxFileNameLength, &dwFileSystemFlags,
                             szFileSystemName, sizeof(szFileSystemName)) == TRUE) {
        return ws2s(szFileSystemName);
    } else {
        // Not all the requested information is retrieved
        DWORD dwError = GetLastError();
        LOGW_WARN(logger(), L"Error in GetVolumeInformation for " << formatSyncName(targetPath.root_name()) << L" ("
                                                                  << CommonUtility::getErrorMessage(dwError) << L")");

        // !!! File system name can be OK or not !!!
        return ws2s(szFileSystemName);
    }
#else
    (void) targetPath;
#endif

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
    return str.size() >= suffix.size() && std::equal(str.begin() + static_cast<long>(str.length() - suffix.length()), str.end(),
                                                     suffix.begin(), [](char c1, char c2) { return c1 == c2; });
}

bool Utility::endsWithInsensitive(const std::string &str, const std::string &suffix) {
    return str.size() >= suffix.size() &&
           std::equal(str.begin() + static_cast<long>(str.length() - suffix.length()), str.end(), suffix.begin(),
                      [](char c1, char c2) { return std::tolower(c1, std::locale()) == std::tolower(c2, std::locale()); });
}

bool Utility::isEqualInsensitive(const std::string &strA, const std::string &strB) {
    return strA.size() == strB.size() && std::equal(strA.begin(), strA.end(), strB.begin(), strB.end(), [](char a, char b) {
               return std::tolower(a, std::locale()) == std::tolower(b, std::locale());
           });
}

bool Utility::contains(const std::string &str, const std::string &substr) {
    return str.find(substr) != std::string::npos;
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

bool Utility::checkIfSameNormalization(const SyncName &a, const SyncName &b, bool &areSame) {
    SyncName aNormalized;
    if (!normalizedSyncName(a, aNormalized)) {
        LOGW_DEBUG(logger(), L"Error in Utility::normalizedSyncName: " << formatSyncName(a));
        return false;
    }
    SyncName bNormalized;
    if (!normalizedSyncName(b, bNormalized)) {
        LOGW_DEBUG(logger(), L"Error in Utility::normalizedSyncName: " << formatSyncName(b));
        return false;
    }
    areSame = (aNormalized == bNormalized);
    return true;
}

bool Utility::checkIfSameNormalization(const SyncPath &a, const SyncPath &b, bool &areSame) {
    SyncPath aNormalized;
    if (!normalizedSyncPath(a, aNormalized)) {
        LOGW_DEBUG(logger(), L"Error in Utility::normalizedSyncPath: " << formatSyncPath(a));
        return false;
    }
    SyncPath bNormalized;
    if (!normalizedSyncPath(b, bNormalized)) {
        LOGW_DEBUG(logger(), L"Error in Utility::normalizedSyncPath: " << formatSyncPath(b));
        return false;
    }
    areSame = (aNormalized == bNormalized);
    return true;
}

bool Utility::checkIfEquivalent(const SyncName &lhs, const SyncName &rhs, bool &isEquivalent, bool normalizeNames) {
    isEquivalent = false;

    if (normalizeNames) {
        if (!Utility::checkIfSameNormalization(lhs, rhs, isEquivalent)) {
            LOGW_WARN(logger(), L"Error in Utility::checkIfSameNormalization: " << Utility::formatSyncName(lhs) << L" / "
                                                                                << Utility::formatSyncName(rhs));
            return false;
        }
    } else {
        isEquivalent = lhs == rhs;
    }

    return true;
}

std::vector<SyncName> Utility::splitPath(const SyncPath &path) {
    std::vector<SyncName> itemNames;
    SyncPath pathTmp(path);

    while (pathTmp != pathTmp.root_path()) {
        itemNames.emplace_back(pathTmp.filename().native());
        pathTmp = pathTmp.parent_path();
    }

    return itemNames;
}

bool Utility::isDescendantOrEqual(const SyncPath &potentialDescendant, const SyncPath &path) {
    if (path == potentialDescendant) return true;
    for (auto it = potentialDescendant.begin(), it2 = path.begin(); it != potentialDescendant.end(); ++it, ++it2) {
        if (it2 == path.end()) {
            return true;
        }
        if (*it != *it2) {
            return false;
        }
    }
    return false;
}

bool Utility::moveItemToTrash(const SyncPath &itemPath) {
    return moveItemToTrash_private(itemPath);
}

#ifdef __APPLE__
bool Utility::preventSleeping(bool enable) {
    return preventSleeping_private(enable);
}
#endif

void Utility::restartFinderExtension() {
#ifdef __APPLE__
    restartFinderExtension_private();
#endif
}

void Utility::str2hexstr(const std::string &str, std::string &hexstr, bool capital) {
    hexstr.resize(str.size() * 2);
    const char a = capital ? 'A' - 1 : 'a' - 1;

    size_t i;
    int c;
    for (i = 0, c = str[0] & 0xFF; i < hexstr.size(); c = str[i / 2] & 0xFF) {
        hexstr[i++] = c > 0x9F ? static_cast<char>(c / 16 - 9) | a : static_cast<char>(c / 16) | '0';
        hexstr[i++] = (c & 0xF) > 9 ? static_cast<char>(c % 16 - 9) | a : static_cast<char>(c % 16) | '0';
    }
}

// Convert string of hex numbers to its equivalent char-stream
void Utility::strhex2str(const std::string &hexstr, std::string &str) {
    str.resize((hexstr.size() + 1) / 2);

    for (size_t i = 0, j = 0; i < str.size(); i++, j++) {
        str[i] = static_cast<char>((hexstr[j] & '@' ? hexstr[j] + 9 : hexstr[j]) << 4), j++;
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

std::string Utility::list2str(std::unordered_set<std::string> inList) {
    bool first = true;
    std::string out;
    for (const auto &str: inList) {
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
    for (const auto &str: inList) {
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
    return (test ? excludedAppFileName : (CommonUtility::getAppWorkingDir() / binRelativePath() / excludedAppFileName).native());
}
#endif

SyncName Utility::getExcludedTemplateFilePath(bool test /*= false*/) {
    return (test ? excludedTemplateFileName
                 : (CommonUtility::getAppWorkingDir() / binRelativePath() / excludedTemplateFileName).native());
}

SyncPath Utility::binRelativePath() {
    SyncPath path(resourcesPath);
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
    // std::ranges::transform(str, upperStr.begin(), [](unsigned char c) { return std::toupper(c); });   // Needs gcc-11
    std::transform(str.begin(), str.end(), upperStr.begin(), [](unsigned char c) { return std::toupper(c); });
    return upperStr;
}

std::string Utility::_errId(const char *file, int line) {
    std::string err = toUpper(std::filesystem::path(file).filename().stem().string().substr(0, 3)) + ":" + std::to_string(line);
    return err;
}

// Be careful, some characters have 2 different encodings in Unicode
// For example 'é' can be coded as 0x65 + 0xcc + 0x81  or 0xc3 + 0xa9
bool Utility::normalizedSyncName(const SyncName &name, SyncName &normalizedName,
                                 const UnicodeNormalization normalization) noexcept {
    if (name.empty()) {
        normalizedName = name;
        return true;
    }

#ifdef _WIN32
    static const int maxIterations = 10;
    LPWSTR strResult = nullptr;
    HANDLE hHeap = GetProcessHeap();

    int64_t iSizeEstimated = static_cast<int64_t>(name.length() + 1) * 2;
    for (int i = 0; i < maxIterations; i++) {
        if (strResult) {
            HeapFree(hHeap, 0, strResult);
        }
        strResult = (LPWSTR) HeapAlloc(hHeap, 0, iSizeEstimated * sizeof(WCHAR));
        iSizeEstimated = NormalizeString(normalization == UnicodeNormalization::NFD ? NormalizationD : NormalizationC,
                                         name.c_str(), -1, strResult, iSizeEstimated);

        if (iSizeEstimated > 0) {
            break; // success
        }

        if (iSizeEstimated <= 0) {
            const DWORD dwError = GetLastError();
            if (dwError != ERROR_INSUFFICIENT_BUFFER) {
                // Real error, not buffer error
                LOGW_DEBUG(logger(), L"Failed to normalize " << formatSyncName(name) << L" ("
                                                             << CommonUtility::getErrorMessage(dwError) << L")");
                return false;
            }

            // New guess is negative of the return value.
            iSizeEstimated = -iSizeEstimated;
        }
    }

    if (iSizeEstimated <= 0) {
        DWORD dwError = GetLastError();
        LOGW_DEBUG(logger(),
                   L"Failed to normalize " << formatSyncName(name) << L" (" << CommonUtility::getErrorMessage(dwError) << L")");
        return false;
    }

    (void) normalizedName.assign(strResult, iSizeEstimated - 1);
    HeapFree(hHeap, 0, strResult);
    return true;
#else
    char *strResult = nullptr;
    if (normalization == UnicodeNormalization::NFD) {
        strResult = reinterpret_cast<char *>(utf8proc_NFD(reinterpret_cast<const uint8_t *>(name.c_str())));
    } else {
        strResult = reinterpret_cast<char *>(utf8proc_NFC(reinterpret_cast<const uint8_t *>(name.c_str())));
    }

    if (!strResult) { // Some special characters seem to be not supported, therefore a null pointer is returned if the
                      // conversion has failed. e.g.: Linux can sometimes send filesystem events with strange characters in
                      // the path
        LOGW_DEBUG(logger(), L"Failed to normalize " << formatSyncName(name));
        return false;
    }

    normalizedName = SyncName(strResult);
    std::free((void *) strResult);
    return true;
#endif
}

bool Utility::normalizedSyncPath(const SyncPath &path, SyncPath &normalizedPath,
                                 const UnicodeNormalization normalization /*= UnicodeNormalization::NFC*/) noexcept {
    auto segmentIt = path.begin();
    if (segmentIt == path.end()) return true;

    auto segment = *segmentIt;
    if (segmentIt->lexically_normal() != SyncPath(Str("/")).lexically_normal()) {
        SyncName normalizedName;
        if (!normalizedSyncName(segment, normalizedName, normalization)) {
            LOGW_DEBUG(logger(), L"Error in Utility::normalizedSyncName: " << formatSyncName(segment));
            return false;
        }
        segment = normalizedName;
    }

    normalizedPath = SyncPath(segment);
    ++segmentIt;
    for (; segmentIt != path.end(); ++segmentIt) {
        if (segmentIt->lexically_normal() != SyncPath(Str("/")).lexically_normal()) {
            SyncName normalizedName;
            if (!normalizedSyncName(*segmentIt, normalizedName, normalization)) {
                LOGW_DEBUG(logger(), L"Error in Utility::normalizedSyncName: " << formatSyncName(*segmentIt));
                return false;
            }
            normalizedPath /= normalizedName;
        }
    }

    return true;
}

bool Utility::checkIfDirEntryIsManaged(const std::filesystem::recursive_directory_iterator &dirIt, bool &isManaged, bool &isLink,
                                       IoError &ioError) {
    return checkIfDirEntryIsManaged(*dirIt, isManaged, isLink, ioError);
}

bool Utility::checkIfDirEntryIsManaged(const DirectoryEntry &dirEntry, bool &isManaged, bool &isLink, IoError &ioError) {
    isManaged = true;
    isLink = false;
    ioError = IoError::Success;

    ItemType itemType;
    bool result = IoHelper::getItemType(dirEntry.path(), itemType);
    ioError = itemType.ioError;
    if (!result) {
        LOGW_WARN(logger(), L"Error in IoHelper::getItemType: " << formatIoError(dirEntry.path(), ioError));
        return false;
    }

    if (itemType.ioError == IoError::NoSuchFileOrDirectory || itemType.ioError == IoError::AccessDenied) {
        LOGW_DEBUG(logger(), L"Error in IoHelper::getItemType: " << formatIoError(dirEntry.path(), ioError));
        return true;
    }

    isLink = itemType.linkType != LinkType::None;
    if (!dirEntry.is_directory() && !dirEntry.is_regular_file() && !isLink) {
        LOGW_WARN(logger(), L"Ignore " << formatSyncPath(dirEntry.path())
                                       << L" because it is not a directory, a regular file or a symlink");
        isManaged = false;
        return true;
    }

    if (dirEntry.path().native().length() > CommonUtility::maxPathLength()) {
        LOGW_WARN(logger(),
                  L"Ignore " << formatSyncPath(dirEntry.path()) << L" because size > " << CommonUtility::maxPathLength());
        isManaged = false;
        return true;
    }

    return true;
}

bool Utility::getLinuxDesktopType(std::string &currentDesktop) {
    const std::string xdgCurrentDesktop = CommonUtility::envVarValue("XDG_CURRENT_DESKTOP");
    if (xdgCurrentDesktop.empty()) {
        return false;
    }

    // ':' is the separator in the env variable, like "ubuntu:GNOME"
    size_t colon_pos = xdgCurrentDesktop.find(':');

    if (colon_pos != std::string::npos) {
        currentDesktop = xdgCurrentDesktop.substr(colon_pos + 1);
        return true;
    }

    return false;
}

#ifdef _WIN32
bool Utility::longPath(const SyncPath &shortPathIn, SyncPath &longPathOut, bool &notFound) {
    int length = GetLongPathNameW(shortPathIn.native().c_str(), 0, 0);
    if (!length) {
        const bool exists = CommonUtility::isLikeFileNotFoundError(GetLastError());
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
        const bool exists = CommonUtility::isLikeFileNotFoundError(GetLastError());
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

bool Utility::runDetachedProcess(std::wstring cmd) {
    PROCESS_INFORMATION pinfo;
    STARTUPINFOW startupInfo = {sizeof(STARTUPINFO),
                                0,
                                0,
                                0,
                                (ulong) CW_USEDEFAULT,
                                (ulong) CW_USEDEFAULT,
                                (ulong) CW_USEDEFAULT,
                                (ulong) CW_USEDEFAULT,
                                0,
                                0,
                                0,
                                0,
                                0,
                                0,
                                0,
                                0,
                                0,
                                0};
    bool success = success = CreateProcess(0, cmd.data(), 0, 0, FALSE, CREATE_UNICODE_ENVIRONMENT | CREATE_NEW_CONSOLE, 0, 0,
                                           &startupInfo, &pinfo);

    if (success) {
        CloseHandle(pinfo.hThread);
        CloseHandle(pinfo.hProcess);
    }
    return success;
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
    (void) (lastTotalUser);
    (void) (lastTotalUserLow);
    (void) (lastTotalSys);
    (void) (lastTotalIdle);
    (void) (percent);
#endif
    return false;
}

bool Utility::cpuUsage(uint64_t &previousTotalTicks, uint64_t &previousIdleTicks, double &percent) {
#ifdef __APPLE__
    return cpuUsage_private(previousTotalTicks, previousIdleTicks, percent);
#else
    (void) (previousTotalTicks);
    (void) (previousIdleTicks);
    (void) (percent);
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

std::string Utility::userName() {
    return userName_private();
}

} // namespace KDC
