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
#include <stdio.h>
#include <tchar.h>
#include <strsafe.h>

#define CONNECTING_STATE 0
#define READING_STATE 1
#define WRITING_STATE 2
#define PIPE_TIMEOUT 5000
#define EVENT_WAIT_TIMEOUT 100
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
    return _channels.back();
}

std::list<std::shared_ptr<AbstractCommChannel>> PipeCommServer::connections() {
    std::list<std::shared_ptr<AbstractCommChannel>> channelList(_channels.begin(), _channels.end());
    return channelList;
}

void PipeCommServer::executeFunc(PipeCommServer *server) {
    server->execute();
    log4cplus::threadCleanup();
}

void PipeCommServer::execute() {
#ifdef _WIN32
    DWORD i, dwWait, cbRet, dwErr;
    BOOL fSuccess;
    HANDLE hEvents[INSTANCES];

    // The initial loop creates several instances of a named pipe along with an event object for each instance. An overlapped
    // ConnectNamedPipe operation is started for each instance.
    for (i = 0; i < INSTANCES; i++) {
        // Create an event object for this instance.
        hEvents[i] = CreateEvent(NULL, // default security attribute
                                 TRUE, // manual-reset event
                                 TRUE, // initial state = signaled
                                 NULL); // unnamed event object

        if (hEvents[i] == NULL) {
            LOG_WARN(Log::instance()->getLogger(), "Error in CreateEvent: err=" << GetLastError());
            _exitInfo = ExitInfo(ExitCode::SystemError, ExitCause::Unknown);
            return;
        }

        auto channel = std::make_shared<PipeCommChannel>();
        _channels.push_back(channel);
        channel->_oOverlap.hEvent = hEvents[i];
        channel->_oOverlap.Offset = 0;
        channel->_oOverlap.OffsetHigh = 0;

        channel->_hPipeInst = CreateNamedPipe(_pipePath.native().c_str(), // pipe name
                                              PIPE_ACCESS_DUPLEX | // read/write access
                                                      FILE_FLAG_OVERLAPPED, // overlapped mode
                                              PIPE_TYPE_MESSAGE | // message-type pipe
                                                      PIPE_READMODE_MESSAGE | // message-read mode
                                                      PIPE_WAIT, // blocking mode
                                              INSTANCES, // number of instances
                                              BUFSIZE * sizeof(TCHAR), // output buffer size
                                              BUFSIZE * sizeof(TCHAR), // input buffer size
                                              PIPE_TIMEOUT, // client time-out (ms)
                                              NULL); // default security attributes

        if (channel->_hPipeInst == INVALID_HANDLE_VALUE) {
            LOG_WARN(Log::instance()->getLogger(), "Error in CreateNamedPipe: err=" << GetLastError());
            _exitInfo = ExitInfo(ExitCode::SystemError, ExitCause::Unknown);
            return;
        }

        // Call the subroutine to connect to the new client
        channel->_fPendingIO = connectToNewClient(channel->_hPipeInst, &channel->_oOverlap);

        channel->_dwState = channel->_fPendingIO ? CONNECTING_STATE // still connecting
                                                 : READING_STATE; // ready to read
    }

    while (!_stopAsked) {
        // Wait for the event object to be signaled, indicating completion of an overlapped read, write, or connect operation.
        dwWait = WaitForMultipleObjects(INSTANCES, // number of event objects
                                        hEvents, // array of event objects
                                        FALSE, // does not wait for all
                                        EVENT_WAIT_TIMEOUT); // wait time (ms)

        if (dwWait == WAIT_TIMEOUT) {
            continue;
        } else if (dwWait == WAIT_FAILED) {
            LOG_WARN(Log::instance()->getLogger(), "Error in WaitForMultipleObjects: err=" << GetLastError());
            _exitInfo = ExitInfo(ExitCode::SystemError, ExitCause::Unknown);
            return;
        } else if (dwWait - WAIT_OBJECT_0 < 0 || dwWait - WAIT_OBJECT_0 > INSTANCES - 1) {
            LOG_WARN(Log::instance()->getLogger(), "Index out of range.");
            _exitInfo = ExitInfo(ExitCode::SystemError, ExitCause::Unknown);
            return;
        }

        // dwWait shows which pipe instance completed the operation.
        i = dwWait - WAIT_OBJECT_0;

        // Get the result if the operation was pending.
        if (_channels[i]->_fPendingIO) {
            fSuccess = GetOverlappedResult(_channels[i]->_hPipeInst, // handle to pipe
                                           &_channels[i]->_oOverlap, // OVERLAPPED structure
                                           &cbRet, // bytes transferred
                                           FALSE); // do not wait

            switch (_channels[i]->_dwState) {
                case CONNECTING_STATE:
                    // Pending connect operation
                    if (!fSuccess) {
                        LOG_WARN(Log::instance()->getLogger(), "Error in GetOverlappedResult: err=" << GetLastError());
                        _exitInfo = ExitInfo(ExitCode::SystemError, ExitCause::Unknown);
                        return;
                    }
                    _channels[i]->_dwState = READING_STATE;
                    break;
                case READING_STATE:
                    // Pending read operation
                    if (!fSuccess || cbRet == 0) {
                        disconnectAndReconnect(_channels[i]);
                        continue;
                    }
                    _channels[i]->_cbRead = cbRet;
                    _channels[i]->_dwState = WRITING_STATE;
                    break;
                case WRITING_STATE:
                    // Pending write operation
                    if (!fSuccess || cbRet != _channels[i]->_cbToWrite) {
                        disconnectAndReconnect(_channels[i]);
                        continue;
                    }
                    _channels[i]->_dwState = READING_STATE;
                    break;
                default: {
                    LOG_WARN(Log::instance()->getLogger(), "Invalid pipe state.");
                    _exitInfo = ExitInfo(ExitCode::SystemError, ExitCause::Unknown);
                    return;
                }
            }
        }

        // The pipe state determines which operation to do next.
        switch (_channels[i]->_dwState) {
            case READING_STATE:
                // The pipe instance is connected to the client and is ready to read a request from the client.
                fSuccess = ReadFile(_channels[i]->_hPipeInst, _channels[i]->_chRequest, BUFSIZE * sizeof(TCHAR),
                                    &_channels[i]->_cbRead, &_channels[i]->_oOverlap);

                // The read operation completed successfully.
                if (fSuccess && _channels[i]->_cbRead != 0) {
                    _channels[i]->_fPendingIO = FALSE;
                    _channels[i]->_dwState = WRITING_STATE;
                    continue;
                }

                // The read operation is still pending.
                dwErr = GetLastError();
                if (!fSuccess && (dwErr == ERROR_IO_PENDING)) {
                    _channels[i]->_fPendingIO = TRUE;
                    continue;
                }

                // An error occurred; disconnect from the client.
                disconnectAndReconnect(_channels[i]);
                break;
            case WRITING_STATE:
                // The request was successfully read from the client. Get the reply data and write it to the client.
                getAnswerToRequest(_channels[i]);

                fSuccess = WriteFile(_channels[i]->_hPipeInst, _channels[i]->_chReply, _channels[i]->_cbToWrite, &cbRet,
                                     &_channels[i]->_oOverlap);

                // The write operation completed successfully.
                if (fSuccess && cbRet == _channels[i]->_cbToWrite) {
                    _channels[i]->_fPendingIO = FALSE;
                    _channels[i]->_dwState = READING_STATE;
                    continue;
                }

                // The write operation is still pending.
                dwErr = GetLastError();
                if (!fSuccess && (dwErr == ERROR_IO_PENDING)) {
                    _channels[i]->_fPendingIO = TRUE;
                    continue;
                }

                // An error occurred; disconnect from the client.
                disconnectAndReconnect(_channels[i]);
                break;
            default: {
                LOG_WARN(Log::instance()->getLogger(), "Invalid pipe state.");
                _exitInfo = ExitInfo(ExitCode::SystemError, ExitCause::Unknown);
                return;
            }
        }
    }

    _exitInfo = ExitInfo(ExitCode::Ok, ExitCause::Unknown);
#endif
}

