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

// A data class holding the switch widget and the line edit field
// which let users confirm the synchronization of a folder whose size
// is above a user-defined threshold (amount).
struct LargeFolderConfirmation : public QObject {  // Derived from QObject because retranslateUi calls tr()
        Q_OBJECT
    public:
        explicit LargeFolderConfirmation(QBoxLayout *folderConfirmationBox);
        void retranslateUi();
        // If this switch is on, a confirmation is asked for synchromizing large folders.
        const CustomSwitch *customSwitch() const { return _switch; };
        // A user-defined size expressed in MBs above which the confirmation is actually required.
        const QLineEdit *amountLineEdit() const { return _amountLineEdit; };
        void setAmountLineEditEnabled(bool enabled);

    private:
        QLabel *_label{nullptr};
        QLabel *_amountLabel{nullptr};
        QLineEdit *_amountLineEdit{nullptr};
        CustomSwitch *_switch{nullptr};
};

// A struct holding together the up-to-date status of the application, an update button, the application version
// with an hyperlink to the release notes.
struct VersionWidget {
        explicit VersionWidget(QBoxLayout *parentBox, const QString &versionNumberLinkText);
        void updateStatus(QString status, bool updateAvailable, const QString &releaseNoteLinkText);
        void setVersionLabelText(const QString &text);

        const QPushButton *updateButton() const { return _updateButton; };
        void setUpdateButtonText(const QString &text);
        const QLabel *updateStatusLabel() const { return _updateStatusLabel; };
        const QLabel *showReleaseNoteLabel() const { return _showReleaseNoteLabel; };
        const QLabel *versionNumberLabel() const { return _versionNumberLabel; };

    private:
        QLabel *_versionLabel{nullptr};
        QLabel *_updateStatusLabel{nullptr};
        QLabel *_showReleaseNoteLabel{nullptr};
        QLabel *_versionNumberLabel{nullptr};
        QPushButton *_updateButton{nullptr};
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

        std::unique_ptr<LargeFolderConfirmation> _largeFolderConfirmation;
        std::unique_ptr<VersionWidget> _versionWidget;
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
        void onLiteSyncWidgetClicked();
        void onLinkActivated(const QString &link);
        void onUpdateInfo();
        void onStartInstaller();

        void retranslateUi();
};

}  // namespace KDC
