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

#include <thread>

namespace KDC {

class PipeCommChannel : public AbstractCommChannel {
    public:
        PipeCommChannel();
        ~PipeCommChannel();

        uint64_t readData(char *data, uint64_t maxlen) override;
        virtual uint64_t writeData(const char *data, uint64_t len) override;
        uint64_t bytesAvailable() const override;
        bool canReadLine() const override;
        std::string id() const override;

    private:
};

class PipeCommServer : public AbstractCommServer {
    public:
        PipeCommServer(const std::string &name);
        ~PipeCommServer();

        void close() override;
        bool listen(const KDC::SyncPath &pipePath) override;
        std::shared_ptr<KDC::AbstractCommChannel> nextPendingConnection() override;
        std::list<std::shared_ptr<KDC::AbstractCommChannel>> connections() override;

        static bool removeServer(const KDC::SyncPath &path) {
            return true;
        }

    private:
        SyncPath _pipePath;
        std::unique_ptr<std::thread> _thread;
        bool _isRunning{false};
        bool _stopAsked{false};

        static void executeFunc(PipeCommServer *server);

        void execute();
        void stop();
        void waitForExit();
};

} // namespace KDC
