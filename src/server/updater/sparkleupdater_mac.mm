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

#include <Sparkle/Sparkle.h>

#include "common/utility.h"
#include "libcommon/utility/utility.h"
#include "sparkleupdater.h"
#include "libcommonserver/log/log.h"
#include "libparms/db/parmsdb.h"

#include <log4cplus/loggingmacros.h>

// DelegateUpdaterObject class
@interface DelegateUpdaterObject : NSObject <SPUUpdaterDelegate> {
@protected
    KDC::DownloadState _state;
    NSString *_availableVersion;
    KDC::QuitCallback _quitCallback;
}
- (BOOL)updaterMayCheckForUpdates:(SPUUpdater *)updater;
- (BOOL)updaterShouldRelaunchApplication:(SPUUpdater *)updater;
- (void)updaterWillRelaunchApplication:(SPUUpdater *)updater;
- (KDC::DownloadState)downloadState;
- (NSString *)availableVersion;
- (void)setQuitCallback:(KDC::QuitCallback)quitCallback;
@end

@implementation DelegateUpdaterObject  //(SUUpdaterDelegateInformalProtocol)
- (instancetype)init {
    self = [super init];
    if (self) {
        _state = KDC::Unknown;
        _availableVersion = @"";
        _quitCallback = nullptr;
    }
    return self;
}

- (BOOL)updaterMayCheckForUpdates:(SPUUpdater *)updater {
    (void)updater;
    LOG_DEBUG(KDC::Log::instance()->getLogger(), "may check: YES");
    return YES;
}

- (BOOL)updaterShouldRelaunchApplication:(SPUUpdater *)updater {
    (void)updater;
    LOG_DEBUG(KDC::Log::instance()->getLogger(), "Updater should relaunch app: YES");
    return YES;
}

- (void)updaterWillRelaunchApplication:(SPUUpdater *)updater {
    (void)updater;
    LOG_DEBUG(KDC::Log::instance()->getLogger(), "Updater will relaunch app");

    if (bool found = false; !KDC::ParmsDb::instance()->updateAppState(KDC::AppStateKey::LastServerSelfRestartDate,
                                                                      KDC::selfRestarterDisableValue, found) ||
                            !found) {  // Deactivate the selfRestarter
        LOG_ERROR(KDC::Log::instance()->getLogger(), "Error in ParmsDb::updateAppState");
    }

    if (_quitCallback) {
        LOG_DEBUG(KDC::Log::instance()->getLogger(), "Ask app client to quit");
        _quitCallback();
    }
}

- (KDC::DownloadState)downloadState {
    return _state;
}

- (NSString *)availableVersion {
    return _availableVersion;
}

- (void)setQuitCallback:(KDC::QuitCallback)quitCallback {
    LOG_DEBUG(KDC::Log::instance()->getLogger(), "Set quitCallback");
    _quitCallback = quitCallback;
}

// Sent when a valid update is found by the update driver.
- (void)updater:(SPUUpdater *)updater didFindValidUpdate:(SUAppcastItem *)update {
    (void)updater;
    LOG_DEBUG(KDC::Log::instance()->getLogger(), "Version: " << update.versionString);
    _state = KDC::FindValidUpdate;
    _availableVersion = [update.versionString copy];
}

// Sent when a valid update is not found.
- (void)updaterDidNotFindUpdate:(SPUUpdater *)update {
    (void)update;
    LOG_DEBUG(KDC::Log::instance()->getLogger(), "No valid update found");
    _state = KDC::DidNotFindUpdate;
}

// Sent immediately before installing the specified update.
- (void)updater:(SPUUpdater *)updater willInstallUpdate:(SUAppcastItem *)update {
    (void)updater;
    LOG_DEBUG(KDC::Log::instance()->getLogger(), "Updater will install update");
}

- (void)updater:(SPUUpdater *)updater didAbortWithError:(NSError *)error {
    (void)updater;
    if ([error code] != SUNoUpdateError) {
        LOG_WARN(KDC::Log::instance()->getLogger(), "Error: " << [error code]);
        _state = KDC::AbortWithError;
    }
}

