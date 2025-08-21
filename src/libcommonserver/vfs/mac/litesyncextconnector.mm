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

#import "litesyncextconnector.h"
#import "../extensions/MacOSX/kDriveLiteSync/Extension/xpcLiteSyncExtensionProtocol.h"
#import "../extensions/MacOSX/kDriveLiteSync/kDrive/xpcLiteSyncExtensionRemoteProtocol.h"
#include "libcommon/utility/types.h"
#include "libcommon/utility/utility.h"
#include "libcommonserver/io/iohelper.h"
#include "libcommonserver/io/filestat.h"
#include "libcommonserver/log/log.h"
#include "libcommonserver/utility/utility.h"

#include <QPixmap>

#import <Cocoa/Cocoa.h>
#import <SystemExtensions/SystemExtensions.h>
#import <Foundation/Foundation.h>

#include <sys/xattr.h>
#include <sys/stat.h>

#include <log4cplus/loggingmacros.h>


#define COPY_CHUNK_SIZE 4096 * 1000

#define CSV_SEPARATOR @";"

#define SYNC_STEPS 20

// SystemExtensionRequestDelegate
@interface SystemExtensionRequestDelegate : NSObject <OSSystemExtensionRequestDelegate> {
    dispatch_semaphore_t _endSemaphore;
}

@property BOOL error;
@property BOOL done;

- (intptr_t)wait;

@end


@implementation SystemExtensionRequestDelegate

- (id)init {
    if (self = [super init]) {
        _endSemaphore = dispatch_semaphore_create(0);
        _error = FALSE;
        _done = FALSE;
    }
    return self;
}

- (OSSystemExtensionReplacementAction)request:(nonnull OSSystemExtensionRequest *)request
                  actionForReplacingExtension:(nonnull OSSystemExtensionProperties *)existing
                                withExtension:(nonnull OSSystemExtensionProperties *)ext {
    NSLog(@"[KD] Replacing extension %@ version %@ with version %@", request.identifier, existing.bundleShortVersion,
          ext.bundleShortVersion);
    return OSSystemExtensionReplacementActionReplace;
}

- (void)request:(nonnull OSSystemExtensionRequest *)request didFailWithError:(nonnull NSError *)error {
    NSLog(@"[KD] ERROR: System extension request failed - %@", error.localizedDescription);
    _error = TRUE;
    dispatch_semaphore_signal(_endSemaphore);
}

- (void)request:(nonnull OSSystemExtensionRequest *)request didFinishWithResult:(OSSystemExtensionRequestResult)result {
    if (result == OSSystemExtensionRequestWillCompleteAfterReboot) {
        NSLog(@"[KD] System extension will apply after reboot - result=%ld", (long) result);
    } else {
        NSLog(@"[KD] Extension %@ registered", request.identifier);
    }
    _done = TRUE;
    dispatch_semaphore_signal(_endSemaphore);
}

- (void)requestNeedsUserApproval:(nonnull OSSystemExtensionRequest *)request {
    NSLog(@"[KD] Extension %@ requires user approval", request.identifier);
}

- (intptr_t)wait {
    return dispatch_semaphore_wait(_endSemaphore, DISPATCH_TIME_NOW);
}

@end


// Connector
@interface Connector : NSObject <XPCLiteSyncExtensionRemoteProtocol> {
    KDC::ExecuteCommand _executeCommand;
    NSString *_pId;
    BOOL _activationDone;
    BOOL _timeout;
    NSMutableSet *_pathSet;
}

@property(retain) NSXPCConnection *connection;

- (instancetype)init:(KDC::ExecuteCommand)executeCommand;
- (void)dealloc;
- (BOOL)installExt:(BOOL *)activationDone;
- (void)onTimeout:(NSTimer *)timer;
- (BOOL)connectToExt;
- (BOOL)reconnectToExt;
- (BOOL)registerFolder:(NSString *)folderPath;
- (BOOL)unregisterFolder:(NSString *)folderPath;
- (BOOL)setAppExcludeList:(NSString *)list;
- (BOOL)updateFetchStatus:(NSString *)path fileStatus:(NSString *)status;
- (BOOL)updateThumbnailFetchStatus:(NSString *)path fileStatus:(NSString *)status;
- (BOOL)getFetchingAppList:(NSMutableDictionary<NSString *, NSString *> **)appMap;
- (BOOL)setThumbnail:(NSString *)path image:(NSImage *)image;

@end


@implementation Connector

- (instancetype)init:(KDC::ExecuteCommand)executeCommand {
    if (self = [super init]) {
        _connection = nil;
        _executeCommand = executeCommand;

        NSProcessInfo *processInfo = [NSProcessInfo processInfo];
        _pId = [@([processInfo processIdentifier]) stringValue];
        _pathSet = [NSMutableSet set];
    }

    return self;
}

- (void)dealloc {
    NSLog(@"[KD] App terminating");
    if (_connection) {
        [_connection invalidate];
        _connection = nil;
    }
}

- (BOOL)installExt:(BOOL *)activationDone {
    *activationDone = FALSE;
    if (@available(macOS 10.15, *)) {
        // Read LiteSyncExtMachName from plist
        NSBundle *appBundle = [NSBundle mainBundle];
        NSString *liteSyncExtMachName = [appBundle objectForInfoDictionaryKey:@"LiteSyncExtMachName"];
        if (liteSyncExtMachName == nil) {
            NSLog(@"[KD] No LiteSyncExtMachName");
            return FALSE;
        }

        // Activate extension
        SystemExtensionRequestDelegate *delegate = [[SystemExtensionRequestDelegate alloc] init];

        OSSystemExtensionRequest *osreqInst = [OSSystemExtensionRequest activationRequestForExtension:liteSyncExtMachName
                                                                                                queue:dispatch_get_main_queue()];
        osreqInst.delegate = delegate;

        [OSSystemExtensionManager.sharedManager submitRequest:osreqInst];
        NSLog(@"[KD] Activation request sent");

        // Set timeout timer
        _timeout = FALSE;
        SEL timeoutSelector = sel_registerName("onTimeout:");
        [NSTimer scheduledTimerWithTimeInterval:5 target:self selector:timeoutSelector userInfo:nil repeats:FALSE];

        // Wait for answer or timeout
        while ([delegate wait] && _timeout == FALSE) {
            [[NSRunLoop currentRunLoop] runMode:NSDefaultRunLoopMode beforeDate:[NSDate dateWithTimeIntervalSinceNow:0]];
        }

        if (delegate.done) {
            NSLog(@"[KD] Activation done");
            *activationDone = TRUE;
            return TRUE;
        } else if (delegate.error) {
            NSLog(@"[KD] Activation request error");
            return FALSE;
        } else {
            NSLog(@"[KD] Activation request pending");
            return TRUE;
        }
    } else {
        NSLog(@"[KD] Need at least macOS 10.15");
        return FALSE;
    }

    return TRUE;
}

- (void)onTimeout:(NSTimer *)timer {
    NSLog(@"Installation timeout");
    _timeout = TRUE;
}

- (void)scheduleRetryToConnectToLiteSyncExt {
    dispatch_async(dispatch_get_main_queue(), ^{
      NSLog(@"[KD] Set timer to retry to connect to LiteSync extension");
      [NSTimer scheduledTimerWithTimeInterval:10 target:self selector:@selector(reconnectToExt) userInfo:nil repeats:NO];
    });
}

