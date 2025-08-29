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

#include "utilities.h"
#include "../../../../src/libcommon/utility/utility_base.h"

#include <winrt\base.h>

#include <vector>
#include <array>
#include <filesystem>

#define PIPE_TIMEOUT 5 * 1000 // ms
#define DEFAULT_BUFLEN 4096

TraceCbk Utilities::s_traceCbk;
std::wstring Utilities::s_appName = std::wstring();
DWORD Utilities::s_processId = 0;
std::wstring Utilities::s_version = std::wstring();
std::wstring Utilities::s_trashURI = std::wstring();
std::wstring Utilities::s_pipeName = std::wstring();
HANDLE Utilities::s_pipe = INVALID_HANDLE_VALUE;

// Definitions for DeviceIoControl - Begin
typedef struct _REPARSE_DATA_BUFFER {
        DWORD ReparseTag;
        WORD ReparseDataLength;
        WORD Reserved;
        union {
                struct {
                        WORD SubstituteNameOffset;
                        WORD SubstituteNameLength;
                        WORD PrintNameOffset;
                        WORD PrintNameLength;
                        ULONG Flags;
                        WCHAR PathBuffer[1];
                } SymbolicLinkReparseBuffer;

                struct {
                        WORD SubstituteNameOffset;
                        WORD SubstituteNameLength;
                        WORD PrintNameOffset;
                        WORD PrintNameLength;
                        WCHAR PathBuffer[1];
                } MountPointReparseBuffer;

                struct {
                        BYTE DataBuffer[1];
                } GenericReparseBuffer;
        };
} REPARSE_DATA_BUFFER;

#define REPARSE_DATA_BUFFER_HEADER_SIZE FIELD_OFFSET(REPARSE_DATA_BUFFER, GenericReparseBuffer)
// Definitions for DeviceIoControl - End

// All methods and fields for this class are static

void Utilities::trace(TraceLevel level, const char *file, int line, const wchar_t *message) {
    if (s_traceCbk) {
        std::wstringstream msgss;
        msgss << std::filesystem::path(file).filename() << L" - " << std::to_wstring(line) << L" - " << message;
        s_traceCbk(level, msgss.str().c_str());
    } else {
        std::wstringstream msgss;
        msgss << std::filesystem::path(file).filename() << L" - " << std::to_wstring(line) << L" - " << message << L"\n";
        wprintf(msgss.str().c_str());
    }
}

void Utilities::traceFileDates(const wchar_t *filePath) {
    FILETIME ftCreationTime;
    FILETIME ftLastAccessTime;
    FILETIME ftLastWriteTime;
    SYSTEMTIME stCreationTimeUTC, stCreationTimeLocal;
    SYSTEMTIME stLastAccessTimeUTC, stLastAccessTimeLocal;
    SYSTEMTIME stLastWriteTimeUTC, stLastWriteTimeLocal;
    winrt::handle fileHandle(CreateFile(filePath, WRITE_DAC, 0, nullptr, OPEN_EXISTING, FILE_FLAG_OPEN_NO_RECALL, nullptr));
    if (fileHandle.get() != INVALID_HANDLE_VALUE) {
        if (GetFileTime(fileHandle.get(), &ftCreationTime, &ftLastAccessTime, &ftLastWriteTime)) {
            FileTimeToSystemTime(&ftCreationTime, &stCreationTimeUTC);
            SystemTimeToTzSpecificLocalTime(NULL, &stCreationTimeUTC, &stCreationTimeLocal);
            FileTimeToSystemTime(&ftLastAccessTime, &stLastAccessTimeUTC);
            SystemTimeToTzSpecificLocalTime(NULL, &stLastAccessTimeUTC, &stLastAccessTimeLocal);
            FileTimeToSystemTime(&ftLastWriteTime, &stLastWriteTimeUTC);
            SystemTimeToTzSpecificLocalTime(NULL, &stLastWriteTimeUTC, &stLastWriteTimeLocal);
            TRACE_DEBUG(L"File path = %ls: creationTime = %d:%d:%d, accessTime = %d:%d:%d, writeTime = %d:%d:%d", filePath,
                        stCreationTimeLocal.wHour, stCreationTimeLocal.wMinute, stCreationTimeLocal.wSecond,
                        stLastAccessTimeLocal.wHour, stLastAccessTimeLocal.wMinute, stLastAccessTimeLocal.wSecond,
                        stLastWriteTimeLocal.wHour, stLastWriteTimeLocal.wMinute, stLastWriteTimeLocal.wSecond);
        }
    } else {
        TRACE_ERROR(L"Error in CreateFile : %ls", Utilities::getLastErrorMessage().c_str());
    }
}

