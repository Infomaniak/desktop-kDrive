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

#import "litesyncextconnector.h"
#import "../extensions/MacOSX/kDriveLiteSync/Extension/xpcLiteSyncExtensionProtocol.h"
#import "../extensions/MacOSX/kDriveLiteSync/kDrive/xpcLiteSyncExtensionRemoteProtocol.h"
#import "../extensions/MacOSX/kDriveLiteSync/Extension/fileAttributes.h"
#include "../../../common/filepermissionholder.h"
#include "../../../common/utility.h"
#include "../../../common/filesystembase.h"
#include "libcommon/utility/utility.h"
#include "libcommonserver/io/iohelper.h"
#include "libcommonserver/log/log.h"
#include "libcommonserver/utility/utility.h"

#include <QDir>

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
        NSLog(@"[KD] System extension will apply after reboot - result=%ld", (long)result);
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
}

@property(retain) NSXPCConnection *connection;

- (instancetype)init:(KDC::ExecuteCommand)executeCommand;
- (void)dealloc;
- (BOOL)installExt:(BOOL *)activationDone;
- (void)onTimeout:(NSTimer *)timer;
- (BOOL)connectToExt;
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

- (BOOL)connectToExt {
    if (_connection != nil) {
        // Already connected
        return TRUE;
    }

    // Setup connection with LiteSync extension
    NSLog(@"[KD] Setup connection with LiteSync extension");

    // Read LiteSyncExtMachName from plist
    NSBundle *appBundle = [NSBundle mainBundle];
    NSString *liteSyncExtMachName = [appBundle objectForInfoDictionaryKey:@"LiteSyncExtMachName"];
    if (!liteSyncExtMachName) {
        NSLog(@"[KD] LiteSyncExtMachName undefined");
        return FALSE;
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
    _connection.interruptionHandler = ^{
      // The LiteSync extension has exited or crashed
      NSLog(@"[KD] Connection with LiteSync extension interrupted");
      _connection = nil;
    };

    _connection.invalidationHandler = ^{
      // Connection can not be formed or has terminated and may not be re-established
      NSLog(@"[KD] Connection with LiteSync extension invalidated");
      _connection = nil;
    };

    // Resume connection
    NSLog(@"[KD] Resume connection with LiteSync extension");
    [_connection resume];

    return TRUE;
}

- (BOOL)registerFolder:(NSString *)path {
    if (_connection) {
        [[_connection remoteObjectProxy] registerFolder:_pId folderPath:path];
        return TRUE;
    }

    return FALSE;
}

- (BOOL)unregisterFolder:(NSString *)path {
    if (_connection) {
        [[_connection remoteObjectProxy] unregisterFolder:_pId folderPath:path];
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
    while (!done && (success = [doneCondition waitUntilDate:timeoutDate])) {
    }
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
                appName = [bundle objectForInfoDictionaryKey:(id)kCFBundleNameKey /*kCFBundleExecutableKey*/];
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

static bool getXAttrValue(const QString &path, const QString &attrName, QString &value, IoError &ioError) {
    std::string strValue;
    bool result = IoHelper::getXAttrValue(SyncPath(path.toStdString()), attrName.toStdString(), strValue, ioError);
    if (!result) {
        return false;
    }
    value = QByteArray(strValue.c_str());
    return true;
}
static bool setXAttrValue(const QString &path, const QString &attrName, const QString &value, IoError &ioError) {
    bool result = IoHelper::setXAttrValue(QStr2Path(path), attrName.toStdString(), value.toStdString(), ioError);
    if (!result) {
        return false;
    }
    return true;
}

class LiteSyncExtConnectorPrivate {
    public:
        LiteSyncExtConnectorPrivate(log4cplus::Logger logger, ExecuteCommand executeCommand);
        ~LiteSyncExtConnectorPrivate();

        bool install(bool &activationDone);
        bool connect();

        bool vfsInit();
        bool vfsStart(const QString &folderPath);
        bool vfsStop(const QString &folderPath);

        bool executeCommand(const QString &commandLine);
        bool updateFetchStatus(const QString &filePath, const QString &status);
        bool setThumbnail(const QString &filePath, const QPixmap &pixmap);
        bool setAppExcludeList(const QString &appList);
        bool getFetchingAppList(QHash<QString, QString> &appTable);

    private:
        log4cplus::Logger _logger;
        Connector *_connector = nullptr;
        ExecuteCommand _executeCommand = nullptr;
};

// LiteSyncExtConnectorPrivate implementation
LiteSyncExtConnectorPrivate::LiteSyncExtConnectorPrivate(log4cplus::Logger logger, ExecuteCommand executeCommand)
    : _logger(logger) {
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
enum LogMode : bool { Debug = true, Warn = false };
//! Returns true and logs the specified IO error if it pertains to the existence of the folder or to its permissions.
/*!
  \param ioError is the IO error that has been raised.
  \param itemType is the type of the item subject to an IO error (e.g., "file" or "Sync folder").
  \param path is the file system path of the item subject to an IO error.
  \param logger is the logger used to log the error message.
  \param debug is an optional boolean value indicating whether the logging mode is DEBUG or WARN. Defaults to true, i.e., DEBUG
  mode. \return true if the IO error pertains to the existence of the folder or to its permissions, false otherwise.
*/
bool checkIoErrorAndLogIfNeeded(IoError ioError, const std::string &itemType, const QString &path, log4cplus::Logger &logger,
                                LogMode mode = LogMode::Debug) {
    if (ioError != IoErrorNoSuchFileOrDirectory && ioError != IoErrorAccessDenied) {
        return false;
    }

    std::wstring message;
    if (ioError == IoErrorNoSuchFileOrDirectory) {
        message = Utility::s2ws(itemType + " doesn't exist - path=");
    }
    if (ioError == IoErrorAccessDenied) {
        message = Utility::s2ws(itemType + " or some containing directory, misses a permission - path=");
    }

    switch (mode) {
        case LogMode::Debug:
            LOGW_DEBUG(logger, message.c_str() << QStr2Path(path).c_str());
        case LogMode::Warn:
            LOGW_WARN(logger, message.c_str() << QStr2Path(path).c_str());
    }

    return true;
}
}  // namespace

bool LiteSyncExtConnectorPrivate::vfsStart(const QString &folderPath) {
    if (!_connector) {
        LOG_ERROR(_logger, "Connector not initialized!");
        return false;
    }

    if (![_connector registerFolder:folderPath.toNSString()]) {
        LOG_ERROR(_logger, "Cannot register folder!");
        return false;
    }

    // Read folder status
    QString value;
    IoError ioError = IoErrorSuccess;
    const bool result = getXAttrValue(folderPath, [EXT_ATTR_STATUS UTF8String], value, ioError);
    if (!result) {
        LOGW_WARN(_logger, L"Error in getXAttrValue: " << Utility::formatIoError(QStr2Path(folderPath), ioError).c_str());
        return false;
    }

    if (checkIoErrorAndLogIfNeeded(ioError, "Sync folder", folderPath, _logger, LogMode::Warn)) {
        return false;
    }

    if (value.isEmpty()) {
        // Set default folder status
        if (!setXAttrValue(folderPath, [EXT_ATTR_STATUS UTF8String], EXT_ATTR_STATUS_ONLINE, ioError)) {
            const std::wstring ioErrorMessage = Utility::s2ws(IoHelper::ioError2StdString(ioError));
            LOGW_WARN(_logger, L"Error in setXAttrValue - path=" << QStr2Path(folderPath).c_str() << L" Error: "
                                                                 << ioErrorMessage.c_str());
            return false;
        }

        if (checkIoErrorAndLogIfNeeded(ioError, "Sync folder", folderPath, _logger, LogMode::Warn)) {
            return false;
        }

        // Set default folder pin state
        if (!setXAttrValue(folderPath, [EXT_ATTR_PIN_STATE UTF8String], EXT_ATTR_PIN_STATE_UNPINNED, ioError)) {
            const std::wstring ioErrorMessage = Utility::s2ws(IoHelper::ioError2StdString(ioError));
            LOGW_WARN(_logger, L"Error in setXAttrValue - path=" << QStr2Path(folderPath).c_str() << L" Error: "
                                                                 << ioErrorMessage.c_str());
            return false;
        }

        if (checkIoErrorAndLogIfNeeded(ioError, "Sync folder", folderPath, _logger, LogMode::Warn)) {
            return false;
        }
    }

    return true;
}

bool LiteSyncExtConnectorPrivate::vfsStop(const QString &folderPath) {
    if (!_connector) {
        LOG_ERROR(_logger, "Connector not initialized!");
        return false;
    }

    if (![_connector unregisterFolder:folderPath.toNSString()]) {
        LOG_ERROR(_logger, "Cannot unregister folder!");
        return false;
    }

    return true;
}

bool LiteSyncExtConnectorPrivate::executeCommand(const QString &commandLine) {
    if (!_connector) {
        LOG_WARN(_logger, "Connector not initialized!");
        return false;
    }

    if (!_executeCommand) {
        LOG_WARN(_logger, "No execute command!");
        return false;
    }

    LOGW_DEBUG(_logger, L"Execute command: " << QStr2WStr(commandLine).c_str());
    _executeCommand(QStr2Str(commandLine).c_str());

    return true;
}

bool LiteSyncExtConnectorPrivate::updateFetchStatus(const QString &filePath, const QString &status) {
    if (!_connector) {
        LOG_WARN(_logger, "Connector not initialized!");
        return false;
    }

    if (![_connector updateFetchStatus:filePath.toNSString() fileStatus:status.toNSString()]) {
        LOGW_ERROR(_logger, L"Call to updateFetchStatus failed - path=" << QStr2WStr(filePath).c_str());
        return false;
    }

    return true;
}

bool LiteSyncExtConnectorPrivate::setThumbnail(const QString &filePath, const QPixmap &pixmap) {
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
        LOGW_WARN(_logger, L"Call to initWithCGImage failed - path=" << QStr2WStr(filePath).c_str());
        error = true;
    }

    if (!error) {
        // Destination image
        int size = qMax(srcImage.size.width, srcImage.size.height);
        NSImage *dstImage = [[NSImage alloc] initWithSize:CGSizeMake(size, size)];

        // Copy source image into destination image
        NSRect dstRect = NSMakeRect(size == srcImage.size.width ? 0 : (size - srcImage.size.width) / 2,
                                    size == srcImage.size.width ? (size - srcImage.size.height) / 2 : 0, srcImage.size.width,
                                    srcImage.size.height);
        [dstImage lockFocus];
        [srcImage drawInRect:dstRect fromRect:NSZeroRect operation:NSCompositingOperationCopy fraction:1.0];
        [dstImage unlockFocus];

        if (![_connector setThumbnail:filePath.toNSString() image:dstImage]) {
            LOGW_WARN(_logger, L"Call to setThumbnail failed - path=" << QStr2WStr(filePath).c_str());
            error = true;
        }
    }

    if (![_connector updateThumbnailFetchStatus:filePath.toNSString()
                                     fileStatus:(error ? QString("KO") : QString("OK")).toNSString()]) {
        LOGW_ERROR(_logger, L"Call to updateThumbnailFetchStatus failed - path=" << QStr2WStr(filePath).c_str());
        error = true;
    }

    return !error;
}

bool LiteSyncExtConnectorPrivate::setAppExcludeList(const QString &appList) {
    if (!_connector) {
        LOG_WARN(_logger, "Connector not initialized!");
        return false;
    }

    if (![_connector setAppExcludeList:appList.toNSString()]) {
        LOG_ERROR(_logger, "Error in setAppExcludeList!");
        return false;
    }

    return true;
}

bool LiteSyncExtConnectorPrivate::getFetchingAppList(QHash<QString, QString> &appTable) {
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
LiteSyncExtConnector::LiteSyncExtConnector(log4cplus::Logger logger, ExecuteCommand executeCommand)
    : _logger(logger), _private(nullptr) {
    LOG_DEBUG(_logger, "LiteSyncExtConnector creation");

    _private = new LiteSyncExtConnectorPrivate(logger, executeCommand);
    if (!_private) {
        LOG_ERROR(_logger, "Error in LiteSyncExtConnectorPrivate constructor");
        throw std::runtime_error("Error in LiteSyncExtConnectorPrivate constructor!");
    }
}

LiteSyncExtConnector *LiteSyncExtConnector::instance(log4cplus::Logger logger, ExecuteCommand executeCommand) {
    if (_liteSyncExtConnector == nullptr) {
        _liteSyncExtConnector = new LiteSyncExtConnector(logger, executeCommand);
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

bool LiteSyncExtConnector::vfsStart(int syncDbId, const QString &folderPath, bool &isPlaceholder, bool &isSyncing) {
    if (!_private) {
        return false;
    }

    if (!_private->vfsStart(folderPath)) {
        return false;
    }

    _folders[syncDbId] = folderPath;

    // Send root status to Finder
    bool isHydrated;
    int progress;
    if (!vfsGetStatus(folderPath, isPlaceholder, isHydrated, isSyncing, progress)) {
        return false;
    }

    if (!sendStatusToFinder(folderPath, isSyncing, isSyncing ? 100 : 0, isHydrated)) {
        LOGW_WARN(_logger, L"Error in sendStatusToFinder - path=" << QStr2WStr(folderPath).c_str());
        return false;
    }

    return true;
}

bool LiteSyncExtConnector::vfsStop(int syncDbId) {
    if (!_private) {
        return false;
    }

    if (!_folders.contains(syncDbId)) {
        LOG_WARN(_logger, "Folder not registred!");
        return false;
    }

    QString path = _folders[syncDbId];
    _folders.remove(syncDbId);

    return _private->vfsStop(path);
}

bool LiteSyncExtConnector::vfsHydratePlaceHolder(const QString &filePath) {
    if (!_private) {
        return false;
    }

    // Read status
    QString value;
    IoError ioError = IoErrorSuccess;
    const bool result = getXAttrValue(filePath, [EXT_ATTR_STATUS UTF8String], value, ioError);
    if (!result) {
        LOGW_WARN(_logger, L"Error in getXAttrValue: " << Utility::formatIoError(QStr2Path(filePath), ioError).c_str());
        return false;
    }

    if (checkIoErrorAndLogIfNeeded(ioError, "File", filePath, _logger)) {
        return false;
    }

    if (value.isEmpty()) {
        LOGW_WARN(_logger, L"File is not a placeholder - path=" << QStr2WStr(filePath).c_str());
        return false;
    }

    if (value == EXT_ATTR_STATUS_OFFLINE) {
        // Try to temporarily change the write access right of the file
        FileSystem::setUserWritePermission(filePath);

        // Get file
        LOGW_DEBUG(_logger, L"Get file " << QStr2WStr(filePath).c_str());
        _private->executeCommand(QString("MAKE_AVAILABLE_LOCALLY_DIRECT:%1").arg(filePath));
    }

    return true;
}

bool LiteSyncExtConnector::vfsDehydratePlaceHolder(const QString &absoluteFilepath) {
    // Read status
    QString value;
    IoError ioError = IoErrorSuccess;
    const bool result = getXAttrValue(absoluteFilepath, [EXT_ATTR_STATUS UTF8String], value, ioError);
    if (!result) {
        LOGW_WARN(_logger, L"Error in getXAttrValue - path=" << QStr2WStr(absoluteFilepath).c_str());
        return false;
    }

    if (checkIoErrorAndLogIfNeeded(ioError, "File", absoluteFilepath, _logger)) {
        return false;
    }

    if (value.isEmpty()) {
        LOGW_WARN(_logger, L"File is not a placeholder - path=" << QStr2WStr(absoluteFilepath).c_str());
        return false;
    }

    FilePermissionHolder permHolder(absoluteFilepath);

    const SyncPath stdPath = QStr2Path(absoluteFilepath);

    struct stat fileStat;
    if (stat(stdPath.c_str(), &fileStat) == -1) {
        LOGW_WARN(_logger, L"Call to stat failed - path=" << QStr2WStr(absoluteFilepath).c_str() << L" errno=" << errno);
        return false;
    }

    // Empty file
    int fd = open(stdPath.c_str(), O_WRONLY);
    if (fd == -1) {
        // Try to temporarily change the write access right of the file
        FileSystem::setUserWritePermission(absoluteFilepath);

        fd = open(stdPath.c_str(), O_WRONLY);
        if (fd == -1) {
            LOGW_WARN(_logger, L"Call to open failed - path=" << QStr2WStr(absoluteFilepath).c_str() << L" errno=" << errno);
            return false;
        }
    }

    if (ftruncate(fd, 0) == -1) {
        LOGW_WARN(_logger, L"Call to ftruncate failed - path=" << QStr2WStr(absoluteFilepath).c_str() << L" errno=" << errno);
        return false;
    }

    if (ftruncate(fd, fileStat.st_size) == -1) {
        LOGW_WARN(_logger, L"Call to ftruncate failed - path=" << QStr2WStr(absoluteFilepath).c_str() << L" errno=" << errno);
        return false;
    }

    if (close(fd) == -1) {
        LOGW_WARN(_logger, L"Call to close failed - path=" << QStr2WStr(absoluteFilepath).c_str() << L" errno=" << errno);
        return false;
    }

    // Set times
    static struct timespec tspec[2];
    tspec[0] = fileStat.st_atimespec;
    tspec[1] = fileStat.st_mtimespec;
    if (utimensat(0, stdPath.c_str(), tspec, 0) == -1) {
        LOGW_WARN(_logger, L"Call to utimensat failed - path=" << QStr2WStr(absoluteFilepath).c_str() << L" errno=" << errno);
        return false;
    }

    // Set status
    if (!vfsSetStatus(absoluteFilepath, false, 0, false)) {
        return false;
    }

    return true;
}

bool LiteSyncExtConnector::vfsSetPinState(const QString &path, const QString &pinState) {
    FilePermissionHolder permHolder(path);

    // Try to temporarily change the write access right of the file
    FileSystem::setUserWritePermission(path);

    IoError ioError = IoErrorSuccess;
    if (!setXAttrValue(path, [EXT_ATTR_PIN_STATE UTF8String], pinState, ioError)) {
        LOGW_WARN(_logger, L"Call to setXAttrValue failed - path=" << QStr2WStr(path).c_str() << L" errno=" << errno);
        return false;
    }

    if (checkIoErrorAndLogIfNeeded(ioError, "Item", path, _logger)) {
        return false;
    }

    // Set status if empty directory
    QFileInfo info(path);
    if (info.isDir()) {
        const QFileInfoList infoList = QDir(path).entryInfoList(QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot);
        bool foundChild = false;
        for (const auto &tmpInfo : qAsConst(infoList)) {
            QString tmpPath(tmpInfo.filePath());

            QString tmpPinState;
            if (!vfsGetPinState(tmpPath, tmpPinState)) {
                continue;
            }

            if (tmpPinState == VFS_PIN_STATE_EXCLUDED) {
                continue;
            }

            foundChild = true;
            break;
        }

        if (!foundChild) {
            // Set status
            bool isHydrated = (pinState == VFS_PIN_STATE_PINNED);
            if (!vfsSetStatus(path, false, 0, isHydrated)) {
                return false;
            }
        }
    }

    return true;
}

bool LiteSyncExtConnector::vfsGetPinState(const QString &path, QString &pinState) {
    // Read pin state
    QString value;
    IoError ioError = IoErrorSuccess;
    const bool result = getXAttrValue(path, [EXT_ATTR_PIN_STATE UTF8String], value, ioError);
    if (!result) {
        LOGW_WARN(_logger, L"Error in getXAttrValue:" << Utility::formatIoError(QStr2Path(path), ioError).c_str());
        return false;
    }

    if (checkIoErrorAndLogIfNeeded(ioError, "Item", path, _logger)) {
        return false;
    }

    if (value.isEmpty()) {
        LOGW_DEBUG(_logger, L"Item is not a placeholder - path=" << QStr2WStr(path).c_str());
        return false;
    }

    pinState = value[0];

    return true;
}

bool LiteSyncExtConnector::vfsConvertToPlaceHolder(const QString &filePath, bool isHydrated) {
    // Set status
    QString status = (isHydrated ? EXT_ATTR_STATUS_OFFLINE : EXT_ATTR_STATUS_ONLINE);
    IoError ioError = IoErrorSuccess;
    if (!setXAttrValue(filePath, [EXT_ATTR_STATUS UTF8String], status, ioError)) {
        const std::wstring ioErrorMessage = Utility::s2ws(IoHelper::ioError2StdString(ioError));
        LOGW_WARN(_logger, L"Call to setXAttrValue failed - path=" << QStr2WStr(filePath).c_str() << L" Error: "
                                                                   << ioErrorMessage.c_str());
        return false;
    }

    if (checkIoErrorAndLogIfNeeded(ioError, "Item", filePath, _logger)) {
        return false;
    }

    // Set pin state
    QString pinState = (isHydrated ? EXT_ATTR_PIN_STATE_PINNED : EXT_ATTR_PIN_STATE_UNPINNED);
    if (!setXAttrValue(filePath, [EXT_ATTR_PIN_STATE UTF8String], pinState, ioError)) {
        const std::wstring ioErrorMessage = Utility::s2ws(IoHelper::ioError2StdString(ioError));
        LOGW_WARN(_logger, L"Call to setXAttrValue failed - path=" << QStr2WStr(filePath).c_str() << L" Error: "
                                                                   << ioErrorMessage.c_str());
        return false;
    }

    if (checkIoErrorAndLogIfNeeded(ioError, "Item", filePath, _logger)) {
        return false;
    }

    return true;
}

bool LiteSyncExtConnector::vfsCreatePlaceHolder(const QString &relativePath, const QString &destPath,
                                                const struct stat *fileStat) {
    if (!fileStat) {
        LOG_WARN(_logger, "Bad parameters");
        return false;
    }

    QString path = destPath + "/" + relativePath;
    std::string stdPath = path.toStdString();

    if (fileStat->st_mode == S_IFDIR || fileStat->st_mode == S_IFREG) {
        int flags = fileStat->st_mode == S_IFDIR ? O_DIRECTORY | O_CREAT                           // Directory
                                                 : O_CREAT | FWRITE | O_NONBLOCK;                  // File
        int mode = fileStat->st_mode == S_IFDIR ? S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH  // Rights rwxr-xr-x
                                                : S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;           // Rights rw-r--r--

        int fd = open(stdPath.c_str(), flags, mode);
        if (fd == -1) {
            LOGW_WARN(_logger, L"Call to open failed - path=" << QStr2WStr(path).c_str() << L" errno=" << errno);
            return false;
        }

        if (fileStat->st_mode == S_IFREG) {
            if (ftruncate(fd, fileStat->st_size) == -1) {
                LOGW_WARN(_logger, L"Call to ftruncate failed - path=" << QStr2WStr(path).c_str() << L" errno=" << errno);
                close(fd);
                return false;
            }
        }

        if (close(fd) == -1) {
            LOGW_WARN(_logger, L"Call to close failed - path=" << QStr2WStr(path).c_str() << L" errno=" << errno);
            return false;
        }
    }

    // Set status
    IoError ioError = IoErrorSuccess;
    if (!setXAttrValue(path, [EXT_ATTR_STATUS UTF8String], EXT_ATTR_STATUS_ONLINE, ioError)) {
        const std::wstring ioErrorMessage = Utility::s2ws(IoHelper::ioError2StdString(ioError));
        LOGW_WARN(_logger,
                  L"Call to setXAttrValue failed - path=" << QStr2WStr(path).c_str() << L"Error: " << ioErrorMessage.c_str());
        return false;
    }

    if (checkIoErrorAndLogIfNeeded(ioError, "Item", path, _logger)) {
        return false;
    }

    // Set pin state
    if (!setXAttrValue(path, [EXT_ATTR_PIN_STATE UTF8String], EXT_ATTR_PIN_STATE_UNPINNED, ioError)) {
        const std::wstring ioErrorMessage = Utility::s2ws(IoHelper::ioError2StdString(ioError));
        LOGW_WARN(_logger,
                  L"Call to setXAttrValue failed - path=" << QStr2WStr(path).c_str() << L" Error: " << ioErrorMessage.c_str());
        return false;
    }

    if (checkIoErrorAndLogIfNeeded(ioError, "Item", path, _logger)) {
        return false;
    }

    // Set file dates
    bool exists = false;
    if (!Utility::setFileDates(QStr2Path(path), std::make_optional<KDC::SyncTime>(fileStat->st_birthtimespec.tv_sec),
                               std::make_optional<KDC::SyncTime>(fileStat->st_mtimespec.tv_sec), exists)) {
        LOGW_WARN(_logger, L"Call to Utility::setFileDates failed - path=" << QStr2WStr(path).c_str());
        return false;
    }

    if (!exists) {
        LOGW_DEBUG(_logger, L"Item doesn't exist - path=" << QStr2WStr(path).c_str());
        return false;
    }

    // Update parent directory status if needed
    QFileInfo info(path);

    bool isPlaceholder;
    bool isHydrated;
    bool isSyncing;
    int progress;
    if (!vfsGetStatus(path, isPlaceholder, isHydrated, isSyncing, progress)) {
        return false;
    }

    if (isHydrated) {
        return vfsProcessDirStatus(info.dir().path());
    }

    return true;
}

bool LiteSyncExtConnector::vfsUpdateFetchStatus(const QString &tmpFilePath, const QString &filePath, unsigned long long completed,
                                                bool &canceled, bool &finished) {
    FilePermissionHolder permHolder(filePath);

    // Temporarily change the write access right of the file
    FileSystem::setUserWritePermission(filePath);

    canceled = false;

    @autoreleasepool {
        // Get file attributes
        NSError *error = nil;
        NSDictionary<NSFileAttributeKey, id> *attributes =
            [[NSFileManager defaultManager] attributesOfItemAtPath:filePath.toNSString() error:&error];
        if (error) {
            LOGW_WARN(_logger, L"Failed to get attributes - path=" << QStr2WStr(filePath).c_str() << L" errno=" << error);
            return false;
        }

        unsigned long long fileSize = [attributes fileSize];
        finished = (completed == fileSize);
        if (finished) {
            time_t creationDate = OldUtility::qDateTimeToTime_t(QFileInfo(filePath).birthTime());

            // Copy tmp file content to file
            @try {
                LOGW_INFO(_logger, L"Copying temp file - from path=" << QStr2WStr(tmpFilePath).c_str() << L" to path="
                                                                     << QStr2WStr(filePath).c_str());

                NSFileHandle *tmpFileHandle = [NSFileHandle fileHandleForReadingAtPath:tmpFilePath.toNSString()];
                NSFileHandle *fileHandle = [NSFileHandle fileHandleForWritingAtPath:filePath.toNSString()];
                NSData *buffer = nil;

                while (true) {
                    @autoreleasepool {
                        if ((buffer = [tmpFileHandle readDataUpToLength:COPY_CHUNK_SIZE error:&error]) == nil) {
                            LOGW_ERROR(_logger, L"Error while reading tmp file - path=" << QStr2WStr(tmpFilePath).c_str()
                                                                                        << L" error=" << error);
                            break;
                        }

                        if (buffer.length == 0) {
                            // Nothing else to read
                            break;
                        }

                        if (![fileHandle writeData:buffer error:&error]) {
                            LOGW_ERROR(_logger, L"Error while writting to file - path=" << QStr2WStr(filePath).c_str()
                                                                                        << L" error=" << error);
                            break;
                        }
                    }
                }

                [tmpFileHandle closeFile];
                [fileHandle closeFile];
            } @catch (NSException *e) {
                LOGW_WARN(_logger, L"Could not copy tmp file - from path="
                                       << QStr2WStr(tmpFilePath).c_str() << L" to path=" << QStr2WStr(filePath).c_str()
                                       << Utility::s2ws(std::string([e.name UTF8String])).c_str());
                return false;
            }

            // Set attributes
            if (![[NSFileManager defaultManager] setAttributes:attributes ofItemAtPath:filePath.toNSString() error:&error]) {
                LOGW_WARN(_logger,
                          L"Could not set attributes to file - path=" << QStr2WStr(filePath).c_str() << L" errno=" << error);
                return false;
            }

            // Set status
            if (!vfsSetStatus(filePath, false, 0, true)) {
                return false;
            }

            if (!_private->updateFetchStatus(filePath, QString("OK"))) {
                LOGW_WARN(_logger, L"Call to updateFetchStatus failed - path=" << QStr2WStr(filePath).c_str());
                return false;
            }

            // Set file dates
            bool exists;
            if (!Utility::setFileDates(QStr2Path(filePath), std::make_optional<KDC::SyncTime>(creationDate), std::nullopt,
                                       exists)) {
                LOGW_WARN(_logger, L"Call to Utility::setFileDates failed - path=" << QStr2WStr(filePath).c_str());
                return false;
            }

            if (!exists) {
                LOGW_DEBUG(_logger, L"Item doesn't exist - path=" << QStr2WStr(filePath).c_str());
                return false;
            }
        } else {
            // Set status
            int progress = ceil(float(completed) / fileSize * 100);
            if (!vfsSetStatus(filePath, true, progress)) {
                return false;
            }
        }
    }

    return true;
}

bool LiteSyncExtConnector::vfsSetThumbnail(const QString &absoluteFilePath, const QPixmap &pixmap) {
    // Set thumbnail
    if (!_private->setThumbnail(absoluteFilePath, pixmap)) {
        LOGW_WARN(_logger, L"Call to setThumbnail failed - path=" << QStr2WStr(absoluteFilePath).c_str());
    }

    return true;
}

bool LiteSyncExtConnector::vfsGetStatus(const QString &absoluteFilePath, bool &isPlaceholder, bool &isHydrated, bool &isSyncing,
                                        int &progress, log4cplus::Logger &logger) noexcept {
    isPlaceholder = false;
    isHydrated = false;
    isSyncing = false;
    progress = 0;

    constexpr auto isExpectedError = [](IoError error) -> bool {
        return error == IoErrorSuccess || error == IoErrorAttrNotFound || error == IoErrorNoSuchFileOrDirectory;
    };

    // Read status
    QString value;
    IoError ioError = IoErrorSuccess;
    if (!getXAttrValue(absoluteFilePath, [EXT_ATTR_STATUS UTF8String], value, ioError) && !isExpectedError(ioError)) {
        LOGW_WARN(logger, L"Error in getXAttrValue: " << Utility::formatIoError(QStr2Str(absoluteFilePath), ioError).c_str());
        return false;
    }

    if (value.isEmpty()) return true;

    isPlaceholder = true;
    isHydrated = (value == EXT_ATTR_STATUS_OFFLINE);
    isSyncing = value.startsWith(EXT_ATTR_STATUS_HYDRATING);

    if (isSyncing) {
        value.remove(0, QString(EXT_ATTR_STATUS_HYDRATING).length());
        progress = value.toInt();
    }

    return true;
}

bool LiteSyncExtConnector::vfsSetAppExcludeList(const QString &appList) {
    if (!_private) {
        return false;
    }

    return _private->setAppExcludeList(appList);
}

bool LiteSyncExtConnector::vfsGetFetchingAppList(QHash<QString, QString> &appTable) {
    if (!_private) {
        return false;
    }

    return _private->getFetchingAppList(appTable);
}

bool LiteSyncExtConnector::vfsUpdateMetadata(const QString &absoluteFilePath, const struct stat *fileStat, QString *error) {
    FilePermissionHolder permHolder(absoluteFilePath);

    if (!fileStat) {
        LOG_WARN(_logger, "Bad parameters");
        *error = QObject::tr("Bad parameters");
        return false;
    }

    std::string stdPath = absoluteFilePath.toStdString();

    // Check status
    bool isPlaceholder;
    bool isHydrated;
    bool isSyncing;
    int progress;
    vfsGetStatus(absoluteFilePath, isPlaceholder, isHydrated, isSyncing, progress);

    if (!isPlaceholder || isHydrated || isSyncing) {
        return true;
    }

    // Create sparse file with rights rw-r--r--
    int fd = open(stdPath.c_str(), FWRITE);
    if (fd == -1) {
        // Try to temporarily change the write access right of the file
        FileSystem::setUserWritePermission(absoluteFilePath);

        fd = open(stdPath.c_str(), FWRITE);
        if (fd == -1) {
            LOGW_WARN(_logger, L"Call to open failed - path=" << QStr2WStr(absoluteFilePath).c_str() << L" errno=" << errno);
            *error = QObject::tr("Call to open failed - path=%1").arg(absoluteFilePath);
            return false;
        }
    }

    if (ftruncate(fd, fileStat->st_size) == -1) {
        LOGW_WARN(_logger, L"Call to ftruncate failed - path=" << QStr2WStr(absoluteFilePath).c_str() << L" errno=" << errno);
        *error = QObject::tr("Call to ftruncate failed - path=%1").arg(absoluteFilePath);
        return false;
    }

    if (close(fd) == -1) {
        LOGW_WARN(_logger, L"Call to close failed - path=" << QStr2WStr(absoluteFilePath).c_str() << L" errno=" << errno);
        *error = QObject::tr("Call to close failed - path=%1").arg(absoluteFilePath);
        return false;
    }

    // Set file dates
    bool exists;
    if (!Utility::setFileDates(QStr2Path(absoluteFilePath), fileStat->st_birthtimespec.tv_sec, fileStat->st_mtimespec.tv_sec,
                               exists)) {
        LOGW_WARN(_logger, L"Call to Utility::setFileDates failed - path=" << QStr2WStr(absoluteFilePath).c_str());
        return false;
    }

    if (!exists) {
        LOGW_DEBUG(_logger, L"Item doesn't exist - path=" << QStr2WStr(absoluteFilePath).c_str());
        return false;
    }

    return true;
}

bool LiteSyncExtConnector::vfsIsExcluded(const QString &path) {
    QString pinState;
    if (!vfsGetPinState(QDir::toNativeSeparators(path), pinState)) {
        return false;
    }

    return pinState == EXT_ATTR_PIN_STATE_EXCLUDED;
}

bool LiteSyncExtConnector::vfsSetStatus(const QString &path, bool isSyncing, int progress, bool isHydrated /*= false*/) {
    bool isPlaceholder = false;
    bool isSyncingCurrent = false;
    bool isHydratedCurrent = false;
    int progressCurrent;
    if (!vfsGetStatus(path, isPlaceholder, isHydratedCurrent, isSyncingCurrent, progressCurrent)) {
        return false;
    }

    if (!isPlaceholder) {
        // After a download, the file is remplaced by the temp file, therefor we need to convert it again to placeholder
        vfsConvertToPlaceHolder(path, isHydrated);
    }

    int roundedProgress = progress;
    if (isSyncing) {
        int stepWidth = 100 / SYNC_STEPS;
        roundedProgress = ceil(float(progress) / stepWidth) * stepWidth;
    }

    if (isSyncing != isSyncingCurrent || roundedProgress != progressCurrent || isHydrated != isHydratedCurrent) {
        // Set status
        QString status = (isSyncing ? QString("%1%2").arg(EXT_ATTR_STATUS_HYDRATING).arg(roundedProgress)
                                    : (isHydrated ? QString(EXT_ATTR_STATUS_OFFLINE) : QString(EXT_ATTR_STATUS_ONLINE)));
        IoError ioError = IoErrorSuccess;
        if (!setXAttrValue(path, [EXT_ATTR_STATUS UTF8String], status, ioError)) {
            const std::wstring ioErrorMessage = Utility::s2ws(IoHelper::ioError2StdString(ioError));
            LOGW_WARN(_logger, L"Call to setXAttrValue failed - path=" << QStr2WStr(path).c_str() << L" Error: "
                                                                       << ioErrorMessage.c_str());
            return false;
        }

        if (checkIoErrorAndLogIfNeeded(ioError, "Item", path, _logger)) {
            return false;
        }

        if (!sendStatusToFinder(path, isSyncing, roundedProgress, isHydrated)) {
            LOGW_WARN(_logger, L"Call to sendStatusToFinder failed - path=" << QStr2WStr(path).c_str());
            return false;
        }

        if (isSyncing != isSyncingCurrent || isHydrated != isHydratedCurrent) {
            // Update parent directory status
            QFileInfo info(path);
            return vfsProcessDirStatus(info.dir().path());
        }
    }
    return true;
}

bool LiteSyncExtConnector::vfsProcessDirStatus(const QString &path) {
    bool isPlaceholder;
    bool isSyncingCurrent;
    bool isHydratedCurrent;
    int progressCurrent;
    if (!vfsGetStatus(path, isPlaceholder, isHydratedCurrent, isSyncingCurrent, progressCurrent)) {
        return false;
    }

    if (!isPlaceholder) {
        return true;
    }

    QString pinState;
    if (!vfsGetPinState(path, pinState)) {
        return false;
    }

    if (pinState == VFS_PIN_STATE_EXCLUDED) {
        return true;
    }

    QDir dir(path);
    const QFileInfoList infoList = dir.entryInfoList(QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot);
    bool existsOneStatusSync = false;
    bool existsOneStatusOnLine = false;
    for (const auto &tmpInfo : qAsConst(infoList)) {
        QString tmpPath(tmpInfo.filePath());
        if (!vfsGetPinState(tmpPath, pinState)) {
            continue;
        }

        if (pinState == VFS_PIN_STATE_EXCLUDED) {
            continue;
        }

        bool isPlaceholder;
        bool isHydrated;
        bool isSyncing;
        int progress;
        if (!vfsGetStatus(tmpPath, isPlaceholder, isHydrated, isSyncing, progress)) {
            continue;
        }

        if (isSyncing) {
            existsOneStatusSync = true;
            break;
        }

        existsOneStatusOnLine |= !isHydrated;
    }

    if (!vfsSetStatus(path, existsOneStatusSync, 100, !existsOneStatusOnLine)) {
        return false;
    }

    return true;
}

void LiteSyncExtConnector::vfsClearFileAttributes(const QString &path) {
    removexattr(path.toStdString().c_str(), [EXT_ATTR_STATUS UTF8String], 0);
    removexattr(path.toStdString().c_str(), [EXT_ATTR_PIN_STATE UTF8String], 0);
}

bool LiteSyncExtConnector::sendStatusToFinder(const QString &path, bool isSyncing, int progress, bool isHydrated) {
    if (!_private) {
        return false;
    }

    QString status;
    if (isSyncing) {
        int stepWidth = 100 / SYNC_STEPS;
        status = QString("SYNC_%1").arg(ceil(float(progress) / stepWidth) * stepWidth);
    } else if (isHydrated) {
        status = "OK";
    } else {
        status = "ONLINE";
    }

    // Update Finder
    LOGW_DEBUG(_logger, L"Send status to the Finder extension - path=" << QStr2WStr(path).c_str());
    return _private->executeCommand(QString("STATUS:%1:%2").arg(status, path));
}

bool LiteSyncExtConnector::checkFilesAttributes(const QString &path, QStringList &filesToFix) {
    QDir dir(path);
    const QFileInfoList infoList = dir.entryInfoList(QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot);

    bool atLeastOneChanged = false;
    for (const auto &tmpInfo : qAsConst(infoList)) {
        QString tmpPath(tmpInfo.filePath());
        QString pinState;
        if (!vfsGetPinState(tmpPath, pinState)) {
            continue;
        }

        if (pinState == VFS_PIN_STATE_EXCLUDED) {
            continue;
        }

        bool tmpIsPlaceholder;
        bool tmpIsHydrated;
        bool tmpIsSyncing;
        int progress;
        if (!vfsGetStatus(tmpPath, tmpIsPlaceholder, tmpIsHydrated, tmpIsSyncing, progress)) {
            continue;
        }

        if (tmpIsSyncing) {
            if (tmpInfo.isDir()) {
                if (!checkFilesAttributes(tmpPath, filesToFix)) {
                    // No file has to be changed but we still need to refreh this directory
                    filesToFix.append(tmpPath);
                }
                atLeastOneChanged = true;
            } else {
                if (pinState == VFS_PIN_STATE_UNPINNED || (pinState == VFS_PIN_STATE_PINNED && !tmpIsHydrated)) {
                    // A file should never be unpinned and syncing
                    // nor pinned and not completly hydrated
                    if (pinState == VFS_PIN_STATE_PINNED && !tmpIsHydrated) {
                        vfsSetPinState(tmpPath, VFS_PIN_STATE_UNPINNED);
                    }

                    vfsSetStatus(tmpPath, false, 0, false);
                    filesToFix.append(tmpPath);
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

}  // namespace KDC
