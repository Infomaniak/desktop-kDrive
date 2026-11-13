/*
 * Infomaniak kDrive - Desktop
 * Copyright (C) 2023-2026 Infomaniak Network SA
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

#include "analyticscatalog.h"

#include <QHash>

#include <initializer_list>
#include <utility>

using namespace Qt::StringLiterals;

namespace KDC::AnalyticsCatalog {

namespace {

// Pairs of an id suffix and its Matomo name: the event name, or the category of a page view.
using NamesBySuffix = std::initializer_list<std::pair<QString, QString>>;

// Detail of the switches whose new state becomes the event value.
constexpr auto switchValueDetail = "value"_L1;

/**
 * Registers the events `<prefix>.<suffix>` of one Matomo category. `valueDetail`, when set, names the detail read as the
 * event value.
 */
void addEvents(QHash<QString, Event> &events, const QString &prefix, const QString &category, const NamesBySuffix names,
               const QString &valueDetail = {}) {
    for (const auto &[suffix, name]: names) {
        (void) events.insert(prefix + u'.' + suffix, {category, name, valueDetail});
    }
}

void addPageViews(QHash<QString, QString> &pageViews, const QString &prefix, const NamesBySuffix categories) {
    for (const auto &[suffix, category]: categories) {
        (void) pageViews.insert(suffix.isEmpty() ? prefix : prefix + u'.' + suffix, category);
    }
}

void addMainWindowEvents(QHash<QString, Event> &events) {
    addEvents(events, u"main.sidebar"_s, u"navBar"_s,
              {{u"home"_s, u"openHome"_s},
               {u"activities"_s, u"openActivity"_s},
               {u"storage"_s, u"openStorage"_s},
               {u"openSyncFolder"_s, u"openSyncDir"_s}});
    addEvents(events, u"main.toolbar"_s, u"navBar"_s, {{u"settings"_s, u"openSettings"_s}, {u"support"_s, u"openSupportWeb"_s}});
    addEvents(events, u"main.toolbar"_s, u"startPauseButton"_s,
              {{u"pauseSync"_s, u"pauseSync"_s}, {u"resumeSync"_s, u"startSync"_s}});
    addEvents(events, u"main.sidebar.syncSelector"_s, u"syncSelector"_s, {{u"select"_s, u"syncSelected"_s}});

    addEvents(events, u"main.home"_s, u"homePage"_s,
              {{u"shortcuts.favorites"_s, u"openFavoritesWeb"_s},
               {u"shortcuts.shared"_s, u"openSharedWeb"_s},
               {u"shortcuts.onlineDrive"_s, u"openKDriveWeb"_s},
               {u"shortcuts.trash"_s, u"openTrashWeb"_s},
               {u"errorBanner.showErrors"_s, u"openErrors"_s},
               {u"primaryAction.showActivities"_s, u"openActivity"_s},
               {u"primaryAction.resumeSync"_s, u"startSync"_s},
               {u"primaryAction.signIn"_s, u"openSignInWeb"_s},
               {u"primaryAction.configureSync"_s, u"configureSync"_s},
               {u"avatar.userSettings"_s, u"openUserSettings"_s},
               {u"avatar.syncSettings"_s, u"openSyncSettings"_s}});

    addEvents(events, u"main.activities"_s, u"activityPage"_s,
              {{u"filterMenu.allActivities"_s, u"showAllActivities"_s},
               {u"filterMenu.thisComputer"_s, u"showMyActivities"_s},
               {u"errorBanner.fixAll"_s, u"openErrors"_s},
               {u"row.openFolder"_s, u"openItemFolder"_s},
               {u"options.openLocal"_s, u"openItem"_s},
               {u"options.openOnline"_s, u"openItemWeb"_s},
               {u"options.copyShareLink"_s, u"copyItemWebLink"_s},
               {u"options.fixErrors"_s, u"openItemError"_s}});

    addEvents(events, u"main.search"_s, u"search"_s, {{u"openLocal"_s, u"openItem"_s}, {u"openOnline"_s, u"openItemWeb"_s}});
}

