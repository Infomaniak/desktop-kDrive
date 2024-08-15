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

#include "syncinfoclient.h"
#include "../synchronizeditem.h"
#include "libcommon/info/driveinfo.h"

#include <map>

#include <QUrl>
#include <QListWidget>

namespace KDC {

class DriveInfoClient : public DriveInfo {
    public:
        enum class SynthesisStackedWidget { Synchronized = 0, Favorites, Activity, FirstAdded };

        enum class ParametersStackedWidget { General = 0, FirstAdded };

        DriveInfoClient();
        DriveInfoClient(const DriveInfo &driveInfo);

        inline SyncStatus status() const { return _status; }
        inline bool unresolvedConflicts() const { return _unresolvedConflicts; }
        inline int unresolvedErrorsCount() const { return _unresolvedErrorsCount; }
        inline void setUnresolvedErrorsCount(int count) { _unresolvedErrorsCount = count; }
        inline int autoresolvedErrorsCount() const { return _autoresolvedErrorsCount; }
        inline void setAutoresolvedErrorsCount(int count) { _autoresolvedErrorsCount = count; }

        inline qint64 totalSize() const { return _totalSize; }
        inline void setTotalSize(qint64 totalSize) { _totalSize = totalSize; }
        inline qint64 used() const { return _used; }
        inline void setUsed(qint64 used) { _used = used; }

        inline SynthesisStackedWidget stackedWidget() const { return _stackedWidgetIndex; }
        inline void setStackedWidget(SynthesisStackedWidget newStackedWidget) { _stackedWidgetIndex = newStackedWidget; }
        inline QListWidget *synchronizedListWidget() { return _synchronizedListWidget; }
        inline void setSynchronizedListWidget(QListWidget *newSynchronizedListWidget) {
            _synchronizedListWidget = newSynchronizedListWidget;
        }
        inline QVector<SynchronizedItem> &synchronizedItemList() { return _synchronizedItemList; }
        inline void setSynchronizedItemList(const QVector<SynchronizedItem> &newSynchronizedItemList) {
            _synchronizedItemList = newSynchronizedItemList;
        }
        inline int synchronizedListStackPosition() const { return _synchronizedListStackPosition; }
        inline void setSynchronizedListStackPosition(int newSynchronizedListStackPosition) {
            _synchronizedListStackPosition = newSynchronizedListStackPosition;
        }
        inline int favoritesListStackPosition() const { return _favoritesListStackPosition; }
        inline void setFavoritesListStackPosition(int newFavoritesListStackPosition) {
            _favoritesListStackPosition = newFavoritesListStackPosition;
        }
        inline int activityListStackPosition() const { return _activityListStackPosition; }
        inline void setActivityListStackPosition(int newActivityListStackPosition) {
            _activityListStackPosition = newActivityListStackPosition;
        }

        inline int errorTabWidgetStackPosition() const { return _errorTabWidgetStackPosition; }
        inline void setErrorTabWidgetStackPosition(int newErrorTabWidgetStackPosition) {
            _errorTabWidgetStackPosition = newErrorTabWidgetStackPosition;
        }
        inline qint64 lastErrorTimestamp() const { return _lastErrorTimestamp; }
        inline void setLastErrorTimestamp(int newLastErrorTimestamp) { _lastErrorTimestamp = newLastErrorTimestamp; }
        inline bool isBeingDeleted() const noexcept { return _isBeingDeleted; };
        inline void setIsBeingDeleted(bool isDeletionOnGoing) noexcept { _isBeingDeleted = isDeletionOnGoing; }

        void updateStatus(std::map<int, SyncInfoClient> &syncInfoMap);
        QString folderPath(std::shared_ptr<std::map<int, SyncInfoClient>> syncInfoMap, int syncDbId,
                           const QString &filePath) const;

    private:
        SyncStatus _status{SyncStatus::Undefined};
        bool _unresolvedConflicts{false};

        qint64 _totalSize{0};
        qint64 _used{0};
        int _unresolvedErrorsCount{0};
        int _autoresolvedErrorsCount{0};
        bool _isBeingDeleted{false};

        // Synthesispopover attributes
        SynthesisStackedWidget _stackedWidgetIndex{SynthesisStackedWidget::Synchronized};
        QListWidget *_synchronizedListWidget{nullptr};
        QVector<SynchronizedItem> _synchronizedItemList;
        int _synchronizedListStackPosition{0};
        int _favoritesListStackPosition{0};
        int _activityListStackPosition{0};

        // Parametersdialog attributes
        int _errorTabWidgetStackPosition{0};
        qint64 _lastErrorTimestamp{0};
};

}  // namespace KDC
