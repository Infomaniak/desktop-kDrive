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

#include <QDataStream>
#include <QList>
#include <string>

namespace KDC {

class ExclusionApp {
    public:
        ExclusionApp() = default;
        ExclusionApp(const std::string &appId, const std::string &description, bool def = false);

        inline void setAppId(const std::string &appId) { _appId = appId; }
        [[nodiscard]] inline const std::string &appId() const { return _appId; }
        inline void setDescription(const std::string &description) { _description = description; }
        [[nodiscard]] inline const std::string &description() const { return _description; }
        inline void setDef(bool def) { _def = def; }
        [[nodiscard]] inline bool def() const { return _def; }

        void toDynamicStruct(Poco::DynamicStruct &dstruct) const;
        void fromDynamicStruct(const Poco::DynamicStruct &dstruct);

        /// TODO : to be removed once we moved to the new GUI ///
        friend void operator>>(QDataStream &in, ExclusionApp &exclusionApp) {
            QString appId;
            QString description;
            bool def = false;
            in >> appId >> description >> def;
            exclusionApp.setAppId(appId.toStdString());
            exclusionApp.setDescription(description.toStdString());
            exclusionApp.setDef(def);
        }
        friend QDataStream &operator<<(QDataStream &out, const ExclusionApp &exclusionApp) {
            out << QString::fromStdString(exclusionApp.appId()) << QString::fromStdString(exclusionApp.description())
                << exclusionApp.def();
            return out;
        }

        friend void operator>>(QDataStream &in, QList<ExclusionApp> &list) {
            qint64 count = 0;
            in >> count;
            for (qint64 i = 0; i < count; i++) {
                ExclusionApp exclusionApp;
                in >> exclusionApp;
                list.push_back(exclusionApp);
            }
        }
        friend QDataStream &operator<<(QDataStream &out, const QList<ExclusionApp> &list) {
            const auto count = static_cast<qint64>(list.size());
            out << count;
            for (qint64 i = 0; i < count; i++) {
                out << list[static_cast<qsizetype>(i)];
            }
            return out;
        }
        /////////////////////////////////////////////////////////

        bool operator==(const ExclusionApp &other) const = default;

    private:
        std::string _appId;
        std::string _description;
        bool _def{false};
};

using ExclusionAppList = std::vector<ExclusionApp>;

} // namespace KDC
