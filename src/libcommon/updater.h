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

#include <QObject>
#include <QString>

#include "libcommon/utility/types.h"

namespace KDC {

class Updater : public QObject {
        Q_OBJECT

    public:
        explicit Updater(QObject *parent = NULL);

        virtual QString version() const = 0;
        // virtual bool isKDCUpdater() = 0;
        // virtual bool isSparkleUpdater() = 0;
        virtual QString statusString() const = 0;
        virtual bool downloadCompleted() const = 0;
        virtual bool updateFound() const = 0;
        virtual void startInstaller() const = 0;
        virtual UpdateState updateState() const = 0;
};

} // namespace KDC
