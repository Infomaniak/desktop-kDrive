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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include "app/fileiconresolver.h"
#include "libcommon/utility/types.h"

#include <QObject>
#include <QString>

#include <cstdint>
#include <optional>
#include <vector>

namespace KDC {

class MainSelectionStore;
class NodeConflictInfo;
class SyncErrorActionHandler;
class SyncErrorService;
class SyncErrorsProjection;
struct ClassifiedSyncError;

/**
 * QML-facing state of the version selection dialog of the conflicts ("Choose the version to keep").
 *
 * It walks through conflicts of the selected synchronization one at a time, loads both versions of each, and sends the
 * choices in one request after the last one. Closing the dialog midway sends the choices already validated. It closes
 * itself when the current conflict disappears or the selection changes.
 */
class ConflictVersionController final : public QObject {
        Q_OBJECT
        Q_PROPERTY(bool isOpen READ isOpen NOTIFY changed)
        Q_PROPERTY(int32_t index READ index NOTIFY changed)
        Q_PROPERTY(int32_t count READ count NOTIFY changed)
        Q_PROPERTY(bool isLast READ isLast NOTIFY changed)
        Q_PROPERTY(QString itemName READ itemName NOTIFY changed)
        Q_PROPERTY(QString itemIconName READ itemIconName NOTIFY changed)
        Q_PROPERTY(bool loading READ loading NOTIFY changed)
        Q_PROPERTY(bool failed READ failed NOTIFY changed)
        Q_PROPERTY(bool resolutionPending READ resolutionPending NOTIFY changed)
        Q_PROPERTY(KDC::ConflictVersionController::Version selection READ selection NOTIFY changed)
        Q_PROPERTY(KDC::ConflictVersionController::Version mostRecent READ mostRecent NOTIFY changed)
        Q_PROPERTY(QString onlineAuthorName READ onlineAuthorName NOTIFY changed)
        Q_PROPERTY(QString onlineSize READ onlineSize NOTIFY changed)
        Q_PROPERTY(QString onlineDate READ onlineDate NOTIFY changed)
        Q_PROPERTY(QString localAuthorName READ localAuthorName NOTIFY changed)
        Q_PROPERTY(QString localSize READ localSize NOTIFY changed)
        Q_PROPERTY(QString localDate READ localDate NOTIFY changed)

    public:
        enum class Version : uint8_t {
            None,
            Online,
            Local,
        };
        Q_ENUM(Version)

        ConflictVersionController(const SyncErrorsProjection &projection, const MainSelectionStore &selectionStore,
                                  SyncErrorService &syncErrorService, SyncErrorActionHandler &actionHandler,
                                  QObject *parent = nullptr);

        [[nodiscard]] bool isOpen() const { return !_errorDbIds.empty(); }
        [[nodiscard]] int32_t index() const { return static_cast<int32_t>(_index); }
        [[nodiscard]] int32_t count() const { return static_cast<int32_t>(_errorDbIds.size()); }
        [[nodiscard]] bool isLast() const { return _index + 1 >= _errorDbIds.size(); }
        [[nodiscard]] QString itemName() const { return _itemName; }
        [[nodiscard]] QString itemIconName() const { return _itemIconName; }
        /** Whether a version of the current conflict is still loading. */
        [[nodiscard]] bool loading() const { return _pendingLoads > 0; }
        /** Whether a version could not be loaded, or the choices could not be sent. */
        [[nodiscard]] bool failed() const { return _loadFailed || _resolutionFailed; }
        [[nodiscard]] bool resolutionPending() const;
        [[nodiscard]] Version selection() const { return _selection; }
        /** The strictly more recent version, or `None` when the dates are equal or unknown. */
        [[nodiscard]] Version mostRecent() const;
        [[nodiscard]] QString onlineAuthorName() const { return _online.authorName; }
        [[nodiscard]] QString onlineSize() const { return _online.size; }
        [[nodiscard]] QString onlineDate() const { return _online.date; }
        [[nodiscard]] QString localAuthorName() const { return _local.authorName; }
        [[nodiscard]] QString localSize() const { return _local.size; }
        [[nodiscard]] QString localDate() const { return _local.date; }

        /** Opens the dialog on `errorDbIds`, conflicts of the selected synchronization; ignored when none is found. */
        void open(std::vector<ErrorDbId> errorDbIds);
        /** Closes the dialog and sends the choices already validated. */
        Q_INVOKABLE void close();
        Q_INVOKABLE void select(KDC::ConflictVersionController::Version version);
        /** Keeps the selected version, then shows the next conflict, or sends every choice after the last one. */
        Q_INVOKABLE void validate();
        /** Opens a version of the current conflict, to compare them. */
        Q_INVOKABLE void openVersion(KDC::ConflictVersionController::Version version) const;

    signals:
        void changed();

    private:
        struct VersionInfo {
                QString authorName;
                QString size;
                QString date;
                std::optional<SyncTime> modificationDate;
        };

        [[nodiscard]] const ClassifiedSyncError *currentError() const;
        void showCurrent();
        void load(const ClassifiedSyncError &classified, Version version);
        void onLoaded(Version version, bool ok, const NodeConflictInfo &info);
        void sendChoices();
        void reset();

        const SyncErrorsProjection &_projection;
        const MainSelectionStore &_selectionStore;
        SyncErrorService &_syncErrorService;
        SyncErrorActionHandler &_actionHandler;
        qint64 _syncDbId{0};
        std::vector<ErrorDbId> _errorDbIds;
        // The validated version of each conflict, `None` until then.
        std::vector<Version> _choices;
        std::size_t _index{0};
        // Responses of a previous conflict, dialog or request are ignored.
        uint64_t _generation{0};
        bool _awaitingResolution{false};
        QString _itemName;
        QString _itemIconName;
        uint8_t _pendingLoads{0};
        bool _loadFailed{false};
        bool _resolutionFailed{false};
        Version _selection{Version::None};
        VersionInfo _online;
        VersionInfo _local;
        FileIconResolver _fileIconResolver;
};

} // namespace KDC
