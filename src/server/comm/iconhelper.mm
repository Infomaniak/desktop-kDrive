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

#import "iconhelper.h"

#include <sys/xattr.h>

#import <AppKit/AppKit.h>

@implementation IconHelper

+ (BOOL)setCustomIcon:(NSImage *)icon forPath:(NSString *)path {
    if (!icon || !path) {
        return NO;
    }

    NSURL *url = [NSURL fileURLWithPath:path];
    NSData *iconData = [self createICNSDataFromImage:icon];

    if (!iconData) {
        return NO;
    }

    /*NSError *error = nil;
    BOOL success = [url setResourceValue:iconData forKey:NSURLCustomIconKey error:&error];

    if (!success) {
        NSLog(@"[KD] Error in setResourceValue: %@", error);
        return NO;
    }*/

    int xattrResult = setxattr([path cStringUsingEncoding:NSUTF8StringEncoding], "com.apple.ResourceFork", iconData.bytes,
                               iconData.length, 0, 0);
    if (xattrResult != 0) {
        NSLog(@" [KD] Error in setxattr: %s", strerror(errno));
    }

    // Force Finder to update
    [[NSWorkspace sharedWorkspace] noteFileSystemChanged:path];

    return YES;
}

+ (NSData *)createICNSDataFromImage:(NSImage *)image {
    if (!image) {
        return nil;
    }

    // Create bitmap representation
    NSBitmapImageRep *bitmapRep = [self bitmapImageRepFromImage:image];
    if (!bitmapRep) {
        return nil;
    }

    // Create ICNS data
    NSData *icnsData = [bitmapRep representationUsingType:NSBitmapImageFileTypePNG properties:@{}];

    return icnsData;
}

+ (NSBitmapImageRep *)bitmapImageRepFromImage:(NSImage *)image {
    NSSize size = image.size;

    if (size.width <= 0 || size.height <= 0) {
        return nil;
    }

    NSBitmapImageRep *bitmapRep = [[NSBitmapImageRep alloc] initWithBitmapDataPlanes:NULL
                                                                          pixelsWide:(NSInteger) size.width
                                                                          pixelsHigh:(NSInteger) size.height
                                                                       bitsPerSample:8
                                                                     samplesPerPixel:4
                                                                            hasAlpha:YES
                                                                            isPlanar:NO
                                                                      colorSpaceName:NSCalibratedRGBColorSpace
                                                                        bitmapFormat:NSBitmapFormatAlphaFirst
                                                                         bytesPerRow:0
                                                                        bitsPerPixel:0];

    if (!bitmapRep) {
        return nil;
    }

    [NSGraphicsContext saveGraphicsState];

    NSGraphicsContext *context = [NSGraphicsContext graphicsContextWithBitmapImageRep:bitmapRep];
    [NSGraphicsContext setCurrentContext:context];

    [image drawInRect:NSMakeRect(0, 0, size.width, size.height)
             fromRect:NSZeroRect
            operation:NSCompositingOperationCopy
             fraction:1.0];

    [NSGraphicsContext restoreGraphicsState];

    return bitmapRep;
}

@end
