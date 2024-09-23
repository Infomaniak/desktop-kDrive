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

#include "placeholders.h"
#include "..\Common\utilities.h"

#include <filesystem>
#include <winrt\windows.storage.provider.h>

namespace winrt {
using namespace Windows::Storage;
}

bool Placeholders::create(const PCWSTR fileId, const PCWSTR relativePath, const PCWSTR destPath,
                          const WIN32_FIND_DATA *findData) {
    std::filesystem::path fullPath = std::filesystem::path(destPath) / std::filesystem::path(relativePath);

    std::wstring fileName(fullPath.filename().c_str());
    std::wstring dirPath(fullPath.parent_path().c_str());

    CF_PLACEHOLDER_CREATE_INFO cloudEntry;
    cloudEntry.FileIdentity = fileId;
    cloudEntry.FileIdentityLength = (USHORT) (wcslen(fileId) + 1) * sizeof(WCHAR);
    cloudEntry.RelativeFileName = fileName.c_str();
    cloudEntry.Flags = CF_PLACEHOLDER_CREATE_FLAG_MARK_IN_SYNC;
    cloudEntry.FsMetadata.BasicInfo.FileAttributes = findData->dwFileAttributes;
    cloudEntry.FsMetadata.BasicInfo.CreationTime = Utilities::fileTimeToLargeInteger(findData->ftCreationTime);
    cloudEntry.FsMetadata.BasicInfo.LastWriteTime = Utilities::fileTimeToLargeInteger(findData->ftLastWriteTime);
    cloudEntry.FsMetadata.BasicInfo.LastAccessTime = Utilities::fileTimeToLargeInteger(findData->ftLastAccessTime);
    cloudEntry.FsMetadata.BasicInfo.ChangeTime = Utilities::fileTimeToLargeInteger(findData->ftLastWriteTime);

    if ((findData->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) {
        cloudEntry.FsMetadata.FileSize.QuadPart = 0;
    } else {
        cloudEntry.FsMetadata.FileSize.QuadPart = ((ULONGLONG) findData->nFileSizeHigh << 32) + findData->nFileSizeLow;
    }

    try {
        TRACE_DEBUG(L"Creating placeholder : path = %ls", fullPath.native().c_str());
        winrt::check_hresult(CfCreatePlaceholders(dirPath.c_str(), &cloudEntry, 1, CF_CREATE_FLAG_NONE, nullptr));
    } catch (winrt::hresult_error const &ex) {
        TRACE_ERROR(L"WinRT error caught : %08x - %s", static_cast<HRESULT>(winrt::to_hresult()), ex.message().c_str());
        return false;
    }

    return true;
}

bool Placeholders::convert(const PCWSTR fileId, const PCWSTR filePath) {
    DWORD dwFlagsAndAttributes = 0;
    bool exists = true;
    if (!Utilities::getCreateFileFlagsAndAttributes(filePath, dwFlagsAndAttributes, exists)) {
        TRACE_ERROR(L"Error in Utilities::getCreateFileFlagsAndAttributes: %ls", filePath);
        return false;
    }

    if (!exists) {
        TRACE_WARNING(L"File/directory doesn't exist anymore : %ls", filePath);
        return true;
    }

    if (dwFlagsAndAttributes & FILE_FLAG_OPEN_REPARSE_POINT) {
        // Links are not managed by MS Cloud File API
        return true;
    }

    winrt::handle fileHandle(CreateFile(filePath, WRITE_DAC, 0, nullptr, OPEN_EXISTING, dwFlagsAndAttributes, nullptr));
    if (fileHandle.get() == INVALID_HANDLE_VALUE) {
        TRACE_ERROR(L"Error in CreateFile : %ls", Utilities::getLastErrorMessage().c_str());
        return false;
    }

    try {
        TRACE_DEBUG(L"Converting to placeholder : path = %ls", filePath);
        winrt::check_hresult(CfConvertToPlaceholder(fileHandle.get(), fileId, (USHORT) (wcslen(fileId) + 1) * sizeof(WCHAR),
                                                    CF_CONVERT_FLAG_MARK_IN_SYNC, nullptr, nullptr));
    } catch (winrt::hresult_error const &ex) {
        TRACE_ERROR(L"WinRT error caught : %08x - %s", static_cast<HRESULT>(winrt::to_hresult()), ex.message().c_str());
        return false;
    }

    return true;
}

bool Placeholders::revert(const PCWSTR filePath) {
    DWORD dwFlagsAndAttributes = 0;
    bool exists = true;
    if (!Utilities::getCreateFileFlagsAndAttributes(filePath, dwFlagsAndAttributes, exists)) {
        TRACE_ERROR(L"Error in Utilities::getCreateFileFlagsAndAttributes: %ls", filePath);
        return false;
    }

    if (!exists) {
        TRACE_WARNING(L"File/directory doesn't exist anymore : %ls", filePath);
        return true;
    }

    if (dwFlagsAndAttributes & FILE_FLAG_OPEN_REPARSE_POINT) {
        // Links are not managed by MS Cloud File API
        return true;
    }

    winrt::handle fileHandle(CreateFile(filePath, WRITE_DAC, 0, nullptr, OPEN_EXISTING, dwFlagsAndAttributes, nullptr));
    if (fileHandle.get() == INVALID_HANDLE_VALUE) {
        TRACE_ERROR(L"Error in CreateFile : %ls", Utilities::getLastErrorMessage().c_str());
        return false;
    }

    try {
        TRACE_DEBUG(L"Reverting placeholder : path = %ls", filePath);
        winrt::check_hresult(CfRevertPlaceholder(fileHandle.get(), CF_REVERT_FLAG_NONE, nullptr));
    } catch (winrt::hresult_error const &ex) {
        TRACE_ERROR(L"WinRT error caught : %08x - %s", static_cast<HRESULT>(winrt::to_hresult()), ex.message().c_str());
        return false;
    }

    return true;
}

bool Placeholders::update(const PCWSTR filePath, const WIN32_FIND_DATA *findData) {
    DWORD dwFlagsAndAttributes = 0;
    bool exists = true;
    if (!Utilities::getCreateFileFlagsAndAttributes(filePath, dwFlagsAndAttributes, exists)) {
        TRACE_ERROR(L"Error in Utilities::getCreateFileFlagsAndAttributes: %ls", filePath);
        return false;
    }

    if (!exists) {
        TRACE_WARNING(L"File/directory doesn't exist anymore : %ls", filePath);
        return true;
    }

    if (dwFlagsAndAttributes & FILE_FLAG_OPEN_REPARSE_POINT) {
        // Links are not managed by MS Cloud File API
        return true;
    }

    winrt::handle fileHandle(CreateFile(filePath, WRITE_DAC, 0, nullptr, OPEN_EXISTING, dwFlagsAndAttributes, nullptr));
    if (fileHandle.get() == INVALID_HANDLE_VALUE) {
        TRACE_ERROR(L"Error in CreateFile : %ls", Utilities::getLastErrorMessage().c_str());
        return false;
    }

    bool res = true;
    CF_PLACEHOLDER_STANDARD_INFO info;
    try {
        TRACE_DEBUG(L"Get placeholder info : path = %ls", filePath);
        DWORD retLength = 0;
        winrt::check_hresult(
                CfGetPlaceholderInfo(fileHandle.get(), CF_PLACEHOLDER_INFO_STANDARD, &info, sizeof(info), &retLength));
    } catch (winrt::hresult_error const &ex) {
        if (ex.code() != HRESULT_FROM_WIN32(ERROR_MORE_DATA)) {
            TRACE_ERROR(L"WinRT error caught : %08x - %s", static_cast<HRESULT>(winrt::to_hresult()), ex.message().c_str());
            res = false;
        }
    }

    try {
        CF_FS_METADATA fsMetadata;
        fsMetadata.BasicInfo.FileAttributes = findData->dwFileAttributes;
        fsMetadata.BasicInfo.CreationTime = Utilities::fileTimeToLargeInteger(findData->ftCreationTime);
        fsMetadata.BasicInfo.LastWriteTime = Utilities::fileTimeToLargeInteger(findData->ftLastWriteTime);
        fsMetadata.BasicInfo.LastAccessTime = Utilities::fileTimeToLargeInteger(findData->ftLastAccessTime);
        fsMetadata.BasicInfo.ChangeTime = Utilities::fileTimeToLargeInteger(findData->ftLastWriteTime);
        fsMetadata.FileSize.QuadPart = ((ULONGLONG) findData->nFileSizeHigh << 32) + findData->nFileSizeLow;

        TRACE_DEBUG(L"Update placeholder : path = %ls", filePath);
        winrt::check_hresult(CfUpdatePlaceholder(fileHandle.get(), &fsMetadata, info.FileIdentity, info.FileIdentityLength,
                                                 nullptr, 0, CF_UPDATE_FLAG_VERIFY_IN_SYNC, nullptr, nullptr));
    } catch (winrt::hresult_error const &ex) {
        TRACE_ERROR(L"WinRT error caught : %08x - %s", static_cast<HRESULT>(winrt::to_hresult()), ex.message().c_str());
        res = false;
    }

    return res;
}

bool Placeholders::getStatus(const PCWSTR filePath, bool *isPlaceholder, bool *isDehydrated, bool *isSynced) {
    DWORD dwFlagsAndAttributes = 0;
    bool exists = true;
    if (!Utilities::getCreateFileFlagsAndAttributes(filePath, dwFlagsAndAttributes, exists)) {
        TRACE_ERROR(L"Error in Utilities::getCreateFileFlagsAndAttributes: %ls", filePath);
        return false;
    }

    if (!exists) {
        TRACE_WARNING(L"File/directory doesn't exist anymore : %ls", filePath);
        return true;
    }

    if (dwFlagsAndAttributes & FILE_FLAG_OPEN_REPARSE_POINT) {
        // Links are not managed by MS Cloud File API
        if (isPlaceholder) {
            *isPlaceholder = false;
        }
        if (isDehydrated) {
            *isDehydrated = false;
        }
        if (isSynced) {
            *isSynced = false;
        }
        return true;
    }

    winrt::handle fileHandle(CreateFile(filePath, WRITE_DAC, 0, nullptr, OPEN_EXISTING, dwFlagsAndAttributes, nullptr));
    if (fileHandle.get() == INVALID_HANDLE_VALUE) {
        TRACE_ERROR(L"Error in CreateFile : %ls", Utilities::getLastErrorMessage().c_str());
        return false;
    }

    FILE_ATTRIBUTE_TAG_INFO fileInformation;
    if (!GetFileInformationByHandleEx(fileHandle.get(), FileAttributeTagInfo, &fileInformation, sizeof(fileInformation))) {
        TRACE_ERROR(L"Error in GetFileInformationByHandleEx : %ls", Utilities::getLastErrorMessage().c_str());
        return false;
    }

    CF_PLACEHOLDER_STATE state = CfGetPlaceholderStateFromFileInfo(&fileInformation, FileAttributeTagInfo);
    if (state == CF_PLACEHOLDER_STATE_INVALID) {
        TRACE_ERROR(L"Error in CfGetPlaceholderStateFromFileInfo : %ls", Utilities::getLastErrorMessage().c_str());
        return false;
    }

    if (isPlaceholder) {
        *isPlaceholder = (state & CF_PLACEHOLDER_STATE_PLACEHOLDER);
    }
    if (isDehydrated) {
        *isDehydrated = ((state & CF_PLACEHOLDER_STATE_PLACEHOLDER) && (state & CF_PLACEHOLDER_STATE_PARTIALLY_ON_DISK));
    }
    if (isSynced) {
        *isSynced = (state & CF_PLACEHOLDER_STATE_IN_SYNC);
    }

    return true;
}

bool Placeholders::setStatus(const PCWSTR path, bool syncOngoing) {
    DWORD dwFlagsAndAttributes = 0;
    bool exists = true;
    if (!Utilities::getCreateFileFlagsAndAttributes(path, dwFlagsAndAttributes, exists)) {
        TRACE_ERROR(L"Error in Utilities::getCreateFileFlagsAndAttributes: %ls", path);
        return false;
    }

    if (!exists) {
        TRACE_WARNING(L"File/directory doesn't exist anymore : %ls", path);
        return true;
    }

    if (dwFlagsAndAttributes & FILE_FLAG_OPEN_REPARSE_POINT) {
        // Links are not managed by MS Cloud File API
        return true;
    }

    winrt::handle fileHandle(CreateFile(path, WRITE_DAC, 0, nullptr, OPEN_EXISTING, dwFlagsAndAttributes, nullptr));
    if (fileHandle.get() == INVALID_HANDLE_VALUE) {
        TRACE_ERROR(L"Error in CreateFile : %ls", Utilities::getLastErrorMessage().c_str());
        return false;
    }

    try {
        TRACE_DEBUG(L"Set placeholder status : syncOngoing = %d, path = %ls", syncOngoing, path);
        winrt::check_hresult(CfSetInSyncState(fileHandle.get(),
                                              syncOngoing ? CF_IN_SYNC_STATE_NOT_IN_SYNC : CF_IN_SYNC_STATE_IN_SYNC,
                                              CF_SET_IN_SYNC_FLAG_NONE, nullptr));
    } catch (winrt::hresult_error const &ex) {
        TRACE_ERROR(L"WinRT error caught : %08x - %s", static_cast<HRESULT>(winrt::to_hresult()), ex.message().c_str());
        return false;
    }

    return true;
}

bool Placeholders::getInfo(const PCWSTR path, CF_PLACEHOLDER_STANDARD_INFO &info) {
    DWORD dwFlagsAndAttributes = 0;
    bool exists = true;
    if (!Utilities::getCreateFileFlagsAndAttributes(path, dwFlagsAndAttributes, exists)) {
        TRACE_ERROR(L"Error in Utilities::getCreateFileFlagsAndAttributes: %ls", path);
        return false;
    }

    if (!exists) {
        TRACE_WARNING(L"File/directory doesn't exist anymore : %ls", path);
        return true;
    }

    if (dwFlagsAndAttributes & FILE_FLAG_OPEN_REPARSE_POINT) {
        // Links are not managed by MS Cloud File API
        info.PinState = CF_PIN_STATE_UNSPECIFIED;
        return true;
    }

    winrt::handle fileHandle(CreateFile(path, WRITE_DAC, 0, nullptr, OPEN_EXISTING, dwFlagsAndAttributes, nullptr));
    if (fileHandle.get() == INVALID_HANDLE_VALUE) {
        TRACE_ERROR(L"Error in CreateFile : %ls", Utilities::getLastErrorMessage().c_str());
        return false;
    }

    try {
        TRACE_DEBUG(L"Get placeholder info : path = %ls", path);
        DWORD retLength = 0;
        winrt::check_hresult(
                CfGetPlaceholderInfo(fileHandle.get(), CF_PLACEHOLDER_INFO_STANDARD, &info, sizeof(info), &retLength));
    } catch (winrt::hresult_error const &ex) {
        if (ex.code() != HRESULT_FROM_WIN32(ERROR_MORE_DATA)) {
            TRACE_ERROR(L"WinRT error caught : %08x - %s", static_cast<HRESULT>(winrt::to_hresult()), ex.message().c_str());
            return false;
        }
    }

    return true;
}

bool Placeholders::setPinState(const PCWSTR path, CF_PIN_STATE state) {
    DWORD dwFlagsAndAttributes = 0;
    bool exists = true;
    if (!Utilities::getCreateFileFlagsAndAttributes(path, dwFlagsAndAttributes, exists)) {
        TRACE_ERROR(L"Error in Utilities::getCreateFileFlagsAndAttributes: %ls", path);
        return false;
    }

    if (!exists) {
        TRACE_WARNING(L"File/directory doesn't exist anymore : %ls", path);
        return true;
    }

    if (dwFlagsAndAttributes & FILE_FLAG_OPEN_REPARSE_POINT) {
        // Links are not managed by MS Cloud File API
        return true;
    }

    winrt::handle fileHandle(CreateFile(path, WRITE_DAC, 0, nullptr, OPEN_EXISTING, dwFlagsAndAttributes, nullptr));
    if (fileHandle.get() == INVALID_HANDLE_VALUE) {
        TRACE_ERROR(L"Error in CreateFile : %ls", Utilities::getLastErrorMessage().c_str());
        return false;
    }

    try {
        TRACE_DEBUG(L"Set pin state to placeholder : state = %ld, path = %ls", static_cast<int>(state), path);
        winrt::check_hresult(CfSetPinState(fileHandle.get(), state, CF_SET_PIN_FLAG_RECURSE, nullptr));
    } catch (winrt::hresult_error const &ex) {
        TRACE_ERROR(L"WinRT error caught : %08x - %s", static_cast<HRESULT>(winrt::to_hresult()), ex.message().c_str());
        return false;
    }

    return true;
}
