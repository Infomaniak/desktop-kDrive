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

#pragma warning(disable : 4251)

#include <string>
#include <vector>
#include <WinSock2.h>

class __declspec(dllexport) CommunicationSocket {
    public:
        static std::wstring DefaultPipePath();

        CommunicationSocket();
        ~CommunicationSocket();

        bool Connect(const std::wstring &pipename);
        bool Close();

        bool SendMsg(const wchar_t *) const;
        bool ReadLine(std::wstring *);

        HANDLE Event() { return _pipe; }

    private:
        HANDLE _pipe;
        std::vector<wchar_t> _buffer;
};
