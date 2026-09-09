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
#include "app/cache/parametersstore.h"
#include "app/services/translationservice.h"
#include "app/services/updatestatusservice.h"
#include "libcommon/theme/theme.h"

#include <version.h>

#include <QDate>
#include <QDesktopServices>
#include <QLoggingCategory>
#include <QPointer>

namespace KDC {

using namespace Qt::StringLiterals;

namespace {
Q_LOGGING_CATEGORY(lcGeneralSettings, "gui.v4.settings", QtInfoMsg)

QString installedVersion() {
    return QString::fromLatin1(KDRIVE_VERSION_STRING);
}

QString versionDetails(const QString &version, const uint64_t build) {
    return qtTrId("aboutAppVersionCopyrightMac").arg(version).arg(build).arg(QDate::currentDate().year());
}
} // namespace

GeneralSettingsController::GeneralSettingsController(ParametersStore &parametersStore, ParametersService &parametersService,
                                                     TranslationService &translationService,
                                                     UpdateStatusService &updateStatusService, QObject *parent) :
    QObject(parent),
    _parametersStore(parametersStore),
    _parametersService(parametersService),
    _translationService(translationService),
    _updateStatusService(updateStatusService) {
    (void) connect(&parametersStore, &ParametersStore::parametersInfoChanged, this, &GeneralSettingsController::changed);
    (void) connect(&translationService, &TranslationService::languageChanged, this, &GeneralSettingsController::changed);
    (void) connect(&updateStatusService, &UpdateStatusService::changed, this, &GeneralSettingsController::changed);
}

bool GeneralSettingsController::ready() const {
    return _parametersStore.parametersInfo().has_value();
}

bool GeneralSettingsController::autoStart() const {
    const auto parametersInfo = _parametersStore.parametersInfo();
    return parametersInfo && parametersInfo->autoStart();
}

bool GeneralSettingsController::notificationsEnabled() const {
    const auto parametersInfo = _parametersStore.parametersInfo();
    return parametersInfo && parametersInfo->notificationsDisabled() == NotificationsDisabled::Never;
}

bool GeneralSettingsController::moveToTrash() const {
    const auto parametersInfo = _parametersStore.parametersInfo();
    return parametersInfo && parametersInfo->moveToTrash();
}

int32_t GeneralSettingsController::language() const {
    const auto parametersInfo = _parametersStore.parametersInfo();
    return static_cast<int32_t>(parametersInfo ? parametersInfo->language() : Language::Default);
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

QString GeneralSettingsController::releaseVersion() const {
    return _updateStatusService.version() ? QString::fromStdString(_updateStatusService.version()->tag) : installedVersion();
}

QString GeneralSettingsController::releaseDetails() const {
    return _updateStatusService.version() ? versionDetails(releaseVersion(), _updateStatusService.version()->buildVersion)
                                          : installedDetails();
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

void GeneralSettingsController::setAutoStart(bool enabled) {
    if (enabled == autoStart()) {
        return;
    }

    save([enabled](ParametersInfo &parametersInfo) { parametersInfo.setAutoStart(enabled); });
}

void GeneralSettingsController::setNotificationsEnabled(bool enabled) {
    if (enabled == notificationsEnabled()) {
        return;
    }

    save([enabled](ParametersInfo &parametersInfo) {
        parametersInfo.setNotificationsDisabled(enabled ? NotificationsDisabled::Never : NotificationsDisabled::Always);
    });
}

void GeneralSettingsController::setMoveToTrash(bool enabled) {
    if (enabled == moveToTrash()) {
        return;
    }

    save([enabled](ParametersInfo &parametersInfo) { parametersInfo.setMoveToTrash(enabled); });
}

void GeneralSettingsController::setLanguage(int32_t languageValue) {
    if (languageValue < static_cast<int32_t>(Language::Default) || languageValue >= static_cast<int32_t>(Language::EnumEnd)) {
        return;
    }

    if (languageValue == language()) {
        return;
    }

    save([languageValue](ParametersInfo &parametersInfo) { parametersInfo.setLanguage(static_cast<Language>(languageValue)); });
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

void GeneralSettingsController::openDownload() {
    openUrl(AppConstants::Settings::downloadUri());
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
