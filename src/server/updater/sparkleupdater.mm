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

#include "sparkleupdater.h"

#include "updatechecker.h"
#include "libcommon/utility/utility.h"
#include "libparms/db/parmsdb.h"
#include "requests/parameterscache.h"

#include "libcommonserver/log/log.h"
#include <log4cplus/loggingmacros.h>

#include <Sparkle/Sparkle.h>
#include <Sparkle/SPUUpdaterDelegate.h>

// DelegateUpdaterObject class
@interface DelegateUpdaterObject : NSObject <SPUUpdaterDelegate> {
@protected
    NSString *_availableVersion;
    NSString *_feedUrl;
    std::function<void()> _quitCallback;
    std::function<void()> _skipCallback;
}
- (BOOL)updaterMayCheckForUpdates:(SPUUpdater *)updater;

- (BOOL)updaterShouldRelaunchApplication:(SPUUpdater *)updater;

- (void)updaterWillRelaunchApplication:(SPUUpdater *)updater;

- (NSString *)availableVersion;

- (void)setQuitCallback:(std::function<void()>)quitCallback;

- (void)setSkipCallback:(std::function<void()>)skipCallback;

- (void)updater:(SPUUpdater *)updater
        userDidMakeChoice:(SPUUserUpdateChoice)choice
                forUpdate:(SUAppcastItem *)updateItem
                    state:(SPUUserUpdateState *)state;
@end

@implementation DelegateUpdaterObject //(SUUpdaterDelegateInformalProtocol)
- (instancetype)init {
    self = [super init];
    if (self) {
        _availableVersion = @"";
        _feedUrl = @"";
        _quitCallback = nullptr;
        _skipCallback = nullptr;
    }
    return self;
}

- (BOOL)updaterMayCheckForUpdates:(SPUUpdater *)updater {
    (void) updater;
    LOG_DEBUG(KDC::Log::instance()->getLogger(), "may check: YES");
    return YES;
}

- (nullable NSString *)feedURLStringForUpdater:(SPUUpdater *)updater {
    return _feedUrl;
}

- (void)setCustomFeedUrl:(std::string)url {
    _feedUrl = [[NSString alloc] initWithUTF8String:url.c_str()];
}

- (BOOL)updaterShouldRelaunchApplication:(SPUUpdater *)updater {
    (void) updater;
    LOG_DEBUG(KDC::Log::instance()->getLogger(), "Updater should relaunch app: YES");
    return YES;
}

- (void)updaterWillRelaunchApplication:(SPUUpdater *)updater {
    (void) updater;
    LOG_DEBUG(KDC::Log::instance()->getLogger(), "Updater will relaunch app");

    if (bool found = false; !KDC::ParmsDb::instance()->updateAppState(KDC::AppStateKey::LastServerSelfRestartDate,
                                                                      KDC::selfRestarterDisableValue, found) ||
                            !found) { // Deactivate the selfRestarter
        LOG_ERROR(KDC::Log::instance()->getLogger(), "Error in ParmsDb::updateAppState");
    }

    if (_quitCallback) {
        LOG_DEBUG(KDC::Log::instance()->getLogger(), "Ask app client to quit");
        _quitCallback();
    }
}

- (NSString *)availableVersion {
    return _availableVersion;
}

- (void)setQuitCallback:(std::function<void()>)quitCallback {
    LOG_DEBUG(KDC::Log::instance()->getLogger(), "Set quitCallback");
    _quitCallback = quitCallback;
}

- (void)setSkipCallback:(std::function<void()>)skipCallback {
    LOG_DEBUG(KDC::Log::instance()->getLogger(), "Set skipCallback");
    _skipCallback = skipCallback;
}

- (void)updater:(SPUUpdater *)updater
        userDidMakeChoice:(SPUUserUpdateChoice)choice
                forUpdate:(SUAppcastItem *)updateItem
                    state:(SPUUserUpdateState *)state {
    if (choice == SPUUserUpdateChoiceSkip) {
        LOG_DEBUG(KDC::Log::instance()->getLogger(), "Version " << [updateItem.versionString UTF8String] << " skipped!");
        if (_skipCallback) {
            _skipCallback();
        }
    }
}

// Sent when a valid update is found by the update driver.
- (void)updater:(SPUUpdater *)updater didFindValidUpdate:(SUAppcastItem *)updateItem {
    (void) updater;
    LOG_DEBUG(KDC::Log::instance()->getLogger(), "Version: " << [updateItem.versionString UTF8String]);
    _availableVersion = [updateItem.versionString copy];
}

// Sent when a valid update is not found.
- (void)updaterDidNotFindUpdate:(SPUUpdater *)update error:(nonnull NSError *)error {
    (void) update;
    LOG_DEBUG(KDC::Log::instance()->getLogger(), "No valid update found - Error code: " << [error code] << ", reason: " <<
                                                         [error.userInfo[SPUNoUpdateFoundReasonKey] integerValue]);
}

