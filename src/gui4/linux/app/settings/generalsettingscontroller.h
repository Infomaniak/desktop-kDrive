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

#pragma once

#include "app/services/parametersservice.h"

#include <QObject>
#include <QUrl>

namespace KDC {

class ParametersStore;
class TranslationService;
class UpdateStatusService;

/** Presentation and immediate-save actions for General settings; the server remains authoritative. */
class GeneralSettingsController final : public QObject {
        Q_OBJECT
        Q_PROPERTY(bool ready READ ready NOTIFY changed)
        Q_PROPERTY(bool saving READ saving NOTIFY changed)
        Q_PROPERTY(bool autoStart READ autoStart NOTIFY changed)
        Q_PROPERTY(bool notificationsEnabled READ notificationsEnabled NOTIFY changed)
        Q_PROPERTY(bool moveToTrash READ moveToTrash NOTIFY changed)
        Q_PROPERTY(int32_t language READ language NOTIFY changed)
        Q_PROPERTY(QVariantList languages READ languages NOTIFY changed)
        Q_PROPERTY(QString errorText READ errorText NOTIFY changed)
        Q_PROPERTY(QString updateText READ updateText NOTIFY changed)
        Q_PROPERTY(bool updateAvailable READ updateAvailable NOTIFY changed)
        Q_PROPERTY(QString releaseVersion READ releaseVersion NOTIFY changed)
        Q_PROPERTY(QString releaseDetails READ releaseDetails NOTIFY changed)
        Q_PROPERTY(QString installedDetails READ installedDetails NOTIFY changed)

    public:
        GeneralSettingsController(ParametersStore &parametersStore, ParametersService &parametersService,
                                  TranslationService &translationService, UpdateStatusService &updateStatusService,
                                  QObject *parent = nullptr);

        [[nodiscard]] bool ready() const;

        [[nodiscard]] bool saving() const { return _saving; }

        [[nodiscard]] bool autoStart() const;
        [[nodiscard]] bool notificationsEnabled() const;
        [[nodiscard]] bool moveToTrash() const;
        [[nodiscard]] int32_t language() const;
        [[nodiscard]] static QVariantList languages();

        [[nodiscard]] QString errorText() const;

        [[nodiscard]] QString updateText() const;
        [[nodiscard]] bool updateAvailable() const;
        [[nodiscard]] QString releaseVersion() const;
        [[nodiscard]] QString releaseDetails() const;
        [[nodiscard]] static QString installedDetails();

        Q_INVOKABLE void setAutoStart(bool enabled);
        Q_INVOKABLE void setNotificationsEnabled(bool enabled);
        Q_INVOKABLE void setMoveToTrash(bool enabled);
        Q_INVOKABLE void setLanguage(int32_t languageValue);

        Q_INVOKABLE void openDownload();
        Q_INVOKABLE void openTrashHelp();
        Q_INVOKABLE void openSupport();
        Q_INVOKABLE void openFeedback();
        Q_INVOKABLE void openLicense();
        Q_INVOKABLE void openSources();

        Q_INVOKABLE void requestOpen() { emit openRequested(); }

        void refreshUpdates() const;

    signals:
        void changed();
        void openRequested();

    private:
        void save(const ParametersService::ParametersMutation &mutation);
        void openUrl(const QUrl &url);

        ParametersStore &_parametersStore;
        ParametersService &_parametersService;
        TranslationService &_translationService;
        UpdateStatusService &_updateStatusService;

        bool _saving{false};

        enum class Error : uint8_t {
            None,
            Save,
            OpenUrl
        };

        Error _error{Error::None};
        QUrl _failedUrl;
};

} // namespace KDC
