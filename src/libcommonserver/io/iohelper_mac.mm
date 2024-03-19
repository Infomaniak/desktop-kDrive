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

#import "libcommon/utility/types.h"
#import "libcommonserver/io/iohelper.h"

#import "config.h"

#include <sys/stat.h>

#import <AppKit/NSApplication.h>
#import <IOKit/pwr_mgt/IOPMLib.h>

namespace KDC {

IoError IoHelper::nsError2ioError(int nsErrorCode) noexcept {
    // See https://developer.apple.com/documentation/foundation/
    switch (nsErrorCode) {
        case NSFileNoSuchFileError:
        case NSFileReadNoSuchFileError:
            return IoErrorNoSuchFileOrDirectory;
        case NSFileReadNoPermissionError:
            return IoErrorAccessDenied;
        default:
            return IoErrorUnknown;
    }
}

bool IoHelper::_checkIfAlias(const SyncPath &path, bool &isAlias, IoError &ioError) noexcept {
    isAlias = false;
    ioError = IoErrorSuccess;

    NSString *pathStr = [NSString stringWithCString:path.c_str() encoding:NSUTF8StringEncoding];
    NSURL *pathURL = [NSURL fileURLWithPath:pathStr];

    NSError *error = nil;
    NSNumber *isAliasNumber = nil;
    BOOL ret = [pathURL getResourceValue:&isAliasNumber forKey:NSURLIsAliasFileKey error:&error];
    if (!ret) {
        ioError = nsError2ioError(error.code);
        if (ioError != IoErrorUnknown) {
            return true;
        }
        return false;
    } else if (isAliasNumber == nil) {
        // Property not available (should not happen)
        return false;
    }

    isAlias = [isAliasNumber boolValue];
    return true;
}

bool IoHelper::createAlias(const std::string &data, const SyncPath &aliasPath, IoError &ioError) noexcept {
    ioError = IoErrorSuccess;

    CFStringRef aliasPathStr = CFStringCreateWithCString(nil, aliasPath.native().c_str(), kCFStringEncodingUTF8);
    CFURLRef aliasUrl = CFURLCreateWithFileSystemPath(nil, aliasPathStr, kCFURLPOSIXPathStyle, false);
    CFRelease(aliasPathStr);

    CFDataRef bookmarkRef = CFDataCreate(NULL, (const UInt8 *)data.data(), data.size());

    CFErrorRef error = nil;
    bool ret = CFURLWriteBookmarkDataToFile(bookmarkRef, aliasUrl, kCFURLBookmarkCreationSuitableForBookmarkFile, &error);
    CFRelease(bookmarkRef);
    CFRelease(aliasUrl);
    if (!ret) {
        CFIndex idx = CFErrorGetCode(error);
        CFRelease(error);
        ioError = nsError2ioError(idx);
        if (ioError != IoErrorUnknown) {
            return true;
        }
        return false;
    }

    return true;
}

bool IoHelper::readAlias(const SyncPath &aliasPath, std::string &data, SyncPath &targetPath, IoError &ioError) noexcept {
    data = std::string{};
    targetPath = SyncPath{};
    ioError = IoErrorSuccess;

    CFStringRef aliasPathStr = CFStringCreateWithCString(nil, aliasPath.native().c_str(), kCFStringEncodingUTF8);
    CFURLRef aliasUrl = CFURLCreateWithFileSystemPath(nil, aliasPathStr, kCFURLPOSIXPathStyle, false);
    CFRelease(aliasPathStr);

    CFErrorRef error = nil;
    CFDataRef bookmarkRef = CFURLCreateBookmarkDataFromFile(nil, aliasUrl, &error);
    CFRelease(aliasUrl);
    if (bookmarkRef == nil) {
        if (error) {
            CFIndex idx = CFErrorGetCode(error);
            CFRelease(error);
            ioError = nsError2ioError(idx);
            if (ioError != IoErrorUnknown) {
                return true;
            }
        }
        return false;
    }

    const uint32_t size = (uint32_t)CFDataGetLength(bookmarkRef);
    unsigned char *buffer = new unsigned char[size];
    CFDataGetBytes(bookmarkRef, CFRangeMake(0, size), (UInt8 *)buffer);
    data = std::string(reinterpret_cast<char const *>(buffer), size);
    delete[] buffer;

    Boolean isStale;
    CFURLRef targetUrl =
        CFURLCreateByResolvingBookmarkData(nil, bookmarkRef, kCFBookmarkResolutionWithoutUIMask, nil, nil, &isStale, &error);
    CFRelease(bookmarkRef);
    if (targetUrl == nil) {
        if (error) {
            CFIndex idx = CFErrorGetCode(error);
            ioError = nsError2ioError(idx);
            CFRelease(error);
            return ioError != IoErrorUnknown;
        }
        return false;
    }

    CFStringRef targetPathStr = CFURLCopyFileSystemPath(targetUrl, kCFURLPOSIXPathStyle);
    CFRelease(targetUrl);
    targetPath = SyncPath(std::string([(NSString *)targetPathStr UTF8String]));

    return true;
}

bool IoHelper::createAliasFromPath(const SyncPath &targetPath, const SyncPath &aliasPath, IoError &ioError) noexcept {
    ioError = IoErrorSuccess;

    CFStringRef aliasPathStr = CFStringCreateWithCString(nil, aliasPath.native().c_str(), kCFStringEncodingUTF8);
    CFURLRef aliasUrl = CFURLCreateWithFileSystemPath(nil, aliasPathStr, kCFURLPOSIXPathStyle, false);
    CFRelease(aliasPathStr);

    CFStringRef targetPathStr = CFStringCreateWithCString(nil, targetPath.native().c_str(), kCFStringEncodingUTF8);
    CFURLRef targetUrl = CFURLCreateWithFileSystemPath(nil, targetPathStr, kCFURLPOSIXPathStyle, false);
    CFRelease(targetPathStr);

    CFErrorRef error = nil;
    CFDataRef bookmarkRef = CFURLCreateBookmarkData(
        // The obj-c/swift doc: https://developer.apple.com/documentation/corefoundation/1542923-cfurlcreatebookmarkdata
        NULL, targetUrl, kCFURLBookmarkCreationSuitableForBookmarkFile, CFArrayRef{}, nil, &error);

    if (error) {
        CFIndex idx = CFErrorGetCode(error);
        CFRelease(error);
        ioError = nsError2ioError(idx);
        return false;
    }

    const bool result =
        CFURLWriteBookmarkDataToFile(bookmarkRef, aliasUrl, kCFURLBookmarkCreationSuitableForBookmarkFile, &error);

    CFRelease(bookmarkRef);
    CFRelease(aliasUrl);
    CFRelease(targetUrl);

    if (!result) {
        if (error) {
            CFIndex idx = CFErrorGetCode(error);
            CFRelease(error);
            ioError = nsError2ioError(idx);
        }

        return false;
    }

    return true;
}

void IoHelper::setFileHidden(const SyncPath &path, bool hidden) noexcept {
    NSString *pathStr = [NSString stringWithCString:path.c_str() encoding:NSUTF8StringEncoding];
    NSURL *pathURL = [NSURL fileURLWithPath:pathStr];
    NSNumber *value = [NSNumber numberWithBool:hidden];

    [pathURL setResourceValue:value forKey:NSURLIsHiddenKey error:NULL];
}

bool isLocked(const SyncPath &path) {
    NSString *pathStr = [NSString stringWithCString:path.c_str() encoding:NSUTF8StringEncoding];

    NSDictionary *attributes = [[NSFileManager defaultManager] attributesOfItemAtPath:pathStr error:nil];
    BOOL isLocked = [[attributes objectForKey:NSFileImmutable] boolValue];
    return isLocked;
}


}  // namespace KDC