- (BOOL)connectToExt {
    if (_connection != nil) {
        // Already connected
        return TRUE;
    }

    // Setup connection with LiteSync extension
    NSLog(@"[KD] Setup connection with LiteSync extension");

    NSString *liteSyncExtMachName = nil;
    if (qApp) {
        // Read LiteSyncExtMachName from plist
        NSBundle *appBundle = [NSBundle mainBundle];
        liteSyncExtMachName = [appBundle objectForInfoDictionaryKey:@"LiteSyncExtMachName"];
        if (!liteSyncExtMachName) {
            NSLog(@"[KD] LiteSyncExtMachName undefined");
            return FALSE;
        }
    } else {
        // For testing
        liteSyncExtMachName = [NSString stringWithUTF8String:KDC::CommonUtility::liteSyncExtBundleId().c_str()];
    }

    _connection = [[NSXPCConnection alloc] initWithMachServiceName:liteSyncExtMachName options:0];
    if (_connection == nil) {
        NSLog(@"[KD] Failed to connect to LiteSync extension");
        return FALSE;
    }

    // Set exported interface
    NSLog(@"[KD] Set exported interface for connection with LiteSync extension");
    _connection.exportedInterface = [NSXPCInterface interfaceWithProtocol:@protocol(XPCLiteSyncExtensionRemoteProtocol)];
    _connection.exportedObject = self;

    // Set remote object interface
    NSLog(@"[KD] Set remote object interface for connection with LiteSync extension");
    _connection.remoteObjectInterface = [NSXPCInterface interfaceWithProtocol:@protocol(XPCLiteSyncExtensionProtocol)];

    // Set connection handlers
    NSLog(@"[KD] Setup connection handlers with LiteSync extension");
    __weak __typeof__(self) weakSelf = self;
    _connection.interruptionHandler = ^{
      // The LiteSync extension has exited or crashed
      NSLog(@"[KD] Connection with LiteSync extension interrupted");
      __strong __typeof__(weakSelf) strongSelf = weakSelf;
      if (strongSelf) {
          strongSelf->_connection = nil;
          [strongSelf scheduleRetryToConnectToLiteSyncExt];
      }
    };

    _connection.invalidationHandler = ^{
      // Connection can not be formed or has terminated and may not be re-established
      NSLog(@"[KD] Connection with LiteSync extension invalidated");
      __strong __typeof__(weakSelf) strongSelf = weakSelf;
      if (strongSelf) {
          strongSelf->_connection = nil;
          [strongSelf scheduleRetryToConnectToLiteSyncExt];
      }
    };

    // Resume connection
    NSLog(@"[KD] Resume connection with LiteSync extension");
    [_connection resume];

    return TRUE;
}

- (BOOL)reconnectToExt {
    if (![self connectToExt]) {
        NSLog(@"[KD] Failed to reconnect to LiteSync extension");
        return FALSE;
    }

    for (NSString *path in _pathSet) {
        if (![self registerFolder:path]) {
            NSLog(@"[KD] Failed to register folder %@", path);
            return FALSE;
        }
    }

    return TRUE;
}

- (BOOL)registerFolder:(NSString *)path {
    if (_connection) {
        [[_connection remoteObjectProxy] registerFolder:_pId folderPath:path];
        [_pathSet addObject:path];
        return TRUE;
    }

    return FALSE;
}

- (BOOL)unregisterFolder:(NSString *)path {
    if (_connection) {
        [[_connection remoteObjectProxy] unregisterFolder:_pId folderPath:path];
        [_pathSet removeObject:path];
        return TRUE;
    }

    return FALSE;
}

- (BOOL)setAppExcludeList:(NSString *)list {
    if (_connection) {
        [[_connection remoteObjectProxy] setAppExcludeList:_pId appList:list];
        return TRUE;
    }

    return FALSE;
}

- (BOOL)updateFetchStatus:(NSString *)path fileStatus:(NSString *)status {
    if (_connection) {
        [[_connection remoteObjectProxy] updateFetchStatus:_pId filePath:path fileStatus:status];
        return TRUE;
    }

    return FALSE;
}

- (BOOL)updateThumbnailFetchStatus:(NSString *)path fileStatus:(NSString *)status {
    if (_connection) {
        [[_connection remoteObjectProxy] updateThumbnailFetchStatus:_pId filePath:path fileStatus:status];
        return TRUE;
    }

    return FALSE;
}

- (BOOL)getFetchingAppList:(NSMutableDictionary<NSString *, NSString *> **)appMap {
    __block NSString *appList;
    __block NSCondition *doneCondition = [[NSCondition alloc] init];
    __block BOOL done = FALSE;
    if (_connection) {
        [[_connection remoteObjectProxy] getFetchingAppList:_pId
                                                   callback:^(NSString *appListTmp) {
                                                     [doneCondition lock];
                                                     appList = appListTmp;
                                                     done = TRUE;
                                                     [doneCondition signal];
                                                     [doneCondition unlock];
                                                   }];
    }

    // Wait for answer with 1 sec timeout
    [doneCondition lock];
    NSDate *timeoutDate = [NSDate dateWithTimeIntervalSinceNow:1];
    BOOL success = FALSE;
    while (!done && (success = [doneCondition waitUntilDate:timeoutDate])) {}
    [doneCondition unlock];

    if (!success) {
        NSLog(@"[KD] getFetchingAppList timeout");
        return FALSE;
    }

    // Create app map
    if (appList.length > 0) {
        *appMap = [[NSMutableDictionary alloc] init];
        NSArray *appArray = [appList componentsSeparatedByString:CSV_SEPARATOR];
        for (NSString *appId in appArray) {
            NSBundle *bundle = [NSBundle bundleWithIdentifier:appId];
            NSString *appName;
            if (bundle != NULL) {
                appName = [bundle objectForInfoDictionaryKey:(id) kCFBundleNameKey /*kCFBundleExecutableKey*/];
            } else {
                appName = @"";
            }
            [(*appMap) setObject:appName forKey:appId];
        }
    }

    return TRUE;
}

- (BOOL)setThumbnail:(NSString *)path image:(NSImage *)image {
    // Get modification date
    NSDictionary *fileAttribs = [[NSFileManager defaultManager] attributesOfItemAtPath:path error:NULL];
    if (!fileAttribs) {
        NSLog(@"[KD] Error in NSFileManager::attributesOfItemAtPath - path=%@", path);
        return false;
    }

    NSDate *modificationDate = [fileAttribs objectForKey:NSFileModificationDate];
    if (!modificationDate) {
        NSLog(@"[KD] Error in NSDictionary::objectForKey:NSFileModificationDate - path=%@", path);
        return false;
    }

    // Set icon (!!! touch modification date !!!)
    if (![[NSWorkspace sharedWorkspace] setIcon:image forFile:path options:0]) {
        NSLog(@"[KD] Error in NSWorkspace::setIcon - path=%@", path);
        return false;
    }

    // Set modification date
    NSDictionary *fileAttribModificationDate =
            [NSDictionary dictionaryWithObjectsAndKeys:modificationDate, NSFileModificationDate, NULL];
    if (!fileAttribModificationDate) {
        NSLog(@"[KD] Error in NSDictionary::dictionaryWithObjectsAndKeys - path=%@", path);
        return false;
    }

    if (![[NSFileManager defaultManager] setAttributes:fileAttribModificationDate ofItemAtPath:path error:NULL]) {
        NSLog(@"[KD] Error in NSFileManager::setAttributes - path=%@", path);
        return false;
    }

    return true;
}