- (void)updater:(SPUUpdater *)updater didFinishLoadingAppcast:(SUAppcast *)appcast {
    (void)updater;
    (void)appcast;
    LOG_DEBUG(KDC::Log::instance()->getLogger(), "Finish loading Appcast");
}
@end

// DelegateUserDriverObject class
@interface DelegateUserDriverObject : NSObject <SPUStandardUserDriverDelegate> {
}

- (BOOL)supportsGentleScheduledUpdateReminders;
@end

@implementation DelegateUserDriverObject

- (BOOL)supportsGentleScheduledUpdateReminders {
    return YES;
}

@end

// SparkleUpdater class
namespace KDC {

class SparkleUpdater::Private {
    public:
        SPUUpdater *updater;
        DelegateUpdaterObject *updaterDelegate;
        SPUStandardUserDriver *spuStandardUserDriver;
        DelegateUserDriverObject *delegateUserDriverObject;
};

// Delete ~/Library//Preferences/864VDCS2QY.com.infomaniak.drive.desktopclient.plist to re-test
SparkleUpdater::SparkleUpdater(const QUrl &appCastUrl) : UpdaterServer() {
    d = new Private;

    d->updaterDelegate = [[DelegateUpdaterObject alloc] init];
    [d->updaterDelegate retain];

    d->delegateUserDriverObject = [[DelegateUserDriverObject alloc] init];
    [d->delegateUserDriverObject retain];

    NSBundle *hostBundle = [NSBundle mainBundle];
    NSBundle *applicationBundle = [NSBundle mainBundle];
    d->spuStandardUserDriver = [[SPUStandardUserDriver alloc] initWithHostBundle:hostBundle delegate:d->delegateUserDriverObject];

    d->updater = [[SPUUpdater alloc] initWithHostBundle:hostBundle
                                      applicationBundle:applicationBundle
                                             userDriver:d->spuStandardUserDriver
                                               delegate:d->updaterDelegate];
    [d->updater setAutomaticallyChecksForUpdates:YES];
    [d->updater setAutomaticallyDownloadsUpdates:NO];
    [d->updater setSendsSystemProfile:NO];
    [d->updater retain];

    setUpdateUrl(appCastUrl);

    // Sparkle 1.8 required
    NSString *userAgent = [NSString stringWithUTF8String:KDC::CommonUtility::userAgentString().c_str()];
    [d->updater setUserAgentString:userAgent];
}

SparkleUpdater::~SparkleUpdater() {
    [d->updater release];
    [d->updaterDelegate release];
    [d->spuStandardUserDriver release];
    delete d;
}

void SparkleUpdater::setUpdateUrl(const QUrl &url) {
    NSURL *nsurl = [NSURL URLWithString:[NSString stringWithUTF8String:url.toString().toUtf8().data()]];
    [d->updater setFeedURL:nsurl];
}

void SparkleUpdater::setQuitCallback(const QuitCallback &quitCallback) {
    [d->updaterDelegate setQuitCallback:quitCallback];
}

bool SparkleUpdater::startUpdater() {
    NSError *error;
    bool success = [d->updater startUpdater:&error];

    if (!success) {
        if (error) {
            LOG_DEBUG(KDC::Log::instance()->getLogger(), "Error in startUpdater " << error.description.UTF8String);
        }
        return false;
    }
    return true;
}

void SparkleUpdater::checkForUpdate() {
    if (startUpdater()) {
        [d->updater checkForUpdates];
        [d->spuStandardUserDriver showUpdateInFocus];
    }
}

void SparkleUpdater::backgroundCheckForUpdate() {
    LOG_DEBUG(KDC::Log::instance()->getLogger(), "launching background check");

    if (startUpdater() && !d->updater.sessionInProgress) {
        [d->updater checkForUpdatesInBackground];
    }
    [d->spuStandardUserDriver showUpdateInFocus];
}

int SparkleUpdater::state() const {
    return [d->updaterDelegate downloadState];
}

QString SparkleUpdater::version() const {
    return [[d->updaterDelegate availableVersion] UTF8String];
}

bool SparkleUpdater::updateFound() const {
    DownloadState state = [d->updaterDelegate downloadState];
    return state == FindValidUpdate;
}

void SparkleUpdater::slotStartInstaller() {
    checkForUpdate();
}

}  // namespace KDC
