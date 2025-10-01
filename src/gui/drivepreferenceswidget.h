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

#pragma once

#include "customswitch.h"
#include "actionwidget.h"
#include "adddrivelistwidget.h"
#include "customtoolbutton.h"
#include "preferencesblocwidget.h"
#include "folderitemwidget.h"
#include "foldertreeitemwidget.h"
#include "widgetwithcustomtooltip.h"
#include "info/searchinfo.h"

#include <QBoxLayout>
#include <QColor>
#include <QFrame>
#include <QLabel>
#include <QMetaObject>
#include <QProgressBar>
#include <QString>
#include <QStringListModel>
#include <QMutex>

namespace KDC {

class CustomPushButton;
class ClientGui;

class DrivePreferencesWidget : public LargeWidgetWithCustomToolTip {
        Q_OBJECT

    public:
        explicit DrivePreferencesWidget(std::shared_ptr<ClientGui> gui, QWidget *parent = nullptr);

        void setDrive(int driveDbId, bool unresolvedErrors);
        void reset();
        void showErrorBanner(bool unresolvedErrors);
        void refreshStatus();

    signals:
        void displayErrors(int driveDbId);
        void errorAdded();
        void openFolder(const QString &filePath);
        void removeDrive(int driveDbId);
        void newBigFolderDiscovered(int syncDbId, const QString &path);
        void undecidedListsCleared();
        void pauseSync(int syncDbId);
        void resumeSync(int syncDbId);

    private:
        enum AddFolderStep {
            SelectLocalFolder = 0,
            SelectServerBaseFolder,
            SelectServerFolders,
            Confirm
        };

        std::shared_ptr<ClientGui> _gui;
        int _driveDbId{0};
        int _userDbId{0};

        QVBoxLayout *_mainVBox{nullptr};
        ActionWidget *_displayErrorsWidget{nullptr};
        ActionWidget *_displayBigFoldersWarningWidget{nullptr};
        QLabel *_userAvatarLabel{nullptr};
        QLabel *_userNameLabel{nullptr};
        QLabel *_userMailLabel{nullptr};
        CustomSwitch *_notificationsSwitch{nullptr};
        int _foldersBeginIndex{0};
        QLabel *_foldersLabel{nullptr};
        CustomPushButton *_addLocalFolderButton{nullptr};
        QLabel *_notificationsLabel{nullptr};
        QLabel *_notificationsTitleLabel{nullptr};
        QLabel *_notificationsDescriptionLabel{nullptr};
        QLabel *_connectedWithLabel{nullptr};
        CustomToolButton *_removeDriveButton{nullptr};
        bool _updatingFoldersBlocs{false};

        QLabel *_searchLabel{nullptr};
        QLineEdit *_searchBox{nullptr};
        QPushButton *_searchButton{nullptr};
        QLabel *_searchProgressLabel{nullptr};
        QListView *_searchResultView{nullptr};
        class SearchItemListModel : public QStringListModel {
            public:
                void setSearchItems(const QList<SearchInfo> &data) {
                    _data = data;
                    QStringList list;
                    for (const auto &item: _data) {
                        list << SyncName2QStr(item.name());
                    }
                    setStringList(list);
                };
                [[nodiscard]] NodeId id(int row) const {
                    if (row > _data.count() - 1) return "0";
                    return _data[row].id();
                }
                void clear() {
                    _data.clear();
                    setStringList({});
                }

            private:
                QList<SearchInfo> _data;
        };
        SearchItemListModel _searchResultModel;

        void showEvent(QShowEvent *event) override;

        bool existUndecidedSet();
        void updateUserInfo();
        void askEnableLiteSync(const std::function<void(bool enable)> &callback);
        void askDisableLiteSync(const std::function<void(bool enable, bool diskSpaceWarning)> &callback, int syncDbId);
        bool switchVfsOn(int syncDbId);
        bool switchVfsOff(int syncDbId, bool diskSpaceWarning);
        void resetFoldersBlocs();
        void updateFoldersBlocs();
        void refreshFoldersBlocs() const;
        static FolderTreeItemWidget *blocTreeItemWidget(PreferencesBlocWidget *folderBloc);
        static FolderItemWidget *blocItemWidget(PreferencesBlocWidget const *folderBloc);
        static QFrame *blocSeparatorFrame(PreferencesBlocWidget const *folderBloc);
        bool addSync(const QString &localFolderPath, bool liteSync, const QString &serverFolderPath,
                     const QString &serverFolderNodeId, const QSet<QString> &blackSet, const QSet<QString> &whiteSet);
        static bool updateSelectiveSyncList(const QHash<int, QHash<const QString, bool>> &mapUndefinedFolders);
        void updateGuardedFoldersBlocs();

        void initializeSearchBloc();

    private slots:
        void onErrorsWidgetClicked();
        void onBigFoldersWarningWidgetClicked();
        void onAddLocalFolder(bool checked = false);
        void onLiteSyncSwitchSyncChanged(int syncDbId, bool activate);
        void onNotificationsSwitchClicked(bool checked = false);
        void onErrorAdded();
        void onRemoveDrive(bool checked = false);
        void onUnsyncTriggered(int syncDbId);
        void onDisplayFolderDetail(int syncDbId, bool display);
        void onOpenFolder(const QString &filePath);
        void onSubfoldersLoaded(bool error, ExitCause exitCause, bool empty);
        void onNeedToSave(bool isFolderItemBlackListed) const;
        void onCancelUpdate(int syncDbId) const;
        void onValidateUpdate(int syncDbId);
        void onNewBigFolderDiscovered(int syncDbId, const QString &path);
        void onUndecidedListsCleared();
        void retranslateUi();
        void onVfsConversionCompleted(int syncDbId);
        void onDriveBeingRemoved();
        void onSearch();
        void onSearchItemDoubleClicked(const QModelIndex &index);
};

} // namespace KDC