// xpcLiteSyncExtensionRemoteProtocol protocol implementation
- (void)getAppId:(void (^)(NSString *))callback {
    NSLog(@"[KD] getAppId called");
    callback(_pId);
}

- (void)sendMessage:(NSData *)params {
    NSLog(@"[KD] sendMessage called");
    NSString *paramsStr = [[NSString alloc] initWithData:params encoding:NSUTF8StringEncoding];
    _executeCommand([paramsStr UTF8String]);
}

@end


namespace KDC {

LiteSyncExtConnector *LiteSyncExtConnector::_liteSyncExtConnector = nullptr;

class LiteSyncExtConnectorPrivate {
    public:
        LiteSyncExtConnectorPrivate(log4cplus::Logger logger, ExecuteCommand executeCommand);
        ~LiteSyncExtConnectorPrivate();

        bool install(bool &activationDone);
        bool connect();

        bool vfsInit();
        bool vfsStart(const SyncPath &folderPath);
        bool vfsStop(const SyncPath &folderPath);

        bool executeCommand(const std::string &commandLine);
        bool updateFetchStatus(const SyncPath &filePath, const std::string &status);
        bool setThumbnail(const SyncPath &filePath, const QPixmap &pixmap);
        bool setAppExcludeList(const std::string &appList);
        bool getFetchingAppList(std::unordered_map<std::string, std::string, StringHashFunction, std::equal_to<>> &appTable);

