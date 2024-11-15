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

#include <QIODevice>

class CustomLogStream : private std::stringstream {
    public:
        CustomLogStream() = default;
        inline CustomLogStream(const CustomLogStream &str) = delete;
        CustomLogStream &operator=(const CustomLogStream &) = delete;

        std::string str() const { return std::basic_stringstream<char>::str(); }

        // We need to cast to std::stringstream as operators<<(std::stringstream, const wchar_t *str /*and const std::wstring
        // &str*/) are defined outside of the class std::stringstream and therefore it is not applicable to the current object
        // because of the private inheritance
        CustomLogStream &operator<<(const char *str) {
            static_cast<std::stringstream &>(*this) << str;
            return *this;
        }
        CustomLogStream &operator<<(const std::string &str) {
            static_cast<std::stringstream &>(*this) << str;
            return *this;
        }

        CustomLogStream &operator<<(bool b) {
            std::stringstream::operator<<(std::boolalpha);
            std::stringstream::operator<<(b);
            return *this;
        }
        CustomLogStream &operator<<(int i) {
            std::stringstream::operator<<(i);
            return *this;
        }
        CustomLogStream &operator<<(long i64) {
            std::stringstream::operator<<(i64);
            return *this;
        }
        CustomLogStream &operator<<(unsigned int ui) {
            std::stringstream::operator<<(ui);
            return *this;
        }
        CustomLogStream &operator<<(long long i64) {
            std::stringstream::operator<<(i64);
            return *this;
        }
        CustomLogStream &operator<<(unsigned long ul) {
            std::stringstream::operator<<(ul);
            return *this;
        }
        CustomLogStream &operator<<(unsigned long long ui64) {
            std::stringstream::operator<<(ui64);
            return *this;
        }
        CustomLogStream &operator<<(double d) {
            std::stringstream::operator<<(d);
            return *this;
        }
        CustomLogStream &operator<<(const QIODevice *ptr) {
            std::stringstream::operator<<(ptr);
            return *this;
        }
        CustomLogStream &operator<<(const std::error_code &code) {
            std::stringstream::operator<<(code.value());
            return *this;
        }
};

class CustomLogWStream : private std::wstringstream {
    public:
        CustomLogWStream() = default;
        inline CustomLogWStream(const CustomLogWStream &wstr) = delete;
        CustomLogWStream &operator=(const CustomLogWStream &) = delete;

        std::wstring str() const { return std::basic_stringstream<wchar_t>::str(); }

        // We need to cast to std::wstringstream as operators<<(std::wstringstream, const wchar_t *str /*and const std::wstring
        // &str*/) are defined outside of the class std::wstringstream and therefore it is not applicable to the current object
        // because of the private inheritance
        CustomLogWStream &operator<<(const wchar_t *str) {
            static_cast<std::wstringstream &>(*this) << str;
            return *this;
        }
        CustomLogWStream &operator<<(const std::wstring &str) {
            static_cast<std::wstringstream &>(*this) << str;
            return *this;
        }

        CustomLogWStream &operator<<(bool b) {
            std::wstringstream::operator<<(std::boolalpha);
            std::wstringstream::operator<<(b);
            return *this;
        }
        CustomLogWStream &operator<<(int i) {
            std::wstringstream::operator<<(i);
            return *this;
        }
        CustomLogWStream &operator<<(unsigned int ui) {
            std::wstringstream::operator<<(ui);
            return *this;
        }
        CustomLogWStream &operator<<(long i64) {
            std::wstringstream::operator<<(i64);
            return *this;
        }
        CustomLogWStream &operator<<(long long i64) {
            std::wstringstream::operator<<(i64);
            return *this;
        }
        CustomLogWStream &operator<<(unsigned long ul) {
            std::wstringstream::operator<<(ul);
            return *this;
        }
        CustomLogWStream &operator<<(unsigned long long ui64) {
            std::wstringstream::operator<<(ui64);
            return *this;
        }
        CustomLogWStream &operator<<(double d) {
            std::wstringstream::operator<<(d);
            return *this;
        }
        CustomLogWStream &operator<<(const QIODevice *ptr) {
            std::wstringstream::operator<<(ptr);
            return *this;
        }
        CustomLogWStream &operator<<(const std::error_code &code) {
            std::wstringstream::operator<<(code.value());
            return *this;
        }
};
