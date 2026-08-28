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

#include "libcommon/utility/cstypes.h"
#include "libcommon/utility/types.h"

#include <QObject>

#include <cstdint>
#include <deque>

namespace KDC {

class AppCache;
class CommService;
class ParametersService;

/**
 * Process-long presentation controller for server-reported mass deletion warnings.
 *
 * It owns the feature-specific FIFO queue, acknowledgement lifecycle, browser action, and soft-warning preference
 * mutation. QML only renders the current entry and invokes semantic actions.
 */
class ManyDeletesController final : public QObject {
        Q_OBJECT
        Q_PROPERTY(Severity severity READ severity NOTIFY presentationChanged)
        Q_PROPERTY(bool visible READ visible NOTIFY presentationChanged)
        Q_PROPERTY(quint64 itemCount READ itemCount NOTIFY presentationChanged)
        Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
        Q_PROPERTY(bool submissionFailed READ submissionFailed NOTIFY submissionFailedChanged)
        Q_PROPERTY(bool canOpenTrash READ canOpenTrash NOTIFY canOpenTrashChanged)

    public:
        enum class Severity : uint8_t {
            None = 0,
            Soft,
            Hard,
        };
        Q_ENUM(Severity)

        explicit ManyDeletesController(CommService &commService, AppCache &appCache, ParametersService &parametersService,
                                       QObject *parent = nullptr);

        [[nodiscard]] Severity severity() const;
        [[nodiscard]] bool visible() const { return !_notifications.empty(); }
        [[nodiscard]] quint64 itemCount() const;
        [[nodiscard]] bool busy() const { return _busy; }
        [[nodiscard]] bool submissionFailed() const { return _submissionFailed; }
        [[nodiscard]] bool canOpenTrash() const;

        Q_INVOKABLE void dismissSoft(bool disableFutureWarnings);
        Q_INVOKABLE void openTrash(bool disableFutureWarnings);
        Q_INVOKABLE void restoreFiles();
        Q_INVOKABLE void deleteOnline();

    signals:
        void presentationChanged();
        void busyChanged();
        void submissionFailedChanged();
        void canOpenTrashChanged();
        void presentationRequested();

    private:
        struct Notification {
                SyncDbId syncDbId{0};
                Severity severity{Severity::None};
                Count itemCount{0};
        };

        void enqueue(SyncDbId syncDbId, TooManyDeletesNotificationType notificationType, Count itemCount);
        void acknowledge(TooManyDeletesUserChoice userChoice);
        void finishCurrent();
        void disableFutureWarnings() const;
        void setBusy(bool busy);
        void setSubmissionFailed(bool failed);
        void notifyPresentationChanged();
        [[nodiscard]] const Notification *currentNotification() const;
        [[nodiscard]] static Severity severityFromNotificationType(TooManyDeletesNotificationType notificationType);

        CommService &_commService;
        AppCache &_appCache;
        ParametersService &_parametersService;
        std::deque<Notification> _notifications;
        bool _busy{false};
        bool _submissionFailed{false};
};

} // namespace KDC
