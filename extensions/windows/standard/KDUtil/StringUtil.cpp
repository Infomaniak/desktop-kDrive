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

#include "stdafx.h"

#include <locale>
#include <string>
#include <codecvt>

#include "StringUtil.h"

std::string StringUtil::toUtf8(const wchar_t *utf16, int len) {
    if (len < 0) {
        len = (int) wcslen(utf16);
    }

    if (len == 0) {
        return std::string();
    }

    int count = WideCharToMultiByte(CP_UTF8, 0, utf16, len, NULL, 0, NULL, NULL);
    std::string str(count, 0);
    WideCharToMultiByte(CP_UTF8, 0, utf16, len, &str[0], count, NULL, NULL);
    return str;
}

std::wstring StringUtil::toUtf16(const char *utf8, int len) {
    if (len < 0) {
        len = (int) strlen(utf8);
    }

    if (len == 0) {
        return std::wstring();
    }

    int count = MultiByteToWideChar(CP_UTF8, 0, utf8, len, NULL, 0);
    std::wstring wstr(count, 0);
    MultiByteToWideChar(CP_UTF8, 0, utf8, len, &wstr[0], count);
    return wstr;
}
