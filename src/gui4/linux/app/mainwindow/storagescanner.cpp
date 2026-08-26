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

[[nodiscard]] bool cancelled(const StorageScanner::CancellationCheck &isCancelled) {
    return isCancelled && isCancelled();
}

[[nodiscard]] StorageScanResult failure(const StorageScanError error) {
    return {.snapshot = std::nullopt, .error = error};
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

[[nodiscard]] QString volumeName(const QStorageInfo &storage) {
    QString label = storage.displayName();
    if (label.isEmpty()) {
        label = storage.name();
    }

    const QString device = QDir::toNativeSeparators(QString::fromLocal8Bit(storage.device()));
    if (label.isEmpty()) {
        return device.isEmpty() ? QDir::toNativeSeparators(storage.rootPath()) : device;
    }
    return device.isEmpty() || device == label ? label : u"%1 (%2)"_s.arg(label, device);
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
                                             const StorageScanner::CancellationCheck &isCancelled) {
    QDirIterator iterator(Path2QStr(directoryPath), QDir::AllEntries | QDir::Hidden | QDir::System | QDir::NoDotAndDotDot,
                          QDirIterator::NoIteratorFlags);
    while (iterator.hasNext()) {
        if (cancelled(isCancelled)) {
            return StorageScanError::Cancelled;
        }
        if (const auto error = processEntry(QStr2Path(iterator.next()), state); error != StorageScanError::None) {
            return error;
        }
    }
    return StorageScanError::None;
}

/** Walks the synchronization root using an explicit stack so other-device subtrees can be pruned before traversal. */
[[nodiscard]] StorageScanError calculateSyncBytes(const SyncPath &syncRoot, const dev_t rootDevice,
                                                  const StorageScanner::CancellationCheck &isCancelled, uint64_t &syncBytes) {
    DirectoryScanState state;
    state.rootDevice = rootDevice;
    state.pendingDirectories.push_back(syncRoot);

    while (!state.pendingDirectories.empty()) {
        if (cancelled(isCancelled)) {
            return StorageScanError::Cancelled;
        }

        const SyncPath directoryPath = std::move(state.pendingDirectories.back());
        state.pendingDirectories.pop_back();
        if (access(directoryPath.c_str(), R_OK | X_OK) != 0) {
            const int errorNumber = errno;
            logFilesystemError("Unable to read Storage directory", directoryPath, errorNumber);
            return errorFromErrno(errorNumber);
        }
        if (const auto error = scanDirectory(directoryPath, state, isCancelled); error != StorageScanError::None) {
            return error;
        }
    }

    if (cancelled(isCancelled)) {
        return StorageScanError::Cancelled;
    }
    syncBytes = state.syncBytes;
    return StorageScanError::None;
}

} // namespace

StorageScanResult StorageScanner::scan(const SyncPath &syncRoot, const CancellationCheck &isCancelled) {
    if (cancelled(isCancelled)) {
        return failure(StorageScanError::Cancelled);
    }
    if (syncRoot.empty()) {
        qCWarning(lcStorageScanner) << "Storage synchronization root is empty";
        return failure(StorageScanError::Unavailable);
    }

    struct stat rootStat{};
    if (stat(syncRoot.c_str(), &rootStat) != 0) {
        const int errorNumber = errno;
        logFilesystemError("Unable to inspect Storage synchronization root", syncRoot, errorNumber);
        return failure(errorFromErrno(errorNumber));
    }
    if (!S_ISDIR(rootStat.st_mode)) {
        qCWarning(lcStorageScanner) << "Storage synchronization root is not a directory | path:" << Path2QStr(syncRoot);
        return failure(StorageScanError::Unavailable);
    }

    QStorageInfo storage(Path2QStr(syncRoot));
    storage.refresh();
    if (!storage.isValid() || !storage.isReady() || storage.bytesTotal() <= 0 || storage.bytesAvailable() < 0) {
        qCWarning(lcStorageScanner) << "Storage volume is unavailable | root:" << Path2QStr(syncRoot);
        return failure(StorageScanError::Unavailable);
    }

    struct stat volumeRootStat{};
    const auto volumeRoot = QStr2Path(storage.rootPath());
    if (volumeRoot.empty()) {
        qCWarning(lcStorageScanner) << "Storage volume root is empty | synchronization root:" << Path2QStr(syncRoot);
        return failure(StorageScanError::Unavailable);
    }
    if (stat(volumeRoot.c_str(), &volumeRootStat) != 0) {
        const int errorNumber = errno;
        logFilesystemError("Unable to inspect Storage volume root", volumeRoot, errorNumber);
        return failure(errorFromErrno(errorNumber));
    }
    if (volumeRootStat.st_dev != rootStat.st_dev) {
        qCWarning(lcStorageScanner) << "Storage volume device changed while resolving the synchronization root | root:"
                                    << Path2QStr(syncRoot) << "| volume root:" << Path2QStr(volumeRoot);
        return failure(StorageScanError::Unavailable);
    }

    uint64_t syncBytes = 0;
    if (const auto error = calculateSyncBytes(syncRoot, rootStat.st_dev, isCancelled, syncBytes);
        error != StorageScanError::None) {
        return failure(error);
    }

    const QByteArray expectedDevice = storage.device();
    storage.refresh();
    if (!storage.isValid() || !storage.isReady() || storage.device() != expectedDevice || storage.bytesTotal() <= 0 ||
        storage.bytesAvailable() < 0) {
        qCWarning(lcStorageScanner) << "Storage volume became unavailable during scan | root:" << Path2QStr(syncRoot);
        return failure(StorageScanError::Unavailable);
    }

    const auto totalBytes = static_cast<uint64_t>(storage.bytesTotal());
    const auto availableBytes = std::min(static_cast<uint64_t>(storage.bytesAvailable()), totalBytes);
    const auto usedBytes = totalBytes - availableBytes;

    StorageSnapshot snapshot;
    snapshot.volumeName = volumeName(storage);
    snapshot.volumeRoot = volumeRoot;
    snapshot.totalBytes = totalBytes;
    snapshot.availableBytes = availableBytes;
    snapshot.syncBytes = std::min(syncBytes, usedBytes);
    qCDebug(lcStorageScanner) << "Storage scan completed | root:" << Path2QStr(syncRoot)
                              << "| total bytes:" << static_cast<qulonglong>(snapshot.totalBytes)
                              << "| available bytes:" << static_cast<qulonglong>(snapshot.availableBytes)
                              << "| synchronization bytes:" << static_cast<qulonglong>(snapshot.syncBytes);
    return {.snapshot = std::move(snapshot), .error = StorageScanError::None};
}

} // namespace KDC
