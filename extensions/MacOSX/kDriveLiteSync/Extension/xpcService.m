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

#import "xpcService.h"

#define CSV_SEPARATOR @";"

@implementation XPCService

- (instancetype)init {
    self = [super init];

    _registeredFoldersMap = [[NSMutableDictionary alloc] init];
    _fetchThumbnailMap = [[NSMutableDictionary alloc] init];
    _fetchMap = [[NSMutableDictionary alloc] init];
    _userBlackListMap = [[NSMutableDictionary alloc] init];
    _fetchingAppArray = [[NSMutableArray alloc] init];

    [self initOpenWhiteListThumbnailSet];
    [self initOpenWhiteListSet];
    [self initOpenBlackListSet];

    NSBundle *extBundle = [NSBundle bundleForClass:[self class]];
    NSString *machServiceName = [extBundle objectForInfoDictionaryKey:@"NSEndpointSecurityMachServiceName"];
    _xpcServiceProxy = [[XPCServiceProxy alloc] initWithDelegate:self serviceName:machServiceName];

    [_xpcServiceProxy start];

    return self;
}

- (BOOL)isFileMonitored:(NSString *)filePath {
    for (NSString *path in _registeredFoldersMap) {
        if ([filePath hasPrefix:path]) {
            return TRUE;
        }
    }
    return FALSE;
}

- (void)initOpenWhiteListThumbnailSet {
    _defaultOpenWhiteListThumbnailSet =
            [NSSet setWithObjects:@"com.apple.quicklook.QuickLookUIService", // Quicklook
                                  @"com.apple.quicklook.satellite", // Quicklook
                                  @"com.apple.quicklook.thumbnail.ImageExtension", // Quicklook Thumbnail
                                  @"com.apple.quicklook.thumbnail.AudiovisualExtension", // Quicklook Thumbnail
                                  nil];
}

- (void)initOpenWhiteListSet {
    _defaultOpenWhiteListSet = [NSSet setWithObjects:@"com.apple.QuickLookDaemon", // Quicklook
                                                     @"com.apple.DesktopServicesHelper", // DesktopServicesHelper
                                                     nil];
}

- (void)initOpenBlackListSet {
    _defaultOpenBlackListSet =
            [NSSet setWithObjects:@"com.infomaniak.drive.desktopclient", @"kDrive_client",
                                  @"com.apple.finder", // Finder
                                  @"com.apple.mdworker", // Spotlight
                                  @"com.apple.mdworker_shared", // Spotlight
                                  @"com.apple.mds", // Spotlight
                                  @"com.apple.mdsync", // Spotlight
                                  @"com.apple.Spotlight", // Spotlight
                                  @"com.apple.quicklook.ThumbnailsAgent", // Quicklook
                                  @"com.apple.quicklook.externalSatellite.arm64", // Quicklook satellite
                                  @"com.apple.quicklook.externalSatellite.x86_64", // Quicklook satellite
                                  @"com.apple.iconservicesagent", // iconservicesagent
                                  @"com.apple.ScopedBookmarkAgent", // ScopedBookmarkAgent
                                  @"com.apple.XprotectFramework.AnalysisService", // Xprotect
                                  @"com.apple.coreservices.uiagent", // CoreServicesUIAgent
                                  @"com.apple.ls", // Commands
                                  @"com.apple.lsd", // Commands
                                  @"com.apple.fgrep", // Commands
                                  @"com.apple.find", // Commands
                                  @"com.apple.bsdtar", // Commands
                                  @"com.apple.zip", // Commands
                                  @"com.apple.git", // Commands
                                  @"com.apple.xattr", // Commands
                                  @"com.apple.bash", // Commands
                                  @"com.apple.rm", // Commands
                                  @"com.apple.filecoordinationd", // System-wide file access coordination
                                  @"com.apple.appkit.xpc.openAndSavePanelService", // openAndSavePanelService
                                  @"com.apple.appkit.xpc.documentPopoverViewService", // documentPopoverViewService
                                  @"com.apple.QuickLookThumbnailing.extension.ThumbnailExtension-macOS", // QuickLookThumbnailing
                                  @"com.apple.quicklook.thumbnail", // Quicklook Thumbnail
                                  @"com.apple.Terminal", // Terminal
                                  @"com.apple.system_profiler", // System profiler
                                  @"com.apple.path_helper", // help manage the PATH environment variable,
                                  @"com.apple.mediaanalysisd-access", // MediaAnalysis
                                  @"com.apple.mediaanalysisd", // MediaAnalysis
                                  @"com.apple.osascript", // osascript
                                  @"com.apple.AMPLibraryAgent", // AMPLibraryAgent
                                  @"com.apple.SceneKitQLThumbnailExtension", // SceneKit
                                  @"com.apple.ModelIO.AssetLoader", // Model I/O
                                  @"com.apple.security.codesign", // codesign
                                  @"com.apple.mdmclient", // MDM
                                  @"com.apple.warmd_agent", // Warmd
                                  @"com.apple.apfsd", // Apfsd
                                  nil];
}