    private:
        log4cplus::Logger _logger;
        Connector *_connector = nullptr;
        ExecuteCommand _executeCommand = nullptr;
};

// LiteSyncExtConnectorPrivate implementation
LiteSyncExtConnectorPrivate::LiteSyncExtConnectorPrivate(log4cplus::Logger logger, ExecuteCommand executeCommand) :
    _logger(logger) {
    LOG_DEBUG(_logger, "LiteSyncExtConnectorPrivate creation");

    _connector = [[Connector alloc] init:executeCommand];
    _executeCommand = executeCommand;
}

LiteSyncExtConnectorPrivate::~LiteSyncExtConnectorPrivate() {}

bool LiteSyncExtConnectorPrivate::install(bool &activationDone) {
    // Install LiteSync extension
    BOOL activationDoneTmp = FALSE;
    if (![_connector installExt:&activationDoneTmp]) {
        LOG_ERROR(_logger, "Error in installExt!");
        return false;
    }

    activationDone = activationDoneTmp;
    return true;
}

bool LiteSyncExtConnectorPrivate::connect() {
    // Connect to LiteSync extension
    if (![_connector connectToExt]) {
        LOG_ERROR(_logger, "Error in connectToExt!");
        return false;
    }

    return true;
}

namespace {
enum LogMode : bool {
    Debug = true,
    Warn = false
};
//! Returns true and logs the specified IO error if it pertains to the existence of the folder or to its permissions.
/*!
  \param ioError is the IO error that has been raised.
  \param itemType is the type of the item subject to an IO error (e.g., "file" or "Sync folder").
  \param path is the file system path of the item subject to an IO error.
  \param logger is the logger used to log the error message.
  \param debug is an optional boolean value indicating whether the logging mode is DEBUG or WARN. Defaults to true, i.e., DEBUG
  mode. \return true if the IO error pertains to the existence of the folder or to its permissions, false otherwise.
*/
bool checkIoErrorAndLogIfNeeded(IoError ioError, const std::string &itemType, const SyncPath &path, log4cplus::Logger &logger,
                                LogMode mode = LogMode::Debug) {
    if (ioError != IoError::NoSuchFileOrDirectory && ioError != IoError::AccessDenied) {
        return false;
    }

    std::wstring message;
    if (ioError == IoError::NoSuchFileOrDirectory) {
        message = CommonUtility::s2ws(itemType + " doesn't exist: ");
    }
    if (ioError == IoError::AccessDenied) {
        message = CommonUtility::s2ws(itemType + " or some containing directory, misses a permission: ");
    }

    switch (mode) {
        case LogMode::Debug:
            LOGW_DEBUG(logger, message << Utility::formatSyncPath(path));
            break;
        case LogMode::Warn:
            LOGW_WARN(logger, message << Utility::formatSyncPath(path));
            break;
    }

    return true;
}
} // namespace

bool LiteSyncExtConnectorPrivate::vfsStart(const SyncPath &folderPath) {
    if (!_connector) {
        LOG_ERROR(_logger, "Connector not initialized!");
        return false;
    }

    NSString *nsPath = [NSString stringWithCString:folderPath.c_str() encoding:NSUTF8StringEncoding];
    if (![_connector registerFolder:nsPath]) {
        LOG_ERROR(_logger, "Cannot register folder!");
        return false;
    }

    // Read folder status
    std::string value;
    IoError ioError = IoError::Success;
    const bool result = IoHelper::getXAttrValue(folderPath, litesync_attrs::status, value, ioError);
    if (!result) {
        LOGW_WARN(_logger, L"Error in getXAttrValue: " << Utility::formatIoError(folderPath, ioError));
        return false;
    }

    if (checkIoErrorAndLogIfNeeded(ioError, "Sync folder", folderPath, _logger, LogMode::Warn)) {
        return false;
    }

    if (value.empty()) {
        // Set default folder status
        if (!IoHelper::setXAttrValue(folderPath, litesync_attrs::status, litesync_attrs::statusOnline, ioError)) {
            LOGW_WARN(_logger, L"Error in setXAttrValue: " << Utility::formatIoError(folderPath, ioError));
            return false;
        }

        if (checkIoErrorAndLogIfNeeded(ioError, "Sync folder", folderPath, _logger, LogMode::Warn)) {
            return false;
        }

        // Set default folder pin state
        if (!IoHelper::setXAttrValue(folderPath, litesync_attrs::pinState, litesync_attrs::pinStateUnpinned, ioError)) {
            LOGW_WARN(_logger, L"Error in setXAttrValue: " << Utility::formatIoError(folderPath, ioError));
            return false;
        }

        if (checkIoErrorAndLogIfNeeded(ioError, "Sync folder", folderPath, _logger, LogMode::Warn)) {
            return false;
        }
    }

    return true;
}

bool LiteSyncExtConnectorPrivate::vfsStop(const SyncPath &folderPath) {
    if (!_connector) {
        LOG_ERROR(_logger, "Connector not initialized!");
        return false;
    }

    NSString *nsPath = [NSString stringWithCString:folderPath.c_str() encoding:NSUTF8StringEncoding];
    if (![_connector unregisterFolder:nsPath]) {
        LOG_ERROR(_logger, "Cannot unregister folder!");
        return false;
    }

    return true;
}

bool LiteSyncExtConnectorPrivate::executeCommand(const std::string &commandLine) {
    if (!_connector) {
        LOG_WARN(_logger, "Connector not initialized!");
        return false;
    }

    if (!_executeCommand) {
        LOG_WARN(_logger, "No execute command!");
        return false;
    }

    LOGW_DEBUG(_logger, L"Execute command: " << CommonUtility::s2ws(commandLine));
    _executeCommand(commandLine.c_str());

    return true;
}

bool LiteSyncExtConnectorPrivate::updateFetchStatus(const SyncPath &filePath, const std::string &status) {
    if (!_connector) {
        LOG_WARN(_logger, "Connector not initialized!");
        return false;
    }

    NSString *nsPath = [NSString stringWithUTF8String:filePath.c_str()];
    NSString *nsStatus = [NSString stringWithUTF8String:status.c_str()];
    if (![_connector updateFetchStatus:nsPath fileStatus:nsStatus]) {
        LOGW_ERROR(_logger, L"Call to updateFetchStatus failed: " << Utility::formatSyncPath(filePath));
        return false;
    }

    return true;
}

bool LiteSyncExtConnectorPrivate::setThumbnail(const SyncPath &filePath, const QPixmap &pixmap) {
    if (!_connector) {
        LOG_WARN(_logger, "Connector not initialized!");
        return false;
    }

    // Source image
    bool error = false;
    CGImageRef imageRef = pixmap.toImage().toCGImage();
    NSImage *srcImage = [[NSImage alloc] initWithCGImage:imageRef size:pixmap.size().toCGSize()];
    CGImageRelease(imageRef);
    if (srcImage == nil) {
        LOGW_WARN(_logger, L"Call to initWithCGImage failed: " << Utility::formatSyncPath(filePath));
        error = true;
    }

    NSString *nsPath = [NSString stringWithUTF8String:filePath.c_str()];
    if (!error) {
        // Destination image
        int size = static_cast<int>(qMax(srcImage.size.width, srcImage.size.height));
        NSImage *dstImage = [[NSImage alloc] initWithSize:CGSizeMake(size, size)];

        // Copy source image into destination image
        NSRect dstRect = NSMakeRect(size == srcImage.size.width ? 0 : (size - srcImage.size.width) / 2,
                                    size == srcImage.size.width ? (size - srcImage.size.height) / 2 : 0, srcImage.size.width,
                                    srcImage.size.height);
        [dstImage lockFocus];
        [srcImage drawInRect:dstRect fromRect:NSZeroRect operation:NSCompositingOperationCopy fraction:1.0];
        [dstImage unlockFocus];

        if (![_connector setThumbnail:nsPath image:dstImage]) {
            LOGW_WARN(_logger, L"Call to setThumbnail failed: " << Utility::formatSyncPath(filePath));
            error = true;
        }
    }

    if (![_connector updateThumbnailFetchStatus:nsPath fileStatus:(error ? QString("KO") : QString("OK")).toNSString()]) {
        LOGW_ERROR(_logger, L"Call to updateThumbnailFetchStatus failed: " << Utility::formatSyncPath(filePath));
        error = true;
    }

    return !error;
}

bool LiteSyncExtConnectorPrivate::setAppExcludeList(const std::string &appList) {
    if (!_connector) {
        LOG_WARN(_logger, "Connector not initialized!");
        return false;
    }

    NSString *nsAppList = [NSString stringWithUTF8String:appList.c_str()];
    if (![_connector setAppExcludeList:nsAppList]) {
        LOG_ERROR(_logger, "Error in setAppExcludeList!");
        return false;
    }

    return true;
}

bool LiteSyncExtConnectorPrivate::getFetchingAppList(
        std::unordered_map<std::string, std::string, StringHashFunction, std::equal_to<>> &appTable) {
    if (!_connector) {
        LOG_WARN(_logger, "Connector not initialized!");
        return false;
    }

    NSMutableDictionary<NSString *, NSString *> *appMap;
    if (![_connector getFetchingAppList:&appMap]) {
        LOG_WARN(_logger, "Error in getFetchingAppList!");
        return false;
    }

    for (NSString *appId in appMap) {
        appTable[[appId UTF8String]] = [appMap[appId] UTF8String];
    }

    return true;
}

// LiteSyncExtConnector implementation
LiteSyncExtConnector::LiteSyncExtConnector(log4cplus::Logger logger, ExecuteCommand executeCommand) :
    _logger(logger) {
    LOG_DEBUG(_logger, "LiteSyncExtConnector creation");

    _private = new LiteSyncExtConnectorPrivate(logger, executeCommand);
    if (!_private) {
        LOG_ERROR(_logger, "Error in LiteSyncExtConnectorPrivate constructor");
        throw std::runtime_error("Error in LiteSyncExtConnectorPrivate constructor!");
    }
}

LiteSyncExtConnector *LiteSyncExtConnector::instance(log4cplus::Logger logger, ExecuteCommand executeCommand) {
    if (_liteSyncExtConnector == nullptr) {
        try {
            _liteSyncExtConnector = new LiteSyncExtConnector(logger, executeCommand);
        } catch (std::exception const &) {
            assert(false);
            return nullptr;
        }
    }

    return _liteSyncExtConnector;
}

LiteSyncExtConnector::~LiteSyncExtConnector() {
    delete _private;
    _private = nullptr;
}

bool LiteSyncExtConnector::install(bool &activationDone) {
    if (!_private) {
        return false;
    }

    return _private->install(activationDone);
}

bool LiteSyncExtConnector::connect() {
    if (!_private) {
        return false;
    }

    return _private->connect();
}

bool LiteSyncExtConnector::vfsStart(int syncDbId, const SyncPath &folderPath, bool &isPlaceholder, bool &isSyncing) {
    if (!_private) {
        return false;
    }

    if (!_private->vfsStart(folderPath)) {
        return false;
    }

    _folders[syncDbId] = folderPath;

    // Send root status to Finder
    VfsStatus vfsStatus;
    if (!vfsGetStatus(folderPath, vfsStatus)) {
        return false;
    }
    isPlaceholder = vfsStatus.isPlaceholder;
    isSyncing = vfsStatus.isSyncing;

    if (!sendStatusToFinder(folderPath, VfsStatus({.isHydrated = vfsStatus.isHydrated,
                                                   .isSyncing = vfsStatus.isSyncing,
                                                   .progress = static_cast<int16_t>(vfsStatus.isSyncing ? 100 : 0)}))) {
        LOGW_WARN(_logger, L"Error in sendStatusToFinder: " << Utility::formatSyncPath(folderPath));
        return false;
    }

    return true;
}

bool LiteSyncExtConnector::vfsStop(int syncDbId) {
    if (!_private) {
        return false;
    }

    if (!_folders.contains(syncDbId)) {
        LOG_WARN(_logger, "Folder not registered!");
        return false;
    }

    const auto path = _folders[syncDbId];
    (void) _folders.erase(syncDbId);

    return _private->vfsStop(path);
}

bool LiteSyncExtConnector::vfsHydratePlaceHolder(const SyncPath &filePath) {
    if (!_private) {
        return false;
    }

    // Read status
    std::string value;
    IoError ioError = IoError::Success;
    const bool result = IoHelper::getXAttrValue(filePath, litesync_attrs::status, value, ioError);
    if (!result) {
        LOGW_WARN(_logger, L"Error in getXAttrValue: " << Utility::formatIoError(filePath, ioError));
        return false;
    }

    if (checkIoErrorAndLogIfNeeded(ioError, "File", filePath, _logger)) {
        return false;
    }

    if (value.empty()) {
        LOGW_WARN(_logger, L"File is not a placeholder: " << Utility::formatSyncPath(filePath));
        return false;
    }

    if (value == litesync_attrs::statusOffline) {
        // Get file
        LOGW_DEBUG(_logger, L"Get file with " << Utility::formatSyncPath(filePath));
        const std::string msg = std::format("MAKE_AVAILABLE_LOCALLY_DIRECT:{}", Path2Str(filePath));
        _private->executeCommand(msg);
    }

    return true;
}

bool LiteSyncExtConnector::vfsDehydratePlaceHolder(const SyncPath &absoluteFilepath, const SyncPath &localSyncPath) {
    // Read status
    std::string value;
    IoError ioError = IoError::Success;
    const bool result = IoHelper::getXAttrValue(absoluteFilepath, litesync_attrs::status, value, ioError);
    if (!result) {
        LOGW_WARN(_logger, L"Error in getXAttrValue: " << Utility::formatSyncPath(absoluteFilepath));
        return false;
    }

    if (checkIoErrorAndLogIfNeeded(ioError, "File", absoluteFilepath, _logger)) {
        return false;
    }

    if (value.empty()) {
        LOGW_WARN(_logger, L"File is not a placeholder: " << Utility::formatSyncPath(absoluteFilepath));
        return false;
    }

    struct stat fileStat;
    if (stat(absoluteFilepath.c_str(), &fileStat) == -1) {
        LOGW_WARN(_logger, L"Call to stat failed: " << Utility::formatErrno(absoluteFilepath, errno));
        return false;
    }

    // Empty file
    int fd = open(absoluteFilepath.c_str(), O_WRONLY);
    if (fd == -1) {
        fd = open(absoluteFilepath.c_str(), O_WRONLY);
        if (fd == -1) {
            LOGW_WARN(_logger, L"Call to open failed: " << Utility::formatErrno(absoluteFilepath, errno));
            return false;
        }
    }

    if (ftruncate(fd, 0) == -1) {
        LOGW_WARN(_logger, L"Call to ftruncate failed: " << Utility::formatErrno(absoluteFilepath, errno));
        return false;
    }

    if (ftruncate(fd, fileStat.st_size) == -1) {
        LOGW_WARN(_logger, L"Call to ftruncate failed: " << Utility::formatErrno(absoluteFilepath, errno));
        return false;
    }

    if (close(fd) == -1) {
        LOGW_WARN(_logger, L"Call to close failed: " << Utility::formatErrno(absoluteFilepath, errno));
        return false;
    }

    // Set times
    static struct timespec tspec[2];
    tspec[0] = fileStat.st_atimespec;
    tspec[1] = fileStat.st_mtimespec;
    if (utimensat(0, absoluteFilepath.c_str(), tspec, 0) == -1) {
        LOGW_WARN(_logger, L"Call to utimensat failed: " << Utility::formatErrno(absoluteFilepath, errno));
        return false;
    }

    // Set status
    VfsStatus vfsStatus;
    if (!vfsSetStatus(absoluteFilepath, localSyncPath, vfsStatus)) {
        return false;
    }

    return true;
}

bool LiteSyncExtConnector::vfsSetPinState(const SyncPath &path, const SyncPath &localSyncPath, const std::string_view &pinState) {
    IoError ioError = IoError::Success;
    if (!IoHelper::setXAttrValue(path, litesync_attrs::pinState, pinState, ioError)) {
        LOGW_WARN(_logger, L"Call to setXAttrValue failed: " << Utility::formatErrno(path, errno));
        return false;
    }
    if (checkIoErrorAndLogIfNeeded(ioError, "Item", path, _logger)) {
        return false;
    }

    // Set status if empty directory
    bool isDirectory = false;
    ioError = IoHelper::checkIfIsDirectory(path, isDirectory);
    if (ioError != IoError::Success) {
        LOGW_WARN(_logger, L"Call to IoHelper::checkIfIsDirectory failed: " << Utility::formatIoError(path, ioError));
        return false;
    }
    if (!isDirectory) return true;

    IoHelper::DirectoryIterator dirIt(path, false, ioError);
    bool endOfDir = false;
    DirectoryEntry entry;
    bool foundChild = false;
    while (dirIt.next(entry, endOfDir, ioError) && !endOfDir && ioError == IoError::Success) {
        std::string tmpPinState;
        if (!vfsGetPinState(entry.path(), tmpPinState)) {
            continue;
        }
        if (tmpPinState == litesync_attrs::pinStateExcluded) {
            continue;
        }
        foundChild = true;
        break;
    }

    if (!foundChild) {
        // Set status
        VfsStatus vfsStatus = {.isHydrated = pinState == litesync_attrs::pinStatePinned};
        if (!vfsSetStatus(path, localSyncPath, vfsStatus)) {
            return false;
        }
    }
    return true;
}

bool LiteSyncExtConnector::vfsGetPinState(const SyncPath &path, std::string &pinState) {
    // Read pin state
    std::string value;
    IoError ioError = IoError::Success;
    const bool result = IoHelper::getXAttrValue(path, litesync_attrs::pinState, value, ioError);
    if (!result) {
        LOGW_WARN(_logger, L"Error in getXAttrValue: " << Utility::formatIoError(path, ioError));
        return false;
    }

    if (checkIoErrorAndLogIfNeeded(ioError, "Item", path, _logger)) {
        return false;
    }

    if (value.empty()) {
        LOGW_DEBUG(_logger, L"Item is not a placeholder: " << Utility::formatSyncPath(path));
        return false;
    }

    pinState = value[0];

    return true;
}

bool LiteSyncExtConnector::vfsConvertToPlaceHolder(const SyncPath &filePath, bool isHydrated) {
    // Set status
    const std::string_view status = (isHydrated ? litesync_attrs::statusOffline : litesync_attrs::statusOnline);
    IoError ioError = IoError::Success;
    if (!IoHelper::setXAttrValue(filePath, litesync_attrs::status, status, ioError)) {
        LOGW_WARN(_logger, L"Call to setXAttrValue failed: " << Utility::formatIoError(filePath, ioError));
        return false;
    }

    if (checkIoErrorAndLogIfNeeded(ioError, "Item", filePath, _logger)) {
        return false;
    }

    // Set pin state
    const std::string_view pinState = (isHydrated ? litesync_attrs::pinStatePinned : litesync_attrs::pinStateUnpinned);
    if (!IoHelper::setXAttrValue(filePath, litesync_attrs::pinState, pinState, ioError)) {
        LOGW_WARN(_logger, L"Call to setXAttrValue failed: " << Utility::formatIoError(filePath, ioError));
        return false;
    }

    if (checkIoErrorAndLogIfNeeded(ioError, "Item", filePath, _logger)) {
        return false;
    }

    return true;
}

bool LiteSyncExtConnector::vfsCreatePlaceHolder(const SyncPath &relativePath, const SyncPath &localSyncPath,
                                                const struct stat *fileStat) {
    if (!fileStat) {
        LOG_WARN(_logger, "Bad parameters");
        return false;
    }

    const auto absolutePath = localSyncPath / relativePath;

    if (fileStat->st_mode == S_IFDIR) {
        IoError ioError = IoError::Success;
        if (!IoHelper::createDirectory(absolutePath, false, ioError)) {
            LOGW_WARN(_logger, L"Call to IoHelper::createDirectory failed: " << Utility::formatSyncPath(absolutePath));
            return false;
        }

        if (ioError != IoError::Success) {
            LOGW_WARN(_logger, L"Failed to create directory: " << Utility::formatIoError(absolutePath, ioError));
            return false;
        }
    } else if (fileStat->st_mode == S_IFREG) {
        int flags = O_CREAT | FWRITE | O_NONBLOCK;
        int mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH; // Rights rw-r--r--

        int fd = open(absolutePath.c_str(), flags, mode);
        if (fd == -1) {
            LOGW_WARN(_logger, L"Call to open failed: " << Utility::formatErrno(absolutePath, errno));
            return false;
        }

        if (fileStat->st_mode == S_IFREG) {
            if (ftruncate(fd, fileStat->st_size) == -1) {
                LOGW_WARN(_logger, L"Call to ftruncate failed: " << Utility::formatErrno(absolutePath, errno));
                close(fd);
                return false;
            }
        }

        if (close(fd) == -1) {
            LOGW_WARN(_logger, L"Call to close failed: " << Utility::formatErrno(absolutePath, errno));
            return false;
        }
    }

    // Set status
    IoError ioError = IoError::Success;
    if (!IoHelper::setXAttrValue(absolutePath, litesync_attrs::status, litesync_attrs::statusOnline, ioError)) {
        LOGW_WARN(_logger, L"Call to setXAttrValue failed: " << Utility::formatIoError(absolutePath, ioError));
        return false;
    }

    if (checkIoErrorAndLogIfNeeded(ioError, "Item", absolutePath, _logger)) {
        return false;
    }

    // Set pin state
    if (!IoHelper::setXAttrValue(absolutePath, litesync_attrs::pinState, litesync_attrs::pinStateUnpinned, ioError)) {
        LOGW_WARN(_logger, L"Call to setXAttrValue failed: " << Utility::formatIoError(absolutePath, ioError));
        return false;
    }

    if (checkIoErrorAndLogIfNeeded(ioError, "Item", absolutePath, _logger)) {
        return false;
    }

    // Set file dates
    if (const IoError tmpIoError =
                IoHelper::setFileDates(absolutePath, fileStat->st_birthtimespec.tv_sec, fileStat->st_mtimespec.tv_sec, false);
        tmpIoError == IoError::NoSuchFileOrDirectory) {
        LOGW_DEBUG(_logger, L"Item doesn't exist: " << Utility::formatSyncPath(absolutePath));
        return false;
    } else if (ioError == IoError::AccessDenied) {
        LOGW_WARN(_logger, L"Access denied to " << Utility::formatSyncPath(absolutePath));
        return false;
    } else if (tmpIoError != IoError::Success) {
        LOGW_WARN(_logger, L"Call to IoHelper::setFileDates failed: " << Utility::formatSyncPath(absolutePath));
        return false;
    }

    // Update parent directory status if needed
    VfsStatus vfsStatus;
    if (!vfsGetStatus(absolutePath, vfsStatus)) {
        return false;
    }

    if (vfsStatus.isHydrated) {
        return vfsProcessDirStatus(absolutePath.parent_path(), localSyncPath);
    }

    return true;
}

bool LiteSyncExtConnector::vfsUpdateFetchStatus(const SyncPath &tmpFilePath, const SyncPath &filePath,
                                                const SyncPath &localSyncPath, unsigned long long completed, bool &canceled,
                                                bool &finished) {
    canceled = false;

    @autoreleasepool {
        // Get file attributes
        NSError *error = nil;
        NSString *nsPath = [NSString stringWithUTF8String:filePath.c_str()];
        NSDictionary<NSFileAttributeKey, id> *attributes = [[NSFileManager defaultManager] attributesOfItemAtPath:nsPath
                                                                                                            error:&error];
        NSInteger errorCode = error ? error.code : 0;
        if (error) {
            LOGW_WARN(_logger, L"Failed to get attributes: " << Utility::formatErrno(filePath, errorCode));
            return false;
        }

        unsigned long long fileSize = [attributes fileSize];
        finished = (completed == fileSize);
        if (finished) {
            // Get file dates
            IoError ioError = IoError::Success;
            FileStat filestat;
            if (!IoHelper::getFileStat(filePath, &filestat, ioError)) {
                LOGW_WARN(_logger, L"Error in IoHelper::getFileStat: " << Utility::formatIoError(filePath, ioError));
                return false;
            }
            if (ioError == IoError::NoSuchFileOrDirectory) {
                LOGW_WARN(_logger, L"Item doesn't exist: " << Utility::formatSyncPath(filePath));
                return false;
            } else if (ioError == IoError::AccessDenied) {
                LOGW_WARN(_logger, L"Access denied to " << Utility::formatSyncPath(filePath));
                return false;
            }

            SyncTime modificationDate = filestat.modificationTime;
            SyncTime creationDate = filestat.creationTime;

            // Copy tmp file content to file
            @try {
                LOGW_INFO(_logger, L"Copying temp file from " << Utility::formatSyncPath(tmpFilePath) << L" to "
                                                              << Utility::formatSyncPath(filePath));

                NSString *nsTmpPath = [NSString stringWithUTF8String:tmpFilePath.c_str()];
                NSFileHandle *tmpFileHandle = [NSFileHandle fileHandleForReadingAtPath:nsTmpPath];
                NSFileHandle *fileHandle = [NSFileHandle fileHandleForWritingAtPath:nsPath];
                NSData *buffer = nil;

                while (true) {
                    @autoreleasepool {
                        if ((buffer = [tmpFileHandle readDataUpToLength:COPY_CHUNK_SIZE error:&error]) == nil) {
                            errorCode = error ? error.code : 0;
                            LOGW_ERROR(_logger,
                                       L"Error while reading tmp file: " << Utility::formatErrno(tmpFilePath, errorCode));
                            break;
                        }
                        errorCode = error ? error.code : 0;
                        if (buffer.length == 0) {
                            // Nothing else to read
                            break;
                        }

                        if (![fileHandle writeData:buffer error:&error]) {
                            errorCode = error ? error.code : 0;
                            LOGW_ERROR(_logger, L"Error while writing to file: " << Utility::formatErrno(filePath, errorCode));
                            break;
                        }
                    }
                }

                [tmpFileHandle closeFile];
                [fileHandle closeFile];
            } @catch (NSException *e) {
                LOGW_WARN(_logger, L"Could not copy tmp file from " << Utility::formatSyncPath(tmpFilePath) << L" to "
                                                                    << Utility::formatSyncPath(filePath) << L" err="
                                                                    << CommonUtility::s2ws(std::string([e.name UTF8String])));
                return false;
            }

            // Set attributes
            if (![[NSFileManager defaultManager] setAttributes:attributes ofItemAtPath:nsPath error:&error]) {
                errorCode = error ? error.code : 0;
                LOGW_WARN(_logger, L"Could not set attributes to file: " << Utility::formatErrno(filePath, errorCode));
                return false;
            }

            // Set status
            VfsStatus vfsStatus = {.isHydrated = true};
            if (!vfsSetStatus(filePath, localSyncPath, vfsStatus)) {
                return false;
            }

            if (!_private->updateFetchStatus(filePath, "OK")) {
                LOGW_WARN(_logger, L"Call to updateFetchStatus failed: " << Utility::formatSyncPath(filePath));
                return false;
            }

            // Set file dates
            if (ioError = IoHelper::setFileDates(filePath, creationDate, modificationDate, false);
                ioError == IoError::NoSuchFileOrDirectory) {
                LOGW_DEBUG(_logger, L"Item doesn't exist: " << Utility::formatSyncPath(filePath));
                return false;
            } else if (ioError == IoError::AccessDenied) {
                LOGW_WARN(_logger, L"Access denied to " << Utility::formatSyncPath(filePath));
                return false;
            } else if (ioError != IoError::Success) {
                LOGW_WARN(_logger, L"Call to IoHelper::setFileDates failed: " << Utility::formatSyncPath(filePath));
                return false;
            }
        } else {
            // Set status
            VfsStatus vfsStatus = {.isSyncing = true, .progress = static_cast<int16_t>(ceil(float(completed) / fileSize * 100))};
            if (!vfsSetStatus(filePath, localSyncPath, vfsStatus)) {
                return false;
            }
        }
    }

    return true;
}

bool LiteSyncExtConnector::vfsUpdateFetchStatus(const SyncPath &absolutePath, const std::string &status) {
    if (!_private->updateFetchStatus(absolutePath, status)) {
        LOGW_WARN(_logger, L"Call to updateFetchStatus failed: " << Utility::formatSyncPath(absolutePath));
        return false;
    }
    return true;
}

bool LiteSyncExtConnector::vfsCancelHydrate(const SyncPath &filePath) {
    if (!_private->updateFetchStatus(filePath, "CANCEL")) {
        LOGW_WARN(_logger, L"Call to updateFetchStatus failed: " << Utility::formatSyncPath(filePath));
        return false;
    }
    return true;
}

bool LiteSyncExtConnector::vfsSetThumbnail(const SyncPath &absoluteFilePath, const QPixmap &pixmap) {
    // Set thumbnail
    if (!_private->setThumbnail(absoluteFilePath, pixmap)) {
        LOGW_WARN(_logger, L"Call to setThumbnail failed: " << Utility::formatSyncPath(absoluteFilePath));
    }

    return true;
}

bool LiteSyncExtConnector::vfsGetStatus(const SyncPath &absoluteFilePath, VfsStatus &vfsStatus,
                                        log4cplus::Logger &logger) noexcept {
    constexpr auto isExpectedError = [](IoError error) -> bool {
        return error == IoError::Success || error == IoError::AttrNotFound || error == IoError::NoSuchFileOrDirectory;
    };

    // Read status
    std::string value;
    IoError ioError = IoError::Success;
    if (!IoHelper::getXAttrValue(absoluteFilePath, litesync_attrs::status, value, ioError) && !isExpectedError(ioError)) {
        LOGW_WARN(logger, L"Error in getXAttrValue: " << Utility::formatIoError(absoluteFilePath, ioError));
        return false;
    }

    if (value.empty()) return true;

    vfsStatus.isPlaceholder = true;
    vfsStatus.isHydrated = (value == litesync_attrs::statusOffline);
    vfsStatus.isSyncing = value.starts_with(litesync_attrs::statusHydrating);

    if (vfsStatus.isSyncing) {
        vfsStatus.progress = static_cast<int16_t>(std::stoi(value.substr(litesync_attrs::statusHydrating.length())));
    }

    return true;
}

bool LiteSyncExtConnector::vfsSetAppExcludeList(const std::string &appList) {
    if (!_private) {
        return false;
    }

    return _private->setAppExcludeList(appList);
}

bool LiteSyncExtConnector::vfsGetFetchingAppList(
        std::unordered_map<std::string, std::string, StringHashFunction, std::equal_to<>> &appTable) {
    if (!_private) {
        return false;
    }

    return _private->getFetchingAppList(appTable);
}

bool LiteSyncExtConnector::vfsUpdateMetadata(const SyncPath &absoluteFilePath, const struct stat *fileStat) {
    if (!fileStat) {
        LOG_WARN(_logger, "Bad parameters");
        return false;
    }

    // Check status
    VfsStatus vfsStatus;
    vfsGetStatus(absoluteFilePath, vfsStatus);

    if (!vfsStatus.isPlaceholder || vfsStatus.isHydrated || vfsStatus.isSyncing) {
        return true;
    }

    // Create sparse file with rights rw-r--r--
    int fd = open(absoluteFilePath.c_str(), FWRITE);
    if (fd == -1) {
        fd = open(absoluteFilePath.c_str(), FWRITE);
        if (fd == -1) {
            LOGW_WARN(_logger, L"Call to open failed - " << Utility::formatErrno(absoluteFilePath, errno));
            return false;
        }
    }

    if (ftruncate(fd, fileStat->st_size) == -1) {
        LOGW_WARN(_logger, L"Call to ftruncate failed - " << Utility::formatErrno(absoluteFilePath, errno));
        return false;
    }

    if (close(fd) == -1) {
        LOGW_WARN(_logger, L"Call to close failed - " << Utility::formatErrno(absoluteFilePath, errno));
        return false;
    }

    // Set file dates
    if (const IoError ioError =
                IoHelper::setFileDates(absoluteFilePath, fileStat->st_birthtimespec.tv_sec, fileStat->st_mtimespec.tv_sec, false);
        ioError == IoError::NoSuchFileOrDirectory) {
        LOGW_DEBUG(_logger, L"Item doesn't exist: " << Utility::formatSyncPath(absoluteFilePath));
        return false;
    } else if (ioError == IoError::AccessDenied) {
        LOGW_WARN(_logger, L"Access denied to " << Utility::formatSyncPath(absoluteFilePath));
        return false;
    } else if (ioError != IoError::Success) {
        LOGW_WARN(_logger, L"Call to IoHelper::setFileDates failed: " << Utility::formatSyncPath(absoluteFilePath));
        return false;
    }

    return true;
}

bool LiteSyncExtConnector::vfsIsExcluded(const SyncPath &path) {
    std::string pinState;
    if (!vfsGetPinState(path.native(), pinState)) {
        return false;
    }

    return pinState == litesync_attrs::pinStateExcluded;
}

bool LiteSyncExtConnector::vfsSetStatus(const SyncPath &path, const SyncPath &localSyncPath, const VfsStatus &vfsStatus) {
    VfsStatus currentVfsStatus;
    if (!vfsGetStatus(path, currentVfsStatus)) {
        return false;
    }

    if (!currentVfsStatus.isPlaceholder) {
        // After a download, the file is replaced by the temp file, therefor we need to convert it again to placeholder
        vfsConvertToPlaceHolder(path, vfsStatus.isHydrated);
    }

    auto roundedProgress = vfsStatus.progress;
    if (vfsStatus.isSyncing) {
        int64_t stepWidth = 100 / SYNC_STEPS;
        roundedProgress = static_cast<int16_t>(ceil(float(vfsStatus.progress) / stepWidth) * stepWidth);
    }

    if (vfsStatus.isSyncing != currentVfsStatus.isSyncing || roundedProgress != currentVfsStatus.progress ||
        vfsStatus.isHydrated != currentVfsStatus.isHydrated) {
        // Set status
        std::string status = IoHelper::statusXAttr(vfsStatus.isSyncing, roundedProgress, vfsStatus.isHydrated);
        IoError ioError = IoError::Success;
        if (!IoHelper::setXAttrValue(path, litesync_attrs::status, status, ioError)) {
            LOGW_WARN(_logger, L"Call to setXAttrValue failed: " << Utility::formatIoError(path, ioError));
            return false;
        }

        if (checkIoErrorAndLogIfNeeded(ioError, "Item", path, _logger)) {
            return false;
        }

        if (!sendStatusToFinder(path, VfsStatus({.isHydrated = vfsStatus.isHydrated,
                                                 .isSyncing = vfsStatus.isSyncing,
                                                 .progress = roundedProgress}))) {
            LOGW_WARN(_logger, L"Call to sendStatusToFinder failed: " << Utility::formatSyncPath(path));
            return false;
        }

        if (vfsStatus.isSyncing != currentVfsStatus.isSyncing || vfsStatus.isHydrated != currentVfsStatus.isHydrated) {
            // Update parent directory status
            const SyncPath parentPath = path.parent_path();
            if (parentPath == localSyncPath) return true;

            if (vfsStatus.isSyncing) {
                {
                    const std::scoped_lock lock(_mutex);
                    _syncingFolders[parentPath].insert(path);
                }

                vfsSetStatus(parentPath, localSyncPath,
                             VfsStatus({.isHydrated = vfsStatus.isHydrated, .isSyncing = vfsStatus.isSyncing, .progress = 100}));
            } else {
                {
                    const std::scoped_lock lock(_mutex);
                    (void) _syncingFolders[parentPath].erase(path);
                    if (_syncingFolders[parentPath].empty()) {
                        (void) _syncingFolders.erase(parentPath);
                        if (!vfsProcessDirStatus(parentPath, localSyncPath)) {
                            return false;
                        }
                    }
                }
            }
        }
    }
    return true;
}

bool LiteSyncExtConnector::vfsCleanUpStatuses(const SyncPath &localSyncPath) {
    const std::scoped_lock lock(_mutex);
    for (const auto &[parentPath, syncingItemPaths]: _syncingFolders) {
        if (!vfsProcessDirStatus(parentPath, localSyncPath)) return false;
    }
    _syncingFolders.clear();
    return true;
}

bool LiteSyncExtConnector::vfsProcessDirStatus(const SyncPath &path, const SyncPath &localSyncPath) {
    VfsStatus vfsStatus;
    if (!vfsGetStatus(path, vfsStatus)) {
        return false;
    }

    if (!vfsStatus.isPlaceholder) {
        return true;
    }

    std::string pinState;
    if (!vfsGetPinState(path, pinState)) {
        return false;
    }

    if (pinState == litesync_attrs::pinStateExcluded) {
        return true;
    }

    bool hasASyncingChild = false;
    bool hasADehydratedChild = false;
    IoError ioError(IoError::Unknown);
    IoHelper::DirectoryIterator dirIt(path, false, ioError);
    bool endOfDir = false;
    DirectoryEntry entry;
    while (dirIt.next(entry, endOfDir, ioError) && !endOfDir && ioError == IoError::Success) {
        if (!vfsGetPinState(entry.path(), pinState)) {
            continue;
        }

        if (pinState == litesync_attrs::pinStateExcluded) {
            continue;
        }

        VfsStatus childVfsStatus;
        if (!vfsGetStatus(entry.path(), childVfsStatus)) {
            continue;
        }

        if (childVfsStatus.isSyncing) {
            hasASyncingChild = true;
            break;
        }

        if (!childVfsStatus.isHydrated) {
            hasADehydratedChild = true;
            break;
        }
    }

    VfsStatus tmpVfsStatus = {.isHydrated = !hasADehydratedChild, .isSyncing = hasASyncingChild, .progress = 100};
    if (!vfsSetStatus(path, localSyncPath, tmpVfsStatus)) {
        return false;
    }

    return true;
}

void LiteSyncExtConnector::vfsClearFileAttributes(const SyncPath &path) {
    removexattr(path.c_str(), litesync_attrs::status.data(), 0);
    removexattr(path.c_str(), litesync_attrs::pinState.data(), 0);
}

bool LiteSyncExtConnector::sendStatusToFinder(const SyncPath &path, const VfsStatus &vfsStatus) {
    if (!_private) {
        return false;
    }

    std::string status;
    if (vfsStatus.isSyncing) {
        int stepWidth = 100 / SYNC_STEPS;
        status = std::format("SYNC_{}", ceil(float(vfsStatus.progress) / stepWidth) * stepWidth);
    } else if (vfsStatus.isHydrated) {
        status = "OK";
    } else {
        status = "ONLINE";
    }

    // Update Finder
    LOGW_DEBUG(_logger, L"Send status to the Finder extension: " << Utility::formatSyncPath(path));
    const std::string msg = std::format("STATUS:{}:{}", status, Path2Str(path));
    return _private->executeCommand(msg);
}

bool LiteSyncExtConnector::checkFilesAttributes(const SyncPath &path, const SyncPath &localSyncPath,
                                                std::list<SyncPath> &filesToFix) {
    IoError ioError = IoError::Unknown;
    IoHelper::DirectoryIterator dirIt(path, false, ioError);
    bool endOfDir = false;
    DirectoryEntry entry;
    bool foundChild = false;
    bool atLeastOneChanged = false;
    while (dirIt.next(entry, endOfDir, ioError) && !endOfDir && ioError == IoError::Success) {
        std::string pinState;
        if (!vfsGetPinState(entry.path(), pinState)) {
            continue;
        }

        if (pinState == litesync_attrs::pinStateExcluded) {
            continue;
        }

        VfsStatus vfsStatus;
        if (!vfsGetStatus(entry.path(), vfsStatus)) {
            continue;
        }

        if (vfsStatus.isSyncing) {
            bool isDirectory = false;
            ioError = IoHelper::checkIfIsDirectory(path, isDirectory);
            if (ioError != IoError::Success) {
                LOGW_WARN(_logger, L"Call to IoHelper::checkIfIsDirectory failed: " << Utility::formatIoError(path, ioError));
                continue;
            }

            if (isDirectory) {
                if (!checkFilesAttributes(entry.path(), localSyncPath, filesToFix)) {
                    // No file has to be changed, but we still need to refresh this directory
                    filesToFix.push_back(entry.path());
                }
                atLeastOneChanged = true;
            } else {
                if (pinState == litesync_attrs::pinStateUnpinned ||
                    (pinState == litesync_attrs::pinStatePinned && !vfsStatus.isHydrated)) {
                    // A file should never be unpinned and syncing
                    // nor pinned and not completely hydrated
                    if (pinState == litesync_attrs::pinStatePinned && !vfsStatus.isHydrated) {
                        vfsSetPinState(entry.path(), localSyncPath, litesync_attrs::pinStateUnpinned);
                    }

                    VfsStatus resetVfsStatus;
                    vfsSetStatus(entry.path(), localSyncPath, resetVfsStatus);
                    filesToFix.push_back(entry.path());
                    atLeastOneChanged = true;
                }
            }
        }
    }
    return atLeastOneChanged;
}

void LiteSyncExtConnector::resetConnector(log4cplus::Logger logger, ExecuteCommand executeCommand) {
    if (_private) {
        delete _private;
        _private = nullptr;

        _private = new LiteSyncExtConnectorPrivate(logger, executeCommand);
        if (!_private) {
            LOG_ERROR(_logger, "Error in LiteSyncExtConnectorPrivate constructor");
            throw std::runtime_error("Error in LiteSyncExtConnectorPrivate constructor!");
        }
    }
}

} // namespace KDC
