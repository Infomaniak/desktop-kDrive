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
#include "requests/parameterscache.h"
#include "libcommonserver/log/log.h"

#include <log4cplus/loggingmacros.h>

#if defined(KD_WINDOWS)
#include <stdio.h>
#include <tchar.h>
#include <strsafe.h>

#define PIPE_INSTANCES 10
#define PIPE_TIMEOUT 5000
#define EVENT_WAIT_TIMEOUT 100
#endif

namespace KDC {

uint64_t PipeCommChannel::readData(CommChar *data, uint64_t maxSize) {
    if (!_connected) return 0;

    auto size = _inBuffer.copy(data, maxSize);
    _inBuffer.erase(0, size);
    return size;
}

uint64_t PipeCommChannel::writeData(const CommChar *data, uint64_t size) {
    if (!_connected) return 0;

#if defined(KD_WINDOWS)
    if (ParametersCache::isExtendedLogEnabled()) {
        LOG_DEBUG(Log::instance()->getLogger(), "Try to write on inst:" << _instance);
    }
    DWORD bytesWritten = 0;
    auto index = toInt(Action::Write);
    BOOL fSuccess = WriteFile(_pipeInst, data, size * sizeof(TCHAR), &bytesWritten, &_overlap[index]);

    // The write operation completed successfully
    if (fSuccess && bytesWritten == size * sizeof(TCHAR)) {
        if (ParametersCache::isExtendedLogEnabled()) {
            LOG_DEBUG(Log::instance()->getLogger(), "Write done on inst:" << _instance);
        }
        _pendingIO[index] = FALSE;
        return size;
    }

    // The write operation is still pending
    DWORD dwErr = GetLastError();
    if (!fSuccess && (dwErr == ERROR_IO_PENDING)) {
        if (ParametersCache::isExtendedLogEnabled()) {
            LOG_DEBUG(Log::instance()->getLogger(), "Write pending on inst:" << _instance);
        }
        _size[index] = size * sizeof(TCHAR);
        _pendingIO[index] = TRUE;
        return size;
    }

    // An error occurred; disconnect from the client.
    LOG_WARN(Log::instance()->getLogger(), "Write error on inst:" << _instance);
    PipeCommServer::disconnectAndReconnect(std::static_pointer_cast<PipeCommChannel>(shared_from_this()));
    return 0;
#endif
}

uint64_t PipeCommChannel::bytesAvailable() const {
    return _inBuffer.size();
}


PipeCommServer::~PipeCommServer() = default;

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
    std::list<std::shared_ptr<AbstractCommChannel>> channelList;
    for (auto &channel: _channels) {
        if (channel->_connected) {
            channelList.push_back(channel);
        }
    }
    return channelList;
}

void PipeCommServer::executeFunc(PipeCommServer *server) {
    server->execute();
    log4cplus::threadCleanup();
}

