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

#include <Poco/Dynamic/Struct.h>

#include <QImage>
#include <QDataStream>
#include <QString>
#include <QList>

namespace KDC {

class ExclusionAppInfo {
        friend bool operator==(const ExclusionAppInfo &lhs, const ExclusionAppInfo &rhs) {
            return (lhs.appId() == rhs.appId()) && (lhs.description() == rhs.description() && lhs.def() == rhs.def());
        }

    public:
        ExclusionAppInfo(const QString &appId, const QString &description, bool def = false);
        ExclusionAppInfo() = default;

        void setAppId(const QString &appId) { _appId = appId; }
        const QString &appId() const { return _appId; }
        void setDescription(const QString &description) { _description = description; }
        const QString &description() const { return _description; }
        inline void setDef(bool def) { _def = def; }
        inline bool def() const { return _def; }

        friend QDataStream &operator>>(QDataStream &in, ExclusionAppInfo &exclusionAppInfo);
        friend QDataStream &operator<<(QDataStream &out, const ExclusionAppInfo &exclusionAppInfo);

        friend QDataStream &operator>>(QDataStream &in, QList<ExclusionAppInfo> &list);
        friend QDataStream &operator<<(QDataStream &out, const QList<ExclusionAppInfo> &list);

        void toDynamicStruct(Poco::DynamicStruct &dstruct) const;
        void fromDynamicStruct(const Poco::DynamicStruct &dstruct);

    private:
        QString _appId;
        QString _description;
        bool _def{false};
};

} // namespace KDC
