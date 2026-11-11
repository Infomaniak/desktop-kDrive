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

#include "generalsettingscontroller.h"

#include "app/appconstants.h"
#include "app/cache/appcache.h"
#include "app/cache/parametersstore.h"
#include "app/services/translationservice.h"
#include "app/services/updatestatusservice.h"
#include "libcommon/utility/utility.h"
#include "libcommon/theme/theme.h"

#include "version.h"

#include <QDate>
#include <QDesktopServices>
#include <QLoggingCategory>
#include <QPointer>

#include <algorithm>

namespace KDC {

using namespace Qt::StringLiterals;

namespace {
Q_LOGGING_CATEGORY(lcGeneralSettings, "gui.v4.settings", QtInfoMsg)

QString installedVersion() {
    return QString::fromLatin1(KDRIVE_VERSION_STRING);
}

QString installedRelease() {
    return u"%1.%2.%3"_s.arg(KDRIVE_VERSION_MAJOR).arg(KDRIVE_VERSION_MINOR).arg(KDRIVE_VERSION_PATCH);
}

// Channels without their own choice in the beta program are shown as "Do not join".
DistributionChannel displayedChannel(const DistributionChannel channel) {
    switch (channel) {
        case DistributionChannel::Beta:
        case DistributionChannel::Internal:
        case DistributionChannel::Test:
            return channel;
        default:
            return DistributionChannel::Prod;
    }
}

QString channelLabel(const DistributionChannel channel) {
    switch (channel) {
        case DistributionChannel::Beta:
            return qtTrId("releaseChannelBeta");
        case DistributionChannel::Internal:
            return qtTrId("releaseChannelInternal");
        case DistributionChannel::Test:
            return qtTrId("releaseChannelTest");
        default:
            return qtTrId("doNotJoin");
    }
}

QString versionDetails(const QString &version, const uint64_t build) {
    return qtTrId("aboutAppVersionCopyrightMac").arg(version).arg(build).arg(QDate::currentDate().year());
}
} // namespace

GeneralSettingsController::GeneralSettingsController(ParametersStore &parametersStore, ParametersService &parametersService,
                                                     TranslationService &translationService,
                                                     UpdateStatusService &updateStatusService, const AppCache &appCache,
                                                     QObject *const parent) :
    QObject(parent),
    _parametersStore(parametersStore),
    _parametersService(parametersService),
    _translationService(translationService),
    _updateStatusService(updateStatusService),
    _appCache(appCache),
    _releaseNotes(parametersStore) {
    (void) connect(&parametersStore, &ParametersStore::parametersChanged, this, &GeneralSettingsController::changed);
    (void) connect(&appCache, &AppCache::usersChanged, this, &GeneralSettingsController::changed);
    (void) connect(&translationService, &TranslationService::languageChanged, this, &GeneralSettingsController::changed);
    (void) connect(&updateStatusService, &UpdateStatusService::changed, this, &GeneralSettingsController::changed);
    (void) connect(&_releaseNotes, &ReleaseNotesService::changed, this, &GeneralSettingsController::releaseNotesChanged);
}

bool GeneralSettingsController::ready() const {
    return _parametersStore.parameters().has_value();
}

bool GeneralSettingsController::autoStart() const {
    const auto parameters = _parametersStore.parameters();
    return parameters && parameters->autoStart();
}

bool GeneralSettingsController::notificationsEnabled() const {
    const auto parameters = _parametersStore.parameters();
    return parameters && parameters->notificationsDisabled() == NotificationsDisabled::Never;
}

bool GeneralSettingsController::moveToTrash() const {
    const auto parameters = _parametersStore.parameters();
    return parameters && parameters->moveToTrash();
}

int32_t GeneralSettingsController::language() const {
    const auto parameters = _parametersStore.parameters();
    return static_cast<int32_t>(parameters ? parameters->language() : Language::Default);
}

QVariantList GeneralSettingsController::languages() {
    return TranslationService::languages();
}

QString GeneralSettingsController::errorText() const {
    switch (_error) {
        case Error::Save:
            return qtTrId("linuxSettingsSaveError");
        case Error::OpenUrl:
            return qtTrId("errorOpeningLocalURL").arg(_failedUrl.toDisplayString());
        case Error::None:
            return {};
    }

    return {};
}

QString GeneralSettingsController::updateText() const {
    if (_updateStatusService.readyToInstall() && _updateStatusService.version()) {
        return qtTrId("updateInstalled").arg(releaseVersion());
    }

    if (_updateStatusService.available()) {
        return _updateStatusService.version() ? qtTrId("updateAvailable").arg(releaseVersion()) : qtTrId("updateDialogTitle");
    }

    switch (_updateStatusService.state()) {
        case UpdateState::UpToDate:
            return qtTrId("appUpToDate");
        case UpdateState::Checking:
            return qtTrId("linuxSettingsCheckingUpdates");
        default:
            return qtTrId("linuxSettingsUpdateUnavailable");
    }
}

bool GeneralSettingsController::updateAvailable() const {
    return _updateStatusService.available();
}

bool GeneralSettingsController::updateDownloading() const {
    return _updateStatusService.downloading();
}

bool GeneralSettingsController::updateReadyToInstall() const {
    return _updateStatusService.readyToInstall();
}

bool GeneralSettingsController::updateInstallable() const {
    return _updateStatusService.installable();
}

bool GeneralSettingsController::automaticUpdates() const {
    const auto parameters = _parametersStore.parameters();
    return parameters && parameters->automaticUpdates();
}

int32_t GeneralSettingsController::distributionChannel() const {
    const auto parameters = _parametersStore.parameters();
    const auto channel = parameters ? parameters->distributionChannel() : DistributionChannel::Prod;
    return static_cast<int32_t>(displayedChannel(channel));
}

QString GeneralSettingsController::betaProgramStatus() const {
    return channelLabel(static_cast<DistributionChannel>(distributionChannel()));
}

// Internal and Test are offered only when a connected account belongs to the staff. The current channel stays listed
// so that the picker never shows a wrong value, even after the staff account is disconnected.
QVariantList GeneralSettingsController::distributionChannels() const {
    const auto users = _appCache.users();
    const bool staff = std::ranges::any_of(users, [](const User &user) { return user.connected() && user.isStaff(); });

    std::vector<DistributionChannel> channels{DistributionChannel::Prod, DistributionChannel::Beta};
    if (staff) {
        channels.push_back(DistributionChannel::Internal);
        channels.push_back(DistributionChannel::Test);
    }

    const auto current = static_cast<DistributionChannel>(distributionChannel());
    if (std::ranges::find(channels, current) == channels.end()) {
        channels.push_back(current);
    }

    QVariantList options;
    for (const auto channel: channels) {
        options.append(QVariantMap{{u"value"_s, static_cast<int32_t>(channel)}, {u"label"_s, channelLabel(channel)}});
    }

    return options;
}

QString GeneralSettingsController::releaseVersion() const {
    const auto &version = _updateStatusService.version();
    if (!version) {
        return installedVersion();
    }

    // A new build of the installed release shares its tag: the build number tells them apart.
    const auto tag = QString::fromStdString(version->tag);
    return tag == installedRelease() ? u"%1.%2"_s.arg(tag).arg(version->buildVersion) : tag;
}

QString GeneralSettingsController::releaseDetails() const {
    const auto &version = _updateStatusService.version();
    return version ? versionDetails(QString::fromStdString(version->tag), version->buildVersion) : installedDetails();
}

QString GeneralSettingsController::installedDetails() {
    return versionDetails(installedVersion(), KDRIVE_VERSION_BUILD);
}

void GeneralSettingsController::save(const ParametersService::ParametersMutation &mutation) {
    if (!ready() || _saving) {
        return;
    }

    _saving = true;
    _error = Error::None;
    emit changed();

    _parametersService.updateParameters(mutation, [self = QPointer<GeneralSettingsController>(this)](const ExitInfo &result) {
        if (!self) {
            return;
        }

        self->_saving = false;
        if (!result) {
            self->_error = Error::Save;
        }
        emit self->changed();
    });
}

void GeneralSettingsController::setAutoStart(const bool enabled) {
    if (enabled == autoStart()) {
        return;
    }

    save([enabled](Parameters &parameters) { parameters.setAutoStart(enabled); });
}

void GeneralSettingsController::setNotificationsEnabled(const bool enabled) {
    if (enabled == notificationsEnabled()) {
        return;
    }

    save([enabled](Parameters &parameters) {
        parameters.setNotificationsDisabled(enabled ? NotificationsDisabled::Never : NotificationsDisabled::Always);
    });
}

void GeneralSettingsController::setAutomaticUpdates(const bool enabled) {
    if (enabled == automaticUpdates()) {
        return;
    }

    save([enabled](Parameters &parameters) { parameters.setAutomaticUpdates(enabled); });
}

void GeneralSettingsController::setMoveToTrash(const bool enabled) {
    if (enabled == moveToTrash()) {
        return;
    }

    save([enabled](Parameters &parameters) { parameters.setMoveToTrash(enabled); });
}

void GeneralSettingsController::setDistributionChannel(const int32_t channelValue) {
    const auto channel = static_cast<DistributionChannel>(channelValue);
    if (displayedChannel(channel) != channel) {
        qCWarning(lcGeneralSettings) << "Ignoring unsupported distribution channel" << channelValue;
        return;
    }

    // Compared with the stored channel: a hidden channel such as Next is displayed as Prod but must still be replaced.
    if (const auto parameters = _parametersStore.parameters(); parameters && parameters->distributionChannel() == channel) {
        return;
    }

    qCInfo(lcGeneralSettings) << "Distribution channel changed | channel:" << channelValue;
    save([channel](Parameters &parameters) { parameters.setDistributionChannel(channel); });
}

void GeneralSettingsController::setLanguage(const int32_t languageValue) {
    if (languageValue < static_cast<int32_t>(Language::Default) || languageValue >= static_cast<int32_t>(Language::EnumEnd)) {
        return;
    }

    if (languageValue == language()) {
        return;
    }

    save([languageValue](Parameters &parameters) { parameters.setLanguage(static_cast<Language>(languageValue)); });
}

void GeneralSettingsController::refreshUpdates() const {
    _updateStatusService.refresh();
}

void GeneralSettingsController::openUrl(const QUrl &url) {
    if (QDesktopServices::openUrl(url)) {
        if (_error == Error::OpenUrl) {
            _error = Error::None;
            _failedUrl = QUrl();
            emit changed();
        }
        return;
    }

    qCWarning(lcGeneralSettings) << "Cannot open settings URL" << url;
    _error = Error::OpenUrl;
    _failedUrl = url;
    emit changed();
}

// Manual fallback when the AppImage cannot be replaced in place (not an AppImage, read-only folder, download error).
void GeneralSettingsController::openDownload() {
    openUrl(AppConstants::Settings::downloadUri());
}

void GeneralSettingsController::installUpdate() const {
    _updateStatusService.installUpdate();
}

// Loaded when the release information is shown, for the announced version in the application language.
void GeneralSettingsController::loadReleaseNotes() {
    const auto &version = _updateStatusService.version();
    auto languageCode = CommonUtility::languageCode(_translationService.language()).left(2);
    if (languageCode.isEmpty()) {
        languageCode = u"en"_s;
    }
    _releaseNotes.load(version ? QString::fromStdString(version->tag) : QString(), languageCode);
}

void GeneralSettingsController::openTrashHelp() {
    openUrl(AppConstants::Settings::trashHelpUri());
}

void GeneralSettingsController::openSupport() {
    openUrl(AppConstants::Support::helpUri());
}

void GeneralSettingsController::openFeedback() {
    openUrl(QUrl{Theme::feedbackUrl(_translationService.language())});
}

void GeneralSettingsController::openLicense() {
    openUrl(AppConstants::Settings::licenseUri());
}

void GeneralSettingsController::openSources() {
    openUrl(AppConstants::Settings::sourcesUri());
}

} // namespace KDC