// Sent immediately before installing the specified update.
- (void)updater:(SPUUpdater *)updater willInstallUpdate:(SUAppcastItem *)update {
    (void) updater;
    LOG_DEBUG(KDC::Log::instance()->getLogger(), "Updater will install update");
}

- (void)updater:(SPUUpdater *)updater didAbortWithError:(NSError *)error {
    (void) updater;
    if ([error code] != SUNoUpdateError) {
        LOG_WARN(KDC::Log::instance()->getLogger(), "Error: " << [error code]);
    }
}

- (void)updater:(SPUUpdater *)updater didFinishLoadingAppcast:(SUAppcast *)appcast {
    (void) updater;
    (void) appcast;
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
        SPUUpdater *updater = nil;
        DelegateUpdaterObject *updaterDelegate = nil;
        SPUStandardUserDriver *spuStandardUserDriver = nil;
        DelegateUserDriverObject *delegateUserDriverObject = nil;
};

SparkleUpdater::SparkleUpdater() {
    d = new Private;
    reset();
}

SparkleUpdater::~SparkleUpdater() {
    delete d;
}

void SparkleUpdater::onUpdateFound() {
    if (isVersionSkipped(versionInfo(_currentChannel).fullVersion())) {
        LOG_INFO(KDC::Log::instance()->getLogger(),
                 "Version " << versionInfo(_currentChannel).fullVersion().c_str() << " is skipped.");
        return;
    }

    if (!d->updater) {
        LOG_WARN(KDC::Log::instance()->getLogger(), "Initialization error!");
        return;
    }

    if ([d->updater sessionInProgress]) {
        LOG_INFO(KDC::Log::instance()->getLogger(),
                 "An update window is already opened or installation is in progress. No need to start a new one.");
        return;
    }
    startInstaller();
}

void SparkleUpdater::setQuitCallback(const std::function<void()> &quitCallback) {
    if (!d->updaterDelegate) {
        LOG_WARN(KDC::Log::instance()->getLogger(), "Initialization error!");
        return;
    }
    [d->updaterDelegate setQuitCallback:quitCallback];
}

void SparkleUpdater::startInstaller() {
    reset(versionInfo(_currentChannel).downloadUrl);

    if (!d->updater || !d->spuStandardUserDriver) {
        LOG_WARN(KDC::Log::instance()->getLogger(), "Initialization error!");
        return;
    }
    [d->updater checkForUpdatesInBackground];
    [d->spuStandardUserDriver showUpdateInFocus];
}

void SparkleUpdater::unskipVersion() {
    AbstractUpdater::unskipVersion();

    // Discard skipped version in Sparkle
    [[NSUserDefaults standardUserDefaults] removeObjectForKey:@ "SUSkippedVersion"];
    [[NSUserDefaults standardUserDefaults] synchronize];
}

void SparkleUpdater::reset(const std::string &url /*= ""*/) {
    // Dismiss an eventual ongoing update
    if (d->spuStandardUserDriver) [d->spuStandardUserDriver dismissUpdateInstallation];

    d->updaterDelegate = [[DelegateUpdaterObject alloc] init];
    const std::function<void()> skipCallback = std::bind_front(&SparkleUpdater::skipVersionCallback, this);
    [d->updaterDelegate setSkipCallback:skipCallback];

    d->delegateUserDriverObject = [[DelegateUserDriverObject alloc] init];

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

    // Sparkle 1.8 required
    NSString *userAgent = [NSString stringWithUTF8String:KDC::CommonUtility::userAgentString().c_str()];
    [d->updater setUserAgentString:userAgent];

    // Migrate away from using `-[SPUUpdater setFeedURL:]`
    [d->updater clearFeedURLFromUserDefaults];

    if (!url.empty()) {
        [d->updaterDelegate setCustomFeedUrl:url];

        if (startSparkleUpdater()) {
            LOG_INFO(KDC::Log::instance()->getLogger(), "Sparkle updater succesfully started with feed URL: " << url);
        }
    }
}

bool SparkleUpdater::startSparkleUpdater() {
    LOG_DEBUG(KDC::Log::instance()->getLogger(), "Starting updater...");
    NSError *error = nullptr;
    bool success = [d->updater startUpdater:&error];

    if (!success) {
        if (error) {
            LOG_WARN(KDC::Log::instance()->getLogger(), "Error in startUpdater " << error.description.UTF8String);
            setState(UpdateState::UpdateError);
        }
        return false;
    }

    return true;
}

void SparkleUpdater::skipVersionCallback() {
    skipVersion(versionInfo(_currentChannel).fullVersion());
}

} // namespace KDC