// The errors page, one `errors` event per error kind managed from its row, and the version conflicts pages.
void addErrorsEvents(QHash<QString, Event> &events) {
    addEvents(events, u"main.errors"_s, u"errorPage"_s,
              {{u"refresh"_s, u"refreshErrors"_s}, {u"showActivities"_s, u"openActivity"_s}});
    addEvents(events, u"main.errors.row"_s, u"errors"_s,
              {{u"conflicts"_s, u"manageMultipleConflicts"_s},
               {u"conflict"_s, u"manageSingleConflict"_s},
               {u"localAccess"_s, u"manageFileAccessError"_s},
               {u"notEnoughDiskSpace"_s, u"manageNotEnoughDiskSpace"_s},
               {u"systemNotEnoughDiskSpace"_s, u"manageNotEnoughDiskSpace"_s},
               {u"forbiddenChar"_s, u"manageUnsupportedChar"_s},
               {u"forbiddenCharEndWithSpace"_s, u"manageEndsWithSpace"_s},
               {u"forbiddenCharOnlySpaces"_s, u"manageSpaceOnly"_s},
               {u"nameLength"_s, u"manageFileNameTooLong"_s},
               {u"pathLength"_s, u"managePathTooLong"_s},
               {u"excludedByTemplate"_s, u"manageExcludedFile"_s},
               {u"fileRescued"_s, u"manageRescuedFile"_s},
               {u"quotaExceeded"_s, u"manageQuotaExceeded"_s},
               {u"invalidSyncDirAccess"_s, u"manageSyncDirAccessError"_s},
               {u"systemSyncDirAccess"_s, u"manageSyncDirAccessError"_s},
               {u"systemSyncDirDiskMissing"_s, u"manageSyncDirDiskMissing"_s},
               {u"dataSyncDirChanged"_s, u"manageSyncDirChanged"_s},
               {u"backErrorDriveAccess"_s, u"manageDriveAccessDenied"_s},
               {u"backErrorDriveAsleep"_s, u"manageDriveAsleep"_s},
               {u"backErrorDriveMaintenance"_s, u"manageDriveInMaintenance"_s},
               {u"backErrorDriveNotRenew"_s, u"manageDriveNotRenew"_s},
               {u"invalidToken"_s, u"manageInvalidToken"_s},
               {u"notEnoughINotifyWatches"_s, u"manageNotEnoughINotifyWatches"_s},
               {u"unknown"_s, u"manageUnexpectedError"_s}});

    addEvents(events, u"main.conflicts.quick"_s, u"batchConflictResolutionPage"_s,
              {{u"apply"_s, u"apply"_s}, {u"manageIndividually"_s, u"openIndividualResolution"_s}});
    addEvents(events, u"main.conflicts.list"_s, u"individualConflictResolutionPage"_s,
              {{u"startChoices"_s, u"startChoices"_s}, {u"chooseVersion"_s, u"manageSingleConflict"_s}});
    addEvents(events, u"main.conflicts.versionDialog"_s, u"errors"_s,
              {{u"open"_s, u"showConflictVersion"_s}, {u"validate"_s, u"validateConflictResolution"_s}});

    addEvents(events, u"main.blockingError"_s, u"driveAccessDeniedPage"_s, {{u"accessDenied.restartSync"_s, u"startSync"_s}});
    addEvents(events, u"main.blockingError"_s, u"maintenanceErrorPage"_s, {{u"maintenance.restartSync"_s, u"startSync"_s}});
    addEvents(events, u"main.blockingError.notRenew"_s, u"notRenewErrorPage"_s,
              {{u"restartSync"_s, u"startSync"_s}, {u"renewSubscription"_s, u"openRenewWeb"_s}});
    addEvents(events, u"main.blockingError"_s, u"loginErrorPage"_s, {{u"loggedOut.signIn"_s, u"openSignInWeb"_s}});
}