std::string Utilities::utf16ToUtf8(const wchar_t *utf16, int len) {
    if (len < 0) {
        len = (int) wcslen(utf16);
    }

    if (len == 0) {
        return std::string();
    }

    int size_needed = WideCharToMultiByte(CP_UTF8, 0, utf16, len, nullptr, 0, nullptr, nullptr);
    std::string strTo(size_needed, 0);
    WideCharToMultiByte(CP_UTF8, 0, utf16, len, &strTo[0], size_needed, nullptr, nullptr);
    return strTo;
}

std::wstring Utilities::utf8ToUtf16(const char *utf8, int len) {
    if (len < 0) {
        len = (int) strlen(utf8);
    }

    if (len == 0) {
        return std::wstring();
    }

    int size_needed = MultiByteToWideChar(CP_UTF8, 0, utf8, len, nullptr, 0);
    std::wstring wstrTo(size_needed, 0);
    MultiByteToWideChar(CP_UTF8, 0, utf8, len, &wstrTo[0], size_needed);
    return wstrTo;
}

void Utilities::initPipeName(const wchar_t *appName) {
    DWORD len = DEFAULT_BUFLEN;
    wchar_t userName[DEFAULT_BUFLEN];
    GetUserName(userName, &len);
    s_pipeName = std::wstring(L"\\\\.\\pipe\\") + std::wstring(appName) + L"-" + std::wstring(userName, len);
    TRACE_DEBUG(L"Init pipe: name = %ls", s_pipeName.c_str());
}

