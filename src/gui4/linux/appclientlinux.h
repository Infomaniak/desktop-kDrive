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

#include "communicationlayer/ipcclient.h"

#include <QGuiApplication>
#include <QLoggingCategory>

namespace KDC {

Q_DECLARE_LOGGING_CATEGORY(lcAppClientLinux)

class AppClientLinux : public QGuiApplication {
        Q_OBJECT

    public:
        explicit AppClientLinux(int &argc, char **argv);

    signals:
        void ipcConnected();
        void ipcDisconnected();
        void ipcMessageReceived(GuiJobType type, int32_t id, uint8_t num, Poco::DynamicStruct params);

    private:
        static void setupLogging();

        IpcClient _ipcClient{this};
};

} // namespace KDC