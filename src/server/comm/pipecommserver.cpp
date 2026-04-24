/*
 * Infomaniak kDrive - Desktop
 * Copyright (C) 2023-2026 Infomaniak Network SA
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
#include "libcommon/theme/theme.h"
#include "libcommon/utility/utility.h"
#include "libcommonserver/utility/utility.h"
#include "libcommonserver/log/log.h"

#include <log4cplus/loggingmacros.h>

#if defined(KD_WINDOWS)
#include <stdio.h>
#include <tchar.h>
#include <strsafe.h>
#endif

namespace KDC {

#if defined(KD_WINDOWS)
constexpr int pipeInstances = 10;
constexpr int pipeTimeOut = 5000; // ms
constexpr int eventWaitTimeout = 100; // ms

constexpr auto connectIndex = toInt(PipeCommChannel::Action::Connect);
constexpr auto readIndex = toInt(PipeCommChannel::Action::Read);
constexpr auto writeIndex = toInt(PipeCommChannel::Action::Write);

int PipeCommServer::nbrOfpipeInstances() {
    return pipeInstances;
}
#endif

uint64_t PipeCommChannel::readData(CommChar *data, uint64_t maxSize) {
    if (!_connected) return 0;

    const auto size = _inBuffer.copy(data, maxSize);
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
    const BOOL fSuccess = WriteFile(_pipeInst, data, size * sizeof(TCHAR), &bytesWritten, &_overlap[writeIndex]);

    // The write operation completed successfully
    if (fSuccess && bytesWritten == size * sizeof(TCHAR)) {
        if (ParametersCache::isExtendedLogEnabled()) {
            LOG_DEBUG(Log::instance()->getLogger(), "Write done on inst:" << _instance);
        }
        _pendingIO[writeIndex] = FALSE;
        return size;
    }

    // The write operation is still pending
    const auto dwErr = GetLastError();
    if (!fSuccess && (dwErr == ERROR_IO_PENDING)) {
        if (ParametersCache::isExtendedLogEnabled()) {
            LOG_DEBUG(Log::instance()->getLogger(), "Write pending on inst:" << _instance);
        }
        _size[writeIndex] = size * sizeof(TCHAR);
        _pendingIO[writeIndex] = TRUE;
        return size;
    }

    // An error occurred; disconnect from the client.
    if (dwErr == ERROR_BROKEN_PIPE) {
        LOG_DEBUG(Log::instance()->getLogger(), "Connexion closed for inst:" << _instance);
    } else {
        LOG_WARN(Log::instance()->getLogger(), "Write error on inst:" << _instance << " err=" << dwErr);
    }

    PipeCommServer::disconnectAndReconnect(std::static_pointer_cast<PipeCommChannel>(shared_from_this()));
    return 0;
#endif
}

uint64_t PipeCommChannel::bytesAvailable() const {
    return _inBuffer.size();
}

PipeCommServer::PipeCommServer(const std::string &name) :
    AbstractCommServer(name) {
    _pipePath = pipePath();
}

PipeCommServer::~PipeCommServer() {
    if (_isRunning) {
        stop();
    }
    waitForExit();
    log4cplus::threadCleanup();
}

void PipeCommServer::close() {
    if (_isRunning) {
        stop();
    }
    waitForExit();
    _isRunning = false;
}

bool PipeCommServer::listen() {
    if (_isRunning) {
        LOG_DEBUG(Log::instance()->getLogger(), name() << " is already running");
        return false;
    }

    if (_pipePath.empty()) {
        LOG_ERROR(Log::instance()->getLogger(), name() << " pipe path is not set");
        return false;
    }

    LOGW_INFO(Log::instance()->getLogger(),
              L"Starting " << CommonUtility::s2ws(name()) << L": " << Utility::formatSyncPath(_pipePath));

    _stopAsked = false;
    _isRunning = true;
    _thread = (std::make_unique<std::thread>(executeFunc, this));

    return true;
}

std::shared_ptr<AbstractCommChannel> PipeCommServer::nextPendingConnection() {
    const std::scoped_lock lock(_channelsMutex);
    return _channels.back();
}

std::list<std::shared_ptr<AbstractCommChannel>> PipeCommServer::connections() {
    const std::scoped_lock lock(_channelsMutex);
    std::list<std::shared_ptr<AbstractCommChannel>> channelList;
    for (auto channel: _channels) {
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
    HANDLE events[pipeInstances * toInt(PipeCommChannel::Action::EnumEnd)] = {nullptr};

    for (DWORD inst = 0; inst < pipeInstances; inst++) {
        // Creates an instance of a named pipe
        const std::scoped_lock lock(_channelsMutex);
        auto channel = makeCommChannel();
        _channels.push_back(channel);
        channel->_instance = inst;

        for (auto action = 0; action < toInt(PipeCommChannel::Action::EnumEnd); action++) {
            // Create an event object
            const auto index = inst * toInt(PipeCommChannel::Action::EnumEnd) + action;
            events[index] = CreateEvent(nullptr, // default security attribute
                                        TRUE, // manual-reset event
                                        FALSE, // initial state = not signaled
                                        nullptr); // unnamed event object

            if (events[index] == nullptr) {
                LOG_WARN(Log::instance()->getLogger(), "Error in CreateEvent: err=" << GetLastError());
                _exitInfo = ExitInfo(ExitCode::SystemError, ExitCause::Unknown);
                return;
            }

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
                                             pipeInstances, // number of instances
                                             BUFSIZE * sizeof(TCHAR), // output buffer size
                                             BUFSIZE * sizeof(TCHAR), // input buffer size
                                             pipeTimeOut, // client time-out (ms)
                                             nullptr); // default security attributes

        if (channel->_pipeInst == INVALID_HANDLE_VALUE) {
            LOG_WARN(Log::instance()->getLogger(), "Error in CreateNamedPipe: err=" << GetLastError());
            _exitInfo = ExitInfo(ExitCode::SystemError, ExitCause::Unknown);
            return;
        }

        // Connect to the pipe
        if (ParametersCache::isExtendedLogEnabled()) {
            LOG_DEBUG(Log::instance()->getLogger(), "Try to connect on inst:" << inst);
        }
        channel->_pendingIO[connectIndex] = connectToPipe(channel->_pipeInst, &channel->_overlap[connectIndex]);
        if (ParametersCache::isExtendedLogEnabled()) {
            if (channel->_pendingIO[connectIndex]) {
                LOG_DEBUG(Log::instance()->getLogger(), "Connect pending on inst:" << inst);
            }
        }

        channel->_connected = channel->_pendingIO[connectIndex] ? FALSE // still connecting
                                                                : TRUE; // ready to read

        channel->setLostConnectionCbk([this](std::shared_ptr<AbstractCommChannel> ch) { lostConnectionCbk(ch); });
        newConnectionCbk();
    }

    _isListening = true;
    while (!_stopAsked) {
        // Wait for the event object to be signaled, indicating completion of an overlapped read, write, or connect operation
        const auto eventCount = pipeInstances * toInt(PipeCommChannel::Action::EnumEnd);
        const auto dwWait = WaitForMultipleObjects(eventCount, events, FALSE,
                                                   eventWaitTimeout); // wait time (ms)

        DWORD index = 0;
        if (dwWait == WAIT_TIMEOUT) {
            continue;
        } else if (dwWait >= WAIT_OBJECT_0 && dwWait < WAIT_OBJECT_0 + eventCount) {
            // Event index
            index = dwWait - WAIT_OBJECT_0;
        } else {
            LOG_WARN(Log::instance()->getLogger(), "Error in WaitForSingleObject: err=" << GetLastError());
            _exitInfo = ExitInfo(ExitCode::SystemError, ExitCause::Unknown);
            break;
        }

        ResetEvent(events[index]);

        const DWORD inst = index / toInt(PipeCommChannel::Action::EnumEnd);
        const auto action = index % toInt(PipeCommChannel::Action::EnumEnd);
        if (ParametersCache::isExtendedLogEnabled()) {
            LOG_DEBUG(Log::instance()->getLogger(), "Event received for inst:" << inst << " action:" << action);
        }

        const std::scoped_lock lock(_channelsMutex);
        if (_channels[inst]->_pendingIO[action]) {
            DWORD size;
            const auto fSuccess = GetOverlappedResult(_channels[inst]->_pipeInst, // handle to pipe
                                                      &_channels[inst]->_overlap[action], // OVERLAPPED structure
                                                      &size, // bytes transferred
                                                      FALSE); // do not wait

            if (!fSuccess) {
                const auto dwErr = GetLastError();
                if (dwErr == ERROR_BROKEN_PIPE) {
                    LOG_DEBUG(Log::instance()->getLogger(), "Connexion closed for inst:" << inst);
                } else {
                    LOG_WARN(Log::instance()->getLogger(), "Error in GetOverlappedResult on inst:" << inst << " err=" << dwErr);
                }

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
                _channels[inst]->_pendingIO[readIndex] = FALSE;
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

        if (!_channels[inst]->_pendingIO[readIndex] && _channels[inst]->_connected) {
            // Read
            if (ParametersCache::isExtendedLogEnabled()) {
                LOG_DEBUG(Log::instance()->getLogger(), "Try to read on inst:" << inst);
            }
            memset(&_channels[inst]->_readData[0], 0, sizeof(_channels[inst]->_readData));
            const auto fSuccess = ReadFile(_channels[inst]->_pipeInst, _channels[inst]->_readData, BUFSIZE * sizeof(TCHAR),
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

            const auto dwErr = GetLastError();
            if (!fSuccess && (dwErr == ERROR_IO_PENDING)) {
                // The read operation is still pending
                if (ParametersCache::isExtendedLogEnabled()) {
                    LOG_DEBUG(Log::instance()->getLogger(), "Read pending on inst:" << inst);
                }
                _channels[inst]->_pendingIO[readIndex] = TRUE;
                continue;
            }

            // An error occurred, disconnect from the client
            if (dwErr == ERROR_BROKEN_PIPE) {
                LOG_DEBUG(Log::instance()->getLogger(), "Connexion closed for inst:" << inst);
            } else {
                LOG_WARN(Log::instance()->getLogger(), "Read error on inst:" << inst << " err=" << dwErr);
            }

            disconnectAndReconnect(_channels[inst]);
        }
    }
    _isListening = false;

    for (auto &channel: _channels) {
        (void) DisconnectNamedPipe(channel->_pipeInst);
        (void) CloseHandle(channel->_pipeInst); // Needed to be able to reuse the pipe (avoid ERROR_PIPE_BUSY)
    }
    _channels.clear();

    _exitInfo = ExitInfo(ExitCode::Ok, ExitCause::Unknown);
#endif
}

#if defined(KD_WINDOWS)
// This function is called when an error occurs or when the client closes its handle to the pipe
// Disconnect from this client, then call ConnectNamedPipe to wait for another client to connect
void PipeCommServer::disconnectAndReconnect(std::shared_ptr<PipeCommChannel> channel) {
    channel->lostConnectionCbk();

    // Disconnect the pipe instance.
    if (ParametersCache::isExtendedLogEnabled()) {
        LOG_DEBUG(Log::instance()->getLogger(), "Disconnect pipe inst:" << channel->_instance);
    }
    if (!DisconnectNamedPipe(channel->_pipeInst)) {
        LOG_WARN(Log::instance()->getLogger(), "DisconnectNamedPipe failed: err=" << GetLastError());
    }

    // Connect to the pipe
    if (ParametersCache::isExtendedLogEnabled()) {
        LOG_DEBUG(Log::instance()->getLogger(), "Try to connect on inst:" << channel->_instance);
    }
    channel->_pendingIO[connectIndex] = connectToPipe(channel->_pipeInst, &channel->_overlap[connectIndex]);
    if (ParametersCache::isExtendedLogEnabled()) {
        if (channel->_pendingIO[connectIndex]) {
            LOG_DEBUG(Log::instance()->getLogger(), "Connect pending on inst:" << channel->_instance);
        }
    }

    channel->_connected = channel->_pendingIO[connectIndex] ? FALSE // still connecting
                                                            : TRUE; // ready to read
}

// This function is called to start an overlapped connect operation
// It returns true if an operation is pending or false if the connection has been completed
bool PipeCommServer::connectToPipe(HANDLE hPipe, LPOVERLAPPED lpo) {
    bool fPendingIO = false;

    // Start an overlapped connection for this pipe instance
    const BOOL fConnected = ConnectNamedPipe(hPipe, lpo);

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

SyncPath PipeCommServer::pipePath() {
    // Get pipe file path
    std::string name(Theme::instance()->appName());
    name.append("-");
    name.append(Utility::userName());

    const SyncPath pipePath = SyncPath(R"(\\.\pipe\)") / Str2SyncName(name);

    return pipePath;
}

} // namespace KDC