void PipeCommServer::execute() {
#if defined(KD_WINDOWS)
    HANDLE events[PIPE_INSTANCES * toInt(PipeCommChannel::Action::EnumEnd)];

    for (DWORD inst = 0; inst < PIPE_INSTANCES; inst++) {
        // Creates an instance of a named pipe
        auto channel = makeCommChannel();
        _channels.push_back(channel);
        newConnectionCbk();

        for (auto action = 0; action < toInt(PipeCommChannel::Action::EnumEnd); action++) {
            // Create an event object
            auto index = inst * toInt(PipeCommChannel::Action::EnumEnd) + action;
            events[index] = CreateEvent(NULL, // default security attribute
                                        TRUE, // manual-reset event
                                        FALSE, // initial state = not signaled
                                        NULL); // unnamed event object

            if (events[index] == NULL) {
                LOG_WARN(Log::instance()->getLogger(), "Error in CreateEvent: err=" << GetLastError());
                _exitInfo = ExitInfo(ExitCode::SystemError, ExitCause::Unknown);
                return;
            }

            channel->_instance = inst;
            channel->_overlap[action].hEvent = events[index];
            channel->_overlap[action].Offset = 0;
            channel->_overlap[action].OffsetHigh = 0;
            channel->_size[action] = 0;
            channel->_pendingIO[action] = FALSE;
        }

        channel->_pipeInst = CreateNamedPipe(_pipePath.native().c_str(), // pipe name
                                             PIPE_ACCESS_DUPLEX | // read/write access
                                                     FILE_FLAG_OVERLAPPED, // overlapped mode
                                             PIPE_TYPE_BYTE | // message-type pipe
                                                     PIPE_READMODE_BYTE | // message-read mode
                                                     PIPE_WAIT, // blocking mode
                                             PIPE_INSTANCES, // number of instances
                                             BUFSIZE * sizeof(TCHAR), // output buffer size
                                             BUFSIZE * sizeof(TCHAR), // input buffer size
                                             PIPE_TIMEOUT, // client time-out (ms)
                                             NULL); // default security attributes

        if (channel->_pipeInst == INVALID_HANDLE_VALUE) {
            LOG_WARN(Log::instance()->getLogger(), "Error in CreateNamedPipe: err=" << GetLastError());
            _exitInfo = ExitInfo(ExitCode::SystemError, ExitCause::Unknown);
            return;
        }

        // Connect to the pipe
        if (ParametersCache::isExtendedLogEnabled()) {
            LOG_DEBUG(Log::instance()->getLogger(), "Try to connect on inst:" << inst);
        }
        auto connectIndex = toInt(PipeCommChannel::Action::Connect);
        channel->_pendingIO[connectIndex] = connectToPipe(channel->_pipeInst, &channel->_overlap[connectIndex]);
        if (ParametersCache::isExtendedLogEnabled()) {
            if (channel->_pendingIO[connectIndex]) {
                LOG_DEBUG(Log::instance()->getLogger(), "Connect pending on inst:" << inst);
            }
        }

        channel->_connected = channel->_pendingIO[connectIndex] ? FALSE // still connecting
                                                                : TRUE; // ready to read
    }

    while (!_stopAsked) {
        // Wait for the event object to be signaled, indicating completion of an overlapped read, write, or connect operation
        auto eventCount = PIPE_INSTANCES * toInt(PipeCommChannel::Action::EnumEnd);
        DWORD dwWait = WaitForMultipleObjects(eventCount, events, FALSE,
                                              EVENT_WAIT_TIMEOUT); // wait time (ms)

        DWORD index = 0;
        if (dwWait == WAIT_TIMEOUT) {
            continue;
        } else if (dwWait >= WAIT_OBJECT_0 && dwWait < WAIT_OBJECT_0 + eventCount) {
            // Event index
            index = dwWait - WAIT_OBJECT_0;
        } else {
            LOG_WARN(Log::instance()->getLogger(), "Error in WaitForSingleObject: err=" << GetLastError());
            _exitInfo = ExitInfo(ExitCode::SystemError, ExitCause::Unknown);
            return;
        }

        ResetEvent(events[index]);

        DWORD inst = index / toInt(PipeCommChannel::Action::EnumEnd);
        auto action = index % toInt(PipeCommChannel::Action::EnumEnd);
        if (ParametersCache::isExtendedLogEnabled()) {
            LOG_DEBUG(Log::instance()->getLogger(), "Event received for inst:" << inst << " action:" << action);
        }

        if (_channels[inst]->_pendingIO[action]) {
            DWORD size;
            BOOL fSuccess = GetOverlappedResult(_channels[inst]->_pipeInst, // handle to pipe
                                                &_channels[inst]->_overlap[action], // OVERLAPPED structure
                                                &size, // bytes transferred
                                                FALSE); // do not wait

            if (!fSuccess) {
                LOG_WARN(Log::instance()->getLogger(), "Error in GetOverlappedResult: err=" << GetLastError());
                disconnectAndReconnect(_channels[inst]);
                continue;
            }

            if (ParametersCache::isExtendedLogEnabled()) {
                LOG_DEBUG(Log::instance()->getLogger(), size << " Bytes transferred for inst:" << inst << " action:" << action);
            }
            _channels[inst]->_pendingIO[action] = FALSE;

            if (action == toInt(PipeCommChannel::Action::Connect)) {
                // Pending connect operation
                if (ParametersCache::isExtendedLogEnabled()) {
                    LOG_DEBUG(Log::instance()->getLogger(), "Pending connect done for inst:" << inst << " action:" << action);
                }
                _channels[inst]->_connected = TRUE;
            } else if (action == toInt(PipeCommChannel::Action::Read)) {
                if (size == 0) {
                    LOG_WARN(Log::instance()->getLogger(), "Pending read error for inst:" << inst << " action:" << action);
                    disconnectAndReconnect(_channels[inst]);
                    continue;
                }
                // Pending read operation
                if (ParametersCache::isExtendedLogEnabled()) {
                    LOG_DEBUG(Log::instance()->getLogger(), "Pending read done for inst:" << inst << " action:" << action);
                }
                _channels[inst]->_size[action] = size;
                _channels[inst]->_inBuffer += CommString(_channels[inst]->_readData);
                _channels[inst]->readyReadCbk();
            } else if (action == toInt(PipeCommChannel::Action::Write)) {
                if (size != _channels[inst]->_size[action]) {
                    LOG_WARN(Log::instance()->getLogger(), "Pending write error for inst:" << inst << " action:" << action);
                    disconnectAndReconnect(_channels[inst]);
                    continue;
                }
                // Pending write operation
                if (ParametersCache::isExtendedLogEnabled()) {
                    LOG_DEBUG(Log::instance()->getLogger(), "Pending write done for inst:" << inst << " action:" << action);
                }
            }
        }

        auto readIndex = toInt(PipeCommChannel::Action::Read);
        if (!_channels[inst]->_pendingIO[readIndex] && _channels[inst]->_connected) {
            // Read
            if (ParametersCache::isExtendedLogEnabled()) {
                LOG_DEBUG(Log::instance()->getLogger(), "Try to read on inst:" << inst);
            }
            memset(&_channels[inst]->_readData[0], 0, sizeof(_channels[inst]->_readData));
            BOOL fSuccess = ReadFile(_channels[inst]->_pipeInst, _channels[inst]->_readData, BUFSIZE * sizeof(TCHAR),
                                     &_channels[inst]->_size[readIndex], &_channels[inst]->_overlap[readIndex]);

            if (fSuccess && _channels[inst]->_size[readIndex] != 0) {
                // The read operation completed successfully
                if (ParametersCache::isExtendedLogEnabled()) {
                    LOG_DEBUG(Log::instance()->getLogger(), "Read done on inst:" << inst);
                }
                _channels[inst]->_pendingIO[readIndex] = FALSE;
                _channels[inst]->_inBuffer += CommString(_channels[inst]->_readData);
                SetEvent(events[index]);
                LOGW_DEBUG(Log::instance()->getLogger(), L"Read buffer:" << _channels[inst]->_inBuffer.c_str());
                _channels[inst]->readyReadCbk();
                continue;
            }

            DWORD dwErr = GetLastError();
            if (!fSuccess && (dwErr == ERROR_IO_PENDING)) {
                // The read operation is still pending
                if (ParametersCache::isExtendedLogEnabled()) {
                    LOG_DEBUG(Log::instance()->getLogger(), "Read pending on inst:" << inst);
                }
                _channels[inst]->_pendingIO[readIndex] = TRUE;
                continue;
            }

            // An error occurred, disconnect from the client
            LOG_WARN(Log::instance()->getLogger(), "Read error on inst:" << inst);
            disconnectAndReconnect(_channels[inst]);
            break;
        }
    }

    _exitInfo = ExitInfo(ExitCode::Ok, ExitCause::Unknown);
#endif
}