bool Utilities::connectToPipeServer() {
    TRACE_DEBUG(L"connectToPipeServer");

    if (s_pipe != INVALID_HANDLE_VALUE) {
        return true;
    }

    if (s_pipeName.empty()) {
        TRACE_ERROR(L"Empty pipe name!");
        return false;
    }

    TRACE_DEBUG(L"Open pipe: name = %ls", s_pipeName.c_str());
    while (true) {
        s_pipe = CreateFile(s_pipeName.data(), GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
        if (s_pipe != INVALID_HANDLE_VALUE) {
            TRACE_DEBUG(L"Connected");
            break;
        }

        if (GetLastError() != ERROR_PIPE_BUSY) {
            TRACE_ERROR(L"Failed to open sync engine pipe with error = %ls", getLastErrorMessage().c_str());
            return false;
        }

        // All pipe instances are busy, so wait
        TRACE_DEBUG(L"Wait pipe");
        if (!WaitNamedPipe(s_pipeName.data(), PIPE_TIMEOUT)) {
            TRACE_ERROR(L"Sync engine pipe not available!");
            return false;
        }
    }

    return true;
}

bool Utilities::writeMessage(const std::wstring &verb, const std::wstring &path, LONGLONG msgId) {
    if (s_pipe == INVALID_HANDLE_VALUE) {
        TRACE_ERROR(L"Invalid pipe!");

        if (!connectToPipeServer()) {
            TRACE_ERROR(L"Error in connectToPipeServer");
            return false;
        }
    }

    // Make UTF8 message
    std::wstring msg;
    if (msgId == 0) {
        msg = std::wstring(verb + MSG_CDE_SEPARATOR + path + MSG_END);
    } else {
        msg = std::wstring(verb + MSG_CDE_SEPARATOR + std::to_wstring(msgId) + MSG_ARG_SEPARATOR + path + MSG_END);
    }

    DWORD numBytesWritten = 0;
    if (!WriteFile(s_pipe, msg.c_str(), DWORD(msg.size() * sizeof(wchar_t)), &numBytesWritten, NULL)) {
        TRACE_ERROR(L"Error writing on sync engine pipe: %ls", getLastErrorMessage().c_str());

        if (GetLastError() == ERROR_PIPE_NOT_CONNECTED) {
            // Try to reconnect
            if (!disconnectFromPipeServer()) {
                TRACE_ERROR(L"Error in disconnectFromPipeServer");
            }
            if (!connectToPipeServer()) {
                TRACE_ERROR(L"Error in connectToPipeServer");
                return false;
            }

            if (!WriteFile(s_pipe, msg.c_str(), DWORD(msg.size()), &numBytesWritten, NULL)) {
                TRACE_ERROR(L"Error writing on sync engine pipe (trial 2): %ls", getLastErrorMessage().c_str());
                return false;
            }
        } else {
            return false;
        }
    }

    return true;
}

bool Utilities::readMessage(std::wstring *response) {
    static std::vector<wchar_t> buffer;

    if (!response) {
        TRACE_ERROR(L"Invalid parameter!");
        return false;
    }

    if (s_pipe == INVALID_HANDLE_VALUE) {
        TRACE_ERROR(L"Invalid pipe!");

        if (!connectToPipeServer()) {
            TRACE_ERROR(L"Error in connectToPipeServer");
            return false;
        }
    }

    response->clear();
    while (true) {
        int lbPos = 0;
        auto it = std::find(buffer.begin() + lbPos, buffer.end(), L'\n');
        if (it != buffer.end()) {
            *response = std::wstring(buffer.data(), DWORD(it - buffer.begin()));
            buffer.erase(buffer.begin(), it + 1);
            return true;
        }

        std::array<wchar_t, 1024> resp;
        DWORD numBytesRead = 0;
        DWORD totalBytesAvailable = 0;

        if (!PeekNamedPipe(s_pipe, NULL, 0, 0, &totalBytesAvailable, 0)) {
            TRACE_ERROR(L"Error reading on sync engine pipe: %ls", getLastErrorMessage().c_str());

            if (GetLastError() == ERROR_PIPE_NOT_CONNECTED) {
                // Try to reconnect
                if (!disconnectFromPipeServer()) {
                    TRACE_ERROR(L"Error in disconnectFromPipeServer");
                }
                if (!connectToPipeServer()) {
                    TRACE_ERROR(L"Error in connectToPipeServer");
                    return false;
                }

                if (!PeekNamedPipe(s_pipe, NULL, 0, 0, &totalBytesAvailable, 0)) {
                    TRACE_ERROR(L"Error reading on sync engine pipe (trial 2): %ls", getLastErrorMessage().c_str());
                    return false;
                }
            } else {
                return false;
            }
        }

        if (totalBytesAvailable == 0) {
            break;
        }

        if (!ReadFile(s_pipe, resp.data(), DWORD(resp.size() * sizeof(wchar_t)), &numBytesRead, NULL)) {
            TRACE_ERROR(L"Error reading on sync engine pipe: %ls", getLastErrorMessage().c_str());
            return false;
        }

        if (numBytesRead <= 0) {
            return false;
        }

        buffer.insert(buffer.end(), resp.begin(), resp.begin() + numBytesRead);
    }

    return true;
}

bool Utilities::disconnectFromPipeServer() {
    TRACE_DEBUG(L"disconnectFromPipeServer");

    if (s_pipe == INVALID_HANDLE_VALUE) {
        TRACE_ERROR(L"Invalid pipe!");
        return false;
    }

    CloseHandle(s_pipe);
    s_pipe = INVALID_HANDLE_VALUE;

    return true;
}

bool Utilities::readNextValue(std::wstring &message, std::wstring &value) {
    auto sep = message.find(MSG_CDE_SEPARATOR, 0);
    if (sep == std::wstring::npos) {
        value = message;
        message.clear();
    } else {
        value = message.substr(0, sep);
        message.erase(0, sep + 1);
    }

    return true;
}

std::wstring Utilities::getLastErrorMessage() {
    const DWORD errorMessageID = ::GetLastError();
    if (errorMessageID == 0) return {};

    LPWSTR messageBuffer = nullptr;
    const size_t size =
            FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, nullptr,
                           errorMessageID, NULL, (LPWSTR) &messageBuffer, 0, nullptr);

    // Escape quotes
    const auto msg = std::wstring(messageBuffer, size);
    std::wostringstream message;
    message << errorMessageID << L" - " << msg;

    LocalFree(messageBuffer);

    return message.str();
}