// Both drive setup dialogs: the advanced settings of onboarding and the drive activation from Settings.
void addDriveSetupDialogEvents(QHash<QString, Event> &events, const QString &dialogId) {
    addEvents(events, dialogId, u"driveSetupDialog"_s,
              {{u"cancel"_s, u"cancel"_s},
               {u"validate"_s, u"confirm"_s},
               {u"summary.configureDrive"_s, u"configureSync"_s},
               {u"drive.changeFolder"_s, u"changeSyncLocalLocation"_s},
               {u"drive.selectFolders"_s, u"changeSyncExclusions"_s}});
}

void addOnboardingEvents(QHash<QString, Event> &events) {
    addEvents(events, u"onboarding.login"_s, u"onboardingWelcomePage"_s,
              {{u"signIn"_s, u"openSignInWeb"_s}, {u"createAccount"_s, u"openSignUpWeb"_s}});
    addEvents(events, u"onboarding"_s, u"onboardingSyncConfigurationPage"_s,
              {{u"driveSelection.selectDrive"_s, u"selectDrive"_s},
               {u"driveSelection.unselectDrive"_s, u"unselectDrive"_s},
               {u"driveSelection.advancedSettings"_s, u"openAdvancedSettings"_s},
               {u"driveSelection.continue"_s, u"confirm"_s},
               {u"noDrive.showOffers"_s, u"openOffersWeb"_s},
               {u"noDrive.startForFree"_s, u"openStartFreeWeb"_s}});
    addDriveSetupDialogEvents(events, u"syncConfig"_s);
}

void addSettingsEvents(QHash<QString, Event> &events) {
    addEvents(events, u"settings.general"_s, u"generalSettingsPage"_s,
              {{u"autoStart"_s, u"changeAutoStart"_s},
               {u"notifications"_s, u"changeNotifications"_s},
               {u"moveToTrash"_s, u"changeMoveToTrash"_s}},
              switchValueDetail);
    addEvents(events, u"settings.general"_s, u"generalSettingsPage"_s,
              {{u"language"_s, u"changeLanguage"_s},
               {u"downloadUpdate"_s, u"startUpdate"_s},
               {u"support"_s, u"openSupportWeb"_s},
               {u"feedback"_s, u"openFeedbackWeb"_s}});

    addEvents(events, u"settings.accounts"_s, u"accountsSettingsPage"_s,
              {{u"connectAccount"_s, u"openOnboarding"_s},
               {u"user.disconnect"_s, u"disconnect"_s},
               {u"disconnectDialog.keepAccount"_s, u"cancelDisconnect"_s},
               {u"disconnectDialog.logOut"_s, u"confirmDisconnect"_s},
               {u"drive.manage"_s, u"openDriveSettings"_s}});
    addEvents(events, u"settings.accounts.driveManagement"_s, u"driveManagementPage"_s,
              {{u"openLocalFolder"_s, u"openSyncDir"_s},
               {u"manageSelection"_s, u"showItemExclusion"_s},
               {u"enableSync"_s, u"create"_s},
               {u"deleteSync"_s, u"delete"_s},
               {u"advancedSyncs"_s, u"manageAdvancedSync"_s}});
    addEvents(events, u"settings.accounts.advancedSyncs"_s, u"driveAdvancedSyncsPage"_s,
              {{u"add"_s, u"create"_s},
               {u"card.deleteSync"_s, u"delete"_s},
               {u"card.openLocalFolder"_s, u"openSyncDir"_s},
               {u"card.manageSelection"_s, u"showItemExclusion"_s}});
    addDriveSetupDialogEvents(events, u"settings.syncActivation"_s);

    addEvents(events, u"settings.advanced"_s, u"advancedSettingsPage"_s,
              {{u"dataManagement.matomo.allowTracking"_s, u"changeMatomoSettings"_s},
               {u"dataManagement.sentry.allowTracking"_s, u"changeSentrySettings"_s},
               {u"debug.useLog"_s, u"changeLogIsOn"_s},
               {u"debug.purgeOldLogs"_s, u"changeLogPurge"_s}},
              switchValueDetail);
    addEvents(events, u"settings.advanced"_s, u"advancedSettingsPage"_s,
              {{u"fileExclusions"_s, u"openExclusionRules"_s},
               {u"dataManagement.sources"_s, u"openSourceCodeWeb"_s},
               {u"network.proxyType"_s, u"changeProxyMode"_s},
               {u"network.save"_s, u"changeProxySettings"_s},
               {u"debug.logLevel"_s, u"changeLogVerbosity"_s},
               {u"debug.openFolder"_s, u"openLogFolder"_s},
               {u"debug.sendLogsDialog.send"_s, u"sendLogToSupport"_s},
               {u"debug.sendLogsDialog.cancelUpload"_s, u"cancelLogToSupport"_s}});
}