#if defined(KD_WINDOWS)
// This function is called when an error occurs or when the client closes its handle to the pipe
// Disconnect from this client, then call ConnectNamedPipe to wait for another client to connect
void PipeCommServer::disconnectAndReconnect(std::shared_ptr<PipeCommChannel> channel) {
    // Disconnect the pipe instance.
    if (!DisconnectNamedPipe(channel->_pipeInst)) {
        LOG_WARN(Log::instance()->getLogger(), "DisconnectNamedPipe failed: err=" << GetLastError());
    }

    // Call a subroutine to connect to the pipe
    auto connectIndex = toInt(PipeCommChannel::Action::Connect);
    channel->_pendingIO[connectIndex] = connectToPipe(channel->_pipeInst, &channel->_overlap[connectIndex]);

    channel->_connected = channel->_pendingIO[connectIndex] ? FALSE // still connecting
                                                            : TRUE; // ready to read
}

// This function is called to start an overlapped connect operation
// It returns true if an operation is pending or false if the connection has been completed
bool PipeCommServer::connectToPipe(HANDLE hPipe, LPOVERLAPPED lpo) {
    bool fPendingIO = false;

    // Start an overlapped connection for this pipe instance
    BOOL fConnected = ConnectNamedPipe(hPipe, lpo);

    // Overlapped ConnectNamedPipe should return zero
    if (fConnected) {
        LOG_WARN(Log::instance()->getLogger(), "Error in ConnectNamedPipe: err=" << GetLastError());
        return false;
    }

    switch (GetLastError()) {
        case ERROR_IO_PENDING:
            // The overlapped connection is in progress
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
