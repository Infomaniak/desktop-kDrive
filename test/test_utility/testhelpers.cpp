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

#include "testhelpers.h"
#include "localtemporarydirectory.h"
#include "io/iohelper.h"

#include "libcommon/utility/utility.h"
#include "libcommonserver/io/iohelper.h"

#include <fstream>

#if defined(KD_LINUX)
#include <regex>
#endif

#include <Poco/JSON/Object.h>

#if defined(KD_WINDOWS)
#include <shlobj_core.h> // SHCreateItemFromIDList
#include <atlbase.h> // CComPtr

#include <sys/utime.h>
#include <sys/types.h>
#else
#include <sys/stat.h>
#include <utime.h>
#endif

namespace KDC::testhelpers {

SyncName makeNfdSyncName() {
    SyncName nfdNormalized;
    if (!Utility::normalizedSyncName(Str("ééé"), nfdNormalized, UnicodeNormalization::NFD)) {
        assert(false);
    }
    return nfdNormalized;
}

SyncName makeNfcSyncName() {
    SyncName nfcNormalized;
    if (!Utility::normalizedSyncName(Str("ééé"), nfcNormalized)) {
        assert(false);
    }
    return nfcNormalized;
}

void generateTestFile(const SyncPath &path, const uint64_t size /*= 0*/) {
    std::ofstream testFile(path, std::ios_base::in | std::ios_base::trunc);
    if (size) {
        setTestFileSize(path, size);
    }
    testFile.close();
}

void generateOrEditTestFile(const SyncPath &path) {
    std::ofstream testFile(path, std::ios::app);
    testFile << "test" << std::endl;
    testFile.close();
}

void setTestFileSize(const SyncPath &path, uint64_t size) {
    const std::string str{"0123456789"};
    std::ofstream ofs(path, std::ios_base::in | std::ios_base::trunc);
    for (uint64_t i = 0; i < static_cast<uint64_t>(round(static_cast<double>(size) / static_cast<double>(str.length()))); i++) {
        ofs << str;
    }
}

void generateBigFiles(const SyncPath &dirPath, const uint16_t size, const uint16_t count) {
    // Generate the 1st big file
    const auto bigFilePath = generateBigFile(dirPath, size);

    // Generate others big files
    for (uint16_t i = 1; i < count; i++) {
        std::stringstream fileName;
        fileName << "big_file_" << size << "_" << i << ".txt";
        const SyncPath newBigFilePath = SyncPath(dirPath) / fileName.str();
        (void) std::filesystem::copy_file(bigFilePath, newBigFilePath, std::filesystem::copy_options::overwrite_existing);
    }
}

SyncPath generateBigFile(const SyncPath &dirPath, const uint16_t size) {
    std::stringstream fileName;
    fileName << "big_file_" << size << "_" << 0 << ".txt";
    const std::string str{"0123456789"};
    const auto bigFilePath = SyncPath(dirPath) / fileName.str();

    std::ofstream ofs(bigFilePath, std::ios_base::in | std::ios_base::trunc);
    for (uint64_t i = 0;
         i < static_cast<uint64_t>(round(static_cast<double>(size * 1000000) / static_cast<double>(str.length()))); i++) {
        ofs << str;
    }
    return bigFilePath;
}

std::string loadEnvVariable(const std::string &key, const bool mandatory) {
    const std::string val = KDC::CommonUtility::envVarValue(key);
    if (val.empty() && mandatory) {
        std::cout << "Environment variables " << key << " is missing!" << std::endl;
        throw std::runtime_error("Environment variables " + key + " is missing!");
    }
    return val;
}

#if defined(KD_MACOS) || defined(KD_WINDOWS)
void createFileWithDehydratedStatus(const SyncPath &filePath) {
    generateOrEditTestFile(filePath);

    auto ioError = IoError::Unknown;
    (void) IoHelper::setDehydratedPlaceholderStatus(filePath, ioError);
    assert(IoHelper::setDehydratedPlaceholderStatus(filePath, ioError) && ioError == IoError::Success &&
           "Unexpected failure of IoHelper::setXAttrValue.");
}
#endif

#if defined(KD_MACOS)
void eraseFromTrash(const KDC::SyncPath &relativePath) {
    std::error_code ec;
    (void) std::filesystem::remove_all(Utility::getTrashPath() / relativePath, ec);
}
bool isInTrash(const SyncPath &path) {
    if (std::error_code ec; !std::filesystem::exists(Utility::getTrashPath() / path, ec) || ec) {
        if (ec) {
            LOGW_WARN(Log::instance()->getLogger(), L"Error in std::filesystem::exists - " << Utility::formatStdError(ec));
        }
        return false;
    }

    return true;
}
#endif

#if defined(KD_LINUX)
SyncPath removeNumericSuffix(const SyncPath &relativePath) {
    if (relativePath.empty()) return {};

    static const std::regex numericSuffixRegex(".*(\\.[0-9]+)$");

    std::list<SyncName> segments = CommonUtility::splitSyncPath(relativePath);
    auto &root = segments.front();

    std::smatch words;
    const std::string rootStr = SyncName2Str(root);
    (void) std::regex_match(rootStr, words, numericSuffixRegex);

    if (words.size() <= 1) return relativePath; // No numeric suffix.

    root = root.substr(0, rootStr.size() - std::string{words[1]}.size()); // Removes suffix from root directory name.

    // Adapt the relative path.
    std::stringstream ss;
    std::uint64_t i = 0;
    for (const auto &segment: segments) {
        if (i > 0) ss << "/";
        ss << SyncName2Str(segment);
        ++i;
    }

    return SyncPath{ss.str()};
}

void eraseFromTrash(const KDC::SyncPath &relativePath) {
    const auto trashPath = Utility::getTrashPath();
    std::error_code ec;

    auto dirIt = std::filesystem::recursive_directory_iterator(trashPath,
                                                               std::filesystem::directory_options::skip_permission_denied, ec);
    if (ec) {
        LOGW_WARN(Log::instance()->getLogger(), L"Error in testhelpers::eraseFromTrash: " << Utility::formatStdError(ec));
        return;
    }

    std::vector<SyncPath> itemsToErase;
    for (; dirIt != std::filesystem::recursive_directory_iterator(); ++dirIt) {
        const auto dirItemRelativePath = std::filesystem::relative(dirIt->path(), trashPath, ec);
        // Filter out the numerical suffix of the root directory name, e.g: `dirname.15` is replaced with `dirname`.
        const auto suffixFreedirectorEntryPath = removeNumericSuffix(dirItemRelativePath);
        if (relativePath == suffixFreedirectorEntryPath) itemsToErase.push_back(dirIt->path());
    }

    for (const auto &pathToErase: itemsToErase) (void) std::filesystem::remove_all(pathToErase, ec);
}

bool isInTrash(const SyncPath &relativePath) {
    const auto trashPath = Utility::getTrashPath();
    std::error_code ec;

    auto dirIt = std::filesystem::recursive_directory_iterator(trashPath,
                                                               std::filesystem::directory_options::skip_permission_denied, ec);
    if (ec) {
        LOGW_WARN(Log::instance()->getLogger(), L"Error in testhelpers::isInTrash: " << Utility::formatStdError(ec));
        return false;
    }

    for (; dirIt != std::filesystem::recursive_directory_iterator(); ++dirIt) {
        const auto dirItemRelativePath = std::filesystem::relative(dirIt->path(), trashPath, ec);
        // Filter out the numerical suffix of the root directory name, e.g.: `dirname.15` is replaced with `dirname`.
        const auto suffixFreedirectorEntryPath = removeNumericSuffix(dirItemRelativePath);
        if (relativePath == suffixFreedirectorEntryPath) return true;
    }

    return false;
}
#endif

#if defined(KD_WINDOWS)
namespace {
HRESULT bindToCsidl(const int csidl, REFIID riid, void **ppv) {
    auto hr = S_OK;
    PIDLIST_ABSOLUTE pidl = nullptr;
    hr = SHGetSpecialFolderLocation(nullptr, csidl, &pidl);
    if (SUCCEEDED(hr)) {
        IShellFolder *psfDesktop = nullptr;
        hr = SHGetDesktopFolder(&psfDesktop);
        if (SUCCEEDED(hr)) {
            if (pidl->mkid.cb) {
                hr = psfDesktop->BindToObject(pidl, nullptr, riid, ppv);
            } else {
                hr = psfDesktop->QueryInterface(riid, ppv);
            }
            (void) psfDesktop->Release();
        }
        CoTaskMemFree(pidl);
    }
    return hr;
}

bool isFolder(IShellItem *item) {
    if (!item) return false;

    SFGAOF attributes = SFGAO_FOLDER;
    if (const HRESULT hr = item->GetAttributes(SFGAO_FOLDER, &attributes); SUCCEEDED(hr)) {
        return (attributes & SFGAO_FOLDER) != 0;
    }

    return false;
}

bool isInFolder(const std::list<SyncName> &relativePath, IShellFolder2 *folder) {
    IEnumIDList *peidl = nullptr;

    if (const auto enumHr = folder->EnumObjects(nullptr, SHCONTF_FOLDERS | SHCONTF_NONFOLDERS, &peidl); SUCCEEDED(enumHr)) {
        PITEMID_CHILD pidlItem = nullptr;
        while (peidl->Next(1, &pidlItem, nullptr) == S_OK) {
            CComPtr<IShellItem> pItem;
            if (const auto createItemHr = SHCreateItemFromIDList(pidlItem, IID_PPV_ARGS(&pItem)); FAILED(createItemHr)) {
                CoTaskMemFree(pidlItem);
                continue;
            }

            STRRET strRet;
            ZeroMemory(&strRet, sizeof(strRet));
            strRet.uType = STRRET_WSTR;

            if (const auto displayNameHr =
                        folder->GetDisplayNameOf(pidlItem, SHGDN_FORADDRESSBAR | SHGDN_INFOLDER | SHGDN_FOREDITING, &strRet);
                FAILED(displayNameHr)) {
                CoTaskMemFree(pidlItem);
                continue;
            }

            LPTSTR lptstr = nullptr;
            if (const auto stringReturnToStrHr = StrRetToStr(&strRet, pidlItem, &lptstr); FAILED(stringReturnToStrHr)) {
                CoTaskMemFree(pidlItem);
                CoTaskMemFree(lptstr);
                continue;
            }

            const bool match = SyncPath(lptstr) == SyncPath(relativePath.front()).stem();
            if (relativePath.size() == 1 && match) {
                return true;
                CoTaskMemFree(pidlItem);
                CoTaskMemFree(lptstr);
                break;
            }

            if (match && isFolder(pItem)) {
                auto childRelativedPath = relativePath;
                childRelativedPath.pop_front();
                IShellFolder2 *psChildFolder = nullptr;
                (void) SHBindToObject(folder, pidlItem, nullptr, IID_IShellFolder2, reinterpret_cast<void **>(&psChildFolder));

                return isInFolder(childRelativedPath, psChildFolder);
            }
        }
    }

    if (peidl) (void) peidl->Release();

    return false;
}

bool isInFolder(const SyncPath &relativePath, IShellFolder2 *folder) {
    return isInFolder(CommonUtility::splitSyncPath(relativePath), folder);
}

} // namespace

bool isInTrash(const SyncPath &relativePath) {
    bool found = false;

    if (const auto coInitializeHr = CoInitialize(nullptr); SUCCEEDED(coInitializeHr)) {
        IShellFolder2 *psfRecycleBin = nullptr;
        if (const auto bindingHr = bindToCsidl(CSIDL_BITBUCKET, IID_PPV_ARGS(&psfRecycleBin)); SUCCEEDED(bindingHr)) {
            found = isInFolder(relativePath, psfRecycleBin);
            (void) psfRecycleBin->Release();
        }
        CoUninitialize();
    }

    return found;
}
#endif
void createSymLinkLoop(const SyncPath &filepath1, const SyncPath &filepath2, const NodeType nodeType) {
    const LocalTemporaryDirectory tempDir;
    const auto currentPath = std::filesystem::current_path();
    std::filesystem::current_path(tempDir.path());

    switch (nodeType) {
        case NodeType::File: {
            const std::ofstream ofs("filepath1");
            break;
        }
        case NodeType::Directory: {
            (void) std::filesystem::create_directories("filepath1");
            break;
        }
        default:
            throw std::invalid_argument(
                    "Invalid argument NodeType argument in createSymLinkLoop. Expected: either NodeType::File or "
                    "NodeType::Directory.");
    }

    std::filesystem::create_symlink(filepath1.filename(), filepath2.filename()); // filepath2 -> filepath1

    std::filesystem::current_path(currentPath);
    std::filesystem::rename(tempDir.path() / filepath2.filename(), filepath2);

    std::filesystem::current_path(tempDir.path());
    (void) std::filesystem::remove_all(filepath1.filename());
    (void) std::filesystem::remove_all(filepath2.filename());
    std::error_code ec;
    std::filesystem::create_symlink(filepath2.filename(), filepath1.filename(), ec); // filepath1 -> filepath2

    std::filesystem::current_path(currentPath);
    std::filesystem::rename(tempDir.path() / filepath1.filename(), filepath1);
}

void setupLogging() {
    IoError ioError = IoError::Success;
    SyncPath logDirPath;
    if (!IoHelper::logDirectoryPath(logDirPath, ioError)) {}

    // Setup log4cplus
    const std::filesystem::path logFilePath = logDirPath / Utility::logFileNameWithTime();
    if (!Log::instance(Path2WStr(logFilePath))) {
        assert(false);
    }
}

} // namespace KDC::testhelpers
