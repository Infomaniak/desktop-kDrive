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
    LOG4CPLUS_DEBUG(_logger, L"Start watching folder " << Utility::formatSyncPath(_folder));
    LOG4CPLUS_DEBUG(_logger, "File system format: " << Utility::fileSystemName(_folder).c_str());

    _fileDescriptor = inotify_init();
    if (_fileDescriptor == -1) {
        LOG4CPLUS_WARN(_logger, "inotify_init() failed: " << strerror(errno));
        return;
    }

    if (!addFolderRecursive(_folder)) {
        return;
    }

    while (!_stop) {
        _ready = true;
        unsigned int avail;
        ioctl(_fileDescriptor, FIONREAD,
              &avail); // Since read() is blocking until something has changed, we use ioctl to check if there is changes
        if (avail > 0) {
            char buffer[BUF_LEN];
            ssize_t len = read(_fileDescriptor, buffer, BUF_LEN);
            if (len == -1) {
                LOG_WARN(_logger, "Error reading file descriptor " << errno);
                continue;
            }
            // iterate events in buffer
            unsigned int offset = 0;
            while (offset < len) {
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

                        changeDetected(path, opType);
                        bool isDirectory = false;
                        auto ioError = IoError::Success;
                        const bool isDirSuccess = IoHelper::checkIfIsDirectory(path, isDirectory, ioError);
                        if (!isDirSuccess) {
                            LOGW_WARN(_logger, L"Error in IoHelper::checkIfIsDirectory: "
                                                       << Utility::formatIoError(path, ioError).c_str());
                            continue;
                        }

                        if (ioError == IoError::AccessDenied) {
                            LOGW_WARN(_logger, L"The item misses search/exec permission - " << Utility::formatSyncPath(path));
                        }

                        if ((event->mask & (IN_MOVED_TO | IN_CREATE)) && isDirectory) {
                            addFolderRecursive(path);
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
    bool ok = true;
    if (access(dir.c_str(), R_OK) != 0) {
        LOG4CPLUS_WARN(_logger, L"SyncDir is not readable: " << Utility::formatSyncPath(dir));
        setExitInfo({ExitCode::SystemError, ExitCause::SyncDirAccesError});
        return false;
    }
    if (std::error_code ec; !std::filesystem::exists(dir, ec)) {
        if (ec) {
            LOG4CPLUS_WARN(_logger, L"Failed to check existence of " << Utility::formatSyncPath(dir) << L": "
                                                                     << Utility::formatStdError(ec));
        } else {
            LOG4CPLUS_WARN(_logger, L"Non existing path coming in: " << Utility::formatSyncPath(dir));
        }
        ok = false;
    } else {
        try {
            const auto dirIt = std::filesystem::recursive_directory_iterator(
                    dir, std::filesystem::directory_options::skip_permission_denied, ec);
            if (ec) {
                LOG4CPLUS_WARN(_logger, L"Error in findSubFolders: " << Utility::formatStdError(ec));
                return false;
            }

            for (const auto &dirEntry: dirIt) {
                if (dirEntry.is_directory()) fullList.push_back(dirEntry.path());
            }

        } catch (std::filesystem::filesystem_error &e) {
            LOG4CPLUS_WARN(_logger, L"Error caught in findSubFolders: " << e.code() << " - " << e.what());
            ok = false;
        } catch (...) {
            LOG4CPLUS_WARN(_logger, L"Error caught in findSubFolders");
            ok = false;
        }
    }

    return ok;
}

bool FolderWatcher_linux::inotifyRegisterPath(const SyncPath &path) {
    if (std::error_code ec; !std::filesystem::exists(path, ec)) {
        if (ec.value() != 0) {
            LOG4CPLUS_WARN(_logger, L"Failed to check if path exists " << Utility::s2ws(path.string()).c_str() << L": "
                                                                       << Utility::s2ws(ec.message()).c_str() << " ("
                                                                       << ec.value() << ")");
        }
        return false;
    }

    const auto wd = inotify_add_watch(_fileDescriptor, path.string().c_str(),
                               IN_CLOSE_WRITE | IN_ATTRIB | IN_MOVE | IN_CREATE | IN_DELETE | IN_MODIFY | IN_DELETE_SELF |
                                       IN_MOVE_SELF | IN_UNMOUNT | IN_ONLYDIR | IN_DONT_FOLLOW);

    if (wd > -1) {
        _watchToPath.insert({wd, path});
        _pathToWatch.insert({path, wd});
    } else {
        // If we're running out of memory or inotify watches, become
        // unreliable.
        if (_isReliable && (errno == ENOMEM || errno == ENOSPC)) {
            _isReliable = false;
            LOG4CPLUS_ERROR(_logger, "Out of memory or limit number of inotify watches reached!"); // TODO : notify the user?
            return false;
        }
    }

    return true;
}

bool FolderWatcher_linux::addFolderRecursive(const SyncPath &path) {
    if (_pathToWatch.contains(path)) {
        // This path is already watched
        return true;
    }

    int subdirs = 0;
    LOG4CPLUS_DEBUG(_logger, L"(+) Watcher:" << Path2WStr(path));

    inotifyRegisterPath(path);

    std::list<SyncPath> allSubFolders;
    if (!findSubFolders(path, allSubFolders)) {
        LOG4CPLUS_ERROR(_logger, "Could not traverse all sub folders");
        return false;
    }

    for (const auto &subDirPath: allSubFolders) {
        if (std::error_code ec ;std::filesystem::exists(subDirPath, ec) && !_pathToWatch.contains(subDirPath)  ) {
            subdirs++;

            inotifyRegisterPath(subDirPath);
        } else {
            if (ec.value() != 0) {
                LOG4CPLUS_WARN(_logger, L"Failed to check if path exists " << Path2WStr(path).c_str() << L": "
                                                                           << Utility::s2ws(ec.message()).c_str() << " ("
                                                                           << ec.value() << ")");
            }
            LOG4CPLUS_DEBUG(_logger, L"    `-> discarded: " << Path2WStr(subDirPath).c_str());
        }
    }

    if (subdirs > 0) {
        LOG4CPLUS_DEBUG(_logger, "    `-> and " << subdirs << " subdirectories");
    }

    return true;
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
        if (const auto wd = inotify_rm_watch(_fileDescriptor, wid); wd > -1) {
            _watchToPath.erase(wid);
            it = _pathToWatch.erase(it);
            LOG4CPLUS_DEBUG(_logger, "Removed watch on" << itPath.c_str());
            continue;
        }

        ++it;
        LOG4CPLUS_ERROR(_logger, "Error in inotify_rm_watch :" << errno);
        sentry::Handler::captureMessage(sentry::Level::Error, "FolderWatcher_linux::removeFoldersBelow", "Error in inotify_rm_watch :" + errno);
    }
}

void FolderWatcher_linux::changeDetected(const SyncPath &path, OperationType opType)const {
    std::list<std::pair<SyncPath, OperationType>> list;
    (void) list.emplace_back(path, opType);
    _parent->changesDetected(list);
}

void FolderWatcher_linux::stopWatching() {
    LOG4CPLUS_DEBUG(_logger, L"Stop watching folder: " << Path2WStr(_folder).c_str());

    close(_fileDescriptor);
}

} // namespace KDC