bool Utilities::checkIfIsLink(const wchar_t *path, bool &isSymlink, bool &isJunction, bool &exists) {
    isSymlink = false;
    isJunction = false;
    exists = true;

    HANDLE hFile = CreateFileW(path, FILE_WRITE_ATTRIBUTES, FILE_SHARE_DELETE | FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
                               OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        exists = !KDC::utility_base::isLikeFileNotFoundError(GetLastError());
        return !exists;
    }

    BYTE buf[MAXIMUM_REPARSE_DATA_BUFFER_SIZE];
    REPARSE_DATA_BUFFER &ReparseBuffer = (REPARSE_DATA_BUFFER &) buf;
    DWORD dwRet;
    if (!DeviceIoControl(hFile, FSCTL_GET_REPARSE_POINT, NULL, 0, &ReparseBuffer, MAXIMUM_REPARSE_DATA_BUFFER_SIZE, &dwRet,
                         NULL)) {
        DWORD dwError = GetLastError();
        CloseHandle(hFile);

        if (const bool notReparsePoint = (dwError == ERROR_NOT_A_REPARSE_POINT); notReparsePoint) {
            return true;
        }

        exists = !KDC::utility_base::isLikeFileNotFoundError(GetLastError());
        return !exists;
    }

    CloseHandle(hFile);

    isSymlink = ReparseBuffer.ReparseTag == IO_REPARSE_TAG_SYMLINK;
    isJunction = ReparseBuffer.ReparseTag == IO_REPARSE_TAG_MOUNT_POINT;

    return true;
}

bool Utilities::checkIfIsDirectory(const wchar_t *path, bool &isDirectory, bool &exists) {
    exists = true;

    std::error_code ec;
    isDirectory = std::filesystem::is_directory(std::filesystem::path(path), ec);
    if (!isDirectory && ec.value() != 0) {
        exists = !KDC::utility_base::isLikeFileNotFoundError(ec);
        if (!exists) {
            return true;
        }

        TRACE_ERROR(L"Failed to check if the item is a directory: %ls (%d)", path, ec.value());
        return false;
    }

    return true;
}

bool Utilities::getCreateFileFlagsAndAttributes(const wchar_t *path, DWORD &dwFlagsAndAttributes, bool &exists) {
    dwFlagsAndAttributes = 0;
    exists = true;

    bool isSymlink = false;
    bool isJunction = false;
    if (!Utilities::checkIfIsLink(path, isSymlink, isJunction, exists)) {
        TRACE_ERROR(L"Error in Utilities::checkIfIsLink: %ls", path);
        return false;
    }

    if (!exists) {
        TRACE_WARNING(L"File/directory doesn't exist anymore : %ls", path);
        return true;
    }

    if (isSymlink) {
        dwFlagsAndAttributes = FILE_FLAG_OPEN_REPARSE_POINT | FILE_FLAG_OPEN_NO_RECALL;
    } else if (isJunction) {
        dwFlagsAndAttributes = FILE_FLAG_OPEN_REPARSE_POINT | FILE_FLAG_BACKUP_SEMANTICS;
    } else {
        bool isDirectory = false;
        if (!Utilities::checkIfIsDirectory(path, isDirectory, exists)) {
            TRACE_ERROR(L"Error in Utilities::checkIfDirectory : %ls", path);
            return false;
        }

        if (!exists) {
            TRACE_WARNING(L"File/directory doesn't exist anymore : %ls", path);
            return true;
        }

        dwFlagsAndAttributes = (isDirectory ? FILE_FLAG_BACKUP_SEMANTICS : FILE_FLAG_OPEN_NO_RECALL);
    }

    return true;
}