QHash<QString, Event> buildEvents() {
    QHash<QString, Event> events;
    addMainWindowEvents(events);
    addErrorsEvents(events);
    addOnboardingEvents(events);
    addSettingsEvents(events);
    return events;
}

QHash<QString, QString> buildPageViews() {
    QHash<QString, QString> pageViews;
    addPageViews(pageViews, u"main"_s,
                 {{u"home"_s, u"homePage"_s},
                  {u"activities"_s, u"activityPage"_s},
                  {u"activities.errors"_s, u"errorPage"_s},
                  {u"activities.conflictsQuick"_s, u"batchConflictResolutionPage"_s},
                  {u"activities.conflictsList"_s, u"individualConflictResolutionPage"_s},
                  {u"storage"_s, u"storagePage"_s}});
    addPageViews(pageViews, u"main.blockingError"_s,
                 {{u"asleep"_s, u"asleepErrorPage"_s},
                  {u"accessDenied"_s, u"driveAccessDeniedPage"_s},
                  {u"loggedOut"_s, u"loginErrorPage"_s},
                  {u"maintenance"_s, u"maintenanceErrorPage"_s},
                  {u"notRenew"_s, u"notRenewErrorPage"_s}});
    addPageViews(pageViews, u"onboarding"_s,
                 {{u"login"_s, u"onboardingWelcomePage"_s},
                  {u"driveSelection"_s, u"onboardingSyncConfigurationPage"_s},
                  {u"ready"_s, u"onboardingFinalPage"_s}});
    addPageViews(pageViews, u"syncConfig"_s, {{QString(), u"driveSetupDialog"_s}});
    addPageViews(pageViews, u"settings"_s,
                 {{u"syncActivation"_s, u"driveSetupDialog"_s},
                  {u"general"_s, u"generalSettingsPage"_s},
                  {u"accounts"_s, u"accountsSettingsPage"_s},
                  {u"advanced"_s, u"advancedSettingsPage"_s},
                  {u"accounts.driveManagement"_s, u"driveManagementPage"_s},
                  {u"accounts.advancedSyncs"_s, u"driveAdvancedSyncsPage"_s}});
    return pageViews;
}

} // namespace

std::optional<Event> eventForAction(const QString &actionId) {
    static const QHash<QString, Event> events = buildEvents();
    if (const auto it = events.constFind(actionId); it != events.cend()) {
        return *it;
    }

    return std::nullopt;
}

std::optional<QString> pageViewForScreen(const QString &screenId) {
    static const QHash<QString, QString> pageViews = buildPageViews();
    if (const auto it = pageViews.constFind(screenId); it != pageViews.cend()) {
        return *it;
    }

    return std::nullopt;
}

} // namespace KDC::AnalyticsCatalog
