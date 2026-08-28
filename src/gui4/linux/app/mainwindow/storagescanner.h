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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include "libcommon/utility/types.h"

#include <QString>

#include <cstdint>
#include <functional>
#include <optional>

namespace KDC {

struct StorageSnapshot {
        QString volumeName;
        SyncPath volumeRoot;
        uint64_t totalBytes{0};
        uint64_t availableBytes{0};
        uint64_t syncBytes{0};

        friend bool operator==(const StorageSnapshot &, const StorageSnapshot &) = default;
};

enum class StorageScanError : uint8_t {
    None = 0,
    Unavailable,
    AccessDenied,
    IoError,
    Canceled,
};

[[nodiscard]] constexpr const char *toString(const StorageScanError error) noexcept {
    switch (error) {
        case StorageScanError::None:
            return "None";
        case StorageScanError::Unavailable:
            return "Unavailable";
        case StorageScanError::AccessDenied:
            return "AccessDenied";
        case StorageScanError::IoError:
            return "IoError";
        case StorageScanError::Canceled:
            return "Canceled";
    }
    return "Unknown";
}

struct StorageScanResult {
        std::optional<StorageSnapshot> snapshot;
        StorageScanError error{StorageScanError::None};

        [[nodiscard]] bool succeeded() const { return snapshot.has_value(); }
};

/**
 * Computes the physical storage occupied by a synchronization root on Linux.
 *
 * The scanner never follows symbolic links and never descends onto a device different from the synchronization root's
 * device. Regular files contribute their allocated blocks rather than their logical size, so fully sparse files use zero
 * bytes. Subdirectories the user cannot traverse are skipped instead of failing the scan, so restricted subtrees are
 * under-counted. The call is synchronous and must run outside the GUI thread.
 */
class StorageScanner {
    public:
        using CancellationCheck = std::function<bool()>;

        [[nodiscard]] static StorageScanResult scan(const SyncPath &syncRoot, const CancellationCheck &isCanceled = {});
};

} // namespace KDC
