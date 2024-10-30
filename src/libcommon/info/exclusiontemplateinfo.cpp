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

#include "exclusiontemplateinfo.h"

namespace KDC {

ExclusionTemplateInfo::ExclusionTemplateInfo(const QString &templ, bool warning, bool def, bool deleted) :
    _templ(templ), _warning(warning), _def(def), _deleted(deleted) {}

ExclusionTemplateInfo::ExclusionTemplateInfo() : _templ(QString()), _warning(false), _def(false), _deleted(false) {}

QDataStream &operator>>(QDataStream &in, ExclusionTemplateInfo &exclusionTemplateInfo) {
    in >> exclusionTemplateInfo._templ >> exclusionTemplateInfo._warning >> exclusionTemplateInfo._def >>
            exclusionTemplateInfo._deleted;
    return in;
}

QDataStream &operator<<(QDataStream &out, const ExclusionTemplateInfo &exclusionTemplateInfo) {
    out << exclusionTemplateInfo._templ << exclusionTemplateInfo._warning << exclusionTemplateInfo._def
        << exclusionTemplateInfo._deleted;
    return out;
}

QDataStream &operator<<(QDataStream &out, const QList<ExclusionTemplateInfo> &list) {
    auto count = list.size();
    out << count;
    for (auto i = 0; i < list.size(); i++) {
        ExclusionTemplateInfo exclusionTemplateInfo = list[i];
        out << exclusionTemplateInfo;
    }
    return out;
}

QDataStream &operator>>(QDataStream &in, QList<ExclusionTemplateInfo> &list) {
    auto count = 0;
    in >> count;
    for (auto i = 0; i < count; i++) {
        ExclusionTemplateInfo *exclusionTemplateInfo = new ExclusionTemplateInfo();
        in >> *exclusionTemplateInfo;
        list.push_back(*exclusionTemplateInfo);
    }
    return in;
}

} // namespace KDC
