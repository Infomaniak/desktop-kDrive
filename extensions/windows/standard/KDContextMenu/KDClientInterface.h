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

#include <string>
#include <vector>
#include <unordered_map>
#include <queue>
#include <thread>
#include <mutex>
#include <atomic>
#include <condition_variable>

class CommunicationSocket;

class KDClientInterface {
    public:
        struct ContextMenuInfo {
                std::vector<std::wstring> watchedDirectories;
                std::wstring contextMenuTitle;
                struct MenuItem {
                        std::wstring command, flags, title;
                };
                std::vector<MenuItem> menuItems;
        };
        static ContextMenuInfo FetchInfo(const std::wstring &files);
        static void SendRequest(const wchar_t *verb, const std::wstring &path);
};
