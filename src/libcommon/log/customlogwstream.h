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
#pragma once
#include <sstream>
#include "libcommon/utility/types.h"

class CustomLogWStream : private std::wstringstream {
    public:
        CustomLogWStream() = default;
        inline CustomLogWStream(const CustomLogWStream &wstr) = delete;
        CustomLogWStream &operator=(const CustomLogWStream &) = delete;

        std::wstring str() const { return std::basic_stringstream<wchar_t>::str(); }
        CustomLogWStream &operator<<(const wchar_t *str) {
            // We need to cast to std::wstringstream as the operator<<(std::wstringstream, const wchar_t *str) is defined outside
            // of the class std::wstringstream and therefore it is not applicable to the current object because of the private
            // inheritance
            static_cast<std::wstringstream &>(*this) << str;
            return *this;
        }
        CustomLogWStream &operator<<(int i) {
            std::wstringstream::operator<<(i);
            return *this;
        }
        CustomLogWStream &operator<<(const std::wstring &str) {
            CustomLogWStream::operator<<(str.c_str());
            return *this;
        }
        CustomLogWStream &operator<<(const QIODevice *ptr) {
            std::wstringstream::operator<<(ptr);
            return *this;
        }
};
