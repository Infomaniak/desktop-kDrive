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

#include "libcommon/comm.h"
#include "libcommon/utility/types.h"

#include <QObject>

namespace KDC {

/**
 * Shared event bus for high-level app services.
 *
 * UI layers can subscribe once to this object for cross-service events
 * such as generic request failures.
 */
class ServiceEventBus : public QObject {
        Q_OBJECT

    public:
        explicit ServiceEventBus(QObject *parent = nullptr);

        void notifyGenericError(const ExitInfo &exitInfo, RequestNum requestNum);

    signals:
        void genericErrorOccurred();
};

} // namespace KDC
