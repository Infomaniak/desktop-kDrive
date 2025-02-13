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
#include "server/updater/abstractupdater.h"

namespace KDC {

class MockUpdater : public AbstractUpdater {
    public:
        MockUpdater(const std::shared_ptr<UpdateChecker> &customUpdateChecker = std::make_shared<UpdateChecker>()) :
            AbstractUpdater(customUpdateChecker) {}

        void startInstaller() override {
            if (_startInstallerMock) _startInstallerMock();
        }
        void onUpdateFound() override {
            if (_quitCallback) _quitCallback();
        }

        void setStartInstallerMock(std::function<void()> startInstallerMock) { _startInstallerMock = startInstallerMock; }
        void setQuitCallback(const std::function<void()>& quitCallback) { _quitCallback = quitCallback; }

    private:
        std::function<void()> _startInstallerMock;
        std::function<void()> _quitCallback;
};
} // namespace KDC
