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

#include "utilities.h"

#include <thread>
#include <condition_variable>
#include <unordered_map>

class PipeClient {
    public:
        static PipeClient &getInstance() {
            static PipeClient instance;
            return instance;
        }

        PipeClient(PipeClient const &) = delete;
        void operator=(PipeClient const &) = delete;

        bool sendMessageWithoutAnswer(const std::wstring &verb, const std::wstring &params);
        bool sendMessageWithAnswer(const std::wstring &verb, const std::wstring &params, LONGLONG &msgId);
        bool readMessage(LONGLONG msgId, std::wstring &response);

    private:
        static LONGLONG _msgId;
        std::unordered_map<LONGLONG, std::wstring> _responseMap;
        std::mutex _responseMapMutex;
        std::condition_variable _responseMapCV;
        bool _endListener;
        std::jthread *_listener;

        PipeClient();
        ~PipeClient();

        static void pipeListener(PipeClient *pipeClient);
};
