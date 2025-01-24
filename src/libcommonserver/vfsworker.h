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

#include "libcommon/utility/types.h"
#include "libcommon/utility/utility.h"
#include "vfs.h"
#include <deque>
#include <QObject>
#include <QList>
#include <QMutex>
#include <QThread>
#include <QWaitCondition>

#include <log4cplus/logger.h>
#include <log4cplus/loggingmacros.h>

namespace KDC {
class VfsWorker : public QObject {
        Q_OBJECT

    public:
        VfsWorker(Vfs *vfs, int type, int num, log4cplus::Logger logger);
        void start();

    private:
        Vfs *_vfs;
        int _type;
        int _num;
        log4cplus::Logger _logger;

        inline log4cplus::Logger logger() { return _logger; }
};
} // namespace KDC
