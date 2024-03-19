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

#include "customcombobox.h"
#include "parameterswidget.h"
#include "libcommon/info/parametersinfo.h"

#include <QColor>
#include <QLabel>
#include <QLineEdit>
#include <QLineEdit>
#include <QPushButton>

namespace KDC {

class ActionWidget;
class ClientGui;

class PreferencesWidget : public ParametersWidget {
        Q_OBJECT

    public:
        explicit PreferencesWidget(std::shared_ptr<ClientGui> gui, QWidget *parent = nullptr);

        void showErrorBanner(bool unresolvedErrors);

    signals:
        void setStyle(bool darkTheme);
        void undecidedListsCleared();
        void displayErrors();
        void restartSync(int syncDbId);

    private:
        std::shared_ptr<ClientGui> _gui;

        QLineEdit *_folderConfirmationAmountLineEdit;
        QPushButton *_updateButton;
        CustomComboBox *_languageSelectorComboBox;
        QLabel *_generalLabel;
        QLabel *_folderConfirmationLabel;
        QLabel *_folderConfirmationAmountLabel;
        QLabel *_darkThemeLabel;
        QLabel *_monochromeLabel;
        QLabel *_launchAtStartupLabel;
        QLabel *_moveToTrashLabel;
        QLabel *_languageSelectorLabel;
        QLabel *_shortcutsLabel;
        QLabel *_advancedLabel;
        QLabel *_debuggingLabel;
        QLabel *_debuggingFolderLabel;
        QLabel *_filesToExcludeLabel;
        QLabel *_proxyServerLabel;
        QLabel *_liteSyncLabel;
        QLabel *_versionLabel;
        QLabel *_updateStatusLabel;
        QLabel *_showReleaseNoteLabel;
        QLabel *_versionNumberLabel;
        ActionWidget *_displayErrorsWidget;

        void showEvent(QShowEvent *event) override;

        void clearUndecidedLists();
        void updateStatus(QString status, bool updateAvailable);

    private slots:
        void onFolderConfirmationSwitchClicked(bool checked = false);
        void onFolderConfirmationAmountTextEdited(const QString &text);
        void onDarkThemeSwitchClicked(bool checked = false);
        void onMonochromeSwitchClicked(bool checked = false);
        void onLaunchAtStartupSwitchClicked(bool checked = false);
        void onLanguageChange();
        void onMoveToTrashSwitchClicked(bool checked = false);
#ifdef Q_OS_WIN
        void onShortcutsSwitchClicked(bool checked = false);
#endif
        void onDebuggingWidgetClicked();
        void onFilesToExcludeWidgetClicked();
        void onProxyServerWidgetClicked();
        void onResourcesManagerWidgetClicked();
        void onLiteSyncWidgetClicked();
        void onLinkActivated(const QString &link);
        void onUpdateInfo();
        void onStartInstaller();

        void retranslateUi();
};

}  // namespace KDC