#ifdef _WIN32
// This function is called when an error occurs or when the client closes its handle to the pipe.
// Disconnect from this client, then call ConnectNamedPipe to wait for another client to connect.
void PipeCommServer::disconnectAndReconnect(std::shared_ptr<PipeCommChannel> channel) {
    // Disconnect the pipe instance.

    if (!DisconnectNamedPipe(channel->_hPipeInst)) {
        LOG_WARN(Log::instance()->getLogger(), "DisconnectNamedPipe failed: err=" << GetLastError());
    }

    // Call a subroutine to connect to the new client.
    channel->_fPendingIO = connectToNewClient(channel->_hPipeInst, &channel->_oOverlap);

    channel->_dwState = channel->_fPendingIO ? CONNECTING_STATE // still connecting
                                             : READING_STATE; // ready to read
}

// This function is called to start an overlapped connect operation.
// It returns true if an operation is pending or false if the connection has been completed.
bool PipeCommServer::connectToNewClient(HANDLE hPipe, LPOVERLAPPED lpo) {
    bool fPendingIO = false;

    // Start an overlapped connection for this pipe instance.
    BOOL fConnected = ConnectNamedPipe(hPipe, lpo);

    // Overlapped ConnectNamedPipe should return zero.
    if (fConnected) {
        LOG_WARN(Log::instance()->getLogger(), "Error in ConnectNamedPipe: err=" << GetLastError());
        return false;
    }

    switch (GetLastError()) {
        case ERROR_IO_PENDING:
            // The overlapped connection in progress.
            fPendingIO = true;
            break;
        case ERROR_PIPE_CONNECTED:
            // Client is already connected, so signal an event.
            if (SetEvent(lpo->hEvent)) break;
            [[fallthrough]];
        default: {
            // If an error occurs during the connect operation...
            LOG_WARN(Log::instance()->getLogger(), "Error in ConnectNamedPipe: err=" << GetLastError());
            return false;
        }
    }

    return fPendingIO;
}

void PipeCommServer::getAnswerToRequest(std::shared_ptr<PipeCommChannel> channel) {
    LOGW_INFO(Log::instance()->getLogger(), L"[" << channel->_hPipeInst << L"] " << channel->_chRequest);
    StringCchCopy(channel->_chReply, BUFSIZE, TEXT("Default answer from server"));
    channel->_cbToWrite = (lstrlen(channel->_chReply) + 1) * sizeof(TCHAR);
}

#endif

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
