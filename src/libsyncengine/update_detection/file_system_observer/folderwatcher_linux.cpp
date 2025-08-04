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

#include "folderwatcher_linux.h"
#include "localfilesystemobserverworker.h"
#include "libcommon/utility/types.h"
#include "libcommonserver/io/iohelper.h"
#include "libcommonserver/utility/utility.h"
#include "requests/parameterscache.h"

#include <sys/inotify.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <string.h>

namespace KDC {

#define EVENT_SIZE (sizeof(struct inotify_event))
#define BUF_LEN (1024 * (EVENT_SIZE + 16))
#define SLEEP_TIME 100 // 100ms

FolderWatcher_linux::FolderWatcher_linux(LocalFileSystemObserverWorker *parent, const SyncPath &path) :
    FolderWatcher(parent, path) {}

SyncPath FolderWatcher_linux::makeSyncPath(const SyncPath &watchedFolderPath, const char *fileName) {
    const auto syncName = SyncName(fileName);
    return syncName.empty() ? watchedFolderPath : watchedFolderPath / syncName;
}

void FolderWatcher_linux::startWatching() {
    LOGW_DEBUG(_logger, L"Start watching folder " << Utility::formatSyncPath(_folder));
    LOG_DEBUG(_logger, "File system format: " << Utility::fileSystemName(_folder));
    LOG_DEBUG(_logger, "Free space on disk: " << Utility::getFreeDiskSpace(_folder) << " bytes.");

    _fileDescriptor = inotify_init();
    if (_fileDescriptor == -1) {
        LOG_WARN(_logger, "inotify_init() failed: " << strerror(errno));
        return;
    }

    if (const auto &exitInfo = addFolderRecursive(_folder); !exitInfo) {
        setExitInfo(exitInfo);
        return;
    }

    while (!_stop) {
        _ready = true;
        unsigned int avail = 0;
        (void) ioctl(static_cast<int>(_fileDescriptor), FIONREAD,
                     &avail); // Since read() is blocking until something has changed, we use ioctl to check if there is changes
        if (avail > 0) {
            char buffer[BUF_LEN];
            ssize_t len = read(static_cast<int>(_fileDescriptor), buffer, BUF_LEN);
            if (len == -1) {
                LOG_WARN(_logger, "Error reading file descriptor " << errno);
                continue;
            }
            // iterate events in buffer
            std::uint64_t offset = 0;
            while (offset < static_cast<std::uint64_t>(len)) {
                if (_stop) {
                    break;
                }
                auto *event = (inotify_event *) (buffer + offset);

                auto opType = OperationType::None;
                bool skip = false;
                if (event->mask & IN_CREATE) {
                    opType = OperationType::Create;
                } else if (event->mask & IN_DELETE || event->mask & IN_MOVED_FROM) {
                    opType = OperationType::Delete;
                } else if (event->mask & IN_MOVED_TO) {
                    opType = OperationType::Move;
                } else if (event->mask & (IN_MODIFY | IN_ATTRIB)) {
                    opType = OperationType::Edit;
                } else {
                    // Ignore all other events
                    skip = true;
                }

                if (!skip && !_stop) {
                    if (_watchToPath.contains(event->wd)) {
                        // `event->name` is empty for instance if the event is a permission change on a watched
                        // directory (see inotify man page).
                        const SyncPath path = makeSyncPath(_watchToPath[event->wd], event->name);
                        if (ParametersCache::isExtendedLogEnabled()) {
                            LOGW_DEBUG(_logger,
                                       L"Operation " << opType << L" detected on item with " << Utility::formatSyncPath(path));
                        }

                        if (const auto exitInfo = changeDetected(path, opType); !exitInfo) {
                            LOGW_WARN(KDC::Log::instance()->getLogger(), L"Error in FolderWatcher_linux::changeDetected for "
                                                                                 << Utility::formatSyncPath(path) << L" "
                                                                                 << exitInfo);
                        }

                        bool isDirectory = false;
                        auto ioError = IoError::Success;
                        if (const bool isDirSuccess = IoHelper::checkIfIsDirectory(path, isDirectory, ioError); !isDirSuccess) {
                            LOGW_WARN(_logger,
                                      L"Error in IoHelper::checkIfIsDirectory: " << Utility::formatIoError(path, ioError));
                            continue;
                        }

                        if (ioError == IoError::AccessDenied) {
                            LOGW_WARN(_logger, L"The item misses search/exec permission - " << Utility::formatSyncPath(path));
                        }

                        if ((event->mask & (IN_MOVED_TO | IN_CREATE)) && isDirectory) {
                            if (auto exitInfo = addFolderRecursive(path); !exitInfo) {
                                setExitInfo(exitInfo);
                                return;
                            };
                        }

                        if (event->mask & (IN_MOVED_FROM | IN_DELETE)) {
                            removeFoldersBelow(path);
                        }
                    }
                }

                offset += sizeof(inotify_event) + event->len;
            }
        } else {
            Utility::msleep(SLEEP_TIME);
        }
    }

    LOGW_DEBUG(_logger, L"Folder watching stopped: " << Utility::formatSyncPath(_folder));
}

bool FolderWatcher_linux::findSubFolders(const SyncPath &dir, std::list<SyncPath> &fullList) {
    IoHelper::DirectoryIterator dirIt;
    if (IoError ioError = IoError::Success; !IoHelper::getDirectoryIterator(dir, true, ioError, dirIt)) {
        LOGW_WARN(logger(), L"Error in DirectoryIterator for " << Utility::formatIoError(dir, ioError));
        if (ioError == IoError::AccessDenied) {
            setExitInfo({ExitCode::SystemError, ExitCause::FileAccessError});
        } else if (ioError == IoError::NoSuchFileOrDirectory) {
            setExitInfo({ExitCode::SystemError, ExitCause::NotFound});
        } else {
            setExitInfo({ExitCode::SystemError, ExitCause::Unknown});
        }
        return false;
    }

    DirectoryEntry entry;
    IoError ioError = IoError::Success;
    bool endOfDir = false;
    while (dirIt.next(entry, endOfDir, ioError) && !endOfDir && ioError == IoError::Success) {
        if (entry.is_directory() && !entry.is_symlink()) fullList.push_back(entry.path());
    }

    return true;
}

FolderWatcher_linux::AddWatchOutcome FolderWatcher_linux::inotifyAddWatch(const SyncPath &path) {
    AddWatchOutcome result;
    result.returnValue = inotify_add_watch(static_cast<int>(_fileDescriptor), path.string().c_str(),
                                           IN_CLOSE_WRITE | IN_ATTRIB | IN_MOVE | IN_CREATE | IN_DELETE | IN_MODIFY |
                                                   IN_DELETE_SELF | IN_MOVE_SELF | IN_UNMOUNT | IN_ONLYDIR | IN_DONT_FOLLOW);
    result.errorNumber = errno;

    return result;
}

ExitInfo FolderWatcher_linux::inotifyRegisterPath(const SyncPath &path) {
    if (std::error_code ec; !std::filesystem::exists(path, ec)) {
        if (ec) {
            LOGW_WARN(_logger, L"Failed to check if path exists for " << Utility::formatStdError(path, ec));
            return {ExitCode::SystemError, ExitCause::Unknown};
        }
        LOGW_DEBUG(_logger, L"Folder " << Utility::formatSyncPath(path) << L" does not exist anymore. Registration aborted.");

        return ExitCode::Ok;
    }

    const auto outcome = inotifyAddWatch(path);
    if (outcome.returnValue == -1) {
        switch (outcome.errorNumber) { // Errors are documented at https://man7.org/linux/man-pages/man2/inotify_add_watch.2.html
            case ENOMEM:
                LOG_ERROR(_logger, "Error in FolderWatcher_linux::inotifyAddWatch: Out of memory.");
                return {ExitCode::SystemError, ExitCause::NotEnoughMemory};
            case ENOSPC:
                // An insufficient number of inotify watches is a common error cause.
                // It needs to be fixed by the user, see e.g.,
                // https://stackoverflow.com/questions/47075661/error-user-limit-of-inotify-watches-reached-extreact-build
                LOG_ERROR(_logger, "Limit number of inotify watches reached. The latter can be raised by the user.");
                return {ExitCode::SystemError, ExitCause::NotEnoughINotifyWatches};
            default:
                LOGW_ERROR(_logger, L"Unhandled error in FolderWatcher_linux::inotifyAddWatch: "
                                            << Utility::formatSyncPath(path) << L" errno=" << outcome.errorNumber
                                            << L". Folder registration failure. Ignoring it.");
                return ExitCode::Ok;
        }
    }

    (void) _watchToPath.insert({outcome.returnValue, path});
    (void) _pathToWatch.insert({path, outcome.returnValue});

    return ExitCode::Ok;
}

ExitInfo FolderWatcher_linux::addFolderRecursive(const SyncPath &path) {
    if (_pathToWatch.contains(path)) {
        // This path is already watched
        return ExitCode::Ok;
    }

    int subdirs = 0;
    LOGW_DEBUG(_logger, L"(+) Watcher:" << Utility::formatSyncPath(path));

    if (auto exitInfo = inotifyRegisterPath(path); !exitInfo) return exitInfo;

    std::list<SyncPath> allSubFolders;
    if (!findSubFolders(path, allSubFolders)) {
        LOG_ERROR(_logger, "Could not traverse all sub folders");
        return {ExitCode::SystemError, ExitCause::Unknown};
    }

    for (const auto &subDirPath: allSubFolders) {
        if (std::error_code ec; std::filesystem::exists(subDirPath, ec) && !_pathToWatch.contains(subDirPath)) {
            subdirs++;

            if (auto exitInfo = inotifyRegisterPath(subDirPath); !exitInfo) return exitInfo;
        } else {
            if (ec) {
                LOGW_WARN(_logger, L"Failed to check if path exists " << Utility::formatSyncPath(path) << L": "
                                                                      << Utility::s2ws(ec.message()) << L" (" << ec.value()
                                                                      << L")");
            }
            LOGW_DEBUG(_logger, L"    `-> discarded: " << Utility::formatSyncPath(subDirPath));
        }
    }

    if (subdirs > 0) {
        LOG_DEBUG(_logger, "    `-> and " << subdirs << " subdirectories");
    }

    return ExitCode::Ok;
}

void FolderWatcher_linux::removeFoldersBelow(const SyncPath &dirPath) {
    auto it = _pathToWatch.find(dirPath.string());
    if (it == _pathToWatch.end()) {
        return;
    }

    // Remove the entry and all subentries
    while (it != _pathToWatch.end()) {
        auto itPath = it->first;
        if (!Utility::isDescendantOrEqual(itPath, dirPath)) {
            break;
        }

        auto wid = it->second;
        if (const auto wd = inotify_rm_watch(static_cast<int>(_fileDescriptor), wid); wd > -1) {
            _watchToPath.erase(wid);
            it = _pathToWatch.erase(it);
            LOG_DEBUG(_logger, "Removed watch on" << itPath);
            continue;
        }

        ++it;
        LOG_ERROR(_logger, "Error in inotify_rm_watch :" << errno);
        sentry::Handler::captureMessage(sentry::Level::Error, "FolderWatcher_linux::removeFoldersBelow",
                                        "Error in inotify_rm_watch :" + std::to_string(errno));
    }
}

ExitInfo FolderWatcher_linux::changeDetected(const SyncPath &path, OperationType opType) const {
    std::list<std::pair<SyncPath, OperationType>> list;
    (void) list.emplace_back(path, opType);
    if (const auto exitInfo = _parent->changesDetected(list); !exitInfo) {
        LOGW_WARN(_logger, L"Error in LocalFileSystemObserverWorker::changesDetected: " << exitInfo);
        return exitInfo;
    }

    return ExitCode::Ok;
}

void FolderWatcher_linux::stopWatching() {
    LOGW_DEBUG(_logger, L"Stop watching folder: " << Utility::formatSyncPath(_folder));

    (void) close(static_cast<int>(_fileDescriptor));
}

} // namespace KDC
