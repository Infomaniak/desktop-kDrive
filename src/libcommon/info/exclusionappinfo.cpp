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

#include "exclusionappinfo.h"

namespace KDC {

ExclusionAppInfo::ExclusionAppInfo(const QString &appId, const QString &description, bool def) :
    _appId(appId),
    _description(description),
    _def(def) {}

ExclusionAppInfo::ExclusionAppInfo() :
    _appId(QString()),
    _description(QString()),
    _def(false) {}

QDataStream &operator>>(QDataStream &in, ExclusionAppInfo &exclusionAppInfo) {
    in >> exclusionAppInfo._appId >> exclusionAppInfo._description >> exclusionAppInfo._def;
    return in;
}

QDataStream &operator<<(QDataStream &out, const ExclusionAppInfo &exclusionAppInfo) {
    out << exclusionAppInfo._appId << exclusionAppInfo._description << exclusionAppInfo._def;
    return out;
}

QDataStream &operator<<(QDataStream &out, const QList<ExclusionAppInfo> &list) {
    int count = static_cast<int>(list.size());
    out << count;
    for (int i = 0; i < count; i++) {
        ExclusionAppInfo exclusionAppInfo = list[i];
        out << exclusionAppInfo;
    }
    return out;
}

QDataStream &operator>>(QDataStream &in, QList<ExclusionAppInfo> &list) {
    auto count = 0;
    in >> count;
    for (int i = 0; i < count; i++) {
        ExclusionAppInfo exclusionAppInfo;
        in >> exclusionAppInfo;
        list.push_back(exclusionAppInfo);
    }
    return in;
}

} // namespace KDC
