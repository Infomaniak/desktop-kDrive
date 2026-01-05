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

#include "KDClientInterface.h"

#include "CommunicationSocket.h"
#include "StringUtil.h"

#include <shlobj.h>

#include <Strsafe.h>

#include <algorithm>
#include <iostream>
#include <sstream>
#include <iterator>
#include <unordered_set>

using namespace std;

#define PIPE_TIMEOUT 5 * 1000 // ms
#define QUERY_END_SEPARATOR L"\\/\n"

KDClientInterface::ContextMenuInfo KDClientInterface::FetchInfo(const std::wstring &files) {
    auto pipename = CommunicationSocket::DefaultPipePath();

    CommunicationSocket socket;
    if (!WaitNamedPipe(pipename.data(), PIPE_TIMEOUT)) {
        return {};
    }
    if (!socket.Connect(pipename)) {
        return {};
    }

    std::wstring msg = L"GET_STRINGS:CONTEXT_MENU_TITLE";
    msg += QUERY_END_SEPARATOR;
    socket.SendMsg(msg.data());

    msg = L"GET_MENU_ITEMS:" + files;
    msg += QUERY_END_SEPARATOR;
    socket.SendMsg(msg.data());

    ContextMenuInfo info;
    std::wstring response;
    bool vfsModeCompatible = false;
    int sleptCount = 0;
    while (sleptCount < 5) {
        if (socket.ReadLine(&response)) {
            if (StringUtil::begins_with(response, wstring(L"REGISTER_PATH:"))) {
                wstring responsePath = response.substr(14); // length of REGISTER_PATH
                info.watchedDirectories.push_back(responsePath);
            } else if (StringUtil::begins_with(response, wstring(L"STRING:"))) {
                wstring stringName, stringValue;
                if (!StringUtil::extractChunks(response, stringName, stringValue)) continue;
                if (stringName == L"CONTEXT_MENU_TITLE") info.contextMenuTitle = std::move(stringValue);
            } else if (StringUtil::begins_with(response, wstring(L"VFS_MODE:"))) {
                wstring vfsMode = response.substr(9); // length of VFS_MODE
                vfsModeCompatible = (vfsMode.compare(L"off") == 0 || vfsMode.compare(L"suffix") == 0);
            } else if (StringUtil::begins_with(response, wstring(L"MENU_ITEM:"))) {
                if (vfsModeCompatible) {
                    wstring commandName, flags, title;
                    if (!StringUtil::extractChunks(response, commandName, flags, title)) continue;
                    info.menuItems.push_back({commandName, flags, title});
                }
            } else if (StringUtil::begins_with(response, wstring(L"GET_MENU_ITEMS:END"))) {
                break; // Stop once we completely received the last sent request
            }
        } else {
            if (socket.Event() == INVALID_HANDLE_VALUE) {
                return {};
            }
            Sleep(50);
            ++sleptCount;
        }
    }
    return info;
}

void KDClientInterface::SendRequest(const wchar_t *verb, const std::wstring &path) {
    auto pipename = CommunicationSocket::DefaultPipePath();

    CommunicationSocket socket;
    if (!WaitNamedPipe(pipename.data(), PIPE_TIMEOUT)) {
        return;
    }
    if (!socket.Connect(pipename)) {
        return;
    }


    socket.SendMsg((verb + (L":" + path + QUERY_END_SEPARATOR)).data());
}
