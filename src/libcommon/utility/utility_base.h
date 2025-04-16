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


#ifdef _WIN32
#include <shlobj.h>
#include <winbase.h>
#include <windows.h>
#include <winerror.h>
#include <strsafe.h>

#include <sstream>
#endif

#include <string>
#include <system_error>

namespace KDC::utility_base {

#ifdef _WIN32
inline std::wstring getErrorMessage(DWORD errorMessageID) {
    LPWSTR messageBuffer = nullptr;
    const size_t size =
            FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, nullptr,
                           errorMessageID, NULL, (LPWSTR) &messageBuffer, 0, nullptr);

    // Escape quotes
    const auto msg = std::wstring(messageBuffer, size);
    std::wostringstream message;
    message << L"(" << errorMessageID << L") - " << msg;

    LocalFree(messageBuffer);

    return message.str();
}

inline std::wstring getLastErrorMessage() {
    return getErrorMessage(GetLastError());
}
inline bool isLikeFileNotFoundError(DWORD dwError) noexcept {
    return (dwError == ERROR_FILE_NOT_FOUND) || (dwError == ERROR_PATH_NOT_FOUND) || (dwError == ERROR_INVALID_DRIVE) ||
           (dwError == ERROR_BAD_NETPATH);
}

inline bool isLikeFileNotFoundError(const std::error_code &ec) noexcept {
    return isLikeFileNotFoundError(static_cast<DWORD>(ec.value()));
}

#endif

#if defined(__APPLE__) || defined(__unix__)
inline bool isLikeFileNotFoundError(const std::error_code &ec) noexcept {
    return ec.value() == static_cast<int>(std::errc::no_such_file_or_directory);
}
#endif


} // namespace KDC::utility_base
