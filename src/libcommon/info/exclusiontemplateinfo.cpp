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

#include "exclusiontemplateinfo.h"

#include "libcommon/utility/utility.h"

static const auto exclusionTemplateInfoString = "template";
static const auto exclusionTemplateInfoWarning = "warning";
static const auto exclusionTemplateInfoDefault = "default";
static const auto exclusionTemplateInfoDeleted = "deleted";

namespace KDC {

ExclusionTemplateInfo::ExclusionTemplateInfo(const QString &templ, bool warning, bool def, bool deleted) :
    _templ(templ),
    _warning(warning),
    _def(def),
    _deleted(deleted) {}


void ExclusionTemplateInfo::toDynamicStruct(Poco::DynamicStruct &dstruct) const {
    CommonUtility::writeValueToStruct(dstruct, exclusionTemplateInfoString, CommonUtility::qStr2CommString(_templ));
    CommonUtility::writeValueToStruct(dstruct, exclusionTemplateInfoWarning, _warning);
    CommonUtility::writeValueToStruct(dstruct, exclusionTemplateInfoDefault, _def);
    CommonUtility::writeValueToStruct(dstruct, exclusionTemplateInfoDeleted, _deleted);
}

void ExclusionTemplateInfo::fromDynamicStruct(const Poco::DynamicStruct &dstruct) {
    CommString templateCommStr;
    CommonUtility::readValueFromStruct(dstruct, exclusionTemplateInfoString, templateCommStr);
    _templ = CommonUtility::commString2QStr(templateCommStr);

    CommonUtility::readValueFromStruct(dstruct, exclusionTemplateInfoWarning, _warning);
    CommonUtility::readValueFromStruct(dstruct, exclusionTemplateInfoDefault, _def);
    CommonUtility::readValueFromStruct(dstruct, exclusionTemplateInfoDeleted, _deleted);
}

void ExclusionTemplateInfo::normalizeExclusionTemplateInfoList(std::vector<ExclusionTemplateInfo> &templateList) {
    SyncNameSet uniqueTemplSet; // Unique template names up to NFC-encoding.
    for (auto it = templateList.begin(); it != templateList.end();) {
        SyncName normalizedTempl;
        if (const auto nfcSuccess =
                    CommonUtility::normalizedSyncName(QStr2SyncName(it->templ()), normalizedTempl, UnicodeNormalization::NFC);
            !nfcSuccess)
            normalizedTempl = QStr2SyncName(it->templ());
        else
            it->setTempl(QString::fromStdString(SyncName2Str(normalizedTempl)));

        if (uniqueTemplSet.emplace(normalizedTempl).second)
            ++it;
        else
            templateList.erase(it);
    }
}

void ExclusionTemplateInfo::computeNormalizations(std::vector<ExclusionTemplateInfo> &templateList) {
    std::vector<ExclusionTemplateInfo> result;

    for (const auto &templateInfo: templateList) {
        const auto normalizations = computeNormalizations(QStr2SyncName(templateInfo.templ()));
        for (const auto &normalization: normalizations)
            result.push_back(ExclusionTemplateInfo{QString::fromStdString(SyncName2Str(normalization))});
    }

    return result;
}

//
//! Computes and returns all possible NFC and NFD normalizations of `templateString` segments
//! interpreted as a file system path.
/*!
  \param templateString is the pattern string the normalizations of which are queried.
  \return a set of std::string containing the NFC and NFD normalizations of exclusionTemplate, if those have been successful.
  The returned set contains additionally the string exclusionTemplate in any case.
*/
std::unordered_set<SyncName> ExclusionTemplateInfo::computeNormalizations(const SyncName &templateString) {
    if (!canNormalize(templateString)) return {templateString};

    const auto normalizations = CommonUtility::computePathNormalizations(templateString);
    std::unordered_set<SyncName> result;
    for (const SyncName &normalization: normalizations) (void) result.emplace(normalization);
    return result;
}


bool ExclusionTemplateInfo::canNormalize(const SyncName &template_) {
    SyncName nfcNormalizedName;
    const bool nfcSuccess = CommonUtility::normalizedSyncName(template_, nfcNormalizedName, UnicodeNormalization::NFC);

    SyncName nfdNormalizedName;
    const bool nfdSuccess = CommonUtility::normalizedSyncName(template_, nfdNormalizedName, UnicodeNormalization::NFD);

    if (!nfcSuccess || !nfdSuccess) {
        return false;
    }
    return true;
}

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
    int count = static_cast<int>(list.size());
    out << count;
    for (int i = 0; i < count; i++) {
        ExclusionTemplateInfo exclusionTemplateInfo = list[i];
        out << exclusionTemplateInfo;
    }
    return out;
}

QDataStream &operator>>(QDataStream &in, QList<ExclusionTemplateInfo> &list) {
    auto count = 0;
    in >> count;
    for (int i = 0; i < count; i++) {
        ExclusionTemplateInfo *exclusionTemplateInfo = new ExclusionTemplateInfo();
        in >> *exclusionTemplateInfo;
        list.push_back(*exclusionTemplateInfo);
    }
    return in;
}

} // namespace KDC
