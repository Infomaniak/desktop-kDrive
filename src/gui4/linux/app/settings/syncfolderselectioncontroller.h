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

#include "app/syncconfiguration/remotefolderprovider.h"
#include "app/syncconfiguration/remotefoldertreemodel.h"

#include <QObject>

#include <cstdint>
#include <vector>

namespace KDC {

class AppCache;
class CommService;

/**
 * Settings "Manage synchronization" page state for one existing synchronization.
 *
 * Role: load the confirmed blacklist of the synchronization into a RemoteFolderTreeModel, let the user edit a draft, and
 * save the complete list. The draft is never published: only a successful save changes the confirmed blacklist.
 */
class SyncFolderSelectionController final : public QObject {
        Q_OBJECT
        Q_PROPERTY(RemoteFolderTreeModel *folderTreeModel READ folderTreeModel CONSTANT)
        Q_PROPERTY(bool loading READ loading NOTIFY stateChanged)
        Q_PROPERTY(bool loadFailed READ loadFailed NOTIFY stateChanged)
        Q_PROPERTY(bool saving READ saving NOTIFY stateChanged)
        Q_PROPERTY(bool saveFailed READ saveFailed NOTIFY stateChanged)
        Q_PROPERTY(bool canSave READ canSave NOTIFY stateChanged)

    public:
        SyncFolderSelectionController(AppCache &appCache, CommService &commService, QObject *parent = nullptr);

        [[nodiscard]] RemoteFolderTreeModel *folderTreeModel() { return &_folderTreeModel; }
        // Covers the blacklist request only; the tree reports its own loading state.
        [[nodiscard]] bool loading() const { return _state == State::LoadingBlackList; }
        [[nodiscard]] bool loadFailed() const { return _state == State::LoadFailed; }
        [[nodiscard]] bool saving() const { return _state == State::Saving; }
        [[nodiscard]] bool saveFailed() const { return _saveFailed; }
        // True once the draft differs from the confirmed blacklist and the tree is fully loaded.
        [[nodiscard]] bool canSave() const;

        Q_INVOKABLE void open(qint64 syncDbId);
        /// Releases the target only when it is still the given synchronization, so a closing page cannot reset its
        /// successor.
        Q_INVOKABLE void close(qint64 syncDbId);
        Q_INVOKABLE void retry();
        Q_INVOKABLE void save();
        void retranslate();

    signals:
        void stateChanged();
        // The selection was saved; the page returns to the previous one.
        void saved();

    private:
        enum class State : uint8_t {
            Idle,
            LoadingBlackList,
            LoadFailed,
            Editing,
            Saving,
        };

        void loadBlackList();
        void setState(State state);
        void resetTarget();

        AppCache &_appCache;
        CommService &_commService;
        CommRemoteFolderProvider _folderProvider;
        RemoteFolderTreeModel _folderTreeModel;
        SyncDbId _syncDbId{0};
        State _state{State::Idle};
        // Sorted like RemoteFolderTreeModel::blackList(), so the draft compares directly with it.
        std::vector<NodeId> _confirmedBlackList;
        bool _saveFailed{false};
};

} // namespace KDC
