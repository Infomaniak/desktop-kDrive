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

#include "pipecommserver.h"
#include "libcommonserver/log/log.h"

#include <log4cplus/loggingmacros.h>

#ifdef _WIN32
#include <windows.h>
#include <stdio.h>
#include <tchar.h>
#include <strsafe.h>

#define BUFSIZE 512

DWORD WINAPI InstanceThread(LPVOID);
VOID GetAnswerToRequest(LPTSTR, LPTSTR, LPDWORD);
#endif

namespace KDC {

PipeCommChannel::PipeCommChannel() :
    AbstractCommChannel() {}

PipeCommChannel::~PipeCommChannel() {}

uint64_t PipeCommChannel::readData(char *data, uint64_t maxlen) {
    return 0;
}

uint64_t PipeCommChannel::writeData(const char *data, uint64_t len) {
    return 0;
}

uint64_t PipeCommChannel::bytesAvailable() const {
    return 0;
}

bool PipeCommChannel::canReadLine() const {
    return true;
}

std::string PipeCommChannel::id() const {
    return "";
}

PipeCommServer::PipeCommServer(const std::string &name) :
    AbstractCommServer(name) {}

PipeCommServer::~PipeCommServer() {
    LOG_DEBUG(Log::instance()->getLogger(), name() << " destroyed");
    log4cplus::threadCleanup();
}

void PipeCommServer::close() {
    if (_isRunning) {
        stop();
    }
    waitForExit();
}

bool PipeCommServer::listen(const SyncPath &pipePath) {
    if (_isRunning) {
        LOG_DEBUG(Log::instance()->getLogger(), name() << " is already running");
        return false;
    }

    LOG_DEBUG(Log::instance()->getLogger(), name() << " start");

    _pipePath = pipePath;
    _stopAsked = false;
    _isRunning = true;
    _thread = (std::make_unique<std::thread>(executeFunc, this));

    return true;
}

std::shared_ptr<AbstractCommChannel> PipeCommServer::nextPendingConnection() {
    return nullptr;
}

std::list<std::shared_ptr<AbstractCommChannel>> PipeCommServer::connections() {
    return std::list<std::shared_ptr<AbstractCommChannel>>();
}

void PipeCommServer::executeFunc(PipeCommServer *server) {
    server->execute();
    log4cplus::threadCleanup();
}

void PipeCommServer::execute() {
#ifdef _WIN32
    BOOL fConnected = FALSE;
    DWORD dwThreadId = 0;
    HANDLE hPipe = INVALID_HANDLE_VALUE;
    HANDLE hThread = NULL;

    for (;;) {
        hPipe = CreateNamedPipe(_pipePath.native().c_str(), // pipe name
                                PIPE_ACCESS_DUPLEX, // read/write access
                                PIPE_TYPE_MESSAGE | // message type pipe
                                        PIPE_READMODE_MESSAGE | // message-read mode
                                        PIPE_WAIT, // blocking mode
                                PIPE_UNLIMITED_INSTANCES, // max. instances
                                BUFSIZE, // output buffer size
                                BUFSIZE, // input buffer size
                                0, // client time-out
                                NULL); // default security attribute

        if (hPipe == INVALID_HANDLE_VALUE) {
            LOG_WARN(Log::instance()->getLogger(), "Error in CreateNamedPipe: err=" << GetLastError());
            return;
        }

        // Wait for the client to connect; if it succeeds,
        // the function returns a nonzero value. If the function
        // returns zero, GetLastError returns ERROR_PIPE_CONNECTED.

        fConnected = ConnectNamedPipe(hPipe, NULL) ? TRUE : (GetLastError() == ERROR_PIPE_CONNECTED);

        if (fConnected) {
            LOG_INFO(Log::instance()->getLogger(), "Client connected, creating a processing thread.");

            // Create a thread for this client.
            hThread = CreateThread(NULL, // no security attribute
                                   0, // default stack size
                                   InstanceThread, // thread proc
                                   (LPVOID) hPipe, // thread parameter
                                   0, // not suspended
                                   &dwThreadId); // returns thread ID

            if (hThread == NULL) {
                LOG_WARN(Log::instance()->getLogger(), "Error in CreateThread: err=" << GetLastError());
                return;
            } else
                CloseHandle(hThread);
        } else
            // The client could not connect, so close the pipe.
            CloseHandle(hPipe);
    }
#endif
}

void PipeCommServer::stop() {
    if (!_isRunning) {
        LOG_DEBUG(Log::instance()->getLogger(), name() << " is not running");
        return;
    }

    if (_stopAsked) {
        LOG_DEBUG(Log::instance()->getLogger(), name() << " is already stoping");
        return;
    }

    LOG_DEBUG(Log::instance()->getLogger(), name() << " stop");

    _stopAsked = true;
}

void PipeCommServer::waitForExit() {
    LOG_DEBUG(Log::instance()->getLogger(), name() << " wait for exit");

    if (_thread && _thread->joinable()) {
        _thread->join();
    }
}

} // namespace KDC
