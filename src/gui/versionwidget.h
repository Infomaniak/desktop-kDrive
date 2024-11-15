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

#pragma once

/**
 * A class holding together the up-to-date status of the application, an update button and the application version with an
 * hyperlink to the release notes.
 */

#include "utility/types.h"


#include <QWidget>


class QRadioButton;
class QPushButton;
class QLabel;
class QBoxLayout;

namespace KDC {

class PreferencesBlocWidget;

class VersionWidget final : public QWidget {
        Q_OBJECT

    public:
        explicit VersionWidget(QWidget *parent = nullptr);
        void refresh(UpdateState state = UpdateState::Unknown) const;

        void showAboutDialog();
        void showReleaseNotes() const;
        void showDownloadPage() const;

    signals:
        void showUpdateDialog(const VersionInfo &versionInfo);

    public slots:
        void onUpdateStateChanged(UpdateState state) const;

    private slots:
        void onChannelButtonClicked() const;
        void onLinkActivated(const QString &link);
        void onUpdateButtonClicked();
        void onJoinBetaButtonClicked();

    private:
        void initVersionInfoBloc(PreferencesBlocWidget *prefBloc);
        void initBetaBloc(PreferencesBlocWidget *prefBloc);
        void refreshChannelButtons(DistributionChannel channel) const;

        void saveDistributionChannel(DistributionChannel channel) const;

        QRadioButton *_prodButton{nullptr};
        QRadioButton *_betaButton{nullptr};
        QRadioButton *_internalButton{nullptr};

        QLabel *_versionLabel{nullptr};

        QLabel *_updateStatusLabel{nullptr};
        QLabel *_showReleaseNotesLabel{nullptr};
        QLabel *_versionNumberLabel{nullptr};
        QPushButton *_updateButton{nullptr};

        QLabel *_betaVersionLabel{nullptr};
        QLabel *_betaVersionDescription{nullptr};
        QPushButton *_joinBetaButton{nullptr};
};

} // namespace KDC
