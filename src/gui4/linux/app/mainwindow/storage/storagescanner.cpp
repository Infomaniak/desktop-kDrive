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

#include "storagescanner.h"

#include <QDir>
#include <QDirIterator>
#include <QLoggingCategory>
#include <QSet>
#include <QStorageInfo>

#include <algorithm>
#include <cerrno>
#include <limits>
#include <sys/stat.h>
#include <system_error>
#include <unistd.h>
#include <utility>
#include <vector>

using namespace Qt::StringLiterals;

namespace KDC {
namespace {

Q_LOGGING_CATEGORY(lcStorageScanner, "gui.v4.storagescanner", QtInfoMsg)

struct FileIdentity {
        dev_t device{0};
        ino_t inode{0};

        friend bool operator==(const FileIdentity &, const FileIdentity &) = default;

        [[nodiscard]] friend size_t qHash(const FileIdentity &identity, const size_t seed = 0) noexcept {
            return qHashMulti(seed, identity.device, identity.inode);
        }
};

[[nodiscard]] bool canceled(const StorageScanner::CancellationCheck &isCanceled) {
    return isCanceled && isCanceled();
}

[[nodiscard]] StorageScanError errorFromErrno(const int errorNumber) {
    switch (errorNumber) {
        case EACCES:
        case EPERM:
            return StorageScanError::AccessDenied;
        case ENOENT:
        case ENOTDIR:
        case ENODEV:
            return StorageScanError::Unavailable;
        default:
            return StorageScanError::IoError;
    }
}

void logFilesystemError(const char *operation, const SyncPath &path, const int errorNumber) {
    const auto errorMessage = std::error_code(errorNumber, std::generic_category()).message();
    qCWarning(lcStorageScanner) << operation << "| path:" << Path2QStr(path)
                                << "| error:" << QString::fromLocal8Bit(errorMessage.c_str()) << "| errno:" << errorNumber;
}

[[nodiscard]] QString titleWithDevice(const QString &title, const QString &device) {
    return device.isEmpty() || device == title ? title : u"%1 (%2)"_s.arg(title, device);
}

[[nodiscard]] QString volumeName(const QStorageInfo &storage) {
    const QString label = storage.name();
    const QString device = QDir::toNativeSeparators(QString::fromLocal8Bit(storage.device()));
    if (!label.isEmpty()) {
        return titleWithDevice(label, device);
    }

    if (storage.isRoot()) {
        return {};
    }

    return titleWithDevice(QDir::toNativeSeparators(storage.rootPath()), device);
}

void addAllocatedBytes(const struct stat &fileStat, uint64_t &total) {
    if (fileStat.st_blocks <= 0) {
        return;
    }

    constexpr uint64_t statBlockSize = 512;
    const auto blocks = static_cast<uint64_t>(fileStat.st_blocks);
    if (blocks > (std::numeric_limits<uint64_t>::max() - total) / statBlockSize) {
        total = std::numeric_limits<uint64_t>::max();
        return;
    }
    total += blocks * statBlockSize;
}

struct DirectoryScanState {
        dev_t rootDevice{0};
        uint64_t syncBytes{0};
        std::vector<SyncPath> pendingDirectories;
        QSet<FileIdentity> countedHardLinks;
};

/**
 * Accounts for one directory entry and queues same-device subdirectories.
 *
 * Entries that disappear during the scan are ignored, matching normal filesystem race handling. Links, other-device
 * entries, special files, and hard links already counted in this synchronization root contribute no bytes.
 */
[[nodiscard]] StorageScanError processEntry(const SyncPath &entryPath, DirectoryScanState &state) {
    struct stat entryStat{};
    if (lstat(entryPath.c_str(), &entryStat) != 0) {
        const int errorNumber = errno;
        if (errorNumber == ENOENT || errorNumber == ENOTDIR) {
            return StorageScanError::None;
        }
        logFilesystemError("Unable to inspect Storage entry", entryPath, errorNumber);
        return errorFromErrno(errorNumber);
    }

    if (S_ISLNK(entryStat.st_mode) || entryStat.st_dev != state.rootDevice) {
        return StorageScanError::None;
    }
    if (S_ISDIR(entryStat.st_mode)) {
        state.pendingDirectories.push_back(entryPath);
        return StorageScanError::None;
    }
    if (!S_ISREG(entryStat.st_mode)) {
        return StorageScanError::None;
    }

    if (entryStat.st_nlink > 1) {
        const FileIdentity identity{.device = entryStat.st_dev, .inode = entryStat.st_ino};
        if (state.countedHardLinks.contains(identity)) {
            return StorageScanError::None;
        }
        (void) state.countedHardLinks.insert(identity);
    }
    addAllocatedBytes(entryStat, state.syncBytes);
    return StorageScanError::None;
}

/** Iterates one readable directory without recursively following it. */
[[nodiscard]] StorageScanError scanDirectory(const SyncPath &directoryPath, DirectoryScanState &state,
                                             const StorageScanner::CancellationCheck &isCanceled) {
    QDirIterator iterator(Path2QStr(directoryPath), QDir::AllEntries | QDir::Hidden | QDir::System | QDir::NoDotAndDotDot,
                          QDirIterator::NoIteratorFlags);
    while (iterator.hasNext()) {
        if (canceled(isCanceled)) {
            return StorageScanError::Canceled;
        }
        if (const auto error = processEntry(QStr2Path(iterator.next()), state); error != StorageScanError::None) {
            return error;
        }
    }
    return StorageScanError::None;
}

/**
 * Walks the synchronization root using an explicit stack so other-device subtrees can be pruned before traversal.
 *
 * Subdirectories the user cannot traverse are logged and skipped: a single restricted directory must not void the whole
 * scan. The synchronization root itself remains a hard failure, as its content would otherwise be reported as empty.
 */
[[nodiscard]] StorageScanError calculateSyncBytes(const SyncPath &syncRoot, const dev_t rootDevice,
                                                  const StorageScanner::CancellationCheck &isCanceled, uint64_t &syncBytes) {
    if (access(syncRoot.c_str(), R_OK | X_OK) != 0) {
        const int errorNumber = errno;
        logFilesystemError("Unable to read Storage synchronization root", syncRoot, errorNumber);
        return errorFromErrno(errorNumber);
    }

    DirectoryScanState state;
    state.rootDevice = rootDevice;
    state.pendingDirectories.push_back(syncRoot);

    while (!state.pendingDirectories.empty()) {
        if (canceled(isCanceled)) {
            return StorageScanError::Canceled;
        }

        const SyncPath directoryPath = std::move(state.pendingDirectories.back());
        state.pendingDirectories.pop_back();
        if (access(directoryPath.c_str(), R_OK | X_OK) != 0) {
            const int errorNumber = errno;
            logFilesystemError("Skipping unreadable Storage directory", directoryPath, errorNumber);
            continue;
        }
        if (const auto error = scanDirectory(directoryPath, state, isCanceled); error != StorageScanError::None) {
            return error;
        }
    }

    if (canceled(isCanceled)) {
        return StorageScanError::Canceled;
    }
    syncBytes = state.syncBytes;
    return StorageScanError::None;
}

/** Locates another synchronization and measures it when it is on `volumeDevice`. Returns nothing once canceled. */
[[nodiscard]] std::optional<StorageScanUpdate> scanOtherSync(const StorageScanTarget &other, const dev_t volumeDevice,
                                                             const StorageScanner::CancellationCheck &isCanceled) {
    StorageScanUpdate update{
            .kind = StorageScanUpdate::Kind::OtherSyncMeasured, .syncDbId = other.syncDbId, .syncRoot = other.syncRoot};
    update.device = StorageScanner::deviceOf(other.syncRoot);
    update.onVolume = update.device.has_value() && *update.device == volumeDevice;
    if (!update.onVolume || !other.needsMeasure) {
        return update;
    }

    const auto measure = StorageScanner::measure(other.syncRoot, volumeDevice, isCanceled);
    if (measure.error == StorageScanError::Canceled) {
        return std::nullopt;
    }

    // An unreadable root has already been logged by the scanner: it stays unknown and out of the total.
    update.bytes = measure.bytes;
    update.error = measure.error;
    if (!measure.bytes.has_value()) {
        update.device.reset();
        update.onVolume = false;
    }
    return update;
}

} // namespace

StorageVolumeResult StorageScanner::readVolume(const SyncPath &syncRoot, const bool logFailures) {
    if (syncRoot.empty()) {
        if (logFailures) {
            qCWarning(lcStorageScanner) << "Storage synchronization root is empty";
        }
        return {.volume = std::nullopt, .error = StorageScanError::Unavailable};
    }

    struct stat rootStat{};
    if (stat(syncRoot.c_str(), &rootStat) != 0) {
        const int errorNumber = errno;
        if (logFailures) {
            logFilesystemError("Unable to inspect Storage synchronization root", syncRoot, errorNumber);
        }
        return {.volume = std::nullopt, .error = errorFromErrno(errorNumber)};
    }
    if (!S_ISDIR(rootStat.st_mode)) {
        if (logFailures) {
            qCWarning(lcStorageScanner) << "Storage synchronization root is not a directory | path:" << Path2QStr(syncRoot);
        }
        return {.volume = std::nullopt, .error = StorageScanError::Unavailable};
    }

    const QStorageInfo storage(Path2QStr(syncRoot));
    if (!storage.isValid() || !storage.isReady() || storage.bytesTotal() <= 0 || storage.bytesAvailable() < 0) {
        if (logFailures) {
            qCWarning(lcStorageScanner) << "Storage volume is unavailable | root:" << Path2QStr(syncRoot);
        }
        return {.volume = std::nullopt, .error = StorageScanError::Unavailable};
    }

    struct stat volumeRootStat{};
    const auto volumeRoot = QStr2Path(storage.rootPath());
    if (volumeRoot.empty()) {
        if (logFailures) {
            qCWarning(lcStorageScanner) << "Storage volume root is empty | synchronization root:" << Path2QStr(syncRoot);
        }
        return {.volume = std::nullopt, .error = StorageScanError::Unavailable};
    }
    if (stat(volumeRoot.c_str(), &volumeRootStat) != 0) {
        const int errorNumber = errno;
        if (logFailures) {
            logFilesystemError("Unable to inspect Storage volume root", volumeRoot, errorNumber);
        }
        return {.volume = std::nullopt, .error = errorFromErrno(errorNumber)};
    }
    if (volumeRootStat.st_dev != rootStat.st_dev) {
        if (logFailures) {
            qCWarning(lcStorageScanner) << "Storage volume device changed while resolving the synchronization root | root:"
                                        << Path2QStr(syncRoot) << "| volume root:" << Path2QStr(volumeRoot);
        }
        return {.volume = std::nullopt, .error = StorageScanError::Unavailable};
    }

    // Filesystems with dynamic allocation (btrfs, ZFS) and FUSE or network mounts may report more available bytes than
    // total bytes. Clamping keeps the used space computation from underflowing.
    const auto totalBytes = static_cast<uint64_t>(storage.bytesTotal());

    StorageVolume volume;
    volume.volumeName = volumeName(storage);
    volume.usesDefaultRootName = volume.volumeName.isEmpty() && storage.isRoot();
    volume.volumeRoot = volumeRoot;
    volume.device = rootStat.st_dev;
    volume.totalBytes = totalBytes;
    volume.availableBytes = std::min(static_cast<uint64_t>(storage.bytesAvailable()), totalBytes);
    return {.volume = std::move(volume), .error = StorageScanError::None};
}

StorageMeasureResult StorageScanner::measure(const SyncPath &syncRoot, const dev_t device, const CancellationCheck &isCanceled) {
    if (canceled(isCanceled)) {
        return {.bytes = std::nullopt, .error = StorageScanError::Canceled};
    }

    uint64_t syncBytes = 0;
    if (const auto error = calculateSyncBytes(syncRoot, device, isCanceled, syncBytes); error != StorageScanError::None) {
        return {.bytes = std::nullopt, .error = error};
    }

    qCDebug(lcStorageScanner) << "Storage measure completed | root:" << Path2QStr(syncRoot)
                              << "| bytes:" << static_cast<qulonglong>(syncBytes);
    return {.bytes = syncBytes, .error = StorageScanError::None};
}

std::optional<dev_t> StorageScanner::deviceOf(const SyncPath &path) {
    struct stat pathStat{};
    if (path.empty() || stat(path.c_str(), &pathStat) != 0) {
        return std::nullopt;
    }

    return pathStat.st_dev;
}

void StorageScanner::scanSyncs(QPromise<StorageScanUpdate> &promise, const StorageScanTarget &selected,
                               const std::vector<StorageScanTarget> &others) {
    using Kind = StorageScanUpdate::Kind;
    const auto isCanceled = [&promise] { return promise.isCanceled(); };

    const auto volumeResult = readVolume(selected.syncRoot);
    if (!volumeResult.volume) {
        (void) promise.addResult(StorageScanUpdate{.kind = Kind::Failed, .error = volumeResult.error});
        return;
    }

    const StorageVolume volume = *volumeResult.volume;
    (void) promise.addResult(StorageScanUpdate{.kind = Kind::Volume, .volume = volume});

    if (selected.needsMeasure) {
        const auto selectedMeasure = measure(selected.syncRoot, volume.device, isCanceled);
        if (selectedMeasure.error == StorageScanError::Canceled) {
            return;
        }
        if (!selectedMeasure.bytes.has_value()) {
            (void) promise.addResult(StorageScanUpdate{.kind = Kind::Failed, .error = selectedMeasure.error});
            return;
        }

        (void) promise.addResult(StorageScanUpdate{.kind = Kind::SelectedSyncMeasured,
                                                   .syncDbId = selected.syncDbId,
                                                   .syncRoot = selected.syncRoot,
                                                   .bytes = selectedMeasure.bytes,
                                                   .onVolume = true});
    }

    for (const auto &other: others) {
        if (isCanceled()) {
            return;
        }

        auto update = scanOtherSync(other, volume.device, isCanceled);
        if (!update.has_value()) {
            return;
        }
        (void) promise.addResult(std::move(*update));
    }

    (void) promise.addResult(StorageScanUpdate{.kind = Kind::OtherSyncsCompleted});
}

} // namespace KDC
