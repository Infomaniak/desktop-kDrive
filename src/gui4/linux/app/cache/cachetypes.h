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

#pragma once

#include "libcommon/utility/types.h"
#include "libcommon/info/accountinfo.h"
#include "libcommon/info/driveavailableinfo.h"
#include "libcommon/info/driveinfo.h"
#include "libcommon/info/errorinfo.h"
#include "libcommon/info/syncinfo.h"
#include "libcommon/info/userinfo.h"

#include <QString>

#include <cstddef>
#include <functional>
#include <optional>
#include <vector>

namespace KDC {

struct AvailableDriveKey {
        UserDbId userDbId{0};
        AccountId accountId{0};
        DriveId driveId{0};

        friend bool operator==(const AvailableDriveKey &lhs, const AvailableDriveKey &rhs) {
            return lhs.userDbId == rhs.userDbId && lhs.accountId == rhs.accountId && lhs.driveId == rhs.driveId;
        }
};

struct SyncContext {
        UserInfo user;
        AccountInfo account;
        DriveInfo drive;
        SyncInfo sync;
        std::vector<ErrorInfo> errors;
        std::optional<ErrorInfo> latestError;

        friend bool operator==(const SyncContext &lhs, const SyncContext &rhs) = default;
};

struct DriveContext {
        UserInfo user;
        AccountInfo account;
        DriveInfo drive;
        std::vector<SyncInfo> syncs;

        friend bool operator==(const DriveContext &lhs, const DriveContext &rhs) = default;
};

struct AvailableDriveContext {
        UserInfo user;
        std::optional<AccountInfo> account;
        DriveAvailableInfo availableDrive;
        bool alreadyConfigured{false};
        std::optional<DriveInfo> configuredDrive;

        friend bool operator==(const AvailableDriveContext &lhs, const AvailableDriveContext &rhs) = default;
};

struct PendingSyncConfig {
        QString localPath;
        QString targetPath;
        QString targetNodeId;
        bool supportVfs{false};
        VirtualFileMode virtualFileMode{VirtualFileMode::Off};
};

} // namespace KDC

template<>
struct std::hash<KDC::AvailableDriveKey> {
        std::size_t operator()(const KDC::AvailableDriveKey &key) const noexcept {
            const auto hashUser = std::hash<KDC::UserDbId>{}(key.userDbId);
            const auto hashAccount = std::hash<KDC::AccountId>{}(key.accountId);
            const auto hashDrive = std::hash<KDC::DriveId>{}(key.driveId);
            return hashUser ^ (hashAccount << 1U) ^ (hashDrive << 2U);
        }
};
