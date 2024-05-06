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
#include "widgetwithcustomtooltip.h"

#include <QColor>

class QBoxLayout;
class QLabel;
class QLineEdit;
class QPushButton;

namespace KDC {

class ActionWidget;
class ClientGui;
class CustomSwitch;

struct FolderConfirmation : public QObject {
        Q_OBJECT
    public:
        explicit FolderConfirmation(QBoxLayout *folderConfirmationBox);
        void retranslateUi();
        CustomSwitch *customSwitch() { return _switch; };
        QLineEdit *amountLineEdit() { return _amountLineEdit; };

    private:
        QLabel *_label{nullptr};
        QLabel *_amountLabel{nullptr};
        QLineEdit *_amountLineEdit{nullptr};
        CustomSwitch *_switch{nullptr};
};

class PreferencesWidget : public LargeWidgetWithCustomToolTip {
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

        std::unique_ptr<FolderConfirmation> _folderConfirmation;
        QPushButton *_updateButton{nullptr};
        CustomComboBox *_languageSelectorComboBox{nullptr};
        QLabel *_generalLabel{nullptr};
        QLabel *_darkThemeLabel{nullptr};
        QLabel *_monochromeLabel{nullptr};
        QLabel *_launchAtStartupLabel{nullptr};
        QLabel *_moveToTrashLabel{nullptr};
        QLabel *_languageSelectorLabel{nullptr};
        QLabel *_shortcutsLabel{nullptr};
        QLabel *_advancedLabel{nullptr};
        QLabel *_debuggingLabel{nullptr};
        QLabel *_debuggingFolderLabel{nullptr};
        QLabel *_filesToExcludeLabel{nullptr};
        QLabel *_proxyServerLabel{nullptr};
        QLabel *_liteSyncLabel{nullptr};
        QLabel *_versionLabel{nullptr};
        QLabel *_updateStatusLabel{nullptr};
        QLabel *_showReleaseNoteLabel{nullptr};
        QLabel *_versionNumberLabel{nullptr};
        ActionWidget *_displayErrorsWidget{nullptr};

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
