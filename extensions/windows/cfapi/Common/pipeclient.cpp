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

#include "pipeclient.h"

LONGLONG PipeClient::_msgId = 0;

PipeClient::PipeClient() : _endListener(false) {
    if (!Utilities::connectToPipeServer()) {
        TRACE_ERROR(L"Error in connectToPipeServer!");
    }

    // Start listener
    TRACE_DEBUG(L"Start pipe client listener");
    _listener = new std::thread(pipeListener, this);
    //_listener->detach();
}

PipeClient::~PipeClient() {
    // Stop listener
    TRACE_DEBUG(L"Stop pipe client listener");
    _endListener = true;
    if (_listener) {
        _listener->join();
    }

    if (!Utilities::disconnectFromPipeServer()) {
        TRACE_ERROR(L"Error in disconnectFromPipeServer!");
    }
}

bool PipeClient::sendMessageWithoutAnswer(const std::wstring &verb, const std::wstring &params) {
    if (!Utilities::writeMessage(verb.c_str(), params.c_str(), 0)) {
        TRACE_ERROR(L"Error in Utilities::writeMessage!");
        return false;
    }

    return true;
}

bool PipeClient::sendMessageWithAnswer(const std::wstring &verb, const std::wstring &params, LONGLONG &msgId) {
    std::unique_lock<std::mutex> lck(_responseMapMutex);
    msgId = ++_msgId;
    _responseMap[msgId] = std::wstring();
    lck.unlock();

    if (!Utilities::writeMessage(verb.c_str(), params.c_str(), msgId)) {
        TRACE_ERROR(L"Error in Utilities::writeMessage!");
        return false;
    }

    return true;
}

bool PipeClient::readMessage(LONGLONG msgId, std::wstring &response) {
    std::unique_lock<std::mutex> lck(_responseMapMutex);
    if (_responseMap.contains(msgId)) {
        do {
            response = _responseMap[msgId];
            if (response.empty()) {
                _responseMapCV.wait(lck);
                continue;
            } else {
                _responseMap.erase(msgId);
            }
        } while (response.empty());
    } else {
        return false;
    }

    return true;
}

void PipeClient::pipeListener(PipeClient *pipeClient) {
    LONGLONG msgId;
    bool valid;
    size_t sep;
    std::wstring response;
    while (!pipeClient->_endListener) {
        if (!Utilities::readMessage(&response)) {
            Sleep(1000);
            continue;
        }

        valid = true;
        if (response.empty()) {
            valid = false;
        } else {
            // Find msg id separator
            sep = response.find(MSG_CDE_SEPARATOR, 0);
            if (sep == std::wstring::npos || sep == 0) {
                valid = false;
            } else {
                // Read msg id
                try {
                    msgId = std::stoll(response.substr(0, sep));
                } catch (const std::exception &) {
                    valid = false;
                }
            }
        }

        if (!valid) {
            Sleep(50);
            continue;
        }

        std::unique_lock<std::mutex> lck(pipeClient->_responseMapMutex);
        if (pipeClient->_responseMap.contains(msgId)) {
            pipeClient->_responseMap[msgId] = response.substr(sep + 1);
            pipeClient->_responseMapCV.notify_all();
        } else {
            TRACE_ERROR(L"Bad message received!");
        }
        lck.unlock();
    }
}
