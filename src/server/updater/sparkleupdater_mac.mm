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

#include <log4cplus/loggingmacros.h>

// DelegateUpdaterObject class
@interface DelegateUpdaterObject : NSObject <SPUUpdaterDelegate> {
@protected
    KDC::DownloadState _state;
    NSString *_availableVersion;

}
- (BOOL)updaterMayCheckForUpdates:(SPUUpdater *)bundle;
- (KDC::DownloadState)downloadState;
- (NSString *)availableVersion;
@end

@implementation DelegateUpdaterObject  //(SUUpdaterDelegateInformalProtocol)
- (instancetype)init {
    self = [super init];
    if (self) {
        _state = KDC::Unknown;
        _availableVersion = @"";
    }
    return self;
}

- (BOOL)updaterMayCheckForUpdates:(SPUUpdater *)bundle {
    Q_UNUSED(bundle)
    LOG_DEBUG(KDC::Log::instance()->getLogger(), "may check: YES");
    return YES;
}

- (KDC::DownloadState)downloadState {
    return _state;
}

- (NSString *)availableVersion {
    return _availableVersion;
}

// Sent when a valid update is found by the update driver.
- (void)updater:(SPUUpdater *)updater didFindValidUpdate:(SUAppcastItem *)update {
    Q_UNUSED(updater)
    LOG_DEBUG(KDC::Log::instance()->getLogger(), "Version: " << update.versionString);
    _state = KDC::FindValidUpdate;
    _availableVersion = [update.versionString copy];
}

// Sent when a valid update is not found.
- (void)updaterDidNotFindUpdate:(SPUUpdater *)update {
    Q_UNUSED(update)
    LOG_DEBUG(KDC::Log::instance()->getLogger(), "No valid update found");
    _state = KDC::DidNotFindUpdate;
}

// Sent immediately before installing the specified update.
- (void)updater:(SPUUpdater *)updater willInstallUpdate:(SUAppcastItem *)update {
    Q_UNUSED(updater)
    LOG_DEBUG(KDC::Log::instance()->getLogger(), "Install update");
}

- (void)updater:(SPUUpdater *)updater didAbortWithError:(NSError *)error {
    Q_UNUSED(updater)
    if ([error code] != SUNoUpdateError) {
        LOG_WARN(KDC::Log::instance()->getLogger(), "Error: " << [error code]);
        _state = KDC::AbortWithError;
    }
}

- (void)updater:(SPUUpdater *)updater didFinishLoadingAppcast:(SUAppcast *)appcast {
    Q_UNUSED(updater)
    Q_UNUSED(appcast)
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
