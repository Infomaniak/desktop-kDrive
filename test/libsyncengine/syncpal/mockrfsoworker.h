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

#include "testincludes.h"
#include "utility/types.h"
#include "update_detection/file_system_observer/remotefilesystemobserverworker.h"

namespace KDC {

class MockRemoteFileSystemObserverWorker : public RemoteFileSystemObserverWorker {
    public:
        using RemoteFileSystemObserverWorker::RemoteFileSystemObserverWorker;
        void setNetworkAvailability(bool networkAvailable) { _networkAvailable = networkAvailable; }
        void simulateFSEvent() { _syncPal->snapshot(ReplicaSide::Local)->startUpdate(); }
        void setLongPollDuration(std::chrono::steady_clock::duration duration) { _longPollDuration = duration; }

    private:
        void execute() override { RemoteFileSystemObserverWorker::execute(); }


        bool _networkAvailable{true};
        std::chrono::steady_clock::duration _longPollDuration = std::chrono::seconds(50);
        ExitCode sendLongPoll(bool& changes) override {
            using namespace std::chrono;
            changes = false;
            if (!_networkAvailable) {
                return ExitCode::NetworkError;
            }

            const auto start = steady_clock::now();
            while (_networkAvailable && !stopAsked() && start + _longPollDuration < steady_clock::now()) {
                Utility::msleep(100);
            }
            if (!_networkAvailable) {
                return ExitCode::NetworkError;
            }
            return ExitCode::Ok;
        }

        ExitCode generateInitialSnapshot() override {
            _snapshot->init();
            _updating = true;

            if (_networkAvailable) {
                _snapshot->setValid(true);
                _updating = false;
                return ExitCode::Ok;
            } else {
                invalidateSnapshot();
                _updating = false;
                return ExitCode::NetworkError;
            }
        }
};
} // namespace KDC
