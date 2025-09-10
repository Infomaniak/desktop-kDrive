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

#include "abstractcommserver.h"
#include "libcommon/utility/types.h"

#include <thread>

#if defined(KD_WINDOWS)
#include <windows.h>
#define BUFSIZE 4096
#endif

namespace KDC {

class PipeCommServer;

class PipeCommChannel : public AbstractCommChannel {
    public:
        enum class Action {
            Connect = 0,
            Read,
            Write,
            EnumEnd
        };

        PipeCommChannel() = default;
        ~PipeCommChannel() = default;

        uint64_t bytesAvailable() const override;
        void close() override {};

    protected:
        uint64_t readData(CommChar *data, uint64_t maxSize) final;
        uint64_t writeData(const CommChar *data, uint64_t size) final;

    private:
        CommString _inBuffer;

#if defined(KD_WINDOWS)
        short _instance = 0;
        OVERLAPPED _overlap[toInt(Action::EnumEnd)];
        HANDLE _pipeInst;
        BOOL _connected = FALSE;
        DWORD _size[toInt(Action::EnumEnd)];
        BOOL _pendingIO[toInt(Action::EnumEnd)];
        TCHAR _readData[BUFSIZE];
#endif


        friend class PipeCommServer;
};

class PipeCommServer : public AbstractCommServer {
    public:
        PipeCommServer(const std::string &name);
        ~PipeCommServer();

        void close() override;
        bool listen(const SyncPath &pipePath) override;
        std::shared_ptr<AbstractCommChannel> nextPendingConnection() override;
        std::list<std::shared_ptr<AbstractCommChannel>> connections() override;

        static bool removeServer(const SyncPath &path) { return true; }

#if defined(KD_WINDOWS)
        static void disconnectAndReconnect(std::shared_ptr<PipeCommChannel> channel);
#endif

    protected:
        virtual std::shared_ptr<PipeCommChannel> makeCommChannel() const = 0;

    private:
        SyncPath _pipePath;
        std::unique_ptr<std::thread> _thread;
        bool _isRunning{false};
        bool _stopAsked{false};
        ExitInfo _exitInfo;

        void execute();
        void stop();
        void waitForExit();

        static void executeFunc(PipeCommServer *server);

#if defined(KD_WINDOWS)
        std::vector<std::shared_ptr<PipeCommChannel>> _channels;

        static bool connectToPipe(HANDLE hPipe, LPOVERLAPPED lpo);
#endif
};

} // namespace KDC
