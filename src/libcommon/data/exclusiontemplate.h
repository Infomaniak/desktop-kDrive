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

#include "utility/types.h"

#include <Poco/DynamicStruct.h>

#include <QDataStream>
#include <QList>
#include <QString>

#include <string>
#include <vector>

namespace KDC {

class ExclusionTemplate {
    public:
        ExclusionTemplate();
        explicit ExclusionTemplate(const std::string &templ, bool warning = false, bool def = false);

        void setTempl(const std::string_view templ) {
            _templ = templ;
            evaluateComplexity();
        }
        [[nodiscard]] const std::string &templ() const { return _templ; }
        void setWarning(const bool warning) { _warning = warning; }
        [[nodiscard]] bool warning() const { return _warning; }
        void setDef(const bool def) { _def = def; }
        [[nodiscard]] bool def() const { return _def; }
        [[nodiscard]] ExclusionTemplateComplexity complexity() const { return _complexity; }

        void toDynamicStruct(Poco::DynamicStruct &dstruct) const;
        void fromDynamicStruct(const Poco::DynamicStruct &dstruct);

        //! NFC normalizes the patterns in the list passed as a parameter and removes duplicates.
        /*!
          \param templateList is the pattern list the normalization of which is queried.
        */
        static void normalizeList(std::vector<ExclusionTemplate> &templateList);

        //! Update the pattern list passed as a parameter with the NFC and NFD versions of the patterns.
        /*!
          \param templateList is the pattern list the update of which is queried.
        */
        static void updateList(std::vector<ExclusionTemplate> &templateList);

        //! Computes and returns all possible NFC and NFD normalizations of `templateString` segments
        //! interpreted as a file system path.
        /*!
          \param templateString is the pattern string the normalizations of which are queried.
          \return a set of std::string containing the NFC and NFD normalizations of exclusionTemplate, if those have been
          successful. The returned set contains additionally the string exclusionTemplate in any case.
        */
        static SyncNameSet computeNormalizations(const SyncName &templateString);

        bool operator==(const ExclusionTemplate &other) const {
            return _templ == other._templ && _warning == other._warning && _def == other._def;
        }

        /// TODO : to be removed once we moved to the new GUI ///
        friend void operator>>(QDataStream &in, ExclusionTemplate &exclusionTemplate) {
            QString templ;
            in >> templ >> exclusionTemplate._warning >> exclusionTemplate._def;
            exclusionTemplate.setTempl(templ.toStdString());
        }
        friend QDataStream &operator<<(QDataStream &out, const ExclusionTemplate &exclusionTemplate) {
            out << QString::fromStdString(exclusionTemplate._templ) << exclusionTemplate._warning << exclusionTemplate._def;
            return out;
        }

        friend void operator>>(QDataStream &in, QList<ExclusionTemplate> &list) {
            qint64 count = 0;
            in >> count;
            for (qint64 i = 0; i < count; i++) {
                ExclusionTemplate exclusionTemplate;
                in >> exclusionTemplate;
                list.push_back(exclusionTemplate);
            }
        }
        friend QDataStream &operator<<(QDataStream &out, const QList<ExclusionTemplate> &list) {
            const auto count = static_cast<qint64>(list.size());
            out << count;
            for (qint64 i = 0; i < count; i++) {
                out << list[i];
            }
            return out;
        }
        /////////////////////////////////////////////////////////

    private:
        std::string _templ;
        bool _warning{false};
        bool _def{false};
        ExclusionTemplateComplexity _complexity{ExclusionTemplateComplexity::Complex};

        void evaluateComplexity();
};

using ExclusionTemplateList = std::vector<ExclusionTemplate>;

} // namespace KDC