- (BOOL)isAppInOpenWhiteListThumbnail:(NSString *)appSigningId {
    assert(_defaultOpenWhiteListThumbnailSet);

    return [_defaultOpenWhiteListThumbnailSet containsObject:appSigningId];
}

- (BOOL)isAppInOpenWhiteList:(NSString *)appSigningId {
    assert(_defaultOpenWhiteListSet);

    return [_defaultOpenWhiteListSet containsObject:appSigningId];
}

- (BOOL)matchGlob:(NSString *)string globToMatch:(NSString *)glob {
    if ([string isEqualToString:glob]) {
        return true;
    } else if ([glob hasSuffix:@"*"]) {
        NSString *tmp = [glob substringToIndex:glob.length - 1];
        if ([string hasPrefix:tmp]) {
            return true;
        }
    }

    return false;
}

- (BOOL)isAppInDefaultBlackList:(NSString *)appSigningId {
    assert(_defaultOpenBlackListSet);

    // Check if the app id belongs to the default blacklist list where prohibited ids are full app identifiers (e.g.,
    // "com.apple.finder").
    const BOOL isBlacklisted = [_defaultOpenBlackListSet containsObject:appSigningId];
    if (isBlacklisted) {
        return true;
    }

    return false;
}

- (BOOL)isAppInOpenBlackList:(NSString *)appSigningId filePath:(NSString *)filePath {
    // Check if app id belongs to default prohibition list
    BOOL forbidden = [self isAppInDefaultBlackList:appSigningId];

    assert(_userBlackListMap);

    if (!forbidden) {
        // Check if app id belongs to user exclusion list
        @synchronized(_registeredFoldersMap) {
            for (NSString *path in _registeredFoldersMap) {
                if ([filePath hasPrefix:path]) {
                    NSMutableSet<NSString *> *appIdSet = _registeredFoldersMap[path];
                    @synchronized(_userBlackListMap) {
                        for (NSString *appId in appIdSet) {
                            for (NSString *forbiddenProcessName in _userBlackListMap[appId]) {
                                if ([self matchGlob:appSigningId globToMatch:forbiddenProcessName]) {
                                    forbidden = true;
                                    break;
                                }
                            }

                            if (forbidden) {
                                break;
                            }
                        }
                    }
                    break;
                }
            }
        }
    }

    return forbidden;
}

- (void)addAppToFetchList:(NSString *)appSigningId {
    assert(_fetchingAppArray);

    @synchronized(_fetchingAppArray) {
        if (![_fetchingAppArray containsObject:appSigningId]) {
            NSLog(@"[KD] add fetching app to list %@", appSigningId);
            [_fetchingAppArray addObject:appSigningId];
        }
    }
}

- (void)sendMessage:(NSString *)filePath query:(NSString *)verb oneApp:(BOOL)one {
    assert(_registeredFoldersMap);

    NSLog(@"[KD] Send message %@:%@", verb, filePath);
    NSMutableSet<NSString *> *appIdSet = [[NSMutableSet<NSString *> alloc] init];
    @synchronized(_registeredFoldersMap) {
        for (NSString *path in _registeredFoldersMap) {
            if ([filePath hasPrefix:path]) {
                [appIdSet unionSet:[_registeredFoldersMap objectForKey:path]];
                if (one) {
                    break;
                }
            }
        }
    }

    for (NSString *appId in appIdSet) {
        [_xpcServiceProxy sendMessage:appId filePath:filePath query:verb];
    }
}

- (BOOL)fetchFile:(NSString *)filePath pid:(pid_t)pid {
    assert(_fetchMap);

    @synchronized(_fetchMap) {
        if (_fetchMap[filePath] == nil) {
            [_fetchMap setObject:[[NSMutableSet alloc] init] forKey:filePath];

            // Ask to the app to fetch the file
            [self sendMessage:filePath query:@"MAKE_AVAILABLE_LOCALLY_DIRECT" oneApp:TRUE];
        }

        // Store the pid
        [[_fetchMap objectForKey:filePath] addObject:[NSNumber numberWithInt:pid]];

        // Stop the process
        NSLog(@"[KD] Stop process %@ opening file %@", [NSNumber numberWithInt:pid], filePath);
        kill(pid, SIGSTOP);
    }

    return TRUE;
}

- (BOOL)fetchThumbnail:(NSString *)filePath pid:(pid_t)pid {
    assert(_fetchThumbnailMap);

    @synchronized(_fetchThumbnailMap) {
        if (_fetchThumbnailMap[filePath] == nil) {
            [_fetchThumbnailMap setObject:[[NSMutableSet alloc] init] forKey:filePath];

            // Ask to the app to fetch the thumbnail
            [self sendMessage:filePath query:@"SET_THUMBNAIL" oneApp:TRUE];
        }

        // Store the pid
        [[_fetchThumbnailMap objectForKey:filePath] addObject:[NSNumber numberWithInt:pid]];

        // Stop the process
        NSLog(@"[KD] Stop process %@ opening thumbnail %@", [NSNumber numberWithInt:pid], filePath);
        kill(pid, SIGSTOP);
    }

    return TRUE;
}

