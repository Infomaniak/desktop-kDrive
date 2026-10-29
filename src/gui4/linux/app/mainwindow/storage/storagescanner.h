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
#include <sys/types.h>

namespace KDC {

/** Capacity of the local volume holding a synchronization root, read without traversing the tree. */
struct StorageVolume {
        QString volumeName;
        bool usesDefaultRootName{false};
        SyncPath volumeRoot;
        dev_t device{0};
        uint64_t totalBytes{0};
        uint64_t availableBytes{0};

        [[nodiscard]] uint64_t usedBytes() const { return totalBytes - availableBytes; }

        friend bool operator==(const StorageVolume &, const StorageVolume &) = default;
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

struct StorageVolumeResult {
        std::optional<StorageVolume> volume;
        StorageScanError error{StorageScanError::None};
};

struct StorageMeasureResult {
        std::optional<uint64_t> bytes;
        StorageScanError error{StorageScanError::None};
};

/**
 * Reads the local volume of a synchronization root and measures the physical storage occupied by synchronization roots
 * on Linux.
 *
 * The measure never follows symbolic links and never descends onto a device different from the measured root's device.
 * Regular files contribute their allocated blocks rather than their logical size, so fully sparse files use zero bytes.
 * Subdirectories the user cannot traverse are skipped instead of failing the measure, so restricted subtrees are
 * under-counted. Every call is synchronous and must run outside the GUI thread: even a volume read can block on an
 * unresponsive mount.
 */
class StorageScanner {
    public:
        using CancellationCheck = std::function<bool()>;

        /**
         * Reads the capacity of the volume holding `syncRoot`, which must be an existing directory. Periodic readers pass
         * `logFailures = false` so an unavailable volume does not repeat the same warning.
         */
        [[nodiscard]] static StorageVolumeResult readVolume(const SyncPath &syncRoot, bool logFailures = true);
        /** Sums the allocated bytes below `syncRoot`, restricted to `device`. */
        [[nodiscard]] static StorageMeasureResult measure(const SyncPath &syncRoot, dev_t device,
                                                          const CancellationCheck &isCanceled = {});
        /** Returns the device of `path`, or nothing if it cannot be inspected. */
        [[nodiscard]] static std::optional<dev_t> deviceOf(const SyncPath &path);
};

} // namespace KDC
