/*
 * Infomaniak kDrive - Desktop
 * Copyright (C) 2023-2024 Infomaniak Network SA
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

#include "folderwatcher_mac.h"
#include "localfilesystemobserverworker.h"
#include "libcommonserver/utility/utility.h"
#include "requests/parameterscache.h"

#include <codecvt>

namespace KDC {

FolderWatcher_mac::FolderWatcher_mac(LocalFileSystemObserverWorker *parent, const SyncPath &path) :
    FolderWatcher(parent, path), _stream(nullptr) {}

static void callback([[maybe_unused]] ConstFSEventStreamRef streamRef, void *clientCallBackInfo, size_t numEvents,
                     void *eventPathsVoid, const FSEventStreamEventFlags eventFlags[],
                     [[maybe_unused]] const FSEventStreamEventId eventIds[]) {
    auto *fw = reinterpret_cast<FolderWatcher_mac *>(clientCallBackInfo);
    if (!fw) {
        // Should never happen
        return;
    }

    static const FSEventStreamEventFlags interestingFlags =
            kFSEventStreamEventFlagItemCreated // for new folder/file
            | kFSEventStreamEventFlagItemRemoved // for rm
            | kFSEventStreamEventFlagItemInodeMetaMod // for mtime change
            | kFSEventStreamEventFlagItemRenamed // also coming for moves to trash in finder
            | kFSEventStreamEventFlagItemModified // for content change
            | kFSEventStreamEventFlagItemChangeOwner; // for rights change

    std::list<std::pair<std::filesystem::path, OperationType>> paths;
    const auto eventPaths = static_cast<CFArrayRef>(eventPathsVoid);
    for (int i = 0; i < static_cast<int>(numEvents); ++i) {
        auto opType = FolderWatcher_mac::getOpType(eventFlags[i]);
        if (!(eventFlags[i] & interestingFlags)) {
            // Ignore changes that does not appear in interestingFlags
            continue;
        }

        const auto pathRef = reinterpret_cast<CFStringRef>(CFArrayGetValueAtIndex(eventPaths, i));
        const char *pathPtr = CFStringGetCStringPtr(pathRef, kCFStringEncodingUTF8);
        char *pathBuf = nullptr;
        if (!pathPtr) {
            // Manage the case when CFStringGetCStringPtr returns NULL (for instance if the string contains accented characters)
            CFIndex pathLength = CFStringGetLength(pathRef);
            CFIndex pathMaxSize = CFStringGetMaximumSizeForEncoding(pathLength, kCFStringEncodingUTF8) + 1;
            pathBuf = (char *) malloc(static_cast<size_t>(pathMaxSize));
            CFStringGetCString(pathRef, pathBuf, pathMaxSize, kCFStringEncodingUTF8);
        }

        if (ParametersCache::isExtendedLogEnabled()) {
            LOGW_DEBUG(fw->logger(),
                       L"Operation " << opType << L" detected on item " << Utility::s2ws(pathPtr ? pathPtr : pathBuf).c_str());
        }

        // TODO : to be tested to get inode (https://github.com/fsevents/fsevents/pull/360/files)
        //        CFDictionaryRef path_info_dict =
        //        reinterpret_cast<CFDictionaryRef>(CFArrayGetValueAtIndex((CFArrayRef)eventPaths, i)); CFStringRef path =
        //        reinterpret_cast<CFStringRef>(CFDictionaryGetValue(path_info_dict, kFSEventStreamEventExtendedDataPathKey));
        //        CFNumberRef cf_inode = reinterpret_cast<CFNumberRef>(CFDictionaryGetValue(path_info_dict,
        //        kFSEventStreamEventExtendedFileIDKey));

        paths.push_back({pathPtr ? pathPtr : pathBuf, opType});

        if (pathBuf) {
            free(pathBuf);
        }
    }

    fw->doNotifyParent(paths);
}

void FolderWatcher_mac::startWatching() {
    LOGW_DEBUG(_logger, L"Start watching folder: " << Utility::formatSyncPath(_folder));
    LOG_DEBUG(_logger, "File system format: " << Utility::fileSystemName(_folder).c_str());
    _ready = true;

    CFStringRef path = CFStringCreateWithCString(nullptr, _folder.c_str(), kCFStringEncodingUTF8);
    CFArrayRef pathsToWatch = CFArrayCreate(nullptr, (const void **) &path, 1, nullptr);

    FSEventStreamContext ctx = {0, this, nullptr, nullptr, nullptr};

    _stream = FSEventStreamCreate(
            nullptr, &callback, &ctx, pathsToWatch, kFSEventStreamEventIdSinceNow,
            0, // latency
            kFSEventStreamCreateFlagUseCFTypes | kFSEventStreamCreateFlagFileEvents /*| kFSEventStreamCreateFlagIgnoreSelf*/);
    // TODO : try kFSEventStreamCreateFlagUseExtendedData to get inode directly from event

    CFRelease(pathsToWatch);
    FSEventStreamScheduleWithRunLoop(_stream, CFRunLoopGetCurrent(), kCFRunLoopDefaultMode);
    FSEventStreamStart(_stream);
    CFRunLoopRun();

    LOGW_DEBUG(_logger, L"Folder watching stopped: " << Utility::formatSyncPath(_folder));
}

void FolderWatcher_mac::doNotifyParent(const std::list<std::pair<std::filesystem::path, OperationType>> &changes) {
    if (!_stop && _parent) {
        _parent->changesDetected(changes);
    }
}

OperationType FolderWatcher_mac::getOpType(const FSEventStreamEventFlags eventFlags) {
    if (eventFlags & kFSEventStreamEventFlagItemRemoved) {
        return OperationType::Delete;
    } else if (eventFlags & kFSEventStreamEventFlagItemCreated) {
        return OperationType::Create;
    } else if (eventFlags & kFSEventStreamEventFlagItemModified || eventFlags & kFSEventStreamEventFlagItemInodeMetaMod) {
        return OperationType::Edit;
    } else if (eventFlags & kFSEventStreamEventFlagItemRenamed) {
        return OperationType::Move;
    } else if (eventFlags & kFSEventStreamEventFlagItemChangeOwner) {
        return OperationType::Rights;
    }
    return OperationType::None;
}

void KDC::FolderWatcher_mac::stopWatching() {
    if (_stream) {
        LOGW_DEBUG(_logger, L"Stop watching folder: " << Utility::formatSyncPath(_folder));
        FSEventStreamStop(_stream);
        FSEventStreamInvalidate(_stream);
        FSEventStreamRelease(_stream);

        _stream = nullptr;
    }
}

} // namespace KDC
