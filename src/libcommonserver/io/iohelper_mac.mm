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

#import "iohelper.h"

#import "utility/utility.h"
#import "utility/types.h"

#import "libcommon/utility/utility.h"

#import "config.h"

#include <sys/stat.h>
#include <sys/mount.h>

#import <AppKit/NSApplication.h>
#import <IOKit/pwr_mgt/IOPMLib.h>

#include <log4cplus/loggingmacros.h>

namespace KDC {

IoError nsError2ioError(NSError *nsError) noexcept {
    // See https://developer.apple.com/documentation/foundation/
    if ([nsError.domain isEqualToString:NSCocoaErrorDomain]) {
        switch (nsError.code) {
            case NSFileNoSuchFileError:
            case NSFileReadNoSuchFileError:
                return IoError::NoSuchFileOrDirectory;
            case NSFileReadNoPermissionError:
            case NSFileWriteNoPermissionError:
                return IoError::AccessDenied;
            case NSFileReadInvalidFileNameError:
                return IoError::InvalidFileName;
            default:
                return IoError::Unknown;
        }
    } else {
        return IoError::Unknown;
    }
}

bool IoHelper::_checkIfAlias(const SyncPath &path, bool &isAlias, IoError &ioError) noexcept {
    isAlias = false;
    ioError = IoError::Success;

    NSString *pathStr = [NSString stringWithCString:path.c_str() encoding:NSUTF8StringEncoding];
    if (pathStr == nil) {
        LOGW_WARN(logger(), L"Bad parameters");
        return false;
    }

    NSURL *pathURL = [NSURL fileURLWithPath:pathStr];

    NSError *error = nil;
    NSNumber *isAliasNumber = nil;
    BOOL ret = [pathURL getResourceValue:&isAliasNumber forKey:NSURLIsAliasFileKey error:&error];
    if (!ret) {
        if (error) {
            ioError = nsError2ioError(error);
            if (ioError != IoError::Unknown) {
                return true;
            } else {
                LOGW_WARN(logger(), L"Error in getResourceValue: " << Utility::formatIoError(path, ioError));
                return false;
            }
        }
        LOGW_WARN(logger(), L"Error in getResourceValue: " << Utility::formatSyncPath(path));
        return false;
    } else if (isAliasNumber == nil) {
        // Property not available (should not happen)
        LOGW_WARN(logger(), L"No NSURLIsAliasFileKey: " << Utility::formatSyncPath(path));
        return false;
    }

    isAlias = [isAliasNumber boolValue];
    return true;
}

bool IoHelper::createAlias(const std::string &data, const SyncPath &aliasPath, IoError &ioError) noexcept {
    ioError = IoError::Success;

    CFStringRef aliasPathStr = CFStringCreateWithCString(nil, aliasPath.native().c_str(), kCFStringEncodingUTF8);
    CFURLRef aliasUrl = CFURLCreateWithFileSystemPath(nil, aliasPathStr, kCFURLPOSIXPathStyle, false);
    CFRelease(aliasPathStr);

    CFDataRef bookmarkRef = CFDataCreate(nullptr, (const UInt8 *) data.data(), static_cast<CFIndex>(data.size()));

    CFErrorRef error = nil;
    bool ret = CFURLWriteBookmarkDataToFile(bookmarkRef, aliasUrl, kCFURLBookmarkCreationSuitableForBookmarkFile, &error);
    CFRelease(bookmarkRef);
    CFRelease(aliasUrl);
    if (!ret) {
        if (error) {
            ioError = nsError2ioError((__bridge NSError *) error);
            CFRelease(error);
            if (ioError != IoError::Unknown) {
                return true;
            } else {
                LOGW_WARN(logger(), L"Error in CFURLWriteBookmarkDataToFile: " << Utility::formatIoError(aliasPath, ioError));
                return false;
            }
        }
        LOGW_WARN(logger(), L"Error in CFURLWriteBookmarkDataToFile: " << Utility::formatSyncPath(aliasPath));
        return false;
    }

    return true;
}

bool IoHelper::readAlias(const SyncPath &aliasPath, std::string &data, SyncPath &targetPath, IoError &ioError) noexcept {
    data = std::string{};
    targetPath = SyncPath{};
    ioError = IoError::Success;

    // Read data
    CFStringRef aliasPathStr = CFStringCreateWithCString(nil, aliasPath.native().c_str(), kCFStringEncodingUTF8);
    CFURLRef aliasUrl = CFURLCreateWithFileSystemPath(nil, aliasPathStr, kCFURLPOSIXPathStyle, false);
    CFRelease(aliasPathStr);

    CFErrorRef error = nil;
    CFDataRef bookmarkRef = CFURLCreateBookmarkDataFromFile(nil, aliasUrl, &error);
    CFRelease(aliasUrl);
    if (bookmarkRef == nil) {
        if (error) {
            ioError = nsError2ioError((__bridge NSError *) error);
            CFRelease(error);
            if (ioError != IoError::Unknown) {
                return true;
            } else {
                LOGW_WARN(logger(), L"Error in CFURLCreateBookmarkDataFromFile: " << Utility::formatIoError(aliasPath, ioError));
                return false;
            }
        }
        LOGW_WARN(logger(), L"Error in CFURLCreateBookmarkDataFromFile: " << Utility::formatSyncPath(aliasPath));
        return false;
    }

    const uint32_t size = (uint32_t) CFDataGetLength(bookmarkRef);
    unsigned char *buffer = new unsigned char[size];
    CFDataGetBytes(bookmarkRef, CFRangeMake(0, size), (UInt8 *) buffer);
    data = std::string(reinterpret_cast<char const *>(buffer), size);
    delete[] buffer;

    // Read target
    Boolean isStale;
    CFURLRef targetUrl =
            CFURLCreateByResolvingBookmarkData(nil, bookmarkRef, kCFBookmarkResolutionWithoutUIMask, nil, nil, &isStale, &error);
    CFRelease(bookmarkRef);
    if (targetUrl == nil) {
        if (error) {
            CFRelease(error);
        }
        return true;
    }

    CFStringRef targetPathStr = CFURLCopyFileSystemPath(targetUrl, kCFURLPOSIXPathStyle);
    CFRelease(targetUrl);
    targetPath = SyncPath(std::string([(__bridge NSString *) targetPathStr UTF8String]));
    CFRelease(targetPathStr);

    return true;
}

bool IoHelper::createAliasFromPath(const SyncPath &targetPath, const SyncPath &aliasPath, IoError &ioError) noexcept {
    ioError = IoError::Success;

    CFStringRef aliasPathStr = CFStringCreateWithCString(nil, aliasPath.native().c_str(), kCFStringEncodingUTF8);
    CFURLRef aliasUrl = CFURLCreateWithFileSystemPath(nil, aliasPathStr, kCFURLPOSIXPathStyle, false);
    CFRelease(aliasPathStr);

    CFStringRef targetPathStr = CFStringCreateWithCString(nil, targetPath.native().c_str(), kCFStringEncodingUTF8);
    CFURLRef targetUrl = CFURLCreateWithFileSystemPath(nil, targetPathStr, kCFURLPOSIXPathStyle, false);
    CFRelease(targetPathStr);

    CFErrorRef error = nil;
    CFDataRef bookmarkRef = CFURLCreateBookmarkData(
            // The obj-c/swift doc: https://developer.apple.com/documentation/corefoundation/1542923-cfurlcreatebookmarkdata
            nullptr, targetUrl, kCFURLBookmarkCreationSuitableForBookmarkFile, CFArrayRef{}, nil, &error);

    if (bookmarkRef == nil) {
        if (error) {
            ioError = nsError2ioError((__bridge NSError *) error);
            CFRelease(error);
        }
        CFRelease(aliasUrl);
        CFRelease(targetUrl);
        LOGW_WARN(logger(), L"Error in CFURLCreateBookmarkData: " << Utility::formatSyncPath(targetPath));
        return false;
    }

    const bool result =
            CFURLWriteBookmarkDataToFile(bookmarkRef, aliasUrl, kCFURLBookmarkCreationSuitableForBookmarkFile, &error);

    CFRelease(bookmarkRef);
    CFRelease(aliasUrl);
    CFRelease(targetUrl);

    if (!result) {
        if (error) {
            ioError = nsError2ioError((__bridge NSError *) error);
            CFRelease(error);
        }
        LOGW_WARN(logger(), L"Error in CFURLWriteBookmarkDataToFile: " << Utility::formatSyncPath(aliasPath));
        return false;
    }

    return true;
}

void IoHelper::setFileHidden(const SyncPath &path, bool hidden) noexcept {
    NSString *pathStr = [NSString stringWithCString:path.c_str() encoding:NSUTF8StringEncoding];
    if (pathStr == nil) {
        return;
    }

    NSURL *pathURL = [NSURL fileURLWithPath:pathStr];
    NSNumber *value = [NSNumber numberWithBool:hidden];

    [pathURL setResourceValue:value forKey:NSURLIsHiddenKey error:NULL];
}

IoError IoHelper::setFileDates(const SyncPath &filePath, SyncTime creationDate, SyncTime modificationDate,
                               bool symlink) noexcept {
    NSDate *cDate = [[NSDate alloc] initWithTimeIntervalSince1970:creationDate];
    NSDate *mDate = [[NSDate alloc] initWithTimeIntervalSince1970:modificationDate];
    NSString *filePathStr = [NSString stringWithUTF8String:filePath.native().c_str()];
    NSError *error = nil;
    bool ret = false;
    if (symlink) {
        if (cDate) {
            ret = [[NSURL fileURLWithPath:filePathStr isDirectory:NO] setResourceValue:cDate
                                                                                forKey:NSURLCreationDateKey
                                                                                 error:&error];
            if (!ret) {
                return nsError2ioError(error);
            }
        }

        if (mDate) {
            ret = [[NSURL fileURLWithPath:filePathStr isDirectory:NO] setResourceValue:mDate
                                                                                forKey:NSURLContentModificationDateKey
                                                                                 error:&error];
            if (!ret) {
                return nsError2ioError(error);
            }
        }
    } else {
        NSMutableDictionary *attrDictionary = [[NSMutableDictionary alloc] init];
        if (cDate) {
            [attrDictionary setObject:cDate forKey:NSFileCreationDate];
        }

        if (mDate) {
            [attrDictionary setObject:mDate forKey:NSFileModificationDate];
        }

        if ([[attrDictionary allKeys] count]) {
            NSFileManager *fileManager = [NSFileManager defaultManager];
            ret = [fileManager setAttributes:attrDictionary ofItemAtPath:filePathStr error:&error];
            if (!ret) {
                return nsError2ioError(error);
            }
        }
    }

    return IoError::Success;
}

bool IoHelper::moveItemToTrash(const SyncPath &itemPath) {
    if (itemPath.empty()) {
        LOG_WARN(Log::instance()->getLogger(), "Path is empty");
        return false;
    }

    NSString *filePath = [NSString stringWithCString:itemPath.c_str() encoding:NSUTF8StringEncoding];

    if (filePath == nullptr) {
        LOGW_WARN(Log::instance()->getLogger(),
                  L"Error in stringWithCString. Failed to cast std filepath to NSString." << Utility::formatSyncPath(itemPath));
        return false;
    }
    NSURL *fileURL = [NSURL fileURLWithPath:filePath];

    NSFileManager *fileManager = [NSFileManager defaultManager];

    NSError *error = nil;
    BOOL success = [fileManager trashItemAtURL:fileURL resultingItemURL:nil error:&error];

    if (error != nil) {
        const auto wcharError = reinterpret_cast<const wchar_t *>(
                [error.localizedDescription cStringUsingEncoding:NSUTF32LittleEndianStringEncoding]);
        LOGW_WARN(Log::instance()->getLogger(), std::wstring(wcharError) << Utility::formatSyncPath(itemPath));
    }

    return success;
}

bool IoHelper::isPathOnMountedDisk(const SyncPath &path, bool &isMounted, IoError &ioError) noexcept {
    isMounted = false;
    ioError = IoError::Success;

    std::string absPath = std::filesystem::absolute(path).string();

    struct statfs *mounts;
    int count = getmntinfo(&mounts, MNT_NOWAIT);
    if (count <= 0) {
        ioError = posixError2ioError(errno);
        LOGW_WARN(logger(), L"Error in getmntinfo: " << Utility::formatIoError(path, ioError));
        return false;
    }

    for (int i = 0; i < count; ++i) {
        std::string mountPoint = mounts[i].f_mntonname;
        if (absPath.compare(0, mountPoint.size(), mountPoint) == 0 &&
            (absPath.size() == mountPoint.size() || absPath[mountPoint.size()] == '/')) {
            isMounted = true;
            return true;
        }
    }

    isMounted = false;
    return true;
}

} // namespace KDC
