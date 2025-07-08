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

#define KD_WINDOWS

#include "framework.h"
#include "debug.h"
#include "targetver.h"

#include <iostream>
#include <sstream>
#include <string>

#include <fileapi.h>
#include <sddl.h>
#include <searchapi.h>
#include <minwinbase.h>
#include <cfapi.h>
#include <source_location>


#define MAX_URI 255
#define MAX_FULL_PATH 32000

#define MSG_CDE_SEPARATOR L':'
#define MSG_ARG_SEPARATOR L'\x1e'
#define MSG_END L"\\/\n"

#define TRACE(TRACE_LEVEL, MESSAGE, ...)                                           \
    {                                                                              \
        const auto location = std::source_location::current();                     \
        const int sz = 255;                                                        \
        wchar_t msg[sz];                                                           \
        swprintf(msg, sz, MESSAGE, __VA_ARGS__);                                   \
        Utilities::trace(TRACE_LEVEL, location.file_name(), location.line(), msg); \
    }
#define TRACE_INFO(MESSAGE, ...) TRACE(TraceLevel::Info, MESSAGE, __VA_ARGS__)
#define TRACE_DEBUG(MESSAGE, ...) TRACE(TraceLevel::Debug, MESSAGE, __VA_ARGS__)
#define TRACE_WARNING(MESSAGE, ...) TRACE(TraceLevel::Warning, MESSAGE, __VA_ARGS__)
#define TRACE_ERROR(MESSAGE, ...) TRACE(TraceLevel::Error, MESSAGE, __VA_ARGS__)

class Utilities {
    public:
        Utilities();

        inline static LARGE_INTEGER fileTimeToLargeInteger(const FILETIME fileTime) {
            LARGE_INTEGER largeInteger;
            largeInteger.LowPart = fileTime.dwLowDateTime;
            largeInteger.HighPart = fileTime.dwHighDateTime;
            return largeInteger;
        }

        inline static LARGE_INTEGER longLongToLargeInteger(const LONGLONG longlong) {
            LARGE_INTEGER largeInteger;
            largeInteger.QuadPart = longlong;
            return largeInteger;
        }

        inline static CF_OPERATION_INFO toOperationInfo(CF_CALLBACK_INFO const *info, CF_OPERATION_TYPE operationType) {
            CF_OPERATION_INFO operationInfo{sizeof(CF_OPERATION_INFO), operationType, info->ConnectionKey, info->TransferKey};

            return operationInfo;
        }

        static void trace(TraceLevel level, const char *file, int line, const wchar_t *message);
        static void traceFileDates(const wchar_t *filePath);

        static std::string utf16ToUtf8(const wchar_t *utf16, int len = -1);
        static std::wstring utf8ToUtf16(const char *utf8, int len = -1);

        template<class T>
        static bool begins_with(const T &input, const T &match) {
            return input.size() >= match.size() && std::equal(match.begin(), match.end(), input.begin());
        }

        template<class T>
        static bool ends_with(const T &input, const T &match) {
            return input.size() >= match.size() && std::equal(match.rbegin(), match.rend(), input.rbegin());
        }

        static void initPipeName(const wchar_t *appName);

        static bool connectToPipeServer();
        static bool writeMessage(const std::wstring &verb, const std::wstring &path, LONGLONG msgId = 0);
        static bool readMessage(std::wstring *response);
        static bool disconnectFromPipeServer();

        static bool readNextValue(std::wstring &message, std::wstring &value);

        static std::wstring getLastErrorMessage();

        static bool checkIfIsLink(const wchar_t *path, bool &isSymlink, bool &isJunction, bool &exists);
        static bool checkIfIsDirectory(const wchar_t *path, bool &isDirectory, bool &exists);
        static bool getCreateFileFlagsAndAttributes(const wchar_t *path, DWORD &dwFlagsAndAttributes, bool &exists);

        static TraceCbk s_traceCbk;
        static std::wstring s_appName;
        static DWORD s_processId;
        static std::wstring s_version;
        static std::wstring s_trashURI;
        static std::wstring s_pipeName;
        static HANDLE s_pipe;
};
