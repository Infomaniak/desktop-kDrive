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

#include "sparkleupdater.h"

#include "updatechecker.h"
#include "common/utility.h"
#include "libcommon/utility/utility.h"
#include "libparms/db/parmsdb.h"

#include "libcommonserver/log/log.h"
#include <log4cplus/loggingmacros.h>

#include <Sparkle/Sparkle.h>
#include <Sparkle/SPUUpdaterDelegate.h>

// DelegateUpdaterObject class
@interface DelegateUpdaterObject : NSObject <SPUUpdaterDelegate> {
@protected
    KDC::DownloadState _state;
    NSString *_availableVersion;
    NSString *_feedUrl;
    std::function<void()> _quitCallback;
}
- (BOOL)updaterMayCheckForUpdates:(SPUUpdater *)updater;
- (BOOL)updaterShouldRelaunchApplication:(SPUUpdater *)updater;
- (void)updaterWillRelaunchApplication:(SPUUpdater *)updater;
- (KDC::DownloadState)downloadState;
- (NSString *)availableVersion;
- (void)setQuitCallback:(std::function<void()>)quitCallback;
@end

@implementation DelegateUpdaterObject  //(SUUpdaterDelegateInformalProtocol)
- (instancetype)init {
    self = [super init];
    if (self) {
        _state = KDC::Unknown;
        _availableVersion = @"";
        _feedUrl = @"";
        _quitCallback = nullptr;
    }
    return self;
}

- (BOOL)updaterMayCheckForUpdates:(SPUUpdater *)updater {
    (void)updater;
    LOG_DEBUG(KDC::Log::instance()->getLogger(), "may check: YES");
    return YES;
}

- (nullable NSString *)feedURLStringForUpdater:(SPUUpdater *)updater {
    return _feedUrl;
}

- (void)setCustomFeedUrl:(std::string)url {
    _feedUrl = [NSString stringWithUTF8String:url.c_str()];
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

- (void)setQuitCallback:(std::function<void()>)quitCallback {
    LOG_DEBUG(KDC::Log::instance()->getLogger(), "Set quitCallback");
    _quitCallback = quitCallback;
}

// Sent when a valid update is found by the update driver.
- (void)updater:(SPUUpdater *)updater didFindValidUpdate:(SUAppcastItem *)update {
    (void)updater;
    LOG_DEBUG(KDC::Log::instance()->getLogger(), "Version: " << [update.versionString UTF8String]);
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

SparkleUpdater::SparkleUpdater() {
    d = new Private;
}

SparkleUpdater::~SparkleUpdater() {
    deleteUpdater();
    delete d;
}

void SparkleUpdater::onUpdateFound() {
    _feedUrl = updateChecker()->versionInfo().downloadUrl;
    startInstaller();
}

void SparkleUpdater::setQuitCallback(const std::function<void()> &quitCallback) {
    [d->updaterDelegate setQuitCallback:quitCallback];
}

void SparkleUpdater::startInstaller() {
    reset(_feedUrl);
    [d->updater checkForUpdates];
    [d->spuStandardUserDriver showUpdateInFocus];
}

void SparkleUpdater::reset(const std::string &url) {
    [d->spuStandardUserDriver dismissUpdateInstallation];
    deleteUpdater();

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

    // Sparkle 1.8 required
    NSString *userAgent = [NSString stringWithUTF8String:KDC::CommonUtility::userAgentString().c_str()];
    [d->updater setUserAgentString:userAgent];

    // Migrate away from using `-[SPUUpdater setFeedURL:]`
    [d->updater clearFeedURLFromUserDefaults];

    [d->updaterDelegate setCustomFeedUrl:_feedUrl];

    if(startSparkleUpdater()) {
        LOG_INFO(KDC::Log::instance()->getLogger(), "Sparkle updater succesfully started with feed URL: " << url.c_str());
    }
}

void SparkleUpdater::deleteUpdater() {
    [d->updater release];
    [d->updaterDelegate release];
    [d->spuStandardUserDriver release];
}

bool SparkleUpdater::startSparkleUpdater() {
    NSError *error = nullptr;
    bool success = [d->updater startUpdater:&error];

    if (!success) {
        if (error) {
            LOG_DEBUG(KDC::Log::instance()->getLogger(), "Error in startUpdater " << error.description.UTF8String);
            setState(UpdateStateV2::UpdateError);
        }
        return false;
    }

    return true;
}

}  // namespace KDC
