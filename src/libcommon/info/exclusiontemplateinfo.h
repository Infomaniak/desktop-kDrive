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

#include <Poco/DynamicStruct.h>

#include <QDataStream>
#include <QString>
#include <QList>

namespace KDC {

class ExclusionTemplateInfo {
    public:
        ExclusionTemplateInfo(const QString &templ, bool warning = false, bool def = false);
        ExclusionTemplateInfo() = default;

        inline void setTempl(const QString &templ) { _templ = templ; }
        inline const QString &templ() const { return _templ; }
        inline void setWarning(bool warning) { _warning = warning; }
        inline bool warning() const { return _warning; }
        inline void setDef(bool def) { _def = def; }
        inline bool def() const { return _def; }
        friend bool operator==(const ExclusionTemplateInfo &lhs, const ExclusionTemplateInfo &rhs) {
            return (lhs.templ() == rhs.templ()) && (lhs.warning() == rhs.warning()) && (lhs.def() == rhs.def());
        }

        void toDynamicStruct(Poco::DynamicStruct &dstruct) const;
        void fromDynamicStruct(const Poco::DynamicStruct &dstruct);

        //! NFC normalizes the patterns in the list passed as a parameter and removes duplicates.
        /*!
          \param templateList is the pattern list the normalization of which is queried.
        */
        static void normalizeExclusionTemplateInfoList(std::vector<ExclusionTemplateInfo> &templateList);

        //! Update the pattern list passed as a parameter with the NFC and NFD versions of the patterns.
        /*!
          \param templateList is the pattern list the update of which is queried.
        */
        static void updateExclusionTemplateInfoList(std::vector<ExclusionTemplateInfo> &templateList);

        //! Computes and returns all possible NFC and NFD normalizations of `templateString` segments
        //! interpreted as a file system path.
        /*!
          \param templateString is the pattern string the normalizations of which are queried.
          \return a set of std::string containing the NFC and NFD normalizations of exclusionTemplate, if those have been
          successful. The returned set contains additionally the string exclusionTemplate in any case.
        */
        static SyncNameSet computeNormalizations(const SyncName &templateString);

        friend QDataStream &operator>>(QDataStream &in, ExclusionTemplateInfo &exclusionTemplateInfo);
        friend QDataStream &operator<<(QDataStream &out, const ExclusionTemplateInfo &exclusionTemplateInfo);

        friend QDataStream &operator>>(QDataStream &in, QList<ExclusionTemplateInfo> &list);
        friend QDataStream &operator<<(QDataStream &out, const QList<ExclusionTemplateInfo> &list);

    private:
        QString _templ;
        bool _warning = false;
        bool _def = false;
};

} // namespace KDC
