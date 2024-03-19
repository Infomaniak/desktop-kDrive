/*
Infomaniak Drive
Copyright (C) 2021 omar.khial@infomaniak.com

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include "driveinfoclient.h"
#include "../guirequests.h"

namespace KDC {

DriveInfoClient::DriveInfoClient()
    : DriveInfo(),
      _status(SyncStatusUndefined),
      _unresolvedConflicts(false),
      _totalSize(0),
      _used(0),
      _unresolvedErrorsCount(0),
      _autoresolvedErrorsCount(0),
      _isBeingDeleted(false),
      _stackedWidgetIndex(SynthesisStackedWidgetSynchronized),
      _synchronizedListWidget(nullptr),
      _synchronizedItemList(QVector<SynchronizedItem>()),
      _synchronizedListStackPosition(0),
      _favoritesListStackPosition(0),
      _activityListStackPosition(0),
      _errorTabWidgetStackPosition(0),
      _lastErrorTimestamp(0) {}

DriveInfoClient::DriveInfoClient(const DriveInfo &driveInfo)
    : DriveInfo(driveInfo),
      _status(SyncStatusUndefined),
      _unresolvedConflicts(false),
      _totalSize(0),
      _used(0),
      _unresolvedErrorsCount(0),
      _autoresolvedErrorsCount(0),
      _stackedWidgetIndex(SynthesisStackedWidgetSynchronized),
      _synchronizedListWidget(nullptr),
      _synchronizedItemList(QVector<SynchronizedItem>()),
      _synchronizedListStackPosition(0),
      _favoritesListStackPosition(0),
      _activityListStackPosition(0),
      _errorTabWidgetStackPosition(0),
      _lastErrorTimestamp(0) {}

void DriveInfoClient::updateStatus(std::map<int, SyncInfoClient> &syncInfoMap) {
    _status = SyncStatusUndefined;
    _unresolvedConflicts = false;

    std::size_t cnt = syncInfoMap.size();

    if (cnt == 1) {
        SyncInfoClient &syncInfo = syncInfoMap.begin()->second;
        switch (syncInfo.status()) {
            case SyncStatusUndefined:
                _status = SyncStatusError;
                break;
            default:
                _status = syncInfo.status();
                break;
        }
        _unresolvedConflicts = syncInfo.unresolvedConflicts();
    } else {
        int errorsSeen = 0;
        int goodSeen = 0;
        int abortOrPausedSeen = 0;
        int runSeen = 0;

        for (auto &syncInfoMapElt : syncInfoMap) {
            SyncInfoClient &syncInfo = syncInfoMapElt.second;
            if (syncInfo.paused()) {
                abortOrPausedSeen++;
            } else {
                switch (syncInfo.status()) {
                    case SyncStatusUndefined:
                        break;
                    case SyncStatusStarting:
                    case SyncStatusRunning:
                        runSeen++;
                        break;
                    case SyncStatusIdle:
                        goodSeen++;
                        break;
                    case SyncStatusError:
                        errorsSeen++;
                        break;
                    case SyncStatusStopAsked:
                    case SyncStatusStoped:
                    case SyncStatusPauseAsked:
                    case SyncStatusPaused:
                        abortOrPausedSeen++;
                }
            }
            if (syncInfo.unresolvedConflicts()) {
                _unresolvedConflicts = true;
            }
        }

        if (errorsSeen > 0) {
            _status = SyncStatusError;
        } else if (runSeen > 0) {
            _status = SyncStatusRunning;
        } else if (abortOrPausedSeen > 0) {
            _status = SyncStatusPaused;
        } else if (goodSeen > 0) {
            _status = SyncStatusIdle;
        }
    }
}

QString DriveInfoClient::folderPath(std::shared_ptr<std::map<int, SyncInfoClient>> syncInfoMap, int syncDbId,
                                    const QString &filePath) const {
    QString fullFilePath = QString();

    const auto &syncInfoMapIt = syncInfoMap->find(syncDbId);
    if (syncInfoMapIt != syncInfoMap->end()) {
        fullFilePath = syncInfoMapIt->second.localPath() + filePath;
    }

    return fullFilePath;
}

}  // namespace KDC
