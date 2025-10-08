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

#include "server/comm/guijobs/abstractguijob.h"
#include "libcommon/info/userinfo.h"

namespace KDC {

class SignalUserAddedJob : public AbstractGuiJob {
    public:
        SignalUserAddedJob(std::shared_ptr<CommManager> commManager, const std::shared_ptr<AbstractCommChannel> channel,
                           const UserInfo &userInfo);

    private:
        // Output parameters
        UserInfo _userInfo;

        ExitInfo deserializeInputParms() override { return ExitCode::Ok; }
        ExitInfo serializeOutputParms(bool hasError = false) override;
        ExitInfo process() override { return ExitCode::Ok; }
};

} // namespace KDC
