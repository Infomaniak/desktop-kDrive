/*
 * Infomaniak kDrive - Desktop
 * Copyright (C) 2023-2026 Infomaniak Network SA
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

#include "changeJournal_win.h"

#include "libcommonserver/utility/utility.h"

#include <array>
#include <cstdio>
#include <vector>

#include <winioctl.h>

namespace KDC {

// Size of the buffer used to read journal records at once.
static constexpr DWORD kReadBufferSize = 64 * 1024;
// Prefix used to build the opaque cursor string.
static constexpr std::string_view kCursorPrefix = "USNJRNL";

ChangeJournal::ChangeJournal(const SyncPath &path) :
    _path(path),
    _logger(Log::instance()->getLogger()) {}

ChangeJournal::~ChangeJournal() {
    closeVolume();
}

ExitInfo ChangeJournal::openVolume() {
    if (_volumeHandle != INVALID_HANDLE_VALUE) return ExitCode::Ok;

    // Determine the volume mount point hosting the observed path (e.g. "C:\").
    std::array<wchar_t, MAX_PATH> volumeMountPoint{};
    if (!GetVolumePathNameW(_path.native().c_str(), volumeMountPoint.data(), static_cast<DWORD>(volumeMountPoint.size()))) {
        const DWORD errorCode = GetLastError();
        LOGW_WARN(_logger, L"GetVolumePathNameW failed for " << Utility::formatSyncPath(_path) << L" - error:" << errorCode);
        return {ExitCode::SystemError, ExitCause::SyncDirAccessError};
    }

    // Build the device path (e.g. "\\.\C:") from the drive letter of the mount point.
    std::wstring mountPoint(volumeMountPoint.data());
    if (mountPoint.empty() || mountPoint[1] != L':') {
        LOGW_WARN(_logger, L"Unsupported volume mount point: " << mountPoint);
        return {ExitCode::SystemError, ExitCause::FileSystemNotSupported};
    }
    _volumePath = L"\\\\.\\";
    _volumePath += mountPoint[0];
    _volumePath += L':';

    // Open the volume to query and read the USN journal in unprivileged mode.
    _volumeHandle = CreateFileW(_volumePath.c_str(), MAXIMUM_ALLOWED, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING,
                                FILE_FLAG_BACKUP_SEMANTICS, nullptr);

    if (_volumeHandle == INVALID_HANDLE_VALUE) {
        const DWORD errorCode = GetLastError();
        LOGW_WARN(_logger, L"CreateFileW failed for volume " << _volumePath << L" - error:" << errorCode);
        return {ExitCode::SystemError, ExitCause::SyncDirAccessError};
    }

    return ExitCode::Ok;
}

void ChangeJournal::closeVolume() {
    if (_volumeHandle != INVALID_HANDLE_VALUE) {
        CloseHandle(_volumeHandle);
        _volumeHandle = INVALID_HANDLE_VALUE;
    }
}

ExitInfo ChangeJournal::queryJournal(USN_JOURNAL_DATA_V0 &usnJournalData) {
    if (const ExitInfo exitInfo = openVolume(); !exitInfo) return exitInfo;

    DWORD bytesReturned = 0;
    if (!DeviceIoControl(_volumeHandle, FSCTL_QUERY_USN_JOURNAL, nullptr, 0, &usnJournalData, sizeof(usnJournalData),
                         &bytesReturned, nullptr)) {
        const DWORD errorCode = GetLastError();
        if (errorCode == ERROR_JOURNAL_NOT_ACTIVE || errorCode == ERROR_INVALID_FUNCTION || errorCode == ERROR_NOT_SUPPORTED) {
            LOGW_DEBUG(_logger, L"No active USN journal on volume " << _volumePath << L" - error:" << errorCode);
            return {ExitCode::SystemError, ExitCause::FileSystemNotSupported};
        }
        LOGW_WARN(_logger, L"FSCTL_QUERY_USN_JOURNAL failed on volume " << _volumePath << L" - error:" << errorCode);
        return {ExitCode::SystemError, ExitCause::FileAccessError};
    }

    _journalId = usnJournalData.UsnJournalID;
    return ExitCode::Ok;
}

bool ChangeJournal::supportsChangeJournal() {
    USN_JOURNAL_DATA_V0 usnJournalData{};
    return static_cast<bool>(queryJournal(usnJournalData));
}

ExitInfo ChangeJournal::currentCursor(std::string &cursor) {
    USN_JOURNAL_DATA_V0 usnJournalData{};
    if (const ExitInfo exitInfo = queryJournal(usnJournalData); !exitInfo) return exitInfo;

    cursor = makeCursor(usnJournalData.UsnJournalID, usnJournalData.NextUsn);
    return ExitCode::Ok;
}

ExitInfo ChangeJournal::getEntries(const std::string &fromCursor, std::list<ChangeJournalEntry> &entries,
                                   std::string &nextCursor) {
    entries.clear();

    uint64_t cursorJournalId = 0;
    int64_t startUsn = 0;
    if (!parseCursor(fromCursor, cursorJournalId, startUsn)) {
        LOG_WARN(_logger, "Malformed change journal cursor");
        return {ExitCode::SystemError, ExitCause::InvalidArgument};
    }

    USN_JOURNAL_DATA_V0 usnJournalData{};
    if (const ExitInfo exitInfo = queryJournal(usnJournalData); !exitInfo) return exitInfo;

    // Security / consistency check: reject cursors generated against a different (deleted and recreated) journal.
    if (cursorJournalId != usnJournalData.UsnJournalID) {
        LOG_WARN(_logger, "Change journal identifier mismatch (cursor: " << cursorJournalId
                                                                         << ", current: " << usnJournalData.UsnJournalID
                                                                         << "), a full rescan is required");
        return {ExitCode::DataError, ExitCause::InvalidArgument};
    }

    // A start USN older than the first available USN means records were purged: incremental data cannot be trusted.
    if (startUsn != 0 && startUsn < usnJournalData.FirstUsn) {
        LOG_WARN(_logger, "Requested USN " << startUsn << " is older than the first available USN " << usnJournalData.FirstUsn
                                           << ", a full rescan is required");
        return {ExitCode::DataError, ExitCause::InvalidArgument};
    }

    READ_USN_JOURNAL_DATA_V1 readData{};
    readData.StartUsn = startUsn;
    readData.ReasonMask = USN_REASON_BASIC_INFO_CHANGE | USN_REASON_CLOSE | USN_REASON_COMPRESSION_CHANGE |
                          USN_REASON_DATA_EXTEND | USN_REASON_DATA_OVERWRITE | USN_REASON_DATA_TRUNCATION | USN_REASON_EA_CHANGE |
                          USN_REASON_ENCRYPTION_CHANGE | USN_REASON_FILE_CREATE | USN_REASON_FILE_DELETE |
                          USN_REASON_NAMED_DATA_EXTEND | USN_REASON_RENAME_NEW_NAME | USN_REASON_RENAME_OLD_NAME |
                          USN_REASON_REPARSE_POINT_CHANGE;

    readData.ReturnOnlyOnClose = FALSE;
    readData.Timeout = 0;
    readData.BytesToWaitFor = 0;
    readData.UsnJournalID = usnJournalData.UsnJournalID;
    readData.MinMajorVersion = 2;
    readData.MaxMajorVersion = 2;

    std::vector<char> buffer(kReadBufferSize);
    int64_t lastUsn = usnJournalData.NextUsn;

    while (true) {
        DWORD bytesReturned = 0;
        // Use the unprivileged read control code (Windows 8+) so that reading the journal does not require administrator
        // privileges. Fall back to the classic control code only if the unprivileged variant is not supported.
        if (!DeviceIoControl(_volumeHandle, FSCTL_READ_UNPRIVILEGED_USN_JOURNAL, &readData, sizeof(readData), buffer.data(),
                             static_cast<DWORD>(buffer.size()), &bytesReturned, nullptr)) {
            const DWORD unprivilegedError = GetLastError();
            if (unprivilegedError == ERROR_INVALID_FUNCTION || unprivilegedError == ERROR_NOT_SUPPORTED) {
                LOGW_WARN(_logger, L"FSCTL_READ_UNPRIVILEGED_USN_JOURNAL failed on volume " << _volumePath << L" - error:"
                                                                                            << unprivilegedError);
                return {ExitCode::SystemError, ExitCause::FileAccessError};
            }
        }

        // The first 8 bytes of the returned buffer contain the USN to use for the next read.
        if (bytesReturned < sizeof(USN)) break;
        const auto nextStartUsn = *reinterpret_cast<const USN *>(buffer.data());

        DWORD offset = sizeof(USN);
        bool recordFound = false;
        while (offset < bytesReturned) {
            const auto *record = reinterpret_cast<const USN_RECORD_V2 *>(buffer.data() + offset);
            if (record->RecordLength == 0) break;
            recordFound = true;

            ChangeJournalEntry entry;
            entry.fileReferenceNumber = record->FileReferenceNumber;
            entry.parentFileReferenceNumber = record->ParentFileReferenceNumber;
            entry.usn = record->Usn;
            entry.timestamp = record->TimeStamp.QuadPart;
            entry.reason = static_cast<ChangeJournalReason>(record->Reason);
            entry.fileAttributes = record->FileAttributes;
            entry.fileName.assign(
                    reinterpret_cast<const wchar_t *>(reinterpret_cast<const char *>(record) + record->FileNameOffset),
                    record->FileNameLength / sizeof(wchar_t));
            lastUsn = record->Usn;
            entries.push_back(std::move(entry));

            offset += record->RecordLength;
        }

        readData.StartUsn = nextStartUsn;

        // Stop once the journal has been fully drained.
        if (!recordFound || nextStartUsn == 0 || nextStartUsn >= usnJournalData.NextUsn) break;
    }

    nextCursor = makeCursor(usnJournalData.UsnJournalID,
                            entries.empty() ? usnJournalData.NextUsn : std::max(lastUsn + 1, readData.StartUsn));
    return ExitCode::Ok;
}

ExitInfo ChangeJournal::pathFromFileReferenceNumber(uint64_t fileReferenceNumber, SyncPath &path) {
    if (const ExitInfo exitInfo = openVolume(); !exitInfo) return exitInfo;

    // Open the file by its NTFS reference number. FILE_READ_ATTRIBUTES is enough to later resolve the path, and using the
    // shared volume handle as root lets us open items by id without any elevation.
    FILE_ID_DESCRIPTOR idDescriptor{};
    idDescriptor.dwSize = sizeof(idDescriptor);
    idDescriptor.Type = FileIdType;
    idDescriptor.FileId.QuadPart = static_cast<LONGLONG>(fileReferenceNumber);

    const HANDLE fileHandle =
            OpenFileById(_volumeHandle, &idDescriptor, FILE_READ_ATTRIBUTES,
                         FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, FILE_FLAG_BACKUP_SEMANTICS);
    if (fileHandle == INVALID_HANDLE_VALUE) {
        const DWORD errorCode = GetLastError();
        // The item may have been deleted since the record was written: this is an expected, non-fatal situation.
        if (errorCode == ERROR_FILE_NOT_FOUND || errorCode == ERROR_PATH_NOT_FOUND || errorCode == ERROR_INVALID_PARAMETER) {
            return {ExitCode::SystemError, ExitCause::NotFound};
        }
        LOGW_WARN(_logger, L"OpenFileById failed on volume " << _volumePath << L" for id " << fileReferenceNumber << L" - error:"
                                                             << errorCode);
        return {ExitCode::SystemError, ExitCause::FileAccessError};
    }

    // Resolve the final path. GetFinalPathNameByHandleW returns the drive-letter form ("C:\\...").
    std::wstring resolved(MAX_PATH, L'\0');
    DWORD length = GetFinalPathNameByHandleW(fileHandle, resolved.data(), static_cast<DWORD>(resolved.size()),
                                             FILE_NAME_NORMALIZED | VOLUME_NAME_DOS);
    if (length > resolved.size()) {
        resolved.resize(length);
        length = GetFinalPathNameByHandleW(fileHandle, resolved.data(), static_cast<DWORD>(resolved.size()),
                                           FILE_NAME_NORMALIZED | VOLUME_NAME_DOS);
    }
    const DWORD errorCode = length == 0 ? GetLastError() : ERROR_SUCCESS;
    CloseHandle(fileHandle);

    if (length == 0) {
        LOGW_WARN(_logger, L"GetFinalPathNameByHandleW failed for id " << fileReferenceNumber << L" - error:" << errorCode);
        return {ExitCode::SystemError, ExitCause::FileAccessError};
    }
    resolved.resize(length);

    // Strip the "\\?\" extended-length prefix if present.
    static const std::wstring extendedPrefix = L"\\\\?\\";
    if (resolved.starts_with(extendedPrefix)) resolved.erase(0, extendedPrefix.size());

    path = SyncPath(resolved);
    return ExitCode::Ok;
}

ExitInfo ChangeJournal::populateEntryCurrentPath(ChangeJournalEntry &entry, bool resolveFullPath) {
    // Try to resolve the item itself first.
    SyncPath itemPath;
    const ExitInfo itemExitInfo = pathFromFileReferenceNumber(entry.fileReferenceNumber, itemPath);
    if (itemExitInfo) {
        if (entry.fileName.empty()) entry.fileName = itemPath.filename().native();
        if (resolveFullPath) entry.path = itemPath;
        return ExitCode::Ok;
    }

    return itemExitInfo;
}

std::string ChangeJournal::makeCursor(uint64_t journalId, int64_t usn) {
    // Format: "USNJRNL:<journalId>:<usn>".
    std::array<char, 64> buffer{};
    (void) std::snprintf(buffer.data(), buffer.size(), "%.*s:%llu:%lld", static_cast<int>(kCursorPrefix.size()),
                         kCursorPrefix.data(), static_cast<unsigned long long>(journalId), static_cast<long long>(usn));
    return std::string(buffer.data());
}

bool ChangeJournal::parseCursor(const std::string &cursor, uint64_t &journalId, int64_t &usn) {
    unsigned long long parsedJournalId = 0;
    long long parsedUsn = 0;
    std::array<char, 16> prefix{};
    const int matched = std::sscanf(cursor.c_str(), "%7[^:]:%llu:%lld", prefix.data(), &parsedJournalId, &parsedUsn);
    if (matched != 3) return false;
    if (kCursorPrefix != prefix.data()) return false;
    if (parsedUsn < 0) return false;

    journalId = parsedJournalId;
    usn = static_cast<int64_t>(parsedUsn);
    return true;
}

} // namespace KDC
