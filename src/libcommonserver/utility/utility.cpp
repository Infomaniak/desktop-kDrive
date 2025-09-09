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

#include "utility/utility_base.h"

#if defined(KD_MACOS)
#include <sys/statvfs.h>
#include <sys/mount.h>
#elif defined(KD_LINUX)
#include <sys/statvfs.h>
#include <sys/statfs.h>
#elif defined(KD_WINDOWS)
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

#if defined(KD_MACOS)
static const SyncName excludedAppFileName(Str("litesync-exclude.lst"));
#endif

// Resources relative path from working dir
#if defined(KD_MACOS)
static const SyncName resourcesPath(Str("../Resources"));
#elif defined(KD_LINUX)
static const SyncName resourcesPath(Str(""));
#elif defined(KD_WINDOWS)
static const SyncName resourcesPath(Str(""));
#endif

struct VariantPrinter {
        std::wstring operator()(std::monostate) { return std::wstring(L"NULL"); }
        std::wstring operator()(int value) { return std::to_wstring(value); }
        std::wstring operator()(int64_t value) { return std::to_wstring(value); }
        std::wstring operator()(double value) { return std::to_wstring(value); }
        std::wstring operator()(const std::string &value) { return CommonUtility::s2ws(value); }
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

#if defined(KD_MACOS)
    struct statvfs stat;
    if (statvfs(tmpDirPath.c_str(), &stat) == 0) {
        return (int64_t) (stat.f_bavail * stat.f_frsize);
    }
#elif defined(KD_LINUX)
    struct statvfs64 stat;
    if (statvfs64(tmpDirPath.c_str(), &stat) == 0) {
        return (int64_t) (stat.f_bavail * stat.f_frsize);
    }
#elif defined(KD_WINDOWS)
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

bool Utility::isCreationDateValid(int64_t creationDate) {
    if (creationDate == 0 || creationDate == 443779200) {
        // Do not upload or save on DB invalid dates
        // 443779200 correspond to "Tuesday 24 January 1984 08:00:00" which is a default date set by macOS
        return false;
    }
    return true;
}

void Utility::msleep(int msec) {
    std::chrono::milliseconds dura(msec);
    std::this_thread::sleep_for(dura);
}

std::wstring Utility::v2ws(const dbtype &v) {
    return std::visit(VariantPrinter{}, v);
}

std::wstring Utility::quotedSyncName(const SyncName &name) {
    return L"'" + SyncName2WStr(name) + L"'";
}

std::wstring Utility::formatSyncName(const SyncName &name) {
    std::wstringstream ss;
    ss << L"name=" << quotedSyncName(name);

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
#if defined(KD_WINDOWS)
    std::stringstream ss;
    ss << ec.message() << " (code: " << ec.value() << ")";
    return CommonUtility::s2ws(ss.str());
#elif defined(KD_LINUX)
    std::stringstream ss;
    ss << ec.message() << ". (code: " << ec.value() << ")";
    return CommonUtility::s2ws(ss.str());
#elif defined(KD_MACOS)
    return CommonUtility::s2ws(ec.message());
#endif
}

std::wstring Utility::formatStdError(const SyncPath &path, const std::error_code &ec) {
    std::wstringstream ss;
    ss << L"path='" << Path2WStr(path) << L"', err='" << formatStdError(ec) << L"'";

    return ss.str();
}

std::wstring Utility::formatIoError(const IoError ioError) {
    std::wstringstream ss;
    ss << CommonUtility::s2ws(IoHelper::ioError2StdString(ioError));

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
    ss << uri.toString() << " : " << code << " - " << description;

    return ss.str();
}

std::string Utility::formatGenericServerError(const std::string &replyBody, const Poco::Net::HTTPResponse &httpResponse) {
    std::stringstream errorStream;
    errorStream << "Error in reply";

    // Try to parse as string
    if (!replyBody.empty()) {
        errorStream << ", error: " << replyBody;
    }

    errorStream << ", content type: " << httpResponse.getContentType().c_str();
    errorStream << ", reason: " << httpResponse.getReason().c_str();

    const std::string encoding = httpResponse.get("Content-Encoding", "");
    if (!encoding.empty()) {
        errorStream << ", encoding: " << encoding.c_str();
    }

    return errorStream.str(); // str() return a copy of the underlying string
}

void Utility::logGenericServerError(const log4cplus::Logger &logger, const std::string &errorTitle, const std::string &replyBody,
                                    const Poco::Net::HTTPResponse &httpResponse) {
    std::string errorMsg = formatGenericServerError(replyBody, httpResponse);
    sentry::Handler::captureMessage(sentry::Level::Warning, errorTitle, errorMsg);
    LOG_WARN(logger, errorTitle << ": " << errorMsg);
}

bool Utility::checkIfEqualUpToCaseAndEncoding(const SyncPath &a, const SyncPath &b, bool &isEqual) {
    isEqual = false;

    SyncPath normalizedA;
    if (!Utility::normalizedSyncPath(a, normalizedA)) {
        LOGW_WARN(_logger, L"Error in Utility::normalizedSyncPath: " << Utility::formatSyncPath(a));
        return false;
    }

    SyncPath normalizedB;
    if (!Utility::normalizedSyncPath(b, normalizedB)) {
        LOGW_WARN(_logger, L"Error in Utility::normalizedSyncPath: " << Utility::formatSyncPath(b));
        return false;
    }

    const SyncName normalizedAStr{normalizedA.native()};
    const SyncName normalizedBStr{normalizedB.native()};

    isEqual = normalizedAStr.size() == normalizedBStr.size() &&
              std::equal(normalizedAStr.begin(), normalizedAStr.end(), normalizedBStr.begin(), normalizedBStr.end(),
                         [](const SyncChar c1, const SyncChar c2) {
                             return std::toupper(c1, std::locale()) == std::toupper(c2, std::locale());
                         });

    return true;
}

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
        str[i] |= static_cast<char>((hexstr[j] & '@' ? hexstr[j] + 9 : hexstr[j]) & 0xF);
    }
}