// XPCServiceProxyDelegate protocol implementation
- (void)registerFolder:(NSString *)appId folderPath:(NSString *)path {
    assert(_registeredFoldersMap);

    NSLog(@"[KD] Register folder %@", path);
    @synchronized(_registeredFoldersMap) {
        if ([_registeredFoldersMap objectForKey:path] == nil) {
            [_registeredFoldersMap setObject:[[NSMutableSet alloc] init] forKey:path];
        }

        [[_registeredFoldersMap objectForKey:path] addObject:appId];
    }
}

- (void)unregisterFolder:(NSString *)appId folderPath:(NSString *)path {
    assert(_registeredFoldersMap);

    NSLog(@"[KD] Unregister folder %@", path);
    @synchronized(_registeredFoldersMap) {
        if ([_registeredFoldersMap objectForKey:path] == nil) {
            NSLog(@"[KD] Warning: Folder not registered %@", path);
            return;
        }

        [[_registeredFoldersMap objectForKey:path] removeObject:appId];

        if ([[_registeredFoldersMap objectForKey:path] count] == 0) {
            [_registeredFoldersMap removeObjectForKey:path];
        }
    }
}

- (void)setAppExcludeList:(NSString *)appId appList:(NSString *)list {
    assert(_userBlackListMap);

    NSLog(@"[KD] Set App exclude list %@", list);
    @synchronized(_userBlackListMap) {
        [_userBlackListMap setObject:[NSSet setWithArray:[list componentsSeparatedByString:CSV_SEPARATOR]] forKey:appId];
    }
}

- (void)updateFetchStatus:(NSString *)appId filePath:(NSString *)filePath fileStatus:(NSString *)fileStatus {
    assert(_fetchMap);

    NSLog(@"[KD] updateFetchStatus %@ %@", filePath, fileStatus);
    @synchronized(_fetchMap) {
        if (_fetchMap[filePath] == nil) {
            NSLog(@"[KD] Warning: File not registered %@", filePath);
            return;
        }

        // Resume the stopped processes
        for (NSNumber *pidNumber in _fetchMap[filePath]) {
            NSLog(@"[KD] Resume process %@ opening file %@", pidNumber, filePath);
            kill([pidNumber intValue], SIGCONT);
        }

        // Remove file path from fetch map
        [_fetchMap removeObjectForKey:filePath];
    }
}

- (void)updateThumbnailFetchStatus:(NSString *)appId filePath:(NSString *)filePath fileStatus:(NSString *)fileStatus {
    assert(_fetchThumbnailMap);

    NSLog(@"[KD] updateThumbnailFetchStatus %@ %@", filePath, fileStatus);
    @synchronized(_fetchThumbnailMap) {
        if (_fetchThumbnailMap[filePath] == nil) {
            NSLog(@"[KD] Warning: File not registered %@", filePath);
            return;
        }

        // Resume the stopped processes
        for (NSNumber *pidNumber in _fetchThumbnailMap[filePath]) {
            NSLog(@"[KD] Resume process %@ opening thumbnail %@", pidNumber, filePath);
            kill([pidNumber intValue], SIGCONT);
        }

        // Remove file path from fetch map
        [_fetchThumbnailMap removeObjectForKey:filePath];
    }
}

- (void)connectionEnded:(NSString *)appId {
    assert(_registeredFoldersMap);

    NSLog(@"[KD] Connection ended");
    @synchronized(_registeredFoldersMap) {
        for (NSString *path in _registeredFoldersMap) {
            if ([[_registeredFoldersMap objectForKey:path] containsObject:appId]) {
                [[_registeredFoldersMap objectForKey:path] removeObject:appId];

                if ([[_registeredFoldersMap objectForKey:path] count] == 0) {
                    [_registeredFoldersMap removeObjectForKey:path];
                }
            }
        }
    }
}

- (NSMutableString *)getFetchingAppList {
    assert(_fetchingAppArray);

    NSLog(@"[KD] Fetching app list");
    NSMutableString *appList = [[NSMutableString alloc] init];
    @synchronized(_fetchingAppArray) {
        for (NSString *app in _fetchingAppArray) {
            if ([appList length] > 0) {
                [appList appendString:CSV_SEPARATOR];
            }
            [appList appendString:app];
        }
    }

    return appList;
}

- (BOOL)isDirectory:(NSString *)path error:(NSError *_Nullable *)error {
    NSDictionary *fileAttribs = [[NSFileManager defaultManager] attributesOfItemAtPath:path error:error];
    if (!fileAttribs) {
        return false;
    }

    NSString *fileType = [fileAttribs objectForKey:NSFileType];

    return fileType == NSFileTypeDirectory;
}

@end
