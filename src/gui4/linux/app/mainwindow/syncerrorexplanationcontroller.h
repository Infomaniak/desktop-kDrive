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

#include "app/errors/syncerrorkind.h"
#include "app/fileiconresolver.h"
#include "libcommon/utility/types.h"

#include <QObject>
#include <QString>

#include <optional>

namespace KDC {

class MainSelectionStore;
class SyncErrorActionHandler;
class SyncErrorsProjection;
struct ClassifiedSyncError;

/**
 * QML-facing state of the explanation dialog of the errors page ("Understand the problem").
 *
 * It shows one error of the selected synchronization at a time and closes itself when that error disappears or the
 * selection changes. Its buttons are revalidated against the current errors, then run by `SyncErrorActionHandler`.
 */
class SyncErrorExplanationController final : public QObject {
        Q_OBJECT
        Q_PROPERTY(bool isOpen READ isOpen NOTIFY changed)
        Q_PROPERTY(KDC::SyncErrors::SyncErrorExplanation kind READ kind NOTIFY changed)
        Q_PROPERTY(QString nodeLabelId READ nodeLabelId NOTIFY changed)
        Q_PROPERTY(QString copyablePath READ copyablePath NOTIFY changed)
        Q_PROPERTY(QString itemName READ itemName NOTIFY changed)
        Q_PROPERTY(QString itemPath READ itemPath NOTIFY changed)
        Q_PROPERTY(QString itemIconName READ itemIconName NOTIFY changed)
        Q_PROPERTY(bool isDirectory READ isDirectory NOTIFY changed)
        Q_PROPERTY(qint64 watchLimit READ watchLimit NOTIFY changed)
        Q_PROPERTY(QString watchLimitCommand READ watchLimitCommand NOTIFY changed)

    public:
        SyncErrorExplanationController(const SyncErrorsProjection &projection, const MainSelectionStore &selectionStore,
                                       SyncErrorActionHandler &actionHandler, QObject *parent = nullptr);

        [[nodiscard]] bool isOpen() const { return _errorDbId.has_value(); }
        [[nodiscard]] SyncErrorExplanation kind() const { return _kind; }
        [[nodiscard]] QString nodeLabelId() const { return _nodeLabelId; }
        [[nodiscard]] QString copyablePath() const { return _copyablePath; }
        [[nodiscard]] QString itemName() const { return _itemName; }
        [[nodiscard]] QString itemPath() const { return _itemPath; }
        [[nodiscard]] QString itemIconName() const { return _itemIconName; }
        [[nodiscard]] bool isDirectory() const { return _isDirectory; }
        /** Current inotify watches limit of the system, or 0 when it cannot be read. */
        [[nodiscard]] qint64 watchLimit() const { return _watchLimit; }
        /** Shell command that raises the inotify watches limit for good, to be run by the user. */
        [[nodiscard]] QString watchLimitCommand() const { return _watchLimitCommand; }

        /** Opens the dialog of `errorDbId`, an error of the selected synchronization; ignored when it has none. */
        void open(ErrorDbId errorDbId);
        Q_INVOKABLE void close();
        /** Runs the main button of the dialog, then closes it. */
        Q_INVOKABLE void triggerMainAction();
        /** Opens Settings where a new synchronization is created (the "new synchronization" link), then closes. */
        Q_INVOKABLE void createNewSync();
        /** Copies the absolute form of `copyablePath` to the clipboard. */
        Q_INVOKABLE void copyPath() const;
        /** Copies `watchLimitCommand` to the clipboard. */
        Q_INVOKABLE void copyWatchLimitCommand() const;

    signals:
        void changed();

    private:
        [[nodiscard]] const ClassifiedSyncError *currentError() const;
        void runAndClose(SyncErrorAction action);
        [[nodiscard]] QString absolutePath(const ClassifiedSyncError &classified) const;

        const SyncErrorsProjection &_projection;
        const MainSelectionStore &_selectionStore;
        SyncErrorActionHandler &_actionHandler;
        std::optional<ErrorDbId> _errorDbId;
        SyncErrorExplanation _kind{SyncErrorExplanation::None};
        QString _nodeLabelId;
        QString _copyablePath;
        // Item chip of the local access dialog.
        QString _itemName;
        QString _itemPath;
        QString _itemIconName;
        bool _isDirectory{false};
        // Inotify watches dialog.
        qint64 _watchLimit{0};
        QString _watchLimitCommand;
        FileIconResolver _fileIconResolver;
};

} // namespace KDC