std::vector<std::string> Utility::splitStr(const std::string &str, const char sep) {
    std::vector<std::string> strings;
    std::istringstream ss(str);
    std::string s;
    while (getline(ss, s, sep)) strings.push_back(s);

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

std::string Utility::nodeSet2str(const NodeSet &set) {
    bool first = true;
    std::string out;
    for (const auto &nodeId: set) {
        if (!first) {
            out += ",";
        }
        if (!nodeId.empty()) {
            out += nodeId;
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

#if defined(KD_MACOS)
SyncPath Utility::getExcludedAppFilePath(const bool test /*= false*/) {
    if (test) return excludedAppFileName;

    auto canonicalPath =
            std::filesystem::weakly_canonical(CommonUtility::getAppWorkingDir() / SyncPath{resourcesPath} / excludedAppFileName);

    return canonicalPath.make_preferred();
}
#endif

SyncPath Utility::getExcludedTemplateFilePath(const bool test /*= false*/) {
    if (test) return excludedTemplateFileName;
    auto canonicalPath = std::filesystem::weakly_canonical(CommonUtility::getAppWorkingDir() / SyncPath{resourcesPath} /
                                                           excludedTemplateFileName);
    return canonicalPath.make_preferred();
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

std::string Utility::_errId(const char *file, int line) {
    std::string err = CommonUtility::toUpper(std::filesystem::path(file).filename().stem().string().substr(0, 3)) + ":" +
                      std::to_string(line);
    return err;
}

// Be careful, some characters have 2 different encodings in Unicode
// For example 'é' can be coded as 0x65 + 0xcc + 0x81  or 0xc3 + 0xa9
bool Utility::normalizedSyncName(const SyncName &name, SyncName &normalizedName,
                                 const UnicodeNormalization normalization) noexcept {
    bool success = CommonUtility::normalizedSyncName(name, normalizedName, normalization);
    std::wstring errorMessage = L"Failed to normalize " + formatSyncName(name);
    if (!success) {
#if defined(KD_WINDOWS)
        const DWORD dwError = GetLastError();
        errorMessage += L" (" + utility_base::getErrorMessage(dwError) + L")";
#endif
        LOGW_DEBUG(logger(), L"Failed to normalize " << errorMessage);
    }

    return success;
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

bool Utility::checkIfDirEntryIsManaged(const DirectoryEntry &dirEntry, bool &isManaged, IoError &ioError,
                                       const ItemType &itemType) {
    isManaged = true;
    ioError = IoError::Success;
    if (dirEntry.path().native().length() > CommonUtility::maxPathLength()) {
        LOGW_WARN(logger(),
                  L"Ignore " << formatSyncPath(dirEntry.path()) << L" because size > " << CommonUtility::maxPathLength());
        isManaged = false;
        return true;
    }

    if (!dirEntry.is_regular_file() && !dirEntry.is_directory()) {
        auto tmpItemType = itemType;
        if (tmpItemType == ItemType()) {
            bool result = IoHelper::getItemType(dirEntry.path(), tmpItemType);
            ioError = tmpItemType.ioError;
            if (!result) {
                LOGW_WARN(logger(), L"Error in IoHelper::getItemType: " << formatIoError(dirEntry.path(), ioError));
                return false;
            }

            if (ioError == IoError::NoSuchFileOrDirectory || ioError == IoError::AccessDenied) {
                LOGW_DEBUG(logger(), L"Error in IoHelper::getItemType: " << formatIoError(dirEntry.path(), ioError));
                return true;
            }
        }
        if (tmpItemType.linkType == LinkType::None) {
            LOGW_WARN(logger(), L"Ignore " << formatSyncPath(dirEntry.path())
                                           << L" because it is not a directory, a regular file or a symlink");
            isManaged = false;
            return true;
        }
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

#if defined(KD_WINDOWS)
bool Utility::longPath(const SyncPath &shortPathIn, SyncPath &longPathOut, bool &notFound) {
    int length = GetLongPathNameW(shortPathIn.native().c_str(), 0, 0);
    if (!length) {
        const bool exists = !utility_base::isLikeFileNotFoundError(GetLastError());
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
        const bool exists = !utility_base::isLikeFileNotFoundError(GetLastError());
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

SyncPath Utility::commonDocumentsFolderName() {
    return Str2SyncName(COMMON_DOC_FOLDER);
}

SyncPath Utility::sharedFolderName() {
    return Str2SyncName(SHARED_FOLDER);
}

bool Utility::isError500(const Poco::Net::HTTPResponse::HTTPStatus httpErrorCode) {
    switch (httpErrorCode) {
        case Poco::Net::HTTPResponse::HTTP_INTERNAL_SERVER_ERROR:
        case Poco::Net::HTTPResponse::HTTP_NOT_IMPLEMENTED:
        case Poco::Net::HTTPResponse::HTTP_BAD_GATEWAY:
        case Poco::Net::HTTPResponse::HTTP_SERVICE_UNAVAILABLE:
        case Poco::Net::HTTPResponse::HTTP_GATEWAY_TIMEOUT:
        case Poco::Net::HTTPResponse::HTTP_VERSION_NOT_SUPPORTED:
        case Poco::Net::HTTPResponse::HTTP_VARIANT_ALSO_NEGOTIATES:
        case Poco::Net::HTTPResponse::HTTP_INSUFFICIENT_STORAGE:
        case Poco::Net::HTTPResponse::HTTP_LOOP_DETECTED:
        case Poco::Net::HTTPResponse::HTTP_NOT_EXTENDED:
        case Poco::Net::HTTPResponse::HTTP_NETWORK_AUTHENTICATION_REQUIRED:
            return true;
        default:
            return false;
    }
}

} // namespace KDC
