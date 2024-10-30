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

#include "libcommon/utility/types.h"
#include "libcommonserver/log/log.h"
#include "vfs.h"

#include <QObject>

namespace KDC {

class PluginFactory {
    public:
        virtual ~PluginFactory() = default;
        virtual QObject *create(KDC::VfsSetupParams &vfsSetupParams, QObject *parent = nullptr) = 0;
};

template<class PluginClass>
class DefaultPluginFactory : public PluginFactory {
    public:
        virtual ~DefaultPluginFactory() = default;
        QObject *create(KDC::VfsSetupParams &vfsSetupParams, QObject *parent = nullptr) override {
            return new PluginClass(vfsSetupParams, parent);
        }
};

/// Return the expected name of a plugin, for use with QPluginLoader
QString pluginFileName(const QString &type, const QString &name);

} // namespace KDC

Q_DECLARE_INTERFACE(KDC::PluginFactory, "org.kDrive.PluginFactory")
